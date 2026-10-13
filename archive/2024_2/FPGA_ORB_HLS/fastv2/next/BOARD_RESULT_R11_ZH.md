# R11 实体上板：后端加速成立，整帧尚未提速

2026-10-10 用户本轮明确授权“上板”，执行 R11 候选的 SRAM/DDR 下载和测试。三次算法运行共 201 帧：101 帧逐帧 CPU 精确比较 PASS，100 帧 Production 仅运行完成；另有一次下载中断，未运行算法，不计入帧数。**不替换 R10 固定基线，不将本次实体授权扩展给定时任务。**

## 版本与安全边界

实际板卡 ALINX ACZ7015、xc7z015、Digilent JTAG-SMT2 `C306ACCBABCD`，DAP ID `4ba00477`、FPGA ID `0373b093`。实际下载明确使用 `board_build/jobs/20261010_132933_ps_a191d754`，ELF SHA256 `A5FC0FA01E8C63D300419DF7BE49E5F02C3EDE9C09FE68D83D6F7C7074D2130E`，没有使用自动指向 R8 的默认 job。

R11 仅优化 PS CORDIC；复用原 R8 bit/HDF，原 xc7z015clg485-2 / next_system_wrapper / 100 MHz、DDR/FIXED_IO 设计不变，无新增 PL 外部执行器。使用隔离 Xilinx/SDK 2018.3、ARM #0；复位 PS、初始化原 DDR/时钟、配置 FPGA SRAM、下载 ELF/归档参考数据并运行。未写 Flash、SD、eFUSE 或非易失启动配置，未改变 PL 时钟或算法验收。

测试前后 ArtifactsOnly 均 PASS，测试后 R10 146 个冻结文件默认哈希检查 PASS。硬件时序/资源未重新生成，仍复用路由 WNS +0.177 ns / TNS 0 / WHS +0.051 ns / THS 0，LUT 20452 / FF 23621 / BRAM36 tile 61 / DSP 16；原 44 项 DRC Warning 未消除。完整离线产物证据见 [PS_CORDIC_R11_ZH.md](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/PS_CORDIC_R11_ZH.md)。

## 下载中断与有界处理

首次默认 JTAG 15 MHz 下载在 `reference_blur.bin` 的 DDR 地址 `0x064A8000` 中断，错误 `Cannot access Debug Port: invalid status: ACK_OK 03186003`。当时尚未写运行命令或执行 con，没有算法比较结果；原失败日志保留。

新增只读 `ps/read_board_link.tcl`：核对相同线缆/器件，在已分配 DDR 区的三个地址各读两次，全部一致；不复位、暂停、恢复、编程或写内存。随后按本机 SDK `scripts/xsdb/xsdb/jtag.tcl` 文档，在同一个 hw_server 生命周期中把适配器时钟从 15 MHz 降到其支持的 1 MHz，再调用未修改的冻结 runner。单帧重试成功后才进行两个 100 帧测试，三次均正常完成。

新增 `ps/run_board_slow_jtag.tcl` 不改变冻结 runner、CPU 参考或 PL，校验固定线缆/器件和频率支持后 source 已配对 runner；没有自动重试循环。该频率仅在当前 hw_server 生命周期有效，服务器重启恢复默认。慢速上传约 1～2 分钟，上传时间不计入帧处理时间。降低 JTAG 频率是本次有效的传输绕开，不是链路/供电异常根因已修复的证明；未进行电源纹波等外部仪表测量。

## 实板结果

输入仍是同一归档 960×720 图像、八层，重复运行，不是变化视频。每帧候选 11078、输出特征 280，错误/detail 均 0。

| 模式 | 帧数与终态 | 平均处理 ms | FPS | 墙钟 ms | 验证 ms | 最小 / 最大处理 ms |
|---|---|---:|---:|---:|---:|---:|
| Verified Serial | 1/1 PASS | 45.420 | 22.016 | 66.554 | 21.135 | 45.420 / 45.420 |
| Verified Pipeline | 100/100 PASS | 41.856 | 23.891 | 63.769 | 21.913 | 39.004 / 60.294 |
| Production Pipeline | 100/100 COMPLETE_UNVERIFIED | 32.344 | 30.917 | 32.344 | 0 | 30.845 / 48.801 |

Verified 的 raw/blur 像素、点、摘要、方向/BRIEF、padding/guard/未选点区域逐帧检查通过；两次稳定 DDR 全 128-word 回读的摘要均为 `a1967dad68429025`，与 R10 一致。Production 的 verified_frames=0、digest=0，只是无运行错误的测速，不能写成 CPU 精确 PASS。两组 Pipeline 各有 700 次下一层提前启动，收尾错误与 pending mask 均 0。

| 模式 | poison ms | cache ms | 剩余 PL 等待 ms | PS 后端 ms | 配置 ms | 控制残差 ms |
|---|---:|---:|---:|---:|---:|---:|
| Verified Serial / 1 | 1.478 | 9.772 | 27.173 | 6.943 | 0.026 | 0.028 |
| Verified Pipeline / 100 | 1.778 | 10.878 | 21.660 | 7.482 | 0.027 | 0.032 |
| Production Pipeline / 100 | 0 | 3.241 | 21.768 | 7.287 | 0.026 | 0.023 |

各项独立舍入，求和可差 1 μs；processing 只扣除验证时间，Verified 仍含 poison/缓存等开销。Pipeline 逐核区间是 PS 延迟观测上界，不是真正 PL 内部周期，不将三个核的累计时间直接相加当整帧时间。

## 与历史 R10 的对照及判断

| 100 帧 Pipeline | R10 历史记录 ms | R11 本次 ms |
|---|---:|---:|
| Verified 平均处理 | 40.904 | 41.856 |
| Verified PS 后端 | 13.140 | 7.482 |
| Verified 剩余 PL 等待 | 15.440 | 21.660 |
| Production 平均处理 | 31.749 | 32.344 |
| Production PS 后端 | 12.858 | 7.287 |
| Production 剩余 PL 等待 | 15.599 | 21.768 |

后端下降约 43%，与实际 ARM 循环消除除法的证据一致。整帧并未随之下降：后端原本已与下一层 PL 重叠，缩短 PS 运算后，原先被遮住的 PL 时间转为等待。因此不能把后端减少的约 5.6 ms 全部算作整帧节省。本次图像核观测累计约 28 ms，比历史约 26～27 ms 有波动；未经同一轮 R10/R11 多次 A/B，不能把整帧差值全部归因于代码改变或断言发生硬件退化。

本次生产平均 32.344 ms 仍低于 30 FPS 的 33.333 ms 预算，但比历史 R10 慢 0.595 ms，最大 48.801 ms 超预算。结论：**PS 局部优化有效，整帧提速未证实；R10 继续作为性能基线，R11 保留为已验收的后端候选。** 下一阶段分项计时/变化输入测试之后，优先针对 PL 等待与 DDR 搬运重叠，而非继续只用 PS 局部速度推算整帧性能。

## 原始证据与复现边界

以下日志均在明确的新 R11 job 中，旧失败/基线 job 未修改：

- `board_run_Verified_Serial_1_20261010_133555_202.log`：原 15 MHz 下载失败。
- `board_link_read_20261010_133719_674.log`：三个 DDR 地址稳定只读诊断。
- `board_run_Verified_Serial_1_slowjtag_20261010_133853_007.log`。
- `board_mailbox_Verified_Serial_1_20261010_134142_030.log`。
- `board_run_Verified_Pipeline_100_slowjtag_20261010_134146_013.log`。
- `board_mailbox_Verified_Pipeline_100_20261010_134408_207.log`。
- `board_run_Production_Pipeline_100_slowjtag_20261010_134412_337.log`。
- `board_mailbox_Production_Pipeline_100_20261010_134638_928.log`。

仅在再次明确授权实体测试后，可以采用下列慢速命令；ArtifactsOnly 本身无 JTAG。该 helper 使用配对且哈希已核对的冻结 runner，**不可跳过第一行验收**：

```powershell
$r11Job = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_132933_ps_a191d754'
pwsh -NoProfile -File C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/scripts/run_board.ps1 -JobRoot $r11Job -ArtifactsOnly
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd' xsct C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/ps/run_board_slow_jtag.tcl $r11Job 100 Verified Pipeline
```

本次最后停在 Production Pipeline 完成结果，ARM #0 由冻结 runner 停止；稳定结果快照后未再下载或恢复 CPU。未测 1000 帧、变化视频、随机 AXI 停顿或处理中复位，也未实施完整视频/SLAM；自动定时优化仍只允许离线，不再自动上板。
