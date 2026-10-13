# 阶段2：独立 PS Profile 固件与稳定逐帧回读

本阶段是测量和正确性准备，不是新的 PL 加速版本。用户在本轮明确“尽快上板测试”后才进行实体操作；不把该授权扩展到后台 heartbeat。R10 固定基线、R11 已接受候选及所有历史失败任务保持不变。

## 已接受的独立 SDK 任务

任务：`board_build/jobs/20261010_154719_ps_02a2d8c3`，PS_ONLY、Profile=true，2026-10-10 北京时间15:47:19～15:48:48完成。实际17/17主机回归通过（17.54秒），含独立方向117266组、普通/Profile变化输入56事务5624特征、逐帧统计与DDR控制完整比较。SDK2018.3 `-O2` 编译链接成功，没有 SDK stderr/编译警告；ArtifactsOnly通过，21个冻结输入与当前作者源、SDK编译源配对，17项关键host源码哈希匹配。

| 文件（相对任务根） | SHA256 |
|---|---|
| vivado/next_fpga.runs/impl_1/next_system_wrapper.bit | 21FEB85AE76C5AF1329E151A4A1872F50BFF8901DDB231BA8D453CB5D82B11B7 |
| vivado/next_system_wrapper.hdf | 5F813CCCB2D2B58AE016A65065816A7B7EA0F8410F17159DD3C5D25BD28D6E5E |
| sdk/next_benchmark/Debug/next_benchmark.elf | 1C8C503E11F76AC3E304BF8200ED32224120A2217EC0D11F89B08F2E28EF98C5 |

bit/HDF明确复用R8，未重新综合、布线、升级IP或修改PL。xc7z015clg485-2、next_system_wrapper、100MHz；复用路由WNS/TNS=+0.177ns/0，WHS/THS=+0.051ns/0，失败端点0，无时钟引脚/未约束内部端点0。LUT20452、FF23621、BRAM36等效tile61、DSP16；原44项DRC Warning保留，无Error/Critical Warning，包括REQP-1839数量上限，不能当作完整CDC证明。

最终ARM ELF实际定义main、next_describe_profiled、next_backend_profile_configure、next_frame_stats和BSP XTime_GetTime，`_end=0x0014d720 < 0x01000000`。默认后端MinGW目标文件仍为`26B09B685AA3DB02A82132CF31EBB1AC2755D5353F3ACFA8BD99CFB62BB86EB8`；默认编译关闭插桩，不把Profile代码混入默认固件。

## 记录与回读口径

原schema5 mailbox/MMIO/PL/特征ABI不变；私有DDR schema1在0x01010000，64字header，最多1000×32字记录。每完整特征四次真实PS计时器读数，方向矩、CORDIC含A32量化、BRIEF分别累计；每帧保存process/wall/verify/backend/cache/wait/config/prep、候选/选中数、读数和clock循环成本。ARM实际timer_hz=333333343，不是100MHz PL时钟。

Profile记录、百分位排序、校准循环及帧间发布在原帧计时区间之外；后端四次读数位于处理区间内。校准循环含循环和读取开销，不能直接从阶段计时扣除，也不是完整插桩开销的因果测量。这里的wall仍是每帧函数测量区间，不包含首次JTAG上传或全部帧间发布，不能称视频端到端延迟。Pipeline逐核时间继续只是PS延迟观测上界。

`board_profile_result.tcl`为纯解码器：要求主邮箱和私有header终态/模式/完成数/timer一致，先限制容量/地址/stride再读payload，检查逐帧完整特征和4次读数、区间嵌套、reserved、索引、nearest-rank P95/P99；成功结果还与主邮箱的整数平均、四舍五入微秒/min/max一致。FAIL仅完整成功前缀作为诊断，Production永不转成PASS。

`read_board_profile.tcl`只读目标：两个完整payload快照完全相同，且每个快照前后main/header不变；每次mrd最多256字，范围只允许既有mailbox和私有Profile区。无reset/stop/con/fpga/dow/mwr、无轮询和重试。稳定数据不自行证明ELF身份，必须关联明确配对job的下载日志。

纯decoder/reader测试64项PASS，含0/1/100/1000记录、失败前缀、uint64高位/统计溢出、错误布局/模式/计数/地址/容量、短读、header/payload不稳定、Production错误PASS和跨邮箱统计；全部硬件命令由mock替换，不转发。SDK2018.3自带Tcl不支持`string is entier`，第一次测试捕获后改为可容纳uint32的wideinteger，原失败日志保留；不能仅凭XSCT exit=0称测试通过，必须有验收标记。

读数作者源冻结证据：`optimization_build/profile_capture_sources_20261010_161606_750/source_manifest.json`。实际原始实体结果和CSV另见`BOARD_RESULT_PROFILE_R11_ZH.md`；本阶段不修改缓存/PL，不是有限优化流程的最终交付。

## 手动复现（仅再次明确实体授权后）

```powershell
$profileJob='C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_154719_ps_02a2d8c3'
$wrapper='C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd'
$psRoot='C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/ps'
pwsh -NoProfile -File C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/scripts/run_board.ps1 -JobRoot $profileJob -ArtifactsOnly
& $wrapper xsct "$psRoot/run_board_slow_jtag.tcl" $profileJob 100 Verified Pipeline
& $wrapper xsct "$psRoot/read_board_profile.tcl" C306ACCBABCD 100 Verified Pipeline
# Production只能COMPLETE_UNVERIFIED；每次重新运行前完成上一轮稳定capture。
& $wrapper xsct "$psRoot/run_board_quiet_jtag.tcl" $profileJob 100 Production Pipeline
& $wrapper xsct "$psRoot/read_board_profile.tcl" C306ACCBABCD 100 Production Pipeline
```

quiet helper只在con成功后把同步250ms观察延迟改成10秒，其他after调用透传；冻结runner的复位/DDR/bit/ELF、数值校验及deadline不变，JTAG仍1MHz。7项纯policy测试通过；若运行超过10秒仍可能遭到后续回读干扰，不能泛称“完全无侵入”。不修改默认runner或已有性能基线。
