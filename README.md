# FPGA ORB R11 完整交接包

> 当前状态：完整 ZIP 上传失败，Release 附件尚未齐全；仅校验文本已上传。请暂勿将仓库或 Release 视为完整交付。本地短目录包及原交接资料均保留。

完整程序、测试素材、修改/测试历史和配对上板文件位于 **[Release 下载页](https://github.com/sunsn061128/fpga-orb-r11/releases/tag/r11-handoff-20261011)** 的 `r11.zip`。

不要用绿色 Code 菜单中的 Download ZIP 代替交接包；那个只下载本页说明。

## 下载与解压

1. 从 Release 的 Assets 下载 `r11.zip` 和 `r11.sha256.txt`。
2. 建议解压到 `C:/h`，得到 `C:/h/r11`，避免 Windows 长路径限制。
3. 先看 `SHORT_PATHS_ZH.md`、`README_ZH.md` 和 `docs/INDEX_ZH.md`。
4. 在解压后的 `r11` 根目录执行只读校验：

```powershell
pwsh -NoProfile -File tooling/verify.ps1
```

下载文件的 SHA256 也可用以下命令查看，与 `r11.sha256.txt` 中的值比较（仓库中的 `r11.sha256` 内容相同）：

```powershell
Get-FileHash -Algorithm SHA256 .\r11.zip
```

本次只是复制、缩短目录和重新打包，原工程和原压缩包不动。顶层缩短为 `r11`，`workspace` 改为 `w`；工程内部目录以及 FPGA/CPU 参考的兄弟关系保持不变。冻结源码、bit/HDF/ELF、测试素材和原始证据保持字节一致；仅交接包自己的导航说明与校验/手动入口适配路径。包内保留原清单和新的校验清单。

## 包内主要内容

| 路径 | 内容 |
|---|---|
| `release/R11_DEFAULT` | 已验收 R11 默认固件、配对 R8 bit/HDF、冻结编译源、SDK/BSP、测试输入和报告 |
| `release/R11_PROFILE_OPTIONAL` | 独立插桩固件，不与默认版混用 |
| `w/FPGA_ORB_HLS` | FPGA/HLS/PS 作者源码、生成器、主机/RTL 测试及历史任务证据 |
| `w/FPGA_ORB_SHARED_v1.0` | CPU 权威参考、验证集、逐段中间结果、规范和许可证 |
| `w/FPGA_ORB_SHARED_v0.95_testdata` | 早期参考与验证资料 |
| `background`、`docs` | 原有资料及交接说明 |
| `manual_helpers`、`tooling` | 已实测手动入口和只读包校验工具 |

工作区包含后续实验源码；**默认 ELF 的权威编译源是 `release/R11_DEFAULT/inputs`**，不能将后续工作区实验称为同一个已验收固件。

## 已有验收结果与边界

- ACZ7015，`xc7z015clg485-2`，`next_system_wrapper`，PL 100 MHz；Vivado HLS/Vivado/SDK 2018.3，主机 MinGW 14.2。
- R11 Verified/Pipeline 最近复测：100/100 逐帧 CPU 精确比较 PASS；摘要 `a1967dad68429025`。
- Production/Pipeline：960×720、8 层、固定图重复 100 帧，平均 **30.862 ms/帧**、最大 **30.874 ms**，约 **32.402 FPS**。Production 为 `COMPLETE_UNVERIFIED`，不是逐帧 CPU 比较 PASS。
- 已归档真实 RTL 仿真：FAST 11、resize 13、Gaussian 14 事务通过。
- 复用 R8 路由报告：WNS +0.177 ns、WHS +0.051 ns、TNS/THS 0；LUT 20452、FF 23621、BRAM36 tile 61、DSP 16；仍有 44 条报告 DRC 警告。

这些是包内已有测试证据，不是本次整理重新测试。范围不含视频解码、实时相机输入、跨层 B1 打包或完整 SLAM；不能宣称变化视频稳定 30 FPS、达到 60 FPS，或已完成本版 1000 帧物理长测。

包校验没有 JTAG 操作。实体下载会复位 PS/PL、覆盖 DDR，必须先阅读 `docs/MANUAL_BOARD_ZH.md` 并显式确认原板卡。不可自动上板或写 Flash/SD/eFUSE。

## 资料与许可

这是完整交接归档的公开分发，不是为全部内容重新授予开源许可。第三方手册、SDK、素材和参考代码保留原来源与许可证；使用、再分发须遵守各自条款。旧 job JSON/工程/冻结日志中的原电脑绝对路径保留为历史，不应直接复用旧缓存。

