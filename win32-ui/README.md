# 圣安地列斯控制中心 Win32 Steam 版

六个中文标签页按原版窗体、弹窗和操作手册核对。游戏联动使用原项目的经典 Steam 后端。游戏进程发现、版本校验、读取、写入和锁定由独立工作线程处理，前台线程负责界面和切页。程序在可用的游戏进程通过校验后连接，切回控制中心时会异步刷新当前载具与玩家数据。未连接时仍可查看地图和编辑配置。

连接后可点击右上角「刷新游戏数据」或按 `F5`，重新读取玩家、载具、时间、天气和地图上的玩家位置。武器区还提供「读取武器」，可见时会定期更新当前武器与弹药，保留尚未应用的武器修改。车库有独立的读取按钮。

随程序嵌入原项目的车辆数据、色表、中文词典、秘籍、地点和默认快捷键。支持的游戏指纹、两个二进制文件的便携打包方式、六页的单人存档验收步骤及已知限制见 [经典 Steam 版实机验收](STEAM-ACCEPTANCE.md)。

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

程序 `GTASAControlCenterUI.exe` 和后端 `GTASASteam.dll` 都位于 `.work\win32-ui\Release\`。复制时将两者放在同一目录。窗口按缩放后至少 1024×768 的桌面设计。

窗口标题、两个二进制的产品版本和便携 ZIP 名称共用 [`version.h`](version.h) 中的版本号。运行 `powershell -NoProfile -File .\win32-ui\version.ps1` 可读取当前版本。开发时的进位规则见 [版本规则](../AGENTS.md)。

完成构建后还可执行 `powershell -NoProfile -File .\win32-ui\smoke-steam-abi.ps1 -ProductionDll .\.work\win32-ui\Release\GTASASteam.dll`。该测试检查生产 DLL 的导出和拒绝不兼容目标的行为，并用独立的 x86 夹具测试类型化游戏操作。夹具不进入便携包。

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

本机在 Windows Server 2025 Hyper-V 远程会话、150% 显示缩放下，用便携包连续切换 600 次。提交页面显示状态的中位数约 4.9 ms，P95 约 7.6 ms。含应用内绘制的中位数约 75 ms，P95 约 130 ms，最慢约 274 ms。启动到首屏约 2.34 秒。载具页预建 190 个子窗口，玩家页预建 270 个，这两个数字包含各自隐藏分区的控件。此项在无游戏进程的基准模式下测量。

## 旧版 Windows 源码兼容

工程默认使用 v145。C 界面源码以 XP 时代的 Win32 API 为基线，配套 C++ 后端还依赖 Windows CryptoAPI 的 SHA-256 支持。旧系统需要选择支持目标系统的编译器、SDK 和 C 运行库重编译，再分别验证界面与后端。当前预编译产物按本机现代 Windows 环境验证。 [Microsoft 的 Windows XP 构建说明](https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp)
