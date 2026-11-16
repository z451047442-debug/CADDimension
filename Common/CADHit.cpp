#include "CADHit.h"
#include <cmath>

namespace cad
{
    RECT NormalizedRect(const RECT& r)
    {
        RECT n;
        n.left   = r.left   < r.right  ? r.left   : r.right;
        n.right  = r.left   < r.right  ? r.right  : r.left;
        n.top    = r.top    < r.bottom ? r.top    : r.bottom;
        n.bottom = r.top    < r.bottom ? r.bottom : r.top;
        return n;
    }

    bool PointInRect(const IPoint2& p, const RECT& r)
    {
        return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
    }

    bool SegmentIntersectsRect(const IPoint2& a, const IPoint2& b, const RECT& r)
    {
        Point2 p0(static_cast<float>(a.x), static_cast<float>(a.y));
        Point2 p1(static_cast<float>(b.x), static_cast<float>(b.y));
        return ClipSegmentToRect(p0, p1,
            static_cast<float>(r.left), static_cast<float>(r.top),
            static_cast<float>(r.right), static_cast<float>(r.bottom));
    }

    float DistPointSegment(const Point2& p, const Point2& a, const Point2& b)
    {
        const Point2 ab = b - a;
        const float l2 = ab.LengthSquared();
        if (l2 <= 1.0e-6f) return Length(p - a);
        float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2;
        t = ClampF(t, 0.0f, 1.0f);
        return Length(p - (a + ab * t));
    }

    bool HitSegments(const HitRegion& hr, const ScreenSeg* segs, int nSegs)
    {
        if (!segs || nSegs <= 0) return false;

        if (hr.type == POINT_RGN) {
            const Point2 mp(static_cast<float>(hr.pt.x), static_cast<float>(hr.pt.y));
            const float eps = static_cast<float>(hr.epsilon > 0 ? hr.epsilon : 4);
            for (int i = 0; i < nSegs; ++i) {
                const Point2 a(static_cast<float>(segs[i].a.x), static_cast<float>(segs[i].a.y));
                const Point2 b(static_cast<float>(segs[i].b.x), static_cast<float>(segs[i].b.y));
                if (DistPointSegment(mp, a, b) <= eps) return true;
            }
            return false;
        }

        if (hr.type == RECT_RGN) {
            const RECT r = NormalizedRect(hr.rect);
            if (hr.crossing) {
                for (int i = 0; i < nSegs; ++i) {
                    if (SegmentIntersectsRect(segs[i].a, segs[i].b, r)) return true;
                }
                return false;
            }
            for (int i = 0; i < nSegs; ++i) {
                if (!PointInRect(segs[i].a, r) || !PointInRect(segs[i].b, r))
                    return false;
            }
            return true;
        }

        if (hr.type == CIRCLE_RGN) {
            const Point2 c(static_cast<float>(hr.circle.x), static_cast<float>(hr.circle.y));
            const float rad = static_cast<float>(hr.circle.r);
            if (hr.crossing) {
                for (int i = 0; i < nSegs; ++i) {
                    const Point2 a(static_cast<float>(segs[i].a.x), static_cast<float>(segs[i].a.y));
                    const Point2 b(static_cast<float>(segs[i].b.x), static_cast<float>(segs[i].b.y));
                    if (DistPointSegment(c, a, b) <= rad) return true;
                }
                return false;
            }
            for (int i = 0; i < nSegs; ++i) {
                const Point2 a(static_cast<float>(segs[i].a.x), static_cast<float>(segs[i].a.y));
                const Point2 b(static_cast<float>(segs[i].b.x), static_cast<float>(segs[i].b.y));
                if (Length(a - c) > rad || Length(b - c) > rad) return false;
            }
            return true;
        }

        return false;
    }
}
