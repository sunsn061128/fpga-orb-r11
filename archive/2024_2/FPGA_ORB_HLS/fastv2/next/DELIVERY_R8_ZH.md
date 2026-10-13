# R8 仿真与上板程序交付

产物交付验收时间：2026-10-10 02:30，北京时间；当时未操作 JTAG。随后用户授权上板，实体板卡一帧与连续 100 帧均 PASS，平均处理 59.516 ms/帧（16.802 FPS），尚未达到 30 FPS。详细口径与日志见 [R8 上板报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R8_ZH.md)。

## 已通过的验收

- 主机回归 12/12：包括独立 CPU 图像参考、方向/BRIEF，以及实际 PS 控制程序的主机模拟和错误注入。模拟计时不能作为板卡性能。
- FAST RTL 11/11：`hls_build/jobs/20261009_223114_front_ee8ba228`，与当前 FAST 源码配对。
- resize RTL 13/13、Gaussian RTL 14/14：`hls_build/jobs/20261010_004058_image_dd67b56b`；xsim Verilog Pass，输出逐字节匹配 CPU 验证的独立参考模型。保持全 86400 字缓冲区、padding/tail、非法参数及重启检查。
- 三核 IP 导出、Vivado 综合/布线、SDK ARM 编译与链接完成：`board_build/jobs/20261010_020213_a86e3550`。构建从 02:02:41 到 02:25:37，约 23 分钟。
- `run_board.ps1 -ArtifactsOnly` 返回 `NEXT_BOARD_ARTIFACT_PASS`：冻结输入、PS 编译源文件、bit/HDF/ELF 哈希和 SDK 硬件配对一致。此命令不操作 JTAG。
- 原 fastv2 基线源码、bit、ELF 的 SHA256 与修复前一致；旧失败目录均保留。

## 硬件和实际路由报告

Vivado HLS/Vivado/SDK 2018.3；ALINX ACZ7015，`xc7z015clg485-2`；顶层 `next_system_wrapper`。PS FCLK0 提供 100 MHz / 10 ns 的 PL 时钟。

| 指标 | 路由后实际结果 |
|---|---:|
| WNS / TNS | +0.177 ns / 0 ns |
| WHS / THS | +0.051 ns / 0 ns |
| Setup / hold 失败端点 | 0 / 0 |
| 无时钟引脚 / 未约束内部端点 | 0 / 0 |
| LUT | 20452 / 46200（44.27%） |
| FF | 23621 / 92400（25.56%） |
| BRAM | 61 / 95 个 36K tile（64.21%，等效 122 个 18K） |
| DSP | 16 / 160（10%） |

报告位于通过任务的 `vivado/post_route_timing.rpt`、`post_route_check_timing.rpt`、`post_route_utilization.rpt` 和 `post_route_drc.rpt`。

### 仍须记录的警告

路由 DRC 没有 Error / Critical Warning，但报告列有 44 条 Warning：DSP 输入/输出流水建议、RAMB36 异步控制提示、无可路由负载等。REQP-1839 达到报告条数上限，不能称为“无警告设计”。异步复位相关 RAM 风险尚未通过物理板卡复位/稳定性试验验证；上板先做一帧，再扩大到连续帧，不在处理中主动复位。

BD 创建日志还有对只读派生参数赋值的提示。已核对实际生成 XCI 的复位极性 `C_EXT_RESET_HIGH=0`，与 PS 的低有效 FCLK_RESET0_N 匹配；路由时钟报告确认 100 MHz。未屏蔽这些警告或添加假路径。

## 配对产物

本次固定交付任务：

```text
C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550
```

- [PL bitstream](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550/vivado/next_fpga.runs/impl_1/next_system_wrapper.bit)
- [硬件 HDF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550/vivado/next_system_wrapper.hdf)
- [PS ELF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550/sdk/next_benchmark/Debug/next_benchmark.elf)
- [PS 初始化脚本](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550/sdk/next_hw/ps7_init.tcl)

SHA256：

```text
bit 21FEB85AE76C5AF1329E151A4A1872F50BFF8901DDB231BA8D453CB5D82B11B7
hdf 5F813CCCB2D2B58AE016A65065816A7B7EA0F8410F17159DD3C5D25BD28D6E5E
elf B021C3F802BF73F07B73D115AA426BC122DDC8684F74D181C782B0FE8762F611
```

ELF text/data/bss 分别为 230220 / 1144 / 22576 字节。不要把新 bit 与旧基线 ELF 混用，也不要只下载 ELF 而保留旧 PL 配置。

## 手动上板

先确认实体板卡和器件、JTAG 线缆、供电、启动拨码、复位及外接 I/O 电压兼容，没有连接危险执行机构。关闭其他控制同一 JTAG 的软件会话。脚本会检查 xc7z015 链，复位 PS、初始化 DDR、下载 PL/PS 与测试输入；不会写 Flash、SD 或 eFUSE。板卡 SRAM 配置掉电丢失。

在 PowerShell 中执行：

```powershell
Set-Location 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS'
$nextJob = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550'

# 只检查文件配对，不接触板卡。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $nextJob -ArtifactsOnly

# 确认板卡安全并决定下载后，先执行一帧自检。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $nextJob -Iterations 1

# 一帧 PASS 后，再执行连续稳定性测试。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $nextJob -Iterations 100
# 需要更长测试时改为 -Iterations 1000。
```

验收看 `NEXT_BOARD_STATUS=PASS`，不是 DCC/Console 是否出现字符。PS 将 schema-3 结果写到固定 DDR `0x01000000`，XSCT 自动回读；一帧测试亦检查八层 raw/blur、点、摘要、方向/BRIEF 等参考结果。当前连续测试重复同一归档帧，不是实际视频流。

计时输出分开列出处理平均耗时、含参考比较的墙钟耗时、参考比较、PS 配置、PS 后端和逐层 PL 等待耗时。打印的 100 MHz 等效周期是 PS 观测时间换算值，不是 PL 内置周期计数器实测。只有物理测试返回之后才能评价缓存/AXI 稳定性和真实 FPS。

## 功能边界

PL：resize、FAST/候选筛选、Gaussian。PS：三核配置/调度、缓存维护、方向与 BRIEF、精确自检、DDR 结果及计时发布。

本交付不包含视频解码、相机采集、跨层 B1 打包、匹配或完整 SLAM。当前已通过实体板卡一帧和重复同一归档输入的 100 帧精确自检；未运行 1000 帧或实际视频流，尚未达到 30 FPS。
