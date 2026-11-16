# 四插件 Bundle：类声明与 ClassDesc 注册草案

> 本文是 [ANNOTATION_SUITE.md](ANNOTATION_SUITE.md) 的附录，记录
> `CADText` / `CADLeader` / `CADTable` 的完整类声明草案，以及四类
> `ClassDesc2` 的注册方式。
>
> **状态：已实现（M0–M3，2026-11-16）。** 类声明以实际源码为准；
> 本文保留为设计对照。实现差异包括：`CADText` 使用 `SimpleObject2`、
> `CADTable` 单元格使用 `TYPE_STRING_TAB`、`CADDimension` 点位 UI 使用
> `ParamMap2UserDlgProc`。详见 [ARCHITECTURE.md](ARCHITECTURE.md)。

## 1. 公共约定

- 四类共享一个 `.dlo`、一个 `hInstance` 与 `CAD Tools` 类别。
- 每类一个 `IParamBlock2`；`CADText/CADLeader/CADTable` 为 `NumRefs()==1`，`CADDimension` 为 2（槽1 旧版兼容，迁移后清空）；节点引用只放在 `TYPE_INODE` 参数中。
- 每类独立 `Class_ID`；`CADDimension` 的现有 ID 不可更改。
- 所有 `Display()` 使用 `ViewExp::NonScalingObjectSize()` 计算屏幕尺寸。
- 字符串资源使用 `GetString(id)`，资源缺失时回退到英文常量。

建议的 Class_ID：

```cpp
#define CADDIMENSION_CLASS_ID Class_ID(0x5bcf0c90, 0x36fdd779) // 现有，不可变
#define CADTEXT_CLASS_ID      Class_ID(0x5bcf0c91, 0x36fdd779) // 建议值
#define CADLEADER_CLASS_ID    Class_ID(0x5bcf0c92, 0x36fdd779) // 建议值
#define CADTABLE_CLASS_ID     Class_ID(0x5bcf0c93, 0x36fdd779) // 建议值
```

## 2. 附录 A：`CADDimension` 新增部分

现有 `CADDimension.h` 的基础虚函数保持不变，新增参数与辅助函数如下：

```cpp
enum CADDimParamIDs {
    p_target = 0, p_text_size, p_dim_offset,   // 现有
    p_point1, p_point2, p_dimpoint,            // 三点
    p_arrow_size, p_extension_gap, p_show_xyz, // 新增
    p_mode                                     // 可选：0=包围盒 1=三点
};

// 在 CADDimension 类中新增：
bool GetDimensionPoints(TimeValue t, Point3& p1, Point3& p2, Point3& p3) const;

bool ComputeDimensionGeometry(const Point3& p1, const Point3& p2, const Point3& p3,
                              Point3& a, Point3& b, Point3& normal) const;

int BuildDimensionPrimitives(TimeValue t, ViewExp* vpt,
                             const Point3& p1, const Point3& p2,
                             const Point3& a, const Point3& b,
                             ScreenSeg* segs, int maxSegs,
                             ScreenLabel* labels, int maxLabels, int& nLabels) const;
```

三点参数与显示细节见主文档第 5 节。

## 3. 附录 B：`CADText` 类声明

```cpp
#pragma once
#include "3dsmaxsdk_preinclude.h"
#include <max.h>
#include <iparamb2.h>
#include <iparamm2.h>

#define CADTEXT_CLASS_ID Class_ID(0x5bcf0c91, 0x36fdd779) // 建议值

class CADText : public SimpleObject
{
public:
    IParamBlock2* pblock = nullptr;
    Interval      ivalid;

    CADText();
    ~CADText() override;

    void Create();

    Class_ID  ClassID() override { return CADTEXT_CLASS_ID; }
    SClass_ID SuperClassID() override { return GEOMOBJECT_CLASS_ID; }
    void      GetClassName(MSTR& s, bool localized) const override;
    void      DeleteThis() override { delete this; }
    void      BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void      EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;

    RefTargetHandle Clone(RemapDir& remap) override;
    RefResult NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget,
                               PartID& partID, RefMessage message, BOOL propagate) override;
    int             NumRefs() override { return 1; }
    RefTargetHandle GetReference(int i) override;
    void            SetReference(int i, RefTargetHandle rtarg) override;
    int             NumSubs() override { return 1; }
    Animatable*     SubAnim(int i) override;
    MSTR            SubAnimName(int i, bool localized) override;
    int             NumParamBlocks() override { return 1; }
    IParamBlock2*   GetParamBlock(int i) override;
    IParamBlock2*   GetParamBlockByID(BlockID id) override;

    CreateMouseCallBack* GetCreateMouseCallBack() override;
    int      IsRenderable() override { return 0; }
    void     InitNodeName(MSTR& s) override;
    Interval ObjectValidity(TimeValue t) override;
    BOOL     HasViewDependentBoundingBox() override { return TRUE; }

    void GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    int  Display(TimeValue t, INode* inode, ViewExp* vpt, int flags) override;
    int  HitTest(TimeValue t, INode* inode, int type, int crossing, int flags,
                 IPoint2* p, ViewExp* vpt) override;

    // SimpleObject 的网格入口；显示型文字返回空网格。
    void BuildMesh(TimeValue t, Mesh& mesh) override;
};

ClassDesc2* GetCADTextDesc();
```

## 4. 附录 C：`CADLeader` 类声明

```cpp
#pragma once
#include "3dsmaxsdk_preinclude.h"
#include <max.h>
#include <iparamb2.h>
#include <iparamm2.h>

#define CADLEADER_CLASS_ID Class_ID(0x5bcf0c92, 0x36fdd779) // 建议值

class CADLeader : public HelperObject
{
public:
    IParamBlock2* pblock = nullptr;
    Interval      ivalid;

    CADLeader();
    ~CADLeader() override;

    void Create();

    Class_ID  ClassID() override { return CADLEADER_CLASS_ID; }
    SClass_ID SuperClassID() override { return HELPER_CLASS_ID; }
    void      GetClassName(MSTR& s, bool localized) const override;
    void      DeleteThis() override { delete this; }
    void      BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void      EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;

    RefTargetHandle Clone(RemapDir& remap) override;
    RefResult NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget,
                               PartID& partID, RefMessage message, BOOL propagate) override;
    int             NumRefs() override { return 1; }
    RefTargetHandle GetReference(int i) override;
    void            SetReference(int i, RefTargetHandle rtarg) override;
    int             NumSubs() override { return 1; }
    Animatable*     SubAnim(int i) override;
    MSTR            SubAnimName(int i, bool localized) override;
    int             NumParamBlocks() override { return 1; }
    IParamBlock2*   GetParamBlock(int i) override;
    IParamBlock2*   GetParamBlockByID(BlockID id) override;

    CreateMouseCallBack* GetCreateMouseCallBack() override;
    void     InitNodeName(MSTR& s) override;
    Interval ObjectValidity(TimeValue t) override;
    BOOL     HasViewDependentBoundingBox() override { return TRUE; }

    void GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    int  Display(TimeValue t, INode* inode, ViewExp* vpt, int flags) override;
    int  HitTest(TimeValue t, INode* inode, int type, int crossing, int flags,
                 IPoint2* p, ViewExp* vpt) override;
};

ClassDesc2* GetCADLeaderDesc();
```

## 5. 附录 D：`CADTable` 类声明

```cpp
#pragma once
#include "3dsmaxsdk_preinclude.h"
#include <max.h>
#include <iparamb2.h>
#include <iparamm2.h>

#define CADTABLE_CLASS_ID Class_ID(0x5bcf0c93, 0x36fdd779) // 建议值

class CADTable : public HelperObject
{
public:
    IParamBlock2* pblock = nullptr;
    Interval      ivalid;

    CADTable();
    ~CADTable() override;

    void Create();

    Class_ID  ClassID() override { return CADTABLE_CLASS_ID; }
    SClass_ID SuperClassID() override { return HELPER_CLASS_ID; }
    void      GetClassName(MSTR& s, bool localized) const override;
    void      DeleteThis() override { delete this; }
    void      BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void      EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;

    RefTargetHandle Clone(RemapDir& remap) override;
    RefResult NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget,
                               PartID& partID, RefMessage message, BOOL propagate) override;
    int             NumRefs() override { return 1; }
    RefTargetHandle GetReference(int i) override;
    void            SetReference(int i, RefTargetHandle rtarg) override;
    int             NumSubs() override { return 1; }
    Animatable*     SubAnim(int i) override;
    MSTR            SubAnimName(int i, bool localized) override;
    int             NumParamBlocks() override { return 1; }
    IParamBlock2*   GetParamBlock(int i) override;
    IParamBlock2*   GetParamBlockByID(BlockID id) override;

    CreateMouseCallBack* GetCreateMouseCallBack() override;
    void     InitNodeName(MSTR& s) override;
    Interval ObjectValidity(TimeValue t) override;
    BOOL     HasViewDependentBoundingBox() override { return TRUE; }

    void GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    int  Display(TimeValue t, INode* inode, ViewExp* vpt, int flags) override;
    int  HitTest(TimeValue t, INode* inode, int type, int crossing, int flags,
                 IPoint2* p, ViewExp* vpt) override;
};

ClassDesc2* GetCADTableDesc();
```

## 6. 附录 E：ClassDesc 注册

### 6.1 目标注册代码

```cpp
__declspec(dllexport) int LibNumberClasses()
{
    return 4;
}

__declspec(dllexport) ClassDesc* LibClassDesc(int i)
{
    switch (i) {
    case 0: return GetCADDimensionDesc();
    case 1: return GetCADTextDesc();
    case 2: return GetCADLeaderDesc();
    case 3: return GetCADTableDesc();
    default: return nullptr;
    }
}
```

### 6.2 `ClassDesc2` 模式

```cpp
class CADTextClassDesc : public ClassDesc2
{
public:
    int   IsPublic() override               { return TRUE; }
    void* Create(BOOL /*loading*/) override { return new CADText(); }

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
```

`CADLeader` / `CADTable` 复制同一模式并替换：

| 项目 | `CADLeader` | `CADTable` |
|------|-------------|------------|
| 创建类型 | `new CADLeader()` | `new CADTable()` |
| 类名 | `CADLeader` | `CADTable` |
| SuperClassID | `HELPER_CLASS_ID` | `HELPER_CLASS_ID` |
| Class_ID | `CADLEADER_CLASS_ID` | `CADTABLE_CLASS_ID` |
| 字符串 ID | `IDS_CADLEADER_CLASS_NAME` | `IDS_CADTABLE_CLASS_NAME` |

`CADDimension` 继续使用现有 `GetCADDimensionDesc()`，`Class_ID` 不变。

## 7. 资源与本地化清单

新增字符串与对话框（建议命名）：

| 类别 | 资源 |
|------|------|
| `CADText` | `IDS_CADTEXT_CLASS_NAME`、`IDS_CADTEXT_PARAMS`、`IDD_CADTEXT_PANEL` |
| `CADLeader` | `IDS_CADLEADER_CLASS_NAME`、`IDS_CADLEADER_PARAMS`、`IDD_CADLEADER_PANEL` |
| `CADTable` | `IDS_CADTABLE_CLASS_NAME`、`IDS_CADTABLE_PARAMS`、`IDD_CADTABLE_PANEL` |

控件 ID 从当前 `_APS_NEXT_CONTROL_VALUE` 之后分配，避免与现有
`IDC_PICK_TARGET`（1001）等冲突。中文标签写入 `.rc`，保持 GBK / code page 936，
构建后确认 `en-US\CADDimension.dlo.mui` 已生成并随 `.dlo` 部署。

## 8. 实现前检查清单

- [ ] 用 2027 SDK 头文件核对 `SimpleObject::BuildMesh` 等纯虚函数签名。
- [ ] 核实 `ITextObject` 是否适合作为 `CADText` 的基类/接口。
- [ ] 核实 `GraphicsWindow::DrawLine` / `DrawRectangle` 的实际方法名与签名。
- [ ] 核实 `ParamBlock2` 的字符串表参数类型（如 `TYPE_STRING_TAB`）。
- [ ] 确认四个 `Class_ID` 全局唯一并在首次发布前锁定。
- [ ] 确认三点交互代码来源（移植或重写）。
- [ ] 确认旧包围盒场景的兼容策略（`p_mode` 双模式或直接替换）。
- [ ] 每个 `Display()` 都使用 `NonScalingObjectSize()` 并完整恢复 GW 状态。
- [ ] 每个类都有批处理探针与手工回归项。
