# ORB60 4PPC 板测结果

125 MHz / XC7Z015CLG485-2 / Vivado HLS、Vivado、SDK 2018.3。bit 和 ELF 已在 ACZ7015 实际运行。

| 输入 | 模式 | 帧数 | 每帧特征 | 结果 |
|---|---|---:|---:|---|
| normal | Verified | 100 | 280 | EXACT_VERIFIED |
| normal | Production | 100 | 280 | COMPLETE_UNVERIFIED |
| noise | Verified | 10 | 942 | EXACT_VERIFIED |
| zero | Verified | 10 | 0 | EXACT_VERIFIED |

Verified 精确比较八层图像、选点、摘要、描述子、未写区域和 4PPC 标志；Production 使用刚校验的同一配置和 normal 输入，独立结果为 COMPLETE_UNVERIFIED。

| normal 100 帧 Production | 冻结基线 r17 | 本版 |
|---|---:|---:|
| PL 时钟 | 125 MHz | 125 MHz |
| 处理平均时间 | 6.968 ms | 6.306 ms |
| PS 程序循环时间 / 帧 | 6.97965 ms | 6.31751 ms |
| PS 程序循环吞吐 | 143.2737 FPS | 158.2902 FPS |

相对 r17 吞吐变化 +10.48%。两版保持相同 960×720 八层、原始预算和 normal 输入 280 特征；输入合同：direct immutable resident DDR; producer handoff before invocation; external capture/transfer excluded。PS 循环计时含启动前准备、缓存维护、配置、等待和调度，排除 JTAG 下载和外部采集/传输。历史 r16 为 120.4358 FPS、r15 为 110.5721 FPS。r17/r16/r15 原目录保持冻结。

| 层 | 本版图像周期 | 本版描述子周期 |
|---|---:|---:|
| 0 | 233796 | 63484 |
| 1 | 168277 | 44807 |
| 2 | 118960 | 43838 |
| 3 | 82459 | 37921 |
| 4 | 58703 | 29085 |
| 5 | 41188 | 24168 |
| 6 | 29777 | 19230 |
| 7 | 21329 | 17194 |

周期按本版 125 MHz 换算；核内计数不含末尾统计写回，完整处理时间以 PS 计时为准。

最终 routed WNS=0.157 ns，WHS=0.046 ns。实际单时钟周期、setup/hold、内部约束覆盖、全部路由和 Error/Critical Warning DRC 通过。LUT 38875/46200，FF 36102/92400，BRAM 66.5/95，DSP 62/160，Slice 11549/11550。普通 DRC Warning 见 reports/post_route_drc.rpt。

独立 CPU 精确测试、完整 Verilog C/RTL 及实际 IP 随机 AXI 背压、延迟响应、4 KB 边界、复位中断和最终响应完成屏障通过。整机共用 DDR 依据实际板测。

实现：Vivado Performance_Explore integrated implementation。硬件定义：Generated board HDF; exact PS init and BSP configuration pairing。125 MHz 候选使用其硬件定义生成新的 SDK BSP，100 MHz 运行库已核对。PS 基础配置、CPU 计时频率、生成 PS init 及运行库配置均检查；PL ABI 来自配对 IP 头文件。原始配对见 reports/source_pairing.json，包哈希见 manifest.json。

复测：Windows PowerShell 执行 ./run_verified.ps1 -Frame normal -Frames 100，可选 noise/zero。启动器检查哈希、JTAG 线和器件后做易失性 SRAM 下载及精确终态验证。依赖本机隔离的 2018.3 profile。工程 C:\Users\dmsun\Desktop\PLD\orb60_4ppc，实际构建 C:\h\orb4\20261013_185832_board_ed859ec6。
