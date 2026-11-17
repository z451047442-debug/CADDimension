# CADDimension 1.0.1 — 安装与使用

面向 **Windows 11 x64 + 3ds Max 2027.2（29.2.0.20588）** 的原生插件 Bundle（`.dlo`），注册四个类：`CADDimension`、`CADText`、`CADLeader`、`CADTable`。

> 支持矩阵：本版本仅在 Windows 11 + 3ds Max 2027.2 上验证。Windows 10 与其他 2027.x 小版本未验证，不在 1.0.1 支持范围内。

## 安装

1. 关闭 3ds Max。
2. 将包内 `CADDimension.dlo` 与 `en-US\CADDimension.dlo.mui` 一起复制到 3ds Max 的插件搜索目录；或通过 `Additional MAX plug-ins` 指向解压目录。
3. 启动 3ds Max。创建面板 → `CAD Tools`，应看到 `CADDimension`、`CADText`、`CADLeader`、`CADTable` 四个类。
4. 语言说明：本版本为 **zh-first**；`en-US` 是唯一 MUI 目录，其中包含中文 UI。中文 3ds Max 应显示中文。

## 卸载

1. 关闭 3ds Max。
2. 删除 `CADDimension.dlo` 与 `en-US\CADDimension.dlo.mui`。
3. 重启 3ds Max，确认四个类不再出现。插件不写注册表、不创建服务。

## 升级

1. 关闭 3ds Max。
2. 用新版本的 `CADDimension.dlo` 与 MUI 覆盖旧文件。
3. 重启 3ds Max，打开旧场景验证。1.0.1 可打开 1.0.0/旧格式场景。

## 回滚到 1.0.0

1. 关闭 3ds Max。
2. 用 1.0.0 golden 文件覆盖：`CADDimension.dlo`（SHA256 `69fc02dc2bb5477d21fe992bfc0b9f98249a5413edafa0ade3b5eb49c578c9fd`）与 `en-US\CADDimension.dlo.mui`（SHA256 `34d53ccc9b64769fce3572d3820a469d7c9e8c4408f2a29e5a30e1c55f53d914`）。
3. 重启 3ds Max，打开旧场景验证四类与参数。回滚步骤与哈希同时记录在发布 manifest 中。

## MAXScript 示例

### CADDimension（三点模式；1.0.1 默认 mode = 1）

```maxscript
cd = CADDimension()
cd.mode = 1
cd.point1 = [0, 0, 0]
cd.point2 = [100, 0, 0]
cd.dimPoint = [50, 50, 0]
cd.textSize = 18
cd.dimensionOffset = 10
cd.showXYZ = true
```

### CADText

```maxscript
t = CADText()
t.text = "Hello CAD"
t.textSize = 24
```

### CADLeader

```maxscript
l = CADLeader()
l.text = "Leader"
l.start = [0, 0, 0]
l.elbow = [10, 0, 10]
l.textPos = [20, 0, 20]
l.arrowSize = 8
```

### CADTable

```maxscript
tb = CADTable()
tb.rows = 2
tb.cols = 2
tb.cellWidth = 80
tb.cellHeight = 24
tb.textSize = 12
tb.cells[1] = "A1"
tb.cells[2] = "B1"
```

## 已知限制

- `CADTable` 单元格文本、`CADLeader` 点位在 1.0.1 仍仅能通过 MAXScript 编辑（修改面板无对应控件）。
- `CADTable` 屏幕对齐，不随对象旋转；`GraphicsWindow` 无 `getColor`，颜色不能恢复；40×20 为条件允许上限。
- `GetDeformBBox` 的物体空间语义、`SyncCells` 撤销同步延后到 1.0.2。
- 发布包不含数字签名；请用 `SHA256SUMS.txt` 与带外发布的 ZIP SHA256 校验完整性。

## 包内容与校验

- 主包 6 个条目：`CADDimension.dlo`、`en-US/CADDimension.dlo.mui`、`README.md`、`CHANGELOG.md`、`smoke_release.ms`、`SHA256SUMS.txt`。
- `SHA256SUMS.txt` 覆盖 **5 个载荷**（DLO、MUI、README、CHANGELOG、`smoke_release.ms`）；SHA 文件自身不计入。
- `smoke_release.ms` 是验证脚本，运行需要内部验证归档中的 `smoke_plugin.ini`；普通用户安装不需要它。
- 版本资源：DLL `FileVersion/ProductVersion = 1.0.1.0`。
