# 圣安地列斯控制中心 Win32 界面原型

此阶段按旧版窗体和操作手册核对原生 C/Win32 界面。六个中文标签页为载具与游戏、玩家状态、车库、游戏秘籍、传送地点和快捷键。修改车辆、玩家和位置时使用本次会话的离线状态。随程序嵌入原项目的车辆数据、色表、中文词典、秘籍、地点和默认快捷键，启动时无需准备外部数据文件。

鼠标可直接点击标签，键盘可用 `Ctrl+Tab` 或 `Ctrl+Shift+Tab` 循环切换。标签栏获得焦点时也可用左右方向键。顶层切页沿用预建页面容器，保留输入与页面选择状态。

## 原版界面对照

| 标签页 | 核对内容 |
| --- | --- |
| [载具与游戏](../GTASAConsole/Help/page1.html) | 212 个车型、主色与副色的 127 色号色板、颜色锁定、车牌、载具防护、车门、引擎和轮胎、方向和速度、飞行辅助、天气与时间。 |
| [玩家状态](../GTASAConsole/Help/page2.html) | 主角火焰伤害防护、防火秘籍及其独立锁定、属性、武器和弹药、武器熟练度、移动和女友进度。 |
| [车库](../GTASAConsole/Help/page3.html) | 四个地区的 17 座车库、各车位的载具、颜色与改装、车库锁定及配置操作。 |
| [游戏秘籍](../GTASAConsole/Help/page4.html) | 130 条原项目秘籍、按组筛选、记录编辑、代码复制、配置读写。 |
| [传送地点](../GTASAConsole/Help/page5.html) | 503 个原项目坐标、地点编辑、交互坐标地图、50–400% 缩放、平移、中心复位、标记和取坐标、会话内传送预览。底图是原型自绘的区域示意图。 |
| [快捷键](../GTASAConsole/Help/page6.html) | 69 条默认绑定、130 条控制中心命令及参数、秘籍和地点选择、按键与修饰键、记录启停、分组及配置读写。 |

快捷键参数保留原项目的值编码。颜色和载具可通过选择器填写，其余参数可按界面提示编辑。配置导入支持旧版 ANSI 文本以及 Unicode 文本，车库保存操作保留现有 INI 的其他区段。

## 构建和试用

在 Visual Studio 2026 中打开 [GTASAControlCenterUI.sln](GTASAControlCenterUI.sln)，选择 **Release | Win32**，生成后用 `Ctrl+F5` 启动。需要 MSVC x86 工具和 Windows SDK。也可在 VS 2026 开发者命令行从仓库根目录构建。

```bat
msbuild win32-ui\GTASAControlCenterUI.sln /m /p:Configuration=Release /p:Platform=Win32
```

可执行文件位于 `.work\win32-ui\Release\GTASAControlCenterUI.exe`。它使用 Windows 自带的控件和静态 C 运行库，可从该目录直接启动。窗口按缩放后至少 1024×768 的桌面设计。

## 复测切页

从仓库根目录在可见桌面中运行以下 PowerShell 命令。第一条运行窗口导航测试，`-TestGlobalShortcuts` 会把测试窗口临时切到前台以验证组合键。其余命令运行独立的性能测试，并计算循环切页的中位数和第 95 百分位耗时。测试结束时窗口自动关闭。

```powershell
powershell -NoProfile -File .\win32-ui\smoke-navigation.ps1
$exe = (Resolve-Path '.\.work\win32-ui\Release\GTASAControlCenterUI.exe').Path
$tsv = Join-Path (Split-Path $exe) 'benchmark.tsv'
$run = Start-Process $exe -ArgumentList "--benchmark --benchmark-file `"$tsv`"" -Wait -PassThru
if ($run.ExitCode -ne 0) { throw "Benchmark failed with exit code $($run.ExitCode)." }
$warm = @(Import-Csv -LiteralPath $tsv -Delimiter "`t" |
    Where-Object phase -eq 'warm' | ForEach-Object { [double]$_.ms } | Sort-Object)
$median = ($warm[299] + $warm[300]) / 2
$p95 = $warm[569]
"Median: $median ms; P95: $p95 ms"
```

TSV 记录启动到首屏绘制、其余各页的首次切换，以及 600 次循环切页。`selection_ms` 记录从开始切页到提交页面显示状态的耗时，`ms` 还包括处理窗口绘制和提交 GDI 批处理。这两项度量应用内响应，桌面合成、显示器刷新和鼠标设备延迟都在该计时范围之外。冒烟脚本另行验证页面层级、句柄复用、列表行数，以及两种车辆颜色、两项防火设置和地图选点。

本机在 Windows Server 2025 Hyper-V 远程会话、150% 显示缩放下，用 Release Win32 构建连续切换 600 次。含应用内绘制的中位数约 53 ms，P95 约 92 ms，最慢约 112 ms。启动到首屏约 1.89 秒。载具页预建 185 个子窗口，玩家页预建 268 个，这两个数字包含各自隐藏分区的控件。

## 旧版 Windows 源码兼容

工程默认使用 v145。C 源码采用 XP 时代的 Win32 API 和旧工具集支持的 C 写法，中文源码保存为带 BOM 的 UTF-8。旧系统要用适合目标系统的编译器、SDK 和 C 运行库重新构建。以 Windows XP 为例，Microsoft 提供了 v141_xp 与配套 Windows 7 SDK 的方案，可用 MSBuild 的 `/p:PlatformToolset` 和 `/p:WindowsTargetPlatformVersion` 覆盖工程设置。本次只验证了当前工具链与系统。 [Microsoft 的 Windows XP 构建说明](https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp)
