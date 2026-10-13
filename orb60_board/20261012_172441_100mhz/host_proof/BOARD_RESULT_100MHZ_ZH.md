# 2026-10-12 ORB60 100MHz真实板测

## 最终配对文件

最终job：`C:/Users/dmsun/Desktop/PLD/orb60_board/20261012_172441_100mhz`。

- bit：`orb60_100mhz.bit`，SHA256 `A80FE3FDFF930A3266B2D8701A9DFBC36319010E56744BA0D20245EDA7C1303D`。
- ELF：`elf/orb60_board.elf`，SHA256 `06EBD4636D27A30F4DDD0F913EF75858A84B36A9A86FC23905D6BF77B7A74A09`。
- 独立CPU参考：`normal/golden.bin`，SHA256 `8DEC300AE57E3E7D334A8811F4EBAD00F5155339995FD136E333609AA7BFDE1B`。
- 初始化及完整下载入口：`inputs/ps7_init.tcl`、`inputs/run.tcl`。
- 同一bit/输入仍驻留时重复入口：`rerun.tcl`，SHA256 `777BF7B674E74D10771CFD44A2368EF838C2F93DDE35F6E00F1B353051C45BDF`。
- 实际编译配对：`pairing.json`；板测/主机补充证据：`acceptance.json`。

用户已明确确认原ACZ7015上电、JTAG连接、供电/跳线/时钟/I/O未改变、无危险负载。实际链：Digilent JTAG-SMT2 C306ACCBABCD，xc7z015 idcode0373b093；仅SRAM/DDR下载，未写Flash/SD/eFUSE。最后100帧程序完成，ARM#0已停止。

PL为原交付2024.2双核bit，s_wrapper/xc7z015clg485-2/100MHz。PS为新写直接MMIO兼容测试程序，明确采用本机SDK 2018.3编译及旧已验收PS基础运行库。PS配置完整核对相同，HP0..3和FCLK100MHz覆写相同；两个PL核的寄存器头来自新bit交付。未调用旧三核驱动，未导出新XSA或伪称2024.2 BSP迁移通过。

## 真实结果与口径

输入为原固定960×720图片，8层、20/7双阈值、280特征，每帧11078候选。

| 模式 | 结果 | 处理平均 | 处理最小/最大 | 含输入拷贝/初始准备的总循环平均 |
|---|---|---:|---:|---:|
| Verified，最终PS版 | 100/100 CPU逐字节PASS | 77.488 ms | 76.512 / 83.756 ms | 301.37816 ms |
| Production，最终PS版 | 100/100 COMPLETE_UNVERIFIED | **11.690 ms** | **11.687 / 11.694 ms** | **14.39856 ms** |

Verified每帧核对8层blur/resize像素、points/summary、逐层完整特征追加及padding/tail/容量未写区，累计28000条特征。扣除CPU比较后仍保留帧内填毒/额外缓存检查开销，不能与Production直接比较。

Production处理FPS为85.543（11.690ms换算），包含帧内PS配置/缓存维护/等待和最终特征PS可见性；开始前DDR输入拷贝、初始输出准备排除。全部100帧wall为1439856微秒，包含上述准备，实际测试循环约69.451 FPS。重复固定图的这100次处理中最大11.694ms，不代表变化视频或高特征数最坏情况。Production100帧本身不逐帧CPU比较；精确性来自另一个Verified100帧运行。

最终Production缓存维护总104693微秒、配置总3917微秒，均为100帧累计。缓存总数包含初始准备维护，不能全部归入上述处理区间。

## 核内周期与重叠

以下为最终Production100帧的平均硬件统计cycles，换算100MHz。统计排除核最终stats写回，PS观察值另留日志；两核跨层重叠，不能把两列总和当整帧耗时。

| 层 | 图像引擎cycles（FAST+Gaussian+Resize） | 描述子cycles | 点数 | 帧内特征偏移 |
|---|---:|---:|---:|---:|
| 0 | 355978 | 123215 | 64 | 0 |
| 1 | 248937 | 86700 | 45 | 64 |
| 2 | 174319 | 83780 | 44 | 109 |
| 3 | 122188 | 72559 | 38 | 153 |
| 4 | 85653 | 54615 | 29 | 191 |
| 5 | 60110 | 44829 | 24 | 220 |
| 6 | 42479 | 34599 | 19 | 244 |
| 7 | 30073 | 29243 | 17 | 263 |

先image0，再descriptor0/image1重叠，以此类推，末尾等descriptor7。三个raw、两个blur槽按完成状态复用；每层point/summary/stats独立。图像核内部三个算子共用读流并行，统计未单独拆FAST/Gaussian/Resize，不凭总周期编造分项。

## 时序资源及尚未覆盖

此轮未改PL，沿用交付中完整routed top报告：WNS+0.238ns、TNS0、WHS+0.035ns、THS0，时序覆盖无未约束内部端点；LUT27033、FF27169、BRAM68/95tile、DSP40。没有新的125MHz布线证据，本轮绝未提频。

normal/noise/zero主机控制器测试通过；noise942点、zero0点独立参考已生成，但**未做实体板测**。没有变化视频、1000帧、P99压力验收，不声称全输入稳定60FPS或完整视频/SLAM。

此前job `20261012_171739_100mhz`的一帧精确PASS和Production100帧11.689ms/17.09292ms wall均保留。最终只删测试程序准备阶段一次重复输入拷贝，PL/算法/寄存器/验收不变；最终ELF重新主机验证、真实Verified100与Production100，不能把旧ELF日志混配。

## 手动重复完整下载

先核对全部SHA、确认板卡连接和安全条件，再执行：

```powershell
& C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd xsct C:/Users/dmsun/Desktop/PLD/orb60_board/20261012_172441_100mhz/inputs/run.tcl C:/Users/dmsun/Desktop/PLD/orb60_board/20261012_172441_100mhz normal 1 Verified
# 不断电/不换程序、上一轮完成后：
& C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd xsct C:/Users/dmsun/Desktop/PLD/orb60_board/20261012_172441_100mhz/rerun.tcl C:/Users/dmsun/Desktop/PLD/orb60_board/20261012_172441_100mhz 100 Production
```

初次完整下载由于1MHz JTAG要上传bit和4.48MB参考，约需1～2分钟；这不计入板内处理耗时。输出从固定DDR邮箱读取，不依赖DCC Console。
