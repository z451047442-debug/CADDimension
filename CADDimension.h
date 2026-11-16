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
#include "Common/CADDraw.h"
#include "Common/CADHit.h"
#include <max.h>
#include <istdplug.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <type_traits>

extern TCHAR* GetString(int id);
extern HINSTANCE hInstance;

// 保持 AppWizard 生成的 Class_ID 不变，避免已注册类失效。
#define CADDIMENSION_CLASS_ID Class_ID(0x5bcf0c90, 0x36fdd779)

// 参数块 ID / 参数 ID / 引用槽
enum CADDimBlockIDs { caddim_params = 0 };
enum CADDimParamIDs {
    p_target = 0, p_text_size, p_dim_offset,
    p_point1, p_point2, p_dimpoint,
    p_arrow_size, p_extension_gap, p_show_xyz, p_mode
};
enum CADDimRefSlots { kPBlockRef = 0, kLegacyTargetRef = 1 };

class CADDimensionPBAccessor;
class CADDimTargetRestore;
class CADDimCloneTargetPostPatch;
class CADDimPostLoadMigration;

class CADDimension : public GeomObject
{
public:
    IParamBlock2* pblock = nullptr;     // 引用槽 0：参数块（唯一引用源）
    Interval      ivalid;

    explicit CADDimension(bool loading = false);
    ~CADDimension() override;

    // 普通初始化函数（不是 Object 的虚函数）
    void Create(bool loading);

    // From Animatable
    Class_ID    ClassID() override { return CADDIMENSION_CLASS_ID; }
    SClass_ID   SuperClassID() override { return GEOMOBJECT_CLASS_ID; }
    void        GetClassName(MSTR& s, bool localized) const override;
    void        DeleteThis() override { delete this; }
    void        BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void        EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;

    // From ReferenceTarget / ReferenceMaker
    RefTargetHandle Clone(RemapDir& remap) override;
    RefResult NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget,
                               PartID& partID, RefMessage message, BOOL propagate) override;
    IOResult Load(ILoad* iload) override;
    int NumRefs() override { return 2; }
    RefTargetHandle GetReference(int i) override;
    void SetReference(int i, RefTargetHandle rtarg) override;
    int NumSubs() override { return 2; }
    Animatable* SubAnim(int i) override
    {
        if (i == kPBlockRef) return pblock;
        if (i == kLegacyTargetRef) return m_legacyTarget;
        return nullptr;
    }
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
    friend class CADDimPostLoadMigration;
    friend class CADDimensionCreateCallBack;

    using ScreenSeg   = cad::ScreenSeg;
    using ScreenLabel = cad::ScreenLabel;

    struct DimGeom
    {
        Point3 p1, p2, p3;       // world points
        Point3 u, n, w;          // measurement direction / plane normal / text offset
        Point3 a, b;             // projected points on the dimension line
        float  dist = 0.0f;
        bool   valid = false;
    };

    // 旧版 B1 目标节点兼容槽（引用槽 1）。迁移后清空。
    INode* m_legacyTarget = nullptr;
    bool   m_legacyMigrated = false;
    bool   m_migrating = false;

    // 创建期预览状态（不写 pblock，不产生撤销项）。
    bool   m_creating = false;
    bool   m_createHasP2 = false;
    Point3 m_createOrigin = Point3(0, 0, 0);
    Point3 m_createP2 = Point3(0, 0, 0);
    Point3 m_createP3 = Point3(0, 0, 0);

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
    bool m_pendingOverflow = false;
    unsigned int m_pendingSeq = 0;
    void PushPendingTarget(INode* oldTarget, TimeValue t);
    bool PopPendingTarget(INode*& oldTarget, TimeValue& t);

    INode* GetTargetNode(TimeValue t) const;
    void   ApplyTargetNode(INode* node, TimeValue t);
    bool   IsInvalidTarget(INode* node, TimeValue t) const;
    bool   GetTargetBoxes(TimeValue t, Box3& localBox, Box3& worldBox, Matrix3& otm) const;
    bool   GetTargetDrawData(TimeValue t, Point3 worldCorners[8], Point3& localSize) const;

    // 旧场景兼容：post-load 回调与 BeginEditParams 调用；禁止在 Display/Eval 内迁移。
    void MigrateLegacyTarget(TimeValue t);
    bool IsGeometryParam(ParamID id) const;

    // 三点标注几何
    bool GetDimensionPoints(TimeValue t, Point3& p1, Point3& p2, Point3& p3) const;
    bool ComputeDimGeom(const Point3& p1, const Point3& p2, const Point3& p3, DimGeom& g) const;
    bool GetDimWorldPoints(TimeValue t, INode* inode, DimGeom& g) const;
    bool GetDimBounds(TimeValue t, INode* inode, Box3& worldBox) const;
    int  BuildDimPrimitives(TimeValue t, ViewExp* vpt, const DimGeom& g,
                            ScreenSeg* segs, int maxSegs,
                            ScreenLabel* labels, int maxLabels, int& nLabels) const;

    // 创建期状态（由 CADDimensionCreateCallBack 调用）
    void SetCreateOrigin(const Point3& world);
    void SetCreateSecond(const Point3& localP2);
    void SetCreatePreview(const Point3& localP3);
    void CommitCreate(const Point3& worldOrigin, const Point3& localP2, const Point3& localP3);
    void CancelCreate();

    // 生成屏幕空间的标注线段/文字（包围盒 12 条边 + 3 条尺寸线 + 辅助线/箭头）
    int BuildScreenPrimitives(TimeValue t, ViewExp* vpt,
                              const Point3 worldCorners[8], const Point3& localSize,
                              ScreenSeg* segs, int maxSegs,
                              ScreenLabel* labels, int maxLabels, int& nLabels) const;

    int DrawNoTargetMarker(GraphicsWindow* gw, ViewExp* vpt, INode* inode, TimeValue t,
                           const MCHAR* text) const;
};

static_assert(std::is_base_of<GeomObject, CADDimension>::value,
              "CADDimension must derive from GeomObject to match GEOMOBJECT_CLASS_ID");

extern ClassDesc2* GetCADDimensionDesc();
