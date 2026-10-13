# FPGA ORB 工程交接导航

> 2026-10-10后续补充：[各版本效果汇总](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/VERSION_RESULTS_ZH.md)包含R11默认固件再次100帧精确PASS与最新生产30.862ms/帧、Profile分项和缓存候选边界。下文保留最初R10交接时的状态；R10仍是固定历史基线，后续版本没有覆盖它。

整理时间：2026-10-10。工程根目录 `C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS`，上级工作区 `C:/Users/dmsun/Desktop/PLD`。本次只登记基线和整理导航，不改算法、不删除/移动文件、不综合或上板。

当前应交接的是 **fastv2/next 的 R10：三核 PL + PS 方向/BRIEF + 跨层流水调度**，不是根目录的早期两核工程，也不是原 fastv2 前端工程。

## 先读什么

1. [固定 R10 基线](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/baselines/R10_20261010/README_ZH.md)：准确版本、bit/HDF/ELF 哈希、正确性/性能口径、时序资源和离线校验。
2. [R9/R10 实板报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R9_R10_ZH.md)：100 帧 A/B、原始日志位置、每帧分项耗时。
3. [R10 流水原理](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/PS_PIPELINE_R10_ZH.md)：缓冲所有权、PS(n)/PL(n+1)、异常收尾和计时边界。
4. [R9 缓存优化](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/PS_OPT_R9_ZH.md)、[R8 修复](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/REPAIR_R8_ZH.md)：为什么不能随意删 cache、为什么 resize 搬运长度必须显式限界。
5. [源码及历史架构说明](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/README_ZH.md)、[根 AGENTS.md](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/AGENTS.md)：编译、生成边界和验收约束。

旧文档包含当时的“尚待仿真/上板”描述：以明确日期和版本区分，当前状态以基线及 R9/R10 实板报告为准；不能把旧工程的通过范围套给 next。

## 1. 上级 PLD 工作区

| 文件夹/文件 | 用途与交接要求 |
|---|---|
| `FPGA_ORB_HLS/` | FPGA 实现、主机回归、HLS、Vivado、SDK、板卡测试；当前主工程 |
| `FPGA_ORB_SHARED_v1.0/` | 独立 CPU 参考：`golden.cpp/.hpp`、`constants.hpp`、规格/包格式、profiles、vectors、tests、scripts、evidence；当前构建的必要外部依赖，不是可丢弃附件 |
| `FPGA_ORB_SHARED_v0.95_testdata/` | 较早 CPU 参考及测试数据；用于历史追溯，不是当前默认参考 |
| `程序描述/` | 原始文字及截图材料；不是当前构建源 |
| `教材文档/` | ACZ7015 用户手册、HDL/裸机/Linux/Pynq 等教材及 Vivado/ILA 文档；硬件查阅资料 |
| `裸机SDK例程/` | 厂商 GPIO、定时器、SPI、VDMA、摄像头等示例；不是本 ORB 程序入口，禁止误下载其中例程替代当前 ELF |
| `引脚定义/` | 板卡/扩展模块的 Excel 管脚表；新增外设时核对，当前设计未接入这些外设功能 |
| `DESIGN_SPEC_v1.0.md`、`QUICK_START_FPGA.md` | 工作区层面的规格/快速说明；权威 CPU 包内也有同名文件，不应假定两份永久相同 |
| `参考论文.png`、`ug585-Zynq-7000-TRM.pdf` | 架构参考图与 Zynq TRM；论文效果不能作为本工程实测结果 |
| `聊天记录-供参考.md`、`tmp/`、`vivado_hls.log` | 历史沟通、临时文件及工具痕迹；不作为构建完成或验收证据 |

当前扫描 v1.0/v0.95 的 vectors 分别有 43/15 个一级目录。一个目录可以对应多种配置；不能把目录数量当成 172 配置验收数量，也不能把它们叫作独立训练/验证机器学习数据集。当前板卡用的是 v1.0 `flight_20261004T184929_998838_6bb93c56_f0000/input.pgm`。

## 2. 主工程版本与目录图

```text
FPGA_ORB_HLS/
├─ baselines/R10_20261010/    当前基线登记、哈希与只读校验
├─ HANDOFF_ZH.md             本交接导航
├─ src/include/host/         早期 HLS 算法、接口及主机 CLI
├─ hls/ps/scripts/vivado/    早期仿真、PS 固件与构建入口
├─ build/host_build*/        早期主机生成物
├─ hls_build/               早期 HLS 生成项目
├─ vivado_build*/sdk_build*/ 早期不同实现试验的硬件/软件生成物
├─ fastv2/
│  ├─ src/include/hls/host/ps/scripts/vivado/   原流式前端工程
│  ├─ board_results/host_build/hls_build/      原 fastv2 证据/生成物
│  └─ next/                 当前交接/后续优化的作者源入口
│     ├─ src/include/       三核 HLS 源、接口、生成表
│     ├─ ps/                PS 后端、调度、协议、回读与 SDK 脚本
│     ├─ host/hls/          独立参考生成器、主机测试、RTL 测试台
│     ├─ scripts/vivado/    隔离构建入口、BD 生成器和验收解析器
│     ├─ host_build/        主机二进制、生成参考、测试记录
│     ├─ hls_build/jobs/    冻结输入的 HLS C/综合/RTL 任务
│     └─ board_build/jobs/  冻结输入的三核硬件/SDK/上板任务
└─ .Xil/NA/tmp/*.log/*.jou   工具痕迹与临时材料，不是当前下载入口
```

| 版本线 | 主要功能 | 必须区分的边界 |
|---|---|---|
| 根目录早期实现 | `orb_resize_hls`、`orb_level_hls` 和后来 `orb_fast_stream_hls`；完整数值合同与主机回放路线 | 根 README/VERIFICATION 描述早期实现，包括旧 RTL 适配器问题；不是 R10 的当前状态 |
| 原 `fastv2/` | 流式 FAST/NMS/网格筛选及固定 resize；历史板端前端验收 | 原实现有 8192 候选帧存储；原板端测试不是当前完整方向/BRIEF 流水 |
| 当前 `fastv2/next/` | resize + FAST 前端 + Gaussian 三核；PS 方向/BRIEF、缓存、固定 DDR 回读和计时 | FAST 流式归约取消 8192 帧候选存储限制；R10 加入跨层 PS/PL 流水，不包含视频/SLAM |

根目录的 `vivado_build_packed64*`、`sdk_build_packed64*`、`vivado_build_fast_stream*`、`sdk_build_fast_stream` 等是历史实现变体，`vivado_build_fastv2`/`sdk_build_fastv2` 属于原 fastv2。交接时保留，不凭名称最新或文件时间选择下载产物。

## 3. 当前 next 作者源：按修改目标找文件

下表相对路径以 `FPGA_ORB_HLS/fastv2/next` 为根，公共依赖另注明。

| 路径 | 责任与注意事项 |
|---|---|
| `src/fastv2_hls.cpp` | FAST 响应、tile-local NMS、候选 FIFO、双阈值网格归约/筛选、结果写回；顶层 `fastv2_front_hls` |
| `src/resize_hls.cpp` | 七种固定层间精确 resize，双像素插值、行缓存/打包；顶层 `fastv2_resize_hls`；行搬运保持 0..121 个 64 位字界限 |
| `src/gaussian_hls.cpp` | 精确 Gaussian7、反射边界、DATAFLOW/行缓存；顶层 `fastv2_gaussian_hls` |
| `include/next_stream.hpp`、`include/next_gaussian.hpp` | 三核接口和主机/HLS 流适配等公共定义 |
| `include/next_backend.h`、`ps/next_backend.c` | PS 方向/整数计算/A32 旋转 BRIEF 及特征记录 ABI；数学结果必须与 CPU 参考一致 |
| `ps/board_benchmark.c` | PS 主程序、串行调度、缓存、自检、计时及固定 DDR mailbox 发布 |
| `ps/board_pipeline.inc` | 作者源，非生成文件；R10 单层前瞻流水、三 raw 槽/双 blur 和点/摘要槽、超时收尾 |
| `ps/board_contract.h` | MMIO、DDR 地址、schema 5、错误/模式/调度枚举；修改可能要求重建硬件/SDK 配对 |
| `ps/build_board_sdk.tcl` | SDK 2018.3 裸机编译/链接生成器 |
| `ps/run_board.tcl`、`ps/board_result.tcl` | 实体板下载/运行与严格 mailbox 解析；对应 job 使用冻结版本 |
| `ps/read_board_mailbox.tcl` | 显式线缆/器件检查和两次稳定终态回读，不复位/下载/暂停/继续/写内存 |
| `ps/test_*.tcl` | 解析器、runner 和回读器纯 mock；不操作硬件，不证明 FPS |
| `host/generate_backend.cpp` | 从独立 v1.0 CPU 参考生成后端采样表、完整特征参考及 blur 文件 |
| `host/generate_front_vectors.cpp`、`hls/front_cases.hpp` | 独立 CPU FAST RTL 向量生成器及案例规格 |
| `host/test_front_contract.cpp`、`test_resize_layout.cpp` | 前端合同、密集/容量/回退，resize ROM 数值与布局合同 |
| `host/test_image_reference.cpp` | 图像测试 oracle 与 v1.0 CPU 像素匹配检查 |
| `host/test_complete.cpp` | 三核 C 模型 + 实际 PS 后端的整帧独立参考比较 |
| `host/test_board_benchmark.cpp`、`board_mock.hpp` | 实际 PS 控制源码主机模拟、缓存所有权/调度/故障注入；不是 RTL/AXI/硬件缓存仿真 |
| `hls/tb_front_stream.cpp` | FAST 十一事务完整状态/点/摘要/未写区 RTL 验收 |
| `hls/tb_resize_stream.cpp`、`tb_gaussian_stream.cpp` | 图像核全尺寸 13/14 事务，完整 86400 字缓冲、padding/tail、非法参数及重启 |
| `hls/image_test_reference.hpp` | 独立图像 oracle，不调用 DUT/读取 DUT 轴 ROM |
| `hls/run_hls.tcl`、`export_board_ip.tcl` | HLS 工程/测试与隔离克隆导出；Windows 工程路径处理必须保留 |
| `vivado/build_system.tcl` | 三核 BD、PS/AXI/时钟/复位及硬件生成器，顶层 `next_system_wrapper` |
| `vivado/timing_checks.tcl`、`test_timing_checks.tcl` | Vivado 2018.3 路由验收数量解析与拒绝测试；不是放宽约束 |
| `scripts/*.ps1`、`CMakeLists.txt` | 主机、HLS、三核板卡、PS-only 和下载入口，见后文命令 |
| 公共 `fastv2/include/fastv2.hpp` | 点/摘要及固定层接口；next 仍引用它 |
| 公共 `fastv2/include/fastv2_profile_rom.hpp` | 固定尺寸/grid/tile/resize 轴等 ROM；由 `fastv2/scripts/generate_profile_rom.py` 生成 |
| 根 `vivado/ps_config.tcl` | 已确认 ACZ7015 PS 配置；当前 next BD 仍依赖它 |

## 4. 生成文件与证据，哪些不能手工改

| 对象 | 来源/处理原则 |
|---|---|
| `next/include/backend_tables.h`、`next/ps/full_reference.inc` | 主机生成器输出，不手改；分别含后端预生成表与验收参考 |
| `next/host_build/generated/front_vectors.inc`、`reference_blur.bin` | 独立 CPU 生成；不要从 DUT 实际输出反推“参考” |
| `fastv2/include/fastv2_profile_rom.hpp` | ROM 生成器输出；改冻结 profile 后重生成并全部验证 |
| `fastv2/ps/fastv2_reference.inc`、`reference_pyramid.bin` | 原 fastv2 CPU 参考输出；当前 next 仍依赖 |
| `hls_build/jobs/*/inputs/` | 冻结源/表/测试台，source_manifest 记录哈希；保留作为 RTL 与 IP 来源 |
| `hls_build/jobs/*/projects/` | 工具生成 RTL、综合/IP、RTL 仿真和数据库；不能直接修生成 Verilog 来绕过作者源 |
| `board_build/jobs/*/inputs/` | 冻结 Tcl/PS 源/图像/参考与 HLS manifest；不手改 |
| `board_build/jobs/*/vivado/` | 生成 XPR/BD/IP/runs、bit/HDF 和路由报告；不是作者源入口 |
| `board_build/jobs/*/sdk/` | 生成 BSP、硬件描述、编译源码副本及 ELF；SDK 中改源不等于作者源改了 |
| `status.json` | 每个任务的构建/仿真阶段结果；PASS 必须结合对应报告，不能自动解释为板卡 PASS |
| `artifact_manifest.json`、`input_manifest.json`、`hardware_parent.json` | 产物、源码与硬件父任务配对证据；不得重写成“最新哈希”来消除错误 |
| `board_run_*.log`、`board_mailbox_*.log` | 真实实体板运行、完整稳定 DDR 快照；与主机/mock/构建状态分开 |
| `.Xil/`、一般 `.jou/.log`、`NA/ps7_summary.html`、`tmp/detect_jtag.tcl` | 辅助/工具痕迹；不作为当前验收入口，尚未逐项确认可安全删除 |

“生成文件”不等于“可以全部删除”：当前基线引用的通过 job 必须保留。旧 FAIL/RUN 日志也保留，用来解释修复；本次不做清理。根目录不是现有 Git checkout，本次没有提交/tag，基线由固定 job 和独立哈希清单登记。

## 5. 任务账本：接手时选哪一个

| 类型 / job | 状态与用途 |
|---|---|
| HLS `20261009_201133_all_2c72ead5`、`20261009_205546_all_a2770252`、`20261009_212533_all_7b96c1af` | 历史 C/综合 PASS，不等于当前 RTL/硬件通过；旧 R3 image 默认不匹配 R8 resize |
| HLS `20261009_220729_front_d1884891` | R4 RTL FAIL，顶层状态返回截位，保留原因 |
| HLS `20261009_223114_front_ee8ba228` | 当前 FAST 来源，RTL 11/11 PASS |
| HLS `20261009_232628_image_d8f043b5` | 旧图像 RTL 展开 FAIL，AXI 模型异常内存页，不是算法结果比较失败 |
| HLS `20261010_004058_image_dd67b56b` | 当前图像核来源，resize/Gaussian RTL 13/14 PASS |
| Board `20261010_001714_3283cda1` | 导出阶段 FAIL，旧 Windows HLS 路径问题 |
| Board `20261010_013122_2d747941` | 时序验收解析阶段 FAIL；不能将其存在 bit 当成交付 PASS |
| Board `20261010_020213_a86e3550` | R8 完整三核硬件及旧 PS PASS；R10 的硬件父任务 |
| PS `20261010_080031_ps_6ea0a406` | R9 schema 4 缓存 A/B 构建及实际测试通过；历史对照 |
| PS `20261010_082210_ps_95eddc45` | 主机 Gaussian 测试超时 FAIL，未绕过验收继续编译 |
| PS `20261010_082701_ps_554841b3` | **当前 R10 schema 5，构建 PASS，实体板精确单帧/100 帧 PASS；Production 单独仅运行完成** |

`last_*.json` 是可变“上一次任务”指针，不是发布版本。用户说检查时只查一次；运行中不重复启动、不循环轮询。新综合/布线启动后告知估时即结束当前轮次。失败先看对应 job 日志，一轮做有界修复，原失败目录不覆盖。

## 6. 构建/检查入口

下列均从工程根运行。这里只给命令，不表示本次执行了重建。主机使用既有 MinGW GCC/G++ 14.2、Visual Studio 2022 内置 CMake/Ninja；脚本包含本机绝对路径，换电脑须在新工作副本适配。Vendor 工具使用过程隔离的 `C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd`，不得修改全局 PATH 或依赖 WSL。

```powershell
Set-Location 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS'

# 最先执行：纯文件检查，不操作板卡、不重建。
pwsh -NoProfile -File baselines/R10_20261010/verify.ps1
pwsh -NoProfile -File baselines/R10_20261010/verify.ps1 -CompareWorkingTree
$r10Job = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_082701_ps_554841b3'
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -ArtifactsOnly

# 修改作者源后的主机回归：重建工作目录，可能重生成表，不改冻结 job。
pwsh -NoProfile -File fastv2/next/scripts/build_and_test.ps1

# 仅改 PS 且 PL/MMIO/DDR ABI 没变：新建 PS-only job，不重新综合。
$hardwareJob = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550'
pwsh -NoProfile -File fastv2/next/scripts/build_ps.ps1 -HardwareJobRoot $hardwareJob

# 改 PL：主机通过后新建对应 RTL job，不复用旧输出。
pwsh -NoProfile -File fastv2/next/scripts/synthesize.ps1 -Selection front -Cosim
pwsh -NoProfile -File fastv2/next/scripts/synthesize.ps1 -Selection image -Cosim -TimeoutMinutes 180
```

三核硬件重建必须显式指定已完成且源码匹配的 FAST/Image job；不要使用未修复 resize 的默认旧 image 路径。当前原样重建的来源如下；若 PL 改过，应换成新通过的 job。

```powershell
$frontJob = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/hls_build/jobs/20261009_223114_front_ee8ba228'
$imageJob = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/hls_build/jobs/20261010_004058_image_dd67b56b'
pwsh -NoProfile -File fastv2/next/scripts/build_board.ps1 -FrontJobRoot $frontJob -ImageJobRoot $imageJob -PreflightOnly
pwsh -NoProfile -File fastv2/next/scripts/build_board.ps1 -FrontJobRoot $frontJob -ImageJobRoot $imageJob -Jobs 8
```

SDK 2018.3 是现有 bare-metal 流程；不要直接迁移为新版 Vitis 工程、升级 IP 或替换 simulator。构建成功仍需路由时序、约束覆盖、DRC、SDK、源码/产物配对分别验收。

## 7. 上板与结果解释

只有用户明确授权并重新确认供电、目标、外设安全、独占 JTAG 后才执行。已验收链：JTAG-SMT2 `C306ACCBABCD`、ARM DAP `4ba00477`、xc7z015 `0373b093`。只进行易失 SRAM/DDR 下载，不写 Flash/SD/eFUSE；测试后 runner 停止 ARM #0 并断开，不依赖 DCC Console。

```powershell
# $r10Job 必须是上节明确的 R10 路径，不依赖 last_board_job。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 1 -Mode Verified -Schedule Pipeline
# 单帧 PASS 后扩大到 100 帧。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 100 -Mode Verified -Schedule Pipeline
# 精确模式 PASS 后再独立生产测速；不是正确性验收。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 100 -Mode Production -Schedule Pipeline
```

mailbox 位于 DDR `0x01000000`，128 个 32 位字、512 字节，schema 5。Verified 正常终态 PASS；Production 正常终态 COMPLETE_UNVERIFIED，验证帧数/digest 均 0；返回错误或超时都应 FAIL。PL 超时/pending mask 不为 0 时不等于 DMA 已停，不能自行覆盖缓冲或自动复位重跑。

原 [图形化 Vivado/SDK 指南](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/VIVADO_SDK_GUI_GUIDE_ZH.md) 的原理可参考，但其旧 bit/ELF 路径不是 R10 下载路径。[测试清单](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_TEST_PENDING_ZH.md) 已标记 R9/R10 单帧/100 帧完成，1000 帧尚待授权测试。

## 8. 应交接哪些文件

完整继续开发：保留整个 `FPGA_ORB_HLS` 和同级 `FPGA_ORB_SHARED_v1.0`，最好保持当前目录关系；历史 v0.95、厂商资料和旧基线一并保留便于追溯。不要只交 next/src 或只交一个 bit。

最低限度的基线证据：本导航、`baselines/R10_20261010`、R10/R8/FAST RTL/Image RTL 四个完整 job，以及对应源码、公共 fastv2 头/ROM/参考、CPU v1.0 包；R9 job 用于缓存优化 A/B 追溯，建议一同交接。只有源码副本不能替代固定产物与实测日志。

job 文件内及现有脚本有本机绝对路径，拷到不同位置不能保证直接打开/重建；应保留原快照，在新副本适配路径。`verify.ps1` 的相对哈希检查可验证移动后的文件内容，但不能自动迁移 Vivado/SDK、硬件父路径或工具安装路径。不要在原基线中改路径后重写 manifest 伪称原配对不变。

## 9. 交接后的待办边界

当前平均 31.749 ms 的生产处理时间只比 33.333 ms 预算少约 1.584 ms，最大 37.799 ms 超预算；下一步应基于当前基线评价平均/尾延迟、变化输入与缓存/DDR 开销，不能只追求单核理论周期。

尚未覆盖 R10 1000 帧、变化视频输入、处理中复位及随机 AXI 停顿；未实现视频解码/采集、跨层 B1 打包、匹配、SLAM。MinGW Gaussian 偶发停滞保留原诊断，不宣称根治。新优化应继续精确比较，禁止关闭完整缓冲、非法参数、padding/tail、容量和重启验收。

用户要求作者函数有标准注释；保护原 fastv2/v1.0 和所有旧 job。定时任务保持暂停，不由交接流程恢复。
