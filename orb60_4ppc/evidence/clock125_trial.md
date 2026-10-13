# r16 后的 125 MHz 主频试验

用户要求把已验收的行流水版本设为 r16 并尝试 125 MHz。r16 已独立封存至 `../r16`，590 个文件 SHA-256 校验通过；normal 100 帧 Production 为 100 MHz / 120.4357848 FPS / 8.30318 ms。历史 r15 同样保持冻结。

本轮保持 960×720 八层、原始预算、相同数值算法及 DDR 输入合同。主频改变必须重新匹配 PS FCLK0、四个 HP ACLK、互联与两个核的 8 ns 时序约束，并生成对应 PS 初始化和运行配置。

r16 最慢 routed setup 路径在图像核：14 层组合逻辑，data delay 9.661 ns（logic 2.306 / route 7.355 ns），100 MHz WNS +0.076 ns。不能只把初始化中的 FCLK 改成 125 MHz 并沿用旧时序结论。

初始方法：Vivado HLS 2018.3 用 8 ns 重新调度两个核，重新跑完整 C/RTL 和随机 AXI/reset，再做 125 MHz 整机综合及路由。新输出独立保存，未经 routed setup/hold/约束/DRC 和 bit/ELF 配对通过，不下载候选。物理验收仍为 normal V/P100、noise/zero V10。

8 ns HLS 作业：图像 `C:/h/orb4/20261013_101904_layer4_51c5f285`，描述子 `C:/h/orb4/20261013_101904_descriptor_58cc08ea`。整机入口显式指定 `--frequency-mhz 125`，检查两个 HLS 及 AXI 仿真的 8 ns 属性；路由重试继承实际频率，验收要求单时钟周期为 8 ns。PS 通过候选硬件定义生成全新 SDK standalone 运行库，再比较 CPU 计时频率及配对 PS init。

PS 基础配置 IO PLL=1800 MHz，整数二级分频不能精确得到 125 MHz。时钟探测证明 PS IP 会根据请求的 FCLK0 自动求解 PLL/分频；直接设置派生 IO PLL 属性会被忽略，已去掉该设置。探测实际 FCLK0=125.000000 MHz，CPU=666.666687 MHz，DDR=533.333333 MHz，`build/clock125/v1/ps_clock_check/accepted.stamp` 为 PASS。BD 检查实际 FCLK0 误差不超过 0.001 MHz，保存实际 CPU/DDR/PLL/分频与 PCAP 属性。板测回读 FCLK0 寄存器并与配对生成的初始化掩码值比较。

首轮 AXI 测试 `build/clock125/v1` 失败：测试时钟已改为 8 ns，但 elapsed_cycles 和超时界限仍除以固定 10000 ps，两个核报出的硬件周期约为测试换算值的 1.25 倍。修正这两处为实际 period_ps，保留所有时钟计数、数值、AXI、guard 和 reset 判断，在新的 `v1_recheck` 输出目录完整复测；旧失败会话保留。

复测两个 8 ns AXI gate 均 SUCCEEDED：图像 3 用例，描述子 7 用例，含复位中断和直接完成屏障。描述子原始 8 ns export SUCCEEDED，完整 Verilog C/RTL 30 次事务 / 1222 特征 Pass。图像原始完整联合仿真继续进行；相同冻结 RTL 在 AXI 通过后独立打包至 `C:/h/orb4/20261013_110053_layer4_d6b9a934`，整机实现与剩余 C/RTL 并行，下载前仍要求原始完整联合仿真通过。流程记录 `build/clock125/v1_impl/status.json`，已完成的 `v1_recheck` 控制器改记 SUPERSEDED，不重跑其已通过 AXI，也不重复启动整机。

整机 125 MHz 原始作业 `C:/h/orb4/20261013_110202_board_53d47b9e`。其 `vivado/ps_clock_config.tsv` 显示 IO PLL=1000 MHz/FBDIV30、FCLK0 4×2 分频得到 125 MHz、PCAP200 MHz、CPU666.666687 MHz、DDR533.333333 MHz。生成 PS init 的 IO PLL FBDIV 值 0x0001E000，FCLK0 掩码值 0x00200400；板测除 FCLK0 外还回读三路 PLL 反馈分频和 CPU 分频。

125 MHz v1 整机综合完成：LUT43378、FF42205；post_synth WNS=-1.790 ns，最慢路径在描述子 CORDIC 的 old_reg[62] 宽位移反馈。独立重试 `C:/h/orb4/20261013_112101_route_018c8dfd` 使用 r16 的 CARRY_REMAP2/no_fanout_opt。尚未做任何候选下载。

v2 针对该路径修改 2018.3 生成器的 shift_floor：HLS/C 仿真使用定义明确的 ap_int<64> 算术右移，native 分支仍为原可移植 floor 公式。对 0≤amount≤31，算术右移与原负数 `-1-((-(value+1))>>amount)` 精确相同，位宽与 CORDIC 迭代/舍入均未改变。目标是消除负数移位前后的两条 64 位取反/加法链；v1 冻结作业继续物理实现，v2 在新的 native/HLS/AXI/整机作业单独验证。

v2 HLS `C:/h/orb4/20261013_112734_descriptor_a518f7b0` 的 C 仿真 30 次 / 1222 特征通过；反馈仍受 II=1 限制，预计关键反馈组合 7.468 ns，整体 HLS 时序预估 10.535 ns（有效预算 7 ns）。v3 保留有符号移位并将 CordicIterations 的 II 明确改为 2，使位移/加法反馈可以分段，迭代数、角度精度和数值算法不变。v2 冻结作业不修改，v3 再做完整 native/HLS/AXI/整机校验。

v3 描述子 `C:/h/orb4/20261013_113600_descriptor_644488b8` export SUCCEEDED：CordicIterations 实际 II=2、depth2，模块预估6.632 ns；完整 Verilog C/RTL 30 次 /1222特征 Pass，8 ns AXI gate `build/clock125/v3/v3_axi_descriptor_8ns` SUCCEEDED。整机作业 `C:/h/orb4/20261013_120135_board_ea402b6f`，独立 CARRY2 路由 `C:/h/orb4/20261013_122406_route_3dc08b7f`，physopt 预估 WNS=-0.135 ns，最终路由尚未验收。v1 CARRY2 最终125 MHz routed WNS=-2.585 ns/WHS+0.041 ns，FAILED，无候选下载。

原8ns图像 XSIM 在90分钟期限到达时只完成16/21，状态FAILED（超时），无完整 Pass 报告，不能据此下载。补做同一冻结 RTL 的独立 ModelSim 全量21用例：`build/clock125/layer_full21_axi`，原19组独立 NumPy/CPU交叉验证向量逐字节复用，另加level=-1/8两个非法参数用例，继续检查整片DDR结果/guard、4KB/随机背压、复位中断和ap_done最终响应屏障。真实全量PASS前不下载；若通过，板测/发布明确记录该完整独立C/参考到RTL gate及原XSIM超时，不伪造vendor报告。

v3 CARRY2 最终125 MHz routed WNS=-0.549 ns、WHS+0.022 ns，setup未通过，未生成接受 bit。最慢路径由图像核格子评分更新的 token寄存器到 best_score[203] 的 CE，13级 LUT/MUX（logic2.336ns、route5.638ns）。Slice11547/11550，204项完全分割的8位评分表造成大读选择器与分散的写使能。另起同RTL控制集 remap≤8 试验 `C:/h/orb4/20261013_141815_route_4a2aca92`。

v4 修改图像2018.3生成器，将 best_score 的 complete partition 改为 RAM_S2P_BRAM，候选更新循环目标 II=2，保留真实同格连续候选的 RAW 依赖，不使用 false dependence。四像素图像通路保持原实现，候选更新可能增加背压，必须重新查实际II、完整数值/AXI及上板整帧性能。描述子复用通过完整验证的v3双周期版本；r16保持冻结。

v3 CE≤8 重映射3792寄存器后装箱失败：LUT46359>46200，未放置实例需要9069 Slice、剩余8885。未生成接受 bit。

v4 native 完整精确校验 PASS。HLS `C:/h/orb4/20261013_144219_layer4_52645026` C仿真21组PASS，ReduceCandidateTokens 实际 II=2/depth2，best_score确实生成RAM_S2P_BRAM block memory。全量独立 ModelSim `build/clock125/v4_layer_full21_axi` 正在验证；驱动记录 `build/clock125/v4_watch`，通过后先打包同一冻结RTL，再启动整机。独立单IP资源综合 `C:/h/orb4/20261013_150739_size_fb3e1d50` SUCCEEDED：LUT28387、FF21779、BRAM53、DSP52；这是资源/OOC检查，不是整机装箱或时序接受结论。

2026-10-13 16:42 更新：v4 HLS export SUCCEEDED，厂商 Verilog C/RTL 完整21组 Pass；独立 `v4_layer_full21_axi` 也完成21组、reset14，全部数值、DDR/guard、AXI及最终响应屏障通过。`scripts/rtl_gate.py.verify_layer21` 已独立验算其结果/config/向量及冻结RTL哈希。整机125 MHz作业 `C:/h/orb4/20261013_163156_board_bf4e9cdf` 已启动，配套全新SDK运行库 SUCCEEDED，PS init 与候选生成配置相同；当前进入整机综合/路由。控制器 `build/clock125/v4_impl` 在时序接受后才生成配对ELF，尚未下载候选。

v4 整机 post_synth：LUT40928、FF37309、BRAM66.5、DSP62，8 ns预估 WNS+0.857 ns/WHS+0.037 ns；这不是最终 routed 验收。独立CARRY2/no_fanout_opt作业 `C:/h/orb4/20261013_164426_route_99a17498` 已启动。16:47 低速JTAG探测确认下载器序号C306ACCBABCD，但500 kHz/TAP复位后仍为DR all zeroes，未读到目标FPGA ID，未写入板卡；已请求恢复供电/连接，路由继续。

17:07 更新：v4 默认整机作业正式 SUCCEEDED，125 MHz routed WNS+0.157 ns/WHS+0.053 ns，67576需布线网络全部完成，路由错误0，内部未约束端点0，DRC无Error/Critical Warning。最终LUT38597、FF36065、Slice11542/11550、BRAM66.5、DSP62。CARRY2路线也SUCCEEDED（WNS+0.115/WHS+0.028）；选择裕量更好的默认路线。首次配对因Python子进程继承另一PowerShell的模块路径而找不到Get-FileHash；修复绝对模块加载后，又发现系统PowerShell把ARM编译器的全局计时器pragma说明当作异常。改用cmd内部捕获日志并严格检查实际退出码，重建后完整mock三组、ARM编译、ELF内存范围和81文件配对SHA通过。原失败状态/log及中间文件保留，成功恢复记录 `build/clock125/v4_impl/ps_pair_final.json`。用户恢复供电/连接后低速JTAG目标ID0373b093/4ba00477正确；已开始125 MHz normal Verified100物理测试。

17:14 物理结果：normal Verified100 EXACT，280特征；紧接同一配置/驻留DDR Production100 完成（COMPLETE_UNVERIFIED），wall697965us、平均处理6968us、整PS循环143.2736599 FPS。冻结r16为120.4357848 FPS，提升18.9626987%，8.30318→6.97965ms/帧。两版相同PS board.c/960×720八层/原预算/normal golden SHA及不可变驻留DDR合同；排除JTAG下载与外部采集/传输。完整比较和实际逐层计数见 `evidence/clock125_comparison.json`。125MHz核间重叠模型6.82072ms、实测差0.15893ms，该差值含末尾写回/响应，不能当作CPU专属耗时。实际FCLK0回读0x00200400，三PLL/CPU分频均匹配。noise Verified10 EXACT942，zero Verified10正在进行；全部通过后才导出正式包。

17:21 最终完成：zero Verified10 EXACT0。正式包 `../orb60_board/20261013_171727_4ppc125mhz_2018_3` 导出，109文件SHA及包内PowerShell启动脚本AST检查PASS；使用包内bit/ELF/PS init重新实际下载normal Verified1，完整终态/时钟回读及280特征精确校验PASS。包manifest SHA256=`50984D4CA90F71799E6D6C1B36FB38A43D69717D89E42F6E1E84740D7EB8B106`，实际独立包日志 `20261013_171822_normal_Verified_1.log`。最终机器可读接受记录 `evidence/clock125_result.json`，发布索引 `evidence/latest_release.json` 指向125MHz包并仍以冻结r16为比较基线。此次仅JTAG易失性配置，不写flash/SD/eFUSE；未另设r17基线。
