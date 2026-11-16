#pragma once

//**************************************************************************/
// CADText - 视口显示型文字对象（SimpleObject2）
//**************************************************************************/

#include "3dsmaxsdk_preinclude.h"
#include "resource.h"
#include "Common/CADDraw.h"
#include "Common/CADHit.h"
#include <max.h>
#include <simpobj.h>
#include <iparamb2.h>
#include <iparamm2.h>

#define CADTEXT_CLASS_ID Class_ID(0x5bcf0c91, 0x36fdd779)

extern TCHAR* GetString(int id);
extern HINSTANCE hInstance;

enum CADTextBlockIDs { cadtext_params = 0 };
enum CADTextParamIDs { p_text = 0, p_text_size };

class CADText : public SimpleObject2
{
public:
    Interval ivalid;

    CADText();
    ~CADText() override { DeleteAllRefsFromMe(); }

    Class_ID    ClassID() override { return CADTEXT_CLASS_ID; }
    SClass_ID   SuperClassID() override { return GEOMOBJECT_CLASS_ID; }
    void        GetClassName(MSTR& s, bool localized) const override;
    void        DeleteThis() override { delete this; }
    void        BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void        EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;

    RefTargetHandle Clone(RemapDir& remap) override;
    RefResult NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget,
                               PartID& partID, RefMessage message, BOOL propagate) override;

    CreateMouseCallBack* GetCreateMouseCallBack() override;
    int      IsRenderable() override { return 0; }
    void     InitNodeName(MSTR& s) override { s = _T("CADText"); }
    Interval ObjectValidity(TimeValue t) override;
    BOOL     HasViewDependentBoundingBox() override { return TRUE; }

    void GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetDeformBBox(TimeValue t, Box3& box, Matrix3* tm, BOOL useSel) override;

    int Display(TimeValue t, INode* inode, ViewExp* vpt, int flags) override;
    int HitTest(TimeValue t, INode* inode, int type, int crossing, int flags,
                IPoint2* p, ViewExp* vpt) override;

    // SimpleObjectBase pure virtual
    void BuildMesh(TimeValue t) override;

private:
    const MCHAR* GetText(TimeValue t) const;
    float GetTextSize(TimeValue t) const;
};

extern ClassDesc2* GetCADTextDesc();
