# ORB60 4PPC / 2018.3

当前已验收基线固定为 **u1**：[封存包说明](../u1/README_ZH.md)。125 MHz、normal 960×720 八层、280 个特征，100 帧 Production 的整帧程序循环为 6.31751 ms，即 **158.2902 FPS**。bit/ELF、原始编译输入、作者工作区及板测证据独立封存；整包清单与指针见 `../u1/MANIFEST.json`、`evidence/baseline_u1.json`。历史 r17/r16/r15 继续冻结。后续比较默认使用 u1，并保持相同输入、预算和整循环计时口径。

u1 在 r17 的 125 MHz 实现上，把描述子 raw/blurred AXI 未完成读请求窗口由 2 增至 8，并加入 HLS latency=64 调度提示，较 r17 的 143.2737 FPS 提升 **10.48%**。native、完整 C/RTL、独立 AXI/reset 和 8 请求排队长延迟验证通过；normal Verified 100、noise/zero Verified 各 10 帧精确校验通过，Production 保持 COMPLETE_UNVERIFIED。125 MHz routed WNS +0.157 ns、WHS +0.046 ns，配对 bit/ELF 和包内启动器已实际上板验收。原发布结果见 [20261013_194107 结果](../orb60_board/20261013_194107_4ppc125mhz_2018_3/RESULTS.md)，原始证据见 [r17 优化试验](evidence/r17_trial.md)、`evidence/r17_prefetch64_result.json`；报告中的 r17 比较基线保留历史含义。

这是从已上板的 100 MHz ORB60 源码独立复制并继续优化的 4PPC 工程。目标器件为 ACZ7015 的 `xc7z015clg485-2`，整机顶层为 `s_wrapper`，当前图像和描述子核共用 PS FCLK0 的 125 MHz 时钟。原来的 2PPC 发布包保留在实验目录之外。

## 数据通路

输入为 960×720 灰度图，八层预算依次为 217、181、151、126、105、87、73、60。FAST/NMS、Gaussian 和 Resize 的主要像素循环按四个像素并行展开，并在 Vivado HLS 2018.3 下取得 II=1。FAST 的四路候选按栅格顺序合并，保持原来的排序和选点语义；缩放保持原来的系数、逐级舍入及跨行打包规则。

描述子的方向量化和 BRIEF 数值语义保持一致。已发布起点用有限深度的字 FIFO 替代 2018.3 不支持的 `stream_of_blocks`。r15 改为 DDR 直接填充两个 PIPO bank，令 patch 加载与上一点计算重叠；r16 把 raw/blur 的行读取改为固定长度分支并流水化，HLS 实际 II 为 raw 9、blur 8；u1 进一步提前发起读请求，并扩大 raw/blur 的未完成读请求窗口。作者输入是 `descriptor_patch2018.hpp`，生成器为 `scripts/migrate_descriptor.py`。当前 normal 程序循环实测为 158.2902 FPS。每个核保留四个 64 位 AXI 主接口；对应主接口经仲裁共享四个 PS HP 端口。PS 使用三个 raw 缓冲、两个 blur 缓冲，令 descriptor(n) 与 image(n+1) 重叠。

PS 首层现在读取生产者移交的对齐、不可变 DDR 输入，省去每帧 CPU 复制；后续层的缓冲轮转保持原有安排。板测使用常驻 DDR 的固定输入，程序循环吞吐不含外部采集或传输；实时生产者需要提供等效的所有权和缓存一致性移交。

## 源码和生成文件

- 四路图像逻辑：`upstream/FPGA_ORB_HLS/fastv2/next/src/layer4_hls.cpp`、`front4_stream.hpp`、`layer4_images.hpp`。
- 兼容代码和缩放 ROM：`scripts/migrate_image.py`、`migrate_descriptor.py`、`generate_quad_rom.py`。修改生成器，不直接修改生成结果。
- 直接填充描述子 patch：`upstream/FPGA_ORB_HLS/fastv2/next/src/descriptor_patch2018.hpp`。
- 整机连接、PS 配置和路由验收：`vivado/`。
- PS 调度、精确比较和 SRAM 下载：`ps_board/`。
- 独立 CPU/RTL 验证：`tests/`、`verification/`。`evidence/upstream_snapshot.json` 记录复制来源的哈希。

`build/` 及 `C:/h/orb4/` 是生成目录。每个 HLS、AXI、Vivado 和板测任务有独立日志、状态及输入哈希；状态必须结合实际报告复核，不能只看厂商启动器的返回码。

## 执行入口

使用 `C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd` 执行 Python 脚本。厂商命令通过同目录的 `xilinx-2018.3.cmd` 运行，避免修改全局 PATH。

1. `scripts/host.py`：CPU 精确比较、边界及完整帧串接。
2. `scripts/hls.py --kernel layer4|descriptor --mode export --period-ns 8`：C 仿真、综合、完整 Verilog 联合仿真及 IP 打包。
3. `verification/run_datapath_axi.py`：显式指定 HLS 任务、向量、独立输出目录、用例和复位用例；真实 RTL 的随机背压、延迟响应、4 KB 边界、越界保护与完成屏障。
4. `scripts/board_build.py --frequency-mhz 125`：显式配对两个 IP、AXI 证明及完整联合仿真任务，生成 125 MHz 整机。只有 setup/hold、时钟覆盖、全网路由及 DRC 通过才产生接受标记。旧发布起点通过 `scripts/board_route.py --from-job <failed full board job> --carry-remap 2 --no-fanout-opt` 解决装箱失败，从同一个完整链接综合网表另开物理实现任务；该重试不生成新 HDF。新任务以自身布局、路由报告为准。
5. `ps_board/build.ps1 -JobRoot <accepted job>`：新导出寄存器 ABI、调度 mock、ARM ELF 和 bit/ELF 哈希配对。仅在完整 PS 配置一致时复用 2018.3 PS standalone 库。
6. `scripts/board_test.py --job <accepted job> --frame normal|noise|zero --frames N --mode Verified|Production`：检查配对及完整联合仿真后，按用户授权执行易失性 SRAM 下载。Production 同一输入需要先通过至少 100 帧 Verified。
7. normal 的 Production 可以加 `--rerun`，仅在当前终态邮箱与紧邻的成功 normal 会话完全一致时复用其配置。`scripts/board_release.py --job <accepted job>` 在 normal Verified/Production 各 100 帧、noise/zero Verified 各 10 帧实际通过后，导出带相对路径哈希、报告和复测启动器的发布包；位置记录于 `evidence/latest_release.json`。

Verified 会比较图像、选点、摘要、描述子及未写区域；Production 用于测量正常调度吞吐，结果标为 `COMPLETE_UNVERIFIED`。内部循环 II、核延迟和整帧上板吞吐是不同指标，性能结论以板测数据为准。该实验不写 Flash、SD 或 eFUSE。

2018.3 IP 打包在 2026 年触发日期版本号溢出时，脚本只在独立生成目录内把 `run_ippack.tcl` 的 revision 改为 1 并重打包，保留原始失败及成功重打包日志；不修改安装目录。

PS 程序使用新 IP 的导出寄存器头文件并检查 4PPC 标志。复用 PS standalone 服务库之前比较完整 876 项 PS 配置及 HP/FCLK 覆盖项；使用本次新工程生成且与已配对 PS 初始化文件逐字节一致的 `ps7_init.tcl`。最终性能及普通 DRC 警告以发布包中的 `RESULTS.md` 和原始报告为准。

## 当前基线与历史板测

当前 u1 基线：[20261013_194107 结果](../orb60_board/20261013_194107_4ppc125mhz_2018_3/RESULTS.md)，接受任务 `C:/h/orb4/20261013_185832_board_ed859ec6`。normal Verified/Production 各 100 帧、noise/zero Verified 各 10 帧通过，特征分别为 280/942/0。normal Production 整 PS 循环 6.31751 ms/帧，即 **158.2902 FPS**，平均处理 6.306 ms，较 r17 提升 10.48%。125 MHz routed WNS +0.157 ns / WHS +0.046 ns；LUT 38875、FF 36102、BRAM 66.5、DSP 62、Slice 11549/11550。完整发布包与启动器已实际上板验收；u1 独立封存 1240 个文件并通过哈希检查。

历史 r17：[20261013_171727 结果](../orb60_board/20261013_171727_4ppc125mhz_2018_3/RESULTS.md)。125 MHz、100 帧 Production 整 PS 循环 6.97965 ms，即 **143.2737 FPS**；较 r16 提升 18.96%。完整精确、AXI、路由和物理门槛通过，源码和证据保持冻结于 `../r17/`。

历史 r16：[20261013_100119 结果](../orb60_board/20261013_100119_4ppc100mhz_2018_3/RESULTS.md)，接受任务 `C:/h/orb4/20261013_083117_route_f6928ddc`。normal Verified/Production 各 100 帧、noise/zero Verified 各 10 帧通过，分别为 280/942/0 个精确特征。normal Production 程序循环 8.30318 ms/帧，即 **120.4358 FPS**，处理平均 8.292 ms（8.279..8.308 ms），较 r15 提升 8.92%。100 MHz routed WNS +0.076 ns / WHS +0.018 ns，TNS/THS 为 0，76411/76411 网完整路由；LUT 42365、FF 38099、BRAM 66、DSP 62、Slice 11547/11550（99.97%）。116 个包文件哈希和启动器语法已检查。性能不含外部采集/传输，Production 为 COMPLETE_UNVERIFIED；完整来源、普通 DRC 警告和物理日志保留于包内。

冻结基线 r15：[20261013_071953 结果](../orb60_board/20261013_071953_4ppc100mhz_2018_3/RESULTS.md)，接受任务 `C:/h/orb4/20261013_005322_route_2ae035d0`。normal Verified/Production 各 100 帧、noise/zero Verified 各 10 帧通过，分别为 280/942/0 个精确特征。normal Production 程序循环 9.04387 ms/帧，即 **110.5721 FPS**，处理平均 9.032 ms（9.025..9.044 ms）。相对原 4PPC 提升 33.99%，相对 PS 直接 DDR 版本再提升 4.27%；输入仍为常驻、不可变 DDR，外部采集传输不计入。最终 WNS +0.201 ns / WHS +0.035 ns，TNS/THS 为 0，75459/75459 网完整路由；LUT 41920、FF 37571、BRAM 66、DSP 62、Slice 100%。117 个发布文件哈希及 PowerShell 启动器语法已检查。完整来源、普通 DRC 警告和物理日志均在包内报告中。

PS 直接 DDR 版本：[20261012_235156 结果](../orb60_board/20261012_235156_4ppc100mhz_2018_3/RESULTS.md)，任务 `C:/h/orb4/20261012_230833_ps_direct_16152a6f`。复用原 4PPC routed bit，重新配对 PS ELF；normal Verified/Production 各 100 帧、noise/zero Verified 各 10 帧通过。normal Production 程序循环 9.43018 ms/帧，即 106.0425 FPS，处理平均 9.419 ms。107 个发布文件哈希通过。新的 PL 优化进度和证据记录于 `evidence/attempt140.md`，不计入这个已发布结果。

2026-10-12 发布包：[4PPC 100 MHz / 2018.3 结果](../orb60_board/20261012_223016_4ppc100mhz_2018_3/RESULTS.md)。normal Verified 100 帧、Production 100 帧，noise/zero Verified 各 10 帧通过。normal Production 处理平均 9.422 ms，程序循环 82.52 FPS；原 2PPC 分别为 11.690 ms、69.45 FPS。最终 WNS +0.123 ns / WHS +0.039 ns，Slice 100% 占用。发布包的 104 个文件哈希和 PowerShell 启动器语法均已检查；该便携启动器尚未额外执行一轮板测，实际板测使用 `scripts/board_test.py` 和同一份导出 Tcl。

实际接受任务为 `C:/h/orb4/20261012_214837_route_1eda9d67`。复现该任务的主要命令：

```powershell
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd' scripts/board_route.py --from-job C:/h/orb4/20261012_195415_board_5b202ec5 --carry-remap 2 --no-fanout-opt
./ps_board/build.ps1 -JobRoot C:/h/orb4/20261012_214837_route_1eda9d67
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd' scripts/board_test.py --job C:/h/orb4/20261012_214837_route_1eda9d67 --frame normal --frames 100 --mode Verified
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd' scripts/board_test.py --job C:/h/orb4/20261012_214837_route_1eda9d67 --frame normal --frames 100 --mode Production --rerun
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd' scripts/board_test.py --job C:/h/orb4/20261012_214837_route_1eda9d67 --frame noise --frames 10 --mode Verified
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd' scripts/board_test.py --job C:/h/orb4/20261012_214837_route_1eda9d67 --frame zero --frames 10 --mode Verified
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd' scripts/board_release.py --job C:/h/orb4/20261012_214837_route_1eda9d67
```

原构建任务和发布包保持冻结；复现时使用脚本新建任务返回的路径替换命令中的接受任务路径。完整 HLS 使用 Vivado HLS 2018.3，整机使用 Vivado 2018.3 build 2405991，PS 使用 SDK 2018.3。独立 AXI 仿真使用 ModelSim 2020.4、cocotb 2.0.1、Python 3.11.6；native 精确比较使用 MinGW 14.2.0。失败的首次全零传输与 DAP 恢复过程保留在发布报告中，断电重启后的同版本复测通过。

r15 发布包的 `run_verified.ps1 -Frame normal -Frames 1` 已额外直接上板复测通过（280 个精确特征）；见 `evidence/portable_launcher_verified.json`。该包的实际复测与 117 个冻结文件哈希均已确认，性能仍取 normal 100 帧 Production。

历史 r16 的 120.4358 FPS 发布包的 `run_verified.ps1 -Frame normal -Frames 1` 也已直接上板复测通过（280 个精确特征）；见 `evidence/row_pipeline_portable_verified.json`。116 个包文件哈希及启动器语法通过。当时板卡装载该版 SRAM 配置，CPU 在稳定终态后停止；冻结 r15 未修改。
