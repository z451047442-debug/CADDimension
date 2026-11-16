#include "CADDimension.h"

namespace
{
    int DimensionMode(IParamBlock2* pblock, TimeValue t)
    {
        int mode = 0;
        if (pblock) {
            Interval iv;
            iv.SetInfinite();
            pblock->GetValue(p_mode, t, mode, iv);
        }
        return mode;
    }
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

bool CADDimension::GetTargetBoxes(TimeValue t, Box3& localBox, Box3& worldBox, Matrix3& otm) const
{
    localBox.Init();
    worldBox.Init();

    INode* target = GetTargetNode(t);
    if (!target) return false;

    const ObjectState os = target->EvalWorldState(t);
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

    otm = target->GetObjectTM(t);
    os.obj->GetDeformBBox(t, localBox, nullptr, FALSE);
    if (localBox.IsEmpty()) return false;

    // Lights are never dimensionable; a helper is allowed only when it exposes
    // a real deform box (e.g. TapeHelp), which the empty-box check above covers.
    if (os.obj->SuperClassID() == LIGHT_CLASS_ID) return false;

    Point3 corners[8];
    cad::BoxCorners(localBox, corners);
    worldBox.IncludePoints(corners, 8, &otm);
    return true;
}

bool CADDimension::GetTargetDrawData(TimeValue t, Point3 worldCorners[8], Point3& localSize) const
{
    Box3 localBox, worldBox;
    Matrix3 otm;
    if (!GetTargetBoxes(t, localBox, worldBox, otm)) return false;

    Point3 corners[8];
    cad::BoxCorners(localBox, corners);
    for (int i = 0; i < 8; ++i)
        worldCorners[i] = corners[i] * otm;

    localSize = localBox.pmax - localBox.pmin;
    return true;
}

bool CADDimension::BoundingBox(TimeValue t, Box3& box) const
{
    if (DimensionMode(pblock, t) == 1) {
        Point3 p1, p2, p3;
        if (!GetDimensionPoints(t, p1, p2, p3)) return false;
        box.Init();
        box.IncludePoints(&p1, 1);
        box.IncludePoints(&p2, 1);
        box.IncludePoints(&p3, 1);
        const Point3 size = box.pmax - box.pmin;
        box.EnlargeBy(Length(size) * 0.1f + 1.0f);
        return true;
    }

    // 现代 SDK 没有 Object::BoundingBox() 虚函数；保留此辅助函数，
    // 供 Display/HitTest/GetWorldBoundBox 复用同一套包围盒逻辑。
    Box3 localBox, worldBox;
    Matrix3 otm;
    if (!GetTargetBoxes(t, localBox, worldBox, otm) || worldBox.IsEmpty())
        return false;
    box = worldBox;
    return true;
}

void CADDimension::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    if (DimensionMode(pblock, t) == 1) {
        Box3 wb;
        if (GetDimBounds(t, inode, wb)) {
            box = wb;
            return;
        }
    }

    Box3 localBox, wb;
    Matrix3 otm;
    if (GetTargetBoxes(t, localBox, wb, otm)) {
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
                margin = worldPerPixel * marginPx * cad::NonScalingFactor(vp);
            }
        }
        if (margin <= 0.0f)
            margin = 0.05f * Length(wb.pmax - wb.pmin) + 0.1f;
        wb.EnlargeBy(margin);
        box = wb;
        return;
    }

    // 未绑定/不支持目标：在对象节点位置显示一个小十字。
    const Point3 c = inode ? inode->GetObjectTM(t).GetTrans() : Point3(0, 0, 0);
    float s = 1.0f;
    if (vp && vp->IsAlive()) {
        GraphicsWindow* gw = vp->getGW();
        const int winW = gw ? gw->getWinSizeX() : 0;
        if (winW > 0) {
            s = vp->GetVPWorldWidth(c) / static_cast<float>(winW)
                * 24.0f * cad::NonScalingFactor(vp);
            if (s < 1.0f) s = 1.0f;
        }
    }
    box.pmin = c - Point3(s, s, s);
    box.pmax = c + Point3(s, s, s);
}

void CADDimension::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* /*vp*/, Box3& box)
{
    if (DimensionMode(pblock, t) == 1) {
        Box3 wb;
        if (inode && GetDimBounds(t, inode, wb)) {
            Point3 corners[8];
            cad::BoxCorners(wb, corners);
            Matrix3 inv = Inverse(inode->GetObjectTM(t));
            box.Init();
            box.IncludePoints(corners, 8, &inv);
            return;
        }
        box.pmin = Point3(-1, -1, -1);
        box.pmax = Point3(1, 1, 1);
        return;
    }

    Box3 localBox, wb;
    Matrix3 otm;
    if (!inode || !GetTargetBoxes(t, localBox, wb, otm) || wb.IsEmpty()) {
        box.pmin = Point3(-1, -1, -1);
        box.pmax = Point3(1, 1, 1);
        return;
    }

    Point3 corners[8];
    cad::BoxCorners(wb, corners);
    Matrix3 inv = Inverse(inode->GetObjectTM(t));
    box.Init();
    box.IncludePoints(corners, 8, &inv);
}

void CADDimension::GetDeformBBox(TimeValue t, Box3& box, Matrix3* tm, BOOL /*useSel*/)
{
    if (DimensionMode(pblock, t) == 1) {
        Point3 p1, p2, p3;
        if (GetDimensionPoints(t, p1, p2, p3)) {
            box.Init();
            box.IncludePoints(&p1, 1);
            box.IncludePoints(&p2, 1);
            box.IncludePoints(&p3, 1);
            const Point3 size = box.pmax - box.pmin;
            box.EnlargeBy(Length(size) * 0.1f + 1.0f);
        }
        else {
            box.Init();
        }
        return;
    }

    Box3 localBox, wb;
    Matrix3 otm;
    if (!GetTargetBoxes(t, localBox, wb, otm) || wb.IsEmpty()) {
        box.Init();
        return;
    }
    if (tm) box = wb * Inverse(*tm);
    else    box = wb;
}
