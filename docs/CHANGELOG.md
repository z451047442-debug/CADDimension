# 变更记录

> 里程碑编号沿用提交信息中的 B1–B4d。日期取自 git 提交日期。
> 更细的代码差异请查看对应提交：`git show <hash>`。

## 未发布（工作区）

### 文档

- 新增 `docs/` 开发文档：README（索引）、ARCHITECTURE、BUILD、DEVELOPMENT、
  TESTING、CHANGELOG。
- 新增 `docs/ANNOTATION_SUITE.md`：四插件 Bundle 总体规划、CADDimension 三点
  标注与 `showXYZ` 设计、待确认决策。
- 新增 `docs/ANNOTATION_SUITE_CLASSES.md`：CADText/CADLeader/CADTable 类声明、
  四类 ClassDesc 注册草案与资源清单。

## 里程碑总览

| 里程碑 | 提交 | 日期 | 主题 |
|--------|------|------|------|
| 基线 | `1a49b55` | 2026-11-15 | 插件基线：对象、绘制、工程与资源 |
| B1 | `6bcaf74` | 2026-11-15 | 修正对象身份与引用契约 |
| B1 探针 | `eb91d0d` | 2026-11-15 | 增加克隆/引用批处理探针 |
| B1.1a | `30ffa65` | 2026-11-15 | 参数块单引用，修复整场景克隆 |
| B1.1b | `99023f9` | 2026-11-15 | PreSet/PostSet 撤销捕获 |
| B2 | `c0d8ec4` | 2026-11-16 | 创建期拾取与 Modify 面板（中文） |
| B3 | `12bc5ac` | 2026-11-16 | 局部包围盒与局部轴方向标注 |
| B4b | `0f05a8d` | 2026-11-16 | 三配置独立输出与 SDK/ABI 对齐 |
| B4d | `25884bb` | 2026-11-16 | 文字颜色、`RoundI` 钳制与 DllEntry 清理 |
| M0 | `e36a833` | 2026-11-16 | 旧场景 fixture 与 TYPE_STRING_TAB 探针 |
| M1a | `883783a` | 2026-11-16 | Common 绘制/命中层与 CADDimension 拆分 |
| M1b | `2cd6926` | 2026-11-16 | 三点标注、p_mode、showXYZ、旧场景迁移 |
| M2 | `939775c` | 2026-11-16 | CADText 与 CADLeader |
| M3 | `a9168de` | 2026-11-16 | CADTable 网格表格 |
| M4 | `b2c81a7` | 2026-11-16 | Pending 栈硬化、表格命中优化、文档 |
| M4b | `b2246d7` | 2026-11-16 | 入口导出名与 NOICF 修复 |

## 详细记录

### B4d — 文字颜色、`RoundI` 钳制与 DllEntry 清理（`25884bb`）

**修复**

- 线颜色与文字颜色统一设置：选中、冻结、线框色三条分支都同时设置
  `LINE_COLOR` 与 `TEXT_COLOR`，避免文字与线条颜色不一致。
- `RoundI()` 对非有限值返回 0，并把输入钳制到 `[-1e6, 1e6]`，避免异常投影值
  转 int 时溢出。
- 清理 `DllEntry.cpp` 中的冗余代码（2 行），保留标准导出与空初始化实现。

### B4b — 三配置独立输出与 SDK/ABI 对齐（`0f05a8d`）

**构建**

- `Debug`、`Hybrid`、`Release` 三配置分别输出到
  `x64\<Config>\Plugins\`，中间文件到 `obj\x64\<Config>\`。
- 统一链接 SDK 的 `lib\x64\Release`；Debug/Hybrid 取消 `_DEBUG`、定义
  `NDEBUG` 与 `SECURE_SCL=0`、强制 `/MD`，并追加
  `/D_ITERATOR_DEBUG_LEVEL=0`，与 SDK 的发布版 CRT 对齐。
- 新增 `probe_plugin_debug.ini`、`probe_plugin_hybrid.ini`。

### B3 — 局部包围盒与局部轴方向标注（`12bc5ac`）

**新增/调整**

- 标注数据改为目标节点的**局部**包围盒：`GetDeformBBox(t, localBox, nullptr)`
  取局部盒，8 角点经 `GetObjectTM()` 变换到世界。
- 三条尺寸线对应局部 AABB 的 X/Y/Z 轴（角点对 `0→1`、`0→2`、`0→4`），
  目标旋转后尺寸方向随局部轴变化。
- `GetLocalBoundBox`、`GetDeformBBox` 与包围盒外扩逻辑相应调整。

### B2 — 创建期拾取与 Modify 面板（中文标签）（`c0d8ec4`）

**新增**

- 创建回调支持两段式流程：有唯一有效选择时首次点击自动绑定并放置；否则先
  拾取目标，再次点击放置。
- 目标拾取过滤器拒绝标注对象自身、其他 CADDimension 与无效对象。
- Modify 面板参数块 UI（`IDD_PANEL`）本地化为中文标签：目标、文字大小、偏移；
  字符串表 `IDS_PARAMS` 为"参数"。

### B1.1b — PreSet/PostSet 撤销捕获（`99023f9`）

**修复**

- 通过 `PBAccessor::PreSet` 记录旧目标、`PostSet` 生成 `CADDimTargetRestore`，
  使 MAXScript 的 `cd.targetNode = ...` 赋值可撤销/重做。
- 连续赋值使用待处理栈逐步回放。
- 克隆、创建期绑定、撤销回放期间抑制撤销记录。
- 非法目标在 `PreSet` 中被强制为空，非法赋值可撤销。

### B1.1a — 参数块单引用，修复整场景克隆（`30ffa65`）

**修复**

- 引用模型收敛为单引用槽：目标节点只保存在参数块的 `TYPE_INODE` 参数中，
  `NumRefs() == 1`。
- 修复 D4 整场景克隆时克隆标注目标被改写的问题；克隆在 `RemapDir` 回填后由
  PostPatchProc 重新写回原目标。
- 新增 `make_legacy_b1.ms`，扩展探针到克隆矩阵、保存/加载、删除与撤销。

### B1 — 修正对象身份与引用契约（`6bcaf74`）

**修复**

- 修正对象身份判定（自身/同类对象不可作为目标）。
- 明确引用契约与目标校验，为后续单引用模型与撤销捕获打基础。

### B1 探针（`eb91d0d`）

**测试**

- 新增 `scripts/probe_b1.ms`：类解析、绑定、克隆矩阵、共享目标、保存/加载、
  删除与撤销/重做。
- 新增 Release 探针插件 ini；探针产物加入 `.gitignore`。

### 基线（`1a49b55`）

**新增**

- 3ds Max 2027 插件工程：`GeomObject` 派生对象、参数块、屏幕空间尺寸标注、
  包围盒、命中测试、资源与构建工程。

## [1.0.0] - 2026-11-16

> 首个生产版本：四类注释工具集（Dimension/Text/Leader/Table）。

### Added
- 单 `.dlo` 四类：`CADDimension`（三点 + `showXYZ` + `p_mode`）、`CADText`、`CADLeader`、`CADTable`。
- `Common/CADDraw`、`Common/CADHit` 共享绘制/命中层。
- 旧 `NumRefs==2` 场景 post-load 迁移与兼容槽。
- `tests/TypeStringProbe/` 探针工程；M0–M3 MAXScript 探针。

### Changed
- `CADDimension` 按职责拆分为 Core/Bounds/Clone/Create/Draw/Hit/ThreePoint/UI。
- 点位坐标使用 `ParamMap2UserDlgProc` 的 9 个 spinner。

### Verified
- 三配置构建；Dimension 29、Text 7、Leader 11、Table 20、B1 16 探针 0 FAIL。

### Known limitations
- 交互/视口/面板/MUI 人工验收待 step 11。
- `GraphicsWindow` 无 `getColor`，颜色不能恢复。
- `CADTable` 屏幕对齐，不随对象旋转；首期单元格编辑仅 MAXScript。
