#include "CADLeader.h"
#include <algorithm>
#include <mouseman.h>

//--------------------------------------------------------------------------
// ClassDesc
//--------------------------------------------------------------------------
class CADLeaderClassDesc : public ClassDesc2
{
public:
    int          IsPublic() override               { return TRUE; }
    void*        Create(BOOL /*loading*/) override { return new CADLeader(); }
    const TCHAR* ClassName() override
    {
        const TCHAR* s = GetString(IDS_CADLEADER_CLASS_NAME);
        return s ? s : _T("CADLeader");
    }
    const TCHAR* NonLocalizedClassName() override { return _T("CADLeader"); }
    SClass_ID    SuperClassID() override          { return HELPER_CLASS_ID; }
    Class_ID     ClassID() override               { return CADLEADER_CLASS_ID; }
    const TCHAR* Category() override              { return _T("CAD Tools"); }
    const TCHAR* InternalName() override          { return _T("CADLeader"); }
    HINSTANCE    HInstance() override             { return hInstance; }
};

ClassDesc2* GetCADLeaderDesc()
{
    static CADLeaderClassDesc desc;
    return &desc;
}

//--------------------------------------------------------------------------
// ParamBlock2
//--------------------------------------------------------------------------
static ParamBlockDesc2 cadleader_pb(
    cadleader_params, _T("CADLeaderParams"), IDS_CADLEADER_PARAMS, GetCADLeaderDesc(),
    P_AUTO_CONSTRUCT + P_AUTO_UI, 0,
    IDD_CADLEADER_PANEL, IDS_CADLEADER_PARAMS, 0, 0, NULL,
    p_start, _T("start"), TYPE_POINT3, 0, 0,
        p_default, Point3(0, 0, 0),
        p_end,
    p_elbow, _T("elbow"), TYPE_POINT3, 0, 0,
        p_default, Point3(0, 0, 0),
        p_end,
    p_textpos, _T("textPos"), TYPE_POINT3, 0, 0,
        p_default, Point3(0, 0, 0),
        p_end,
    p_leader_text, _T("text"), TYPE_STRING, 0, 0,
        p_default, _T("Leader"),
        p_ui, TYPE_EDITBOX, IDC_LEADER_TEXT_EDIT,
        p_nonLocalizedName, _T("Text"),
        p_end,
    p_leader_size, _T("textSize"), TYPE_FLOAT, 0, 0,
        p_default, 12.0f,
        p_range, 6.0f, 72.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_LEADER_SIZE_EDIT, IDC_LEADER_SIZE_SPIN, 0.1f,
        p_nonLocalizedName, _T("Text Size"),
        p_end,
    p_leader_arrow, _T("arrowSize"), TYPE_FLOAT, 0, 0,
        p_default, 7.0f,
        p_range, 4.0f, 20.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_LEADER_ARROW_EDIT, IDC_LEADER_ARROW_SPIN, 0.1f,
        p_nonLocalizedName, _T("Arrow Size"),
        p_end,
    p_end
);

//--------------------------------------------------------------------------
// CADLeader
//--------------------------------------------------------------------------
CADLeader::CADLeader()
{
    ivalid.SetInfinite();
    GetCADLeaderDesc()->MakeAutoParamBlocks(this);
    pblock = GetParamBlock(0);
}

void CADLeader::GetClassName(MSTR& s, bool localized) const
{
    const TCHAR* n = localized ? GetString(IDS_CADLEADER_CLASS_NAME) : nullptr;
    s = n ? n : _T("CADLeader");
}

void CADLeader::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    GetCADLeaderDesc()->BeginEditParams(ip, this, flags, prev);
}

void CADLeader::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    GetCADLeaderDesc()->EndEditParams(ip, this, flags, next);
}

RefTargetHandle CADLeader::Clone(RemapDir& remap)
{
    CADLeader* obj = new CADLeader();
    BaseClone(this, obj, remap);
    if (pblock)
        obj->ReplaceReference(0, remap.CloneRef(pblock));
    obj->ivalid.SetEmpty();
    return obj;
}

RefResult CADLeader::NotifyRefChanged(const Interval&, RefTargetHandle hTarget,
                                      PartID&, RefMessage message, BOOL)
{
    if (message == REFMSG_CHANGE || message == REFMSG_SUBANIM_STRUCTURE_CHANGED) {
        if (hTarget == pblock) {
            ivalid.SetEmpty();
            NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
        }
    }
    return REF_SUCCEED;
}

MSTR CADLeader::SubAnimName(int /*i*/, bool localized)
{
    const TCHAR* n = localized ? GetString(IDS_CADLEADER_PARAMS) : _T("Parameters");
    return n ? n : _T("Parameters");
}

ObjectState CADLeader::Eval(TimeValue)
{
    return ObjectState(this);
}

Interval CADLeader::ObjectValidity(TimeValue t)
{
    Interval iv = HelperObject::ObjectValidity(t);
    iv &= ivalid;
    if (pblock) pblock->GetValidity(t, iv);
    return iv;
}

bool CADLeader::GetLeaderPoints(TimeValue t, Point3& start, Point3& elbow, Point3& text) const
{
    if (m_leaderCreating) {
        start = Point3(0, 0, 0);
        elbow = m_elbow;
        text = m_text;
        return m_hasElbow;
    }
    if (!pblock) return false;
    Interval iv;
    iv.SetInfinite();
    pblock->GetValue(p_start, t, start, iv);
    pblock->GetValue(p_elbow, t, elbow, iv);
    pblock->GetValue(p_textpos, t, text, iv);
    return true;
}

bool CADLeader::GetLeaderWorldPoints(TimeValue t, INode* inode,
                                     Point3& start, Point3& elbow, Point3& text) const
{
    if (!inode) return false;
    Point3 s, e, p;
    if (!GetLeaderPoints(t, s, e, p)) return false;
    const Matrix3 tm = inode->GetObjectTM(t);
    start = s * tm;
    elbow = e * tm;
    text = p * tm;
    return true;
}

void CADLeader::SetCreateStart(const Point3& world)
{
    m_leaderCreating = true;
    m_hasElbow = false;
    m_start = world;
    m_elbow = Point3(0, 0, 0);
    m_text = Point3(0, 0, 0);
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void CADLeader::SetCreateElbow(const Point3& localElbow)
{
    m_leaderCreating = true;
    m_hasElbow = true;
    m_elbow = localElbow;
    m_text = localElbow;
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void CADLeader::SetCreateText(const Point3& localText)
{
    if (!m_leaderCreating || !m_hasElbow) return;
    m_text = localText;
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void CADLeader::CommitCreate(const Point3& worldStart,
                             const Point3& localElbow,
                             const Point3& localText)
{
    (void)worldStart;
    if (!pblock) {
        CancelCreate();
        return;
    }
    pblock->SetValue(p_start, 0, Point3(0, 0, 0));
    pblock->SetValue(p_elbow, 0, localElbow);
    pblock->SetValue(p_textpos, 0, localText);
    m_leaderCreating = false;
    m_hasElbow = false;
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void CADLeader::CancelCreate()
{
    m_leaderCreating = false;
    m_hasElbow = false;
    m_elbow = Point3(0, 0, 0);
    m_text = Point3(0, 0, 0);
    ivalid.SetEmpty();
    NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

//--------------------------------------------------------------------------
// Bounding boxes
//--------------------------------------------------------------------------
void CADLeader::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp*, Box3& box)
{
    Point3 s, e, p;
    if (GetLeaderWorldPoints(t, inode, s, e, p)) {
        box.Init();
        box.IncludePoints(&s, 1);
        box.IncludePoints(&e, 1);
        box.IncludePoints(&p, 1);
        const Point3 size = box.pmax - box.pmin;
        box.EnlargeBy(Length(size) * 0.1f + 1.0f);
        return;
    }
    const Point3 c = inode ? inode->GetObjectTM(t).GetTrans() : Point3(0, 0, 0);
    box.pmin = c - Point3(1, 1, 1);
    box.pmax = c + Point3(1, 1, 1);
}

void CADLeader::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    Box3 wb;
    GetWorldBoundBox(t, inode, vp, wb);
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

//--------------------------------------------------------------------------
// Display / hit test
//--------------------------------------------------------------------------
int CADLeader::Display(TimeValue t, INode* inode, ViewExp* vpt, int)
{
    if (!vpt || !vpt->IsAlive() || !inode) return 0;
    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    Point3 s, e, p;
    if (!GetLeaderWorldPoints(t, inode, s, e, p)) return 0;

    cad::GWDrawGuard guard(gw);
    gw->setTransform(Matrix3());
    gw->setRndLimits((gw->getRndLimits() | GW_WIREFRAME) & ~GW_ILLUM);
    cad::SetupObjectColor(gw, inode);

    float textPt = 12.0f, arrowPx = 7.0f;
    const MCHAR* text = _T("Leader");
    if (pblock) {
        Interval iv;
        iv.SetInfinite();
        pblock->GetValue(p_leader_size, t, textPt, iv);
        pblock->GetValue(p_leader_arrow, t, arrowPx, iv);
        pblock->GetValue(p_leader_text, t, text, iv);
    }
    const float ns = cad::NonScalingFactor(vpt);
    gw->SetTextPointSize(cad::ClampF(textPt * ns, 6.0f, 72.0f));

    Point2 ss, se, sp;
    bool oks = false, oke = false, okp = false;
    cad::MapWorldToScreen(gw, s, ss, oks);
    cad::MapWorldToScreen(gw, e, se, oke);
    cad::MapWorldToScreen(gw, p, sp, okp);
    if (!oks || !oke) return 0;

    cad::ScreenSeg segs[4];
    int n = 0;
    segs[n].a = cad::ToScreenPoint(ss);
    segs[n].b = cad::ToScreenPoint(se);
    ++n;
    if (okp) {
        segs[n].a = cad::ToScreenPoint(se);
        segs[n].b = cad::ToScreenPoint(sp);
        ++n;
    }

    // Arrowhead at the start, pointing along start -> elbow.
    const Point2 d = se - ss;
    const float len = Length(d);
    if (len > 1.0e-3f) {
        const Point2 dir = d / len;
        const Point2 perp(-dir.y, dir.x);
        const float arrow = std::min(arrowPx * ns, len * 0.35f);
        segs[n].a = cad::ToScreenPoint(ss);
        segs[n].b = cad::ToScreenPoint(ss + dir * arrow + perp * (arrow * 0.45f));
        ++n;
        segs[n].a = cad::ToScreenPoint(ss);
        segs[n].b = cad::ToScreenPoint(ss + dir * arrow - perp * (arrow * 0.45f));
        ++n;
    }

    for (int i = 0; i < n; ++i)
        cad::DrawScreenSeg(gw, segs[i]);

    if (okp) {
        cad::ScreenLabel label;
        label.pos = cad::ToScreenPoint(sp);
        label.text = text ? text : _T("Leader");
        cad::DrawScreenLabel(gw, label);
    }
    return 1;
}

int CADLeader::HitTest(TimeValue t, INode* inode, int type, int crossing,
                       int flags, IPoint2* p, ViewExp* vpt)
{
    if (!vpt || !vpt->IsAlive() || !inode || !p) return 0;
    if ((flags & HIT_SELONLY) && !inode->Selected()) return 0;
    if ((flags & HIT_UNSELONLY) && inode->Selected()) return 0;

    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    Point3 s, e, tp;
    if (!GetLeaderWorldPoints(t, inode, s, e, tp)) return 0;

    cad::GWDrawGuard guard(gw);
    gw->setTransform(Matrix3());

    const float ns = cad::NonScalingFactor(vpt);
    HitRegion hr;
    MakeHitRegion(hr, type, crossing,
                  static_cast<int>(cad::ClampF(5.0f * ns, 2.0f, 12.0f)), p);

    Point2 ss, se, sp;
    bool oks = false, oke = false, okp = false;
    cad::MapWorldToScreen(gw, s, ss, oks);
    cad::MapWorldToScreen(gw, e, se, oke);
    cad::MapWorldToScreen(gw, tp, sp, okp);
    if (!oks || !oke) return 0;

    cad::ScreenSeg segs[3];
    int n = 0;
    segs[n].a = cad::ToScreenPoint(ss);
    segs[n].b = cad::ToScreenPoint(se);
    ++n;
    if (okp) {
        segs[n].a = cad::ToScreenPoint(se);
        segs[n].b = cad::ToScreenPoint(sp);
        ++n;
    }
    return cad::HitSegments(hr, segs, n) ? 1 : 0;
}

//--------------------------------------------------------------------------
// Create callback: start -> elbow -> text position.
//--------------------------------------------------------------------------
class CADLeaderCreateCallBack : public CreateMouseCallBack
{
    CADLeader* ob = nullptr;
    int    stage = 0;
    Point3 origin = Point3(0, 0, 0);
    Point3 elbow = Point3(0, 0, 0);

public:
    void SetObj(CADLeader* obj) { ob = obj; stage = 0; }

    int proc(ViewExp* vpt, int msg, int, int, IPoint2 m, Matrix3& mat) override
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
                if (stage == 0) mat.SetTrans(world);
                else if (stage == 2) ob->SetCreateText(world - origin);
                return TRUE;
            }

            if (stage == 0) {
                origin = world;
                mat.SetTrans(origin);
                ob->SetCreateStart(origin);
                stage = 1;
                return TRUE;
            }

            if (stage == 1) {
                const Point3 local = world - origin;
                if (Length(local) < 1.0e-4f) return TRUE;
                elbow = local;
                ob->SetCreateElbow(local);
                stage = 2;
                return TRUE;
            }

            ob->CommitCreate(origin, elbow, world - origin);
            stage = 0;
            ob = nullptr;
            return 0;
        }
        return TRUE;
    }
};

static CADLeaderCreateCallBack g_cadleaderCreateCB;

CreateMouseCallBack* CADLeader::GetCreateMouseCallBack()
{
    g_cadleaderCreateCB.SetObj(this);
    return &g_cadleaderCreateCB;
}
