# ORB60 4PPC 板测结果

100 MHz / XC7Z015CLG485-2 / Vivado HLS、Vivado、SDK 2018.3。bit 和 ELF 已在 ACZ7015 上实际运行。

| 输入 | 模式 | 完成帧数 | 每帧特征数 | 结果 |
|---|---|---:|---:|---|
| normal | Verified | 100 | 280 | EXACT_VERIFIED |
| normal | Production | 100 | 280 | COMPLETE_UNVERIFIED |
| noise | Verified | 10 | 942 | EXACT_VERIFIED |
| zero | Verified | 10 | 0 | EXACT_VERIFIED |

Verified 逐字节比较八层图像、选点、摘要、描述子和未写区域，同时检查新核的 4PPC 标志。Production 测试沿用刚通过校验的硬件配置及 normal 输入，独立结果为 COMPLETE_UNVERIFIED。

| normal，100 帧 Production | 原 2PPC / 2024.2 bit | 本版 4PPC / 2018.3 bit |
|---|---:|---:|
| 处理平均时间 | 11.690 ms | 9.422 ms |
| 程序循环总时间 / 帧 | 14.39856 ms | 12.11829 ms |
| 程序循环吞吐 | 69.45 FPS | 82.52 FPS |

处理速度为原版的 1.241 倍，程序循环吞吐为原版的 1.188 倍。处理时间从首层核启动计时，含后续缓存维护、寄存器配置、等待和调度；不含最初输入复制及初始准备。程序循环总时间包含这些准备，不含 JTAG 下载。两版同时存在 PPC 和工具版本变化，因此该比较不能单独归因于 PPC。

Production 的处理时间最小 9.413 ms，最大 9.430 ms，100 帧程序循环总时间 1211.829 ms。下表为核内实际计数器的每帧平均周期，未包含末尾统计写回；完整处理时间以 PS 计时为准。

| 层 | 2PPC 图像周期 | 4PPC 图像周期 | 原版描述子周期 | 本版描述子周期 |
|---|---:|---:|---:|---:|
| 0 | 355978 | 228644 | 123215 | 169872 |
| 1 | 248937 | 162878 | 86700 | 119376 |
| 2 | 174319 | 113857 | 83780 | 109911 |
| 3 | 122188 | 78619 | 72559 | 92298 |
| 4 | 85653 | 54895 | 54615 | 69945 |
| 5 | 60110 | 39213 | 44829 | 56919 |
| 6 | 42479 | 28505 | 34599 | 44622 |
| 7 | 30073 | 21047 | 29243 | 38734 |

图像核周期已明显减少，本次 2018.3 描述子实现的周期比原版增加，且整帧仍包含 CPU 缓存维护和调度，因此整帧提升未达到两倍。数值一致已由独立仿真及板测确认；版本迁移不保证相同资源、延迟或物理实现。

最终 routed WNS=0.123 ns，WHS=0.039 ns；无未约束内部路径、路由错误及 Error/Critical Warning DRC。LUT 42337/46200，FF 37808/92400，BRAM 64/95，DSP 61/160，Slice 11550/11550。Slice 利用率很高，后续加逻辑需要重新检查装箱及路由。

DRC 报告列出 109 个普通 Warning：DSP 输入/输出流水级建议（DPIP-1、DPOP-1/2），厂商 AXI/FIFO 相关 RAMB36 异步控制检查（REQP-1839），无可路由负载（RTSTAT-10），以及 REQP-1839 报告条数达到 20 的提示（CHECK-3）。setup/hold 通过不表示异步复位已被 STA 覆盖；独立 HLS 核的复位中断测试通过，整机板测采用初始化后的固定配置，未测试在运行中复位整个 PS/AXI 互联。详细路径保留于 reports/post_route_drc.rpt。

验证还包括独立 CPU 精确测试、完整 Verilog C/RTL 联合仿真（图像 21 用例、描述子 30 用例 / 1222 特征）及真实 IP 的随机 AXI 背压、延迟写响应、4 KB 边界、复位中断和最终响应完成屏障。AXI 仿真是独立核测试；整机共用 DDR 的验证依据是这里的实际板测。

物理实现采用相同已综合链接网表，CARRY_REMAP=2、ExploreWithRemap、control_set_merge、place -no_fanout_opt、phys_opt Explore、route Explore。此前三个任务因 Slice 装箱失败；它们的结果不作为接受依据。2018.3 IP 日期 revision 溢出仅在生成目录内重打包修复。复用的 PS standalone 库经过 876 项 PS 配置比较；新工程生成的 PS init 与该库对应配置逐字节一致。此包没有新的 HDF，PL 寄存器 ABI 来自新 IP 导出头文件。

源码工程：`C:\Users\dmsun\Desktop\PLD\orb60_4ppc`。实际接受构建：`C:\h\orb4\20261012_214837_route_1eda9d67`。`reports/source_pairing.json` 保存原始绝对路径来源；本包内容校验使用 `manifest.json` 的相对路径 SHA-256。

全零输入的初次参考数据下载曾发生 DAP ACK/IDCODE 错误，应用尚未运行；TAP 复位和 2018.3/2020.2 DAP 调试复位未恢复。用户给板卡断电重启后，重新下载同一 2018.3 bit/ELF 完成全零校验。失败会话及恢复日志保留于 reports/failed_sessions；2020.2 只用于这次失败的调试接口恢复尝试，没有参与本版综合、路由、ELF 编译或成功板测。

复测：在 Windows PowerShell 中运行 `./run_verified.ps1 -Frame normal -Frames 100`（也可选 noise/zero）。启动器会检查包内哈希、指定 JTAG 线和器件，执行易失性 SRAM 下载并检查稳定终态。依赖本机隔离的 2018.3 profile；无需新 HDF。不写 Flash、SD 或 eFUSE。
