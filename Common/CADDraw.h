#pragma once

//**************************************************************************/
// Common/CADDraw - shared viewport drawing helpers for the CAD annotation
// bundle. These helpers deliberately work in screen space so that line width,
// arrow size and text size do not change with viewport zoom.
//**************************************************************************/

#include "3dsmaxsdk_preinclude.h"
#include <max.h>
#include <GraphicsWindow.h>

namespace cad
{
    struct ScreenSeg
    {
        IPoint2 a;
        IPoint2 b;
    };

    struct ScreenLabel
    {
        IPoint2 pos;
        MSTR    text;
    };

    float   ClampF(float v, float lo, float hi);
    int     RoundI(float v);
    IPoint2 ToScreenPoint(const Point2& p);
    float   NonScalingFactor(ViewExp* vpt);
    void    BoxCorners(const Box3& b, Point3 c[8]);

    // Safe inversion for node/object transforms. Normal matrices use
    // Inverse(); near-singular or non-finite transforms fall back to
    // PseudoInverse(), and any remaining non-finite result is replaced by
    // identity so NaN/Inf never reaches Box3.
    Matrix3 SafeInverse(const Matrix3& m);

    // World -> screen. Caller must set the GraphicsWindow transform to identity.
    // `ok` is false when the point is behind the perspective camera or clipped.
    bool MapWorldToScreen(GraphicsWindow* gw, const Point3& world,
                          Point2& screen, bool& ok);

    bool ClipSegmentToRect(Point2& a, Point2& b,
                           float xmin, float ymin, float xmax, float ymax);

    void DrawScreenSeg(GraphicsWindow* gw, const ScreenSeg& seg);
    void DrawScreenLabel(GraphicsWindow* gw, const ScreenLabel& label);

    void SetColor(GraphicsWindow* gw, const Point3& color);
    void SetupObjectColor(GraphicsWindow* gw, INode* inode);

    // Saves/restores transform, render limits and text point size. There is no
    // GraphicsWindow::getColor(), so callers must set the object color after
    // construction; the previous color cannot be restored.
    class GWDrawGuard
    {
    public:
        explicit GWDrawGuard(GraphicsWindow* gw);
        ~GWDrawGuard();

        GWDrawGuard(const GWDrawGuard&) = delete;
        GWDrawGuard& operator=(const GWDrawGuard&) = delete;

    private:
        GraphicsWindow* m_gw;
        Matrix3         m_tm;
        DWORD           m_limits;
        float           m_textPt;
    };
}
