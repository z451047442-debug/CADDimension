//**************************************************************************/
// CADDimension.cpp
// AutoCAD 风格的视口尺寸标注对象（3ds Max 2027 SDK）
//
// 核心思路：
//   1. 通过参数块 TYPE_INODE 参数（pblock 唯一引用源）绑定目标节点；
//   2. 目标包围盒用 GetDeformBBox(t, box, nullptr) 取局部盒，8 个角点经
//      node->GetObjectTM() 变换为世界坐标；尺寸标签取局部盒；
//   3. 用 GraphicsWindow::wTransPoint() 把世界点变换到屏幕坐标
//      （2027 SDK 中 ViewExp 没有 MapWorldToScreen()）；
//   4. 用 wPolyline()/wText() 在屏幕空间绘制，因此线宽/字号天然不随
//      视口缩放变化；NonScalingObjectSize() 作为用户可调的标注尺寸系数。
//**************************************************************************/

#include "CADDimension.h"
#include <gfx.h>
#include <hold.h>
#include <ioapi.h>
#include <SingleWeakRefMaker.h>
#include <cmath>
#include <mouseman.h>

//--------------------------------------------------------------------------
// 基础工具：已迁移到 Common/CADDraw.h / Common/CADHit.h
//--------------------------------------------------------------------------


//--------------------------------------------------------------------------
// 参数块访问器：MAXScript / 参数面板设置 targetNode 时刷新视口
//--------------------------------------------------------------------------
class CADDimTargetRestore : public RestoreObj
{
    MaxSDK::TypedSingleWeakRefMaker<CADDimension> dim;
    MaxSDK::TypedSingleWeakRefMaker<INode>        oldTarget;
    MaxSDK::TypedSingleWeakRefMaker<INode>        newTarget;
    TimeValue                                     t;

public:
    CADDimTargetRestore(CADDimension* d, INode* o, INode* n, TimeValue time)
        : dim(d), oldTarget(o), newTarget(n), t(time) {}

    void Restore(int /*isUndo*/) override
    {
        CADDimension* d = dim.Get();
        if (!d) return;
        const bool prev = d->m_inUndoRestore;
        d->m_inUndoRestore = true;
        d->ApplyTargetNode(oldTarget.Get(), t);
        d->m_inUndoRestore = prev;
    }

    void Redo() override
    {
        CADDimension* d = dim.Get();
        if (!d) return;
        const bool prev = d->m_inUndoRestore;
        d->m_inUndoRestore = true;
        d->ApplyTargetNode(newTarget.Get(), t);
        d->m_inUndoRestore = prev;
    }

    int Size() override { return sizeof(*this); }
};

class CADDimensionPBAccessor : public PBAccessor
{
public:
    void PreSet(PB2Value& v, ReferenceMaker* owner, ParamID id,
                int /*tabIndex*/, TimeValue t) override
    {
        if (id != p_target || !owner) return;
        CADDimension* dim = static_cast<CADDimension*>(owner);
        INode* newNode = static_cast<INode*>(v.r);
        if (dim->IsInvalidTarget(newNode, t)) v.r = nullptr;

        if (dim->m_inClone || dim->m_suppressUndo || dim->m_inUndoRestore ||
            theHold.RestoreOrRedoing()) {
            dim->m_pendingCount = 0;
            dim->m_pendingOverflow = false;
            return;
        }

        INode* oldNode = dim->GetTargetNode(t);
        dim->PushPendingTarget(oldNode, t);
    }

    // The pblock owns the reference; only invalidate/refresh here.
    void Set(PB2Value& /*v*/, ReferenceMaker* owner, ParamID id,
             int /*tabIndex*/, TimeValue /*t*/) override
    {
        if (!owner) return;
        CADDimension* dim = static_cast<CADDimension*>(owner);
        if (dim->m_inClone) return;
        if (id != p_target && !dim->IsGeometryParam(id)) return;
        dim->ivalid.SetEmpty();
        dim->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
    }

    void PostSet(const PB2Value& /*v*/, ReferenceMaker* owner, ParamID id,
                 int /*tabIndex*/, TimeValue /*t*/) override
    {
        if (!owner) return;
        CADDimension* dim = static_cast<CADDimension*>(owner);
        if (id != p_target) {
            if (dim->m_inClone) return;
            if (!dim->IsGeometryParam(id)) return;
            dim->ivalid.SetEmpty();
            dim->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
            return;
        }
        if (dim->m_inClone || dim->m_suppressUndo || dim->m_inUndoRestore ||
            theHold.RestoreOrRedoing()) {
            dim->m_pendingCount = 0;
            dim->m_pendingOverflow = false;
            return;
        }

        INode* oldNode = nullptr;
        TimeValue pendingTime = 0;
        if (!dim->PopPendingTarget(oldNode, pendingTime)) return;
        INode* newNode = dim->GetTargetNode(pendingTime);
        if (oldNode == newNode) return;

        BOOL started = FALSE;
        if (!theHold.Holding()) {
            theHold.Begin();
            started = TRUE;
        }
        CADDimTargetRestore* restore =
            new CADDimTargetRestore(dim, oldNode, newNode, pendingTime);
        if (!theHold.Put(restore)) {
            delete restore;
            if (started) theHold.Cancel();
            return;
        }
        if (started) theHold.Accept(_T("CADDimension Target"));
    }
};

static CADDimensionPBAccessor g_caddimAccessor;

//--------------------------------------------------------------------------
// 旧场景兼容：加载完成后把 B1 的引用槽 1 目标迁移到 pblock。
// PostLoadCallback 在 pblock 数据加载完成之后执行（ParamBlock2PLCB 优先级 0）。
//--------------------------------------------------------------------------
class CADDimPostLoadMigration : public PostLoadCallback
{
    MaxSDK::TypedSingleWeakRefMaker<CADDimension> dim;
public:
    explicit CADDimPostLoadMigration(CADDimension* d) : dim(d) {}
    void proc(ILoad* /*iload*/) override
    {
        if (CADDimension* d = dim.Get())
            d->MigrateLegacyTarget(0);
    }
};

//--------------------------------------------------------------------------
// ClassDesc
//--------------------------------------------------------------------------
class CADDimensionClassDesc : public ClassDesc2
{
public:
    int          IsPublic() override               { return TRUE; }
    void*        Create(BOOL loading) override      { return new CADDimension(loading != FALSE); }
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
    P_AUTO_CONSTRUCT + P_AUTO_UI, kPBlockRef,
    // rollout
    IDD_PANEL, IDS_PARAMS, 0, 0, NULL,
    // params
    p_target, _T("targetNode"), TYPE_INODE, P_READ_SECOND_FLAG_VALUE,
        P_USE_NODE_TM_VALIDITY | P_USE_NODE_OS_VALIDITY, 0,
        p_ui, TYPE_PICKNODEBUTTON, IDC_PICK_TARGET,
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Target Node"),
        p_end,
    p_text_size, _T("textSize"), TYPE_FLOAT, 0, 0,
        p_default, 12.0f,
        p_range, 6.0f, 48.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_TEXT_SIZE_EDIT, IDC_TEXT_SIZE_SPIN, 0.1f,
        p_nonLocalizedName, _T("Text Size"),
        p_end,
    p_dim_offset, _T("dimensionOffset"), TYPE_FLOAT, 0, 0,
        p_default, 28.0f,
        p_range, 8.0f, 200.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_OFFSET_EDIT, IDC_OFFSET_SPIN, 0.1f,
        p_nonLocalizedName, _T("Dimension Offset"),
        p_end,
    p_point1, _T("point1"), TYPE_POINT3, 0, 0,
        p_default, Point3(0.0f, 0.0f, 0.0f),
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Point 1"),
        p_end,
    p_point2, _T("point2"), TYPE_POINT3, 0, 0,
        p_default, Point3(0.0f, 0.0f, 0.0f),
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Point 2"),
        p_end,
    p_dimpoint, _T("dimPoint"), TYPE_POINT3, 0, 0,
        p_default, Point3(0.0f, 0.0f, 0.0f),
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Dimension Point"),
        p_end,
    p_arrow_size, _T("arrowSize"), TYPE_FLOAT, 0, 0,
        p_default, 7.0f,
        p_range, 4.0f, 20.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_ARROW_EDIT, IDC_ARROW_SPIN, 0.1f,
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Arrow Size"),
        p_end,
    p_extension_gap, _T("extensionGap"), TYPE_FLOAT, 0, 0,
        p_default, 2.0f,
        p_range, 0.0f, 20.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_GAP_EDIT, IDC_GAP_SPIN, 0.1f,
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Extension Gap"),
        p_end,
    p_show_xyz, _T("showXYZ"), TYPE_BOOL, 0, 0,
        p_default, FALSE,
        p_ui, TYPE_SINGLECHECKBOX, IDC_SHOW_XYZ,
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Show XYZ"),
        p_end,
    p_mode, _T("mode"), TYPE_INT, 0, 0,
        p_default, 0,
        p_range, 0, 1,
        p_accessor, &g_caddimAccessor,
        p_nonLocalizedName, _T("Mode"),
        p_end,
    p_end
);

extern ParamMap2UserDlgProc* CreateCADDimensionDlgProc();

static struct CADDimDlgProcInstaller
{
    CADDimDlgProcInstaller()
    {
        caddim_param_blk.SetUserDlgProc(CreateCADDimensionDlgProc());
    }
} g_caddimDlgProcInstaller;

//--------------------------------------------------------------------------
// 构造 / 初始化 / 引用管理
//--------------------------------------------------------------------------
CADDimension::CADDimension(bool loading)
{
    ivalid.SetInfinite();
    Create(loading); // Object 没有 virtual Create()，这是本类的初始化入口
}

CADDimension::~CADDimension()
{
    DeleteAllRefsFromMe();
}

void CADDimension::Create(bool loading)
{
    // P_AUTO_CONSTRUCT 会创建参数块并通过 SetReference(kPBlockRef, ...) 挂接。
    if (!pblock) {
        GetCADDimensionDesc()->MakeAutoParamBlocks(this);
        pblock = GetParamBlock(kPBlockRef);
    }

    // 新建对象默认三点模式；读档对象保持 p_mode 的默认值 0（旧场景兼容）。
    if (!loading && pblock)
        pblock->SetValue(p_mode, 0, 1);
}

INode* CADDimension::GetTargetNode(TimeValue t) const
{
    INode* target = nullptr;
    if (pblock) {
        Interval v;
        v.SetInfinite();
        target = pblock->GetINode(p_target, t, v);
    }
    // 旧场景在 post-load 迁移之前只读回退到兼容槽；Display/Eval 不写 pblock。
    if (!target && m_legacyTarget)
        target = m_legacyTarget;
    return target;
}

RefTargetHandle CADDimension::GetReference(int i)
{
    if (i == kPBlockRef) return static_cast<RefTargetHandle>(pblock);
    if (i == kLegacyTargetRef) return static_cast<RefTargetHandle>(m_legacyTarget);
    return nullptr;
}

void CADDimension::SetReference(int i, RefTargetHandle rtarg)
{
    if (i == kPBlockRef) {
        pblock = static_cast<IParamBlock2*>(rtarg);
        return;
    }
    if (i == kLegacyTargetRef) {
        m_legacyTarget = static_cast<INode*>(rtarg);
        if (m_legacyTarget)
            m_legacyMigrated = false;
    }
}


RefResult CADDimension::NotifyRefChanged(const Interval& /*changeInt*/,
                                         RefTargetHandle hTarget,
                                         PartID& /*partID*/,
                                         RefMessage message,
                                         BOOL /*propagate*/)
{
    if (m_inClone || m_migrating) return REF_SUCCEED;

    if (message == REFMSG_CHANGE || message == REFMSG_SUBANIM_STRUCTURE_CHANGED) {
        if (hTarget == pblock || (m_legacyTarget && hTarget == m_legacyTarget)) {
            // The pblock owns the node reference. If the target was deleted,
            // GetTargetNode() returns null and the no-target marker is drawn.
            ivalid.SetEmpty();
            NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
        }
    }
    else if (message == REFMSG_TARGET_DELETED) {
        // The pblock handles the node reference; never call
        // DeleteReference()/ReplaceReference() from this notification.
        if (m_legacyTarget && hTarget == m_legacyTarget) {
            // Only clear the compatibility slot; do not touch the pblock target
            // that migration may have just written.
            m_legacyTarget = nullptr;
            return REF_SUCCEED;
        }
        ivalid.SetEmpty();
        NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
    }
    return REF_SUCCEED;
}

void CADDimension::PushPendingTarget(INode* oldTarget, TimeValue t)
{
    if (m_pendingCount >= kMaxPendingTargets) {
        // Fail safe: drop this record and skip undo capture for the next
        // PostSet instead of resetting the stack and pairing the wrong value.
        m_pendingCount = 0;
        m_pendingOverflow = true;
        return;
    }
    m_pendingTargets[m_pendingCount].oldTarget = oldTarget;
    m_pendingTargets[m_pendingCount].t = t;
    ++m_pendingCount;
    ++m_pendingSeq;
}

bool CADDimension::PopPendingTarget(INode*& oldTarget, TimeValue& t)
{
    if (m_pendingOverflow) {
        m_pendingOverflow = false;
        return false;
    }
    if (m_pendingCount <= 0) return false;
    --m_pendingCount;
    oldTarget = m_pendingTargets[m_pendingCount].oldTarget;
    t = m_pendingTargets[m_pendingCount].t;
    return true;
}

void CADDimension::ApplyTargetNode(INode* node, TimeValue t)
{
    // Undo-free raw setter; PreSet/PostSet handle undo for property sets.
    if (pblock) pblock->SetValue(p_target, t, node);
}

void CADDimension::SetTargetNode(INode* node, TimeValue t, bool allowUndo)
{
    if (GetTargetNode(t) == node) return;

    const bool prev = m_suppressUndo;
    if (!allowUndo) m_suppressUndo = true;
    ApplyTargetNode(node, t);
    m_suppressUndo = prev;
}

IOResult CADDimension::Load(ILoad* iload)
{
    IOResult res = GeomObject::Load(iload);
    if (res == IO_OK && iload)
        iload->RegisterPostLoadCallback(new CADDimPostLoadMigration(this));
    return res;
}

bool CADDimension::IsGeometryParam(ParamID id) const
{
    return id == p_point1 || id == p_point2 || id == p_dimpoint ||
           id == p_arrow_size || id == p_extension_gap ||
           id == p_show_xyz || id == p_mode;
}

void CADDimension::MigrateLegacyTarget(TimeValue t)
{
    if (m_legacyMigrated) return;
    if (!pblock) return;

    INode* legacy = m_legacyTarget;
    if (!legacy) {
        m_legacyMigrated = true;
        return;
    }

    const bool prevMigrating = m_migrating;
    const bool prevSuppress = m_suppressUndo;
    m_migrating = true;
    m_suppressUndo = true;

    Interval iv;
    iv.SetInfinite();
    INode* current = pblock->GetINode(p_target, t, iv);
    if (!current)
        pblock->SetValue(p_target, t, legacy);

    ReplaceReference(kLegacyTargetRef, nullptr);
    m_legacyTarget = nullptr;
    m_legacyMigrated = true;

    m_suppressUndo = prevSuppress;
    m_migrating = prevMigrating;

    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

MSTR CADDimension::SubAnimName(int i, bool localized)
{
    MSTR s;
    if (i == kPBlockRef) {
        const TCHAR* n = localized ? GetString(IDS_PARAMS) : _T("Parameters");
        s = n ? n : _T("Parameters");
    }
    else if (i == kLegacyTargetRef) {
        s = _T("Legacy Target");
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

void CADDimension::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    MigrateLegacyTarget(ip ? ip->GetTime() : 0);
    GetCADDimensionDesc()->BeginEditParams(ip, this, flags, prev);
}

void CADDimension::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    GetCADDimensionDesc()->EndEditParams(ip, this, flags, next);
}

ObjectState CADDimension::Eval(TimeValue /*t*/)
{
    return ObjectState(this);
}

Interval CADDimension::ObjectValidity(TimeValue t)
{
    Interval iv = Object::ObjectValidity(t);
    iv &= ivalid;
    if (pblock) pblock->GetValidity(t, iv);
    return iv;
}
