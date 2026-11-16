# 架构设计

> 基线：提交 `25884bb`。文中的行号以该提交为准，代码变动后请同步修正。

## 1. 概述

CADDimension 是一个 `GeomObject` 派生插件对象。它不生成网格，也不是可渲染
对象；它的职责是：

1. 通过参数块绑定一个**目标节点**；
2. 取得目标的局部包围盒并变换到世界坐标；
3. 把包围盒 8 个角点投影到屏幕；
4. 在屏幕空间绘制包围盒边线、三条轴向尺寸线、箭头与尺寸文字；
5. 在屏幕空间完成命中测试（点选、窗口、交叉、圆形区域）；
6. 让目标绑定可撤销/重做、可克隆、可保存/加载。

`Eval()` 返回 `ObjectState(this)`（`CADDimension.cpp:478`），对象自身就是求值
结果；`IsRenderable()` 返回 0（`CADDimension.h:85`）。

## 2. 文件与职责

| 文件 | 职责 |
|------|------|
| `CADDimension.h` | 类声明、Class_ID、参数/引用枚举、私有状态与辅助函数 |
| `CADDimension.cpp` | 参数块、访问器与撤销、创建回调、包围盒、绘制、命中测试、克隆 |
| `DllEntry.cpp` | `DllMain`、`LibDescription`/`LibNumberClasses`/`LibClassDesc` 等导出 |
| `CADDimension.rc` | `IDD_PANEL` 对话框与字符串表（GBK 编码） |
| `resource.h` | 资源 ID 定义 |
| `CADDimension.def` | DLL 导出符号与序号 |
| `CADDimension.vcxproj` | 三配置构建、SDK 属性表、链接库 |

## 3. 注册与对象模型

- `DllEntry.cpp` 导出标准插件入口；当前 `LibNumberClasses()` 返回 1，`LibClassDesc(0)`
  返回 `GetCADDimensionDesc()`（`DllEntry.cpp:48-61`）。规划中的四类 Bundle
  （四个 ClassDesc）见 [ANNOTATION_SUITE.md](ANNOTATION_SUITE.md)。
- `CADDimensionClassDesc` 派生自 `ClassDesc2`，`Create()` 返回 `new CADDimension()`
  （`CADDimension.cpp:240-260`）。
- `Class_ID(0x5bcf0c90, 0x36fdd779)` 与 `GEOMOBJECT_CLASS_ID` 必须保持不变，
  否则已保存场景中的对象无法解析（`CADDimension.h:38`）。
- `DllMain` 只做三件事：切换语言包区域、保存 `hInstance`、禁用线程库调用；
  不在 `DllMain` 内做初始化（`DllEntry.cpp:26-37`）。

## 4. 引用契约（核心）

```text
INode（标注节点）
  └─ Object*  = CADDimension
       引用槽 0 ──► IParamBlock2 pblock
                        └─ 参数 p_target (TYPE_INODE) ──► 目标 INode
                                                              └─ 目标 Object
```

- `NumRefs() == 1`、`NumSubs() == 1`，唯一的引用槽 `kPBlockRef = 0` 指向参数块
  （`CADDimension.h:73-81`）。
- **目标节点不再单独占引用槽**，只存在于参数块的 `TYPE_INODE` 参数中。
- 这样做的原因：早期实现用两个引用（参数块 + `P_OWNERS_REF` 节点），整场景克隆
  时 `RemapDir` 回填可能让克隆参数块里的目标被改写，出现"克隆标注指向克隆目标"
  或引用分叉。单引用源消除了两份状态，克隆逻辑只需修复参数块（B1.1a）。
- `GetReference`/`SetReference` 只处理槽 0（`CADDimension.cpp:329-338`）。

**约束：新增任何节点引用都必须通过同一参数块中的新 `TYPE_INODE` 参数实现，
不要增加引用槽。** 若确需增加引用，必须同时更新 `NumRefs`、`GetReference`、
`SetReference`、`Clone`、撤销逻辑与探针。

## 5. 参数块

参数块定义在 `CADDimension.cpp:271-296`，使用 `P_AUTO_CONSTRUCT + P_AUTO_UI`，
构造时自动创建并通过 `SetReference(kPBlockRef, ...)` 挂接（`Create()`，
`CADDimension.cpp:312-319`）。

| 枚举 | 参数名 | MAXScript 属性 | 类型 | 默认值 | 范围 | 界面 |
|------|--------|----------------|------|--------|------|------|
| `p_target` | `targetNode` | `targetNode` | `TYPE_INODE` | 空 | — | `IDC_PICK_TARGET` 拾取按钮 |
| `p_text_size` | `textSize` | `textSize` | `TYPE_FLOAT` | 12.0 | 6–48 | `IDC_TEXT_SIZE_EDIT/SPIN` |
| `p_dim_offset` | `dimensionOffset` | `dimensionOffset` | `TYPE_FLOAT` | 28.0 | 8–200 | `IDC_OFFSET_EDIT/SPIN` |

`p_target` 的关键标志：

- `P_USE_NODE_TM_VALIDITY | P_USE_NODE_OS_VALIDITY`：参数块有效性跟随目标节点的
  变换与对象状态，目标移动/变形后视口可重绘。
- `p_accessor = &g_caddimAccessor`：绑定变更时刷新视口并生成撤销记录。
- `p_nonLocalizedName`：提供英文内部名，便于本地化与 MAXScript。

`P_AUTO_UI` 使用的对话框是 `IDD_PANEL`（`CADDimension.rc:28-42`），中文标签为
`目标:`、`文字大小:`、`偏移:`。资源文件必须保持 GBK / code page 936 编码。

## 6. 创建与拾取

创建回调为 `CADDimensionCreateCallBack`（`CADDimension.cpp:514-573`）：

1. `MOUSE_FREEMOVE`：`SnapPreview` 预览。
2. `MOUSE_ABORT` / `MOUSE_PROPCLICK`：重置并中止。
3. `MOUSE_POINT`（第一次点击）：
   - 若当前恰好选中 1 个有效目标，**立即绑定并在本次点击放置**；
   - 否则调用 `PickNode()`，用 `CADDimPickFilter` 过滤后绑定，进入 `stage = 1`；
4. 第二次点击（`stage == 1`）：完成放置，结束创建。
5. 创建期绑定使用 `SetTargetNode(node, t, /*allowUndo=*/false)`，不产生撤销项
   （`CADDimension.cpp:437-445`）。

目标合法性（`IsInvalidTarget` 与 `IsValidCADDimTarget`）：

- 非空，且求值对象非空；
- 不能是标注对象自身，也不能是另一个 `CADDimension`；
- 灯光（`LIGHT_CLASS_ID`）一律拒绝；
- 辅助对象仅在 `GetDeformBBox` 返回非空盒时接受（如 TapeHelp）；
- 间接环（经修改器栈形成的 CADDimension → CADDimension）用
  `thread_local` 深度计数阻断，深度上限 8（`CADDimension.cpp:604-612`）。

**语义细节**：向 `targetNode` 赋一个非法节点时，`PBAccessor::PreSet` 会把输入
强制改为 `nullptr`（`CADDimension.cpp:179`）。因此结果是"清空绑定"，并且该清空
本身可撤销。探针以 `cd.targetNode == undefined` 验证这一点。

## 7. 包围盒与坐标

### 7.1 取目标包围盒

`GetTargetBoxes()`（`CADDimension.cpp:592-626`）：

1. 取目标节点并求值；
2. 用 `os.obj->GetDeformBBox(t, localBox, nullptr, FALSE)` 取**局部**包围盒；
3. 8 个角点经 `target->GetObjectTM(t)` 变换到世界坐标，合并为世界盒；
4. 返回局部盒、世界盒与目标对象变换。

`GetTargetDrawData()` 另外返回 8 个世界角点与 `localSize = pMax - pMin`
（局部盒三边长度），供绘制与标注取数（`CADDimension.cpp:628-641`）。

### 7.2 三个包围盒虚函数

现代 SDK 没有 `Object::BoundingBox()` 虚函数。实际实现：

| 函数 | 行为 |
|------|------|
| `GetWorldBoundBox` | 返回目标世界盒，并按视口像素余量外扩（见下） |
| `GetLocalBoundBox` | 把世界盒 8 角点用**标注节点** `GetObjectTM()` 的逆变换回局部 |
| `GetDeformBBox` | 返回世界盒；若传入 `tm`，返回 `wb * Inverse(*tm)` |

`BoundingBox()` 是本类保留的普通辅助函数，不是虚函数覆盖
（`CADDimension.cpp:643-653`，头文件注释见 `CADDimension.h:12-14`）。

### 7.3 世界盒外扩

`GetWorldBoundBox()`（`CADDimension.cpp:655-700`）在外扩时考虑标注本身的像素
占用：

```text
offsetPx  = dimensionOffset（默认 28）
marginPx  = offsetPx + 120            // 尺寸线 + 箭头 + 文字余量
worldPerPixel = GetVPWorldWidth(盒中心) / 视口宽度
margin    = worldPerPixel * marginPx * NonScalingObjectSize
```

若无法取得视口信息，退化为 `0.05 * 盒对角线长度 + 0.1`。无目标时，以标注节点
位置生成一个约 24 像素的小盒。

`HasViewDependentBoundingBox()` 返回 `TRUE`（`CADDimension.h:89`），因为外扩量
依赖视口。

## 8. 视口绘制管线

`Display()`（`CADDimension.cpp:777-823`）的步骤：

1. 保存 `GraphicsWindow` 的变换矩阵、渲染限制与文字点大小；
2. `setTransform(Matrix3())` 置单位矩阵——`wTransPoint` 需要世界坐标直接进入；
3. 设置线框绘制限制与颜色；
4. 文字点大小 = `textSize * NonScalingObjectSize`，钳制到 6–40；
5. 取目标绘制数据；失败则画"无目标/不支持目标"标记；
6. `BuildScreenPrimitives()` 生成线段与文字；
7. 逐段裁剪后 `wPolyline()`，逐字定位后 `wText()`；
8. 恢复全部 `GraphicsWindow` 状态。

### 8.1 世界 → 屏幕

`MapWorldToScreen()`（`CADDimension.cpp:734-745`）：

- 使用 `gw->wTransPoint(&world, &ip)`，**不是** `ViewExp::MapWorldToScreen`
  （2027 SDK 不存在该接口）；
- 输出原点在左上角，与鼠标 `IPoint2` 坐标一致，可直接用于命中测试；
- 透视视图中相机前方的点：若 `wTransPoint` 返回 `GW_FRONT_PLANE` 标志，则视为
  不可用；正交视图忽略该标志。

### 8.2 屏幕图元生成

`BuildScreenPrimitives()`（`CADDimension.cpp:828-953`）生成：

1. **包围盒 12 条边**：8 个世界角点投影后按固定边表连接；任一端点无效则跳过该边。
2. **三条尺寸线**，对应目标**局部** AABB 轴：
   - X：角点 `0 → 1`
   - Y：角点 `0 → 2`
   - Z：角点 `0 → 4`

   角点顺序见 `BoxCorners()`（`CADDimension.cpp:53-65`），因此尺寸方向始终是目标
   的局部轴方向，而不是世界轴方向（B3）。
3. 每条轴的处理：
   - 若投影后线段长度 `< 4 * ns`（该轴几乎与视向平行），跳过该轴；
   - 计算线段法线，并强制指向屏幕包围盒外侧；
   - 偏移量 `(dimensionOffset + axis * 22) * ns`，三条尺寸线错开排列；
   - 画尺寸界线（留 2 px 间隙、超出 5 px）、尺寸线、两端 V 形箭头；
   - 箭头长度 `7 * ns`，且不超过该轴投影长度的 30%；
   - 轴标签为 `X|Y|Z %.3f`，取局部盒对应边长，位置在尺寸线中点外侧 11 px。
4. **汇总标签**：`X %.3f   Y %.3f   Z %.3f`，位于屏幕包围盒左上角；即使某轴
   被跳过，仍可看到三个方向的尺寸。
5. 固定容量：线段 48 条、标签 4 个。调用点位于 `Display()` 与 `HitTest()`，
   扩容时两处都要改。

`NonScalingFactor()`（`CADDimension.cpp:44-50`）把
`ViewExp::NonScalingObjectSize()` 钳制到 0.1–10，作为所有屏幕尺寸的用户系数。

### 8.3 绘制与裁剪

- `DrawScreenSeg()`（`CADDimension.cpp:958-972`）：先做 Liang-Barsky 裁剪
  （`ClipSegmentToRect`，`CADDimension.cpp:67-97`），再以 2 点 `wPolyline` 绘制。
  `w` 系列函数不裁剪，越界可能崩溃。
- `DrawScreenLabel()`（`CADDimension.cpp:974-994`）：用 `getTextExtents` 居中，
  并夹到视口内；若文字仍放不下则放弃绘制。
- `DrawNoTargetMarker()`（`CADDimension.cpp:996-1020`）：在标注节点位置画十字，
  并显示 `CADDimension: no target` 或 `CADDimension: unsupported target`。

### 8.4 颜色

`SetupLineColor()`（`CADDimension.cpp:753-772`）：

| 状态 | 颜色 |
|------|------|
| 选中 | `GetSelColor()` |
| 冻结 | `GetFreezeColor()` |
| 其他 | 节点线框色 `GetWireColor()` |
| 无节点 | 黄色近似 `(0.9, 0.9, 0.2)` |

`LINE_COLOR` 与 `TEXT_COLOR` 设置为同一颜色（B4d），保证文字与线条一致。

## 9. 命中测试

`HitTest()`（`CADDimension.cpp:1025-1069`）完全在屏幕空间进行：

1. 先处理 `HIT_SELONLY` / `HIT_UNSELONLY`；
2. `MakeHitRegion` 的容差为 `5 * ns`，钳制到 2–12 px；
3. 用与显示相同的 `BuildScreenPrimitives()` 重建线段（标签不参与命中）；
4. `HitSegments()`（`CADDimension.cpp:1071-1114`）按区域类型判定：
   - `POINT_RGN`：点到线段距离 ≤ 容差；
   - `RECT_RGN` + `crossing`：任一线段与矩形相交即命中；
   - `RECT_RGN` + 窗口选择：**所有**线段两端点都在矩形内才命中；
   - `CIRCLE_RGN`：任一线段到圆心距离 ≤ 半径；
5. 无目标时，对十字标记做同样的线段命中。

## 10. 撤销 / 重做

### 10.1 捕获流程

`CADDimensionPBAccessor`（`CADDimension.cpp:170-233`）：

1. `PreSet`：校验新目标；若非法则改为 `nullptr`。在正常用户赋值时，把**旧目标**
   与时间压入待处理栈（最多 4 项）。
2. `PostSet`：弹出栈顶，读取新目标；相同则不产生撤销项；不同则：
   - 若当前没有 `theHold`，`Begin()` 并记录；
   - 创建 `CADDimTargetRestore`（`CADDimension.cpp:136-168`）；
   - `theHold.Put()` 成功后 `Accept(_T("CADDimension Target"))`。

`CADDimTargetRestore` 用 `MaxSDK::TypedSingleWeakRefMaker` 弱引用标注对象与新旧
目标：对象或目标被删除后，回放自动降级为空操作，不会悬空。

### 10.2 抑制标志

| 场景 | 标志 | 行为 |
|------|------|------|
| 克隆 / RemapDir 回填 | `m_inClone` | 跳过待处理栈与撤销记录 |
| 创建期自动绑定 | `m_suppressUndo` | 同上 |
| 撤销/重做回放 | `m_inUndoRestore` | 同上，避免递归记录 |
| 宿主正在撤销/重做 | `theHold.RestoreOrRedoing()` | 同上 |

`PushPendingTarget`/`PopPendingTarget` 是 LIFO 栈；连续多次赋值可按相反顺序
逐步撤销/重做（探针第 10 节验证）。栈满（4 项）时整体重置后再压入，属于防御性
策略。

## 11. 克隆

`Clone()`（`CADDimension.cpp:367-386`）：

1. `new CADDimension()` + `BaseClone()`；
2. 显式克隆参数块引用：
   `obj->ReplaceReference(kPBlockRef, remap.CloneRef(pblock))`；
3. 注册 `CADDimCloneTargetPostPatch`（`CADDimension.cpp:340-365`）；
4. 清空有效性。

**为什么需要 PostPatchProc**：`RemapDir` 的回填阶段可能在克隆完成后改写克隆
参数块中的节点参数，使其指向被映射的（克隆）目标。PostPatchProc 在回填结束后
运行：取映射后的参数块，若目标不等于原目标，则临时关闭参数块通知、把目标写回
**原始目标**、恢复通知。`m_inClone` 同时阻止访问器记录撤销、阻止
`NotifyRefChanged` 产生副作用。

探针覆盖：单对象克隆、整场景克隆（目标+标注同选）、克隆的克隆、仅标注克隆、
目标+标注克隆、共享目标。所有情况下克隆标注都应保持原目标。

## 12. 通知与失效

`NotifyRefChanged()`（`CADDimension.cpp:388-411`）：

- `m_inClone` 期间直接返回 `REF_SUCCEED`；
- `REFMSG_CHANGE` / `REFMSG_SUBANIM_STRUCTURE_CHANGED`（来自参数块）：
  清空 `ivalid`，并 `NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE)`；
- `REFMSG_TARGET_DELETED`：只做失效与通知。**参数块持有节点引用，禁止在此消息
  中调用 `DeleteReference()`/`ReplaceReference()`。**

`ObjectValidity()` 合并 `Object::ObjectValidity()` 与 `pblock` 的有效性
（`CADDimension.cpp:483-489`）。

## 13. 已知限制与设计边界

- 只标注目标**局部轴对齐包围盒（AABB）**，不计算最小/定向包围盒。
- 灯光不能作为目标；辅助对象必须有非空形变包围盒。
- 屏幕图元容量固定：48 条线段、4 个标签（3 轴 + 汇总）。
- 与视向几乎平行的轴不画尺寸线，但汇总标签仍显示三向尺寸。
- 标注位置由目标世界包围盒决定，不随标注节点自身变换移动；节点变换只在
  "无目标标记"与 `GetLocalBoundBox` 中参与计算。
- 非可渲染对象（`IsRenderable() == 0`），不参与渲染输出。
- 间接环只做深度上限防御，不是通用环检测。
- 旧版 B1 场景（`NumRefs == 2`）的加载兼容性由 G7 探针验证，但仓库不保存
  `.max` 场景文件。
- 当前 `CADDimension` 使用目标包围盒标注；规划中的"三点线性标注 + showXYZ"
  尚未实现，详见 [ANNOTATION_SUITE.md](ANNOTATION_SUITE.md)。

## 14. 源码索引

| 主题 | 位置 |
|------|------|
| 类声明与枚举 | `CADDimension.h:38-155` |
| ClassDesc | `CADDimension.cpp:240-266` |
| 参数块 | `CADDimension.cpp:271-296` |
| 构造函数 / `Create()` | `CADDimension.cpp:301-319` |
| 引用管理 | `CADDimension.cpp:329-338` |
| 撤销 RestoreObj | `CADDimension.cpp:136-168` |
| 参数访问器 | `CADDimension.cpp:170-233` |
| 克隆 | `CADDimension.cpp:340-386` |
| 通知 | `CADDimension.cpp:388-411` |
| 待处理目标栈 | `CADDimension.cpp:413-429` |
| 创建回调 | `CADDimension.cpp:494-579` |
| 目标校验 | `CADDimension.cpp:584-626` |
| 包围盒 | `CADDimension.cpp:643-729` |
| 世界→屏幕 | `CADDimension.cpp:734-745` |
| 颜色 | `CADDimension.cpp:747-772` |
| `Display` | `CADDimension.cpp:777-823` |
| 屏幕图元 | `CADDimension.cpp:828-953` |
| 图元绘制 | `CADDimension.cpp:958-1020` |
| 命中测试 | `CADDimension.cpp:1025-1114` |
| 插件导出 | `DllEntry.cpp:41-87` |
| 资源与字符串 | `CADDimension.rc:28-42`、`99-107` |

## 实现补充（M0–M3）

- `Common/CADDraw`：世界→屏幕、裁剪、屏幕线段/文字、颜色、`GWDrawGuard`。
- `Common/CADHit`：点/矩形/圆命中。
- `CADDimension` 拆分：Core / Bounds / Clone / Create / Draw / Hit / ThreePoint / UI，所有文件 ≤500 行。
- 引用契约：pblock 为真源；引用槽 1 仅为旧版 `NumRefs==2` 兼容槽，`PostLoadCallback` 迁移后清空。
- `CADText`：`SimpleObject2` + 空 `BuildMesh(TimeValue)` + 屏幕文字。
- `CADLeader`：`HelperObject` 三段交互，箭头折线 + 附着文字。
- `CADTable`：`TYPE_STRING_TAB` 单元格（`r*cols+c`），`SyncCells` 重建并保留重叠格。
- 已知限制：无 `getColor`（颜色不能恢复）；MUI 语言目录/中文面板需人工验收。
