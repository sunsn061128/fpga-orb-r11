# r17 已验收基线

ACZ7015 / xc7z015clg485-2 / s_wrapper / 125 MHz / Vivado HLS、Vivado、SDK 2018.3。normal 960×720 八层、原预算、280特征，100帧Production整PS循环143.2737 FPS，处理6.968ms、整帧6.97965ms。输入不可变且常驻DDR，含PS准备/缓存维护/配置/调度，排除JTAG及外部采集传输。

normal Verified100、noise/zero Verified各10帧精确校验通过；Production仍为COMPLETE_UNVERIFIED。实际包内启动器normal Verified1也已通过。125 MHz routed WNS+0.157ns、WHS+0.053ns，完整路由/约束/DRC门槛通过。完整报告见release/RESULTS.md。

- release/：原验收包逐文件复制，bit/ELF、初始化、三组输入、报告及启动器保持原哈希。原发布报告引用当时的r16比较基线，这是保留的历史事实。
- source/：封存时作者工作区，含生成器、PS、约束和测试；实际编译输入以frozen_hls/、frozen_board/为准。
- frozen_hls/：接受IP的冻结编译输入及主机证明。
- frozen_board/：冻结物理实现输入、完整链接综合网表及已接受路由checkpoint。
- frozen_ps_runtime/：本版125MHz硬件定义生成的SDK运行库和输入，复制前核对原清单哈希。
- BASELINE.json / MANIFEST.json / verify.ps1：固定测速口径、来源及整包独立校验。

本目录保持冻结。新优化在开发工作区生成新作业，使用相同输入、预算和整PS循环计时与r17比较。源码重建依赖记录的Windows隔离2018.3工具链，未声称验证过迁移到另一目录的源码重建。只读完整性检查：./verify.ps1。已授权易失性板测：./release/run_verified.ps1 -Frame normal -Frames 100。
