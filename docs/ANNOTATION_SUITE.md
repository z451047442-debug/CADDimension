# CAD 注释工具集（四插件 Bundle）规划

> **状态：已实现（M0–M3，2026-11-16）。**
> 分支 `feature/annotation-suite` 已注册四个类：
> `CADDimension`（三点 + `showXYZ` + `p_mode` 旧场景兼容）、`CADText`、
> `CADLeader`、`CADTable`。实现与文件布局见 [ARCHITECTURE.md](ARCHITECTURE.md)，
> 验证命令见 [TESTING.md](TESTING.md)。交互/视口/面板的人工验收已完成（2026-11-16）。
> 类声明与 ClassDesc 注册代码见
> [ANNOTATION_SUITE_CLASSES.md](ANNOTATION_SUITE_CLASSES.md)。

## 1. 背景与目标

构建类似 AutoCAD"注释"面板的一组对象，四个独立类注册在同一个 Plugin Bundle
（一个 `.dlo`）中：

| 类 | 定位 | 基类（建议） |
|----|------|--------------|
| `CADDimension` | 线性尺寸标注 | `GeomObject`（现有） |
| `CADText` | 独立文字对象 | `SimpleObject`（待验证 `ITextObject`） |
| `CADLeader` | 带箭头引线与附着文字 | `HelperObject` |
| `CADTable` | 参数化网格表格 | `HelperObject`（可选 `ShapeObject`） |

共同要求：

- 每个类拥有独立的 `ClassDesc2`、`Class_ID`、`ParamBlock2` 与修改面板；
- 所有 `Display()` 使用 `ViewExp::NonScalingObjectSize()` 作为尺寸系数，保证
  标注元素的屏幕大小不随视口缩放变化；
- 保持现有 `CADDimension` 的 `Class_ID` 不变，避免已保存场景失效；
- 新类通过同一 DLL 的 `LibNumberClasses()` / `LibClassDesc()` 注册。

## 2. 需求摘要

| 类 | 交互 / UI | 显示内容 | 关键参数 |
|----|-----------|----------|----------|
| `CADDimension` | 三点：起点 → 终点 → 尺寸线位置；`showXYZ` 复选框 | 尺寸线、延伸线、箭头、距离文本；可选 X/Y/Z 分量 | `point1`、`point2`、`dimPoint`、`textSize`、`arrowSize`、`showXYZ` |
| `CADText` | UI 输入文字、字号；点击放置 | 屏幕空间文字 | `text`、`textSize` |
| `CADLeader` | 三段：起点 → 折点 → 文字位置 | 箭头折线 + 附着文字 | `start`、`elbow`、`textPos`、`text`、`textSize`、`arrowSize` |
| `CADTable` | 参数控制行数、列数、单元格文本 | 网格线 + 单元格文字 | `rows`、`cols`、`cellWidth`、`cellHeight`、`textSize`、单元格文本 |

## 3. 实现状态（M0–M3）

| 项目 | 状态（2026-11-16） |
|------|---------------------|
| Bundle 类数 | 4（`LibNumberClasses()==4`） |
| `CADDimension` | 三点标注 + `showXYZ` + `p_mode` 双模式；旧 `NumRefs==2` 场景 post-load 迁移 |
| `CADText` | `SimpleObject2`，单击放置，屏幕文字 |
| `CADLeader` | `HelperObject`，三段交互，箭头折线 + 文字 |
| `CADTable` | `HelperObject`，`TYPE_STRING_TAB` 单元格，默认 20×20 |
| 共享绘制/命中 | `Common/CADDraw`、`Common/CADHit` |
| 自动化探针 | `scripts/probe_dim_three_point.ms`、`probe_cadtext.ms`、`probe_cadleader.ms`、`probe_cadtable.ms`、`scripts/probe_b1.ms` |
| 人工验收 | 已完成（2026-11-16）：三击/单击交互、视口视觉、中文面板/MUI、显示性能 |

> 下文的“目标设计”仍作为实现对照保留；实际签名/参数以源码为准。

## 4. 总体架构

### 4.1 单 DLL、多 ClassDesc

```text
CADDimension.dlo（Plugin Bundle）
├─ LibNumberClasses() = 4
├─ LibClassDesc(0) → GetCADDimensionDesc()  → CADDimension
├─ LibClassDesc(1) → GetCADTextDesc()       → CADText
├─ LibClassDesc(2) → GetCADLeaderDesc()     → CADLeader
└─ LibClassDesc(3) → GetCADTableDesc()      → CADTable
```

- 四个类共享同一个 `.dlo` 与资源/MUI；`ClassDesc2::HInstance()` 均返回同一
  `hInstance`。
- 每类的 `SuperClassID` 决定它在 3ds Max 中的归属：
  `GEOMOBJECT_CLASS_ID`（Dimension/Text）或 `HELPER_CLASS_ID`（Leader/Table）。
- 所有类归属同一创建面板类别 `CAD Tools`。

### 4.2 建议的模块划分

现有仓库是扁平结构，扩展时建议按共享层拆分（保持每个文件职责单一）：

```text
Common/
  CADDraw.h/.cpp        # 世界→屏幕、裁剪、文字、非缩放系数、颜色
  CADUndo.h/.cpp        # RestoreObj 基类、访问器辅助
  CADBundle.h           # 四个 Class_ID、公共宏
CADDimension/
  CADDimension.h/.cpp
CADText/
  CADText.h/.cpp
CADLeader/
  CADLeader.h/.cpp
CADTable/
  CADTable.h/.cpp
```

首期可以先保留扁平文件（`CADText.cpp` 等），但**绘制与投影助手必须共享**，
避免四份 `wTransPoint`/裁剪/文字夹取代码分叉。

### 4.3 注册代码（目标形态）

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

对应 `CADDimension.def` 无需增加导出函数；仍导出
`LibDescription/LibNumberClasses/LibClassDesc/LibVersion` 等标准入口。

### 4.4 Class_ID 分配

| 类 | 建议 Class_ID | 说明 |
|----|----------------|------|
| `CADDimension` | `Class_ID(0x5bcf0c90, 0x36fdd779)` | 现有，**不可更改** |
| `CADText` | `Class_ID(0x5bcf0c91, 0x36fdd779)` | 建议值，发布前确认全局唯一 |
| `CADLeader` | `Class_ID(0x5bcf0c92, 0x36fdd779)` | 建议值 |
| `CADTable` | `Class_ID(0x5bcf0c93, 0x36fdd779)` | 建议值 |

`Class_ID` 一旦发布就不能复用或修改；建议在首个正式版本前固定。

### 4.5 通用显示契约

所有类的 `Display()` 必须遵守：

1. 保存并恢复 `GraphicsWindow` 的变换、渲染限制、文字点大小；
2. `setTransform(Matrix3())` 后使用 `wTransPoint()` 做世界→屏幕投影
   （2027 SDK 没有 `ViewExp::MapWorldToScreen()`）；
3. 通过 `ViewExp::NonScalingObjectSize()` 取得用户系数，钳制到合理范围
   （现有实现为 0.1–10）；
4. 所有线宽、箭头、文字、间隙都以**屏幕像素**为单位乘以该系数；
5. 线段先做视口裁剪再绘制，文字先夹到视口内；
6. 颜色沿用选中色 / 冻结色 / 线框色，并为线条与文字设置同一颜色。

现有实现可直接抽为共享助手：
`MapWorldToScreen()`、`ClipSegmentToRect()`、`DrawScreenSeg()`、
`DrawScreenLabel()`、`NonScalingFactor()`（`CADDimension.cpp:44-50`、`67-97`、
`734-745`、`958-994`）。

### 4.6 通用数据 / 撤销 / 克隆契约

- 每类一个 `IParamBlock2`；`CADText/CADLeader/CADTable` 为 `NumRefs()==1`；`CADDimension` 为 2（槽1 仅旧版兼容，迁移后清空）。
- 节点引用只放在 `TYPE_INODE` 参数中；不要新增引用槽。
- 用户可撤销的变更通过 `PBAccessor::PreSet/PostSet` + `RestoreObj` 捕获；
  创建期、克隆期、撤销回放期必须抑制记录。
- 克隆时显式克隆参数块；若参数块中含节点引用，必须在 `RemapDir` 回填后
  通过 `PostPatchProc` 修复目标。
- `REFMSG_TARGET_DELETED` 中只做失效与通知，不操作引用。
- 持久化：`Class_ID` 固定；新增参数使用新的 Param ID，保持旧场景可加载。

### 4.7 绘制 API 选择

需求提到 `GraphicsWindow::DrawLine` / `DrawRectangle`。现有代码使用
`wPolyline()` / `wText()`，并已处理视口裁剪与文字夹取。建议：

- 统一封装为共享助手，内部优先使用 `wPolyline()`（2 点即线段）；
- 若 2027 SDK 的 `DrawLine`/`DrawRectangle` 在裁剪、状态管理上更合适，再在
  助手内部替换，不改变各 `Display()` 的调用方式；
- **实现前先以 2027 SDK 头文件核实方法名与签名**，不要直接照搬其他版本文档。

表格网格本质是多条线段；用统一线段助手即可复用裁剪逻辑。

## 5. CADDimension 详细设计（优先实现）

### 5.1 目标行为

1. 第一次点击：确定尺寸测量起点 `p1`；
2. 第二次点击：确定测量终点 `p2`；
3. 移动鼠标时预览尺寸线位置；
4. 第三次点击：确定尺寸线经过的位置 `p3`，完成创建；
5. 视口显示：延伸线、尺寸线、两端箭头、距离文本；
6. `showXYZ` 打开时，在距离文本后附加 X/Y/Z 分量。

### 5.2 数据模型

建议参数（Point3 均存**对象局部坐标**）：

| 参数 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `point1` | `TYPE_POINT3` | `(0,0,0)` | 测量起点 |
| `point2` | `TYPE_POINT3` | `(0,0,0)` | 测量终点 |
| `dimPoint` | `TYPE_POINT3` | `(0,0,0)` | 尺寸线经过点 |
| `textSize` | `TYPE_FLOAT` | 12 | 文字大小，6–48 |
| `arrowSize` | `TYPE_FLOAT` | 7 | 箭头长度，4–20 |
| `extensionGap` | `TYPE_FLOAT` | 2 | 延伸线起点间隙，0–20 |
| `showXYZ` | `TYPE_BOOL` | `false` | 是否显示分量 |

**坐标锚定**：第一次点击时把对象节点平移到 `p1`（`mat.SetTrans(p1)`），并把
`p1` 存为局部原点 `(0,0,0)`；后续点存 `worldPoint - p1`。`Display()` 中
`world = local * inode->GetObjectTM(t)`。这样移动/旋转标注节点会整体移动标注，
与 3ds Max 的对象操作习惯一致。

替代方案：存世界坐标并锁定节点变换为单位矩阵；实现更简单，但节点变换不再
有意义。建议采用局部坐标方案。

### 5.3 几何计算

设局部点 `p1`、`p2`、`p3`：

```text
u = normalize(p2 - p1)                 // 测量方向
if |p2 - p1| < eps  → 无效标注
n = normalize(cross(u, p3 - p1))       // 三点定义平面法线
if |cross| < eps → 退化，回退到局部 Z 或创建时视向
w = normalize(cross(n, u))
a = p3 + u * dot(p1 - p3, u)           // p1 在尺寸线上的投影
b = p3 + u * dot(p2 - p3, u)           // p2 在尺寸线上的投影
distance = Length(p2 - p1)
```

- 尺寸线：`a → b`，方向为 `u`；
- 延伸线：`p1 → a`、`p2 → b`，两端留 `extensionGap` 并在尺寸线一侧略微超出；
- 箭头：在 `a`、`b` 处各画两条 V 形短线，指向尺寸线内侧；
- 文字：`(a + b) * 0.5 + w * textOffset`，显示 `distance`。

`showXYZ` 的文本建议为：

```text
距离 123.456
ΔX 100.000  ΔY 20.000  ΔZ 0.000
```

分量取**世界坐标**差值（`p2World - p1World`），因为用户通常关心全局 X/Y/Z；
若后续需要局部轴分量，可增加参数切换。该语义需在实现前确认（见第 8 节）。

### 5.4 三点交互状态机

在 `CreateMouseCallBack::proc()` 中：

| 阶段 | 事件 | 行为 |
|------|------|------|
| 0 | `MOUSE_POINT` | `p1 = SnapPoint(...)`；`mat.SetTrans(p1)`；存局部 `(0,0,0)`；进入阶段 1 |
| 1 | `MOUSE_POINT` | `p2 = SnapPoint(...) - p1`；零长度则拒绝；进入阶段 2 |
| 2 | `MOUSE_MOVE` | 更新 `dimPoint` 预览并重绘 |
| 2 | `MOUSE_POINT` | 写入三个点，返回 `0` 完成创建 |
| 任意 | `MOUSE_ABORT` / `MOUSE_PROPCLICK` | 重置并 `CREATE_ABORT` |
| 任意 | `MOUSE_FREEMOVE` | `SnapPreview` + `SNAP_IN_3D` |

创建期写入参数块时使用现有的 `m_suppressUndo` 模式，不产生撤销项。

### 5.5 绘制流程

1. 保存/恢复 `GraphicsWindow` 状态；
2. 置单位变换，设置线框限制与颜色；
3. 由参数点计算 `a`、`b`、文字锚点；
4. 投影 `p1`、`p2`、`a`、`b`、文字锚点到屏幕；
5. 绘制两条延伸线、尺寸线、两组箭头；
6. 绘制距离文字；`showXYZ` 时追加第二行；
7. `ns = NonScalingFactor(vpt)`：
   - 文字点大小 = `textSize * ns`（钳制 6–40）；
   - 箭头 = `arrowSize * ns`；
   - 延伸线间隙 = `extensionGap * ns`；
   - 延伸线超出量 = `5 * ns`；
8. 无效点（零长度、点无法投影）时画"No dimension"标记。

### 5.6 修改面板

首期建议控件：

| 控件 | 类型 | 参数 |
|------|------|------|
| 起点 X/Y/Z | 数值旋转器 | `point1` |
| 终点 X/Y/Z | 数值旋转器 | `point2` |
| 尺寸线位置 X/Y/Z | 数值旋转器 | `dimPoint` |
| 文字大小 | 旋转器 | `textSize` |
| 箭头大小 | 旋转器 | `arrowSize` |
| 延伸线间隙 | 旋转器 | `extensionGap` |
| 显示 XYZ | 复选框 | `showXYZ` |

中文标签建议：`起点`、`终点`、`尺寸线位置`、`文字大小`、`箭头大小`、
`延伸线间隙`、`显示 XYZ`。资源文件保持 GBK/936，部署 MUI。

视口内重新拾取点的按钮属于可选增强：需要自定义鼠标过程，建议放到第二期。

### 5.7 撤销 / 克隆 / 持久化

- 点的修改通过参数块完成；为支持 MAXScript 赋值撤销，需为点参数增加
  `PBAccessor` + `RestoreObj`（参考现有 `CADDimTargetRestore` 模式）。
- 克隆：无节点引用时只需 `BaseClone` + `CloneRef(pblock)`；若保留目标节点
  关联，沿用现有 `PostPatchProc` 修复逻辑。
- 持久化：`CADDIMENSION_CLASS_ID` 不变；新增参数使用新 Param ID。
- **旧场景兼容**：现有场景保存的是包围盒模式数据。若直接替换数据模型，旧场景
  将显示无效。建议增加 `p_mode`（`0 = 包围盒`、`1 = 三点`），旧场景默认 `0`，
  新创建默认 `1`；或明确接受旧场景不再正确显示。该决策见第 8 节。

### 5.8 验收标准

- 三次点击完成创建；第二次点击与第三次点击之间有预览；
- 距离值与两点世界距离一致；
- 延伸线、尺寸线、箭头、文字齐全；
- `showXYZ` 开关即时生效，分量值正确；
- 缩放视口时文字/箭头/间隙的屏幕尺寸保持恒定；
- 移动/旋转标注节点，标注整体跟随；
- 撤销/重做、保存/加载、克隆均不丢失点数据；
- 零长度或退化平面不崩溃，显示无效标记；
- 修改面板中文标签与 MUI 正确。

### 5.9 实施顺序

1. 参数块数据模型 + 三点 `CreateMouseCallBack`；
2. 几何计算与屏幕投影；
3. 尺寸线/延伸线/箭头/文字绘制；
4. `showXYZ` 与修改面板；
5. 撤销/克隆/持久化与旧场景策略；
6. 批处理探针 + 交互回归。

## 6. CADText / CADLeader / CADTable 设计

### 6.1 CADText

**基类选择**：

| 方案 | 优点 | 代价 |
|------|------|------|
| `SimpleObject` + 显示型文字 | 与现有代码一致；只画屏幕文字，不生成网格 | 不可渲染/导出 |
| `ITextObject` / 文本形状管线 | 可渲染、可挤出 | 依赖 SDK 文本接口，复杂度高 |

建议首期采用 `SimpleObject` + `Display()` 屏幕文字；实现前用 2027 SDK 头文件
核实 `ITextObject` 是否为可用基类/接口。

参数：`text`（字符串）、`textSize`（6–72）。位置由对象节点变换决定；创建时
点击放置。修改面板提供文本框与字号旋转器。

显示：投影节点位置，按 `textSize * NonScalingObjectSize()` 绘制文字；颜色使用
线框色/选中色/冻结色。

### 6.2 CADLeader

基类：`HelperObject`。

参数：`start`、`elbow`、`textPos`（`TYPE_POINT3`，对象局部）、`text`
（字符串）、`textSize`、`arrowSize`。

三段式交互：

| 阶段 | 事件 | 行为 |
|------|------|------|
| 0 | `MOUSE_POINT` | 起点；`mat.SetTrans(start)` |
| 1 | `MOUSE_POINT` | 折点 |
| 2 | `MOUSE_MOVE` | 预览文字位置 |
| 2 | `MOUSE_POINT` | 文字位置，完成 |
| 任意 | 中止 | 重置 |

显示：`start → elbow → textAnchor` 折线；`start` 端画箭头；文字放在 `textPos`；
所有尺寸乘 `NonScalingObjectSize()`。命中测试对折线线段做屏幕空间距离测试。

### 6.3 CADTable

基类建议 `HelperObject`：

- 只做视口显示，不参与渲染/导出 → `HelperObject` 足够；
- 若未来需要渲染表格或导出几何，再迁移到 `ShapeObject`/网格对象。

参数：`rows`、`cols`（1–50）、`cellWidth`、`cellHeight`（像素）、`textSize`；
单元格文本需要一个字符串容器。优先核实 2027 `ParamBlock2` 是否支持字符串表
参数（如 `TYPE_STRING_TAB`）；若不支持，则使用自定义 `Tab<MSTR>`，并在
`Clone` 与场景持久化中自行序列化。

显示：以对象节点位置为左上角，按 `NonScalingObjectSize()` 计算像素尺寸，
用线段助手绘制外框与行列网格；每个单元格内绘制文字（超宽时裁剪/省略）。
命中测试对网格线做屏幕空间线段测试。

## 7. 类声明与 ClassDesc 注册

完整类声明草案与四类 ClassDesc 注册代码见
[ANNOTATION_SUITE_CLASSES.md](ANNOTATION_SUITE_CLASSES.md)，其中包含：

- `CADDimension` 三点标注新增的 Param ID 与辅助函数；
- `CADText` / `CADLeader` / `CADTable` 的完整类声明草案；
- `LibNumberClasses()` / `LibClassDesc()` 与各 `ClassDesc2` 注册草案；
- 新增资源 ID、中文标签与编译前检查清单。

> 已实现并通过 1.0.0 自动化/人工验收；实际签名以源码为准。

## 8. 决策记录（已在 1.0.0 确定，保留为历史）

| # | 决策 | 选项 | 建议 |
|---|------|------|------|
| 1 | 三点交互代码来源 | 从外部旧版移植 / 重新实现 | 先确认旧版位置；无来源则按第 5 节重建 |
| 2 | 是否保留包围盒模式 | 新增 `p_mode` 双模式 / 直接替换 | 建议双模式，旧场景默认 `0` |
| 3 | `showXYZ` 语义 | 距离 + 世界 ΔX/ΔY/ΔZ / 局部轴分量 / 仅显示三轴线 | 建议世界分量，必要时加切换 |
| 4 | `CADText` 基类 | `SimpleObject` / `ITextObject` | 先 `SimpleObject` 显示型；核实 SDK 后再考虑文本形状 |
| 5 | `CADTable` 基类 | `HelperObject` / `ShapeObject` | 显示用途选 `HelperObject` |
| 6 | 表格文本存储 | SDK 字符串表参数 / 自定义 `Tab<MSTR>` | 先核实 `TYPE_STRING_TAB`；不可用则自定义持久化 |
| 7 | 网格绘制 API | `DrawLine/DrawRectangle` / `wPolyline` | 统一封装；优先沿用已裁剪的 `wPolyline` |
| 8 | Class_ID | 4.4 节建议值 | 首个正式版前锁定并确认全局唯一 |
| 9 | 点重拾 UI | 数值旋转器 / 视口重拾按钮 | 首期数值旋转器；视口重拾二期 |
| 10 | 中文资源 | 每类独立对话框 / 共用对话框 | 独立对话框，便于参数扩展 |

## 9. 测试与验收

- **批处理**：每类至少一个探针，验证类解析、创建、参数、克隆、保存/加载、
  撤销/重做；`LibNumberClasses() == 4` 且 `LibClassDesc(i)` 返回非空。
- **CADDimension 专项**：三次点击、预览、距离值、showXYZ、节点变换、
  零长度保护、屏幕尺寸恒定。
- **手工回归**：四类在创建面板中可见；修改面板标签正确；MUI 部署后中文正常；
  视口缩放时所有标注元素尺寸恒定。
- **兼容性**：旧 B1/B4d 场景加载不崩溃；若采用双模式，旧场景保持包围盒显示。

---

参考：现有实现的绘制、投影、撤销与克隆细节见
[ARCHITECTURE.md](ARCHITECTURE.md)；构建与部署见 [BUILD.md](BUILD.md)；
测试方法见 [TESTING.md](TESTING.md)。
