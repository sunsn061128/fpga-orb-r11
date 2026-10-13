# FPGA ORB 各版本效果汇总

记录日期：2026-10-10（北京时间），包含当天18:53起再次验证的 R11 上板结果。本文是历史结果索引，不是新基线 manifest；R10 冻结登记、所有旧任务、失败日志与原始数据保持不变。初次汇总只整理文件；随后按用户“再验证一次”授权补记新实体测试，没有重新构建。

## 1. 先看结论

- **最新完整 ORB 实测候选：R11 默认固件。** 固定 960×720、八层、280 特征输入，再次100/100帧流水CPU精确PASS；最新独立生产组平均 **30.862 ms/帧、32.402 FPS**，最大 **30.874 ms**，该组100帧均低于33.333…ms预算。上一次同条件平均30.861ms、最大30.875ms；两组结果可重复，不称连续200帧或变化视频达标。
- **固定历史基线仍是 R10。** 生产流水平均31.749ms、31.497FPS；不以本汇总或最新复测替换其哈希和历史记录。R10未用最新quiet观察方式重测，不能把R10/R11差值全部当成算法收益。
- **R11 Profile 是计时版，不是另一个加速算法。** 最新平均30.859763ms，P99 30.870548ms；用于定位耗时。默认/Profile单轮平均接近，不能精确推算全部插桩成本。
- **缓存行候选尚未进入固件。** 独立主机专项和完整17项回归通过，归档输入维护字节量下降约82.9%，但范围调用由16次增到5982次；没有新的ARM毫秒/FPS或新可上板ELF证据。
- 所有上述完整实板测速仍重复同一张归档图，不含相机/解码、跨层B1打包、匹配或SLAM；不是变化视频达标证明。

## 2. 比较口径

1. **完整ORB**：resize、FAST/NMS/网格筛选、Gaussian、方向及BRIEF；next仍未完成视频和跨层B1序列化。
2. **Verified处理时间**：扣除参考比较/哈希等验证区间，但仍含poison测试准备、缓存、配置、PS/PL工作；实际wall包含验证。它不是精简生产程序耗时。
3. **Production**：关闭poison与逐帧参考扫描，保留算法及必要缓存维护；终态为`COMPLETE_UNVERIFIED`，verified_frames=0、digest=0。不能称该测速组逐帧CPU精确PASS。
4. **FPS**为处理区间吞吐，不含首次JTAG上传、输入/参考初始化及帧间发布等开销。表中优先保留原mailbox的FPS；用已经舍入的ms反算可能差末位。
5. **quiet/延后回读**：同一程序开始运行后十秒才读结果。短100帧测试已在第一次读取前完成；改变的是观察方式，不是新算法、新PL或取消正确性验收。
6. 同层三核可以并行；R10/R11还让PS(n)与PL(n+1)重叠。不同视频帧没有重叠。逐核PS观测时间含AXI/延迟观察，不是核内独立周期，不能直接相加。

## 3. 早期实现：功能和工作量不同，不与完整帧直接排名

| 版本线 | 功能/实际实板工作量 | 正确性范围 | 平均处理/吞吐 | 含验证wall | 判断及证据 |
|---|---|---|---|---|---|
| CPU v0.95/v1.0参考包 | 独立算法、规格、归档数据与参考输出 | 根HLS实现已对v1.0的43图/172配置逐字节通过B1、跨层顺序和12段trace | 本汇总没有同口径CPU纯软件实测值 | — | 不是一个已测速FPGA版本；见[根验收记录](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/VERIFICATION.md) |
| 根目录v1.0两核 | PL resize+完整level核，**96×80单层自检** | 实板1000/1000固定样本PASS；262候选/40特征；不能把主机172配置范围套给板测 | 14.895ms，67.136FPS | 17.640ms，56.689FPS | 小样本稳定性结果，不能证明960×720八层30FPS；根RTL cosim没有PASS，见[根验收记录](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/VERIFICATION.md) |
| 原fastv2，100帧 | **仅前端**：960×720八层resize+FAST/NMS/筛选，不含方向/BRIEF | 100/100像素、点、摘要精确PASS；11078候选/280选中 | 65.044ms，15.374FPS | 114.581ms | 原8192候选存储结构；见[原fastv2实板报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/BOARD_RESULT_20261009_ZH.md) |
| 原fastv2，1000帧 | 同一前端/固定输入 | 1000/1000精确PASS | 65.250ms，15.326FPS；最大72.236ms | 114.477ms | resize累计64.228ms为主要瓶颈；前端累计36.800ms，与resize并行，不能相加 |

根目录另保留`vivado_build_packed64`、`vivado_build_packed64_hp01`、`vivado_build_fast_stream`及对应SDK目录等历史试验。目录存在、bit存在或一次工具成功都不足以列为已验收完整帧版本；本次没有找到与上述已发布报告同口径的独立冻结测速/配对验收登记，故不补造它们的ms/FPS，也不删除这些目录。

## 4. 完整next版本的主要实板效果

以下均为同一归档960×720八层输入，11078候选/280完整特征；主要连续组为100帧。R8～R11/Profile均使用100MHz同一R8三核PL，差异主要在PS固件和观察方式。

| 版本/主要变化 | 精确模式处理ms | 含验证wall ms | 精确验收 | 独立生产处理ms / FPS | 生产最大ms | 证据/解释 |
|---|---:|---:|---|---|---:|---|
| R8：三核PL+PS方向/BRIEF，跨层串行 | 59.516 | 81.258 | 100/100 PASS | 未单独测 | — | [R8报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R8_ZH.md)；包含测试准备，不能冒充Production |
| R9：缓存策略，串行 | 52.454 | 74.004 | 100/100 PASS | 44.387 / 22.529 | 50.585 | [R9/R10报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R9_R10_ZH.md)；同ELF Legacy→Verified主要节省cache |
| R10：PS(n)/PL(n+1)跨层流水，固定历史基线 | 40.904 | 62.303 | 100/100 PASS | 31.749 / 31.497 | 37.799 | [基线登记](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/baselines/R10_20261010/README_ZH.md)；平均达预算，最大超预算 |
| R11默认：CORDIC等价移位，历史250ms轮询 | 41.856 | 63.769 | 100/100 PASS | 32.344 / 30.917 | 48.801 | [R11首次报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R11_ZH.md)；PS局部加速，没有证明整帧变快 |
| R11 Profile：独立插桩，历史250ms轮询 | 41.829 | 63.899 | 100/100 PASS | 32.429 / 30.836 | 51.770 | [Profile报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_PROFILE_R11_ZH.md)；完整固件已配对，主机17项通过 |
| 同一Profile，首次quiet观察实验 | 未在该quiet组测 | — | 正确性来自独立Verified组 | 30.864 / 32.400 | 30.876 | 同Profile/输入/硬件，仅改变回读策略；100帧0超预算，不是新算法版本 |
| R11默认，quiet复测1 | 39.586 | 60.586 | 100/100 PASS | 30.861 / 32.403 | 30.875 | [复测记录](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RETEST_R11_20261010_ZH.md)；独立生产100帧0超预算 |
| **R11默认，最新quiet复测2** | **39.597** | **60.701** | **100/100 PASS** | **30.862 / 32.402** | **30.874** | [再次验证](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RETEST_R11_20261010_ZH.md)；18:53起新两组，各100帧；生产0超预算 |
| 同一Profile，最新quiet复测 | 本组未重复Verified | — | 保留历史独立Verified PASS，不伪称本组精确 | 30.859763 / 约32.404 | 30.873170 | [最新复测](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RETEST_R11_20261010_ZH.md)；P95 30.866939/P99 30.870548ms，100帧0超预算 |

### 补充对照：不要只挑一个最快值

| 对照 | 精确处理ms / wall ms | 生产处理ms | 有效判断 |
|---|---|---:|---|
| R9同ELF Legacy Serial，100帧 | 59.769 / 81.735 | — | Legacy→Verified 52.454ms节省7.315ms，cache 17.756→10.550ms；是PS缓存收益，不是PL提频 |
| R10 Serial，100帧 | 52.309 / 74.020 | 44.330 | 同R10流水为40.904/31.749ms；串行/流水两组分别精确PASS |
| R11最新Serial，1帧 | 45.460 / 66.705 | — | 单帧精确PASS，仅用于恢复后自检，不作为100帧均值排名 |

R11优化方向函数在ARM反汇编中将CORDIC循环六处静态64位除法调用点消除，独立方向回归117266组通过；历史Production后端12.858→7.287ms约下降43%。但被隐藏的PL时间会转成等待，不能把后端节省直接全额扣到帧时间。旧R10、历史R11与最新quiet组观察条件不同，不能把表中的所有差值都归因于算法。

## 5. next早期开发阶段和未上板候选

| 阶段 | 实际改动/结果 | 验收与问题 | 可登记的帧性能 |
|---|---|---|---|
| 初始next三核原型 | 主机组合resize、FAST、Gaussian与PS后端 | 历史C/综合任务保留，不作为当前发布源；未单独建立完整板测版本 | 未测 |
| R2 | 固定表/规律扫描的三核结构 | C/综合通过；HLS合计45988 LUT、202 BRAM18K，BRAM超器件190预算；有DATAFLOW形式警告 | 未测，II=1不是FPS |
| R3 | FAST流式归约，删除8192整帧候选限制；收紧网格/整数宽度，Gaussian对称系数 | HLS合计39256 LUT、171 BRAM18K；Gaussian相关形式警告消除，但FAST网格状态警告未解决 | 未测 |
| R4 | FAST网格状态收进同一actor | C/综合通过，RTL FAIL：容量错误码4在顶层返回路径截为0；不是合法图像数值全面失败 | 未测，不可交付 |
| R5 | 独立完成FIFO修复跨DATAFLOW状态返回 | FAST RTL 11事务Verilog PASS，完整状态/点/摘要/未写区检查；包括70398候选密集输入及容量/重启 | 无独立整帧板测；为R8硬件FAST来源 |
| R6 | resize13/Gaussian14全尺寸RTL验收 | 主机通过；原RTL在AXI模型展开失败，1409107页、最坏memcpy估计536870911次；未进入算法比较 | 未测，异常估计不能当实测耗时 |
| R7 | 独立三核Vivado、SDK、DDR邮箱调度准备 | 12项主机/PS控制验收；旧硬件准备遇导出路径和后续报告解析问题 | 无独立已接受板测 |
| R8 | 行搬运字数显式限0..121，导出路径和时序解析修复 | 新resize13/Gaussian14 RTL PASS；AXI模型3/4页，配对bit/HDF/ELF、路由及SDK通过 | 见完整版本表，不把旧FAIL任务改成PASS |
| R11后续阶段2 | 变化输入、独立计时版、统计/P95/P99、稳定DDR reader | 主机完整17项；后端56事务5624特征/32角度bin，计时A/B25600特征；Profile SDK/实板已通过 | 见Profile行；host时间不外推ARM |
| 阶段3A缓存候选 | 32字节行规划；raw方向矩行，blur所有32bin包络；非法/边缘/FULL回退 | `20261010_172307_stage3_8b69fba1`专项PASS、完整17/17 PASS；56事务5624特征、23拒绝检查、旧CPU视图/guard/八层归档通过 | **没有ARM实测、没有接入固件** |
| 后续阶段3B/4/5 | 缓存范围调用成本、Gaussian/FAST搬运计算重叠、最终R11+交付 | 本汇总没有新的已接受SDK/RTL/路由交付证据 | 未测；25～28ms仅目标，不登记为结果 |

R2～R7依据[next历史架构记录](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/README_ZH.md)，R8依据[修复验收记录](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/REPAIR_R8_ZH.md)。开发修订编号不是互相独立的成品板测版本。

缓存候选真实八层280点：维护731232/4280704字节，下降82.9%，5982范围调用（整图16次）。日志总数733472/4282944、5984次另外包含一个33×33边缘FULL样本，不能直接冒充八层真实图计数。规划/调用/barrier成本可能抵消减少的字节；完整Verified仍须全区域维护和精确扫描，不能取消它来证明加速。证据：[专项](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/optimization_build/jobs/20261010_172307_stage3_8b69fba1/focused_test.log)、[完整回归](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/optimization_build/jobs/20261010_172307_stage3_8b69fba1/host.log)。

## 6. 路由资源与时序：按硬件版本，不按PS版本重复算

目标均为ACZ7015、xc7z015clg485-2、PL 100MHz，Vivado HLS/Vivado/SDK 2018.3。

| 硬件线 | LUT | FF | BRAM36等效tile | DSP | WNS/TNS ns | WHS/THS ns | 报告性质 |
|---|---:|---:|---:|---:|---|---|---|
| 根v1.0两核 | 25389 | 28085 | 16 | 88 | +0.201 / 0 | +0.044 / 0 | 已路由；191 DRC Warning、无Error，未约束内部端点0 |
| 原fastv2前端 | 13367 | 16700 | 64 | 11 | +0.012 / 0 | +0.044 / 0 | 已路由；不是next三核资源 |
| R8三核，被R9/R10/R11/Profile复用 | 20452 | 23621 | 61 | 16 | +0.177 / 0 | +0.051 / 0 | 已路由；setup/hold失败0、无时钟/未约束内部端点0、无DRC Error/Critical Warning、44 Warning保留 |
| 缓存候选 | 不新增PL报告 | — | — | — | — | — | 主机PS实验尚未集成，不宣称新实现通过 |

next资源比例为LUT44.27%、FF25.56%、BRAM64.21%、DSP10%。PS-only改动没有降低PL占用；R2/R3/R5的HLS估算与此表Vivado路由值不能直接当同层级测量。时序通过不等于任意AXI停顿/复位/CDC/板卡稳定性全部通过。

直接资源来源：[原fastv2路由利用率](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/vivado_build_fastv2/post_route_utilization.rpt)、[原fastv2时序](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/vivado_build_fastv2/fastv2_fpga.runs/impl_1/fastv2_system_wrapper_timing_summary_routed.rpt)、[R8配对交付](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/DELIVERY_R8_ZH.md)。

## 7. 当前瓶颈，以最新Profile实体读数为准

| 互斥处理项 | 平均ms | 后端内部子项 | 平均ms |
|---|---:|---|---:|
| 剩余PL等待 | 20.587019 | 方向矩 | 2.028465 |
| PS后端整体 | 6.983474 | CORDIC/A32 | 0.580033 |
| 缓存维护 | 3.241134 | BRIEF | 4.304825 |
| 配置 | 0.025953 | 三个子项已含在PS后端，不能再加到整帧 | — |
| 控制残差 | 约0.022 | clock读取循环在原frame区间外，不扣除 | 0.050493 |

因此优先评价PL等待/DDR搬运与计算重叠，其次谨慎评价缓存维护净收益，再考虑BRIEF访存/运算。不是直接宣称这些时间能够全部消除；修改必须保留参考精确性、全缓冲、边界/非法/容量/重启验收。

## 8. 精确任务与下载文件定位

下列是已接受历史job，不依赖可变`last_*.json`指针。链接直接指向配对ELF；相应bit/HDF同在该job。只需离线查验时使用`run_board.ps1 -JobRoot <明确job> -ArtifactsOnly`，本汇总不授权自动下载。

| 版本 | 精确job | ELF |
|---|---|---|
| R8硬件父任务 | `20261010_020213_a86e3550` | [R8 ELF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550/sdk/next_benchmark/Debug/next_benchmark.elf) |
| R9 | `20261010_080031_ps_6ea0a406` | [R9 ELF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_080031_ps_6ea0a406/sdk/next_benchmark/Debug/next_benchmark.elf) |
| 固定R10 | `20261010_082701_ps_554841b3` | [R10 ELF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_082701_ps_554841b3/sdk/next_benchmark/Debug/next_benchmark.elf) |
| 最新实测R11默认 | `20261010_132933_ps_a191d754` | [R11 ELF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_132933_ps_a191d754/sdk/next_benchmark/Debug/next_benchmark.elf) |
| R11 Profile | `20261010_154719_ps_02a2d8c3` | [Profile ELF](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_154719_ps_02a2d8c3/sdk/next_benchmark/Debug/next_benchmark.elf) |

latest默认R11 ELF SHA256：`A5FC0FA01E8C63D300419DF7BE49E5F02C3EDE9C09FE68D83D6F7C7074D2130E`；Profile：`1C8C503E11F76AC3E304BF8200ED32224120A2217EC0D11F89B08F2E28EF98C5`。复用bit SHA256 `21FEB85AE76C5AF1329E151A4A1872F50BFF8901DDB231BA8D453CB5D82B11B7`，HDF `5F813CCCB2D2B58AE016A65065816A7B7EA0F8410F17159DD3C5D25BD28D6E5E`。

第一组quiet复测的100帧默认精确、默认Production及Profile Production原始日志与稳定快照在`C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/optimization_build/r11_board_manual_20261010_174409_494`，具体文件名见[复测报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RETEST_R11_20261010_ZH.md)。同目录保留1MHz下上传失败及恢复记录；后续成功不证明DAP/供电/USB/JTAG异常根因已经根治。

18:53起第二组默认R11精确/生产复测的记录另存`C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/optimization_build/r11_repeat_20261010_185304_528`；[生产运行日志](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/optimization_build/r11_repeat_20261010_185304_528/04_production_pipeline_100.log)、[稳定生产快照](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/optimization_build/r11_repeat_20261010_185304_528/05_production_snapshot.log)。本轮没有新的Profile记录，分项子区间仍引用上一组Profile，不重新标记为本轮实测。

## 9. 尚未证明的事和后续记录规则

- next完整链尚未实板1000帧、变化视频、随机AXI停顿、处理中复位或电源/温度资格测试；原fastv2的1000帧不能迁移成next完整链的覆盖。
- 根实现172配置主机回放不能迁移成next172配置RTL/板测；当前next三核RTL为FAST11、resize13、Gaussian14事务，数值和流程完成成立，不夸大有限FIFO-full/任意复位覆盖。
- 生产测速与精确PASS分别记录；新增源改动不能继承旧ELF/bit的性能。
- 新记录注明日期、job/源码/产物配对、模式/调度、观察方式、图像/特征工作量、帧数、mean/max/P95/P99可用范围，并保留失败记录。未测写“未测”，不填目标值或按主机估算FPS。
- 本文不是`DELIVERY_R11_PLUS_ZH.md`，阶段3/4/最终配对交付未完成；不因本次总结或已完成人工板测就宣称定时优化最终目标完成。

整理采用`fpga-development`技能的证据分层：主机/RTL/路由/SDK/实体结果分开登记，保留源与生成文件边界及已接受基线。
