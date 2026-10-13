# FPGA ORB：所有版本与优化效果

当前基线 **u1：ACZ7015 / xc7z015clg485-2 / s_wrapper / 125 MHz，实测158.2902 FPS（6.31751 ms/帧）**，较r17提升10.48%。

本分支归档全部已发布路线及关键试验，包含原始源码、冻结编译输入、配对bit/ELF、时序/资源报告和实际验收日志。先阅读[所有版本与效果对照](docs/ALL_VERSIONS_RESULTS_ZH.md)，机器数据见[VERSION_RESULTS.json](docs/VERSION_RESULTS.json)。

| 路径 | 内容 |
|---|---|
| [u1/](u1/README_ZH.md) | 当前完整冻结基线，125MHz / 158.29FPS |
| [r17/](r17/README_ZH.md)、[r16/](r16/README_ZH.md)、[r15/](r15/README_ZH.md) | 历史完整冻结基线，143.27 / 120.44 / 110.57FPS |
| [orb60_4ppc/](orb60_4ppc/README.md) | 当前2018.3作者工作区、生成器、PS、测试和优化过程证据 |
| [orb60_board/](orb60_board/) | 历次接受发布包；2PPC、初始4PPC、PS直接DDR及两个工具链路线的实际板测 |
| [archive/2024_2/](archive/2024_2/README.md) | 原2024.2的2PPC/4PPC、100/125MHz源码、冻结输入、bit和报告 |
| [experiments/read8_125mhz/](experiments/read8_125mhz/) | 读窗口扩大但无有效性能提升的候选及实际捕获 |
| 原FPGA_ORB_HLS/、frozen/R11_DEFAULT/ | R11历史源码与默认ELF冻结输入，保持原发布内容 |

性能主口径是固定960×720、八层、原预算、normal280特征、不可变常驻DDR的完整PS循环，含准备/缓存/配置/调度，排除外部采集/传输和JTAG。实际板测、RTL内存模型结果和R11历史处理速率在对照文档中分别注明。Production保持COMPLETE_UNVERIFIED；精确结论来自对应Verified验收。

只读完整性校验：`pwsh -NoProfile -File tools/verify_versions.ps1`。此次整理未重新运行综合或板测；封存文件均保留原字节和许可证。历史绝对工具路径需要按主机配置，完整迁移重建未验证。

---

以下保留原R11发布说明。

# FPGA ORB R11：源码与配对固件

此仓库采用直接公开源码的方式，不再上传完整交接 ZIP。
源码、构建脚本、测试代码与说明可在线浏览；已验收的配对固件在
[Release](https://github.com/sunsn061128/fpga-orb-r11/releases/tag/r11-handoff-20261011)
单独下载。完整测试素材、SDK/BSP、生成工程和全部历史 job 仍保存在本地交接包。

## 目录与版本边界

| 路径 | 内容与边界 |
|---|---|
| `FPGA_ORB_HLS/` | 作者源码、HLS/PS/Vivado 脚本及测试；含旧实现和后续开发快照 |
| `FPGA_ORB_HLS/fastv2/next/` | 三核 mixed-PPC 实现及 PS 后端，开发快照含未启用的缓存实验 |
| `FPGA_ORB_SHARED_v1.0/` | CPU 权威参考源码、规范和原许可证；不含完整 vectors |
| `frozen/R11_DEFAULT/inputs/` | 已交付默认 R11 ELF 的冻结编译源；以此为固件源码依据 |
| `evidence/` | 默认 R11 主机记录、源码/产物清单及复用 R8 路由报告 |
| `docs/` | 构建前提、固件下载说明、公开文件溯源清单 |

开发快照不等于默认 ELF 的全部冻结编译源，不能把新缓存实验称为已上板版本。
源码复制保留字节和原目录关系；只新增公开分发说明，不修改算法。

## 已归档效果

- ACZ7015，`xc7z015clg485-2`，`next_system_wrapper`，PL 100 MHz。
- R11 默认 ELF 复用已验收 R8 bit/HDF；不是本次重新综合或上板。
- 最新 quiet 复测：Verified Pipeline 100/100 CPU 精确 PASS，摘要 `a1967dad68429025`。
- Production 固定 960×720 输入、八层、100 帧：平均 30.862 ms，最大 30.874 ms，约 32.402 FPS；终态是 `COMPLETE_UNVERIFIED`，不是逐帧精确 PASS。
- 路由 WNS +0.177 ns、WHS +0.051 ns、TNS/THS 0；LUT 20452、FF 23621、BRAM36 tile 61、DSP 16；仍有 44 条 DRC Warning。
- 已有真实 RTL：FAST 11、resize 13、Gaussian 14 事务通过；完整仿真 job 留在本地归档，不随源码仓库上传。

固定图短组结果不代表变化视频长期稳定 30 FPS、60 FPS、视频解码或完整 SLAM。
详细历史及失败/恢复边界保留在 next 目录的中文报告中。

## 使用

建议克隆到短路径，例如 `C:/h/orb`。版本保持 Vivado HLS/Vivado/SDK 2018.3，
主机 MinGW 14.2.0 与 CMake/Ninja。见 [构建前提](docs/BUILD_PUBLIC_ZH.md)
和 [固件说明](docs/FIRMWARE_ZH.md)。

没有完整外部测试输入时，不能宣称执行了全套回归。本文不授权自动 JTAG、
下载、复位或 Flash/SD/eFUSE 写入。

## 许可

CPU 参考保留 `FPGA_ORB_SHARED_v1.0/LICENSE`（GPL-3.0-or-later）及
`NOTICE.md`；`constants.hpp` 中保留原 GPL/BSD 来源声明。此整理不为全部
内容重新授予 MIT 等统一许可；使用和再分发须遵守各文件原许可。
第三方 SDK、手册和完整素材不在本源码仓库中。
