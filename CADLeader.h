#pragma once

//**************************************************************************/
// CADLeader - 带箭头引线与附着文字的 HelperObject
//**************************************************************************/

#include "3dsmaxsdk_preinclude.h"
#include "resource.h"
#include "Common/CADDraw.h"
#include "Common/CADHit.h"
#include <max.h>
#include <iparamb2.h>
#include <iparamm2.h>

#define CADLEADER_CLASS_ID Class_ID(0x5bcf0c92, 0x36fdd779)

extern TCHAR* GetString(int id);
extern HINSTANCE hInstance;

enum CADLeaderBlockIDs { cadleader_params = 0 };
enum CADLeaderParamIDs {
    p_start = 0, p_elbow, p_textpos,
    p_leader_text, p_leader_size, p_leader_arrow
};

class CADLeader : public HelperObject
{
public:
    IParamBlock2* pblock = nullptr;
    Interval      ivalid;

    // 创建期预览（不写 pblock）。
    bool   m_leaderCreating = false;
    bool   m_hasElbow = false;
    Point3 m_start = Point3(0, 0, 0);
    Point3 m_elbow = Point3(0, 0, 0);
    Point3 m_text = Point3(0, 0, 0);

    CADLeader();
    ~CADLeader() override { DeleteAllRefsFromMe(); }

    Class_ID    ClassID() override { return CADLEADER_CLASS_ID; }
    SClass_ID   SuperClassID() override { return HELPER_CLASS_ID; }
    void        GetClassName(MSTR& s, bool localized) const override;
    void        DeleteThis() override { delete this; }
    void        BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void        EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;

    RefTargetHandle Clone(RemapDir& remap) override;
    RefResult NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget,
                               PartID& partID, RefMessage message, BOOL propagate) override;
    int             NumRefs() override { return 1; }
    RefTargetHandle GetReference(int i) override { return i == 0 ? pblock : nullptr; }
    void            SetReference(int i, RefTargetHandle rtarg) override
    {
        if (i == 0) pblock = static_cast<IParamBlock2*>(rtarg);
    }
    int             NumSubs() override { return 1; }
    Animatable*     SubAnim(int i) override { return i == 0 ? pblock : nullptr; }
    MSTR            SubAnimName(int i, bool localized) override;
    int             NumParamBlocks() override { return 1; }
    IParamBlock2*   GetParamBlock(int i) override { return i == 0 ? pblock : nullptr; }
    IParamBlock2*   GetParamBlockByID(BlockID id) override
    {
        return (pblock && pblock->ID() == id) ? pblock : nullptr;
    }

    CreateMouseCallBack* GetCreateMouseCallBack() override;
    ObjectState Eval(TimeValue t) override;
    void        InitNodeName(MSTR& s) override { s = _T("CADLeader"); }
    Interval    ObjectValidity(TimeValue t) override;
    BOOL        HasViewDependentBoundingBox() override { return TRUE; }

    void GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;

    int Display(TimeValue t, INode* inode, ViewExp* vpt, int flags) override;
    int HitTest(TimeValue t, INode* inode, int type, int crossing, int flags,
                IPoint2* p, ViewExp* vpt) override;

    // create state / preview
    void SetCreateStart(const Point3& world);
    void SetCreateElbow(const Point3& localElbow);
    void SetCreateText(const Point3& localText);
    void CommitCreate(const Point3& worldStart, const Point3& localElbow, const Point3& localText);
    void CancelCreate();

private:
    bool GetLeaderPoints(TimeValue t, Point3& start, Point3& elbow, Point3& text) const;
    bool GetLeaderWorldPoints(TimeValue t, INode* inode,
                              Point3& start, Point3& elbow, Point3& text) const;
};

extern ClassDesc2* GetCADLeaderDesc();
