# 测试与验证

> 测试分两层：**批处理回归探针**覆盖引用/克隆/撤销/持久化等逻辑；
> **交互式手工回归**覆盖绘制、拾取、界面等无法批处理验证的行为。

## 1. 前置条件

- 已构建目标配置（默认 Release）。
- 使用与配置匹配的插件 ini：
  `probe_plugin.ini`（Release）、`probe_plugin_debug.ini`（Debug）、
  `probe_plugin_hybrid.ini`（Hybrid）。
- 3ds Max 2027 安装目录下存在 `3dsmaxbatch.exe`。
- 确保插件搜索路径中没有另一份旧的 `CADDimension.dlo`，避免加载到旧版本。

## 2. 批处理探针

### 2.1 运行命令

```bat
"C:\Program Files\Autodesk\3ds Max 2027\3dsmaxbatch.exe" ^
    scripts\probe_b1.ms -p scripts\probe_plugin.ini ^
    -log scripts\probe_b1.log -v 4
```

脚本内部会：

- 打开 `scripts\probe_b1_result.log` 写入结构化结果；
- 多次调用 `resetMaxFile #noPrompt`，**只应在批处理环境运行**；
- 失败时抛出异常（伴随 `[PROBE][FAIL]`），成功结束时输出 `[PROBE][DONE]`。

### 2.2 覆盖矩阵

| 编号 | 验证内容 | 期望 |
|------|----------|------|
| G7 | 旧版 B1 场景（`NumRefs == 2`）加载 | 不崩溃；无旧场景文件时输出 INFO 并跳过 |
| 1 | 类解析 | `CADDimension` 类可被 MAXScript 解析 |
| 2 | 绑定 | `cd.targetNode == ProbeBox` |
| 3 | 单对象克隆 | 克隆标注保持原目标 |
| 4 | 整场景克隆（目标 + 标注同选） | 所有克隆标注保持原目标 |
| 5 | 克隆的克隆 | 保持原目标 |
| 6 | 仅标注克隆 | 保持原目标 |
| 7 | 目标 + 标注克隆 | 保持原目标 |
| 8 | 共享目标 | 两个标注可绑定同一对象且互不影响 |
| 9 | 保存 / 加载 | 重新加载后目标引用与数量正确 |
| 10 | 删除目标 + 撤销/重做 | 删除不崩溃；撤销可恢复；重做不崩溃 |
| 11 | MAXScript 属性赋值撤销/重做 | 单次、连续多次赋值均可逐步撤销/重做；非法目标被拒绝且拒绝可撤销 |
| 12 | G5 重绘 / `ObjectValidity` | 批处理无法验证，输出 INFO，转手工回归 |

### 2.3 结果判读

- 出现 `[PROBE][PASS]`：单项通过。
- 出现 `[PROBE][FAIL]`：脚本抛出异常，测试失败；查看随后异常信息与
  `probe_b1.log`。
- 出现 `[PROBE][DONE]`：脚本正常走完。
- 部分环境在批处理中禁用撤销，脚本会以 INFO 提示"undo may be disabled"，
  此时相关项不算失败，但需要在交互式环境补测。

### 2.4 旧版场景兼容（G7）

`scripts\make_legacy_b1.ms` 用于生成旧格式场景：

```bat
"<3ds Max 2027>\3dsmaxbatch.exe" scripts\make_legacy_b1.ms ^
    -p scripts\probe_plugin.ini -log scripts\make_legacy_b1.log -v 4
```

注意：

- 该脚本的注释说明它需要用 **当时的 B1 二进制**运行才能生成真正的旧格式
  （`NumRefs == 2`、`P_OWNERS_REF`）场景；用当前构建运行生成的是当前格式。
- 生成文件为 `scripts\probe_b1_scene.max`，属于探针产物，已被 `.gitignore`
  忽略，不随仓库分发。若文件不存在，G7 自动跳过。

### 2.5 路径依赖

`probe_b1.ms` 中的场景与日志路径是绝对路径：

- 结果日志：`D:/PycharmProjects/CADDimension/scripts/probe_b1_result.log`
- 旧场景：`D:/PycharmProjects/CADDimension/scripts/probe_b1_scene.max`
- 往返场景：`D:/PycharmProjects/CADDimension/scripts/probe_b1_roundtrip.max`

`make_legacy_b1.ms` 同样写死输出路径。**仓库移动后必须先修改这些路径**，否则
探针会读写错误位置。

## 3. 手工回归清单

在交互式 3ds Max 2027 中执行：

| ID | 场景 | 步骤 | 期望 |
|----|------|------|------|
| M1 | 单选自动绑定 | 选中一个 Box，创建 CADDimension，点击一次 | 首次点击即完成绑定与放置 |
| M2 | 无选择拾取 | 取消选择，创建，点击目标，再点击放置 | 两段式完成，绑定正确 |
| M3 | 修改面板 | 选中标注，打开 Modify | 显示中文标签：目标 / 文字大小 / 偏移 |
| M4 | 拾取按钮重绑 | 用"目标"拾取按钮换一个对象 | 视口标注立即跟随新目标 |
| M5 | 参数调整 | 调整 `textSize` 与 `dimensionOffset` | 字号与偏移变化；线宽/字号不随视口缩放 |
| M6 | 目标变换 | 移动、旋转、缩放目标 | 标注跟随；尺寸方向沿目标局部轴 |
| M7 | 颜色 | 选中、冻结、修改线框色 | 线与文字同色（选中色/冻结色/线框色） |
| M8 | 删除目标 | 删除被标注对象 | 标注位置显示十字 + "no target"，不崩溃 |
| M9 | 撤销/重做 | 删除目标后撤销/重做；重新绑定后撤销/重做 | 状态正确恢复，无崩溃 |
| M10 | 克隆 | 单克隆、整场景克隆、克隆的克隆 | 克隆标注保持原目标 |
| M11 | 保存/加载 | 保存场景并重新打开 | 绑定、参数、显示均保持 |
| M12 | 命中测试 | 点选线上/空白；窗口框选与交叉框选 | 点选命中线段；窗口需完全包含，交叉相交即命中 |
| M13 | 视图类型 | 透视、正交、相机后方有点 | 不出现错误投影或崩溃 |
| M14 | 视口边缘 | 把目标移到视口边缘，文字贴边 | 线段被裁剪，文字不越界 |
| M15 | 多标注共享目标 | 两个标注绑定同一对象 | 两者均正常显示，互不影响 |

## 4. 变更与回归范围

| 改动区域 | 必测内容 |
|----------|----------|
| 引用 / 撤销 / 克隆 | 批处理探针全部（1–11） |
| 绘制 / 包围盒 / 命中 | M5–M8、M12–M14 |
| 资源 / 修改面板 | M3、M4；确认 MUI 已部署 |
| 构建配置 / SDK 升级 | Debug、Hybrid、Release 三配置分别构建，并用对应 ini 各跑一次探针 |
| MAXScript 属性 | 探针第 11 项 + MAXScript 手工赋值/撤销 |
| 目标类型支持 | M1、M2、M8；确认拾取过滤与事后赋值行为一致 |

## 5. 已知限制

- 探针不能验证交互式重绘（G5），只能验证数据状态。
- 拾取对话框与修改面板控件无法在批处理中自动点击。
- 批处理环境的撤销能力取决于启动参数，部分撤销项只能交互式复测。
- 探针不做像素级视觉比对；颜色、箭头形状、文字位置需人工检查。

## M0–M3 自动化验证（2026-11-16）

命令模板：

```bat
"D:\Program Filesds Max 2027ds Max 2027dsmaxbatch.exe" scripts\<probe>.ms -p scripts\probe_plugin.ini -dm on -log scripts\<probe>.log -v 4
```

| 探针 | Release | Debug | Hybrid |
|---|---|---|---|
| `probe_dim_three_point.ms` | 29 PASS | 29 PASS | 29 PASS |
| `probe_cadtext.ms` | PASS | PASS | PASS |
| `probe_cadleader.ms` | 11 PASS | 11 PASS | 11 PASS |
| `probe_cadtable.ms` | 20 PASS | 20 PASS | 20 PASS |
| `probe_b1.ms` | 16 PASS | — | — |

旧场景 fixture：`scripts/fixtures/legacy_b1_nrefs2.max`，SHA256 `96fa6381ad376ceb62db4296a7c3c8ac39767bb386948d5dce90a13d18d7b441`。

人工清单（step 11）：三击/单击创建与预览、视口线/箭头/文字、缩放 10%–1000% 像素恒定、中文面板/MUI、20×20 表格流畅度。
