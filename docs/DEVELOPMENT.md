# 开发指南

> 适用对象：在本仓库中修改或扩展 CADDimension 的开发者。
> 架构背景见 [ARCHITECTURE.md](ARCHITECTURE.md)，构建见 [BUILD.md](BUILD.md)。

## 1. 开发流程

1. 阅读 [ARCHITECTURE.md](ARCHITECTURE.md) 的"引用契约"与"撤销/克隆"两节。
2. 修改代码（保持 `CADDimension.cpp` 的模块分区：基础工具 → 访问器 → ClassDesc
   → 参数块 → 引用/通知 → 创建 → 包围盒 → 绘制 → 命中）。
3. 构建 Release；涉及行为变更时再构建 Debug/Hybrid 验证 ABI。
4. 运行 [TESTING.md](TESTING.md) 中的批处理探针。
5. 涉及界面的改动做手工回归（G5 等无法批处理验证的项）。
6. 更新本目录文档与 [CHANGELOG.md](CHANGELOG.md)。

## 2. SDK 2027 API 陷阱（必读）

这些是代码中已经踩过并规避的问题，不要按旧版 SDK 的习惯改回去：

| # | 陷阱 | 正确做法 | 代码位置 |
|---|------|----------|----------|
| 1 | `Object::Display` 不是 3 参重载 | 实现 `Display(TimeValue, INode*, ViewExp*, int)` | `CADDimension.h:96` |
| 2 | `ViewExp` 没有 `MapWorldToScreen()` | 用 `GraphicsWindow::wTransPoint()`；调用前把 GW 变换置为单位矩阵 | `CADDimension.cpp:734-745` |
| 3 | `Object` 没有 `BoundingBox()` 虚函数 | 实现 `GetWorldBoundBox` / `GetLocalBoundBox` / `GetDeformBBox`；`BoundingBox()` 仅作内部辅助 | `CADDimension.cpp:643-729` |
| 4 | `Object` 没有虚 `Create()` | 本类的 `Create()` 是普通初始化函数，在构造函数中调用 | `CADDimension.cpp:301-319` |
| 5 | `wPolyline`/`wText` 不做视口裁剪 | 线段先做 Liang-Barsky 裁剪；文字先夹到视口内 | `CADDimension.cpp:67-97`、`958-994` |
| 6 | `wTransPoint` 的返回标志 | 透视视图中带 `GW_FRONT_PLANE` 的点不可用；正交视图忽略 | `CADDimension.cpp:740-744` |
| 7 | `NonScalingObjectSize` 语义 | 它是用户偏好系数，不是自动缩放；屏幕空间绘制已保证非缩放 | `CADDimension.cpp:44-50` |
| 8 | `REFMSG_TARGET_DELETED` 中不能操作引用 | 参数块持有节点引用，只做失效与通知 | `CADDimension.cpp:404-409` |

补充：

- `P_USE_NODE_TM_VALIDITY | P_USE_NODE_OS_VALIDITY` 让参数块有效性跟随目标
  变换/对象状态；不要删除，否则目标移动后可能不重绘。
- 所有 `GraphicsWindow` 状态（变换、渲染限制、文字点大小）必须在绘制后恢复；
  异常路径也不能遗漏。
- `Display` 内只用栈上固定数组，不做堆分配，不抛异常。

## 3. 编码规范

- 4 空格缩进；大括号独占一行（Allman）；`override` 显式标注。
- 文件内辅助函数用 `static`，不要污染导出符号。
- 优先使用 SDK 类型：`MSTR`、`MCHAR`、`TCHAR`、`Point2/Point3`、`IPoint2/IPoint3`、
  `Box3`、`Matrix3`。
- 数值转换必须有限性安全：`RoundI()` 对非有限值返回 0，并钳制到 ±1e6
  （`CADDimension.cpp:32-37`）。新增屏幕坐标转换时复用该函数。
- 屏幕图元容量是固定常量（48 线段 / 4 标签）。增加图元类型时同时修改
  `Display()` 与 `HitTest()` 两处调用点。
- 警告级别为 `/W4`，提交前保持无新增警告。
- 注释以中文为主，保留对 SDK 行为的英文说明；引用行号时注明基线提交。
- 字符串一律使用 `_T()` / `MCHAR` 宏，资源缺失时有代码内英文回退
  （例如 `GetString()` 返回空时回退 `_T("CADDimension")`）。

## 4. 资源与本地化

- `CADDimension.rc` 使用 **GBK / code page 936**，文件内声明
  `#pragma code_page(936)`。不要用 UTF-8 重存，否则中文标签会变成乱码。
- 当前中文标签：`目标:`、`文字大小:`、`偏移:`；字符串表 `IDS_PARAMS` 为"参数"。
- 新增界面文本的步骤：
  1. `resource.h` 增加控件 ID（当前下一个可用值见 `_APS_NEXT_CONTROL_VALUE`）；
  2. `CADDimension.rc` 在 `IDD_PANEL` 中加入控件（保持 GBK 编码）；
  3. 在参数块描述中通过 `p_ui` 绑定控件；
  4. 代码中的回退字符串保持英文，资源字符串提供本地化值；
  5. 构建后确认 `en-US\CADDimension.dlo.mui` 已生成并随 `.dlo` 部署。

## 5. 扩展配方

### 5.1 新增一个参数

以增加"小数位数"为例：

1. 参数 ID 枚举（`CADDimension.h:42`）：

   ```cpp
   enum CADDimParamIDs { p_target = 0, p_text_size, p_dim_offset, p_decimals };
   ```

2. 参数块描述（`CADDimension.cpp:271` 之后）：

   ```cpp
   p_decimals, _T("decimals"), TYPE_INT, 0, 0,
       p_default, 3,
       p_range, 0, 6,
       p_ui, TYPE_SPINNER, EDITTYPE_INT, IDC_DECIMALS_EDIT, IDC_DECIMALS_SPIN, 1.0f,
       p_nonLocalizedName, _T("Decimals"),
       p_end,
   ```

3. `resource.h` 增加 `IDC_DECIMALS_EDIT/SPIN`，并更新 `_APS_NEXT_CONTROL_VALUE`。
4. `CADDimension.rc` 的 `IDD_PANEL` 增加对应标签、编辑框与旋转器。
5. 使用时读取：

   ```cpp
   int decimals = 3;
   if (pblock) {
       Interval v; v.SetInfinite();
       pblock->GetValue(p_decimals, t, decimals, v);
   }
   ```

6. 格式化时检查固定缓冲区长度：

   ```cpp
   _stprintf_s(buf, _countof(buf), _T("%s %.*f"), axisName[axis], decimals, val);
   ```

7. MAXScript 属性名自动为 `cd.decimals`；无需额外注册。

注意：

- 只有 `p_target` 绑定了 `p_accessor`。新参数若需要撤销/重做，必须自己实现
  `PBAccessor` 或 `RestoreObj`，不要假设默认行为。
- 若新参数是节点类型，必须使用同一参数块中的 `TYPE_INODE`，并扩展克隆的
  PostPatchProc（见 5.4），不要新增引用槽。

### 5.2 让一个操作可撤销

`targetNode` 的撤销由访问器自动处理。其他可变状态可参考以下模式：

```cpp
class CADDimMyRestore : public RestoreObj
{
    MaxSDK::TypedSingleWeakRefMaker<CADDimension> dim;
    int oldValue, newValue;
public:
    CADDimMyRestore(CADDimension* d, int o, int n)
        : dim(d), oldValue(o), newValue(n) {}

    void Restore(int /*isUndo*/) override
    {
        CADDimension* d = dim.Get();
        if (!d) return;
        // 应用 oldValue（建议走无副作用的原始 setter）
    }
    void Redo() override
    {
        CADDimension* d = dim.Get();
        if (!d) return;
        // 应用 newValue
    }
    int Size() override { return sizeof(*this); }
};
```

使用：

```cpp
BOOL started = FALSE;
if (!theHold.Holding()) { theHold.Begin(); started = TRUE; }
if (theHold.Put(new CADDimMyRestore(this, oldValue, newValue)) && started)
    theHold.Accept(_T("CADDimension ..."));
```

规则：

- 回放路径必须绕过捕获逻辑（参考 `m_inUndoRestore` / `m_suppressUndo`）。
- 创建期、克隆期、撤销回放期间不得产生新的撤销项。
- 弱引用保护对象删除后的回放安全。

### 5.3 修改视口显示

- 图元生成集中在 `BuildScreenPrimitives()`（`CADDimension.cpp:828-953`）。
- 所有尺寸以像素为单位，并乘以 `NonScalingFactor(vpt)`。
- 新线段必须经过 `DrawScreenSeg()`；新文字必须经过 `DrawScreenLabel()`。
- 若增加图元数量，扩大 `segs`/`labels` 数组并同步 `maxSegs`/`maxLabels`。
- 先在透视与正交视图、目标接近视口边缘、目标部分在相机后方三种情况下自测。

### 5.4 新增节点类参数或改变克隆语义

克隆修复位于 `CADDimCloneTargetPostPatch::Proc()`（`CADDimension.cpp:349-364`）。
若新增 `TYPE_INODE` 参数：

1. 在 PostPatchProc 中取得映射后的参数块；
2. 关闭通知，把节点参数写回"原始目标"；
3. 恢复通知；
4. 用 `m_inClone` 包住，避免访问器记录撤销；
5. 扩展探针的克隆矩阵（单克隆、整场景、克隆的克隆）。

### 5.5 支持新的目标类型

目标准入逻辑集中在：

- `GetTargetBoxes()`（`CADDimension.cpp:592-626`）；
- `IsInvalidTarget()` / `IsValidCADDimTarget()`（`CADDimension.cpp:584-590`、`494-500`）。

默认规则：任何能返回非空 `GetDeformBBox` 的对象都可用；灯光明确拒绝；辅助对象
必须有真实包围盒。新增例外时，务必同时更新创建拾取过滤与属性赋值校验，避免
"创建时可绑、事后可绑"行为不一致。

### 5.6 扩展为多类 Plugin Bundle

当前 DLL 只注册一个类。扩展为四类时，把 `LibNumberClasses()` 改为 4，并在
`LibClassDesc(i)` 中分发到各自的 `ClassDesc2`；类 ID、基类、共享绘制与撤销契约
见 [ANNOTATION_SUITE.md](ANNOTATION_SUITE.md)。

## 6. 不可破坏的不变量

1. `NumRefs() == 1`，唯一引用是参数块；目标只存在于 `p_target`。
2. 参数块持有节点引用；不要在 `REFMSG_TARGET_DELETED` 中操作引用。
3. 撤销只在正常用户赋值时捕获；克隆/创建/回放必须抑制。
4. 克隆必须在 `RemapDir` 回填后修复目标节点。
5. 绘制必须在屏幕空间完成，并完整保存/恢复 GW 状态。
6. `wTransPoint` 调用前 GW 变换必须是单位矩阵。
7. 非法目标必须被拒绝（目前语义为清空绑定）。
8. 资源文件保持 GBK/936，MUI 与 `.dlo` 同结构部署。

## 7. 调试

- 使用 `Debug` 或 `Hybrid` 配置构建，输出 `x64\<Config>\Plugins\CADDimension.dlo`。
- PDB 位于 `obj\x64\<Config>\CADDimension.pdb`；在 Visual Studio 中附加到
  `3dsmax.exe`，并把该目录加入符号路径。
- 注意：Debug 配置链接的是发布版 CRT/SDK 库，SDK 内部断言与迭代器调试不可用；
  它只提供本插件代码的调试信息。
- 逻辑问题优先用批处理探针复现；绘制/交互问题需在交互式 3ds Max 中验证。

## 8. MAXScript 接口

```maxscript
-- 创建与绑定
cd = CADDimension()
cd.targetNode = $Box001

-- 参数
cd.textSize = 14          -- 6..48
cd.dimensionOffset = 36   -- 8..200

-- 查看全部属性
showProperties cd

-- 非法目标（会被拒绝并清空绑定）
cd.targetNode = cd

-- 克隆（克隆保持原目标）
cd2 = copy cd
```

## 9. 提交前评审清单

- [ ] Debug / Hybrid / Release 三配置均可构建。
- [ ] `scripts\probe_b1.ms` 全部 PASS，且输出 `[PROBE][DONE]`。
- [ ] 没有新增引用槽；若新增节点参数，克隆 PostPatch 已覆盖。
- [ ] 新增状态变更具备撤销/重做，且创建/克隆/回放路径已抑制。
- [ ] `GraphicsWindow` 状态在正常与失败路径都能恢复。
- [ ] 非法目标在创建拾取与属性赋值两条路径上行为一致。
- [ ] `.rc` 仍为 GBK/936；MUI 输出存在。
- [ ] 文档与 `CHANGELOG.md` 已同步。
