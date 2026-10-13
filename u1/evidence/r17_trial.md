# r17 后的优化试验

用户要求保留已验收125 MHz版本为r17并继续优化。r17已独立封存到../r17，1217文件逐项SHA及独立Windows PowerShell verify.ps1通过；normal100帧Production整PS循环143.2736599 FPS。r16/r15保持历史冻结。r17原发布包保留其当时与r16比较的报告，不改写历史接受记录。

实际r17描述子累计4.56896ms；核间重叠模型6.82072ms、实测6.97965ms。后六层配对等待描述子的比例增加，0.15893ms差值包含最终写回/响应，不能当作CPU单独耗时。125MHz最终关键setup路径在描述子PIPO写使能，WNS+0.157ns，因此本轮先保持125MHz。

第一候选扩大描述子raw/blurred两路m_axi num_read_outstanding，从2到8。points和features/stats维持原窗口；计算、地址、实际读取字节、输出、预算、四像素图像核不变。目标是让更多短突发patch行读取在DDR响应延迟期间并发。必须重新确认真实HLS结构/资源、完整30事务1222特征C/RTL、随机AXI/reset/完成屏障、整机装箱/时序、PS配对及同输入normal V/P100、noise/zero V10后才报告性能收益。

18:04更新：完整native精确gate PASS；新HLS作业 `C:/h/orb4/20261013_173703_descriptor_95026780` 在8ns下C仿真30事务/1222特征通过，顶层Verilog两路实际NUM_READ_OUTSTANDING=8，points=2。HLS预估BRAM18=27、DSP16、FF11844、LUT15763，与r17相同，不能据此推断最终集成资源。独立 `build/r17_opt/read8/read8_axi_descriptor_8ns` 已SUCCEEDED，实际7组case[35,0,30,24,25,36,37]、reset7、最终响应完成屏障PASS。厂商完整Verilog C/RTL仍在运行；驱动 `build/r17_opt/read8` 通过后复用r17的接受图像核和全21组AXI证明，生成新的125MHz整机作业与配对PS运行库。尚未下载新候选、尚无新FPS。

18:34更新：read8描述子完整Verilog C/RTL30事务/1222特征PASS，export SUCCEEDED；新整机 `C:/h/orb4/20261013_181237_board_fc1b8d97` 及CARRY2路由 `C:/h/orb4/20261013_182938_route_68002f4b` 正在实现，配套SDK runtime SUCCEEDED。真实单IP综合：r17 LUT8793/FF11544/BRAM13.5，read8 LUT8822/FF11558/BRAM13.5。普通AXI模型逐突发响应，不能验证多个AR并发；新增可独立接受AR的有界顺序ID0读队列，并在Monitor逐AR记录size/beats、检查RLAST和R响应背压稳定性，默认保留原单请求模型。补充相同随机种子、40..80周期首响应延迟、队列容量8的两版实际ModelSim试验 `build/r17_opt/r17_queued40_80`、`build/r17_opt/read8_queued40_80` 均SUCCEEDED，case0/37、reset7、完整DDR/guard及完成屏障PASS。r17峰值两路各2、read8各3；case0周期182476→182532，case37周期727150→726578，效果很小，尚无板测FPS。

第二候选在8个未完成请求之外，给raw/blurred端口增加latency=64调度提前量；计算、地址、字节和像素通路不改。依据与当前工具匹配的[UG902 v2018.3](https://docs.amd.com/api/khub/documents/5gyizn_8XEownMJ13k6BRA/content) 114页，这个值用于提前安排AXI请求；实际并发、面积、时序和FPS必须重新验证。read8冻结作业继续完成板测。

18:56 read8板测完成normal Verified100 EXACT280，紧邻同配置Production100 wall698066us、143.2529302 FPS；r17为143.2736599，差异-0.01447%，不视作有效改善，不发布其为新接受性能版本，也不重新封存基线。比较 `evidence/r17_read8_comparison.json`，实际日志保留在181237作业中。候选routed WNS+0.051ns/WHS+0.044ns，完整bit/ELF配对通过；CPU/PLL/FCLK0回读正确。

第二HLS `C:/h/orb4/20261013_184246_descriptor_818b3438`：native完整精确PASS，C仿真30事务/1222特征PASS。RawPatchRows深度21→84、BlurPatchRows18→81，II仍9/8；预估BRAM27/DSP16/FF12041/LUT15858。普通独立AXI7组 `build/r17_opt/prefetch64/prefetch64_axi_descriptor_8ns` SUCCEEDED。高延迟补充 `build/r17_opt/prefetch64_queued40_80` 同case0/reset7/40..80首响应/队列8实际SUCCEEDED，两路请求峰值8/8，计数63767，对照r17同模型case0计数182476；完整DDR/特征/guard、R背压稳定性、逐请求ARLEN/RLAST、复位与完成屏障通过。这是独立内存模型结果，不能换算成板上FPS。控制器 `build/r17_opt/prefetch64` 在厂商完整C/RTL与export通过后自动启动125MHz整机。

19:36更新：prefetch64完整Verilog C/RTL30事务/1222特征及export SUCCEEDED。默认整机 `C:/h/orb4/20261013_185832_board_ed859ec6` 最终125 MHz routed WNS+0.157ns/WHS+0.046ns，严格路由/内部约束/DRC gate和bit/ELF配对PASS；LUT38875、FF36102、Slice11549/11550、BRAM66.5、DSP62。normal Verified100 EXACT280；紧邻同配置Production100 wall631751us，平均处理6306us，整PS循环158.2902124 FPS，对r17提升10.4810281%（6.97965→6.31751ms/帧）。比较 `evidence/r17_prefetch64_comparison.json` 验证同PS board.c/normal golden/预算/不可变DDR合同。描述子累计4.56896→2.237816ms，所有描述子阶段已短于下一层图像；重叠模型6.173464ms，完整循环差0.144046ms，不是CPU专属测量。noise Verified10 EXACT942；zero Verified10正在进行。继续保持r17为冻结基线，不自动建立r18。

19:48最终完成：zero Verified10 EXACT0。正式包 `../orb60_board/20261013_194107_4ppc125mhz_2018_3` 已导出，含补充8请求并发验证的冻结模型/结果/运行时证明；128文件SHA与PowerShell启动脚本AST独立检查PASS。包内bit/ELF/PS init实际重新下载normal Verified1，128-word终态、时钟回读及280特征精确校验PASS。包MANIFEST SHA256=`C1893DB9340572A2BAF92B6E3BCA7EF2F25040A19757E6461A9F2013265DAD8C`。最终记录 `evidence/r17_prefetch64_result.json`，包检查 `evidence/r17_package_check.json`，当前发布索引仍注明冻结比较基线r17。备用CARRY2路线也SUCCEEDED，未选用；最终接受/板测/发布使用默认路线。仅JTAG易失性配置，不写flash/SD/eFUSE，r17/r16/r15均保持冻结。
