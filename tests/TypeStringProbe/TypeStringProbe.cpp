//**************************************************************************/
// TypeStringProbe - M0 probe plugin (not part of the production bundle)
//
// Purpose:
//   Verify that ParamBlock2 TYPE_STRING_TAB can be:
//     - sized with IParamBlock2::SetCount()
//     - written/read element-wise from C++ (SetValue/GetValue + tabIndex)
//     - read/written element-wise from MAXScript
//     - undone, cloned, saved and loaded
//
// It is a deliberately minimal GeomObject: no drawing, no UI, no resources.
//**************************************************************************/

#include "../../3dsmaxsdk_preinclude.h"
#include <max.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <istdplug.h>
#include <mouseman.h>
#include "resource.h"

#define TSP_CLASS_ID Class_ID(0x7a110001, 0x51aa0001)

HINSTANCE hInstance = nullptr;

enum { tsp_params = 0 };
enum { p_rows = 0, p_cols, p_cells, p_cpp_readback };
enum { kPBlockRef = 0 };

class TypeStringProbe;

//--------------------------------------------------------------------------
// ClassDesc (Create is defined after TypeStringProbe)
//--------------------------------------------------------------------------
class TSPDesc : public ClassDesc2
{
public:
    int          IsPublic() override;
    void*        Create(BOOL loading) override;
    const TCHAR* ClassName() override;
    const TCHAR* NonLocalizedClassName() override;
    SClass_ID    SuperClassID() override;
    Class_ID     ClassID() override;
    const TCHAR* Category() override;
    const TCHAR* InternalName() override;
    HINSTANCE    HInstance() override;
};

static TSPDesc g_tspDesc;
static ClassDesc2* GetTSPDesc() { return &g_tspDesc; }

//--------------------------------------------------------------------------
// Minimal object
//--------------------------------------------------------------------------
class TypeStringProbe : public GeomObject
{
public:
    IParamBlock2* pblock = nullptr;
    Interval      ivalid;

    TypeStringProbe();
    ~TypeStringProbe() override { DeleteAllRefsFromMe(); }

    void Create();

    Class_ID    ClassID() override { return TSP_CLASS_ID; }
    SClass_ID   SuperClassID() override { return GEOMOBJECT_CLASS_ID; }
    void        GetClassName(MSTR& s, bool /*localized*/) const override { s = _T("TypeStringProbe"); }
    void        DeleteThis() override { delete this; }
    void        BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override
    {
        GetTSPDesc()->BeginEditParams(ip, this, flags, prev);
    }
    void        EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override
    {
        GetTSPDesc()->EndEditParams(ip, this, flags, next);
    }

    RefTargetHandle Clone(RemapDir& remap) override;
    RefResult NotifyRefChanged(const Interval&, RefTargetHandle, PartID&,
                               RefMessage, BOOL) override { return REF_SUCCEED; }
    int             NumRefs() override { return 1; }
    RefTargetHandle GetReference(int i) override { return i == kPBlockRef ? pblock : nullptr; }
    void            SetReference(int i, RefTargetHandle rtarg) override
    {
        if (i == kPBlockRef) pblock = static_cast<IParamBlock2*>(rtarg);
    }
    int             NumSubs() override { return 1; }
    Animatable*     SubAnim(int i) override { return i == 0 ? pblock : nullptr; }
    MSTR            SubAnimName(int /*i*/, bool /*localized*/) override { return _T("Parameters"); }
    int             NumParamBlocks() override { return 1; }
    IParamBlock2*   GetParamBlock(int i) override { return i == 0 ? pblock : nullptr; }
    IParamBlock2*   GetParamBlockByID(BlockID id) override
    {
        return (pblock && pblock->ID() == id) ? pblock : nullptr;
    }

    CreateMouseCallBack* GetCreateMouseCallBack() override;
    int      IsRenderable() override { return 0; }
    void     InitNodeName(MSTR& s) override { s = _T("TypeStringProbe"); }
    ObjectState Eval(TimeValue) override { return ObjectState(this); }
    Interval ObjectValidity(TimeValue t) override
    {
        Interval iv = Object::ObjectValidity(t);
        iv &= ivalid;
        return iv;
    }
    void GetWorldBoundBox(TimeValue, INode*, ViewExp*, Box3& box) override
    {
        box.pmin = Point3(-1, -1, -1);
        box.pmax = Point3(1, 1, 1);
    }
    void GetLocalBoundBox(TimeValue, INode*, ViewExp*, Box3& box) override
    {
        box.pmin = Point3(-1, -1, -1);
        box.pmax = Point3(1, 1, 1);
    }
    int Display(TimeValue, INode*, ViewExp*, int) override { return 0; }
    int HitTest(TimeValue, INode*, int, int, int, IPoint2*, ViewExp*) override { return 0; }
};

//--------------------------------------------------------------------------
// C++ readback: PBAccessor::Get computes "cppReadback" from cells[0].
// Reading it from MAXScript proves the C++ GetValue path works.
//--------------------------------------------------------------------------
class TSPAccessor : public PBAccessor
{
public:
    void Get(PB2Value& v, ReferenceMaker* owner, ParamID id,
             int /*tabIndex*/, TimeValue t, Interval& valid) override
    {
        if (id != p_cpp_readback || !owner) return;
        TypeStringProbe* probe = static_cast<TypeStringProbe*>(owner);
        if (!probe->pblock) return;

        const MCHAR* cell = nullptr;
        Interval iv;
        iv.SetInfinite();
        probe->pblock->GetValue(p_cells, t, cell, iv, 0);

        const int n = probe->pblock->Count(p_cells);
        MSTR s = _T("CPP:");
        if (cell) s += cell;
        MCHAR buf[32];
        _stprintf_s(buf, _countof(buf), _T("|n=%d"), n);
        s += buf;

        v.ReplaceString(s.data());
        valid.SetInfinite();
    }
};

static TSPAccessor g_tspAccessor;

//--------------------------------------------------------------------------
// ParamBlock2: one string tab, initially 4 cells, variable size.
//--------------------------------------------------------------------------
static ParamBlockDesc2 tsp_pb(
    tsp_params, _T("TSPParams"), IDS_TSP_PARAMS, GetTSPDesc(),
    P_AUTO_CONSTRUCT, kPBlockRef,
    p_rows, _T("rows"), TYPE_INT, 0, IDS_TSP_ROWS,
        p_default, 2,
        p_range, 1, 20,
        p_end,
    p_cols, _T("cols"), TYPE_INT, 0, IDS_TSP_COLS,
        p_default, 2,
        p_range, 1, 20,
        p_end,
    p_cells, _T("cells"), TYPE_STRING_TAB, 4, P_VARIABLE_SIZE, IDS_TSP_CELLS,
        p_accessor, &g_tspAccessor,
        p_end,
    p_cpp_readback, _T("cppReadback"), TYPE_STRING, 0, IDS_TSP_CPP,
        p_accessor, &g_tspAccessor,
        p_end,
    p_end
);

//--------------------------------------------------------------------------
// ClassDesc method definitions (no virt-specifier on out-of-class definitions)
//--------------------------------------------------------------------------
int          TSPDesc::IsPublic()               { return TRUE; }
void*        TSPDesc::Create(BOOL /*loading*/) { return new TypeStringProbe(); }
const TCHAR* TSPDesc::ClassName()              { return _T("TypeStringProbe"); }
const TCHAR* TSPDesc::NonLocalizedClassName()  { return _T("TypeStringProbe"); }
SClass_ID    TSPDesc::SuperClassID()           { return GEOMOBJECT_CLASS_ID; }
Class_ID     TSPDesc::ClassID()                { return TSP_CLASS_ID; }
const TCHAR* TSPDesc::Category()               { return _T("CAD Tools Probe"); }
const TCHAR* TSPDesc::InternalName()           { return _T("TypeStringProbe"); }
HINSTANCE    TSPDesc::HInstance()              { return hInstance; }

//--------------------------------------------------------------------------
// CreateMouseCallBack: one click places the object; no drawing.
//--------------------------------------------------------------------------
class TSPCreateCallBack : public CreateMouseCallBack
{
public:
    int proc(ViewExp*, int msg, int, int, IPoint2, Matrix3&) override
    {
        if (msg == MOUSE_POINT) return 0;
        if (msg == MOUSE_ABORT || msg == MOUSE_PROPCLICK) return CREATE_ABORT;
        return TRUE;
    }
};

static TSPCreateCallBack g_tspCreateCB;

CreateMouseCallBack* TypeStringProbe::GetCreateMouseCallBack()
{
    return &g_tspCreateCB;
}

//--------------------------------------------------------------------------
// Construction / initial C++ writes
//--------------------------------------------------------------------------
TypeStringProbe::TypeStringProbe()
{
    ivalid.SetInfinite();
    Create();
}

void TypeStringProbe::Create()
{
    if (!pblock) {
        GetTSPDesc()->MakeAutoParamBlocks(this);
        pblock = GetParamBlock(0);
    }
    if (!pblock) return;

    // C++ write path: size the tab and fill four cells.
    const int n = 4;
    pblock->SetCount(p_cells, n);
    for (int i = 0; i < n; ++i) {
        MCHAR buf[32];
        _stprintf_s(buf, _countof(buf), _T("cpp%d"), i);
        pblock->SetValue(p_cells, 0, buf, i);
    }
}

RefTargetHandle TypeStringProbe::Clone(RemapDir& remap)
{
    TypeStringProbe* obj = new TypeStringProbe();
    BaseClone(this, obj, remap);
    if (pblock)
        obj->ReplaceReference(kPBlockRef, remap.CloneRef(pblock));
    obj->ivalid.SetEmpty();
    return obj;
}

//--------------------------------------------------------------------------
// DLL exports
//--------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID /*lpvReserved*/)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        MaxSDK::Util::UseLanguagePackLocale();
        hInstance = hinstDLL;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) const TCHAR* LibDescription()
{
    return _T("TypeStringProbe (M0 probe)");
}

extern "C" __declspec(dllexport) int LibNumberClasses()
{
    return 1;
}

extern "C" __declspec(dllexport) ClassDesc* LibClassDesc(int i)
{
    return i == 0 ? GetTSPDesc() : nullptr;
}

extern "C" __declspec(dllexport) ULONG LibVersion()
{
    return VERSION_3DSMAX;
}

extern "C" __declspec(dllexport) int LibInitialize()
{
    return TRUE;
}

extern "C" __declspec(dllexport) int LibShutdown()
{
    return TRUE;
}
