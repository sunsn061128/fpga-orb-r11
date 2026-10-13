# fastv2 上板验收：2026-10-09

结论：100 帧和 1000 帧均 PASS，固定归档图的七级 resize 像素、八层选中点及全部 64 字节摘要与独立 v1.0 CPU 参考一致。1000 帧前端流水平均 65.250 ms（15.326 FPS），未达到 30 FPS；包含自检的平均总耗时 114.477 ms（8.735 FPS）。此结果不包含完整 ORB 描述子阶段或视频解码。

## 硬件与程序

- 板卡/器件：ALINX ACZ7015，xc7z015clg485-2；JTAG 读得 xc7z015 IDCODE 0373b093，ARM DAP 4ba00477。
- 探针：Digilent JTAG-SMT2，序列号 C306ACCBABCD。
- Vivado/SDK/XSCT：2018.3；PL 时钟配置 100 MHz，PS 应用 Cortex-A9 #0、-O2。
- 使用 bit：`vivado_build_fastv2/fastv2_fpga.runs/impl_1/fastv2_system_wrapper.bit`，SHA-256 `A23EE52051BD1EFE7FEABC198193F319102FEDC2F73EA8C75A5B2EE24186D2DB`。
- 使用 ELF：`sdk_build_fastv2/fastv2_benchmark/Debug/fastv2_benchmark.elf`，SHA-256 `12EEA246D3DCABAB20004CE4A50A8553179C9215E246994D2111C7908E303876`。
- 下载前 HDF、bit、PS C 和参考 include 均与 SDK 导入副本通过 SHA-256 配对检查。
- 复用已完成的实现：100 MHz 路由后 WNS +0.012 ns、TNS 0，未约束端点 0；此次未修改或重新综合/布线 PL。
- 仅复位/初始化 PS 并进行易失 JTAG 下载；未烧写 Flash、SD、eFUSE，未改原基线文件。运行结束已停止 #0 并断开 XSCT，fastv2 易失配置仍保留；断电后不会自动恢复这次下载。

## 测试命令与原始记录

```powershell
pwsh -File fastv2/scripts/run_board_benchmark.ps1 -Iterations 100
pwsh -File fastv2/scripts/run_board_benchmark.ps1 -Iterations 1000
```

原始日志：[100 帧](board_results/board_100_20261009_193647.log)、[1000 帧](board_results/board_1000_20261009_193744.log)。结果经固定 DDR mailbox 回读，不依赖 DCC console。

| 指标（每个960×720输入帧，含八层） | 100 帧平均 | 1000 帧平均 |
|---|---:|---:|
| front 等待累计 | 36.661 ms | 36.800 ms |
| 七次 resize 等待累计 | 64.030 ms | 64.228 ms |
| front PS 配置 | 0.020 ms | 0.020 ms |
| resize PS 配置 | 0.005 ms | 0.005 ms |
| 并行流水（含 PS 配置） | 65.044 ms | 65.250 ms |
| 流水 FPS | 15.374 | 15.326 |
| 验证及输出准备 | 49.526 ms | 49.216 ms |
| 包含验证的墙钟时间 | 114.581 ms | 114.477 ms |
| 墙钟 FPS | 8.727 | 8.735 |

1000 帧流水最小/最大为 64.846/72.236 ms；front 等待累计最小/最大为 36.461/44.220 ms；resize 等待累计最小/最大为 63.833/71.222 ms。1000 帧帧内墙钟总计 114477.458 ms。front 与 resize 并行，两个累计等待时间不能直接相加。

每帧候选数 11078，选中数 280；选中点聚合摘要 `78dac6e0cc89ed75`，图像聚合摘要 `ad12d51bb796d2ec`，两次运行一致。每帧均进行逐像素、逐点及摘要比对，没有只检查首帧。

## 1000 帧分层数据

resize 列按目标层编号，例如 L1 为 L0→L1。

| 层 | front 平均 | resize 平均 | 候选/选中 |
|---|---:|---:|---:|
| L0 | 11.665 ms | 无 | 3873/64 |
| L1 | 8.135 ms | 21.079 ms | 2595/45 |
| L2 | 5.717 ms | 14.759 ms | 1765/44 |
| L3 | 4.012 ms | 10.282 ms | 1128/38 |
| L4 | 2.865 ms | 7.119 ms | 776/29 |
| L5 | 1.976 ms | 5.011 ms | 492/24 |
| L6 | 1.438 ms | 3.492 ms | 296/19 |
| L7 | 0.992 ms | 2.487 ms | 153/17 |

日志中的周期为 PS 观测时间换算的 100 MHz 等效周期，含 AXI/轮询，不是核内独立计数器。首级 resize 为 2107916 等效周期，front L0 为 1166501 等效周期。

## 性能瓶颈与下一步

resize 基本决定当前并行流水耗时；PS 配置仅约 0.025 ms，并非主要瓶颈。front 独立累计也超过 33.333 ms，因此只改善 resize 不能保证达成 30 FPS。

生成 RTL 的直接证据：

- `fastv2/hls_build/fastv2_resize/solution1/syn/verilog/fastv2_resize_hls.v`：源接口内部 `I_ARLEN=1`，目标接口 `I_AWLEN=1`。
- AXI 适配器按 `(tmp_len << USER_ADDR_ALIGN)-1` 计算长度，因此这里的内部值 1 是单个数据项，不是外部 AXI LEN=1 的两拍 burst。
- front 的输入内部请求长度同样固定为 1。源码连续读取/写入不代表 HLS 2018.3 自动生成了长 burst。
- resize HLS 报告中插值、装载循环均为 II=1，但总事务延迟为未知，未计入真实 DDR 握手等待。

据这些证据推断，单拍 AXI 的请求/响应开销是优先优化对象；本次未使用 ILA 或总线性能计数器，尚不能精确分解每类停顿占比。

下一轮优先将 resize 的计算与 DDR 写回解耦，采用独立块缓存/连续写回循环形成 burst，并同时改进输入连续 burst；front 输入也需同类处理。验收要看生成 AXI 请求长度、全尺寸 RTL 验证和物理平均耗时，不能只凭 II=1。随后再考虑 resize 与 front 直接流式衔接，减少金字塔中间 DDR 往返。

此次通过的是同一固定归档图的 1000 次重复压力测试，不代表全部视频帧/阈值配置均已完成上板验收。缓存与 AXI 在本次重复测试中未观察到错误，不能据此保证所有工作负载稳定。
