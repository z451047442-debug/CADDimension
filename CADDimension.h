#pragma once

//**************************************************************************/
// CADDimension - AutoCAD 风格的视口尺寸标注对象
//
// 说明（2027 SDK 实际 API）：
//   1. Object 的显示虚函数是 Display(TimeValue, INode*, ViewExp*, int)，
//      不存在 Display(TimeValue, ViewExp*, int) 这个重载。
//   2. ViewExp 没有 MapWorldToScreen()；官方 TapeHelp 使用
//      GraphicsWindow::wTransPoint() 做世界->屏幕转换，并使用
//      wPolyline()/wText() 在屏幕空间绘制。
//   3. Object 没有名为 BoundingBox() 的虚函数；实际需要实现
//      GetWorldBoundBox()/GetLocalBoundBox()。本类额外保留了
//      BoundingBox() 作为内部辅助函数。
//   4. Object 没有 virtual Create()；这里的 Create() 是普通初始化入口，
//      在构造函数中调用，用于创建自动参数块。
//
// MAXScript 绑定示例（参数块 TYPE_INODE 自动暴露 targetNode）：
//   cd = CADDimension()          -- 或在 Create 面板创建后取 $CADDimension001
//   cd.targetNode = $Box001      -- accessor 会同步引用并刷新视口
//   cd.textSize = 14
//   cd.dimensionOffset = 36
//**************************************************************************/

#include "3dsmaxsdk_preinclude.h"
#include "resource.h"
#include <max.h>
#include <istdplug.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <GraphicsWindow.h>
#include <type_traits>

extern TCHAR* GetString(int id);
extern HINSTANCE hInstance;

// 保持 AppWizard 生成的 Class_ID 不变，避免已注册类失效。
#define CADDIMENSION_CLASS_ID Class_ID(0x5bcf0c90, 0x36fdd779)

// 参数块 ID / 参数 ID / 引用槽
enum CADDimBlockIDs { caddim_params = 0 };
enum CADDimParamIDs { p_target = 0, p_text_size, p_dim_offset };
enum CADDimRefSlots { kPBlockRef = 0 };

class CADDimensionPBAccessor;
class CADDimTargetRestore;
class CADDimCloneTargetPostPatch;

class CADDimension : public GeomObject
{
public:
    IParamBlock2* pblock = nullptr;     // 引用槽 0：参数块（唯一引用源）
    Interval      ivalid;

    CADDimension();
    ~CADDimension() override;

    // 普通初始化函数（不是 Object 的虚函数）
    void Create();

    // From Animatable
    Class_ID    ClassID() override { return CADDIMENSION_CLASS_ID; }
    SClass_ID   SuperClassID() override { return GEOMOBJECT_CLASS_ID; }
    void        GetClassName(MSTR& s, bool localized) const override;
    void        DeleteThis() override { delete this; }

    // From ReferenceTarget / ReferenceMaker
    RefTargetHandle Clone(RemapDir& remap) override;
    RefResult NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget,
                               PartID& partID, RefMessage message, BOOL propagate) override;
    int NumRefs() override { return 1; }
    RefTargetHandle GetReference(int i) override;
    void SetReference(int i, RefTargetHandle rtarg) override;
    int NumSubs() override { return 1; }
    Animatable* SubAnim(int i) override { return i == 0 ? pblock : nullptr; }
    MSTR SubAnimName(int i, bool localized) override;
    int NumParamBlocks() override { return 1; }
    IParamBlock2* GetParamBlock(int i) override { return i == 0 ? pblock : nullptr; }
    IParamBlock2* GetParamBlockByID(BlockID id) override;

    // From BaseObject / Object
    CreateMouseCallBack* GetCreateMouseCallBack() override;
    int         IsRenderable() override { return 0; }
    void        InitNodeName(MSTR& s) override { s = _T("CADDimension"); }
    ObjectState Eval(TimeValue t) override;
    Interval    ObjectValidity(TimeValue t) override;
    BOOL        HasViewDependentBoundingBox() override { return TRUE; }

    // From BaseObject：真实的包围盒虚函数
    void GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetDeformBBox(TimeValue t, Box3& box, Matrix3* tm, BOOL useSel) override;

    int Display(TimeValue t, INode* inode, ViewExp* vpt, int flags) override;
    int HitTest(TimeValue t, INode* inode, int type, int crossing, int flags,
                IPoint2* p, ViewExp* vpt) override;

    // 显式包围盒辅助（对应需求中的 BoundingBox()）
    bool BoundingBox(TimeValue t, Box3& box) const;

    // 供 UI / 其他代码调用；pblock 的 TYPE_INODE 参数同时暴露给 MAXScript
    void SetTargetNode(INode* node, TimeValue t, bool allowUndo = true);

private:
    friend class CADDimensionPBAccessor;
    friend class CADDimTargetRestore;
    friend class CADDimCloneTargetPostPatch;

    // Set while Clone's PostPatchProc rewrites the cloned pblock target.
    bool m_inClone = false;
    // Set while RestoreObj applies an undo/redo value.
    bool m_inUndoRestore = false;
    // Set while SetTargetNode(..., allowUndo=false) applies a creation bind.
    bool m_suppressUndo = false;

    // PreSet stores the old target; PostSet consumes it synchronously.
    struct PendingTarget { INode* oldTarget; TimeValue t; };
    enum { kMaxPendingTargets = 4 };
    PendingTarget m_pendingTargets[kMaxPendingTargets] = {};
    int m_pendingCount = 0;
    unsigned int m_pendingSeq = 0;
    void PushPendingTarget(INode* oldTarget, TimeValue t);
    bool PopPendingTarget(INode*& oldTarget, TimeValue& t);

    struct ScreenSeg   { IPoint2 a, b; };
    struct ScreenLabel { IPoint2 pos; MSTR text; };

    INode* GetTargetNode(TimeValue t) const;
    void   ApplyTargetNode(INode* node, TimeValue t);
    bool   IsInvalidTarget(INode* node, TimeValue t) const;
    bool   GetTargetWorldBox(TimeValue t, Box3& box) const;
    Point2 MapWorldToScreen(GraphicsWindow* gw, const Point3& world, bool& ok) const;

    // 生成屏幕空间的标注线段/文字（包围盒 12 条边 + 3 条尺寸线 + 辅助线/箭头）
    int BuildScreenPrimitives(TimeValue t, ViewExp* vpt, const Box3& box,
                              ScreenSeg* segs, int maxSegs,
                              ScreenLabel* labels, int maxLabels, int& nLabels) const;

    void DrawScreenSeg(GraphicsWindow* gw, const ScreenSeg& seg) const;
    void DrawScreenLabel(GraphicsWindow* gw, const ScreenLabel& label) const;
    void DrawNoTargetMarker(GraphicsWindow* gw, ViewExp* vpt, INode* inode, TimeValue t) const;
    void SetupLineColor(GraphicsWindow* gw, INode* inode) const;

    static bool HitSegments(const HitRegion& hr, const ScreenSeg* segs, int nSegs);
};

static_assert(std::is_base_of<GeomObject, CADDimension>::value,
              "CADDimension must derive from GeomObject to match GEOMOBJECT_CLASS_ID");

extern ClassDesc2* GetCADDimensionDesc();
