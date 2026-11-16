#include "CADDimension.h"

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

    const float ns = cad::NonScalingFactor(vpt);
    HitRegion hr;
    MakeHitRegion(hr, type, crossing,
                  static_cast<int>(cad::ClampF(5.0f * ns, 2.0f, 12.0f)), p);

    cad::GWDrawGuard guard(gw);
    gw->setTransform(Matrix3()); // 单位矩阵：wTransPoint 按世界坐标解释

    int mode = 0;
    if (pblock) {
        Interval iv;
        iv.SetInfinite();
        pblock->GetValue(p_mode, t, mode, iv);
    }

    bool hit = false;
    if (mode == 1) {
        DimGeom g;
        if (GetDimWorldPoints(t, inode, g)) {
            ScreenSeg   segs[32];
            ScreenLabel labels[4];
            int nLabels = 0;
            const int nSeg = BuildDimPrimitives(t, vpt, g, segs, 32, labels, 4, nLabels);
            hit = cad::HitSegments(hr, segs, nSeg);
        }
    }
    else {
        Point3 worldCorners[8];
        Point3 localSize;
        if (GetTargetDrawData(t, worldCorners, localSize)) {
            ScreenSeg   segs[48];
            ScreenLabel labels[4];
            int nLabels = 0;
            const int nSeg = BuildScreenPrimitives(t, vpt, worldCorners, localSize,
                                                   segs, 48, labels, 4, nLabels);
            hit = cad::HitSegments(hr, segs, nSeg);
        }
    }

    if (!hit && mode == 0) {
        bool ok = false;
        Point2 c;
        cad::MapWorldToScreen(gw, inode->GetObjectTM(t).GetTrans(), c, ok);
        if (ok) {
            const float r = 8.0f * ns;
            ScreenSeg cross[2];
            cross[0].a = IPoint2(cad::RoundI(c.x - r), cad::RoundI(c.y));
            cross[0].b = IPoint2(cad::RoundI(c.x + r), cad::RoundI(c.y));
            cross[1].a = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y - r));
            cross[1].b = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y + r));
            hit = cad::HitSegments(hr, cross, 2);
        }
    }

    return hit ? 1 : 0;
}

