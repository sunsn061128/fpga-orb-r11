# R11 起：基于 R10 的有界优化与定时交付

授权：2026-10-10 用户要求开始优化、采用定时检测、最终生成可上板文件。本任务不授权实体上板、JTAG 或非易失写入。R10 历史基线与所有旧 job 不修改；作者源可在 next 中继续演进，新构建只写独立 job。

## 最终交付合同

目标仍为 960×720、八层、参考结果一致；平均 25～28 ms 是优化目标，不是离线可以保证的实测结果。交付配对 bit/HDF/ELF、源码快照/哈希、CPU 精确测试与受影响 RTL 报告、路由时序/资源/警告、手动下载命令及待测清单。不能用未上板的新版本继承 R10 的 31.749 ms 性能。

PS-only 版本可复用已接受 bit/HDF，但必须明确标注；若修改 PL 则必须重新生成并验收 bit/HDF/ELF，不能混用旧 IP。最终离线交付后暂停定时任务，不自动进入实体板测试或扩展成完整视频/SLAM。

## 有限阶段，逐个推进

1. R11：先优化 CORDIC 循环的带符号除法为语义明确的无除法移位，保持数学/记录 ABI；增加独立 v1.0 方向边界和随机输入测试，新建 PS-only SDK job。查 ARM 机器码，确认是否消除循环里的除法，不能只凭源码推测性能。
2. 分项计时/测试：方向矩、CORDIC、BRIEF 分开计时并验证计时影响；为后续上板准备逐帧记录、P95/P99 和变化输入的 CPU 参考测试。保持 Production 只能 COMPLETE_UNVERIFIED，不能以其摘要零值充当正确性 PASS。物理 1000 帧/变化视频测试只登记待测，不自动执行。
3. 缓存：尝试基于实际特征访问的 raw/blur 缓存行集合维护，先读完已失效的点/摘要，再维护像素；处理重叠、对齐、非法样本、空点、复用工作区和旧缓存毒值，保留全范围 fallback。完整 Verified 图像/guard 扫描仍需全范围失效；Production 路径由主机独立观察器验收，不能取消完整结果比较来掩盖错数。
4. PL：优先评估 Gaussian/FAST 的搬运与扫描重叠、有限双缓冲；分别做隔离小步试验，至少覆盖相同十一 FAST / 十三 resize / 十四 Gaussian 事务与独立 CPU 比较，不能直接改生成 RTL。resize 行搬运 0..121-word 界限、86400-word 缓冲和非法参数/重启/padding/tail 保留。不存在确定收益证据时不要同时扩大 PPC、提频、重做融合或启用第二 CPU 核。
5. 交付：选已通过且当前源码匹配的 HLS job，显式 FrontJobRoot/ImageJobRoot 新建三核工程（Jobs 8），路由/SDK 完成后 ArtifactsOnly、基线历史哈希及报告验收；输出 DELIVERY_R11_PLUS_ZH.md 并暂停定时任务。资源/时序恶化或方案无效时记录并退回已验收候选，不盲目反复重跑同一修复。

## 每次定时执行规则

使用 fpga-development 技能，读根 AGENTS.md、REPAIR_R8_ZH.md、本文件和最新进展。只读一次当前新任务的状态/进程/已完成报告；任务活跃则结束本轮，不重复启动、不循环轮询、不长期等待。无变化不通知。

每轮最多推进一个有界阶段或一段基于日志的修复；主机检查使用原 MinGW 14.2、每项 120 秒 watchdog；Xilinx/SDK 2018.3 使用隔离 wrapper，无全局 PATH/WSL/自动 IP 升级。后台启动必须隐藏窗口、有明确 PID/任务目录和日志，已有同阶段进程不重复启动。综合/布线启动后告知估时即结束。长任务 watchdog 属于工作进程，不是聊天持续等待。

只在有意义进展、失败/阻塞、完成或需用户决策时通知。错误区分编译/工具启动失败、RTL 展开失败、算法不一致和时序失败，不相互冒充。保留原日志，标准注释覆盖新函数。任何基线文件哈希改变先停止调查；工作源变化属于新版本，不更新 R10 manifest。

## 当前进展

- 开始前：R10 146 个冻结文件及 61 个工作源/依赖哈希全部通过，无已有 vivado/HLS/xsim/SDK 编译任务占用。
- 已修改作者源 ps/next_backend.c，仅替换 CORDIC 的 next_signed_shift，仍精确向下取整并避免负数右移实现依赖；其他角度舍入/归一化、描述子/容量等不改。
- 新增 host/test_backend_angle.cpp，625 组边界、16641 组小矩网格、100000 组固定种子全 int32 随机对照独立 v1.0 golden.cpp；CMake 加入第 13 项测试。
- 首次主机编译失败：MinGW 报 cc1/cc1plus CreateProcess 文件不存在；实际两文件均存在。随后同版本独立 C/C++ 语法检查正常退出；记录为工具子进程启动异常，不是数值测试 FAIL。下一步一次完整有日志重试，不能跳过回归直接交付。
- 尚未生成 R11 ELF，尚未改 PL，尚未测试新版本板卡毫秒数。

### 首次编译诊断与定时恢复

原 fpga heartbeat 已通过应用工具更新为 ACTIVE，保持每 30 分钟、当前对话及原名称，改用本阶段交付合同；没有创建重复任务。恢复授权来自本次用户请求，旧 R10 基线文档中的“定时暂停”是登记时状态。

有日志的第二次 parallel=2 编译仍失败，这次为 as.exe CreateProcess，日志 optimization_build/r11_host_20261010_123105.log；实际 as.exe 存在，同版本直接 gcc -c 正常，同环境 CMake --parallel 1 完成七个编译/链接步骤。仅将 next 主机入口改成可指定 BuildJobs，默认 1，不改变工具版本；这是基于本轮诊断的保守绕开，不宣称启动异常根因已修复。

完整主机回归已通过 13/13，CTest 总耗时 24.85 秒；新增方向测试精确通过 117266 组输入。保存完整逐项记录到 optimization_build/r11_host_pass_20261010_123517_057.log。R10 冻结文件再次通过 146 项哈希检查；PS-only preflight 通过，确认 PL 与 MMIO/DDR ABI 未改变。

2026-10-10 12:35（北京时间）已启动独立 PS-only 后台构建，启动进程 PID 43708，父硬件 job 为 board_build/jobs/20261010_020213_a86e3550。预计 2～5 分钟，包括入口重新进行主机回归和 SDK 编译；本轮不等待。启动输出为 optimization_build/r11_ps_launch_20261010_123517_057.stdout.log / .stderr.log。新 job 由工作进程创建，实际路径以 board_build/last_ps_job.json 为准，必须核对本次时间和源码哈希；后续检查先确认该进程/新 job 状态，禁止重复启动。

后续在此追加任务路径、阶段和真实结果；检查 board_build/last_ps_job.json 时要比对本阶段源码哈希与时间，不能把旧 R10 PASS 当新 R11 完成。

### 13:02 定时检查：主机 Gaussian 偶发超时，SDK 未运行

本轮一次快照确认新 PS job `board_build/jobs/20261010_123517_ps_9e0f4cf5` 在 12:37:28 已 FAIL，阶段 host；进程 43708 已退出，无活跃 FPGA/SDK 工具。入口重新进行的 13 项回归中 12 项 PASS，`next_gaussian_stream_all8` 无输出并达到 120.03 秒 watchdog。它未进入 SDK、未生成新 ELF，不是已经观测到数值不一致；此前 12:32 的 13/13 PASS 保留为独立历史结果，不能代替本次失败验收。默认 R10 哈希检查仍为 146/146 PASS。

完整超时 CTest 记录另存 `optimization_build/r11_ps_fail_20261010_123517_ctest.log`，原启动日志和失败 job 不覆盖。本轮修复 `build_ps.ps1` 的失败证据收集：以后 host 阶段异常也保存 job 自有 `host_detailed.log`，避免下一次 CTest 覆盖唯一逐项证据。

不盲目重新启动同一个 SDK 流程。Gaussian 主机 bench 新增 main/reference/kernel/compare 的 stderr 刷新标记，只增诊断，不改 PL 作者逻辑、输入、十四事务或完整输出检查；vendor C/RTL 模式不输出这些标记。新增 `scripts/diagnose_gaussian_host.ps1`，原 MinGW 14.2 单目标串行构建后最多四次执行，每次 10 秒；第一次超时尝试附加 GDB 抓全线程栈（最多 10 秒），仅终止本 worker 拥有的测试/GDB PID，不扩大到其他任务。四次 PASS 也不能宣称偶发停滞根因已经修复；仍需新 PS job 完整 13 项回归作为交付门槛。两个 PowerShell 脚本语法解析 PASS。

13:05:04 已隐藏启动诊断 worker PID 41748，启动输出 `optimization_build/gaussian_diag_launch_20261010_130504_127.stdout.log` / `.stderr.log`。本轮结束不等待。下一次先读取 **optimization_build/last_diag.json 指定诊断 job 的 status.json、进程和完成日志，一次即可**；诊断活跃不重复启动。诊断失败按日志/调用栈继续一段修复；诊断通过则记录启动/十四事务证据，并只允许一次新的独立 PS 构建尝试，不复用失败 job，若仍超时先调查新的定位证据。阶段 1 尚未完成；后端分项计时、缓存与 PL 重叠阶段均未开始，没有任何新板卡耗时结论。

### 13:29 手动检查：诊断通过，新的 PS 构建已启动

一次状态/进程快照确认诊断 job `optimization_build/20261010_130504_gaussian_4716ee6c` 于 13:05:08 已 PASS，四次均 exit=0；每次都有十四条逐事务 PASS 和四十三条 main/reference/kernel/compare 刷新标记。完成日志与当前 executable/bench/kernel SHA256 一致。没有复现超时，因此没有 GDB 调用栈，也不能宣称停滞根因已解决。R10 默认冻结哈希 146/146 PASS；未有其他活跃 FPGA/SDK/诊断任务。

PS-only preflight 再次 PASS，PL/MMIO/DDR/点记录 ABI 保持不变。按上述诊断后的单次尝试规则，13:29:32 隐藏启动新的 PS-only worker PID 43324，父硬件仍为 R8 `board_build/jobs/20261010_020213_a86e3550`。启动日志 `optimization_build/r11_ps_after_diag_20261010_132932_313.stdout.log` / `.stderr.log`；新 job 路径由 worker 发布到 `board_build/last_ps_job.json`，不能混淆旧 12:35 FAIL 或 R10 PASS。预计 2～5 分钟，仍包含完整 13 项主机回归、SDK 和配对验收；本轮不等待、不操作 JTAG。

下一次只检查该新 PS job 状态、进程和完成报告一次。活跃则结束；失败先读 host_detailed.log 的定位阶段或 SDK 错误，不反复启动；PASS 后执行 ArtifactsOnly，核对冻结 next_backend.c 与当前 R11 源码/SDK链接并比较 R10/R11 ARM 方向函数机器码，完成阶段 1 后才进入分项计时准备。新 ELF 和板卡性能在本轮均未验收。

### 后续手动检查：阶段 1 离线验收完成

一次快照确认 `board_build/jobs/20261010_132933_ps_a191d754` 于 13:31:16 完成 PS_ONLY/PASS，无活跃构建进程。实际该任务完整主机 13/13 PASS（16.66 秒），含 117266 组独立方向比较；SDK 2018.3 `-O2` 编译链接成功。显式该新 job 的 ArtifactsOnly 验收 PASS，当前四个 PS 作者源与冻结/编译源一致。R10 默认冻结哈希 146/146 PASS，复用的四份路由报告与父任务哈希一致。

实际链接 ELF 反汇编确认：R10 CORDIC 循环区域六处静态 64 位除法调用点，R11 循环零处；完整函数调用点 11→5，余下循环外取模保持原样。不是动态周期、毫秒或新 FPS 测量。新的 ELF SHA256 `A5FC0FA01E8C63D300419DF7BE49E5F02C3EDE9C09FE68D83D6F7C7074D2130E`；bit/HDF 明确复用 R8，PL 没有新综合/RTL。详细阶段证据和候选配对文件见 `PS_CORDIC_R11_ZH.md`。

本轮完成阶段 1 的配对/机器码核验，不重复构建。**下一轮进入阶段 2：分项计时与变化输入测试准备**，保留本已接受 R11 候选及历史失败 job；不得跳回 R11 SDK 盲目重跑。计时默认关闭或独立 Profile 模式，先验证插桩不改变 CPU 精确结果及默认路径；尚未开始缓存/PL 修改。全阶段最终交付未完成，因此不暂停定时任务、不写最终完成声明、不自动上板。

### 用户随后授权的 R11 实体测试（不扩展给定时任务）

用户本轮明确“上板”：R11 单帧 Verified Serial PASS、100/100 Verified Pipeline 精确 PASS，摘要 `a1967dad68429025` 与 R10 一致；Production Pipeline 100 帧 COMPLETE_UNVERIFIED，平均 32.344 ms，30.917 FPS，最大 48.801 ms。后端 7.287 ms，相比历史 R10 12.858 ms 降约 43%，但剩余 PL 等待增加，整帧没有比 R10 的 31.749 ms 更快。不要盲目重复 R11 CORDIC 改动或继承局部速度为整帧收益，不替换 R10 固定性能基线；保留 R11 已验收候选，下一轮仍为阶段 2，之后 PL/DDR 搬运重叠是整帧优化重点。

首次 15 MHz 上传参考图像时 JTAG 调试口报错，算法未运行；只读核验恢复后，按 SDK 本地命令实现同会话 1 MHz 适配器时钟绕开，三次算法运行及三次稳定结果回读均成功。作者辅助脚本 `ps/read_board_link.tcl`、`ps/run_board_slow_jtag.tcl` 仅供再次明确授权后的人工实体流程；**定时检查不得调用 slow_jtag runner 或任何实体读写**。旧 job/冻结源码不修改，无非易失写入。完整证据与未覆盖边界见 `BOARD_RESULT_R11_ZH.md`。测试后 R11 ArtifactsOnly 及 R10 146 冻结哈希均 PASS。本次最后 ARM #0 已暂停，不再自动恢复。

### 13:49 定时阶段 2A：变化输入的独立后端验收准备

本轮只取一次任务/进程快照：R11 PS job 为 `20261010_132933_ps_a191d754` PASS，硬件/图像仍为已接受 R8，没有活跃编译或仿真；R10 默认冻结哈希 146/146 PASS。不上板、不改缓存/PL/PS 算法和公开 ABI，保留 R11 已接受候选。

新增 `host/test_backend_changing.cpp`，用独立 v1.0 `gaussian7/describe` 生成像素与描述子参考，不读取 PS A32 偏移表作为参考。八种层几何 × 六种变化图案（零、255、横/纵渐变、棋盘、固定种子噪声），变化点数量/坐标/输入顺序，复用同一完整 960×720 raw/blur 分配并毒化未用区；逐项比较所有特征字段和 256-bit BRIEF，检查图像只读、输出前后 guard 与未写尾部。每层追加空点复用，总 56 个合法事务、5624 个特征；测试要求覆盖全部 32 个角度 bin，并拒绝八种点/容量/层号/尺寸错误，首记录失败时输出不变。常量图像覆盖四个合法有效坐标边缘，随机方向使用足够旋转采样边距。覆盖的是后端主机契约，不是 FAST 选点、物理缓存、变化视频或 1000 帧板测；原 FAST11/resize13/Gaussian14 与完整集成比较保留。

CMake 新增第 14 项 `next_backend_changing_exact`；`scripts/validate_stage2.ps1` 新建独立日志/source-hash job，先串行编译/执行该焦点测试，再完整 14 项主机回归，每项 120 秒 watchdog，保存焦点和完整 LastTest.log。前后输入与冻结 SHA256 不匹配不能 PASS；MinGW 固定 14.2.0，无 SDK/JTAG/自动重试。下一轮同时读取 **optimization_build/last_stage2_job.json 指向的 status.json、完成日志及源哈希** 一次，不能把 R11 旧 13 项 PASS 当第 14 项通过。

PowerShell 语法解析 PASS。直接 g++ 语法命令再次出现 `cc1plus.exe CreateProcess: No such file or directory`，实际 40,262,144 字节编译器文件存在，驱动 `-###` 路径/参数正常，PATH 长度 3084 且没有 GCC_EXEC_PREFIX/COMPILER_PATH 覆盖；按驱动参数直接调用同版本 cc1plus 语法检查 PASS。这只是编译驱动子进程异常证据，不是算法 FAIL 或根因解决。接下来仅一次隔离 worker 使用已有 CMake/Ninja 串行构建路线；若仍 FAIL，保留 job 并按日志调查，不能盲目重复启动。

阶段 2A 启动后本轮停止，预计 1～3 分钟。通过后阶段 2 尚未全部结束：下一步是默认关闭的方向矩/CORDIC/BRIEF 分项计时及逐帧/P95/P99 准备，先验插桩精确性和开销，再进入缓存阶段。不得从本测试声明新 ELF、RTL、30 FPS 或完整最终交付已完成。

实际隐藏启动时间为北京时间 2026-10-10 14:40:56，PID 34308；启动日志 `optimization_build/stage2_launch_20261010_144056_211.stdout.log` / `.stderr.log`。本轮未查询该新 worker 的运行结果，任务目录由 worker 发布到 `last_stage2_job.json`，下轮一次读取即可。不得因尚未查结果重复启动。

### 14:42 定时检查：编译驱动路径的一次有界修复

一次快照确认 `optimization_build/jobs/20261010_144056_stage2_4656a5b8` 于 14:40:59 FAIL，阶段 focused_compile，未执行新的焦点测试或完整 14 项回归。十个作者/依赖文件的冻结和当前 SHA256 均匹配；无活跃编译/仿真，R10 冻结 146/146 PASS。失败记录保留，旧 R11 的 PASS 不能替代此任务。

失败日志是 GCC 驱动启动带多级 `../` 的 `as.exe` 路径报 CreateProcess；规范化后的原文件实际存在（1,845,760 字节），直接 `as --version` 正常，仍为同安装 GNU Binutils 2.42。驱动 `-###` 证实加 `-B` 后 cc1plus/as 使用同安装的规范绝对路径。原 MinGW 14.2.0、原测试源、原 -O3/-DNDEBUG，在独立生成目录 `optimization_build/compiler_path_20261010_144420_754` 用这两个前缀编译成功（exit=0，实际 object 存在，30 秒 watchdog）。不是更换编译器、数学测试通过或已经证明异常根因。

只改 `scripts/build_and_test.ps1` 与 `scripts/validate_stage2.ps1` 的 CMake C/C++ 工具查找前缀：`-B C:/Users/dmsun/mingw/mingw64/libexec/gcc/x86_64-w64-mingw32/14.2.0/ -B C:/Users/dmsun/mingw/mingw64/x86_64-w64-mingw32/bin/`。不改全局 PATH、编译优化/精确测试、作者算法或 RTL，继续串行构建；stage2 状态记录工具哈希与新 flags。新脚本解析通过后只启动一个新隔离 job，预计 2～5 分钟（工具前缀变化会重新编译）。下轮若仍编译 FAIL，按新证据调查，不重复相同改法；若焦点/完整回归 PASS，确认逐例比较和全部32 bin/sourcehash再推进默认关闭的分项计时。

实际启动于北京时间 14:45:12，隐藏 worker PID 3472，日志 `optimization_build/stage2_canonical_launch_20261010_144512_150.stdout.log` / `.stderr.log`，实际 job 由 `last_stage2_job.json` 发布。本轮未查询新任务完成状态、不等待、不重复启动。

### 14:49 手动检查：路径规范化没有解决启动异常

一次快照确认新任务 `optimization_build/jobs/20261010_144512_stage2_92048b40` 于 14:45:20 FAIL，阶段 focused_compile。C++ 生成器编译/链接及独立 CPU ROM 生成成功，随后原 MinGW gcc 启动已规范化的绝对 `cc1.exe` 路径报 CreateProcess/文件不存在。未运行焦点数值测试或全套 14 项回归。十个源码/依赖冻结哈希与当前文件、五个工具哈希均匹配；无活跃 FPGA/主机任务，R10 冻结 146/146 PASS。两个失败 stage2 job 全部保留；旧 R11 的已接受 ELF 不受影响。

**纠正前一轮推断：`-B` 只改变查找路径，没有解决本机启动异常，不能再次宣称有效修复或盲目启动同一 worker。** 本轮仅进行一次有界环境/启动诊断，不继续插桩/缓存/PL 开发。最近一小时 Application 和 Defender Warning/Error 查询无相关事件，CodeIntegrity 查询无匹配工具事件；不能据此排除安全软件。环境总约 6988 字符，空闲物理内存约 19 GiB，没有已观测到的巨大环境块或物理内存耗尽。cc1.exe 导入表仅列 Windows KERNEL32/UCRT API 模块，未列 MinGW 自带 DLL；不是 DLL 根因的完整证明。

新诊断记录 `optimization_build/compiler_env_20261010_145154_926/results.json`：同版本 gcc、同 C 后端源、相同 -O3/-DNDEBUG/-B 参数，两种受控环境分别只编译一次（继承 PATH 并前置 MinGW / 最小 MinGW+Windows PATH），各自 10 秒 watchdog，两次 exit=0 且生成实际 object。原 PATH 在 finally 恢复，没有全局修改，也没有安装/升级工具。两组都 PASS，未得到 PATH 污染的因果证据，不把单次直接编译成功当完整回归通过；未复制诊断 object 到 CMake 构建区。

本轮停止，**不启动新构建**，避免相同无效修复重跑。阶段 2A 尚未验收，分项计时尚未开始；下一轮应基于失败 job 中的真实 CMake/Ninja gcc 命令和 host_build 工作目录做有界启动差异/原生错误证据收集，而不是再改目录字符串或重新跑 SDK。需验证的差异是直接启动与 CMake/Ninja 子进程链及工作目录/实际参数，不能凭诊断成功归因。仍禁止定时 JTAG/实体操作，不修改基线和验收标准。

### 用户“继续”：文件重定向启动链的有界绕开

一次状态快照确认原 stage2 FAIL 不变，无活跃 FPGA/主机任务；R10 冻结 146/146 PASS。本轮仅修复并验证主机工具启动方式，不进入分项计时/缓存/PL，不操作 JTAG。

受控诊断 `optimization_build/compiler_stdio_20261010_145619_801`：用同 CMake/Ninja、同缓存/目标/源码/MinGW14.2/规范路径/-O3/-DNDEBUG/串行参数，工作目录为 host_build；区别是通过隐藏 Start-Process 启动、stdout/stderr 先重定向到独立文件，而非 PowerShell 原生管道接 Tee。实际失败过的 gcc C 后端编译、归档、两个 C++ 编译和最终链接五步均 exit=0，保留 verbose 命令和日志。仅说明这一启动方式本次可工作；输出管道是待确认相关因素，不是已证明的异常根因。

新增 `scripts/host_process.ps1` 共用启动器，改 `build_and_test.ps1` 和 `validate_stage2.ps1` 调用：原生工具输出先写独立日志，完成后读取日志文本，再由 PowerShell 收集。参数逐一引用并拒绝无法安全引用的参数，不更换工具/优化级别、不重复启动失败命令；现有日志不能覆盖。非零退出抛错，配置 120 秒/编译 300 秒/焦点 CTest 总 150 秒/全套 CTest 总 1800 秒 watchdog，每项测试仍保留 CTest 120 秒。超时仅终止启动器拥有的 PID/子树。stage2 冻结并校验新增启动器；失败时保存焦点或完整 LastTest 证据。新增 `-LogRoot` 用于 job 自有原生日志。

三份脚本语法解析 PASS；启动器用 CMake -E 的参数含空格透传和 exit=1 拒绝测试通过，记录 `optimization_build/host_launcher_test_20261010_145906_202`；另做一秒 owned-process 超时测试，要求 TIMEOUT 状态且该 PID 已退出。尚未宣称 56 事务数值比较或 14 项回归通过。按新启动链只启动一次新的独立 stage2 worker，预计 1～3 分钟；本轮不等待，下一轮核对新的 `last_stage2_job.json`、源哈希、完整原生日志和逐例比较。若仍工具启动 FAIL，不能循环盲目重试或把诊断成功当交付门槛。

实际启动北京时间 15:00:01，隐藏 PID 36892；日志 `optimization_build/stage2_filelaunch_20261010_150001_763.stdout.log` / `.stderr.log`。超时回归日志 `optimization_build/host_launcher_timeout_20261010_145958_468` 为 PASS，确认仅终止 owned CMake 测试进程。本轮不查询新 worker 状态，job 路径以 worker 发布的 last_stage2_job.json 为准。

### 后续手动检查：阶段 2A PASS，推进 2B 分项计时接口

一次快照确认 `optimization_build/jobs/20261010_150002_stage2_73164a0c` 于北京时间 15:01:25 PASS，耗时约 83 秒；无活跃构建。十一份源码/依赖的冻结与工作哈希一致。焦点测试 1.11 秒 PASS，完整 14/14 回归 29.27 秒 PASS；已读取实际 48 条变化图案逐例 PASS 与汇总，含每层空点，总 56 事务、5624 特征、32 bin、8 拒绝用例，独立方向 117266 组 PASS；原 resize13/Gaussian14 仍通过。完整主机构建实际执行 40 步编译/链接且 stderr 无错误。R10 冻结 146/146 PASS。新启动方式本次完整验收成功，但仍不宣称偶发启动/停滞根因已根治；所有旧 FAIL 保留。

本轮只推进阶段 2B 的后端计时接口/精确性小步。`ps/next_backend.c` 加编译期 `NEXT_BACKEND_PROFILE=1` 保护的三个区间（方向矩、CORDIC含A32量化、BRIEF）；默认编译不包含 profile 存储、分支或时钟调用。原 `next_backend.h`、PL/MMIO/DDR/特征/mailbox ABI 不改。新增独立 `next_backend_profile.h` 和重命名 profile library，不绑定 ARM 定时器，也不改变板卡默认模式；所有新函数有标准注释。接口单核非重入，调用方拥有统计区/clock/context，成功配置清零，配对 null 关闭，半空配对拒绝；每完整特征四次时钟读取，三段无符号差值允许64位回绕，失败采样不记完整特征，统计不进入已有 PASS/FAIL 记录。

重用同一变化输入测试，新增第15项 `next_backend_profile_exact`：相同 CPU 独立参考、guard/tail/非法/空点；固定七 tick 假时钟覆盖计数、分段边界及回绕，关闭时不读时钟/不改统计且与普通函数完整64字节一致。这里的 fake ticks 仅为计时契约测试，不能报告 host/ARM 的真实周期或插桩开销。worker 要求普通/profile 两项焦点与完整15项均 PASS，同时要求默认后端 host object 与修改前 SHA256 `26B09B685AA3DB02A82132CF31EBB1AC2755D5353F3ACFA8BD99CFB62BB86EB8` 一致，证明此工具配置下 default-off 不改变目标文件；如果不一致先查机器码/flags，不伪称数值错误。

新 worker 冻结 profile header、库作者源和测试/启动器，现行完整门槛升为15项。PS作者源虽然添加了 default-off 宏区，旧 R11 ELF 仍对应旧冻结源，不把新作者源冒充已重新链接；尚未新 SDK/RTL/板测。本轮启动新的独立 host job 后停止，预计1～3分钟，下次一次核对 `last_stage2_job.json` 的新 `STAGE2_PROFILE_HOST`、源码/默认对象哈希、15项与profile记录。通过后还需准备真实时钟读数开销/逐帧/P95/P99以及ARM绑定的独立Profile工件，再完成阶段2；缓存/PL尚未修改。

实际隐藏启动北京时间15:08:24，PID5060，日志 `optimization_build/stage2_profile_launch_20261010_150824_732.stdout.log` / `.stderr.log`；本轮未再次读取新任务状态，下一次以新 job 的实际结果为准。

### 用户“继续”：阶段2B通过，推进2C逐帧统计与真实host计时开销

一次快照确认 `optimization_build/jobs/20261010_150825_stage2_7752b9d4` 于北京时间15:08:50 PASS，焦点2/2（1.72秒）、完整15/15（14.10秒）。十二份输入的冻结/当前哈希一致；默认后端目标文件仍为 `26B09B685AA3DB02A82132CF31EBB1AC2755D5353F3ACFA8BD99CFB62BB86EB8`，计时编译关闭没有改变此MinGW配置的对象。已读取普通/profile的56事务5624特征CPU精确记录与profile 22496次读数、三段、回绕及关闭覆盖。无活跃构建，R10冻结146/146 PASS。旧R11 SDK和RTL PASS并非新profile交付证据。

本轮只推进阶段2C：新增纯C `next_frame_stats`（不绑定板卡），最多1000个实际帧的total/min/max和nearest-rank P95/P99，均值为total/count；排序在处理区间外、调用方scratch、不分配堆，空/非法/溢出返回错误且不改结果。覆盖1000逆序、1/2/3/100相同样本、零、UINT64_MAX、sum溢出及空指针/计数越界，不改已有mailbox/MMIO/DDR ABI。不是1000帧板测。

新增第16项 `next_profile_frames_contract`：六种确定性变化图案、八层，独立v1.0 CPU预计算blur和全64字节参考；100次host帧，每层16点（每帧128点），按奇偶轮换普通/profile先后，warmup和输入/参考/逐帧比较不纳入计时。200个A/B后端帧共25600特征精确比较；实际steady_clock纳秒记录三段/总耗时，检查51200次读数及分段和不超过所属frame区间。保存100条原始帧CSV，并打印默认/profile均值/P95/P99、A/B差值及512次clock读取循环成本；**clock循环含循环开销、不直接扣除，A/B可能噪声甚至负值，不设速度门槛、不外推ARM周期或30FPS**。这些是合成选中点的后端host帧，不包含FAST选点或PL/缓存/完整视频。

stage2 worker冻结15份输入并要求三项焦点及完整16/16、默认对象未变、全部原精确测试和100帧CSV。每项120秒watchdog；焦点总390秒，完整总2000秒，无自动重试。当前尚未执行新的C/C++构建或测试，脚本解析后只启动一个隐藏独立任务，预计1～3分钟，不等待、不读第二次状态。本轮不修改PL、不进行SDK或JTAG；通过后阶段2仍需独立ARM Profile固件绑定和逐帧DDR接口/变化输入待测准备，不能提前称最终配对交付完成。

实际脚本语法解析PASS；北京时间15:19:54隐藏启动PID42372，日志 `optimization_build/stage2_frames_launch_20261010_151954_748.stdout.log` / `.stderr.log`。新job由worker发布到 `last_stage2_job.json`；本轮未查询新任务结果，下一轮只取一次状态。禁止因尚未读取结果重复启动。

### 阶段2C PASS，阶段2D独立板端Profile作者源及验收准备

一次快照确认 `optimization_build/jobs/20261010_151955_stage2_9c06bafa` 于北京时间15:20:14 PASS（约19秒），焦点3/3 1.50秒，完整16/16 10.44秒；15份冻结/当前输入哈希匹配，无活跃工具，R10冻结146/146 PASS。100条CSV已保存。独立CPU精确比较25600特征，51200次profile clock读取；完整方向/变化输入/FAST11/resize13/Gaussian14/PS错误注入仍通过。此轮host默认184160ns/profile188950ns，A/B均值差4790ns（2.601%），clock循环13520ns；是电脑端128特征后端工作量，不是ARM/PL/完整帧测速或可外推开销，原数据保留。

用户在本轮明确“可以上板”。记录该授权用于当前独立Profile固件通过后的人工作业：只允许目标ACZ7015/xc7z015的SRAM/DDR测试，不包含Flash/SD/eFUSE，仍需当时核验目标/电源/连接/安全及配对工件；**不把这句话扩展成定时任务无限自动JTAG授权，heartbeat仍只构建/检查**。旧已测R11不重复冒充最新Profile；当前还没有Profile ELF，不执行实体下载。

本轮只推进阶段2D的作者源/主机验收。默认 `board_benchmark.c` 中所有新hook受 `NEXT_BOARD_PROFILE` 编译保护；独立SDK入口 `board_profile_sdk.c` 包含冻结benchmark的.inc副本，`backend_profile_sdk.c` 独立重命名profile后端。已接BSP XTime_GetTime（host用同名mock）、每frame重置计时器/统计，完整特征四次读数。保留默认算法、所有原校验和Production语义，不改PL/MMIO/原mailbox布局；当前SDK构建/配对脚本尚未扩展，两个wrapper不是已经生成的程序。

新增私有PS DDR契约 `board_profile_contract.h` / `board_profile.inc`：0x01010000，64字header+1000×32字frame（128256字节），处于原mailbox之后/输入PGM之前；原程序_end检查仍在。记录process/wall/verify、moments/CORDIC含A32/BRIEF、backend/cache/wait/config/prep和clock读取循环、候选/选中数/完整特征/读数/frame index，64位tick按低/高字存，不伪称PL内部周期。准备和逐帧record/calibration在原frame计时外；profile本身四次读数包含在backend内，不直接扣除。全部成功记录和P95/P99先flush，再写并flushterminal status；失败仅提交完整成功帧前缀，RUNNING没有已提交records读取合同。Production侧仍只COMPLETE_UNVERIFIED。

第17项 `next_board_profile_control` 重用完整PS串行/流水/独立CPU结果观察与全部错误注入，同时检查私有header状态/模式/完成数与mailbox、逐帧字段、读数、分段/全帧时间以及独立排序的P95/P99；另用人工ticks写满1000记录，检查越界不写、tail guard、重启清count及错误读数拒绝。这不是1000帧实体/C模型运行或真实cache模型。默认PS控制测试继续存在；worker冻结24份输入，默认后端object哈希门槛保留，焦点4项/完整17项各120秒，总watchdog510/2140秒，没有自动重试。

脚本解析后启动一次新独立host job，预计1～3分钟，本轮不等待。PASS后下一轮仅扩展独立Profile SDK/配对与稳定DDR读取工具；须新SDK通过、ARM链接核验与ArtifactsOnly后才可执行当前人工授权的实体测试。缓存/PL阶段尚未开始，最终交付尚未完成，不暂停heartbeat。

两份PowerShell脚本语法解析PASS；北京时间15:35:47隐藏启动PID40912，日志 `optimization_build/stage2_board_profile_launch_20261010_153547_704.stdout.log` / `.stderr.log`。新job由worker发布，未查询第二次结果，不重复启动。尚未新SDK或实体操作。

### 用户“检查”：阶段2D host PASS，独立Profile SDK生成/配对阶段

一次快照确认 `optimization_build/jobs/20261010_153548_stage2_515e6291` 于北京时间15:36:41 PASS，24份冻结/当前输入哈希匹配，焦点4/4 8.79秒、完整17/17 32.73秒；已读Profile终端/容量/越界/重启、默认PS完整错误注入的验收记录。无活跃主机/vendor任务，R10冻结146/146 PASS。C++ host仅一条enum/uint32条件表达式Warning，本轮作者源显式uint32 cast消除，下一SDK job会重新完成17项主机回归，不把修改前PASS冒充新源测试。

本轮仅推进Profile SDK生成及配对这一有界阶段，不改缓存/PL、不执行实体操作。`build_ps.ps1 -Profile` 新建PS_ONLY job（保留Profile=true标签，明确复用R8 bit/HDF），先17项回归，记录关键host源前后SHA并核对冻结SDK输入与实际测试源一致；原生启动器靠异常/完整标记判断，移除不可靠的Start-Process之后LASTEXITCODE假设。job自有原生host日志，冻结七个Profile新增源码/头文件，默认不带-Profile时不复制这些输入。

`build_board_sdk.tcl` 只从冻结输入选择入口：默认仍编译原board_benchmark.c，Profile将该文件字节原样复制为board_benchmark_source.inc并编译独立board_profile_sdk.c作为唯一main；另编译backend_profile_sdk.c和next_frame_stats.c，不新增全局编译参数/不升级IP/SDK。缺失/半套Profile输入在创建workspace前拒绝，已有workspace拒绝覆盖。独立.profile_sdk_success仅实际SDK编译成功后写入。Artifact检查保留全部父bit/HDF/源配对门槛，加Profile标签/七文件/原benchmark.inc精确配对、三个真实.o、最终ARM ELF定义符号main/next_describe_profiled/configure/stats/XTime_GetTime和_end低于mailbox。默认旧job仍可ArtifactsOnly。

两份PowerShell脚本解析通过；SDK生成纯mock `optimization_build/sdk_mapping_20261010_154528_447` 11例PASS（默认/Profile、七种缺失/半套、两种覆盖拒绝），未调用SDK构建/JTAG，dummy ELF不是真实固件。新Profile preflight通过，当前PL/MMIO/记录ABI与R8一致。第一次PowerShell解析捕获换行语法错误，已在作者源修复后重新解析通过，未启动错误脚本或污染旧job。

按已通过映射/preflight只启动一次新的独立PS Profile构建，预计2～5分钟（17项host+SDK2018.3+ArtifactOnly），本轮启动即停止，不等待、不循环查新结果。下一轮一次读取last_ps_job的新时间/Profile标签、完整17项、SDK日志/.o/ARM链接及输入哈希，不能用旧R11 ELF替代。PASS后再完成稳定DDR读数工具/无板mock，随后当前人工上板授权才具备执行条件；本轮没有Profile实板耗时，旧R10性能不继承。

实际北京时间15:47:18隐藏启动PID34784，日志 `optimization_build/profile_sdk_launch_20261010_154718_333.stdout.log` / `.stderr.log`；新job由last_ps_job发布，本轮没有第二次查询或JTAG操作。

### 15:51定时检查及用户随后明确要求“尽快上板测试”：阶段2接受

本轮首先只取一次任务快照：`board_build/jobs/20261010_154719_ps_02a2d8c3`在15:48:48已PS_ONLY/Profile/PASS，无活跃编译；实际17/17 host（17.54秒）、SDK2018.3-O2和ArtifactsOnly通过，21冻结输入/17关键host源码匹配当前作者源，真实ARM timer/profile/stats链接定义与_end边界通过。bit/HDF明确复用R8，不重新综合。R10冻结146/146 PASS；并非旧R11/R10状态冒充新Profile。

增加纯schema1 decoder及bounded稳定reader，64项无板mock PASS：容量/布局/主邮箱/timer/模式/终态、0/1/100/1000记录、uint64/溢出/失败前缀、Production永不PASS、百分位及主邮箱统计、短读/不稳定payload/metadata和错误目标。Tcl2018.3不支持entier的首次失败已修复为wideinteger，原失败日志保留。Source证据`optimization_build/profile_capture_sources_20261010_161606_750`。新reader不能自行证明ELF身份，要求配对下载日志。阶段细节见`PS_PROFILE_STAGE2_ZH.md`。

用户随后“尽快上板测试”覆盖本轮离线-only动作范围，允许本次人工实体测试，不扩展给heartbeat。确认原xc7z015/线缆C306ACCBABCD和DDR读数，SRAM/DDR运行301帧：Verified Serial1及Pipeline100精确PASS，digest与R10一致；Production两组各100仅COMPLETE_UNVERIFIED。仍同一归档图，不是变化视频/1000帧。最后CPU#0由runner停止，未写非易失存储，测试后Artifact和R10冻结再验PASS。

Profile原250ms JTAG回读：Verified Pipeline平均41.829ms，Production平均32.429ms/P99 49.798ms/max51.770ms。新增纯policy七项mock通过，人工同Profile/输入/硬件的10秒延后回读组Production平均30.864ms/P99 30.875ms/max30.876ms，100帧均在33.333ms内。消失的周期尖峰支持调试观察干扰，不是新的算法加速；不改R10历史性能/manifest，不称变化视频达标。quiet helper仅manual1..100，冻结runner/精确验收/timeout保持，超10秒运行仍可能有观察影响。

quiet主要分项：PL等待20.522ms、backend7.053ms、cache3.241ms、config0.026ms；backend内部moments2.096/CORDIC0.580/BRIEF4.307ms，clock循环约0.050ms未扣除。完整实体日志、逐帧CSV和边界见`BOARD_RESULT_PROFILE_R11_ZH.md`。默认backend对象SHA仍26B09B685AA3DB02A82132CF31EBB1AC2755D5353F3ACFA8BD99CFB62BB86EB8。

**阶段2的有限准备/独立固件/稳定回读已经接受；下一heartbeat进入阶段3的一段有界缓存行集合主机实验，先检查当前job和hash一次，不重跑阶段2/SDK，不调用任何live JTAG helper。** 此次实体实验额外补充不改变1000帧/变化视频待测边界。PL/缓存作者实现尚未改变，阶段3/4/5和DELIVERY_R11_PLUS未完成，因此不暂停定时任务或宣称最终优化交付完成。

### 阶段3A：缓存行规划器的独立主机验收准备

用户确认继续。一次状态/进程快照：最新Profile PS任务仍为20261010_154719_ps_02a2d8c3/PASS，旧stage2 PASS；没有stage3 job或活跃工具。R10冻结146/146 PASS。现有固件、PL、CMake17项门槛保持不变；本阶段不SDK/JTAG，不更改已接受工件。

作者源`include/next_cache_plan.h`/`ps/next_cache_plan.c`为单核、调用方拥有的约5.4KiB双图位图：32字节缓存行，raw覆盖独立方向矩行，blur使用所有32角度的固定逐行包络。一次初始化包络在帧计时之外，避免从尚未失效的raw中读取角度；安全几何/分配/对齐/重叠/点界限不满足或要求完整校验时，整份计划清空并FULL回退。连续缓存行合并，无堆分配，不自行操作BSP/barrier。必须先完成DMA、维护点和摘要后才可传点。本阶段尚未接入board_pipeline或固件，不冒充新可上板版本。

新增`host/test_cache_plan.cpp`：独立v1.0常量和运行时旋转验证包络、raw精确缓存行以及所有bin的blur覆盖；八层×六变化图案及空点复用，56事务5624特征、32bin、40稀疏/8边缘FULL/8空点；独立DMA内存和CPU旧毒值只刷新计划中的整行，随后调用真实C后端，完整64字节记录和输入/输出guard/tail必须精确。23项非法/容量/对齐/重叠/溢出/部分计划回退接口检查，另33×33合法常量边缘FULL和归档图八层真实选中点。完整960×720图像分配保留；这些是覆盖模型而非真实cache/dirty writeback/barrier/AXI证据。

`scripts/validate_stage3.ps1`新建独立冻结源/依赖/参考图/工具哈希job，焦点C/C++编译和测试各120秒watchdog，文件重定向启动、MinGW14.2、无自动重试。然后完整17项主机回归，校验源码前后哈希及default-off backend对象原哈希；保存逐例日志、归档点维护字节数/范围调用数。维护量减少不是ARM毫秒收益，后续接入前须评估规划/调用开销；完整Verified仍需全范围维护。下一次读取`optimization_build/last_stage3_job.json`的新任务及完成报告一次；若运行中结束，不重复启动。FAIL先按日志修复一段，不能用stage2或旧SDK PASS代替。本轮仅解析脚本后启动一次后台job，预计2～5分钟，启动后停止，不等待结果。

PowerShell语法解析PASS；北京时间2026-10-10 17:01:38隐藏启动worker PID35616，日志`optimization_build/stage3_cache_launch_20261010_170138_264.stdout.log`/`.stderr.log`。本轮未读取新job状态或执行第二次检查，job路径由worker发布到last_stage3_job.json；源码尚未C/C++验收，不宣称缓存优化已通过。正确性/配对完整验收后才暂停定时任务，当前仍仅阶段3A。

### 17:02定时检查：阶段3编译启动链的一段有界修复

一次任务/进程快照确认`optimization_build/jobs/20261010_170138_stage3_64b6105c`于17:01:42 FAIL，阶段focused_compile；gcc驱动无法CreateProcess启动确实存在的同版本cc1.exe。85份冻结/当前输入哈希一致，无活跃工具，R10冻结146/146 PASS。没有开始缓存覆盖/特征比较，不能说算法FAIL，也不能用旧17项或Profile SDK PASS替代。

只修复缓存实验的编译启动方式，不改作者算法、固件、PL或验收门槛：新增独立`scripts/cache_plan_host/CMakeLists.txt`，以显式冻结next/独立CPU路径创建job自有CMake/Ninja串行项目，与已通过全套主机回归的进程链一致，保留MinGW14.2及Release -O3/-DNDEBUG/-B。每次configure/compile/test各120秒，不做自动重试；原失败job全部保留。

一次针对性诊断`optimization_build/cache_cmake_diagnostic_20261010_170528_779`中C/C++14.2编译器ABI真实编译通过，但在源路径处理遇到Windows反斜线`\U`转义错误；诊断保留，尚未编译planner目标。模板用TO_CMAKE_PATH规范化两个显式冻结根目录，避免再解析原生路径；不手改生成工程。脚本语法解析通过后只启动一份新的隔离stage3任务，预计2～5分钟，启动后停止，不查询新结果。此轮仅启动链修复，数值/流量/完整17项仍待新job验收；若新链再次失败，调查新证据，不盲目重复相同修复，不接入固件。

### 后续手动“继续”：专项通过，但完整回归发生测试启动拒绝

实际新CMake任务`optimization_build/jobs/20261010_170656_stage3_f4a754fa`由PID33812于17:06:56启动，17:08:08在full_host阶段FAIL；86份输入前后/冻结哈希一致，R10冻结146/146 PASS，无活跃构建。CMake路径/编译链确实通过，焦点65条逐事务PASS、5624特征精确、全部32bin、23接口拒绝、33×33边缘和归档八层280点通过，未启用固件缓存。默认backend对象仍26B09B685AA3DB02A82132CF31EBB1AC2755D5353F3ACFA8BD99CFB62BB86EB8。

归档记录含一个小尺寸边缘额外样本：733472/4282944字节、5984范围调用；剔除该样本后真实八层是731232/4280704字节、5982调用，即维护量约减82.9%。范围调用由整图16次大增，规划/每范围BSP调用/barrier成本可能抵消维护减少，不能推出ARM节省83%或毫秒/FPS收益。完整Verified仍不允许用该稀疏计划代替全图/guard比较。

完整17项中16项PASS，仅next_backend_changing_exact为BAD_COMMAND/Not Run，CTest报告`Process not started [operation not permitted]`且耗时0.00秒；不是数值不一致/测试超时。原host.log/host_detailed和启动日志已保留。未伪称17/17或混合不同执行的PASS为新job接受。

本轮只做一次启动诊断阶段：文件哈希EBA39D7B5293984F5F7945B1C3A99B96C5A202600354A066D84C36AAAF6C804A与历史已通过changing程序一致，当前用户FullControl/ReadExecute、无Zone.Identifier；Defender/AppLocker近15分钟无匹配Warning/Error，CodeIntegrity仅svchost/Bonjour/NI DLL签名事件，不能据此断言根因或安全软件无关。记录`optimization_build/cache_test_launch_diagnostic_20261010_172103_544`：首次诊断在启动器空Arguments参数绑定处拒绝，未启动程序；不修改安全配置或搬移/改名程序绕过限制。改用原生无参数Start-Process，`cache_test_direct_20261010_172141_421`实际exit0、完整5624特征标记PASS且前后EXE哈希一致。单次CTest焦点`cache_ctest_diagnostic_20261010_172212_411`亦1/1PASS、0.26秒，保存独立LastTest记录。不能称根因已修复，也不能代替新的完整17项。

基于两种实际启动成功证据仅允许一份新的独立stage3验证任务，保持源码/编译器/数值门槛，无自动重试。启动后停止，预计2～5分钟；若再次BAD_COMMAND，必须调查进程启动/系统级证据，不循环重跑。当前不集成缓存，不开始PL，也不SDK/JTAG。下一轮读取新last_stage3_job一次，只有完整接受后再推进范围调用开销评估和有界集成。

实际北京时间17:23:07隐藏启动PID28848，日志`optimization_build/stage3_after_launch_diag_20261010_172307_164.stdout.log`/`.stderr.log`；本轮未再次查询新任务状态，不重复启动。最终交付尚未完成，定时任务继续保留完成后暂停的停止条件。
