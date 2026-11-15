//**************************************************************************/
// CADDimension.cpp
// AutoCAD 风格的视口尺寸标注对象（3ds Max 2027 SDK）
//
// 核心思路：
//   1. 通过参数块 TYPE_INODE 参数 + 独立引用槽绑定目标节点；
//   2. 目标包围盒用 INode::EvalWorldState() + Object::GetDeformBBox()
//      获取（传入 node->GetObjectTM()，直接得到世界空间盒）；
//   3. 用 GraphicsWindow::wTransPoint() 把世界点变换到屏幕坐标
//      （2027 SDK 中 ViewExp 没有 MapWorldToScreen()）；
//   4. 用 wPolyline()/wText() 在屏幕空间绘制，因此线宽/字号天然不随
//      视口缩放变化；NonScalingObjectSize() 作为用户可调的标注尺寸系数。
//**************************************************************************/

#include "CADDimension.h"
#include <gfx.h>
#include <cmath>

//--------------------------------------------------------------------------
// 基础工具
//--------------------------------------------------------------------------
static float ClampF(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int RoundI(float v)
{
    return static_cast<int>(std::floor(v + 0.5f));
}

static IPoint2 ToScreenPoint(const Point2& p)
{
    return IPoint2(RoundI(p.x), RoundI(p.y));
}

static float NonScalingFactor(ViewExp* vpt)
{
    if (!vpt) return 1.0f;
    // NonScalingObjectSize() 默认 1.0；用户可在 Viewport Preferences 中调整。
    // TapeHelp 用它抵消视图缩放：worldSize = ns * GetVPWorldWidth()/360。
    return ClampF(vpt->NonScalingObjectSize(), 0.1f, 10.0f);
}

// 按 min/max 顺序填充 8 个角点，供投影与包围盒计算使用。
static void BoxCorners(const Box3& b, Point3 c[8])
{
    const Point3& lo = b.pmin;
    const Point3& hi = b.pmax;
    c[0] = Point3(lo.x, lo.y, lo.z);
    c[1] = Point3(hi.x, lo.y, lo.z);
    c[2] = Point3(lo.x, hi.y, lo.z);
    c[3] = Point3(hi.x, hi.y, lo.z);
    c[4] = Point3(lo.x, lo.y, hi.z);
    c[5] = Point3(hi.x, lo.y, hi.z);
    c[6] = Point3(lo.x, hi.y, hi.z);
    c[7] = Point3(hi.x, hi.y, hi.z);
}

// Liang-Barsky 2D 线段裁剪。w 系列绘制函数不裁剪，越界可能崩溃，故自行裁剪。
static bool ClipSegmentToRect(Point2& a, Point2& b,
                              float xmin, float ymin, float xmax, float ymax)
{
    if (xmin > xmax) { const float t = xmin; xmin = xmax; xmax = t; }
    if (ymin > ymax) { const float t = ymin; ymin = ymax; ymax = t; }

    float t0 = 0.0f, t1 = 1.0f;
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;

    auto edge = [&](float p, float q) -> bool
    {
        if (p == 0.0f) return q >= 0.0f;
        const float r = q / p;
        if (p < 0.0f) { if (r > t1) return false; if (r > t0) t0 = r; }
        else           { if (r < t0) return false; if (r < t1) t1 = r; }
        return true;
    };

    if (!edge(-dx, a.x - xmin)) return false;
    if (!edge( dx, xmax - a.x)) return false;
    if (!edge(-dy, a.y - ymin)) return false;
    if (!edge( dy, ymax - a.y)) return false;
    if (t0 > t1) return false;

    const Point2 a0 = a, b0 = b;
    a = a0 + (b0 - a0) * t0;
    b = a0 + (b0 - a0) * t1;
    return true;
}

static RECT NormalizedRect(const RECT& r)
{
    RECT n;
    n.left   = r.left   < r.right  ? r.left   : r.right;
    n.right  = r.left   < r.right  ? r.right  : r.left;
    n.top    = r.top    < r.bottom ? r.top    : r.bottom;
    n.bottom = r.top    < r.bottom ? r.bottom : r.top;
    return n;
}

static bool PointInRect(const IPoint2& p, const RECT& r)
{
    return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
}

static bool SegmentIntersectsRect(const IPoint2& a, const IPoint2& b, const RECT& r)
{
    Point2 p0(static_cast<float>(a.x), static_cast<float>(a.y));
    Point2 p1(static_cast<float>(b.x), static_cast<float>(b.y));
    return ClipSegmentToRect(p0, p1,
        static_cast<float>(r.left), static_cast<float>(r.top),
        static_cast<float>(r.right), static_cast<float>(r.bottom));
}

static float DistPointSegment(const Point2& p, const Point2& a, const Point2& b)
{
    const Point2 ab = b - a;
    const float l2 = ab.LengthSquared();
    if (l2 <= 1.0e-6f) return Length(p - a);
    float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2;
    t = ClampF(t, 0.0f, 1.0f);
    return Length(p - (a + ab * t));
}

//--------------------------------------------------------------------------
// 参数块访问器：MAXScript / 参数面板设置 targetNode 时同步引用槽并刷新视口
//--------------------------------------------------------------------------
class CADDimensionPBAccessor : public PBAccessor
{
public:
    // R3: reject invalid targets before the value is stored. TYPE_INODE's v.r
    // may be null; the "must not set null" rule does not apply to v.r.
    void PreSet(PB2Value& v, ReferenceMaker* owner, ParamID id,
                int /*tabIndex*/, TimeValue t) override
    {
        if (id != p_target || !owner) return;
        CADDimension* dim = static_cast<CADDimension*>(owner);
        INode* node = static_cast<INode*>(v.r);
        if (dim->IsInvalidTarget(node, t)) v.r = nullptr;
    }

    // P_OWNERS_REF: the pblock owns the reference; only invalidate/refresh here.
    void Set(PB2Value& /*v*/, ReferenceMaker* owner, ParamID id,
             int /*tabIndex*/, TimeValue /*t*/) override
    {
        if (id != p_target || !owner) return;
        CADDimension* dim = static_cast<CADDimension*>(owner);
        if (dim->m_inRefDeleted) return;
        dim->ivalid.SetEmpty();
        dim->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
    }
};

static CADDimensionPBAccessor g_caddimAccessor;

//--------------------------------------------------------------------------
// ClassDesc
//--------------------------------------------------------------------------
class CADDimensionClassDesc : public ClassDesc2
{
public:
    int          IsPublic() override               { return TRUE; }
    void*        Create(BOOL /*loading*/) override { return new CADDimension(); }
    const TCHAR* ClassName() override
    {
        const TCHAR* s = GetString(IDS_CLASS_NAME);
        return s ? s : _T("CADDimension");
    }
    const TCHAR* NonLocalizedClassName() override { return _T("CADDimension"); }
    SClass_ID    SuperClassID() override          { return GEOMOBJECT_CLASS_ID; }
    Class_ID     ClassID() override               { return CADDIMENSION_CLASS_ID; }
    const TCHAR* Category() override
    {
        const TCHAR* s = GetString(IDS_CATEGORY);
        return s ? s : _T("CAD Tools");
    }
    const TCHAR* InternalName() override          { return _T("CADDimension"); }
    HINSTANCE    HInstance() override             { return hInstance; }
};

ClassDesc2* GetCADDimensionDesc()
{
    static CADDimensionClassDesc caddimDesc;
    return &caddimDesc;
}

//--------------------------------------------------------------------------
// ParamBlock2：targetNode 自动暴露为 MAXScript 属性
//--------------------------------------------------------------------------
static ParamBlockDesc2 caddim_param_blk(
    caddim_params, _T("CADDimensionParams"), 0, GetCADDimensionDesc(),
    P_AUTO_CONSTRUCT, kPBlockRef,
    p_target, _T("targetNode"), TYPE_INODE, P_OWNERS_REF, 0,
        p_refno, kTargetRef,
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Target Node"),
        p_end,
    p_text_size, _T("textSize"), TYPE_FLOAT, 0, 0,
        p_default, 12.0f,
        p_range, 6.0f, 48.0f,
        p_nonLocalizedName, _T("Text Size"),
        p_end,
    p_dim_offset, _T("dimensionOffset"), TYPE_FLOAT, 0, 0,
        p_default, 28.0f,
        p_range, 8.0f, 200.0f,
        p_nonLocalizedName, _T("Dimension Offset"),
        p_end,
    p_end
);

//--------------------------------------------------------------------------
// 构造 / 初始化 / 引用管理
//--------------------------------------------------------------------------
CADDimension::CADDimension()
{
    ivalid.SetInfinite();
    Create(); // Object 没有 virtual Create()，这是本类的初始化入口
}

CADDimension::~CADDimension()
{
    DeleteAllRefsFromMe();
}

void CADDimension::Create()
{
    // P_AUTO_CONSTRUCT 会创建参数块并通过 SetReference(kPBlockRef, ...) 挂接。
    if (!pblock) {
        GetCADDimensionDesc()->MakeAutoParamBlocks(this);
        pblock = GetParamBlock(kPBlockRef);
    }
}

RefTargetHandle CADDimension::GetReference(int i)
{
    if (i == kPBlockRef) return pblock;
    if (i == kTargetRef) return static_cast<RefTargetHandle>(targetNode);
    return nullptr;
}

void CADDimension::SetReference(int i, RefTargetHandle rtarg)
{
    if (i == kPBlockRef) {
        pblock = static_cast<IParamBlock2*>(rtarg);
        return;
    }
    if (i == kTargetRef) {
        INode* node = static_cast<INode*>(rtarg);
        targetNode = node;
        m_targetInvalid = false;

        // R3: safe guard without evaluating the pipeline. GetObjectRef() is the
        // official way to inspect the node's referenced object (inode.h:868).
        // Modifier stacks are handled by the evaluated check in
        // GetTargetWorldBox() and by the recursion depth guard.
        if (node) {
            Object* base = node->GetObjectRef();
            if (base == this ||
                (base && base->ClassID() == CADDIMENSION_CLASS_ID))
                m_targetInvalid = true;
        }
        ivalid.SetEmpty();
    }
}

RefTargetHandle CADDimension::Clone(RemapDir& remap)
{
    CADDimension* obj = new CADDimension();

    // R1: BaseClone clones all references (including the pblock and slot 1).
    BaseClone(this, obj, remap);

    // D4: the dimension clone keeps the original target, not a remapped clone.
    if (targetNode) obj->ReplaceReference(kTargetRef, targetNode);

    obj->ivalid.SetEmpty();
    return obj;
}

RefResult CADDimension::NotifyRefChanged(const Interval& /*changeInt*/,
                                         RefTargetHandle hTarget,
                                         PartID& /*partID*/,
                                         RefMessage message,
                                         BOOL /*propagate*/)
{
    // R2: pblock->RefDeleted() may notify; never re-enter the handler.
    if (m_inRefDeleted) return REF_SUCCEED;

    if (message == REFMSG_CHANGE) {
        // 参数块变化（文字大小/偏移）或目标节点移动/修改，都必须让节点重绘。
        if (hTarget == pblock || hTarget == static_cast<RefTargetHandle>(targetNode)) {
            ivalid.SetEmpty();
            NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
        }
    }
    else if (message == REFMSG_TARGET_DELETED) {
        if (hTarget == static_cast<RefTargetHandle>(targetNode)) {
            // ref.h:207-223: clear the reference storage directly. Do not call
            // DeleteReference()/ReplaceReference() from this notification.
            m_inRefDeleted = true;
            targetNode = nullptr;
            m_targetInvalid = false;
            if (pblock) pblock->RefDeleted(p_target);
            m_inRefDeleted = false;
            ivalid.SetEmpty();
            NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
        }
    }
    return REF_SUCCEED;
}

void CADDimension::SetTargetNode(INode* node, TimeValue t)
{
    // P_OWNERS_REF: the pblock writes directly to reference slot kTargetRef.
    // PreSet validates; Set invalidates and refreshes.
    if (pblock) pblock->SetValue(p_target, t, node);
}

MSTR CADDimension::SubAnimName(int i, bool localized)
{
    MSTR s;
    if (i == 0) {
        const TCHAR* n = localized ? GetString(IDS_PARAMS) : _T("Parameters");
        s = n ? n : _T("Parameters");
    }
    return s;
}

IParamBlock2* CADDimension::GetParamBlockByID(BlockID id)
{
    return (pblock && pblock->ID() == id) ? pblock : nullptr;
}

void CADDimension::GetClassName(MSTR& s, bool localized) const
{
    const TCHAR* n = localized ? GetString(IDS_CLASS_NAME) : nullptr;
    s = n ? n : _T("CADDimension");
}

ObjectState CADDimension::Eval(TimeValue /*t*/)
{
    return ObjectState(this);
}

Interval CADDimension::ObjectValidity(TimeValue /*t*/)
{
    Interval iv;
    iv.SetInfinite();
    return iv;
}

//--------------------------------------------------------------------------
// 创建鼠标回调：单击一次即可放置标注对象（目标稍后用 MAXScript 绑定）
//--------------------------------------------------------------------------
class CADDimensionCreateCallBack : public CreateMouseCallBack
{
public:
    int proc(ViewExp* vpt, int msg, int /*point*/, int /*flags*/,
             IPoint2 m, Matrix3& mat) override
    {
        if (!vpt || !vpt->IsAlive()) return FALSE;
        if (msg == MOUSE_FREEMOVE) {
            vpt->SnapPreview(m, m, nullptr, SNAP_IN_3D);
            return TRUE;
        }
        if (msg == MOUSE_POINT || msg == MOUSE_MOVE) {
            mat.SetTrans(vpt->SnapPoint(m, m, nullptr, SNAP_IN_3D));
            if (msg == MOUSE_POINT) return 0; // 单击完成
            return TRUE;
        }
        if (msg == MOUSE_ABORT) return CREATE_ABORT;
        return TRUE;
    }
};

static CADDimensionCreateCallBack g_caddimCreateCB;

CreateMouseCallBack* CADDimension::GetCreateMouseCallBack()
{
    return &g_caddimCreateCB;
}

//--------------------------------------------------------------------------
// 包围盒
//--------------------------------------------------------------------------
bool CADDimension::IsInvalidTarget(INode* node, TimeValue t) const
{
    if (!node) return false;
    const ObjectState os = node->EvalWorldState(t);
    Object* obj = os.obj;
    return obj == this || (obj && obj->ClassID() == CADDIMENSION_CLASS_ID);
}

bool CADDimension::GetTargetWorldBox(TimeValue t, Box3& box) const
{
    box.Init();
    if (!targetNode || m_targetInvalid) return false;

    // EvalWorldState 会评估修改器堆栈；GetDeformBBox 的 tm 参数传入
    // node->GetObjectTM()，得到的就是世界空间包围盒。
    const ObjectState os = targetNode->EvalWorldState(t);
    if (!os.obj || os.obj == this || os.obj->ClassID() == CADDIMENSION_CLASS_ID)
        return false;

    // P0-D: defense in depth against indirect CADDimension -> CADDimension
    // cycles that bypass the direct class check (e.g. via a modifier stack).
    static thread_local int s_depth = 0;
    if (s_depth >= 8) return false;
    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& depth) : d(depth) { ++d; }
        ~DepthGuard() { --d; }
    } guard(s_depth);

    Matrix3 otm = targetNode->GetObjectTM(t);
    os.obj->GetDeformBBox(t, box, &otm, FALSE);

    if (box.IsEmpty()) {
        // 灯光/空对象等可能没有 deform 包围盒，退回节点位置的小盒。
        // B3 replaces this with an explicit "unsupported" marker.
        const Point3 c = otm.GetTrans();
        box.pmin = c - Point3(1.0f, 1.0f, 1.0f);
        box.pmax = c + Point3(1.0f, 1.0f, 1.0f);
    }
    return true;
}

bool CADDimension::BoundingBox(TimeValue t, Box3& box) const
{
    // 现代 SDK 没有 Object::BoundingBox() 虚函数；保留此辅助函数，
    // 供 Display/HitTest/GetWorldBoundBox 复用同一套包围盒逻辑。
    return GetTargetWorldBox(t, box);
}

void CADDimension::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    Box3 wb;
    if (GetTargetWorldBox(t, wb)) {
        float margin = 0.0f;
        if (vp && vp->IsAlive()) {
            GraphicsWindow* gw = vp->getGW();
            const int winW = gw ? gw->getWinSizeX() : 0;
            if (winW > 0) {
                // 根据用户设置的最大尺寸线偏移估算视口外扩像素，再换算成世界单位。
                // NonScalingObjectSize 作为用户可调的尺寸系数参与计算。
                float offsetPx = 28.0f;
                if (pblock) {
                    Interval v;
                    v.SetInfinite();
                    pblock->GetValue(p_dim_offset, t, offsetPx, v);
                }
                const float marginPx = offsetPx + 120.0f; // 尺寸线 + 箭头 + 文字余量
                const float worldPerPixel =
                    vp->GetVPWorldWidth(wb.Center()) / static_cast<float>(winW);
                margin = worldPerPixel * marginPx * NonScalingFactor(vp);
            }
        }
        if (margin <= 0.0f)
            margin = 0.05f * Length(wb.pmax - wb.pmin) + 0.1f;
        wb.EnlargeBy(margin);
        box = wb;
        return;
    }

    // 未绑定目标：在对象节点位置显示一个小十字。
    const Point3 c = inode ? inode->GetObjectTM(t).GetTrans() : Point3(0, 0, 0);
    float s = 1.0f;
    if (vp && vp->IsAlive()) {
        GraphicsWindow* gw = vp->getGW();
        const int winW = gw ? gw->getWinSizeX() : 0;
        if (winW > 0) {
            s = vp->GetVPWorldWidth(c) / static_cast<float>(winW)
                * 24.0f * NonScalingFactor(vp);
            if (s < 1.0f) s = 1.0f;
        }
    }
    box.pmin = c - Point3(s, s, s);
    box.pmax = c + Point3(s, s, s);
}

void CADDimension::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* /*vp*/, Box3& box)
{
    Box3 wb;
    if (!inode || !GetTargetWorldBox(t, wb) || wb.IsEmpty()) {
        box.pmin = Point3(-1, -1, -1);
        box.pmax = Point3(1, 1, 1);
        return;
    }

    Point3 corners[8];
    BoxCorners(wb, corners);
    Matrix3 inv = Inverse(inode->GetObjectTM(t));
    box.Init();
    box.IncludePoints(corners, 8, &inv);
}

void CADDimension::GetDeformBBox(TimeValue t, Box3& box, Matrix3* tm, BOOL /*useSel*/)
{
    Box3 wb;
    if (!GetTargetWorldBox(t, wb) || wb.IsEmpty()) {
        box.Init();
        return;
    }
    if (tm) box = wb * Inverse(*tm);
    else    box = wb;
}

//--------------------------------------------------------------------------
// 世界坐标 -> 屏幕坐标
//--------------------------------------------------------------------------
Point2 CADDimension::MapWorldToScreen(GraphicsWindow* gw, const Point3& world, bool& ok) const
{
    // 真实 2027 API 是 GraphicsWindow::wTransPoint()；ViewExp 没有
    // MapWorldToScreen()。调用前必须先把 gw 的变换设为单位矩阵。
    // 输出原点在左上角，与鼠标 IPoint2 坐标一致，故可直接用于 HitTest。
    IPoint3 p;
    const DWORD clip = gw->wTransPoint(&world, &p);
    // 透视视图中相机后方的点投影无意义；正交视图可忽略该标志。
    const bool perspective = gw->isPerspectiveView() != 0;
    ok = !perspective || ((clip & GW_FRONT_PLANE) == 0);
    return Point2(static_cast<float>(p.x), static_cast<float>(p.y));
}

void CADDimension::SetupLineColor(GraphicsWindow* gw, INode* inode) const
{
    if (!inode) {
        gw->setColor(LINE_COLOR, Point3(0.9f, 0.9f, 0.2f));
        return;
    }
    if (inode->Selected()) {
        gw->setColor(LINE_COLOR, GetSelColor());
        return;
    }
    if (inode->IsFrozen()) {
        gw->setColor(LINE_COLOR, GetFreezeColor());
        return;
    }
    const DWORD c = inode->GetWireColor();
    gw->setColor(LINE_COLOR,
                 Point3(GetRValue(c) / 255.0f,
                        GetGValue(c) / 255.0f,
                        GetBValue(c) / 255.0f));
}

//--------------------------------------------------------------------------
// Display：视口绘制入口
//--------------------------------------------------------------------------
int CADDimension::Display(TimeValue t, INode* inode, ViewExp* vpt, int /*flags*/)
{
    if (!vpt || !vpt->IsAlive()) return 0;
    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    const Matrix3 oldTM = gw->getTransform();
    const DWORD   oldLimits = gw->getRndLimits();
    gw->setTransform(Matrix3()); // 世界坐标按单位变换进入 wTransPoint
    gw->setRndLimits((oldLimits | GW_WIREFRAME) & ~GW_ILLUM);
    SetupLineColor(gw, inode);

    float textPt = 12.0f;
    if (pblock) {
        Interval v;
        v.SetInfinite();
        pblock->GetValue(p_text_size, t, textPt, v);
    }

    // NonScalingObjectSize 只作为用户偏好系数：屏幕空间绘制本身已经
    // 保证线宽/字号不随视口缩放变化。TapeHelp 的经典世界空间公式为
    // worldSize = NonScalingObjectSize()*GetVPWorldWidth()/360。
    const float ns = NonScalingFactor(vpt);
    const float oldTextPt = gw->GetTextPointSize();
    gw->SetTextPointSize(ClampF(textPt * ns, 6.0f, 40.0f));

    Box3 box;
    if (GetTargetWorldBox(t, box)) {
        ScreenSeg   segs[48];
        ScreenLabel labels[4];
        int nLabels = 0;
        const int nSeg = BuildScreenPrimitives(t, vpt, box, segs, 48, labels, 4, nLabels);
        for (int i = 0; i < nSeg; ++i)  DrawScreenSeg(gw, segs[i]);
        for (int i = 0; i < nLabels; ++i) DrawScreenLabel(gw, labels[i]);
    } else {
        DrawNoTargetMarker(gw, vpt, inode, t);
    }

    gw->SetTextPointSize(oldTextPt);
    gw->setRndLimits(oldLimits);
    gw->setTransform(oldTM);
    return 0;
}

//--------------------------------------------------------------------------
// 生成屏幕空间标注：包围盒 12 条边 + X/Y/Z 三条尺寸线 + 箭头 + 文字
//--------------------------------------------------------------------------
int CADDimension::BuildScreenPrimitives(TimeValue t, ViewExp* vpt, const Box3& box,
                                        ScreenSeg* segs, int maxSegs,
                                        ScreenLabel* labels, int maxLabels, int& nLabels) const
{
    nLabels = 0;
    if (!vpt || !vpt->IsAlive()) return 0;
    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    const float ns = NonScalingFactor(vpt);

    float baseOffset = 28.0f;
    if (pblock) {
        Interval v;
        v.SetInfinite();
        pblock->GetValue(p_dim_offset, t, baseOffset, v);
    }

    Point3 corners[8];
    BoxCorners(box, corners);

    Point2 sc[8];
    bool   okp[8];
    for (int i = 0; i < 8; ++i)
        sc[i] = MapWorldToScreen(gw, corners[i], okp[i]);

    float minX = 1.0e30f, minY = 1.0e30f;
    float maxX = -1.0e30f, maxY = -1.0e30f;
    int valid = 0;
    for (int i = 0; i < 8; ++i) {
        if (!okp[i]) continue;
        if (sc[i].x < minX) minX = sc[i].x;
        if (sc[i].y < minY) minY = sc[i].y;
        if (sc[i].x > maxX) maxX = sc[i].x;
        if (sc[i].y > maxY) maxY = sc[i].y;
        ++valid;
    }
    if (valid == 0) return 0;

    const Point2 center((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);

    int nSeg = 0;
    auto addSeg = [&](const Point2& a, const Point2& b)
    {
        if (!segs || nSeg >= maxSegs) return;
        segs[nSeg].a = ToScreenPoint(a);
        segs[nSeg].b = ToScreenPoint(b);
        ++nSeg;
    };

    // 包围盒 12 条边
    static const int edges[12][2] = {
        {0,1},{1,3},{3,2},{2,0},
        {4,5},{5,7},{7,6},{6,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; ++i) {
        const int a = edges[i][0], b = edges[i][1];
        if (okp[a] && okp[b]) addSeg(sc[a], sc[b]);
    }

    // 三条尺寸线：X: 0->1, Y: 0->2, Z: 0->4
    static const int axisPairs[3][2] = { {0,1}, {0,2}, {0,4} };
    const MCHAR* axisName[3] = { _T("X"), _T("Y"), _T("Z") };

    for (int axis = 0; axis < 3; ++axis) {
        const int ia = axisPairs[axis][0];
        const int ib = axisPairs[axis][1];
        if (!okp[ia] || !okp[ib]) continue;

        const Point2 a = sc[ia];
        const Point2 b = sc[ib];
        const Point2 d = b - a;
        const float len = Length(d);
        if (len < 4.0f * ns) continue; // 该轴与视向几乎平行，跳过

        Point2 n(-d.y / len, d.x / len); // 线段的法线
        const Point2 mid = (a + b) * 0.5f;
        if (n.x * (mid.x - center.x) + n.y * (mid.y - center.y) < 0.0f)
            n = Point2(-n.x, -n.y); // 始终朝屏幕包围盒外侧

        const float off = (baseOffset + axis * 22.0f) * ns;
        const Point2 a2 = a + n * off;
        const Point2 b2 = b + n * off;
        const Point2 gap  = n * (2.0f * ns);
        const Point2 over = n * (5.0f * ns);

        // 尺寸界线（从包围盒角点引到尺寸线，略微超出）
        addSeg(a + gap, a2 + over);
        addSeg(b + gap, b2 + over);
        addSeg(a2, b2); // 尺寸线

        // 箭头（两条短线组成的 V 形）
        const Point2 dir = d * (1.0f / len);
        const Point2 perp(-dir.y, dir.x);
        float arrow = 7.0f * ns;
        if (arrow > len * 0.30f) arrow = len * 0.30f;
        addSeg(a2, a2 + dir * arrow + perp * (arrow * 0.45f));
        addSeg(a2, a2 + dir * arrow - perp * (arrow * 0.45f));
        addSeg(b2, b2 - dir * arrow + perp * (arrow * 0.45f));
        addSeg(b2, b2 - dir * arrow - perp * (arrow * 0.45f));

        if (labels && nLabels < maxLabels) {
            MCHAR buf[96];
            const float val = (axis == 0) ? (box.pmax.x - box.pmin.x)
                              : (axis == 1) ? (box.pmax.y - box.pmin.y)
                                            : (box.pmax.z - box.pmin.z);
            _stprintf_s(buf, _countof(buf), _T("%s %.3f"), axisName[axis], val);
            const Point2 lp = (a2 + b2) * 0.5f + n * (11.0f * ns);
            labels[nLabels].pos = ToScreenPoint(lp);
            labels[nLabels].text = buf;
            ++nLabels;
        }
    }

    // 汇总文字：即使某轴与视向平行，也能看到三个方向的实际尺寸
    if (labels && nLabels < maxLabels) {
        MCHAR buf[160];
        _stprintf_s(buf, _countof(buf), _T("X %.3f   Y %.3f   Z %.3f"),
                    box.pmax.x - box.pmin.x,
                    box.pmax.y - box.pmin.y,
                    box.pmax.z - box.pmin.z);
        Point2 lp(minX + 4.0f, minY - 18.0f * ns);
        if (lp.y < 18.0f * ns) lp.y = minY + 18.0f * ns;
        labels[nLabels].pos = ToScreenPoint(lp);
        labels[nLabels].text = buf;
        ++nLabels;
    }
    return nSeg;
}

//--------------------------------------------------------------------------
// 屏幕空间绘制：线宽固定为 1 设备像素，文字为固定屏幕字体
//--------------------------------------------------------------------------
void CADDimension::DrawScreenSeg(GraphicsWindow* gw, const ScreenSeg& seg) const
{
    const float w = static_cast<float>(gw->getWinSizeX());
    const float h = static_cast<float>(gw->getWinSizeY());
    if (w <= 4.0f || h <= 4.0f) return;

    Point2 a(static_cast<float>(seg.a.x), static_cast<float>(seg.a.y));
    Point2 b(static_cast<float>(seg.b.x), static_cast<float>(seg.b.y));
    if (!ClipSegmentToRect(a, b, 1.0f, 1.0f, w - 1.0f, h - 1.0f)) return;

    IPoint3 pts[2];
    pts[0] = IPoint3(RoundI(a.x), RoundI(a.y), 0);
    pts[1] = IPoint3(RoundI(b.x), RoundI(b.y), 0);
    gw->wPolyline(2, pts, nullptr, FALSE, nullptr);
}

void CADDimension::DrawScreenLabel(GraphicsWindow* gw, const ScreenLabel& label) const
{
    if (label.text.data()[0] == 0) return;

    SIZE sz = { 0, 0 };
    gw->getTextExtents(label.text.data(), &sz);

    const int w = gw->getWinSizeX();
    const int h = gw->getWinSizeY();
    int x = label.pos.x - sz.cx / 2;
    int y = label.pos.y - sz.cy / 2;

    if (x < 2) x = 2;
    if (y < 2) y = 2;
    if (x + sz.cx > w - 2) x = w - 2 - sz.cx;
    if (y + sz.cy > h - 2) y = h - 2 - sz.cy;
    if (x < 2 || y < 2) return;

    IPoint3 p(x, y, 0);
    gw->wText(&p, label.text.data());
}

void CADDimension::DrawNoTargetMarker(GraphicsWindow* gw, ViewExp* vpt,
                                      INode* inode, TimeValue t) const
{
    if (!inode) return;

    bool ok = false;
    const Point2 c = MapWorldToScreen(gw, inode->GetObjectTM(t).GetTrans(), ok);
    if (!ok) return;

    const float ns = NonScalingFactor(vpt);
    const float r = 8.0f * ns;

    ScreenSeg cross[2];
    cross[0].a = IPoint2(RoundI(c.x - r), RoundI(c.y));
    cross[0].b = IPoint2(RoundI(c.x + r), RoundI(c.y));
    cross[1].a = IPoint2(RoundI(c.x), RoundI(c.y - r));
    cross[1].b = IPoint2(RoundI(c.x), RoundI(c.y + r));
    DrawScreenSeg(gw, cross[0]);
    DrawScreenSeg(gw, cross[1]);

    ScreenLabel lab;
    lab.pos = IPoint2(RoundI(c.x), RoundI(c.y + r + 16.0f * ns));
    lab.text = _T("CADDimension: no target");
    DrawScreenLabel(gw, lab);
}

//--------------------------------------------------------------------------
// HitTest：在屏幕空间对标注线段做距离/区域测试
//--------------------------------------------------------------------------
int CADDimension::HitTest(TimeValue t, INode* inode, int type, int crossing,
                          int flags, IPoint2* p, ViewExp* vpt)
{
    if (!vpt || !vpt->IsAlive() || !inode || !p) return 0;
    if ((flags & HIT_SELONLY) && !inode->Selected()) return 0;
    if ((flags & HIT_UNSELONLY) && inode->Selected()) return 0;

    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    const float ns = NonScalingFactor(vpt);
    HitRegion hr;
    MakeHitRegion(hr, type, crossing,
                  static_cast<int>(ClampF(5.0f * ns, 2.0f, 12.0f)), p);

    const Matrix3 oldTM = gw->getTransform();
    gw->setTransform(Matrix3()); // 单位矩阵：wTransPoint 按世界坐标解释

    bool hit = false;
    Box3 box;
    if (GetTargetWorldBox(t, box)) {
        ScreenSeg   segs[48];
        ScreenLabel labels[4];
        int nLabels = 0;
        const int nSeg = BuildScreenPrimitives(t, vpt, box, segs, 48, labels, 4, nLabels);
        hit = HitSegments(hr, segs, nSeg);
    } else {
        bool ok = false;
        const Point2 c = MapWorldToScreen(gw, inode->GetObjectTM(t).GetTrans(), ok);
        if (ok) {
            const float r = 8.0f * ns;
            ScreenSeg cross[2];
            cross[0].a = IPoint2(RoundI(c.x - r), RoundI(c.y));
            cross[0].b = IPoint2(RoundI(c.x + r), RoundI(c.y));
            cross[1].a = IPoint2(RoundI(c.x), RoundI(c.y - r));
            cross[1].b = IPoint2(RoundI(c.x), RoundI(c.y + r));
            hit = HitSegments(hr, cross, 2);
        }
    }

    gw->setTransform(oldTM);
    return hit ? 1 : 0;
}

bool CADDimension::HitSegments(const HitRegion& hr, const ScreenSeg* segs, int nSegs)
{
    if (!segs || nSegs <= 0) return false;

    if (hr.type == POINT_RGN) {
        const Point2 mp(static_cast<float>(hr.pt.x), static_cast<float>(hr.pt.y));
        const float eps = static_cast<float>(hr.epsilon > 0 ? hr.epsilon : 4);
        for (int i = 0; i < nSegs; ++i) {
            const Point2 a(static_cast<float>(segs[i].a.x), static_cast<float>(segs[i].a.y));
            const Point2 b(static_cast<float>(segs[i].b.x), static_cast<float>(segs[i].b.y));
            if (DistPointSegment(mp, a, b) <= eps) return true;
        }
        return false;
    }

    if (hr.type == RECT_RGN) {
        const RECT r = NormalizedRect(hr.rect);
        if (hr.crossing) {
            for (int i = 0; i < nSegs; ++i) {
                if (SegmentIntersectsRect(segs[i].a, segs[i].b, r)) return true;
            }
            return false;
        }
        // 窗口选择：要求整条标注线都在矩形内
        for (int i = 0; i < nSegs; ++i) {
            if (!PointInRect(segs[i].a, r) || !PointInRect(segs[i].b, r))
                return false;
        }
        return true;
    }

    if (hr.type == CIRCLE_RGN) {
        const Point2 c(static_cast<float>(hr.circle.x), static_cast<float>(hr.circle.y));
        const float rad = static_cast<float>(hr.circle.r);
        for (int i = 0; i < nSegs; ++i) {
            const Point2 a(static_cast<float>(segs[i].a.x), static_cast<float>(segs[i].a.y));
            const Point2 b(static_cast<float>(segs[i].b.x), static_cast<float>(segs[i].b.y));
            if (DistPointSegment(c, a, b) <= rad) return true;
        }
        return false;
    }

    return false;
}
