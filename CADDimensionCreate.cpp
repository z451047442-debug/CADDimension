#include "CADDimension.h"
#include <mouseman.h>

//--------------------------------------------------------------------------
// 三点创建：起点 -> 终点 -> 尺寸线位置。
// 首击把对象节点平移到起点，后续点存为“世界点 - 起点”的局部坐标。
//--------------------------------------------------------------------------
class CADDimensionCreateCallBack : public CreateMouseCallBack
{
    CADDimension* ob = nullptr;
    int    stage = 0;                 // 0 = 起点, 1 = 终点, 2 = 尺寸线位置
    Point3 m_origin = Point3(0, 0, 0);  // 世界坐标
    Point3 m_p2 = Point3(0, 0, 0);      // 局部坐标

public:
    void SetObj(CADDimension* obj)
    {
        ob = obj;
        stage = 0;
    }

    int proc(ViewExp* vpt, int msg, int /*point*/, int /*flags*/,
             IPoint2 m, Matrix3& mat) override
    {
        if (!vpt || !vpt->IsAlive() || !ob) return FALSE;

        if (msg == MOUSE_FREEMOVE) {
            vpt->SnapPreview(m, m, nullptr, SNAP_IN_3D);
            return TRUE;
        }

        if (msg == MOUSE_ABORT || msg == MOUSE_PROPCLICK) {
            ob->CancelCreate();
            stage = 0;
            ob = nullptr;
            return CREATE_ABORT;
        }

        if (msg == MOUSE_POINT || msg == MOUSE_MOVE) {
            const Point3 world = vpt->SnapPoint(m, m, nullptr, SNAP_IN_3D);

            if (msg == MOUSE_MOVE) {
                if (stage == 0) {
                    mat.SetTrans(world);
                }
                else if (stage == 2) {
                    ob->SetCreatePreview(world - m_origin);
                }
                return TRUE;
            }

            if (stage == 0) {
                m_origin = world;
                mat.SetTrans(m_origin);
                ob->SetCreateOrigin(m_origin);
                stage = 1;
                return TRUE;
            }

            if (stage == 1) {
                const Point3 local = world - m_origin;
                if (Length(local) < 1.0e-4f)
                    return TRUE; // reject zero-length second point
                m_p2 = local;
                ob->SetCreateSecond(local);
                stage = 2;
                return TRUE;
            }

            // stage == 2: commit.
            ob->CommitCreate(m_origin, m_p2, world - m_origin);
            stage = 0;
            ob = nullptr;
            return 0;
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
