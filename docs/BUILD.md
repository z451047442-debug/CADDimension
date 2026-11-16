# 构建、部署与加载

> 以 `CADDimension.vcxproj` 与 `scripts/probe_plugin*.ini` 为准；`.def` 保留为历史文件，不参与链接。项目只支持 **x64**，目标 3ds Max 版本为 2027。

## 1. 前置条件

| 依赖 | 要求 |
|------|------|
| Windows | x64（Windows 10/11） |
| Visual Studio | 含 C++ 桌面开发工作负载与 MSBuild |
| Windows SDK | `WindowsTargetPlatformVersion` 为 `10.0.19041.0`（见工程文件） |
| 3ds Max SDK | 2027 版；目录下必须有 `ProjectSettings\PropertySheets` 与 `lib\x64\Release` |

## 2. 环境变量

工程在 `Globals` 中读取 `MAXSDK_DIR`，并允许用 MSBuild 属性 `MaxSDK` 覆盖：

```xml
<MaxSDK Condition="'$(MaxSDK)' == ''">$(MAXSDK_DIR)</MaxSDK>
```

构建前设置（cmd）：

```bat
set MAXSDK_DIR=D:\3dsMaxSDK2027\maxsdk
```

PowerShell：

```powershell
$env:MAXSDK_DIR = "D:\3dsMaxSDK2027\maxsdk"
```

或不设环境变量、直接传参：

```bat
msbuild CADDimension.vcxproj /p:Configuration=Release /p:Platform=x64 ^
        /p:MaxSDK=D:\3dsMaxSDK2027\maxsdk
```

工程内置 `ValidateMaxSdk` 目标，会在以下情况直接报错：

- `MAXSDK_DIR` 为空；
- 目录下找不到 `3dsmax.general.project.settings.props`。

## 3. 构建配置矩阵

| 配置 | 用途 | 关键编译设置 | 链接的 SDK 库 |
|------|------|--------------|----------------|
| `Release` | 日常发布 | `/MD`、`NDEBUG`、`SECURE_SCL=0`、`/W4`（`/MD` 由 SDK 属性表/默认值提供） | `lib\x64\Release` |
| `Debug` | 附加调试器定位逻辑问题 | 额外取消 `_DEBUG`；`/MD`、`NDEBUG`、`_ITERATOR_DEBUG_LEVEL=0`；关闭增量链接 | `lib\x64\Release` |
| `Hybrid` | 介于两者之间，进一步取消 `_HYBRID`/`IS_HYBRID` | 与 Debug 相同 | `lib\x64\Release` |

### 3.1 为什么 Debug 也使用发布版 CRT

3ds Max 2027 SDK 只提供 **Release** 静态库（`lib\x64\Release`）。若 Debug
配置使用 `/MDd` 或 `_ITERATOR_DEBUG_LEVEL=2`：

- 迭代器/容器的内存布局与 SDK 库不一致；
- 分配与释放跨越 CRT 边界时堆损坏；
- 典型表现是加载插件即崩溃或随机崩溃。

因此工程做了三件事：

1. `UndefinePreprocessorDefinitions` 取消 `_DEBUG`；
2. 定义 `NDEBUG`、`SECURE_SCL=0`；
3. 追加 `/D_ITERATOR_DEBUG_LEVEL=0`，并强制 `/MD`。

**修改构建配置时不要破坏这组约束。** 三种配置都必须使用发布版 CRT 与 SDK
Release 库。

### 3.2 链接设置

- 附加库目录：`$(MaxSDK)\lib\x64\Release\`
- 附加依赖：`bmm.lib;core.lib;Geom.lib;gfx.lib;mesh.lib;maxutil.lib;maxscrpt.lib;manipsys.lib;paramblk2.lib`
- 模块定义文件：不使用；入口由 `extern "C" __declspec(dllexport)` 导出，链接加 `/OPT:NOICF`
- 链接增量：Debug/Hybrid 关闭
- 全程序优化：关闭

## 4. 构建命令

在 Visual Studio Developer Command Prompt 中执行：

```bat
msbuild CADDimension.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

三种配置全量验证：

```bat
msbuild CADDimension.vcxproj /t:Rebuild /p:Configuration=Debug   /p:Platform=x64
msbuild CADDimension.vcxproj /t:Rebuild /p:Configuration=Hybrid  /p:Platform=x64
msbuild CADDimension.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

也可以打开 `CADDimension.slnx`（需支持该解决方案格式的 Visual Studio 版本）。

## 5. 输出布局

```text
x64\<Config>\Plugins\CADDimension.dlo          # 插件本体
x64\<Config>\Plugins\en-US\CADDimension.dlo.mui # 本地化资源卫星文件
x64\<Config>\Plugins\CADDimension.ilk          # Debug/Hybrid 增量链接中间文件
obj\x64\<Config>\                              # 中间文件与 PDB
```

- 输出扩展名由工程设为 `.dlo`，这是 3ds Max 对象插件格式。
- `en-US\CADDimension.dlo.mui` 由 SDK 资源构建流程生成，**部署时必须与 `.dlo`
  保持相对目录结构**，否则中文界面字符串可能无法加载。
- `obj/`、`x64/`、`build.log`、`*.user` 均被 `.gitignore` 忽略，不要提交。

## 6. 加载插件

### 6.1 随 3ds Max 启动加载

将整个插件目录（含 `en-US` 子目录）加入 3ds Max 的第三方插件搜索路径，或复制
到 3ds Max 安装目录的 `plugins` 下。也可在
Customize → Configure System Paths → 3rd Party Plug-Ins 中添加输出目录。

### 6.2 用探针配置加载（推荐用于验证）

`scripts/probe_plugin*.ini` 通过 `Additional MAX plug-ins` 指定各配置的输出目录：

| 文件 | 指向 |
|------|------|
| `scripts\probe_plugin.ini` | `x64\Release\Plugins\` |
| `scripts\probe_plugin_debug.ini` | `x64\Debug\Plugins\` |
| `scripts\probe_plugin_hybrid.ini` | `x64\Hybrid\Plugins\` |

这些 ini 是 **UTF-16 LE**（带 BOM）编码。若仓库移动了位置，必须同步修改其中的
绝对路径；请使用支持 Unicode 的编辑器。

批处理加载示例：

```bat
"D:/Program Files/3ds Max 2027/3ds Max 2027/3dsmaxbatch.exe" ^
    scripts\probe_b1.ms -p scripts\probe_plugin.ini ^
    -log scripts\probe_b1.log -v 4
```

## 7. 导出符号

入口使用 `extern "C" __declspec(dllexport)`，`.def` 不参与链接；链接器按名称导出：

`LibDescription`、`LibNumberClasses`、`LibClassDesc`、`LibVersion`、`LibInitialize`、`LibShutdown`。

序号由链接器分配，不应依赖固定序数。`LibNumberClasses()` 返回 4，`LibClassDesc(0..3)`
分别返回 Dimension/Text/Leader/Table 的 `ClassDesc2`。`/OPT:NOICF` 用于避免 COMDAT
折叠导致 `dumpbin` 显示导出名错绑。DLO 还可能导出少量 SDK `CoreExport` 符号，
属已知 P2，不影响加载。

## 8. 故障排查

| 现象 | 可能原因 | 处理 |
|------|----------|------|
| 构建报 `MAXSDK_DIR is not set` | 未设置环境变量 | 设置 `MAXSDK_DIR` 或传 `/p:MaxSDK=` |
| 构建报 SDK property sheets 不存在 | 路径指向了 3ds Max 安装目录而非 SDK 根 | 指向包含 `ProjectSettings` 的目录 |
| 3ds Max 中找不到 `CADDimension` | `.dlo` 未加载/位数不对/版本不匹配 | 确认 x64 输出、插件路径、2027 版本；用探针 ini 排除路径问题 |
| 加载即崩溃或随机崩溃 | CRT/ABI 不一致 | 确认使用 `/MD`、`NDEBUG`、`_ITERATOR_DEBUG_LEVEL=0`，且链接的是 SDK Release 库 |
| 修改面板出现英文或空白 | MUI 未部署/资源编码错误 | 保留 `en-US\CADDimension.dlo.mui`；`.rc` 保持 GBK/936 |
| 重建时文件被占用 | 3ds Max 正加载着 `.dlo` | 关闭 3ds Max 后重建 |
| 加载了旧版本行为 | 多个路径存在同名插件 | 清理重复的 `.dlo`，只保留一个搜索路径 |
| 增量构建异常 | 中间文件损坏 | 使用 `/t:Rebuild` 或删除 `obj\x64\<Config>` |
