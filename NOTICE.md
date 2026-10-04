# 来源、版权与第三方组件记录

## 原版项目与资源

上游项目为 GTA San Andreas Control Center v2.1.1。工程的版本声明指向 estetiksoft.de；原始 Help/ReadMe.txt 将项目称为 open source，并致谢社区开发者和 gtaforums.com。车辆图片及相关说明保留在 Help/carpics_dat.html、Help/page3.html；这些历史资料指向 g-unleashed.com。本目录保留了原始源文件注释、署名、帮助内容和资源。

本次提供的材料中未发现明确 SPDX 标识或独立完整许可证。这里记录已有事实，不将全部上游代码、图片或二进制资源改授予 MIT、Apache 或其他许可。

## 当前正式版本

VB6/twinBASIC 工程取自 LegacyPerf_20261004 的正式 release/GTASAConsole。包含汉化、性能优化、后端调用及输入修复后的实际发布源码。旧版 C++ SteamMemory.cpp 与 exports.def 取自对应的 trainer-steam-backend；它们没有依赖 C# 工程。新增便携构建脚本用于从这些源码离线重建。

provenance/source-correspondence.json 逐文件记录来源区域、相对路径、原哈希、明确的便携调整和排除项。provenance/delivery-manifest.json 保留既有正式交付事实，移除了旧电脑的目录路径；provenance/release-source-verification.json 保留正式源码检查事实。

原始源码和交付文件留在原项目，本次备份不改写它们。

## Microsoft VB6 控件

vendor/vb6-controls/TabCtl32.ocx 和 mscomctl.ocx 是 Microsoft 运行组件，两者版本均为 6.1.98.16；本项目对这两个组件不拥有源代码或重新授权权利。它们来自既有正式交付依赖，历史 MSI 包名为 VB60SP6-KB957924-v2-x86-ENU.msi；MSI 本身未纳入备份。

历史下载链接现已失效，当前[微软官方页面](https://www.microsoft.com/en-us/download/details.aspx?id=7030)提供较新的 VB60SP6-KB2708437-x86-ENU.msi。本次没有将新版当成与旧版字节相同，也没有下载或安装它。由于精确版本无法保证可重新获取，保留必要的两个原运行组件以消除恢复时的缺件；相关哈希见 tools.lock.json。其授权以 Microsoft 对应运行组件的条款为准。

项目通过原有 app-local-controls.manifest 提供免注册 COM 激活。构建步骤不运行 regsvr32。

## 外部开发工具

twinBASIC、Python、Node.js、MSVC、Windows SDK 及其安装包不在此目录中，它们使用各自许可和官方发行渠道。Windows OLE Automation 类型库由当前系统提供，不复制进入源码备份。工具版本和官方链接见 tools.lock.json。
