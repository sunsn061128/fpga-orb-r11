# R10：PS 描述子与下一层 PL 并行

状态：2026-10-10 离线验收与 SDK 编译、产物配对 PASS；后续用户授权上板，Serial/Pipeline 单帧及 100 帧精确测试均 PASS，Production 两模式各 100 帧 COMPLETE_UNVERIFIED。结果见 [实体板卡报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R9_R10_ZH.md)。R9 独立 A/B 也已完成，复现命令保留在 [测试清单](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_TEST_PENDING_ZH.md)，不会被本版本替代。

## 修改范围

离线实施只改 PS 调度、结果协议、主机测试和构建/回读脚本；方向/BRIEF 数学、三个 HLS 核和原基线不变。复用 R8 已接受的 `xc7z015clg485-2 / next_system_wrapper / 100 MHz` bit/HDF，不重新综合、布线；后续上板只进行易失 SRAM/DDR 下载测试。

R9 每层完成三个 PL 核后，PS 算完描述子才启动下一层。R10 完成本层 PL、回收缓存后，先启动下一层 PL，再计算本层方向/BRIEF；同一核上一笔完成且 idle 后才允许重新配置。默认仍为 `Serial`，`Pipeline` 必须显式选择；本次上板不修改默认路径。

## 缓冲所有权

| 数据 | 缓冲 | 原因 |
|---|---|---|
| raw | 输入 `0x02800000`；轮转 `0x02400000 / 0x02600000 / 0x02C00000` | 本层 PS 读取、下一层 PL 读取和 resize 写更下一层可能同时存在，两个轮转槽不够 |
| blur | `0x02A00000 / 0x02E00000` | PS 使用本层时，Gaussian 写下一层 |
| 点/摘要 | `0x04000000 / 0x04400000`；`0x04100000 / 0x04500000` | PS 使用本层时，FAST 写下一层 |
| 特征 | `0x04200000` | 只有 PS 写；每层完成比较后才复用 |

额外预留有效缓冲容量约 1.4 MB DDR，地址均分离并按缓存行对齐。Verified 保持完整 poison、padding/guard、未选点区域和字节精确比较；Production 启动前统一清理未知缓存所有权，写者完成后再 invalidate，运行中不向 PL 输出区写 poison。

## 结果与计时

schema 5 保留原字段，增加 `schedule`（字 112）、下一层提前启动次数（113）、PL 延迟观测上界标志（114）、失败收尾错误（115）和尚未确认完成的核掩码（116）。正常流水每帧应有 7 次提前启动，核启动仍为 8/7/8；候选 11078、特征 280 不变。

- Verified：本层 PS 描述子与下一层 PL 重叠，但必须等下一层 PL 完成后才做 CPU 参考扫描。`processing = wall - verification` 不扣除与 PL 重叠的验证时间，缓存、poison、配置、剩余等待和 PS 后端仍计入处理。
- Production：省掉 poison 和逐帧精确比较，仍运行全部 PL/PS 算法，终态只能是 `COMPLETE_UNVERIFIED`，验证帧数和摘要为零。
- `pl_wait_ms` 是 CPU 剩余阻塞时间，不重复累计已与后端重叠的时间。Pipeline 的 front/resize/Gaussian 时间是“启动至 PS 读到完成”的上界，后端繁忙会推迟读完成位；不能据此声称 PL 核变慢或得到真实内部周期。
- PS 后端失败时，先有界收尾已启动的下一层，保留原始后端错误并额外报告收尾错误。PL 超时只报告未完成核掩码，不声称 DMA 已停；不得在此状态下自行覆盖缓冲或自动重跑。

按旧 R8 分层耗时，仅计算相邻层 `min(PS后端[n], PL关键路径[n+1])`，无额外开销时最多可隐藏约 11.94 ms。这个理想模型忽略 DDR 竞争、R9 缓存变化和准备开销，不能预测实板 FPS，也不足以证明 30 FPS。真实收益必须由同一 ELF 的 Serial/Pipeline A/B 测试决定。

## 离线验收与边界

主机使用实际三核 C 模型和实际 PS 后端；延迟完成模型覆盖三种核完成顺序、短/长后端、连续复用 DDR、地址/缓存所有权检查及异常收尾，不是 RTL/AXI/缓存一致性仿真。SDK 必须使用冻结源码新建独立任务，检查 ELF 与原 bit/HDF 的配对。

```powershell
pwsh -NoProfile -File fastv2/next/scripts/build_ps.ps1 -HardwareJobRoot 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550'
```

首次回归在未修改的 Gaussian stream 主机测试无输出停滞，未进入流水测试；只结束所属测试进程，保存日志于 `host_build/r10_first_regression_stall.log`。其后的流水测试通过。新增每项 120 秒 watchdog；第二次完整回归在另一个 Gaussian 主机测试超时，其隔离任务 `20261010_082210_ps_95eddc45` 保持 FAIL，没有绕过验收编译 SDK。

独立诊断在相同 MinGW 14.2 环境下连续三次执行 Gaussian 测试均返回 0，另一次 GDB 运行也正常结束，未复现停滞；原因尚未确认，不宣称已经根治。保留 `host_build/r10_diag_20261010_082611/` 诊断日志。之后新隔离任务 `20261010_082701_ps_554841b3` 完整回归 12/12 PASS（20.18 s），其中 PS 调度测试 14.30 s：原串行 16 帧/30 项异常检查，以及流水 18 帧/28 项异常或非法请求检查；三种完成顺序、DDR 复用、点/摘要/图像/方向/BRIEF、padding/guard 和超时处理均通过。MinGW 的模拟时间不用于报告硬件 FPS。

### 通过的独立产物

任务根目录：`C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_082701_ps_554841b3`，08:27:01～08:29:13 完成；SDK 2018.3 ARM Cortex-A9 编译链接 PASS，text/data/bss = 235708/1144/22576 字节。

| 产物（相对任务根目录） | SHA256 |
|---|---|
| `sdk/next_benchmark/Debug/next_benchmark.elf` | `EF378C5A6EEC9F761C3A32B25B172F37E027252086FFD11FEBA32B9114F10E85` |
| `vivado/next_fpga.runs/impl_1/next_system_wrapper.bit` | `21FEB85AE76C5AF1329E151A4A1872F50BFF8901DDB231BA8D453CB5D82B11B7` |
| `vivado/next_system_wrapper.hdf` | `5F813CCCB2D2B58AE016A65065816A7B7EA0F8410F17159DD3C5D25BD28D6E5E` |

后两项直接复用 R8 硬件。无新综合/布线；沿用父任务真实路由结果：100 MHz，WNS +0.177 ns / TNS 0，WHS +0.051 ns / THS 0，未约束内部端点与无时钟引脚均 0；LUT 20452、FF 23621、BRAM36 tile 61、DSP 16。父任务无 DRC Error/Critical Warning，但保留 44 条 Warning；本 PS 改动没有消除硬件残留警告。

结果解析器通过 25 项拒绝测试；回读脚本纯 mock 通过 schema 3/4/5 的 2/6/10 案例，均无 JTAG。实际任务的 schema-5 runner/helper/流水源与本轮测试文件哈希一致，SDK 中的八份源码与冻结输入配对；不兼容 Pipeline 请求两例在任何硬件操作前拒绝。证据包括任务内 `host.log`、`host_detailed.log`、`sdk.stdout.log`、`sdk.stderr.log`、`runner_mock_test.log` 和 `result_parser_test.log`。

原 fastv2 源码/bit/ELF 和 R9 ELF 哈希未变。默认依然 Serial，不把主机 PASS 或 SDK 链接成功当作新流水的物理板卡 PASS。手动下载、单帧/100 帧的 Serial/Pipeline 复现命令及停止条件见 [测试清单](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_TEST_PENDING_ZH.md)。

## 后续授权实体板卡结果（2026-10-10）

Verified Serial/Pipeline 各 100 帧精确 PASS，摘要均 `a1967dad68429025`，平均处理 52.309/40.904 ms（流水减少约 21.80%）；含参考比较的墙钟为 74.020/62.303 ms。Pipeline 提前启动 700 次、收尾错误/pending mask 均 0，原 bit/HDF/ELF 配对未变。

Production Serial/Pipeline 各 100 帧仅运行完成，平均 44.330/31.749 ms（22.558/31.497 FPS）；流水最小/最大 31.245/37.799 ms。固定归档输入的生产平均满足 30 FPS 预算，但最慢帧超过 33.333 ms，且 Production 不逐帧 CPU 比较。缓存减少也贡献部分收益，不能全部归因为重叠。

本次覆盖单帧及 100 帧固定输入的正常运行；不证明变化视频输入、1000 帧稳定性、随机 AXI 停顿或处理中复位安全，也不是视频解码、跨层 B1 打包或完整 SLAM 程序。离线 Gaussian 偶发停滞仍保留原记录，不因本次上板通过宣称根治。
