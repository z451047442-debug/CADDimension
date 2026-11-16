#include "CADDimension.h"

//--------------------------------------------------------------------------
// Display：视口绘制入口
//--------------------------------------------------------------------------
int CADDimension::Display(TimeValue t, INode* inode, ViewExp* vpt, int /*flags*/)
{
    if (!vpt || !vpt->IsAlive()) return 0;
    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    cad::GWDrawGuard guard(gw);
    gw->setTransform(Matrix3()); // 世界坐标按单位变换进入 wTransPoint
    gw->setRndLimits((gw->getRndLimits() | GW_WIREFRAME) & ~GW_ILLUM);
    cad::SetupObjectColor(gw, inode);

    float textPt = 12.0f;
    int   mode = 0;
    if (pblock) {
        Interval v;
        v.SetInfinite();
        pblock->GetValue(p_text_size, t, textPt, v);
        pblock->GetValue(p_mode, t, mode, v);
    }

    const float ns = cad::NonScalingFactor(vpt);
    gw->SetTextPointSize(cad::ClampF(textPt * ns, 6.0f, 40.0f));

    int drawn = 0;

    if (mode == 1) {
        DimGeom g;
        if (GetDimWorldPoints(t, inode, g)) {
            ScreenSeg   segs[32];
            ScreenLabel labels[4];
            int nLabels = 0;
            const int nSeg = BuildDimPrimitives(t, vpt, g, segs, 32, labels, 4, nLabels);
            for (int i = 0; i < nSeg; ++i) {
                cad::DrawScreenSeg(gw, segs[i]);
                ++drawn;
            }
            for (int i = 0; i < nLabels; ++i) {
                cad::DrawScreenLabel(gw, labels[i]);
                ++drawn;
            }
        }
        if (drawn == 0 && !m_creating)
            drawn = DrawNoTargetMarker(gw, vpt, inode, t, _T("CADDimension: invalid points"));
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
            for (int i = 0; i < nSeg; ++i) {
                cad::DrawScreenSeg(gw, segs[i]);
                ++drawn;
            }
            for (int i = 0; i < nLabels; ++i) {
                cad::DrawScreenLabel(gw, labels[i]);
                ++drawn;
            }
        }
        else {
            INode* target = GetTargetNode(t);
            drawn = DrawNoTargetMarker(gw, vpt, inode, t,
                target ? _T("CADDimension: unsupported target") : _T("CADDimension: no target"));
        }
    }

    return drawn > 0 ? 1 : 0;
}

//--------------------------------------------------------------------------
// 生成屏幕空间标注：包围盒 12 条边 + X/Y/Z 三条尺寸线 + 箭头 + 文字
//--------------------------------------------------------------------------
int CADDimension::BuildScreenPrimitives(TimeValue t, ViewExp* vpt,
                                        const Point3 worldCorners[8], const Point3& localSize,
                                        ScreenSeg* segs, int maxSegs,
                                        ScreenLabel* labels, int maxLabels, int& nLabels) const
{
    nLabels = 0;
    if (!vpt || !vpt->IsAlive()) return 0;
    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    const float ns = cad::NonScalingFactor(vpt);

    float baseOffset = 28.0f;
    if (pblock) {
        Interval v;
        v.SetInfinite();
        pblock->GetValue(p_dim_offset, t, baseOffset, v);
    }

    Point2 sc[8];
    bool   okp[8];
    for (int i = 0; i < 8; ++i)
        cad::MapWorldToScreen(gw, worldCorners[i], sc[i], okp[i]);

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
        segs[nSeg].a = cad::ToScreenPoint(a);
        segs[nSeg].b = cad::ToScreenPoint(b);
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
            const float val = (axis == 0) ? localSize.x
                              : (axis == 1) ? localSize.y
                                            : localSize.z;
            _stprintf_s(buf, _countof(buf), _T("%s %.3f"), axisName[axis], val);
            const Point2 lp = (a2 + b2) * 0.5f + n * (11.0f * ns);
            labels[nLabels].pos = cad::ToScreenPoint(lp);
            labels[nLabels].text = buf;
            ++nLabels;
        }
    }

    // 汇总文字：即使某轴与视向平行，也能看到三个方向的实际尺寸
    if (labels && nLabels < maxLabels) {
        MCHAR buf[160];
        _stprintf_s(buf, _countof(buf), _T("X %.3f   Y %.3f   Z %.3f"),
                    localSize.x, localSize.y, localSize.z);
        Point2 lp(minX + 4.0f, minY - 18.0f * ns);
        if (lp.y < 18.0f * ns) lp.y = minY + 18.0f * ns;
        labels[nLabels].pos = cad::ToScreenPoint(lp);
        labels[nLabels].text = buf;
        ++nLabels;
    }
    return nSeg;
}

//--------------------------------------------------------------------------
// 屏幕空间绘制：已迁移到 Common/CADDraw.h
//--------------------------------------------------------------------------


int CADDimension::DrawNoTargetMarker(GraphicsWindow* gw, ViewExp* vpt,
                                     INode* inode, TimeValue t, const MCHAR* text) const
{
    if (!inode) return 0;

    bool ok = false;
    Point2 c;
    cad::MapWorldToScreen(gw, inode->GetObjectTM(t).GetTrans(), c, ok);
    if (!ok) return 0;

    const float ns = cad::NonScalingFactor(vpt);
    const float r = 8.0f * ns;

    ScreenSeg cross[2];
    cross[0].a = IPoint2(cad::RoundI(c.x - r), cad::RoundI(c.y));
    cross[0].b = IPoint2(cad::RoundI(c.x + r), cad::RoundI(c.y));
    cross[1].a = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y - r));
    cross[1].b = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y + r));
    cad::DrawScreenSeg(gw, cross[0]);
    cad::DrawScreenSeg(gw, cross[1]);

    ScreenLabel lab;
    lab.pos = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y + r + 16.0f * ns));
    lab.text = text ? text : _T("CADDimension: no target");
    cad::DrawScreenLabel(gw, lab);
    return 1;
}
