# r15 后的描述子行流水试验

最新状态：已恢复上板，normal 100 帧精确校验及性能测试、noise/zero 各 10 帧精确校验均通过。100 MHz / 同输入 / 同预算，实测 **120.4358 FPS**，较冻结 r15 **+8.92%**。独立发布包：`../orb60_board/20261013_100119_4ppc100mhz_2018_3`，116 文件哈希及启动器语法已通过；包内 `run_verified.ps1 -Frame normal -Frames 1` 已直接上板复测通过，280 特征精确一致。

基线 `../r15` 保持冻结：100 MHz、normal 960×720、八层原始预算、280 特征、100 帧 Production，程序循环吞吐 110.5721334 FPS。

候选为 descriptor row-fixed v6，只替换描述子 IP。raw 每行固定 32/40 字节读取，blur 每行按 8–48 字节固定长度分支读取。HLS 实际行循环 II 为 raw 9、blur 8。数值算法、特征预算、DDR 输入合同、100 MHz 和图像核沿用 r15。

- 当前作者头文件 SHA-256：ECC1105E565A5FCD54FA545D24C5A937CB6136E78B275CEFD7D988C461D4B9EE。
- 修改前 v4 作者备份：`build/row_pipeline_trial/before_sources/`。
- 2026-10-13 新运行 `scripts/host.py`：HOST_EXACT_PASS，普通/噪声/全零整帧及边界、方向量化、连续调用检查通过。
- 描述子完整 HLS/C/RTL 导出：`C:/h/orb4/20261013_000740_descriptor_d930122f`，SUCCEEDED，与当前作者/生成 C++ 哈希一致。
- 随机 AXI / reset 证明：`build/attempt140/axi_ddr_row_v6`，SUCCEEDED。独立核测试，不模拟两核共用真实 DDR 的争用。
- 集成构建：`C:/h/orb4/20261013_080802_board_93c3bfdd`，已启动，尚待布局布线和实际板测。
- 顶层综合及初始链接已通过。独立路由：`C:/h/orb4/20261013_083117_route_f6928ddc`，使用同一网表及 r15 的 CARRY_REMAP=2 / no_fanout_opt 策略，尚在运行。链接检查点 SHA-256：3D43CFB7BC02A40842CAECCFB6AC8EDDAECF064086C3638D679AD5A6B01A0DE6。
- `scripts/compare_r15.py --job <accepted job>` 检查相同输入和 PS 作者源码，按原始物理日志输出整帧 FPS、各层计数器及重叠模型。已用 r15 自身数据核对，增益为 0%。
- 默认 Performance_Explore 布局于 08:35 失败：Place 30-487，剩余 7403 Slice、未放置逻辑要求 7423 Slice，缺 20 Slice。1257 个控制集。厂商父启动器返回 0，但没有接受标记，脚本正确判为 FAILED；未下载该候选。r15 路由策略的独立任务继续运行。
- 独立任务布局成功：LUT 42365、FF 38099、Slice 11547/11550（99.97%）。这是 post-place 资源，尚未完成 routed 时序验收或上板。

独立描述子综合相比 r15 增加 464 LUT、528 FF。最终集成 routed 相比 r15 增加 445 LUT、528 FF，Slice 由 11550 降到 11547；100 MHz WNS +0.076 ns、WHS +0.018 ns，setup/hold 均通过。

## 暂停点（2026-10-13 09:09，用户要求“先暂停”）

独立路由任务 `C:/h/orb4/20261013_083117_route_f6928ddc` 已 SUCCEEDED，接受标记已生成；100 MHz routed WNS +0.076 ns、WHS +0.018 ns。最终 LUT 42365、FF 38099、BRAM 66、DSP 62、Slice 11547/11550。PS mock 和 ARM 编译通过，新 bit/ELF 的 `pairing.json` 为 PASS。

首次 normal Verified 100 在下载前失败：JTAG 链为空、Cable/device mismatch。Windows 未枚举指定下载器；独立低速探测同样 Cable mismatch，未执行 TAP 复位、下载或内存访问。没有新候选的物理正确性或 FPS 结果，未导出新接受发布包，r15 保持冻结。所有构建、板测控制和探测进程已结束；暂停期间不继续任何硬件操作。

继续时先恢复 USB/JTAG 和供电，再直接使用已配对的任务（无需重建 PS）：`scripts/board_test.py --job C:/h/orb4/20261013_083117_route_f6928ddc --frame normal --frames 100 --mode Verified`。通过后紧接 normal Production 100 `--rerun`，期间不插入其他 JTAG 操作。随后用 `scripts/compare_r15.py` 核对实际收益，有收益再补 noise/zero Verified 10 并导出接受包。

接受步骤：完整 routed setup/hold/约束/DRC → 新 bit/ELF 配对 → normal Verified 100 → normal Production 100，与 r15 同口径比较。若有实际收益，再补 noise/zero Verified 10 和可直接复测的独立发布包。

## 恢复后的物理接受（2026-10-13）

用户重新要求“上板”，Windows 恢复识别 C306ACCBABCD，实际 JTAG 链匹配 DAP 4ba00477 / FPGA 0373b093，易失性 SRAM 下载后全部板测通过：

- normal Verified 100：EXACT_VERIFIED，280 特征；日志 `20261013_095315_normal_Verified_100.log`。
- normal Production 100：COMPLETE_UNVERIFIED，280 特征；总循环 830318 us，即 8.30318 ms/帧、120.4357848 FPS，处理平均 8292 us（8279..8308）。日志 `20261013_095535_normal_Production_100.log`。这是性能依据，不使用 Verified 中受 CPU 比较干扰的计时。
- noise Verified 10：EXACT_VERIFIED，942 特征；日志 `20261013_095700_noise_Verified_10.log`。
- zero Verified 10：EXACT_VERIFIED，0 特征；日志 `20261013_095858_zero_Verified_10.log`。

`evidence/row_pipeline_comparison.json` 已核对相同 golden、原始 PS 作者源码、输入合同和 100 MHz，并从实际终态日志核验邮箱。r15 590 个封存文件 SHA-256 复核通过，未修改基线。

各层描述子平均周期总和 657740 → 503830，减少约 23.4%。现在前三组重叠由 image(n+1) 主导，后四组仍由 descriptor(n) 主导；重叠模型 8.13535 ms，整循环与模型差 0.16783 ms，这不是独立测得的 CPU 时间。图像逻辑未改，图像计数总和略增；尚未单独测量 DDR 争用等待，不能把全部变化归因于 DDR。

最终 routed：76411/76411 网络已布线，路由错误 0、内部未约束路径 0、setup/hold TNS/THS 0。DRC 无 Error/Critical Warning，108 个普通 Warning（DSP pipeline、BRAM async control、无负载网络及报告规则限额），详见新包 reports/post_route_drc.rpt。独立核 reset 测试通过；未测试整机运行中 PS/AXI reset。

bit SHA-256：74D4976175370C2EEAD989A904DBCD613CB8A5A908FDF86929FA1F4899C98132。

ELF SHA-256：485BD2FD696C6AF65ED76FEC19A195ADA7EE53BA3C623F9752BE54EFAD1527B3。

发布包启动器复测日志：`20261013_100158_normal_Verified_1.log`，CPU_EXACT_PASS。证明指针：`evidence/row_pipeline_portable_verified.json`。板卡当前装载该版本 SRAM 配置，CPU 在稳定终态后停止；未写 Flash、SD 或 eFUSE。
