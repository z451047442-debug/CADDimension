#pragma once

//**************************************************************************/
// CADTable - 视口网格表格（HelperObject）
// 单元格文本存 TYPE_STRING_TAB，索引 r*cols+c；首期通过 MAXScript 编辑。
//**************************************************************************/

#include "3dsmaxsdk_preinclude.h"
#include "resource.h"
#include "Common/CADDraw.h"
#include "Common/CADHit.h"
#include <max.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <vector>

#define CADTABLE_CLASS_ID Class_ID(0x5bcf0c93, 0x36fdd779)

enum CADTableBlockIDs { cadtable_params = 0 };
enum CADTableParamIDs {
    p_rows = 0, p_cols, p_cell_w, p_cell_h, p_table_text_size, p_cells
};

extern TCHAR* GetString(int id);
extern HINSTANCE hInstance;

class CADTable : public HelperObject
{
public:
    IParamBlock2* pblock = nullptr;
    Interval      ivalid;

    CADTable();
    ~CADTable() override { DeleteAllRefsFromMe(); }

    Class_ID    ClassID() override { return CADTABLE_CLASS_ID; }
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
    void        InitNodeName(MSTR& s) override { s = _T("CADTable"); }
    Interval    ObjectValidity(TimeValue t) override;
    BOOL        HasViewDependentBoundingBox() override { return TRUE; }

    void GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;

    int Display(TimeValue t, INode* inode, ViewExp* vpt, int flags) override;
    int HitTest(TimeValue t, INode* inode, int type, int crossing, int flags,
                IPoint2* p, ViewExp* vpt) override;

    // Resize the string tab to rows*cols, preserving overlapping cells.
    void SyncCells(TimeValue t);
    void ReadTableParams(TimeValue t, int& rows, int& cols,
                         float& cellW, float& cellH, float& textSize) const;

private:
    int BuildTablePrimitives(TimeValue t, ViewExp* vpt, const Point2& origin,
                             cad::ScreenSeg* segs, int maxSegs,
                             cad::ScreenLabel* labels, int maxLabels, int& nLabels) const;
};

extern ClassDesc2* GetCADTableDesc();
