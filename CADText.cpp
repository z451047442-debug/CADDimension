#include "CADText.h"
#include <algorithm>
#include <mouseman.h>

//--------------------------------------------------------------------------
// ClassDesc
//--------------------------------------------------------------------------
class CADTextClassDesc : public ClassDesc2
{
public:
    int          IsPublic() override               { return TRUE; }
    void*        Create(BOOL /*loading*/) override { return new CADText(); }
    const TCHAR* ClassName() override
    {
        const TCHAR* s = GetString(IDS_CADTEXT_CLASS_NAME);
        return s ? s : _T("CADText");
    }
    const TCHAR* NonLocalizedClassName() override { return _T("CADText"); }
    SClass_ID    SuperClassID() override          { return GEOMOBJECT_CLASS_ID; }
    Class_ID     ClassID() override               { return CADTEXT_CLASS_ID; }
    const TCHAR* Category() override              { return _T("CAD Tools"); }
    const TCHAR* InternalName() override          { return _T("CADText"); }
    HINSTANCE    HInstance() override             { return hInstance; }
};

ClassDesc2* GetCADTextDesc()
{
    static CADTextClassDesc desc;
    return &desc;
}

//--------------------------------------------------------------------------
// ParamBlock2
//--------------------------------------------------------------------------
static ParamBlockDesc2 cadtext_pb(
    cadtext_params, _T("CADTextParams"), IDS_CADTEXT_PARAMS, GetCADTextDesc(),
    P_AUTO_CONSTRUCT + P_AUTO_UI, 0,
    IDD_CADTEXT_PANEL, IDS_CADTEXT_PARAMS, 0, 0, NULL,
    p_text, _T("text"), TYPE_STRING, 0, 0,
        p_default, _T("CADText"),
        p_ui, TYPE_EDITBOX, IDC_TEXT_CONTENT_EDIT,
        p_nonLocalizedName, _T("Text"),
        p_end,
    p_text_size, _T("textSize"), TYPE_FLOAT, 0, 0,
        p_default, 12.0f,
        p_range, 6.0f, 72.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_CTEXT_SIZE_EDIT, IDC_CTEXT_SIZE_SPIN, 0.1f,
        p_nonLocalizedName, _T("Text Size"),
        p_end,
    p_end
);

//--------------------------------------------------------------------------
// CADText
//--------------------------------------------------------------------------
CADText::CADText()
{
    ivalid.SetInfinite();
    GetCADTextDesc()->MakeAutoParamBlocks(this);
    pblock2 = GetParamBlock(0);
}

void CADText::BuildMesh(TimeValue /*t*/)
{
    // Display-only object: keep an empty mesh.
    mesh.setNumVerts(0);
    mesh.setNumFaces(0);
}

const MCHAR* CADText::GetText(TimeValue t) const
{
    const MCHAR* s = nullptr;
    if (pblock2) {
        Interval iv;
        iv.SetInfinite();
        pblock2->GetValue(p_text, t, s, iv);
    }
    return s ? s : _T("CADText");
}

float CADText::GetTextSize(TimeValue t) const
{
    float s = 12.0f;
    if (pblock2) {
        Interval iv;
        iv.SetInfinite();
        pblock2->GetValue(p_text_size, t, s, iv);
    }
    return s;
}

void CADText::GetClassName(MSTR& s, bool localized) const
{
    const TCHAR* n = localized ? GetString(IDS_CADTEXT_CLASS_NAME) : nullptr;
    s = n ? n : _T("CADText");
}

void CADText::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    GetCADTextDesc()->BeginEditParams(ip, this, flags, prev);
}

void CADText::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    GetCADTextDesc()->EndEditParams(ip, this, flags, next);
}

RefTargetHandle CADText::Clone(RemapDir& remap)
{
    CADText* obj = new CADText();
    BaseClone(this, obj, remap);
    if (pblock2)
        obj->ReplaceReference(0, remap.CloneRef(pblock2));
    obj->ivalid.SetEmpty();
    return obj;
}

RefResult CADText::NotifyRefChanged(const Interval&, RefTargetHandle hTarget,
                                    PartID&, RefMessage message, BOOL)
{
    if (message == REFMSG_CHANGE || message == REFMSG_SUBANIM_STRUCTURE_CHANGED) {
        if (hTarget == pblock2) {
            ivalid.SetEmpty();
            NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
        }
    }
    return REF_SUCCEED;
}

Interval CADText::ObjectValidity(TimeValue t)
{
    Interval iv = SimpleObjectBase::ObjectValidity(t);
    iv &= ivalid;
    if (pblock2) pblock2->GetValidity(t, iv);
    return iv;
}

//--------------------------------------------------------------------------
// Bounding boxes
//--------------------------------------------------------------------------
static Box3 TextWorldBounds(INode* inode, TimeValue t, float textSize)
{
    const Point3 c = inode ? inode->GetObjectTM(t).GetTrans() : Point3(0, 0, 0);
    const float s = std::max(1.0f, textSize);
    Box3 b;
    b.pmin = c - Point3(s, s, s);
    b.pmax = c + Point3(s, s, s);
    return b;
}

void CADText::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp*, Box3& box)
{
    box = TextWorldBounds(inode, t, GetTextSize(t));
}

void CADText::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp*, Box3& box)
{
    const Box3 wb = TextWorldBounds(inode, t, GetTextSize(t));
    if (!inode) {
        box = wb;
        return;
    }
    Point3 corners[8];
    cad::BoxCorners(wb, corners);
    Matrix3 inv = cad::SafeInverse(inode->GetObjectTM(t));
    box.Init();
    box.IncludePoints(corners, 8, &inv);
}

void CADText::GetDeformBBox(TimeValue t, Box3& box, Matrix3* /*tm*/, BOOL)
{
    // Display-only object: use a local box around the node origin.
    const float s = std::max(1.0f, GetTextSize(t));
    box.pmin = Point3(-s, -s, -s);
    box.pmax = Point3(s, s, s);
}

//--------------------------------------------------------------------------
// Display / hit test
//--------------------------------------------------------------------------
int CADText::Display(TimeValue t, INode* inode, ViewExp* vpt, int)
{
    if (!vpt || !vpt->IsAlive() || !inode) return 0;
    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    cad::GWDrawGuard guard(gw);
    gw->setTransform(Matrix3());
    gw->setRndLimits((gw->getRndLimits() | GW_WIREFRAME) & ~GW_ILLUM);
    cad::SetupObjectColor(gw, inode);

    const float ns = cad::NonScalingFactor(vpt);
    const float ts = cad::ClampF(GetTextSize(t) * ns, 6.0f, 72.0f);
    gw->SetTextPointSize(ts);

    bool ok = false;
    Point2 c;
    cad::MapWorldToScreen(gw, inode->GetObjectTM(t).GetTrans(), c, ok);
    if (!ok) return 0;

    const float r = 4.0f * ns;
    cad::ScreenSeg cross[2];
    cross[0].a = IPoint2(cad::RoundI(c.x - r), cad::RoundI(c.y));
    cross[0].b = IPoint2(cad::RoundI(c.x + r), cad::RoundI(c.y));
    cross[1].a = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y - r));
    cross[1].b = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y + r));
    cad::DrawScreenSeg(gw, cross[0]);
    cad::DrawScreenSeg(gw, cross[1]);

    cad::ScreenLabel label;
    label.pos = cad::ToScreenPoint(Point2(c.x, c.y - ts));
    label.text = GetText(t);
    cad::DrawScreenLabel(gw, label);
    return 1;
}

int CADText::HitTest(TimeValue t, INode* inode, int type, int crossing,
                     int flags, IPoint2* p, ViewExp* vpt)
{
    if (!vpt || !vpt->IsAlive() || !inode || !p) return 0;
    if ((flags & HIT_SELONLY) && !inode->Selected()) return 0;
    if ((flags & HIT_UNSELONLY) && inode->Selected()) return 0;

    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    cad::GWDrawGuard guard(gw);
    gw->setTransform(Matrix3());

    const float ns = cad::NonScalingFactor(vpt);
    HitRegion hr;
    MakeHitRegion(hr, type, crossing,
                  static_cast<int>(cad::ClampF(5.0f * ns, 2.0f, 12.0f)), p);

    bool ok = false;
    Point2 c;
    cad::MapWorldToScreen(gw, inode->GetObjectTM(t).GetTrans(), c, ok);
    if (!ok) return 0;

    const float r = 4.0f * ns;
    cad::ScreenSeg cross[2];
    cross[0].a = IPoint2(cad::RoundI(c.x - r), cad::RoundI(c.y));
    cross[0].b = IPoint2(cad::RoundI(c.x + r), cad::RoundI(c.y));
    cross[1].a = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y - r));
    cross[1].b = IPoint2(cad::RoundI(c.x), cad::RoundI(c.y + r));
    return cad::HitSegments(hr, cross, 2) ? 1 : 0;
}

//--------------------------------------------------------------------------
// Create callback: single click placement.
//--------------------------------------------------------------------------
class CADTextCreateCallBack : public CreateMouseCallBack
{
    CADText* ob = nullptr;
public:
    void SetObj(CADText* obj) { ob = obj; }

    int proc(ViewExp* vpt, int msg, int, int, IPoint2 m, Matrix3& mat) override
    {
        if (!vpt || !vpt->IsAlive() || !ob) return FALSE;
        if (msg == MOUSE_FREEMOVE) {
            vpt->SnapPreview(m, m, nullptr, SNAP_IN_3D);
            return TRUE;
        }
        if (msg == MOUSE_ABORT || msg == MOUSE_PROPCLICK) {
            ob = nullptr;
            return CREATE_ABORT;
        }
        if (msg == MOUSE_POINT || msg == MOUSE_MOVE) {
            mat.SetTrans(vpt->SnapPoint(m, m, nullptr, SNAP_IN_3D));
            if (msg == MOUSE_MOVE) return TRUE;
            ob = nullptr;
            return 0;
        }
        return TRUE;
    }
};

static CADTextCreateCallBack g_cadtextCreateCB;

CreateMouseCallBack* CADText::GetCreateMouseCallBack()
{
    g_cadtextCreateCB.SetObj(this);
    return &g_cadtextCreateCB;
}
