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
            return;
        }

        INode* oldNode = dim->GetTargetNode(t);
        dim->PushPendingTarget(oldNode, t);
    }

    // The pblock owns the reference; only invalidate/refresh here.
    void Set(PB2Value& /*v*/, ReferenceMaker* owner, ParamID id,
             int /*tabIndex*/, TimeValue /*t*/) override
    {
        if (id != p_target || !owner) return;
        CADDimension* dim = static_cast<CADDimension*>(owner);
        if (dim->m_inClone) return;
        dim->ivalid.SetEmpty();
        dim->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
    }

    void PostSet(const PB2Value& /*v*/, ReferenceMaker* owner, ParamID id,
                 int /*tabIndex*/, TimeValue /*t*/) override
    {
        if (id != p_target || !owner) return;
        CADDimension* dim = static_cast<CADDimension*>(owner);
        if (dim->m_inClone || dim->m_suppressUndo || dim->m_inUndoRestore ||
            theHold.RestoreOrRedoing()) {
            dim->m_pendingCount = 0;
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

INode* CADDimension::GetTargetNode(TimeValue t) const
{
    if (!pblock) return nullptr;
    Interval v;
    v.SetInfinite();
    return pblock->GetINode(p_target, t, v);
}

RefTargetHandle CADDimension::GetReference(int i)
{
    return (i == kPBlockRef) ? static_cast<RefTargetHandle>(pblock) : nullptr;
}

void CADDimension::SetReference(int i, RefTargetHandle rtarg)
{
    if (i == kPBlockRef)
        pblock = static_cast<IParamBlock2*>(rtarg);
}

class CADDimCloneTargetPostPatch : public PostPatchProc
{
    IParamBlock2* origPblock;
    INode*        originalTarget;
    CADDimension* clonedObj;
public:
    CADDimCloneTargetPostPatch(IParamBlock2* p, INode* t, CADDimension* c)
        : origPblock(p), originalTarget(t), clonedObj(c) {}

    int Proc(RemapDir& remap) override
    {
        if (!origPblock || !originalTarget) return TRUE;
        IParamBlock2* clonedPblock =
            static_cast<IParamBlock2*>(remap.FindMapping(origPblock));
        if (!clonedPblock) return TRUE;
        if (clonedPblock->GetINode(p_target, 0) == originalTarget) return TRUE;

        if (clonedObj) clonedObj->m_inClone = true;
        const BOOL wasEnabled = clonedPblock->IsNotificationEnabled();
        clonedPblock->EnableNotifications(FALSE);
        clonedPblock->SetValue(p_target, 0, originalTarget);
        clonedPblock->EnableNotifications(wasEnabled);
        if (clonedObj) clonedObj->m_inClone = false;
        return TRUE;
    }
};

RefTargetHandle CADDimension::Clone(RemapDir& remap)
{
    CADDimension* obj = new CADDimension();
    INode* originalTarget = GetTargetNode(0);

    BaseClone(this, obj, remap);

    // BaseClone copies base-class data; clone the pblock reference explicitly
    // (see ref.h:2073-2100 and the CAT CloneParamBlock sample).
    if (pblock) obj->ReplaceReference(kPBlockRef, remap.CloneRef(pblock));

    // D4: the pblock is the only storage. Fix it after RemapDir backpatching,
    // otherwise the cloned target can be overwritten by the backpatch pass.
    if (originalTarget && pblock)
        remap.AddPostPatchProc(
            new CADDimCloneTargetPostPatch(pblock, originalTarget, obj), true);

    obj->ivalid.SetEmpty();
    return obj;
}

RefResult CADDimension::NotifyRefChanged(const Interval& /*changeInt*/,
                                         RefTargetHandle hTarget,
                                         PartID& /*partID*/,
                                         RefMessage message,
                                         BOOL /*propagate*/)
{
    if (m_inClone) return REF_SUCCEED;

    if (message == REFMSG_CHANGE || message == REFMSG_SUBANIM_STRUCTURE_CHANGED) {
        if (hTarget == pblock) {
            // The pblock owns the node reference. If the target was deleted,
            // GetTargetNode() returns null and the no-target marker is drawn.
            ivalid.SetEmpty();
            NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
        }
    }
    else if (message == REFMSG_TARGET_DELETED) {
        // The pblock handles the node reference; never call
        // DeleteReference()/ReplaceReference() from this notification.
        ivalid.SetEmpty();
        NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
    }
    return REF_SUCCEED;
}

void CADDimension::PushPendingTarget(INode* oldTarget, TimeValue t)
{
    if (m_pendingCount >= kMaxPendingTargets) m_pendingCount = 0;
    m_pendingTargets[m_pendingCount].oldTarget = oldTarget;
    m_pendingTargets[m_pendingCount].t = t;
    ++m_pendingCount;
    ++m_pendingSeq;
}

bool CADDimension::PopPendingTarget(INode*& oldTarget, TimeValue& t)
{
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

void CADDimension::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
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
