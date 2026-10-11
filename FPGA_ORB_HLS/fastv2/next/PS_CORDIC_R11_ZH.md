# R11：CORDIC 无循环除法的 PS 阶段候选

2026-10-10 离线验收完成。**这是优化计划第一阶段的可上板候选，不是全部 R11+ 优化最终交付，也没有实体上板结果。** R10 基线与旧 job 均保留。

## 已通过证据

新任务：`board_build/jobs/20261010_132933_ps_a191d754`，13:29:33～13:31:16（北京时间），PS_ONLY / PASS。主机 CTest 13/13 PASS，16.66 秒；方向函数对照未改 v1.0 CPU 参考的 117266 组输入全部一致；完整 Gaussian/方向/BRIEF 八层 280 点比较及串行/流水 PS 控制、缓存、错误注入检查通过。详细证据为该 job 的 `host.log`、`host_detailed.log`，不是引用先前失败任务的结果。

SDK 2018.3 ARM Cortex-A9 #0，`-O2 -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard` 编译实际冻结源码，链接 `board_benchmark.o` 和 `next_backend.o` 成功。当前作者源与该 job 的冻结 `board_benchmark.c`、`board_pipeline.inc`、`board_contract.h`、`next_backend.c` 哈希一致；现有 ArtifactsOnly 检查同时验证冻结源、SDK 编译源、硬件父任务、SDK 导入 bit/HDF 和交付 bit/HDF/ELF 配对，返回 `NEXT_BOARD_ARTIFACT_PASS`。

R10 冻结 146 文件默认哈希验收 PASS。Gaussian 原偶发主机超时仍保留历史失败记录；四次独立诊断及本次完整回归通过，但不宣称启动/停滞根因已解决。

## ARM 机器码核对

命令使用 `xilinx-2018.3.cmd` 隔离环境调用 SDK 自带 `arm-none-eabi-objdump.exe -d <ELF>`，提取实际链接 ELF 的 `next_angle_q16` 函数。原始对照位于 `optimization_build/r10_angle_arm_20261010.asm.log`、`r11_angle_arm_20261010.asm.log`。

- R10 CORDIC 循环区域 `0x100c18..0x100d94` 有六处静态 `bl __aeabi_ldivmod` 调用点（分支路径不同，不是每轮执行六次）。
- R11 对应循环 `0x100c08..0x100cc4` 没有除法调用，以 `lsr/lsl/orr/mvn` 等指令完成精确移位；不会从此循环跳到余下的除法调用。
- 整个函数的静态 64 位除法调用点由 11 降为 5；剩余五处在循环外的角度归一化/最终取模路径，本轮未修改。
- 这证明本次源码优化实际落入 ARM 指令，不证明每帧快了多少；静态调用点数不是动态执行次数、周期或毫秒。

## 配对文件

以下均以该新 job 为根，不使用自动指向 R8 的默认 `last_board_job.json`：

| 文件 | SHA256 |
|---|---|
| `vivado/next_fpga.runs/impl_1/next_system_wrapper.bit` | `21FEB85AE76C5AF1329E151A4A1872F50BFF8901DDB231BA8D453CB5D82B11B7` |
| `vivado/next_system_wrapper.hdf` | `5F813CCCB2D2B58AE016A65065816A7B7EA0F8410F17159DD3C5D25BD28D6E5E` |
| `sdk/next_benchmark/Debug/next_benchmark.elf` | `A5FC0FA01E8C63D300419DF7BE49E5F02C3EDE9C09FE68D83D6F7C7074D2130E` |

PS 仅改 CORDIC 移位，`next_backend.c` SHA256 `4021FED7B85150165DE30D17C5C363C5C9B6E0232470F8BF4AB384A604DBAC3E`；PL/MMIO/DDR/记录 ABI 未改。bit/HDF **复用** 已接受 R8 父任务 `20261010_020213_a86e3550`，不是本次重新综合布线。

## 复用硬件的报告边界

目标 `xc7z015clg485-2`，top `next_system_wrapper`，FCLK0/PL 100 MHz（10 ns）。四份复制的路由报告与父任务哈希完全相同。WNS +0.177 ns / TNS 0 / WHS +0.051 ns / THS 0；setup/hold 失败端点均 0，无时钟引脚及未约束内部端点均 0。LUT 20452、FF 23621、BRAM36 tile 61、DSP 16、Slice 8077。

DRC 报告列出 44 项 Warning，无 Error/Critical Warning；保留 DSP 流水建议、BRAM 异步控制和无可路由负载警告，REQP-1839 达到 20 条报告上限。时序报告不是 CDC/异步复位安全性的完整证明。原 FAST/resize/Gaussian RTL 11/13/14 事务证据来自相同未改硬件源码的历史接受任务，本轮没有重跑 RTL。

## 复查及下一步

```powershell
Set-Location 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS'
$r11Job = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_132933_ps_a191d754'
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r11Job -ArtifactsOnly
pwsh -NoProfile -File baselines/R10_20261010/verify.ps1
```

这些命令不执行 JTAG。本轮没有物理下载、1000 帧或变化视频测试，没有测量新版本平均 ms/FPS，不能继承 R10 的 31.749 ms。下一定时阶段准备方向矩/CORDIC/BRIEF 分项计时与变化输入精确回归，之后才推进缓存与隔离 PL 搬运重叠试验。定时任务保持活动；全阶段配对交付后才暂停。

## 后续实体测试更新

用户随后明确授权上板，R11 单帧 Serial 及连续 100 帧 Verified Pipeline 精确 PASS，摘要与 R10 一致；Production Pipeline 100 帧为 COMPLETE_UNVERIFIED，平均 32.344 ms/帧。后端平均约 7.3～7.5 ms，比历史 R10 降低约 43%，但整帧没有更快。上述“尚未上板”描述是离线验收时的状态，不作为当前状态；详细原始证据、15 MHz 下载中断/1 MHz 绕开和性能边界见 [BOARD_RESULT_R11_ZH.md](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R11_ZH.md)。R10 基线继续保留，定时任务不因此获得自动上板授权。
