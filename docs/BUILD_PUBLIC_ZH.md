# 公开源码的构建前提

本仓库保留原工程/CPU参考的兄弟目录结构，建议克隆到 C:/h/orb。
完整 vectors、参考二进制、SDK/BSP 和历史生成 job 不随源码上传。
这不是可脱离外部测试素材直接完成全部回归的完整归档。

## 只读校验

从仓库根目录执行：

```powershell
pwsh -NoProfile -File tools/verify_public.ps1
```

该命令只检查公开副本的哈希，没有 JTAG，也不运行 RTL 或编译。

## 主机回归

保持原版本：MinGW 14.2.0、CMake/Ninja、Python。Windows 上旧入口记录了
作者机器的工具绝对路径，迁移时只适配副本中的工具路径，保持版本和选项，
不修改系统全局 PATH，不使用 WSL。

至少需要从原素材包恢复以下文件到原相对位置：

- FPGA_ORB_SHARED_v1.0/vectors/flight_20261004T184929_998838_6bb93c56_f0000/input.pgm
- 原更早模型/批量回放所需的其他 vectors，按各脚本的参数要求提供。

依次生成几何 ROM、fastv2 的参考二进制，再运行 next：

```powershell
python FPGA_ORB_HLS/fastv2/scripts/generate_profile_rom.py
pwsh -NoProfile -File FPGA_ORB_HLS/fastv2/scripts/build_and_test.ps1
pwsh -NoProfile -File FPGA_ORB_HLS/fastv2/next/scripts/build_and_test.ps1
```

next 入口的 CTest 每项有 120 秒 watchdog；默认串行编译。不要把缺少素材、
工具启动失败或尚未运行的测试说成算法通过/失败。本次发布没有重跑这些命令。

## HLS/Vivado/SDK

只能使用 Vivado HLS/Vivado/SDK 2018.3，器件 xc7z015clg485-2、10 ns。
原脚本使用进程隔离 xilinx-2018.3.cmd，外部 wrapper 不随此仓库分发。
新的 RTL 必须按原脚本创建独立 job，验收真实 Verilog Pass 报告后，显式传入
源码匹配的 FrontJobRoot/ImageJobRoot 才能准备新硬件。旧 job 和缓存未上传，
不能将它们的历史路径当作当前存在的工程或直接跳过 RTL 验收。

作者入口位于 FPGA_ORB_HLS/fastv2/next/scripts/。本仓库不自动综合、布线或上板；
冻结默认 ELF 的 SDK 重建还需要全部本地参考输入和独立 SDK 工程。

## 版本依据

开发快照包含 Profile/缓存实验的源码及测试。已发布默认 ELF 的编译依据是
frozen/R11_DEFAULT/inputs，证据中默认 R11 的历史主机验收是 13 项；
后续快照的 17 项测试不是同一次默认固件构建，不混用结果。
PUBLIC_SOURCE_MANIFEST.json 记录每个副本来自交接包的哪一个文件。
