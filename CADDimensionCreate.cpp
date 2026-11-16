#include "CADDimension.h"
#include <mouseman.h>

//--------------------------------------------------------------------------
// 创建鼠标回调：有单选直接绑定并放置；无单选先拾取目标，再点击放置。
//--------------------------------------------------------------------------
static bool IsValidCADDimTarget(INode* node, TimeValue t, CADDimension* dim)
{
    if (!node) return false;
    const ObjectState os = node->EvalWorldState(t);
    Object* obj = os.obj;
    return obj && obj != dim && obj->ClassID() != CADDIMENSION_CLASS_ID;
}

class CADDimPickFilter : public PickNodeCallback
{
    CADDimension* dim;
    TimeValue t;
public:
    CADDimPickFilter(CADDimension* d, TimeValue time) : dim(d), t(time) {}
    BOOL Filter(INode* node) override
    {
        return IsValidCADDimTarget(node, t, dim) ? TRUE : FALSE;
    }
};

class CADDimensionCreateCallBack : public CreateMouseCallBack
{
    CADDimension* ob = nullptr;
    int stage = 0; // 0 = need target/placement, 1 = target picked, need placement
public:
    void SetObj(CADDimension* obj) { ob = obj; stage = 0; }

    int proc(ViewExp* vpt, int msg, int /*point*/, int /*flags*/,
             IPoint2 m, Matrix3& mat) override
    {
        if (!vpt || !vpt->IsAlive() || !ob) return FALSE;

        if (msg == MOUSE_FREEMOVE) {
            vpt->SnapPreview(m, m, nullptr, SNAP_IN_3D);
            return TRUE;
        }
        if (msg == MOUSE_ABORT || msg == MOUSE_PROPCLICK) {
            stage = 0;
            ob = nullptr;
            return CREATE_ABORT;
        }
        if (msg == MOUSE_POINT || msg == MOUSE_MOVE) {
            mat.SetTrans(vpt->SnapPoint(m, m, nullptr, SNAP_IN_3D));
            if (msg == MOUSE_MOVE) return TRUE;

            Interface* ip = GetCOREInterface();
            if (!ip) return TRUE;
            const TimeValue t = ip->GetTime();

            if (stage == 1) {
                stage = 0;
                ob = nullptr;
                return 0; // second click places
            }

            // Single selection: auto-bind and place with this click.
            if (ip->GetSelNodeCount() == 1) {
                INode* sel = ip->GetSelNode(0);
                if (IsValidCADDimTarget(sel, t, ob)) {
                    ob->SetTargetNode(sel, t, false);
                    stage = 0;
                    ob = nullptr;
                    return 0;
                }
            }

            // No usable single selection: pick the target first, then place.
            CADDimPickFilter filter(ob, t);
            INode* picked = ip->PickNode(vpt->GetHWnd(), m, &filter);
            if (picked) {
                ob->SetTargetNode(picked, t, false);
                stage = 1;
            }
            return TRUE;
        }
        return TRUE;
    }
};

static CADDimensionCreateCallBack g_caddimCreateCB;

CreateMouseCallBack* CADDimension::GetCreateMouseCallBack()
{
    g_caddimCreateCB.SetObj(this);
    return &g_caddimCreateCB;
}
