# R9：PS 分项计时和缓存维护优化

2026-10-10，北京时间。按 `fpga-development` 技能实施缓存所有权、精确验证与产物配对检查。初始离线实施只修改 PS 调度/计时及构建、测试脚本，不修改 PL 算法或方向/BRIEF 数值计算；后续授权实体板卡测试结果见文末。

## 已完成的优化

完整精确模式 `Verified` 保留每帧八层 raw/blur、点、摘要、方向/BRIEF 比较，以及 poison、guard、padding 和未使用点区检查。没有为了提速关闭精确验收。

基于实际 standalone_v6_8 BSP 的 `xil_cache.c`，`Xil_DCacheFlushRange` 会写回脏行并失效 L1/L2，最后执行屏障，因此删除它后面的重复 invalidate。输入图像在初次拷贝时 flush 一次；后续 raw 金字塔由 PL 写入，PS 只读，不再每层重复 flush。精确模式中 resize 输出已在上一层读取 guard 前失效，下一层不再次失效同一 raw 输入。PL 写出的 blur、点、摘要及精确模式的 resize 输出仍保留写后失效。

这些规则只适用于当前隔离的、缓存行对齐的工作区及“PS 不修改 raw/PL 输出”的契约；未来加入输入更新、CPU 图像修改或跨层流水时必须重新分析所有权，不能照搬。

## 三种模式

| 模式 | 缓存策略 / 自检 | 终态 |
|---|---|---|
| `Legacy` | 保留旧缓存策略，新增计时；逐帧完整精确自检 | `PASS` / `FAIL` |
| `Verified`（新 ELF 默认） | 减少重复缓存操作；逐帧完整精确自检 | `PASS` / `FAIL` |
| `Production`（必须显式选择） | 无 poison 填充、逐帧参考比较、guard 检查及输出哈希；仍运行三核和全部方向/BRIEF | `COMPLETE_UNVERIFIED` / `FAIL` |

Production 初次进入时，在计时循环外清理未知的输出缓存行，此后 PS 只读 PL 输出，并在 CPU 使用前失效。它保留核心返回值、超时、选中点数量范围及后端运行错误检查，但**不能作为数据正确性验收**；`verified_frames=0`，输出摘要哈希为 0。生产 FPS 与带 poison 的 Verified FPS 必须分别标注，不能把删掉测试准备说成硬件计算加速。

## 分项计时

固定 DDR `0x01000000` 的 512 字节结果区升级为 schema 4，原有结果索引不变。

| 新字段（32 位字索引） | 含义 |
|---|---|
| 26 / 27 | 请求及返回模式 / 实际精确验证帧数 |
| 28 | 每帧输出 poison 填充平均微秒 |
| 29 | 每帧帧内缓存维护平均微秒 |
| 30 | 每帧各层三核并行启动到全部完成的等待总时间 |
| 31 | 处理时间减去已统计部分后的控制与计时残差 |
| 88..95 / 96..103 / 104..111 | 逐层填充 / 缓存 / 并行等待微秒 |

处理耗时仍含必要缓存维护、配置、PL 等待、PS 后端；Verified 还含 poison 准备，只扣除参考比较、guard 检查及结果哈希。初次输入验证、Production 初始清理、JTAG 上传与帧间 mailbox 发布不在帧处理区间内。分项读计时器本身存在开销，归入测量区间或残差；换算后分项之和可能有数微秒舍入差异。

`pl_wait_ms` 是三核实际等待关键路径，不是三个核时间相加。逐核等效周期依然来自 PS 观察，并非 PL 内置计数器。方向与 BRIEF 暂时仍合并为 `backend_ms`；尚未实现跨层 PS/PL 流水或新增多像素核。

## 验证证据

- 主机 12/12 回归 PASS，总测试约 12.78 秒，MinGW GCC 14.2.0；使用真实 PS 源码、HLS C 模型及独立 CPU 参考。
- 三模式正常路径共 16 帧，包括重复运行、非法迭代数回退及无命令时默认精确模式。主机独立观察器检查 Production 每层特征，确保没有漏算后端；这不向生产固件加入参考扫描。
- 30 个注入失败被正确捕获：像素、点、摘要、描述子、guard、padding、无图像写入、未使用点区写入、非法数量、返回错误、超时、非法模式及坏输入。特意向 Production 注入描述子错误时，结果只能是未验证完成，不能发布 PASS。
- 同一帧缓存 API 请求范围累计量：Legacy 15846720 字节，Verified 7940992 字节，减少约 49.9%。这是 API 扫描范围，不是实测 DDR 传输量、缓存命中率或 FPS。
- 纯 Tcl 结果检查器通过 16 个拒绝案例；运行脚本 mock 对 schema 3/4 的 2/6 个案例通过，检查模式命令写入和终态解码，无 JTAG。
- 首次 MinGW 编译曾出现子进程启动失败；同版本直接语法检查及随后正常构建均通过，未替换工具或放宽验收。

## 独立可下载产物

PS-only 任务在 08:00:31～08:02:47 完成，状态 `PASS` 表示**构建和文件配对通过，不是物理板卡 PASS**：

```text
C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_080031_ps_6ea0a406
```

- [新 ELF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_080031_ps_6ea0a406/sdk/next_benchmark/Debug/next_benchmark.elf)，SHA256 `047C2028EFEA4166C351CD3F7B9D23C268223ED8EA93372B484A4C2FF05A2B24`。
- [配对 bit](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_080031_ps_6ea0a406/vivado/next_fpga.runs/impl_1/next_system_wrapper.bit)，SHA256 `21FEB85AE76C5AF1329E151A4A1872F50BFF8901DDB231BA8D453CB5D82B11B7`。
- [配对 HDF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_080031_ps_6ea0a406/vivado/next_system_wrapper.hdf)，SHA256 `5F813CCCB2D2B58AE016A65065816A7B7EA0F8410F17159DD3C5D25BD28D6E5E`。

SDK 2018.3，Cortex-A9 #0，`-O2`，ELF text/data/bss = 231940 / 1144 / 22576 字节。SDK 链接、编译源码快照、硬件及 bit 配对检查通过。bit/HDF 从已上板通过的 R8 任务复制且哈希不变；没有重新综合、布线或改变 100 MHz、`xc7z015clg485-2`。继承原路由 WNS +0.177 ns / TNS 0 / WHS +0.051 ns、LUT 20452 / FF 23621 / BRAM36K tile 61 / DSP 16，以及原 DRC Warning；不是新的路由结果。

原 fastv2 源码、bit、ELF 哈希与 R8 前一致。旧任务和 `last_board_job.json` 均保留；新任务记录在 `last_ps_job.json`，下载必须显式指定新路径。

## 复现与下一步

```powershell
Set-Location 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS'
$hardwareJob = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550'
pwsh -NoProfile -File fastv2/next/scripts/build_ps.ps1 -HardwareJobRoot $hardwareJob

$psJob = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_080031_ps_6ea0a406'
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $psJob -ArtifactsOnly
# 以下仅在用户授权实体板卡测试、安全核验后执行。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $psJob -Iterations 1 -Mode Verified
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $psJob -Iterations 100 -Mode Verified
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $psJob -Iterations 100 -Mode Legacy
# 只有精确模式通过后，再独立测生产处理时间；其完成状态不是精确 PASS。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $psJob -Iterations 100 -Mode Production
```

初始离线实施未接触 JTAG，其主机结果不能报告新性能或已达 30 FPS。后续已实施并测试 R10 PS 后端与下一层 PL 重叠，保留本 R9 作为独立 A/B 基线。

## 后续授权实体板卡结果（2026-10-10）

单帧 Verified 和 100 帧 Legacy/Verified 均精确 PASS，候选/特征 11078/280，100 帧摘要均 `a1967dad68429025`。Legacy/Verified 平均处理 59.769/52.454 ms，减少 7.315 ms（约 12.24%）；缓存维护 17.756/10.550 ms，减少 7.206 ms。Production 100 帧平均 44.387 ms、22.529 FPS，无运行时错误但不逐帧精确比较，终态 COMPLETE_UNVERIFIED。测试后文件配对和原基线哈希再次核对通过。

这是真实板卡数据，不是主机 mock；完整原始日志、R10 对比及未覆盖范围见 [实体板卡报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R9_R10_ZH.md)。
