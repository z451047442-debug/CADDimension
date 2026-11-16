#include "CADDimension.h"

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
