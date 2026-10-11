# R9/R10 实体板卡 A/B 测试

状态：2026-10-10 用户授权“上板”，R9/R10 单帧与 100 帧 A/B 测试已完成，共 10 次、703 帧；其中 403 帧精确比较 PASS，300 帧 Production 仅运行完成。1000 帧、变化输入视频及处理中复位尚未测试。

## 器件、产物与安全边界

ALINX ACZ7015，`xc7z015clg485-2`，100 MHz，顶层 `next_system_wrapper`。实际 JTAG 链为 Digilent JTAG-SMT2 `C306ACCBABCD`、ARM DAP `4ba00477`、FPGA `0373b093`，与 R8 一致。使用过程隔离的 Xilinx/SDK 2018.3；下载前两套 `run_board.ps1 -ArtifactsOnly` 均 PASS。

R9 固定任务 `board_build/jobs/20261010_080031_ps_6ea0a406`，ELF SHA256 `047C2028EFEA4166C351CD3F7B9D23C268223ED8EA93372B484A4C2FF05A2B24`；R10 固定任务 `board_build/jobs/20261010_082701_ps_554841b3`，ELF SHA256 `EF378C5A6EEC9F761C3A32B25B172F37E027252086FFD11FEBA32B9114F10E85`。

两者均复用 R8 bit/HDF：SHA256 分别为 `21FEB85AE76C5AF1329E151A4A1872F50BFF8901DDB231BA8D453CB5D82B11B7` / `5F813CCCB2D2B58AE016A65065816A7B7EA0F8410F17159DD3C5D25BD28D6E5E`。没有重新综合/布线。沿用原实际路由 WNS +0.177 ns / TNS 0 / WHS +0.051 ns / THS 0，无未约束内部端点；LUT 20452、FF 23621、BRAM36 tile 61、DSP 16。原 DRC 的 44 条 Warning 仍保留，未做处理中复位试验。

设计外部接口只有 DDR/FIXED_IO，无新增 PL 外部执行器。每次测试只复位 PS、初始化 DDR、配置 FPGA SRAM、下载配对 ELF/固定输入并运行；不写 Flash、SD、eFUSE。各次终态由固定 DDR mailbox 回读，脚本停止 ARM #0 后断开连接。

## R9 缓存策略对比

| 模式 / 帧数 | 终态 | 平均处理 ms | FPS | 墙钟 ms | 验证 ms | 最小 / 最大 ms |
|---|---|---:|---:|---:|---:|---:|
| Verified / 1 | PASS | 51.624 | 19.370 | 72.578 | 20.954 | 51.624 / 51.624 |
| Legacy / 100 | PASS 100/100 | 59.769 | 16.731 | 81.735 | 21.966 | 58.228 / 66.268 |
| Verified / 100 | PASS 100/100 | 52.454 | 19.064 | 74.004 | 21.549 | 51.331 / 57.441 |
| Production / 100 | COMPLETE_UNVERIFIED 100/100 | 44.387 | 22.529 | 44.387 | 0 | 43.459 / 50.585 |

| 100 帧模式 | poison 填充 ms | cache ms | PL 等待 ms | PS 后端 ms | 配置 ms | 控制残差 ms |
|---|---:|---:|---:|---:|---:|---:|
| Legacy | 1.487 | 17.756 | 27.344 | 13.129 | 0.027 | 0.027 |
| Verified | 1.425 | 10.550 | 27.538 | 12.888 | 0.027 | 0.027 |
| Production | 0 | 3.979 | 27.427 | 12.939 | 0.026 | 0.017 |

两种精确模式均逐帧检查八层 raw/blur、点、摘要、方向/BRIEF、padding/guard 和未选点区域；每帧候选 11078、特征 280，错误码及 detail 均为 0。通过两次稳定 128-word DDR 快照，100 帧 Legacy 与 Verified 特征摘要均为 `a1967dad68429025`。

同一 R9 ELF 的 Verified 相比 Legacy 平均处理时间减少 7.315 ms（约 12.24%）；缓存维护减少 7.206 ms（约 40.58%），解释了大部分收益。不能将这个比例称为 DDR 带宽提升，或把 poison/比较的删减算成 PL 算法加速。

Production 完成 100 帧，无运行时错误、候选/特征数保持 11078/280，但 `verified_frames=0`、摘要为 0，不是精确 PASS。相较 Verified 另省 8.067 ms，主要来自取消 poison 及其缓存准备；仍保留消费前失效和完整后端运算。这不是新 PL 硬件计算加速。

原先 R8 的约 19.29 ms 未细分开销，现在通过同策略的 R9 Legacy 可见其中 cache 17.756 ms、poison 1.487 ms，配置与残差各 0.027 ms。这支持“缓存维护是高耗时项”，而不再只依据残差猜测。数值是本轮实测，不能回写为旧 R8 的直接分项测量。

## R10 流水对比

单帧 Serial/Pipeline 均精确 PASS，候选/特征数 11078/280、摘要 `a1967dad68429025` 均与 R9 一致。

| 精确模式 / 帧数 | 处理 ms | FPS | 墙钟 ms | 验证 ms | cache ms | 剩余 PL 等待 ms | PS 后端 ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| Serial / 1 | 51.160 | 19.546 | 72.474 | 21.314 | 9.792 | 27.186 | 12.652 |
| Pipeline / 1 | 39.249 | 25.478 | 60.520 | 21.272 | 9.642 | 15.255 | 12.798 |
| Serial / 100 | 52.309 | 19.117 | 74.020 | 21.711 | 10.570 | 27.392 | 12.867 |
| Pipeline / 100 | 40.904 | 24.447 | 62.303 | 21.399 | 10.843 | 15.440 | 13.140 |

流水单帧比串行少 11.911 ms，启动下一层 7 次，收尾错误与 pending mask 均 0。连续 100 帧两模式也均 PASS，摘要相同；流水提前启动 700 次，收尾错误与 pending mask 均 0。Serial 最小/最大 50.992/58.962 ms，Pipeline 39.281/48.153 ms。逐帧图像、点、摘要、方向/BRIEF 和输出边界精确一致。

同一 R10 ELF 的流水平均节省 11.405 ms（约 21.80%），主要来自剩余 PL 阻塞时间减少 11.952 ms；PS 后端时间并没有消失，而是与下一层 PL 重叠。新增 cache/后端等开销部分抵消理论收益；不能将被隐藏的时间当成算法计算本身减少。带 poison 的精确处理平均为 40.904 ms，尚未达到 33.333 ms。

### Production 独立测速

| 100 帧调度 | 终态 | 平均处理/墙钟 ms | FPS | 最小 / 最大 ms | cache ms | 剩余 PL 等待 ms | PS 后端 ms |
|---|---|---:|---:|---:|---:|---:|---:|
| Serial | COMPLETE_UNVERIFIED 100/100 | 44.330 | 22.558 | 43.398 / 52.173 | 3.916 | 27.571 | 12.801 |
| Pipeline | COMPLETE_UNVERIFIED 100/100 | 31.749 | 31.497 | 31.245 / 37.799 | 3.243 | 15.599 | 12.858 |

两模式错误码/detail 均 0、候选/特征均 11078/280，`verified_frames=0`、digest 为 0；流水提前启动 700 次，收尾错误和 pending mask 均 0。流水平均比串行减少 12.581 ms（约 28.38%），其中剩余等待减少 11.972 ms、cache 减少 0.673 ms。Pipeline Production 不再失效未被 PL 改写的固定 raw0 输入，而 Serial Production 仍有该操作，因此不能将全部收益只归因于 PS/PL 重叠。

在本固定 960×720 输入、280 特征的 100 帧测试中，Production 平均 31.749 ms，比 30 FPS 的 33.333 ms 预算低约 1.584 ms。但最大值 37.799 ms 超预算，不证明每帧硬实时达标；Production 无逐帧 CPU 比较，不能称为精确 PASS。精确模式的 100/100 PASS 与生产测速分别成立，不能将两者合并为“生产每帧均已验证”。默认仍为 Serial，未自动替换默认路径。

Pipeline 的逐核时间是延迟观测上界，不按三核总和判断关键路径；正常 100 帧应有 700 次下一层提前启动，收尾错误与 pending mask 均为 0。

## 原始证据与回读工具

R9 任务中的日志：

- `board_run_Verified_Serial_1_20261010_113822_614.log`
- `board_run_Legacy_Serial_100_20261010_114002_826.log`
- `board_mailbox_Legacy_Serial_100_20261010_114201_284.log`
- `board_run_Verified_Serial_100_20261010_114206_613.log`
- `board_mailbox_Verified_Serial_100_20261010_114354_819.log`
- `board_run_Production_Serial_100_20261010_114359_881.log`
- `board_mailbox_Production_Serial_100_20261010_114552_035.log`

R10 任务中的日志与快照：

- `board_run_Verified_Serial_1_20261010_114557_268.log`
- `board_mailbox_Verified_Serial_1_20261010_114642_638.log`
- `board_run_Verified_Pipeline_1_20261010_114648_176.log`
- `board_mailbox_Verified_Pipeline_1_20261010_114801_453.log`
- `board_run_Verified_Serial_100_20261010_114806_966.log`
- `board_mailbox_Verified_Serial_100_20261010_114923_331.log`
- `board_run_Verified_Pipeline_100_20261010_114928_567.log`
- `board_mailbox_Verified_Pipeline_100_20261010_115107_764.log`
- `board_run_Production_Serial_100_20261010_115113_546.log`
- `board_mailbox_Production_Serial_100_20261010_115325_693.log`
- `board_run_Production_Pipeline_100_20261010_115331_031.log`
- `board_mailbox_Production_Pipeline_100_20261010_115741_295.log`

新增作者源 `ps/read_board_mailbox.tcl`：显式检查线缆/器件，仅两次读取 DDR 固定结果区，要求内容一致且是终态；打印完整 128 个字和摘要，不复位、下载、暂停、继续或写内存。纯 Tcl mock 的八个稳定/短读/变化/坏标识/运行中/坏协议/错线缆/错器件案例均通过。原 frozen runner 和 ELF 不修改。

测试后两套 `-ArtifactsOnly` 再次 PASS；原 fastv2 源码/bit/ELF 哈希仍与测试前一致。最后一次 Production Pipeline 的两次 DDR 读取一致，schema 5/status 4/completed 100，字 113=700、115=116=0；ARM #0 保持测试脚本结束时的暂停状态。不写非易失存储，不恢复定时任务。

复现 R10（须重新确认板卡安全并获得上板授权；不会重新综合）：

```powershell
Set-Location 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS'
$r10Job = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_082701_ps_554841b3'
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -ArtifactsOnly
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 100 -Mode Verified -Schedule Pipeline
# 上一项精确 PASS 后才进行生产测速，其终态不是精确 PASS。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 100 -Mode Production -Schedule Pipeline
```

## 覆盖限制

重复同一张 960×720 归档输入及其八层金字塔，不是视频或 100 张不同输入；处理时间不含首次 JTAG 上传、输入参考初始化、帧间 mailbox 发布。精确模式含 poison、必要缓存维护和 PS/PL 工作，只扣除参考比较与哈希；微秒舍入可能带来分项 1～数微秒差异。Production 只能报告 `COMPLETE_UNVERIFIED`，不能等同于逐帧精确 PASS。仅本固定输入生产模式平均超过 30 FPS；尚不能宣称完整视频/SLAM、变化帧输入带宽、1000 帧稳定性、随机 AXI 停顿或所有帧 33.333 ms 内完成。
