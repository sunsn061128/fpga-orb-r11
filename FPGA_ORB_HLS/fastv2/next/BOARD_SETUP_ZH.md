# 新版三核 + PS ORB 上板工程

本工程只生成独立硬件与软件产物，不覆盖原 fastv2 的源码、bit、ELF、Vivado 或 SDK 目录。不会自动操作 JTAG 或写入 QSPI/SD。物理下载必须在核对构建结果、当前板卡连接后执行。

## 本轮接入范围

PL：双像素 resize、单像素 FAST/NMS/网格筛选、流式 Gaussian7。

PS：八层调度、缓存维护、整数方向计算、查表旋转 A32 BRIEF、逐字节验证及 DDR 结果邮箱。每层同时启动 FAST、Gaussian 和下一层 resize；等待三者都完成后处理描述子，之后才复用原图缓冲区。没有声称 PS 与 PL 跨层全重叠。

这是整帧特征提取与自检工程，不包含视频解码、摄像头接入、特征匹配、跨层 B1 包序列化或 SLAM。输入仍是一个归档 960×720 灰度帧，连续重复 1～1000 次；不能据此宣称整段视频或全部 172 配置已验收。

## 构建命令

```powershell
pwsh -File fastv2/next/scripts/build_and_test.ps1
pwsh -File fastv2/next/scripts/build_board.ps1 -PreflightOnly
pwsh -File fastv2/next/scripts/build_board.ps1 -Jobs 8
```

入口默认隐藏后台运行并返回 PID。输出放在 `fastv2/next/board_build/jobs/<时间_编号>/`，最新入口为 `board_build/last_board_job.json`，与正在运行的 `hls_build/last_job.json` 分开记录。每次只新建目录，不删除或复用已有构建。

流水线先克隆已完成的 HLS 工程，再从克隆导出 IP、创建三核 Vivado 工程、综合布线、生成 bit/HDF，最后用 SDK 2018.3 构建 Cortex-A9 的 ELF。克隆时核对当前源码、公共 ABI、profile ROM 与已综合任务的 SHA-256，核对器件和 10 ns 时钟，并直接从生成的 AXI-Lite RTL 检查 PS 使用的寄存器偏移。

当前默认 FAST 来自 R5 任务 `20261009_223114_front_ee8ba228`，resize/Gaussian 来自源码未变的 R3 任务 `20261009_212533_all_7b96c1af`。不会导出或修改正在进行 R6 RTL 协同仿真的工程。图像核的全尺寸 RTL 验收仍独立进行。

使用进程隔离的 Xilinx 2018.3 profile；Vivado 并行 jobs=8，`general.maxThreads=8`，不表示每一步始终占满八核。本机为 i7-1360P、12 个物理核心/16 个逻辑处理器、约 32 GiB 内存。综合/布线实际占用和速度取决于工具阶段。watchdog 默认 360 分钟，只终止本任务创建的工具进程树。

IP 导出只允许已知 Vivado 2018.3 的日期 revision 整数溢出兼容修补，并且仅修改克隆的生成 `run_ippack.tcl`。其他导出错误不伪造成功。Vivado 必须完成布线，setup/hold slack 非负、`no_clock` 和 `unconstrained_internal_endpoints` 为零、没有 Error/Critical Warning 级 DRC，才能继续 SDK；无法识别时序覆盖报告格式也会拒绝自动验收。通过这些门槛仍不表示板端 PASS。

## 数据路径及地址

所有 PL/AXI fabric 使用 PS FCLK0 的 100 MHz 和同步复位；DDR/FIXED_IO 使用原已验证的 PS 配置，不引入新的 PL 外部接口。

| 路径 | 分配 |
|---|---|
| resize 读/写 DDR | HP0 / HP1 |
| FAST + Gaussian 读/写 DDR | 共享 HP2 / HP3，各使用两个 AXI 主入口 |
| FAST / resize / Gaussian AXI-Lite | 0x43C00000 / 0x43C10000 / 0x43C20000 |
| DDR schema-3 邮箱 | 0x01000000，512 字节 |
| 原图 ping-pong / 高斯输出 | 0x02400000、0x02600000 / 0x02A00000 |
| CPU 原图参考 / 高斯参考 | 0x06000000 / 0x06400000 |

工作缓冲区互不重叠，按缓存行对齐；PL 写前先 flush/invalidate，完成后 invalidate。图像末字必须补零；选中点未写区域和各输出后的 64 字节保护区必须保持毒值。描述子在参考像素扫描之前计算，避免自检先把 raw/blur 缓存预热而低估 PS 后端耗时。

## 构建后上板

```powershell
pwsh -File fastv2/next/scripts/run_board.ps1 -ArtifactsOnly
# 确认板卡上电、JTAG 连接及当前授权后：
pwsh -File fastv2/next/scripts/run_board.ps1 -Iterations 1
pwsh -File fastv2/next/scripts/run_board.ps1 -Iterations 100
```

建议先一帧逐字节 PASS，再连续 100～1000 帧。脚本核对冻结输入、SDK 源文件、HDF、bit 的配对；JTAG 必须检测到一个匹配 idcode 的 xc7z015，才执行系统复位、易失 PL SRAM 配置及 ARM #0 ELF 下载。没有烧录 Flash。

每帧比较七级 resize 生成的金字塔、八层 Gaussian 像素、64 字节摘要、全部选中点、方向/moments/A32 bin/32 字节描述子；不只比较摘要哈希。PS 把结果写入不同于旧基线的 schema-3 邮箱，XSCT 直接读取，不依赖 DCC 或固定断点。

## 性能口径

- `processing_ms`：八层处理平均耗时，包含缓存维护、PS 配置、重叠的三个 PL 核及 PS 描述子，只减去独立参考比较/保护区检查/校验哈希的时间。
- `wall_ms`：同一帧包含上述逐字节验收的总耗时。两者不含一次性 JTAG 上传、参考上传校验、初始化和帧间邮箱发布。
- 分别记录 FAST、resize、Gaussian 的 PS 观察完成时间、配置开销、PS 后端时间、八层明细以及处理耗时 min/max。重叠执行的三个核时间不能直接相加当整帧耗时。
- `PS_observed_equivalent_cycles` 是 PS 观察区间换算为标称 100 MHz 的等效周期，包含 AXI/polling，不是核内部计数器。

目标仍是完整特征提取平均 ≤33.33 ms/帧。必须等真实硬件测量才能判定，不把模拟计时、HLS II 或综合估计频率当成板端 FPS。

## 已通过的前置检查

主机回归 12/12，包含真实三核 C 模型及实际 PS 控制代码的 5 帧测试、11 个故障注入；错误像素、摘要、点、保护区、返回码、超时、描述子及上传损坏均能提交 FAIL。ARM C99 语法检查通过。寄存器/缓存/计时的主机模型只是软件验证，不证明硬件缓存一致性、并发或 FPGA 性能。

`board_build/`、`host_build/reference_blur.bin` 为生成文件。`include/backend_tables.h`、`ps/full_reference.inc` 由独立 v1.0 CPU 生成，不手工修改。正式构建、时序、SDK 和板端结果仍待后续检查。
