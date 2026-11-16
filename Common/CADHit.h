#pragma once

#include "CADDraw.h"

namespace cad
{
    RECT NormalizedRect(const RECT& r);
    bool PointInRect(const IPoint2& p, const RECT& r);
    bool SegmentIntersectsRect(const IPoint2& a, const IPoint2& b, const RECT& r);
    float DistPointSegment(const Point2& p, const Point2& a, const Point2& b);

    // Screen-space hit test. POINT_RGN uses the region epsilon; RECT_RGN
    // respects crossing vs window; CIRCLE_RGN uses the circle radius.
    bool HitSegments(const HitRegion& hr, const ScreenSeg* segs, int nSegs);
}
