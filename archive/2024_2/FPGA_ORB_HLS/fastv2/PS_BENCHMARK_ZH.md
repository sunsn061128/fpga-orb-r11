# fastv2 PS 自检与测速

本程序针对 ACZ7015 / `xc7z015clg485-2`，Vivado HLS、Vivado、SDK 均使用 2018.3。PL 为已完成实现的 100 MHz fastv2 工程；PS 程序运行在 Cortex-A9 #0，采用 `-O2`。所有产物均与原 v1.0、fast_stream 基线隔离。

## 验证范围

固定输入为归档 `flight_20261004T184929_998838_6bb93c56_f0000/input.pgm`，960×720 灰度图；FAST 高/低阈值为 20/7，启用双阈值，网格 top-1。

参考数据由独立的 v1.0 `golden.cpp` 生成，不从 PL 输出反推。主机构建自动生成 `ps/fastv2_reference.inc`（摘要、点记录）和 `ps/reference_pyramid.bin`（八层连续像素，2140237 字节）。禁止手工修改这两个生成文件。

每帧验证七次 resize 和八层 FAST/NMS/grid：图像先核对摘要，再与独立参考像素逐字节比对；点记录按参考顺序逐字节比对；每层 64 字节结果的版本、状态、候选数、选中数、网格、tile、fallback 和保留字段全部核对。

| 层 | 尺寸 | 候选数 | 选中数 |
|---|---|---:|---:|
| L0 | 960×720 | 3873 | 64 |
| L1 | 800×600 | 2595 | 45 |
| L2 | 667×500 | 1765 | 44 |
| L3 | 556×417 | 1128 | 38 |
| L4 | 463×347 | 776 | 29 |
| L5 | 386×289 | 492 | 24 |
| L6 | 322×241 | 296 | 19 |
| L7 | 268×201 | 153 | 17 |
| 合计 | 八层 | 11078 | 280 |

本层 front 与下一层 resize 并行启动，两核完成后才切换 ping-pong 图像。PL 写入前，PS 对输出做预填、clean 和 invalidate；完成后再次 invalidate。预填可以检测重复输入时未更新的输出，不能代替真实 AXI/缓存压力测试。

目前只验收 resize、FAST、NMS、grid 前端，尚不包含 Gaussian、方向、BRIEF 或完整 ORB 描述子，也不包含视频解码。

## 离线命令

在 `C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS` 执行：

```powershell
pwsh -File fastv2/scripts/build_and_test.ps1
pwsh -File fastv2/scripts/build_sdk.ps1
& 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd' xsct fastv2/ps/test_mailbox.tcl
pwsh -File fastv2/scripts/run_board_benchmark.ps1 -ArtifactsOnly
```

主机回归含实际 PS 控制/验证代码接实际 HLS C 模型，连续 100 帧，以及 14 类故障注入：错误摘要、错误点、错误 resize 像素、两核超时、两核非零返回、两驱动初始化失败、摘要未写、点未写、resize 未写、损坏参考图和非法 PGM。还检查 1001 次请求回退为 1 次。该测试的驱动、缓存和计时器是模拟的，输出耗时不是 FPGA 性能。

SDK 构建仅重建 `sdk_build_fastv2/`，不会修改原 SDK/Vivado 目录。编译生成：

```text
sdk_build_fastv2/fastv2_benchmark/Debug/fastv2_benchmark.elf
```

`-ArtifactsOnly` 只检查文件与 SHA-256 配对：当前 HDF/bit 必须与 SDK 导入副本一致，当前 C/参考 include 必须与 SDK 源码副本一致；不连接 JTAG。

## 获得上板授权后执行

确认板卡上电、JTAG 接线正确、只有目标 ACZ7015 可被选中，未连接会被输出驱动的危险负载。脚本会复位 PS 并替换当前易失 PL 配置，不烧写 QSPI/SD/eFUSE。

```powershell
pwsh -File fastv2/scripts/run_board_benchmark.ps1 -Iterations 100
# 初测通过后再进行长测：
pwsh -File fastv2/scripts/run_board_benchmark.ps1 -Iterations 1000
```

脚本校验 xc7z015 IDCODE，选择 Cortex-A9 #0，初始化 PS，下载匹配 bit/ELF、输入 PGM 和参考像素，运行后读取 DDR。终态应为 `status=PASS`，`completed=requested`；无需等待 DCC console，也不依赖硬编码断点。

仅查询一次已有结果，不复位、不下载、不暂停 CPU：

```powershell
& 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd' xsct fastv2/ps/check_board_result.tcl
```

## 测速口径

- `front_wait_ms`：八层 front 的 PS 观测等待时间之和，含 FAST、NMS、网格筛选、AXI 和轮询；不是纯 FAST 像素核时间。
- `resize_wait_ms`：七次 resize 的 PS 观测等待时间之和。与 front 重叠，不可直接相加得到流水帧耗时。
- `front_config_ms` / `resize_config_ms`：两核 PS 寄存器配置开销，Start 开销包含在等待区间内。
- `pipeline_ms`：八个阶段累加，每阶段从配置开始计到较晚完成的核；包含 PS 配置、启动和并行等待，排除验证与输出预填/缓存准备。提供平均值、最小值、最大值及 FPS。
- `verification_ms`：每帧图像摘要、逐像素和逐点比较、结果检查，以及输出预填/缓存准备的累计时间。
- `wall_ms`：每帧实际自检墙钟时间，包括验证、缓存和调度等全部帧内开销；不含一次性 PS 初始化、参考校验或 JTAG 下载。
- `*_elapsed_cycles`：PS 计时换算的 100 MHz 等效周期，包含 AXI 与轮询，不是核内硬件计数器。核内精确周期仍需另加计数器并重新实现 PL。

30 FPS 对应平均每帧不超过 33.333 ms。前端处理目标看 `pipeline_ms`，实际此自检程序连续吞吐看 `wall_ms`；不能用离线模拟计时或单核相加/单核 FPS 宣称完整 ORB 达到 30 FPS。

## DDR 回读协议

512 字节 mailbox 地址为 `0x01000000`；command magic=`0x46563243`，result magic=`0x46563252`，schema=2。状态为 0=EMPTY、1=RUNNING、2=PASS、3=FAIL。终态先写回全部数据，再提交状态，避免先看到 PASS 后看到旧摘要。

| 字索引（32 位） | 含义 |
|---|---|
| 0–1 | 命令 magic、请求帧数（1～1000） |
| 2–9 | 结果 magic、schema、状态、错误码、完成帧、失败帧、PS/PL 时钟 |
| 10–14 | 总候选数、选中点摘要高/低字、图像摘要高/低字 |
| 15–24 | front / resize 配置、等待平均/最小/最大耗时及 milli-FPS |
| 25–32 | 流水平均/milli-FPS、验证/墙钟平均、墙钟 milli-FPS、总耗时高/低字、失败 detail |
| 33–40 | 每层候选数 |
| 41–56 | 每层选中点摘要高/低字 |
| 57–64 / 65–72 | 每层 front / resize 平均微秒（resize L0=0） |
| 73 / 74–81 | 总选中数 / 每层选中数 |
| 82–97 / 98–113 | 每层 front / resize 等效周期高/低字 |
| 114–115 | 流水帧耗时最小/最大微秒 |

每层 PL 原始 64 字节结果保留在 `0x04100000 + 64*level`。选中点缓冲区 `0x04000000` 只保留最后处理层；完整参考像素在 `0x06000000`。

错误码：10=PGM，11=参考像素，101/102=resize/front 驱动初始化，201/202=resize/front 超时，203/204=resize/front 返回错误，205=图像不一致，206=64 字节摘要不一致，207=点记录不一致。206/207 的 detail 高 8 位为层号、低 24 位为首个不同的摘要字/点索引；205 为层号及摘要诊断低位或像素索引。

## 当前证据边界

2026-10-09 已完成物理板 100 帧和 1000 帧 PASS，1000 帧前端流水平均 65.250 ms，尚未达到 30 FPS。详细分层数据、产物哈希及瓶颈证据见 [上板验收记录](BOARD_RESULT_20261009_ZH.md)。下述离线/上板证据区别仍然适用。

主机四项回归、离线 mailbox 校验、SDK 2018.3 编译可在不上板情况下确认；真实缓存、AXI 竞争、100～1000 帧稳定性及平均耗时必须由物理板测试确认。SDK 创建空应用时的 MSS 提示是厂商生成流程警告；应用编译应无 C 警告。此次不修改 HLS/RTL，不重跑综合和布线。
