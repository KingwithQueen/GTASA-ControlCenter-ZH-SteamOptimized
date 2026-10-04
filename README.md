# GTA SA Control Center 汉化与优化源码

本项目基于GTA SA Control Center开源代码，汉化了修改器文本，适配了steam版GTASA。工程入口为 `GTASAConsole/GTASAConsole.vbp`，采用 VB6 工程格式，以 twinBASIC BETA 991 编译 Win32。源文件取自正式 release；Steam 后端取自对应的旧版 C++ 后端。

## 内容

| 目录或文件 | 用途 |
| --- | --- |
| `GTASAConsole/` | 正式表单、模块、控件、FRX/CTX 二进制设计资源、RES、图标、地图、车辆图片、数据表与帮助资料 |
| `native/SteamMemory.cpp`、`exports.def` | 可重建 GTASASteam.dll 的完整后端源码及导出清单；内含离线 UNIT_TEST |
| `localization/zh-CN.master.tsv` | 正式最终字典；构建时逐字节导出到生成目录 |
| `localization/source-tables/` | 保留的翻译编辑表，供维护查阅；不自动覆盖最终字典 |
| `resources/defaults/` | 从正式 RES 中提取的原始默认设置，不含用户当前设置 |
| `vendor/vb6-controls/` | 正式版本使用的两个必要 OCX 运行依赖，版本 6.1.98.16 |
| `scripts/`、`build.cmd` | 可迁移的离线构建、字典导出及免注册清单嵌入脚本 |
| `tools.lock.json` | 工具版本、官方来源与已知哈希；不包含编译器或安装包 |
| `provenance/` | 正式版本对应关系、来源哈希、移植调整及验证记录 |
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

## 与正式交付的对应关系

正式原版 EXE SHA-256：

```text
76954b8b6763b9832fb9f201d3ffdf241f03825a0f61cff9da6b7e2845e737d7
```

本备份保持实际编译源码与资源。仅有三处便携化调整：VBP 的 Windows OLE 引用改为相对路径；未启用的转储注释改为相对文件名；未加入工程的 mdlRegistry.bas 中遗留路径整理。具体前后哈希见 `provenance/source-correspondence.json`。编译时只在生成副本中将 OLE 引用解析成其当前实际位置。没有重新应用 baseline 补丁，也没有用 benchmark 源码替代正式源码。

重新生成的 EXE/DLL 可能因工具链、编译时间或资源嵌入而与既有发布产物哈希不同。

## 依赖与授权记录

两个 OCX 是必要运行依赖，未能确认精确历史版本仍可从官方重新下载，因此保留本次正式版本的原文件。旧包与组件哈希均已记录；当前微软下载页提供较新的 KB2708437 包，不能直接当成已验证的同版本替代品。没有包含旧 MSI 或开发工具。详情见 tools.lock.json 和 NOTICE.md。

本目录保留原作者声明、帮助与署名。版权与来源事实见 NOTICE.md。