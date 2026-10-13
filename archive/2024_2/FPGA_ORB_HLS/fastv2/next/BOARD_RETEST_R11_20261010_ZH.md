# R11 人工上板复测：两组quiet复测通过，最新生产平均30.862ms

2026-10-10，用户明确要求 R11 上板并确认原 ACZ7015 已上电、连接 JTAG，供电/跳线/外接 I/O 未改变且无危险负载。本次仅 SRAM/DDR 作业，不授权定时任务自动操作硬件，不写 Flash/SD/eFUSE。

## 文件与目标

- 默认 R11：`board_build/jobs/20261010_132933_ps_a191d754`，ELF SHA256 `A5FC0FA01E8C63D300419DF7BE49E5F02C3EDE9C09FE68D83D6F7C7074D2130E`。
- 待补充分项的 Profile：`board_build/jobs/20261010_154719_ps_02a2d8c3`，ELF SHA256 `1C8C503E11F76AC3E304BF8200ED32224120A2217EC0D11F89B08F2E28EF98C5`。本轮尚未下载此固件。
- 两组 `run_board.ps1 -JobRoot <明确路径> -ArtifactsOnly` 均通过；复用 R8 bit/HDF，PL 100 MHz、`xc7z015clg485-2`、`next_system_wrapper`。本轮不重新综合，路由数据仍来自已接受 R8，不是新测时序。
- 实际链路：Digilent JTAG-SMT2 `C306ACCBABCD`、DAP ID `4ba00477`、FPGA ID `0373b093`；隔离 Xilinx/SDK 2018.3。适配器设为 1 MHz，未改变 FPGA 时钟或全局 PATH。
- R10 冻结文件测试前后均 146/146 哈希 PASS；所有旧 job 和历史报告保留。

## 本轮实际结果

| 测试 | 本次结果 | 平均处理 ms | 含验证 wall ms |
|---|---|---:|---:|
| 默认 R11，Verified Serial，1 帧 | CPU 精确 PASS，稳定回读成功 | 45.460 | 66.705 |
| 默认 R11，Verified Pipeline，100 帧 | 参考数据上传失败；算法未启动 | 未测 | 未测 |
| 默认 R11，Production Pipeline，100 帧 | 未执行 | 未测 | 未测 |
| Profile，Production Pipeline，100 帧 | 未执行 | 未测 | 未测 |

单帧候选 11078、选中/完整特征 280，digest `a1967dad68429025` 与历史 R10 一致；验证时间 21.245 ms。cache 9.771 ms、PL wait 27.184 ms、PS backend 6.973 ms、poison 1.478 ms、config 0.026 ms；这是串行 Verified 单帧，不能当生产流水性能。输入仍为归档 960×720、八层，不是变化视频。采用原已验证 quiet helper，首次结果回读延后十秒，不改冻结 runner 或精确验收。

## 阻断证据与停止边界

第二次已完成 bit/ELF、输入图和 raw 参考上传，但 blur 参考上传到 `0x064D8000` 时出现 `Invalid DAP IDCODE / Invalid DAP ACK value: 6`。日志确认此时适配器已是 1 MHz；不能再声称降至 1 MHz 就解决故障，也不能计作算法数值失败或 100 帧 PASS。`con` 尚未执行，未开始这次算法运行。

随后仅运行一次只读 `read_board_link.tcl C306ACCBABCD`：扫描仍见正确 FPGA，但 DAP 报 `AP transaction error, DAP status 30000021`，找不到 ARM Cortex-A9 #0，无法稳定读取 DDR。没有反复重下载、改 FPGA 算法、取消比较或重写非易失存储。runner 错误分支尝试 stop，但当前 ARM 状态无法通过调试口确认；不宣称已经可靠停止所有硬件事务。

隔离 wrapper 的外层返回值在此异常仍为 0；本轮按 `NEXT_BOARD_ERROR`、缺少请求终态及 `NEXT_LINK_READ_ERROR` 判失败，不将外层 exit=0 当通过。

请用户给板卡断电重上电，并重新插好 USB/JTAG 后确认。恢复后先重新进行只读链路检查，再继续未完成的流水精确测试与测速。电源/USB/线缆/信号完整性及 DAP 状态根因尚未定位。

## 原始证据

本轮日志目录：`optimization_build/r11_board_manual_20261010_174409_494`。

- `01_r11_verified_serial_1.log`：真实下载与单帧 PASS。
- `02_r11_verified_serial_snapshot.log`：两次稳定 128-word 结果及 digest。
- `03_r11_verified_pipeline_100.log`：保留失败上传全过程。
- `04_link_after_download_failure.log`：只读 DAP 故障诊断。
- `helpers/`：本轮 helper/decoder 源码副本，复制时已核对 SHA256。

缓存行规划器仍只完成主机实验，尚未接入本轮固件；未运行新版本 1000 帧/变化视频。此报告不替换历史 R11/Profile 数据，不宣称新的 30 FPS 或最终 R11+ 优化交付已完成。

## 用户再次“继续”：链路恢复后的实测结果

下面是随后完成的独立运行，不抹去上面的失败尝试。一次只读快照重新识别出正确线缆、FPGA 和两个 ARM 目标；DDR 报控制器仍在复位，无法读旧结果。此状态不同于此前找不到 ARM 的 DAP 故障，不能视为旧 DDR 数据有效。随后由原冻结 runner 完整执行系统复位、ps7_init、SRAM 配置、ELF/输入/全部参考上传及运行。每组仍使用 1 MHz 适配器和十秒延后回读；三组都完整上传成功，只有一次 quiet 延后标记，首次结果读取在算法完成之后。本轮没有改硬件、固件或算法。

| 版本/模式 | 帧数/终态 | 平均处理 ms | 实际wall ms | 最小/最大处理 ms | FPS |
|---|---|---:|---:|---:|---:|
| 默认 R11，Verified Pipeline | 100/100 CPU精确PASS | 39.586 | 60.586 | 39.015/39.882 | 25.261 |
| 默认 R11，Production Pipeline | 100/100 COMPLETE_UNVERIFIED | 30.861 | 30.861 | 30.846/30.875 | 32.403 |
| R11 Profile，Production Pipeline | 100/100 COMPLETE_UNVERIFIED | 30.859763 | 30.859763 | 30.843032/30.873170 | 约32.404 |

默认 R11 Verified 的 raw/blur、点、摘要、方向/BRIEF、padding/guard/未写尾区逐帧精确通过，稳定双快照 digest 仍为 `a1967dad68429025`。该组 verified_frames=100、700次跨层提前启动，abort_drain_error/pending_core_mask 都为0。Production 两组 verified_frames=0、digest=0、verify/prep=0，不称逐帧CPU一致PASS。默认 R11 未包含 Profile 记录区，不能为其杜撰 P95/P99。

同一归档960×720八层、每帧11078候选/280特征。默认生产组最大值低于1000/30ms，故本组100帧全部满足33.333…ms处理预算；Profile独立稳定快照保留100条真实记录，P95 30.866939ms、P99 30.870548ms，以PS tick整数比较也为0/100超预算。仅建立本固定输入、本次100帧的处理区间结果，不代表变化视频、1000帧长期稳定性、视频端到端或任意帧硬实时保证。平均预算余量约2.47ms，不提前继承给缓存候选或后续新PL。

### 分项，以本次 Profile 为准

| 项目 | 平均ms | 关系 |
|---|---:|---|
| 剩余PL等待 | 20.587019 | 暴露的等待约66.7%，不是三核内部周期之和 |
| PS后端整体 | 6.983474 | 包含下面三个子项 |
| 缓存维护 | 3.241134 | 原整区维护策略，缓存候选尚未集成 |
| 配置 | 0.025953 | 互斥处理分项 |
| 控制残差 | 约0.022 | 主邮箱记录，舍入后总和可能略有差异 |
| 方向矩 | 2.028465 | 后端内部，不重复加到整帧 |
| CORDIC/A32 | 0.580033 | 后端内部 |
| BRIEF | 4.304825 | 后端内部 |

Profile真实BSP timer为333333343Hz，每帧1120次后端读时钟；clock读取循环均值0.050493ms，在原帧区间外，含循环成本且不扣除。默认/Profile平均整帧仅差约1微秒，不能用此单轮、非交错对照精确估计插桩开销或宣称插桩加速。三核同层并行、PS(n)/PL(n+1)重叠；不同视频帧不重叠。Profile前端/resize/Gaussian观测累计26.647/21.567/26.569ms是延迟观测上界，不可相加为整帧周期。

### 新证据与最终状态

仍在原日志目录中新增：

- `05_link_continue.log`：ARM恢复、DDR处于复位的只读快照。
- `06_r11_verified_pipeline_100_after_recovery.log`、`07_r11_verified_pipeline_snapshot.log`：100帧精确PASS及稳定摘要。
- `08_r11_production_pipeline_100.log`、`09_r11_production_pipeline_snapshot.log`：默认固件生产测速及稳定结果。
- `10_profile_production_pipeline_100.log`、`11_profile_production_snapshot.log`：Profile生产测速、主邮箱/header、全部100条原始tick CSV行、百分位与分项，双payload及metadata bookend稳定。

六份新运行/回读日志的请求终态、无错误标记核对通过。首个额外PowerShell计数检查因管道展开每行字段数组而误报record count，改为保留原始CSV行后确认实际100条、index 0..99、0条超预算；未重跑板测、删除异常帧或修改原始记录。reader本身的完整统计/百分位/主邮箱配对检查已在捕获时通过。

两组ArtifactsOnly与R10冻结146/146再次PASS；默认R11 ELF与Profile ELF及复用bit/HDF仍是上面固定配对。PL路由证据继承已接受R8：100MHz，WNS +0.177ns/TNS 0、WHS +0.051ns/THS 0，无未约束内部端点；LUT20452/FF23621/BRAM36等效tile61/DSP16，DRC无Error/Critical Warning、44条Warning保留。本次不重新进行RTL仿真/综合/布线，不能把继承报告说成今天新实现结果。

最后ARM#0由成功的冻结runner执行stop，随后reader只读，未恢复。跨两轮共完成301帧：默认精确101帧、生产200帧；另保留一次未启动算法的上传失败。链路恢复与完整上传成功不证明电源/USB/JTAG根因已根治。本次人工R11上板复测已完成，不再重复下载；缓存/PL优化的有限阶段与最终R11+交付尚未完成，因此不将本次复测完成等同于定时任务最终目标完成。

## 用户“再验证一次”：18:53起的第二组quiet复测

用户再次明确授权人工上板，沿用原已确认的ACZ7015、供电/接线与无危险负载条件，不扩展为定时任务硬件授权。先ArtifactOnly PASS、独占进程检查无其他vendor作业；只读链识别原C306ACCBABCD/xc7z015/ARM目标，DDR当时在复位，不能读取旧结果。随后由原冻结runner完整复位、初始化PS/DDR、重新上传同一bit/ELF/归档图和全部参考，不利用旧DDR数据充当新测试。

仍为默认R11 `20261010_132933_ps_a191d754`，同一ELF SHA256、R8三核100MHz硬件和原CPU参考；没有代码、IP、时钟、算法或验收变化。使用上轮冻结的helpers目录，其quiet/slow/poll SHA256分别为`24CC7DC5AB882E7C8BC00B3E5F7CDC14DD78CEDDE61597C73374204B21D1D439`、`85F24B4C97EE1CE0AED065CD86345E2FDE4FE4D29C0CE40AC5597A5A56080249`、`7A9EC92D1776E23AD286A27B0B562D648EB01BE8FC76C231657ADF33A9AE732A`，与原记录一致。适配器1MHz，两组各只有一次十秒延后回读，首次读数均在算法完成后。

| 本轮模式 | 帧数/终态 | 平均处理ms | wall ms | 最小/最大ms | FPS |
|---|---|---:|---:|---:|---:|
| Verified Pipeline | 100/100 CPU精确PASS | 39.597 | 60.701 | 38.990/39.819 | 25.254 |
| Production Pipeline | 100/100 COMPLETE_UNVERIFIED | 30.862 | 30.862 | 30.849/30.874 | 32.402 |

Verified再度完整图像/点/摘要/方向BRIEF/边界比较PASS，双稳定128-word快照digest仍为`a1967dad68429025`；验证时间21.104ms。本轮两组都是11078候选/280特征、700次跨层提前启动，abort_drain_error/pending_core_mask均为0。Production的verified_frames/digest/verify/prep均为0，不能称其逐帧CPU精确PASS。

生产互斥分项平均：cache3.240ms、剩余PL等待20.605ms、PS后端6.968ms、config0.026ms、控制残差0.022ms。未再下载Profile，故本轮默认固件没有P95/P99或后端三个子区间新记录；上面的Profile数据仍是上一组，不伪称本轮新测。

| 默认R11同条件对照 | 精确平均/wall ms | 生产平均ms | 生产最大ms |
|---|---|---:|---:|
| 第一组quiet复测 | 39.586/60.586 | 30.861 | 30.875 |
| 本轮第二组quiet复测 | 39.597/60.701 | 30.862 | 30.874 |

两组独立100帧生产测试都没有超过33.333…ms预算，平均差约1µs、最大差约1µs，支持固定输入短组结果可重复；不是连续200帧压力测试、变化视频、任意输入或长期硬实时保证。最新平均预算余量约2.471ms。精确与生产组分别成立，不合并为“30.862ms的每帧已经精确验证”。

本轮独立原始记录目录：`C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/optimization_build/r11_repeat_20261010_185304_528`，五份日志为`01_link.log`、`02_verified_pipeline_100.log`、`03_verified_snapshot.log`、`04_production_pipeline_100.log`、`05_production_snapshot.log`。最终run/reader无错误、请求终态、schema5/模式/100完成/提前启动/收尾字段及生产最大值门槛再核对通过。ArtifactOnly再PASS、R10冻结146/146再PASS。

本轮新增200帧实体运行，没有下载失败、算法错误或AXI超时记录。结束ARM#0已由成功runner停止，随后仅只读稳定结果；不写Flash/SD/eFUSE、不修改原基线、不重跑RTL/综合/布线，也不继续扩大物理测试。当前人工重复验证完成，1000帧和变化视频仍待测试，有限离线优化的最终交付尚未完成。
