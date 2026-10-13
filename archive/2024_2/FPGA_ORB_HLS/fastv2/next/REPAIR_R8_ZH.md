# R8 构建及仿真修复

## 已定位的问题

- 三核工程停在 `export_front`：导出脚本直接向 HLS 2018.3 传递带反斜线和盘符的绝对工程路径，被当作非法工程名。现在规范化路径，切换到父目录，仅打开已存在的克隆工程名；导出错误保留原始日志提示，不再被“缺少打包脚本”掩盖。
- resize RTL 停在静态展开，未进入结果比较。生成的 AXI 模型 `mem_page_num=1409107`，两个模型数据库达到约 2.75 GB / 3.77 GB。综合 XML 将行搬运 `memcpy` 最大次数估成 536870911，总最坏延迟估成 1159641992883 周期；这不是实测帧耗时。

## 修改和验收边界

为行搬运字节数显式编码七位字数界限。冻结尺寸下每次搬运最多 121 个 64 位字；合法输入的地址、数据、打包顺序及像素运算不变。主机断言检查 0..121 的契约，保持完整 86400 字 AXI 缓冲区、13/14 笔图像测试、非法参数和重启覆盖。不修改生成 RTL 或旧失败记录。

修复后的硬件必须使用新 resize 综合输出：

```powershell
pwsh -File fastv2/next/scripts/build_and_test.ps1
pwsh -File fastv2/next/scripts/synthesize.ps1 -Selection image -Cosim -TimeoutMinutes 180
# 待图像任务 PASS，使用其 job 路径；不能使用旧 R3 resize。
pwsh -File fastv2/next/scripts/build_board.ps1 -ImageJobRoot <已通过的新图像任务路径> -Jobs 8
pwsh -File fastv2/next/scripts/run_board.ps1 -ArtifactsOnly
```

主机通过仅证明 C 模型一致。新 RTL、导出、综合、布线、时序、SDK 链接和文件配对仍须各自验证；本修复不授权 JTAG 操作，也不代表已达到 30 FPS。

## RTL 验收结果（2026-10-10）

`hls_build/jobs/20261010_004058_image_dd67b56b` 在 00:40:59～01:00:43 完成，状态 PASS；Vivado HLS/xsim 2018.3，目标 `xc7z015clg485-2`，时钟 10 ns。

- resize：13/13 逐事务比较通过，Verilog Pass；AXI 模型内存页从 1409107 降到 3。
- Gaussian：14/14 逐事务比较通过，Verilog Pass；AXI 模型内存页为 4。
- 未出现验收禁止的 DATAFLOW 状态/通道警告。完整声明输出区含 padding/tail 和非法参数结果均被比较。
- HLS 估计 resize 6914 LUT / 4074 FF / 46 BRAM18K / 15 DSP，Gaussian 6187 / 3894 / 24 / 1；不是最终路由资源。
- 三核源文件、器件、时钟和 PS MMIO 预检通过；路由、SDK 及 bit/HDF/ELF 配对仍待下一阶段验收。

测试序列含小层、非法参数和不同模式，平均事务周期不能作为真实每帧处理耗时或 FPS。

## 三核构建验收脚本修复（2026-10-10 02:00）

`board_build/jobs/20261010_013122_2d747941` 已实际完成 IP 导出和布线，但在验收报告解析处失败，SDK 未启动。其已完成路由报告显示 WNS +0.177 ns、TNS 0、WHS +0.051 ns；无时钟引脚和未约束内部端点均为 0。这些结果不等于整个 job PASS。

Vivado 2018.3 的 `check_timing -verbose` 报告将数量写在章节正文 `There are N ...`，而非 `checking category (N)` 标题中。新增 `vivado/timing_checks.tcl` 严格检查正文数量，排除目录标题；异常数量和缺失正文仍报错。`vivado/test_timing_checks.tcl` 使用实际已完成报告，并注入无时钟、未约束端点、缺失数量及空报告四种失败，验证验收门槛不被放宽。新构建冻结该检查器，旧失败 job 和报告不修改。

## 最终产物验收

`board_build/jobs/20261010_020213_a86e3550` 在 02:25:37 完成 PASS，SDK 链接成功；02:30 文件配对检查 PASS。100 MHz 路由 WNS +0.177 ns / TNS 0 / WHS +0.051 ns，无未约束内部端点，无 DRC Error/Critical Warning；保留并记录 DRC Warning。实际 LUT 20452、FF 23621、BRAM36K tile 61、DSP 16。完整产物路径、哈希、残留警告和手动下载方法见 `DELIVERY_R8_ZH.md`。该产物验收阶段未操作 JTAG。

## 授权后的实体板卡验收（2026-10-10）

用户授权“上板”后，确认 xc7z015、JTAG-SMT2 `C306ACCBABCD` 及产物配对，进行 SRAM 下载和 PS 运行。一帧 PASS；连续 100/100 帧 PASS，平均处理 59.516 ms/帧，16.802 FPS，含验证墙钟 81.258 ms/帧。重复归档输入的八层图像、点、摘要和方向/BRIEF 与 CPU 参考一致。未写非易失存储、未改基线，不宣称达到 30 FPS 或完整视频/SLAM。详细计时边界与原始日志见 [R8 上板报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R8_ZH.md)。
