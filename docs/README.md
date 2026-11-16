# CADDimension 开发文档

CADDimension 是面向 **3ds Max 2027** 的 C++ 插件（`.dlo`），注册为 `GeomObject`
派生对象，在视口中为目标节点绘制 AutoCAD 风格的尺寸标注：目标局部包围盒的
12 条边、X/Y/Z 三条尺寸线、尺寸界线、箭头与尺寸文字。

本目录是项目的开发文档入口，内容以提交 `25884bb`（2026-11-16）的源码为准。
修改代码后，请同步更新对应文档。

> **规划状态**：当前分支只注册 `CADDimension` 一个类，且是目标包围盒标注实现；
> 四插件注释工具集（三点交互、`showXYZ`、CADText/CADLeader/CADTable）尚处于设计
> 阶段，见 [ANNOTATION_SUITE.md](ANNOTATION_SUITE.md)。

## 文档索引

| 文档 | 内容 | 何时阅读 |
|------|------|----------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | 对象模型、引用契约、创建/绘制/命中/撤销/克隆机制 | 修改核心行为前 |
| [BUILD.md](BUILD.md) | 环境变量、三种构建配置、SDK/ABI 约束、部署与加载 | 首次构建、换机器、排查加载失败 |
| [DEVELOPMENT.md](DEVELOPMENT.md) | SDK 2027 API 陷阱、编码规范、扩展配方、评审清单 | 日常开发 |
| [TESTING.md](TESTING.md) | 批处理探针、覆盖矩阵、手工回归清单 | 提交前验证 |
| [CHANGELOG.md](CHANGELOG.md) | B1–B4d 里程碑变更记录 | 追溯设计演进 |
| [ANNOTATION_SUITE.md](ANNOTATION_SUITE.md) | 四插件 Bundle 总体规划、CADDimension 三点标注设计 | 规划四类注释工具前 |
| [ANNOTATION_SUITE_CLASSES.md](ANNOTATION_SUITE_CLASSES.md) | CADText/CADLeader/CADTable 类声明与 ClassDesc 注册草案 | 实现新类前 |

## 快速开始

1. 设置 `MAXSDK_DIR` 指向 3ds Max 2027 SDK 根目录（其下应存在
   `ProjectSettings\PropertySheets` 与 `lib\x64\Release`）。
2. 构建 Release：
   `msbuild CADDimension.vcxproj /p:Configuration=Release /p:Platform=x64`
3. 让 3ds Max 加载 `x64\Release\Plugins\CADDimension.dlo`，并保留同目录下的
   `en-US\CADDimension.dlo.mui`（本地化资源卫星文件）。
4. 创建面板 → `CAD Tools` → `CADDimension`；或使用 MAXScript：

   ```maxscript
   cd = CADDimension()
   cd.targetNode = $Box001
   cd.textSize = 14
   cd.dimensionOffset = 36
   ```

详细步骤见 [BUILD.md](BUILD.md)。

## 插件事实表

| 项目 | 值 | 源码位置 |
|------|-----|----------|
| 类名 | `CADDimension` | `CADDimension.cpp:245` |
| 类别 | `CAD Tools` | `CADDimension.rc:102`（`IDS_CATEGORY`） |
| Class_ID | `Class_ID(0x5bcf0c90, 0x36fdd779)` | `CADDimension.h:38` |
| 超类 | `GEOMOBJECT_CLASS_ID` | `CADDimension.h:63` |
| 参数块 | `CADDimensionParams`（BlockID 0） | `CADDimension.cpp:271` |
| 参数 / MAXScript 属性 | `targetNode`、`textSize`、`dimensionOffset` | `CADDimension.cpp:277-294` |
| 插件版本声明 | `VERSION_3DSMAX` | `DllEntry.cpp:66` |
| 输出文件 | `x64\<Config>\Plugins\CADDimension.dlo` | `CADDimension.vcxproj:34-37` |

## 仓库结构

```text
CADDimension/
├─ CADDimension.h / .cpp     # 插件类：参数、绘制、拾取、撤销、克隆
├─ DllEntry.cpp              # DLL 入口与插件导出函数
├─ CADDimension.rc           # 对话框与本地化字符串（GBK / code page 936）
├─ resource.h                # 资源 ID
├─ CADDimension.def          # 导出符号表
├─ CADDimension.vcxproj      # Debug / Hybrid / Release 三配置
├─ CADDimension.slnx         # 解决方案
├─ scripts/
│  ├─ probe_b1.ms            # 批处理回归探针
│  ├─ make_legacy_b1.ms      # 生成旧版场景用于兼容性验证
│  └─ probe_plugin*.ini      # 各配置的插件加载路径（UTF-16 LE）
└─ docs/                     # 本目录
```

## 设计要点（一句话版）

- **单一引用源**：目标节点只通过参数块的 `TYPE_INODE` 参数持有，插件自身只有
  一个引用槽（参数块），见 [ARCHITECTURE.md](ARCHITECTURE.md#4-引用契约核心)。
- **屏幕空间标注**：世界坐标经 `GraphicsWindow::wTransPoint()` 投影后，以固定
  像素尺寸绘制，线宽/字号不随视口缩放变化。
- **可撤销绑定**：`targetNode` 的属性赋值由 `PBAccessor::PreSet/PostSet` 捕获并
  生成 `RestoreObj`，MAXScript、修改面板与撤销/重做保持一致。

## 当前实现状态（2026-11-16）

- 单 DLL 注册 4 类：`CADDimension`、`CADText`、`CADLeader`、`CADTable`。
- 三配置构建：`Release` / `Debug` / `Hybrid`，输出 `x64/<Config>/Plugins/CADDimension.dlo` 与 `en-US/CADDimension.dlo.mui`。
- 自动化探针全部通过：Dimension 29、Text 7、Leader 11、Table 20、B1 16（0 FAIL）。
- 已确认限制：三击/单击交互、视口视觉、中文面板/MUI、20×20 性能仍需人工验收；`GraphicsWindow` 无 `getColor`，颜色按对象重设。
