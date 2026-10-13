# R8 实体板卡测试结果

记录时间：2026-10-10 07:21，北京时间。按用户“上板”授权，使用 FPGA development 技能要求的器件/产物核对、一帧自检、连续帧验证顺序；未修改算法或重新构建。

## 目标和产物

ALINX ACZ7015，`xc7z015clg485-2`，PL 100 MHz，顶层 `next_system_wrapper`。JTAG 读取到 Digilent JTAG-SMT2，序列号 `C306ACCBABCD`；FPGA IDCODE `0373b093`，同时识别 ARM DAP。工具为过程隔离的 Xilinx 2018.3，不修改全局 PATH。

使用固定任务 `board_build/jobs/20261010_020213_a86e3550` 的 bit/HDF/ELF；下载前 `run_board.ps1 -ArtifactsOnly` 再次 PASS，冻结源码、输入、SDK 硬件和产物配对通过。完整路径、SHA256、资源、路由时序和残留警告见 [交付说明](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/DELIVERY_R8_ZH.md)。

只复位 PS、初始化 DDR、配置 FPGA SRAM、下载 ELF 与测试数据。未写 Flash、SD 或 eFUSE，原基线文件未改动。测试结束后脚本停止 ARM #0 并断开连接；本次 SRAM 配置掉电丢失。

## 实测验收

| 项目 | 一帧自检 | 100 帧连续测试 |
|---|---:|---:|
| DDR 回读状态 | PASS，1/1 | PASS，100/100 |
| 错误码 / detail | 0 / 00000000 | 0 / 00000000 |
| 平均处理耗时 | 58.247 ms/帧 | 59.516 ms/帧 |
| 处理 FPS | 17.168 | 16.802 |
| 含参考验证的墙钟耗时 | 79.477 ms/帧 | 81.258 ms/帧 |
| 参考验证耗时 | 21.230 ms/帧 | 21.742 ms/帧 |
| PS 寄存器配置 | 0.027 ms/帧 | 0.027 ms/帧 |
| PS 方向/BRIEF 后端 | 12.683 ms/帧 | 12.755 ms/帧 |
| 每帧八层候选 / 特征数量 | 11078 / 280 | 11078 / 280 |

八层 raw/blur 图像、选中点记录、摘要、方向和 BRIEF 特征均通过 CPU 参考精确比较；输出 guard、padding 及未用点区检查通过。100 帧期间未报告 AXI 超时、核心返回错误或数据/边界不一致。

30 FPS 预算为约 33.333 ms/帧；当前 59.516 ms/帧，仍未达标，约需 1.79 倍整体提速。

## 100 帧平均逐层耗时

下表单位为 ms；三个 PL 核在同一层并行启动，不能把三列相加当成整帧耗时。

| 层 | FAST 前端 | resize | Gaussian | PS 后端 |
|---|---:|---:|---:|---:|
| 0 | 8.322 | 6.890 | 8.555 | 2.941 |
| 1 | 5.827 | 4.918 | 5.977 | 2.085 |
| 2 | 4.263 | 3.562 | 4.176 | 1.979 |
| 3 | 2.889 | 2.451 | 3.036 | 1.796 |
| 4 | 2.249 | 1.823 | 2.031 | 1.304 |
| 5 | 1.532 | 1.311 | 1.420 | 1.068 |
| 6 | 1.093 | 0.965 | 1.003 | 0.839 |
| 7 | 0.767 | — | 0.669 | 0.743 |
| 核/后端跨层总计 | 26.942 | 21.920 | 26.866 | 12.755 |

总计取计时器累积后换算，可能与已四舍五入的逐层值之和略有差异。PL 数值为 PS 从启动到观察到完成的时间，包含 AXI/轮询影响；日志中的 100 MHz 等效周期不是 PL 内部计数器。

## 计时与覆盖边界

处理耗时从各帧开始到结束，扣除参考比较、guard 检查与结果哈希时间；仍包括输出区测试用 poison 填充、缓存维护、PS 配置/等待和方向/BRIEF 后端。它不是仅 PL 计算时间，也不能直接称为去掉自检准备后的生产程序耗时。初次 JTAG 上传和输入参考初始化不计入帧耗时。

连续测试重复同一张 960×720 归档图像及其八层金字塔，不是 100 张不同图像或真实视频流。结果证明该输入下连续 100 帧精确一致，不能推广为所有输入、随机 AXI 背压、处理中复位或长期运行均已覆盖。此前 DRC 的 RAM 异步控制等 Warning 仍保留，未执行处理中复位试验。

当前实现包含三核 PL 和 PS 方向/BRIEF，但不包含视频解码、相机采集、跨层 B1 打包、匹配或完整 SLAM。本次未运行 1000 帧，也未扩大优化范围。

## 命令与原始记录

在 `C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS` 中执行：

```powershell
$nextJob = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550'
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $nextJob -ArtifactsOnly
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $nextJob -Iterations 1
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $nextJob -Iterations 100
```

状态从 schema-3 固定 DDR 结果区 `0x01000000` 自动回读，不依赖 DCC 输出或硬编码断点。

- [一帧原始上板日志](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550/board_run_1_20261010.log)
- [100 帧原始上板日志](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_020213_a86e3550/board_run_100_20261010.log)
