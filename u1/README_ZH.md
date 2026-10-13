# u1 已验收基线

ACZ7015 / xc7z015clg485-2 / s_wrapper / 125 MHz / Vivado HLS、Vivado、SDK 2018.3。normal 960×720 八层、原预算、280 特征，100 帧 Production 整 PS 循环 **158.2902 FPS**，处理 6.306 ms、整帧 6.31751 ms；较 r17 的 143.2737 FPS 提升 10.48%。输入不可变且常驻 DDR，含 PS 准备、缓存维护、配置和调度，排除 JTAG 及外部采集传输。

描述子 raw/blurred AXI 未完成读请求窗口从 2 增至 8，并加入 HLS latency=64 调度提示；算术、地址、字节数、图像核和 PS 调度保持验收版本。完整 native、C/RTL、AXI/reset 和 8 请求排队长延迟证明通过。

normal Verified 100、noise/zero Verified 各 10 帧精确校验通过；Production 仍为 COMPLETE_UNVERIFIED。实际包内启动器 normal Verified 1 也已通过。125 MHz routed WNS +0.157 ns、WHS +0.046 ns，完整路由、约束和 DRC 门槛通过。完整报告见 release/RESULTS.md。

- release/：原验收包逐文件复制，bit/ELF、初始化、三组输入、报告及启动器保持原哈希。原发布报告引用当时的 r17 比较基线，这是保留的历史事实。
- source/：封存时作者工作区，含生成器、PS、约束和测试；实际编译输入以 frozen_hls/、frozen_board/ 为准。
- frozen_hls/：接受 IP 的冻结编译输入及主机证明。
- frozen_board/：冻结物理实现输入、完整链接综合网表及已接受路由 checkpoint。
- frozen_ps_runtime/：本版 125 MHz 硬件定义生成的 SDK 运行库和输入，复制前核对原清单哈希。
- evidence/：原上板验收、性能比较、包检查及试验记录，保留原始路径和比较对象。
- BASELINE.json / MANIFEST.json / verify.ps1：固定测速口径、来源及整包独立校验。

本目录保持冻结。新优化在开发工作区生成新作业，使用相同输入、预算和整 PS 循环计时与 u1 比较。源码重建依赖记录的 Windows 隔离 2018.3 工具链，未验证迁移目录后的源码重建。只读完整性检查：./verify.ps1。已授权易失性板测：./release/run_verified.ps1 -Frame normal -Frames 100。
