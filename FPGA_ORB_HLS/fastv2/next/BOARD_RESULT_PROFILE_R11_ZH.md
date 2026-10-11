# R11 Profile 实板：分项计时、精确比较与调试回读干扰

2026-10-10，用户在本轮明确“尽快上板测试”，因此从离线检查切换为人工授权实体测试。实际运行四组，共301帧：101帧CPU精确比较PASS、200帧Production仅COMPLETE_UNVERIFIED。后者没有逐帧参考比较，不能改称CPU一致PASS。没有修改原R10基线、PL或固件算法；本轮不进入缓存或PL优化，不暂停未完成的有限优化定时任务，也不授予定时任务JTAG权限。

## 实际版本与边界

所有下载均明确使用`board_build/jobs/20261010_154719_ps_02a2d8c3`独立Profile任务，不用last_board默认R8或旧R11 ELF替代。最新ELF SHA256为`1C8C503E11F76AC3E304BF8200ED32224120A2217EC0D11F89B08F2E28EF98C5`；bit/HDF复用R8，完整路径/配对哈希、SDK/主机/链接/路由证据见[阶段2说明](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/PS_PROFILE_STAGE2_ZH.md)。

目标ALINX ACZ7015/xc7z015，JTAG-SMT2 `C306ACCBABCD`，DAP `4ba00477`、FPGA `0373b093`，ARM#0，PL100MHz；原DDR/FIXED_IO设计，无新增外部执行器。测试前只读检查三个既有DDR地址各两次，稳定后才SRAM配置/DDR初始化及下载。JTAG在同一会话中用原已验证helper设为1MHz，仅改变适配器时钟，不提PL频。未写Flash/SD/eFUSE/启动镜像，无全局PATH变化/工具升级。

第一次主机调用将通用原生启动器的参数逐一引用直接用于cmd /c，Xilinx setupEnv路径解析失败，尚未访问JTAG；失败证据保留在`optimization_build/profile_link_20261010_160038_254`。改用现有隔离wrapper入口后只读链路和四次下载均成功，不把主机启动失败计成板卡算法FAIL。

四次都使用同一归档960×720图、固定八层，候选11078、特征280；不是100张变化视频。Profile实际使用333333343Hz PS BSP timer，每帧1120次后端插桩读数。每次完成立即用无写入reader做两个完整稳定快照、metadata前后bookend，先capture再进行下一次下载。最终ArtifactsOnly再次PASS，R10冻结146/146哈希PASS。最后ARM#0由冻结runner停止，未再次恢复或自动上板。

## 实际结果（ms/帧）

| 运行方式 | 帧数/终态 | 平均处理 | 每帧wall | P95处理 | P99处理 | 最小/最大处理 |
|---|---|---:|---:|---:|---:|---:|
| Verified Serial，原250ms轮询 | 1/1精确PASS | 45.546 | 66.801 | 45.546 | 45.546 | 45.546/45.546 |
| Verified Pipeline，原250ms轮询 | 100/100精确PASS | 41.829 | 63.899 | 55.021 | 59.742 | 38.793/59.754 |
| Production Pipeline，原250ms轮询 | 100/100 COMPLETE_UNVERIFIED | 32.429 | 32.429 | 44.756 | 49.798 | 30.853/51.770 |
| Production Pipeline，延后回读 | 100/100 COMPLETE_UNVERIFIED | 30.864 | 30.864 | 30.874 | 30.875 | 30.854/30.876 |

Verified raw/blur像素、点、摘要、方向/BRIEF、padding/guard/未写尾区逐帧精确通过；digest `a1967dad68429025`与R10一致。Pipeline每组提前启动700次，abort_drain_error/pending_core_mask为0。Production的verified_frames=0、digest=0、verify/prep=0，不能用摘要零值充当一致性证明。

延后回读组平均处理口径32.400 FPS；该100帧的P99和最大值都低于33.333ms预算。原轮询Production有11/100帧超过预算，延后回读0/100。**仅说明固定图像、本次100帧、Profile处理区间；不保证变化视频、任意帧硬实时、默认无插桩版本或完整视频/SLAM达标。** 不把这些数字更新进R10 manifest。

## 观察器对照：不是新的算法加速

原冻结runner每250ms读128字mailbox。新作者helper`run_board_quiet_jtag.tcl`不改冻结runner/bit/HDF/ELF/输入/CPU验收/deadline：仅con成功后，原同步250ms after改成10秒；其他after参数和con参数原样透传。7项纯mock通过，无真实JTAG。实际quiet日志只有一次10秒延后标记，此组程序约3.1秒已完成，首次结果读数发生在完成后；新profile记录保留全部100帧，不删异常帧、不warmup截断、不扣除时钟开销。

相同Profile程序两组Production：平均32.429→30.864ms，P99 49.798→30.875ms，最大51.770→30.876ms。周期性尖峰在延后回读后消失，支持运行时调试访问是此前时延尖峰的重要来源。底层DAP/DDR停顿机制尚未用逻辑分析/AXI计数器定位；这是一次同版本观察方式对照，非交错多轮A/B，也不是算法优化收益。旧R10/R11历史数值保留原口径，不能未经同观察方式重测就宣称它们的真实算法退化或新算法提升4.8%。

## 分项：以延后回读Production为主

| 原互斥处理项 | 平均ms | 关系 |
|---|---:|---|
| 剩余PL等待 | 20.522 | 最主要的暴露耗时，约66.5%；不是三个核内部周期之和 |
| PS后端整体 | 7.053 | 下表三段包含在此项，不能重复相加 |
| 缓存维护 | 3.241 | 当前仍原全区域策略，未进行稀疏缓存优化 |
| 寄存器配置 | 0.026 | 低占比 |
| 控制残差 | 0.022 | 包含未归入以上阶段的处理开销 |

| PS后端子区间 | 平均ms | P99ms |
|---|---:|---:|
| 方向矩 | 2.096 | 2.134 |
| CORDIC及A32量化 | 0.580 | 0.581 |
| BRIEF | 4.307 | 4.313 |

三段共约6.982ms，小于后端7.053ms，余量为校验/元数据/输出等区间外代码与仪表边界。clock读取循环平均0.050473ms，在每帧原计时区间外，含循环/读数成本，未扣除；实际四次特征clock读数位于后端内。它不能替代同机普通/Profile多轮A/B来精确估计全部插桩成本，且帧间Profile记录/发布不计入processing/wall；视频端到端耗时仍未测。

三核同层并行、PS(n)与PL(n+1)重叠；视频帧不重叠。quiet组观测front累计26.647ms、resize21.567ms、Gaussian26.568ms，这些是延迟观测上界，不能相加为整帧，也不能称真正kernel周期。下一有限阶段先做缓存访问行集合的精确主机实验，再评估Gaussian/FAST搬运扫描重叠；不要仅根据BRIEF4.3ms就预设它能从整帧全部消除。

## 原始日志和CSV

以下均在上述独立Profile job根目录；CSV是稳定快照中原始uint64 ticks的机械导出，不是重算伪造帧：

- `board_profile_Verified_Serial_1_20261010_160153_609.log`。
- `profile_snapshot_Verified_Serial_1_20261010_160657_628.log`及同名CSV（1帧）。
- `board_profile_Verified_Pipeline_100_20261010_160720_866.log`。
- `profile_snapshot_Verified_Pipeline_100_20261010_160720_866.log`及同名CSV（100帧）。
- `board_profile_Production_Pipeline_100_20261010_161029_309.log`。
- `profile_snapshot_Production_Pipeline_100_20261010_161029_309.log`及同名CSV（100帧）。
- `board_profile_Production_Pipeline_100_quiet_20261010_161439_138.log`。
- `profile_snapshot_Production_Pipeline_100_quiet_20261010_161439_138.log`及同名CSV（100帧）。

Verified捕获时的reader已有全帧/百分位验证；随后增加主邮箱统计桥接，Verified CSV的process/wall/verify/config/backend/prep/cache/wait八项均用独立PowerShell整数平均、舍入核对一致。两个Production现场capture使用新增桥接后的reader。最终纯reader64项PASS、默认backend对象哈希不变，reader作者源冻结在`optimization_build/profile_capture_sources_20261010_161606_750`。

未运行1000帧实体、变化视频、随机AXI停顿、处理中复位、电源纹波测量或普通/Profile同会话交错多轮性能比较。host已覆盖变化输入与人工1000条统计/容量，不等于上述物理测试。有限优化阶段3/4/最终交付尚未完成；本轮实体结果只记录，不自动扩展板卡作业。
