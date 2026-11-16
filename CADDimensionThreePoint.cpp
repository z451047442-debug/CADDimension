#include "CADDimension.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace
{
    constexpr float kMinLength = 1.0e-4f;
    constexpr float kMinCross  = 1.0e-6f;

    bool IsFinitePoint(const Point3& p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }
}

bool CADDimension::GetDimensionPoints(TimeValue t, Point3& p1, Point3& p2, Point3& p3) const
{
    if (m_creating) {
        p1 = Point3(0.0f, 0.0f, 0.0f);
        p2 = m_createP2;
        p3 = m_createP3;
        return m_createHasP2;
    }

    if (!pblock) return false;
    Interval iv;
    iv.SetInfinite();
    pblock->GetValue(p_point1, t, p1, iv);
    pblock->GetValue(p_point2, t, p2, iv);
    pblock->GetValue(p_dimpoint, t, p3, iv);
    return true;
}

bool CADDimension::ComputeDimGeom(const Point3& p1, const Point3& p2,
                                  const Point3& p3, DimGeom& g) const
{
    g = DimGeom();
    g.p1 = p1;
    g.p2 = p2;
    g.p3 = p3;

    if (!IsFinitePoint(p1) || !IsFinitePoint(p2) || !IsFinitePoint(p3))
        return false;

    const Point3 delta = p2 - p1;
    const float len = Length(delta);
    if (len < kMinLength) return false;

    const Point3 u = delta / len;
    Point3 n = CrossProd(u, p3 - p1);
    float nlen = Length(n);
    if (nlen < kMinCross * std::max(1.0f, Length(p3 - p1))) {
        // Degenerate plane: pick an axis that is not parallel to u.
        const Point3 axis = (std::fabs(u.z) < 0.9f) ? Point3(0, 0, 1) : Point3(1, 0, 0);
        n = CrossProd(u, axis);
        nlen = Length(n);
        if (nlen < kMinCross) return false;
    }
    n /= nlen;

    Point3 w = CrossProd(n, u);
    const float wlen = Length(w);
    if (wlen < kMinCross) return false;
    w /= wlen;

    g.u = u;
    g.n = n;
    g.w = w;
    g.a = p3 + u * DotProd(p1 - p3, u);
    g.b = p3 + u * DotProd(p2 - p3, u);
    g.dist = len;
    g.valid = true;
    return true;
}

bool CADDimension::GetDimWorldPoints(TimeValue t, INode* inode, DimGeom& g) const
{
    if (!inode) return false;
    Point3 p1, p2, p3;
    if (!GetDimensionPoints(t, p1, p2, p3)) return false;

    const Matrix3 tm = inode->GetObjectTM(t);
    return ComputeDimGeom(p1 * tm, p2 * tm, p3 * tm, g);
}

bool CADDimension::GetDimBounds(TimeValue t, INode* inode, Box3& worldBox) const
{
    DimGeom g;
    if (!GetDimWorldPoints(t, inode, g)) return false;

    worldBox.Init();
    worldBox.IncludePoints(&g.p1, 1);
    worldBox.IncludePoints(&g.p2, 1);
    worldBox.IncludePoints(&g.a, 1);
    worldBox.IncludePoints(&g.b, 1);

    const Point3 size = worldBox.pmax - worldBox.pmin;
    const float margin = Length(size) * 0.1f + 1.0f;
    worldBox.EnlargeBy(margin);
    return true;
}

int CADDimension::BuildDimPrimitives(TimeValue t, ViewExp* vpt, const DimGeom& g,
                                     ScreenSeg* segs, int maxSegs,
                                     ScreenLabel* labels, int maxLabels, int& nLabels) const
{
    nLabels = 0;
    if (!vpt || !vpt->IsAlive() || !g.valid) return 0;
    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    const float ns = cad::NonScalingFactor(vpt);

    float textPt = 12.0f;
    float arrowPx = 7.0f;
    float gapPx = 2.0f;
    BOOL  showXYZ = FALSE;
    if (pblock) {
        Interval iv;
        iv.SetInfinite();
        pblock->GetValue(p_text_size, t, textPt, iv);
        pblock->GetValue(p_arrow_size, t, arrowPx, iv);
        pblock->GetValue(p_extension_gap, t, gapPx, iv);
        pblock->GetValue(p_show_xyz, t, showXYZ, iv);
    }

    Point2 s1, s2, sa, sb;
    bool ok1 = false, ok2 = false, oka = false, okb = false;
    cad::MapWorldToScreen(gw, g.p1, s1, ok1);
    cad::MapWorldToScreen(gw, g.p2, s2, ok2);
    cad::MapWorldToScreen(gw, g.a, sa, oka);
    cad::MapWorldToScreen(gw, g.b, sb, okb);
    if (!oka || !okb) return 0;

    int nSeg = 0;
    auto addSeg = [&](const Point2& a, const Point2& b)
    {
        if (!segs || nSeg >= maxSegs) return;
        segs[nSeg].a = cad::ToScreenPoint(a);
        segs[nSeg].b = cad::ToScreenPoint(b);
        ++nSeg;
    };

    const Point2 d = sb - sa;
    const float len = Length(d);
    if (len < 1.0e-3f) return 0;

    const Point2 dir = d / len;
    const Point2 perp(-dir.y, dir.x);

    // Extension lines from the measured points to the dimension line.
    auto extension = [&](const Point2& base, const Point2& target)
    {
        const Point2 v = target - base;
        const float l = Length(v);
        if (l < 1.0e-3f) return;
        const Point2 u = v / l;
        const float gap = cad::ClampF(gapPx * ns, 0.0f, l * 0.45f);
        const Point2 start = base + u * gap;
        const Point2 end = target + u * (5.0f * ns);
        addSeg(start, end);
    };
    if (ok1) extension(s1, sa);
    if (ok2) extension(s2, sb);

    addSeg(sa, sb);

    // Arrowheads at both ends, pointing inward.
    const float arrow = std::min(arrowPx * ns, len * 0.30f);
    addSeg(sa, sa + dir * arrow + perp * (arrow * 0.45f));
    addSeg(sa, sa + dir * arrow - perp * (arrow * 0.45f));
    addSeg(sb, sb - dir * arrow + perp * (arrow * 0.45f));
    addSeg(sb, sb - dir * arrow - perp * (arrow * 0.45f));

    if (labels && nLabels < maxLabels) {
        MCHAR buf[128];
        _stprintf_s(buf, _countof(buf), _T("%.3f"), g.dist);
        const Point2 mid = (sa + sb) * 0.5f;
        const Point2 lp = mid + perp * (textPt * ns * 0.9f);
        labels[nLabels].pos = cad::ToScreenPoint(lp);
        labels[nLabels].text = buf;
        ++nLabels;

        if (showXYZ && nLabels < maxLabels) {
            const Point3 delta = g.p2 - g.p1;
            _stprintf_s(buf, _countof(buf),
                        _T("dX %.3f  dY %.3f  dZ %.3f"),
                        delta.x, delta.y, delta.z);
            labels[nLabels].pos = cad::ToScreenPoint(lp + Point2(0.0f, textPt * ns));
            labels[nLabels].text = buf;
            ++nLabels;
        }
    }

    return nSeg;
}

void CADDimension::SetCreateOrigin(const Point3& world)
{
    m_creating = true;
    m_createHasP2 = false;
    m_createOrigin = world;
    m_createP2 = Point3(0, 0, 0);
    m_createP3 = Point3(0, 0, 0);
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void CADDimension::SetCreateSecond(const Point3& localP2)
{
    m_creating = true;
    m_createHasP2 = true;
    m_createP2 = localP2;
    m_createP3 = localP2;
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void CADDimension::SetCreatePreview(const Point3& localP3)
{
    if (!m_creating || !m_createHasP2) return;
    m_createP3 = localP3;
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void CADDimension::CommitCreate(const Point3& worldOrigin,
                                const Point3& localP2,
                                const Point3& localP3)
{
    (void)worldOrigin;
    if (!pblock) {
        CancelCreate();
        return;
    }

    const bool prev = m_suppressUndo;
    m_suppressUndo = true;

    pblock->SetValue(p_point1, 0, Point3(0, 0, 0));
    pblock->SetValue(p_point2, 0, localP2);
    pblock->SetValue(p_dimpoint, 0, localP3);
    pblock->SetValue(p_mode, 0, 1);

    m_suppressUndo = prev;
    m_creating = false;
    m_createHasP2 = false;
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void CADDimension::CancelCreate()
{
    m_creating = false;
    m_createHasP2 = false;
    m_createP2 = Point3(0, 0, 0);
    m_createP3 = Point3(0, 0, 0);
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}
