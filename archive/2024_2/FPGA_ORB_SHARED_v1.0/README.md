# FPGA ORB v1.0源码与测试交接包

默认：8层/1.2/T2 20-7/K1无回填/A32/Gaussian7/标准bit_pattern_31_。
降级：仅改4层，其余一致。先读QUICK_START_FPGA.md，再读DESIGN_SPEC_v1.0.md、FORMAT.md。
golden.cpp/.hpp/constants.hpp/main.cpp是CPU整数参考，不是HLS/RTL；许可证见LICENSE/NOTICE.md。

- vectors/：全部43个既有输入（9合成+34真实样本），PGM/同像素PNG/来源元数据/manifest。
- profiles/default8/：8层正常1000与初始化5000的B1包及每级中间结果。
- profiles/degraded4/：4层两档完整独立参考。
- scripts/：标准库比对、哈希检查和重放；不需要原始完整数据集或源码外部路径。
- patches/：所有SLAM部署必需的H1控制流补丁、说明及应用检查。
- evidence/：已完成G1/H1/H2决策依据，不是本版新跑的SLAM结果。
- pyramid_storage.csv/storage_estimates.json：752×480/960×720各层及4/8层容量估算。
- VALIDATION/REPLAY_VALIDATION/GENERATION/DELIVERY_ACCEPTANCE.json：本版实际组件生成和核验结果。
- SHA256SUMS及zip旁.sha256：包内全部文件与整个压缩包的哈希。

43×2层配置×2档=172份新生成输出。完整相机内参不是前端提取输入；Tello素材/决策结果均
“借用内参，未本机标定”，不提供绝对精度或真实尺度。CPU黄金耗时不代表FPGA或RK3588。
本包不含整段视频、全序列特征库或上板驱动，也不自动执行SLAM实验。

复现生成在本项目中用scripts/release_v100.py（先prepare再generate，验证后finalize）。
收件人只需按QUICK_START_FPGA.md编译和重放，输出目录必须新建，失败证据不覆盖。
