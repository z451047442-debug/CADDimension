#include "CADDraw.h"
#include <gfx.h>
#include <algorithm>
#include <cmath>

namespace cad
{
    float ClampF(float v, float lo, float hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    int RoundI(float v)
    {
        if (!std::isfinite(v)) return 0;
        v = ClampF(v, -1.0e6f, 1.0e6f);
        return static_cast<int>(std::floor(v + 0.5f));
    }

    IPoint2 ToScreenPoint(const Point2& p)
    {
        return IPoint2(RoundI(p.x), RoundI(p.y));
    }

    float NonScalingFactor(ViewExp* vpt)
    {
        if (!vpt) return 1.0f;
        return ClampF(vpt->NonScalingObjectSize(), 0.1f, 10.0f);
    }

    void BoxCorners(const Box3& b, Point3 c[8])
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

    Matrix3 SafeInverse(const Matrix3& m)
    {
        const Point3 r0 = m.GetRow(0);
        const Point3 r1 = m.GetRow(1);
        const Point3 r2 = m.GetRow(2);
        const float det = DotProd(r0, CrossProd(r1, r2));
        const float nmax = std::max(Length(r0), std::max(Length(r1), Length(r2)));
        const float tol = 1.0e-5f * nmax * nmax * nmax;

        Matrix3 out;
        if (std::isfinite(det) && nmax > 0.0f && std::fabs(det) > tol)
            out = Inverse(m);
        else
            out = PseudoInverse(m);

        bool finite = true;
        for (int r = 0; r < 3 && finite; ++r) {
            for (int c = 0; c < 3; ++c) {
                if (!std::isfinite(out.GetRow(r)[c])) {
                    finite = false;
                    break;
                }
            }
        }
        if (!finite)
            out.IdentityMatrix();
        return out;
    }

    bool MapWorldToScreen(GraphicsWindow* gw, const Point3& world,
                          Point2& screen, bool& ok)
    {
        screen = Point2(0.0f, 0.0f);
        ok = false;
        if (!gw) return false;

        IPoint3 p;
        p.x = p.y = p.z = 0;
        const DWORD clip = gw->wTransPoint(&world, &p);

        const bool perspective = gw->isPerspectiveView() != 0;
        ok = !perspective ||
             ((clip & (GW_FRONT_PLANE | GW_BACK_PLANE)) == 0);

        screen = Point2(static_cast<float>(p.x), static_cast<float>(p.y));
        return ok;
    }

    bool ClipSegmentToRect(Point2& a, Point2& b,
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
            else          { if (r < t0) return false; if (r < t1) t1 = r; }
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

    void DrawScreenSeg(GraphicsWindow* gw, const ScreenSeg& seg)
    {
        if (!gw) return;
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

    void DrawScreenLabel(GraphicsWindow* gw, const ScreenLabel& label)
    {
        if (!gw || label.text.data()[0] == 0) return;

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

    void SetColor(GraphicsWindow* gw, const Point3& color)
    {
        if (!gw) return;
        gw->setColor(LINE_COLOR, color);
        gw->setColor(TEXT_COLOR, color);
    }

    void SetupObjectColor(GraphicsWindow* gw, INode* inode)
    {
        if (!gw) return;
        if (!inode) {
            SetColor(gw, Point3(0.9f, 0.9f, 0.2f));
            return;
        }
        if (inode->Selected()) {
            SetColor(gw, GetSelColor());
            return;
        }
        if (inode->IsFrozen()) {
            SetColor(gw, GetFreezeColor());
            return;
        }
        const DWORD c = inode->GetWireColor();
        SetColor(gw, Point3(GetRValue(c) / 255.0f,
                            GetGValue(c) / 255.0f,
                            GetBValue(c) / 255.0f));
    }

    GWDrawGuard::GWDrawGuard(GraphicsWindow* gw)
        : m_gw(gw)
        , m_tm(gw ? gw->getTransform() : Matrix3())
        , m_limits(gw ? gw->getRndLimits() : 0)
        , m_textPt(gw ? gw->GetTextPointSize() : 0.0f)
    {
    }

    GWDrawGuard::~GWDrawGuard()
    {
        if (!m_gw) return;
        m_gw->SetTextPointSize(m_textPt);
        m_gw->setRndLimits(m_limits);
        m_gw->setTransform(m_tm);
    }
}
