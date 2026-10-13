# r16 已验收基线

当前基线固定为 r16：ACZ7015 / xc7z015clg485-2 / s_wrapper / 100 MHz / Vivado HLS、Vivado、SDK 2018.3。normal 960×720 八层、280 个特征，100 帧 Production 实测 120.4358 FPS，处理 8.292 ms，整帧循环 8.30318 ms。输入常驻且不可变，含 PS 程序开销，不含外部采集传输。

- `release/`：已上板的 bit、配对 ELF、三组输入、完整验收报告及 `run_verified.ps1`。文件从已验收的时间戳发布包复制，原文件哈希保持一致；启动器已在原包上实际复测一帧 normal。
- `source/`：封存时的作者工作区，含生成器、PS、约束和测试入口；不含 build 运行目录。它包含编译之后修复的主机启动脚本，实际 HLS 编译输入以 frozen_hls 为准。
- `frozen_hls/`：两个被接受 IP 的原始编译输入、输入清单及 host 生成证明，复制前逐项核对原哈希。
- `frozen_board/`：物理实现输入、完整链接综合网表和已接受 routed checkpoint。PS 初始化及 ABI 在 release 中。
- `BASELINE.json`：固定测速口径、器件/工具、时序/资源、bit/ELF 哈希与原始任务来源。
- `MANIFEST.json` / `verify.ps1`：整包相对路径 SHA-256 与只读校验入口。

normal Verified 100 帧、normal Production 100 帧、noise/zero Verified 各 10 帧通过。Production 的标识仍为 COMPLETE_UNVERIFIED；逐字节数值一致由紧邻的 Verified 测试确认。WNS +0.076 ns、WHS +0.018 ns，TNS/THS 为零。Slice 使用 99.97%，普通 DRC 警告保留在 release/reports。

后续优化在开发工作区新建实验与输出，使用相同输入、预算及计时口径与 r16 比较，改变主频时必须注明基线 100 MHz 与候选实际主频；本目录保持冻结。源码重建仍使用原开发环境的过程隔离工具链和 SDK standalone 依赖，本次封存不声称已完成脱离该环境的源码重建。

复测入口：`./release/run_verified.ps1 -Frame normal -Frames 100`。其操作为用户已授权的易失性 SRAM/DDR 下载，不写非易失配置。`./verify.ps1` 只检查文件完整性。
