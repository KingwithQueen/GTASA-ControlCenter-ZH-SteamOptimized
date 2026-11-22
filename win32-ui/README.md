# 圣安地列斯控制中心 Win32 界面原型

此阶段用于评估原生 C/Win32 在接近原版控件密度时的界面响应。六个中文标签页为载具与游戏、玩家状态、车库、游戏秘籍、传送地点和快捷键。滑块、输入框、复选框、列表和车库地区选择可离线试用。可在列表详情中编辑并保存当次会话的示例数据。鼠标可点击标签，键盘可用 `Ctrl+Tab` 或 `Ctrl+Shift+Tab` 循环切换，标签栏获得焦点时也可用左右方向键。

所有页面及其子控件在启动时创建。切页切换两个页面容器的可见性，并重绘新页面。控件句柄、输入内容和列表滚动状态随页面保留。车库覆盖四个地区共 17 条样例，其他三个列表分别有 130 条秘籍、503 条地点和 69 条快捷键。虚拟列表按可见行请求数据，地点页还用 GDI 绘制样例坐标和所选地点。

## 构建和试用

在 Visual Studio 2026 中打开 [GTASAControlCenterUI.sln](GTASAControlCenterUI.sln)，选择 **Release | Win32**，生成后用 `Ctrl+F5` 启动。需要 MSVC x86 工具和 Windows SDK。也可在 VS 2026 开发者命令行从仓库根目录构建。

```bat
msbuild win32-ui\GTASAControlCenterUI.sln /m /p:Configuration=Release /p:Platform=Win32
```

可执行文件位于 `.work\win32-ui\Release\GTASAControlCenterUI.exe`。它使用 Windows 自带的控件和静态 C 运行库，可从该目录直接启动。窗口按缩放后至少 1024×768 的桌面设计。

## 复测切页

从仓库根目录在可见桌面中运行以下 PowerShell 命令。第一条运行窗口导航测试，`-TestGlobalShortcuts` 会把测试窗口临时切到前台以验证组合键。其余命令运行独立的性能测试，并计算循环切页的中位数和第 95 百分位耗时。测试结束时窗口自动关闭。

```powershell
powershell -NoProfile -File .\win32-ui\smoke-navigation.ps1 -TestGlobalShortcuts
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

TSV 记录启动到首屏绘制、其余各页的首次切换，以及 600 次循环切页。`selection_ms` 记录从开始切页到提交页面显示状态的耗时，`ms` 还包括处理窗口绘制和提交 GDI 批处理。这两项度量应用内响应，桌面合成、显示器刷新和鼠标设备延迟都在该计时范围之外。导航脚本另行验证原生标签栏的鼠标消息、方向键、组合键、页面层级、句柄复用和列表行数。

### 本机测试结果

Windows Server 2025 Hyper-V 远程桌面，150% 显示缩放，VS 2026 Release | Win32。启动到首屏绘制约 654 ms，其余五页的首次切换含绘制用时 25–74 ms。

| 600 次循环切页 | 中位数 | P95 | 最大值 |
| --- | ---: | ---: | ---: |
| 提交页面显示状态 | 4.4 ms | 6.5 ms | 9.0 ms |
| 含应用内绘制 | 72.2 ms | 111.3 ms | 133.0 ms |

载具页和玩家页分别有 124 和 143 个子窗口。测试中的主要耗时来自控件绘制，玩家页含绘制的 P95 为 125.0 ms。窗口导航测试通过。

## 旧版 Windows 源码兼容

工程默认使用 v145。C 源码采用 XP 时代的 Win32 API 和旧工具集支持的 C 写法，中文源码保存为带 BOM 的 UTF-8。旧系统要用适合目标系统的编译器、SDK 和 C 运行库重新构建。以 Windows XP 为例，Microsoft 提供了 v141_xp 与配套 Windows 7 SDK 的方案，可用 MSBuild 的 `/p:PlatformToolset` 和 `/p:WindowsTargetPlatformVersion` 覆盖工程设置。本次只验证了当前工具链与系统。 [Microsoft 的 Windows XP 构建说明](https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp)
