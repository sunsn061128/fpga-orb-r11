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
| 处理平均时间 | 11.690 ms | 8.292 ms |
| 程序循环总时间 / 帧 | 14.39856 ms | 8.30318 ms |
| 程序循环吞吐 | 69.45 FPS | 120.44 FPS |

处理速度为原版的 1.410 倍，程序循环吞吐为原版的 1.734 倍。处理时间从首层核启动计时，含后续缓存维护、寄存器配置、等待和调度；程序循环总时间还包含启动前准备，不含 JTAG 下载及外部采集/传输。本版输入合同：direct immutable resident DDR; producer handoff before invocation; external capture/transfer excluded。两版同时存在 PPC、工具版本和实现变化，因此该比较不能单独归因于 PPC。

Production 的处理时间最小 8.279 ms，最大 8.308 ms，100 帧程序循环总时间 830.318 ms。下表为核内实际计数器的每帧平均周期，未包含末尾统计写回；完整处理时间以 PS 计时为准。

同为 2018.3 / 100 MHz / normal 八层 280 特征、100 帧 Production 的已验收阶段：

| 阶段 | 处理平均时间 | 程序循环时间 / 帧 | 程序循环吞吐 |
|---|---:|---:|---:|
| 原 4PPC | 9.422 ms | 12.11829 ms | 82.52 FPS |
| 4PPC + PS 直接 DDR 输入 | 9.419 ms | 9.43018 ms | 106.04 FPS |
| 冻结基线 r15 | 9.032 ms | 9.04387 ms | 110.57 FPS |
| 本版 | 8.292 ms | 8.30318 ms | 120.44 FPS |

本版相对冻结基线 r15 的程序循环吞吐变化为 +8.92%。两版使用相同 2018.3 工具、100 MHz 时钟、常驻 DDR 输入和原始特征预算。r15 保持冻结，验收记录副本见 reports/r15_acceptance.json。

本版相对原 4PPC 的程序循环吞吐变化为 +45.95%，相对 PS 直接 DDR 版本为 +13.57%。历史验收记录及哈希已核对，副本保留于 reports/*_acceptance.json。后两版采用相同的常驻、不可变 DDR 输入合同；原 4PPC 的每帧准备仍包含 CPU 输入复制。

| 层 | 2PPC 图像周期 | 4PPC 图像周期 | 原版描述子周期 | 本版描述子周期 |
|---|---:|---:|---:|---:|
| 0 | 355978 | 228688 | 123215 | 126176 |
| 1 | 248937 | 169779 | 86700 | 90363 |
| 2 | 174319 | 119544 | 83780 | 76556 |
| 3 | 122188 | 84789 | 72559 | 65530 |
| 4 | 85653 | 57127 | 54615 | 50403 |
| 5 | 60110 | 42003 | 44829 | 39652 |
| 6 | 42479 | 29076 | 34599 | 30333 |
| 7 | 30073 | 21079 | 29243 | 24817 |

核内周期与整帧计时口径不同，整帧包含缓存维护、配置、等待和调度。数值一致已由独立仿真及板测确认；版本迁移不保证相同资源、延迟或物理实现。

最终 routed WNS=0.076 ns，WHS=0.018 ns；无未约束内部路径、路由错误及 Error/Critical Warning DRC。LUT 42365/46200，FF 38099/92400，BRAM 66/95，DSP 62/160，Slice 11547/11550。Slice 利用率很高，后续加逻辑需要重新检查装箱及路由。

普通 DRC Warning 的具体数量、类别和路径保留于 reports/post_route_drc.rpt；接受门槛拒绝 Error/Critical Warning。setup/hold 通过不表示异步复位已被 STA 覆盖；独立 HLS 核的复位中断测试通过，整机板测采用初始化后的固定配置，未测试在运行中复位整个 PS/AXI 互联。

验证还包括独立 CPU 精确测试、完整 Verilog C/RTL 联合仿真（图像 21 用例、描述子 30 用例 / 1222 特征）及真实 IP 的随机 AXI 背压、延迟写响应、4 KB 边界、复位中断和最终响应完成屏障。AXI 仿真是独立核测试；整机共用 DDR 的验证依据是这里的实际板测。

实际接受实现：opt ExploreWithRemap; control_set_merge; place Default; physopt Explore; route Explore; place -no_fanout_opt; CARRY_REMAP=2。详细构建来源见 reports/source_status.json。 2018.3 IP 日期 revision 溢出仅在生成目录内重打包修复。复用的 PS standalone 库经过 876 项 PS 配置比较；相应工程的 PS init 与该库对应配置逐字节一致。PL 寄存器 ABI 来自与 bit 配对的 IP 导出头文件。硬件定义来源：Inherited source PS configuration; no new HDF claimed。

源码工程：`C:\Users\dmsun\Desktop\PLD\orb60_4ppc`。实际接受构建：`C:\h\orb4\20261013_083117_route_f6928ddc`。`reports/source_pairing.json` 保存原始绝对路径来源；本包内容校验使用 `manifest.json` 的相对路径 SHA-256。

此构建的失败板测会话如有发生，保留于 reports/failed_sessions；仅 SUCCEEDED 的完整物理校验作为接受依据。

复测：在 Windows PowerShell 中运行 `./run_verified.ps1 -Frame normal -Frames 100`（也可选 noise/zero）。启动器会检查包内哈希、指定 JTAG 线和器件，执行易失性 SRAM 下载并检查稳定终态。依赖本机隔离的 2018.3 profile；无需新 HDF。不写 Flash、SD 或 eFUSE。
