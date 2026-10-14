# GTA SA Control Center 汉化与优化源码

本项目基于GTA SA Control Center开源代码，汉化了修改器文本，适配了steam版GTASA。工程入口为 `GTASAConsole/GTASAConsole.vbp`，采用 VB6 工程格式，以 twinBASIC BETA 991 编译 Win32。源文件取自正式 release；Steam 后端取自对应的旧版 C++ 后端。

20261006 开发分支补全了 17 个车库存车编辑和锁定、一击必杀、任务显示计时冻结、六位女友统计和脚本进度同步、Hot Coffee 脚本开关、游戏内反馈、原版驾驶快捷键及遥控载具识别；同时修正 Steam 整数统计表起点。编译、原生夹具和旧版界面联调通过，真实游戏验收仍待进行。实现与使用边界见 `开发记录20261006.md`。

20261006 r2 继续恢复玩家页 22 组开关和锁定按钮，修复弹药锁定、传送旋转和玩家防护显示，增加车牌更新，并修正普通汽车与特殊车型的存车判断。可选火车组件仅在游戏实际加载且核对通过后启用相应车型，使用和移除条件见 [r2 开发记录](开发记录20261006-r2.md)。离线验证和真实游戏验收分别记录。

## 内容

| 目录或文件 | 用途 |
| --- | --- |
| `GTASAConsole/` | 正式表单、模块、控件、FRX/CTX 二进制设计资源、RES、图标、地图、车辆图片、数据表与帮助资料 |
| `native/SteamMemory.cpp`、`exports.def` | 可重建 GTASASteam.dll 的完整后端源码及导出清单；内含离线 UNIT_TEST |
| `localization/zh-CN.master.tsv` | 正式最终字典；构建时逐字节导出到生成目录 |
| `localization/source-tables/` | 保留的翻译编辑表，供维护查阅；不自动覆盖最终字典 |
| `resources/defaults/` | 干净默认设置；快捷键已恢复为 69 条、65 条启用，并与 RES 内置资源同步 |
| `vendor/vb6-controls/` | 正式版本使用的两个必要 OCX 运行依赖，版本 6.1.98.16 |
| `scripts/`、`build.cmd` | 可迁移的离线构建、字典导出及免注册清单嵌入脚本 |
| `tools.lock.json` | 工具版本、官方来源与已知哈希；不包含编译器或安装包 |
| `provenance/` | 20261004 基线对应关系及新功能地址、资源与离线验证记录 |
| `FILES.sha256` | 本次备份文件的 SHA-256 清单；运行 `python -B scripts/verify_files.py` 核验，编辑源码后需重新生成 |

保留了正式源码所需的二进制项目资源，不能因扩展名为 RES、FRX、CTX、OCX 而删除。帮助源文件和既有 CHM 均保留，避免丢失历史资料。没有打包游戏本体、C# 项目、离线禁用游戏的实验源码、编译器、MSI、下载缓存、用户 INI、用户 Config.dat、个人坐标、抓取转储或构建输出。

## 重建

在 Windows 上安装或解压以下工具，随后从本目录打开 CMD。工具无需放入版本库。

- [twinBASIC BETA 991](https://github.com/twinbasic/twinbasic/releases/tag/beta-x-0991)，构建目标 Win32。对应 ZIP 与 EXE 哈希见 tools.lock.json。
- [Python 3.10.3](https://www.python.org/downloads/release/python-3103/)；脚本仅用标准库，无 pip 依赖。
- [Node.js 24.19.0](https://nodejs.org/download/release/v24.19.0/)；仅用内置 fetch/WebSocket，无 npm 依赖。
- [Visual Studio / Build Tools](https://visualstudio.microsoft.com/downloads/) 的 C++ 桌面开发、MSVC x86 与 Windows SDK。本次使用 14.51.36231。
- Windows 自带的 OLE Automation 类型库；构建脚本从系统复制到生成目录，不更改系统注册表。

将 Python 和 Node 加入 PATH，将 twinBASIC EXE 路径设为 TWINBASIC_EXE。也可以把下载解压后的工具放在忽略的 `.tools/` 中，例如：

```bat
set "TWINBASIC_EXE=%CD%\.tools\twinBASIC-991\twinBASIC.exe"
build.cmd --twinbasic "%TWINBASIC_EXE%" --output ".work\restore-check"
```

MSVC 默认通过 vswhere 查找。若自动查找失败，先将 `VCVARS32_PATH` 设置为已安装工具链的 `vcvars32.bat` 完整路径。Node 不在 PATH 时增加 `--node "node.exe 的完整路径"`。输出目录必须尚不存在；再次构建应使用新的输出目录。所用编译器自动化依赖 beta 991，其他版本需重新验证。

脚本会完成：复制本目录源码到输出目录、验证并复制两个 OCX、解析生成副本中的 Windows OLE 引用、导出最终字典和干净默认配置、编译并运行 C++ 离线单元测试、编译后端 DLL、在独立桌面启动专用 twinBASIC 编译进程、导入 VBP、执行 Build、结束专用编译进程，最后嵌入并验证 `app-local-controls.manifest`。它不执行修改器，也不查找或连接游戏。

生成的 `GTASAConsole/GTASA_Center_ZH.exe`、`GTASASteam.dll`、两个 OCX、字典、数据表和图片位于指定输出目录内；`build-result.json` 保存编译结果及产物哈希。编译器缓存、日志和测试产物只位于输出目录；默认输出及启动临时目录由 .gitignore 忽略。

只准备可供手动导入的工程，可使用：

```bat
build.cmd --prepare-only --output ".work\manual-import"
```

此模式不编译后端。完整可运行包需先构建后端 DLL，再在 twinBASIC 中编译 Win32，最后执行清单嵌入步骤；推荐使用完整 build.cmd 流程。直接导入本目录的 VBP 可能出现相对 OLE 引用转换提示，生成副本会自动解析它。

运行旧版界面与新接口的离线联调，可在初始化开发环境后执行 `python scripts/verify-offline-ui.py --output .work/offline-ui-check --twinbasic 工具路径`。脚本只编译并运行专用测试副本，移除游戏发现与打开进程入口，使用当前测试进程中的模拟内存。`OFFLINE_UI_FIXTURE` 仅用于测试 DLL，正常构建没有这些导出。

已有配置中的 14 条驾驶快捷键可用 `scripts/Restore-OriginalDrivingShortcuts.ps1 -ConfigPath 配置路径` 恢复。先关闭修改器；工具保存完整备份，只把对应 UID 的 Active 字段从 0 改为 1，保留其他字段、汉化文字和个人记录。它不会恢复已删除的记录；新发行包提供完整默认预设。

离线验证通过并将源文件及更新后的 FILES.sha256 提交到本地分支后，运行 `python -B scripts/package-candidate.py --build 构建目录 --offline-ui 界面测试目录 --output 新的发行目录` 可生成独立 ZIP。输出放在源码目录外且必须尚不存在。打包脚本核对 x86、正式导出、免注册清单、内置快捷键、产物哈希及离线结果，只收录运行文件、必要资源、恢复工具、说明与验证记录，随后逐文件核验 ZIP。候选包记录源码提交，真实游戏验收状态保持 pending。

## 与正式交付的对应关系

正式原版 EXE SHA-256：

```text
76954b8b6763b9832fb9f201d3ffdf241f03825a0f61cff9da6b7e2845e737d7
```

上述 EXE 和 `provenance/source-correspondence.json` 对应保留的 20261004 基线。该基线仅有三处便携化调整：VBP 的 Windows OLE 引用改为相对路径；未启用的转储注释改为相对文件名；未加入工程的 mdlRegistry.bas 中遗留路径整理。本分支在基准提交 b96b553d5c1bdf35d7ec5dfbc4d912c4cbe6296f 上实现功能补全，不能再用历史来源哈希表示当前文件。`FILES.sha256` 记录当前源文件，新增验证记录见 `provenance/feature-evidence-20261006.json`。

重新生成的 EXE/DLL 可能因工具链、编译时间或资源嵌入而与既有发布产物哈希不同。

## 依赖与授权记录

两个 OCX 是必要运行依赖，未能确认精确历史版本仍可从官方重新下载，因此保留本次正式版本的原文件。旧包与组件哈希均已记录；当前微软下载页提供较新的 KB2708437 包，不能直接当成已验证的同版本替代品。没有包含旧 MSI 或开发工具。详情见 tools.lock.json 和 NOTICE.md。

本目录保留原作者声明、帮助与署名。版权与来源事实见 NOTICE.md。
