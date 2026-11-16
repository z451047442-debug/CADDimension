#include "CADTable.h"
#include <algorithm>
#include <mouseman.h>

namespace
{
    constexpr int kMaxSegs = 256;
    constexpr int kMaxLabels = 900;
}

//--------------------------------------------------------------------------
// Accessor: keep the string tab in sync with rows/cols.
//--------------------------------------------------------------------------
class CADTablePBAccessor : public PBAccessor
{
public:
    void PostSet(const PB2Value&, ReferenceMaker* owner, ParamID id,
                 int, TimeValue t) override
    {
        if (!owner) return;
        CADTable* table = static_cast<CADTable*>(owner);
        if (id == p_rows || id == p_cols) {
            table->SyncCells(t);
        }
        table->ivalid.SetEmpty();
        table->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
    }
};

static CADTablePBAccessor g_tableAccessor;

//--------------------------------------------------------------------------
// ClassDesc
//--------------------------------------------------------------------------
class CADTableClassDesc : public ClassDesc2
{
public:
    int          IsPublic() override               { return TRUE; }
    void*        Create(BOOL /*loading*/) override { return new CADTable(); }
    const TCHAR* ClassName() override
    {
        const TCHAR* s = GetString(IDS_CADTABLE_CLASS_NAME);
        return s ? s : _T("CADTable");
    }
    const TCHAR* NonLocalizedClassName() override { return _T("CADTable"); }
    SClass_ID    SuperClassID() override          { return HELPER_CLASS_ID; }
    Class_ID     ClassID() override               { return CADTABLE_CLASS_ID; }
    const TCHAR* Category() override              { return _T("CAD Tools"); }
    const TCHAR* InternalName() override          { return _T("CADTable"); }
    HINSTANCE    HInstance() override             { return hInstance; }
};

ClassDesc2* GetCADTableDesc()
{
    static CADTableClassDesc desc;
    return &desc;
}

//--------------------------------------------------------------------------
// ParamBlock2
//--------------------------------------------------------------------------
static ParamBlockDesc2 cadtable_pb(
    cadtable_params, _T("CADTableParams"), IDS_CADTABLE_PARAMS, GetCADTableDesc(),
    P_AUTO_CONSTRUCT + P_AUTO_UI, 0,
    IDD_CADTABLE_PANEL, IDS_CADTABLE_PARAMS, 0, 0, NULL,
    p_rows, _T("rows"), TYPE_INT, 0, IDS_TABLE_ROWS,
        p_default, 20,
        p_range, 1, 40,
        p_ui, TYPE_SPINNER, EDITTYPE_INT, IDC_TABLE_ROWS_EDIT, IDC_TABLE_ROWS_SPIN, 1.0f,
        p_accessor, &g_tableAccessor,
        p_end,
    p_cols, _T("cols"), TYPE_INT, 0, IDS_TABLE_COLS,
        p_default, 20,
        p_range, 1, 20,
        p_ui, TYPE_SPINNER, EDITTYPE_INT, IDC_TABLE_COLS_EDIT, IDC_TABLE_COLS_SPIN, 1.0f,
        p_accessor, &g_tableAccessor,
        p_end,
    p_cell_w, _T("cellWidth"), TYPE_FLOAT, 0, IDS_TABLE_CELL_W,
        p_default, 80.0f,
        p_range, 40.0f, 400.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_TABLE_CW_EDIT, IDC_TABLE_CW_SPIN, 1.0f,
        p_accessor, &g_tableAccessor,
        p_end,
    p_cell_h, _T("cellHeight"), TYPE_FLOAT, 0, IDS_TABLE_CELL_H,
        p_default, 24.0f,
        p_range, 16.0f, 120.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_TABLE_CH_EDIT, IDC_TABLE_CH_SPIN, 1.0f,
        p_accessor, &g_tableAccessor,
        p_end,
    p_table_text_size, _T("textSize"), TYPE_FLOAT, 0, IDS_TABLE_TEXT_SIZE,
        p_default, 12.0f,
        p_range, 6.0f, 72.0f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_TABLE_TS_EDIT, IDC_TABLE_TS_SPIN, 0.1f,
        p_accessor, &g_tableAccessor,
        p_end,
    p_cells, _T("cells"), TYPE_STRING_TAB, 400, P_VARIABLE_SIZE, IDS_CADTABLE_CELLS,
        p_end,
    p_end
);

//--------------------------------------------------------------------------
// CADTable
//--------------------------------------------------------------------------
CADTable::CADTable()
{
    ivalid.SetInfinite();
    GetCADTableDesc()->MakeAutoParamBlocks(this);
    pblock = GetParamBlock(0);
    SyncCells(0);
}

void CADTable::GetClassName(MSTR& s, bool localized) const
{
    const TCHAR* n = localized ? GetString(IDS_CADTABLE_CLASS_NAME) : nullptr;
    s = n ? n : _T("CADTable");
}

void CADTable::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    GetCADTableDesc()->BeginEditParams(ip, this, flags, prev);
}

void CADTable::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    GetCADTableDesc()->EndEditParams(ip, this, flags, next);
}

RefTargetHandle CADTable::Clone(RemapDir& remap)
{
    CADTable* obj = new CADTable();
    BaseClone(this, obj, remap);
    if (pblock)
        obj->ReplaceReference(0, remap.CloneRef(pblock));
    obj->ivalid.SetEmpty();
    return obj;
}

RefResult CADTable::NotifyRefChanged(const Interval&, RefTargetHandle hTarget,
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

MSTR CADTable::SubAnimName(int /*i*/, bool localized)
{
    const TCHAR* n = localized ? GetString(IDS_CADTABLE_PARAMS) : _T("Parameters");
    return n ? n : _T("Parameters");
}

ObjectState CADTable::Eval(TimeValue)
{
    return ObjectState(this);
}

Interval CADTable::ObjectValidity(TimeValue t)
{
    Interval iv = HelperObject::ObjectValidity(t);
    iv &= ivalid;
    if (pblock) pblock->GetValidity(t, iv);
    return iv;
}

void CADTable::ReadTableParams(TimeValue t, int& rows, int& cols,
                               float& cellW, float& cellH, float& textSize) const
{
    rows = 20;
    cols = 20;
    cellW = 80.0f;
    cellH = 24.0f;
    textSize = 12.0f;
    if (!pblock) return;

    Interval iv;
    iv.SetInfinite();
    pblock->GetValue(p_rows, t, rows, iv);
    pblock->GetValue(p_cols, t, cols, iv);
    pblock->GetValue(p_cell_w, t, cellW, iv);
    pblock->GetValue(p_cell_h, t, cellH, iv);
    pblock->GetValue(p_table_text_size, t, textSize, iv);

    rows = std::max(1, std::min(40, rows));
    cols = std::max(1, std::min(20, cols));
    cellW = std::max(40.0f, std::min(400.0f, cellW));
    cellH = std::max(16.0f, std::min(120.0f, cellH));
    textSize = std::max(6.0f, std::min(72.0f, textSize));
}

void CADTable::SyncCells(TimeValue t)
{
    if (!pblock) return;

    int rows = 20, cols = 20;
    float cw = 80.0f, ch = 24.0f, ts = 12.0f;
    ReadTableParams(t, rows, cols, cw, ch, ts);

    const int n = rows * cols;
    const int oldN = pblock->Count(p_cells);
    if (n == oldN) return;

    Interval iv;
    iv.SetInfinite();
    std::vector<MSTR> old(static_cast<size_t>(oldN));
    for (int i = 0; i < oldN; ++i) {
        const MCHAR* s = nullptr;
        pblock->GetValue(p_cells, t, s, iv, i);
        old[static_cast<size_t>(i)] = s ? s : _T("");
    }

    pblock->SetCount(p_cells, n);
    const int keep = std::min(oldN, n);
    for (int i = 0; i < keep; ++i)
        pblock->SetValue(p_cells, t, old[static_cast<size_t>(i)].data(), i);
}

//--------------------------------------------------------------------------
// Bounding boxes (screen-constant table; world box estimated from viewport).
//--------------------------------------------------------------------------
void CADTable::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    int rows = 20, cols = 20;
    float cw = 80.0f, ch = 24.0f, ts = 12.0f;
    ReadTableParams(t, rows, cols, cw, ch, ts);

    const Point3 c = inode ? inode->GetObjectTM(t).GetTrans() : Point3(0, 0, 0);
    float wx = 10.0f, wy = 10.0f;
    if (vp && vp->IsAlive()) {
        GraphicsWindow* gw = vp->getGW();
        const int winW = gw ? gw->getWinSizeX() : 0;
        if (winW > 0) {
            const float perPixel = vp->GetVPWorldWidth(c) / static_cast<float>(winW);
            const float ns = cad::NonScalingFactor(vp);
            wx = perPixel * (cols * cw * ns) * 0.5f + perPixel * 10.0f;
            wy = perPixel * (rows * ch * ns) * 0.5f + perPixel * 10.0f;
        }
    }
    box.pmin = c - Point3(wx, wy, wx);
    box.pmax = c + Point3(wx, wy, wx);
}

void CADTable::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    Box3 wb;
    GetWorldBoundBox(t, inode, vp, wb);
    if (!inode) {
        box = wb;
        return;
    }
    Point3 corners[8];
    cad::BoxCorners(wb, corners);
    Matrix3 inv = Inverse(inode->GetObjectTM(t));
    box.Init();
    box.IncludePoints(corners, 8, &inv);
}

//--------------------------------------------------------------------------
// Display / hit test
//--------------------------------------------------------------------------
int CADTable::BuildTablePrimitives(TimeValue t, ViewExp* vpt, const Point2& origin,
                                   cad::ScreenSeg* segs, int maxSegs,
                                   cad::ScreenLabel* labels, int maxLabels,
                                   int& nLabels) const
{
    nLabels = 0;
    if (!vpt || !vpt->IsAlive()) return 0;

    int rows = 20, cols = 20;
    float cellW = 80.0f, cellH = 24.0f, textSize = 12.0f;
    ReadTableParams(t, rows, cols, cellW, cellH, textSize);

    const float ns = cad::NonScalingFactor(vpt);
    const float cw = std::max(4.0f, cellW * ns);
    const float ch = std::max(4.0f, cellH * ns);
    const float totalW = cols * cw;
    const float totalH = rows * ch;

    int nSeg = 0;
    auto addSeg = [&](float x1, float y1, float x2, float y2)
    {
        if (!segs || nSeg >= maxSegs) return;
        segs[nSeg].a = IPoint2(cad::RoundI(x1), cad::RoundI(y1));
        segs[nSeg].b = IPoint2(cad::RoundI(x2), cad::RoundI(y2));
        ++nSeg;
    };

    for (int r = 0; r <= rows; ++r) {
        const float y = origin.y + r * ch;
        addSeg(origin.x, y, origin.x + totalW, y);
    }
    for (int c = 0; c <= cols; ++c) {
        const float x = origin.x + c * cw;
        addSeg(x, origin.y, x, origin.y + totalH);
    }

    if (labels && pblock) {
        Interval iv;
        iv.SetInfinite();
        for (int r = 0; r < rows && nLabels < maxLabels; ++r) {
            for (int c = 0; c < cols && nLabels < maxLabels; ++c) {
                const int idx = r * cols + c;
                const MCHAR* s = nullptr;
                pblock->GetValue(p_cells, t, s, iv, idx);
                if (!s || !s[0]) continue;
                MCHAR buf[64];
                _tcsncpy_s(buf, _countof(buf), s, _TRUNCATE);
                labels[nLabels].pos = IPoint2(
                    cad::RoundI(origin.x + c * cw + cw * 0.5f),
                    cad::RoundI(origin.y + r * ch + ch * 0.5f));
                labels[nLabels].text = buf;
                ++nLabels;
            }
        }
    }

    return nSeg;
}

int CADTable::Display(TimeValue t, INode* inode, ViewExp* vpt, int)
{
    if (!vpt || !vpt->IsAlive() || !inode) return 0;
    GraphicsWindow* gw = vpt->getGW();
    if (!gw) return 0;

    cad::GWDrawGuard guard(gw);
    gw->setTransform(Matrix3());
    gw->setRndLimits((gw->getRndLimits() | GW_WIREFRAME) & ~GW_ILLUM);
    cad::SetupObjectColor(gw, inode);

    int rows = 20, cols = 20;
    float cw = 80.0f, ch = 24.0f, ts = 12.0f;
    ReadTableParams(t, rows, cols, cw, ch, ts);
    const float ns = cad::NonScalingFactor(vpt);
    gw->SetTextPointSize(cad::ClampF(ts * ns, 6.0f, 72.0f));

    bool ok = false;
    Point2 origin;
    cad::MapWorldToScreen(gw, inode->GetObjectTM(t).GetTrans(), origin, ok);
    if (!ok) return 0;

    cad::ScreenSeg   segs[kMaxSegs];
    cad::ScreenLabel labels[kMaxLabels];
    int nLabels = 0;
    const int nSeg = BuildTablePrimitives(t, vpt, origin, segs, kMaxSegs,
                                          labels, kMaxLabels, nLabels);
    if (nSeg <= 0) return 0;

    for (int i = 0; i < nSeg; ++i)
        cad::DrawScreenSeg(gw, segs[i]);
    for (int i = 0; i < nLabels; ++i)
        cad::DrawScreenLabel(gw, labels[i]);

    return 1;
}

int CADTable::HitTest(TimeValue t, INode* inode, int type, int crossing,
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
    Point2 origin;
    cad::MapWorldToScreen(gw, inode->GetObjectTM(t).GetTrans(), origin, ok);
    if (!ok) return 0;

    cad::ScreenSeg segs[kMaxSegs];
    cad::ScreenLabel labels[kMaxLabels];
    int nLabels = 0;
    const int nSeg = BuildTablePrimitives(t, vpt, origin, segs, kMaxSegs,
                                          labels, kMaxLabels, nLabels);
    if (nSeg <= 0) return 0;
    return cad::HitSegments(hr, segs, nSeg) ? 1 : 0;
}

//--------------------------------------------------------------------------
// Create callback: single click placement.
//--------------------------------------------------------------------------
class CADTableCreateCallBack : public CreateMouseCallBack
{
    CADTable* ob = nullptr;
public:
    void SetObj(CADTable* obj) { ob = obj; }

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

static CADTableCreateCallBack g_cadtableCreateCB;

CreateMouseCallBack* CADTable::GetCreateMouseCallBack()
{
    g_cadtableCreateCB.SetObj(this);
    return &g_cadtableCreateCB;
}
