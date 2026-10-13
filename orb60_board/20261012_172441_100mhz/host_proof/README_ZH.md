# ORB60 100MHz本机PS兼容板测

本程序针对新双核 `s_wrapper`，不是旧R11/R12三核固件。复用交付的2024.2 bit，不重建PL、不改频率、不写Flash。原交付包、冻结证据和旧工程不移动、不覆盖。

## 构建边界

本机无2024.2；PS采用明确的SDK 2018.3 ARM GCC及已验收的standalone PS基础运行库。构建入口逐项比较完整PS配置，并核对双方FCLK0=100MHz、HP0..3均启用；编译使用新bit配套的两个生成寄存器头，绝不调用旧三核驱动。BSP中的历史PL驱动定义不能作为新核ABI；本程序只链接PS缓存、计时、启动等基础服务。

这是直接MMIO的独立板测应用，**不声称导出了新XSA/HDF或完成2024.2 BSP迁移**。`pairing.json`保存所有实际编译输入、运行库、bit和ELF哈希。ELF必须结束于结果区`0x01000000`以下。

## 合同及测试

- 镜像引擎`0x43C00000`，描述子引擎`0x43C10000`，使用交付生成头中的32-bit地址寄存器。
- 3个raw/2个blur槽，描述子(n)与图像(n+1)重叠；每层points/summary/statistics独立保留。
- 特征按原顺序追加到容量1000、每项64字节的整帧缓冲，不再覆盖前层。
- Verified比较全层blur、resize、完整16000字节points、64字节summary、全部已追加特征及未写特征/尾guard；图像比较含对齐padding和完整86400字端口未写尾部。
- 图像/描述子统计版本、级别、数量、读写字节和完成状态核对；核心每次调用有1秒watchdog，失败不重新启动未完成的DMA。
- Production不做CPU逐帧比较，终态只能是COMPLETE_UNVERIFIED，不能称作逐帧exact PASS。
- 处理时间从layer0配置前到最终特征写完并失效缓存结束，包含配置、帧内缓存维护和等待。开始前的输入拷贝/初始输出准备不计入；总循环wall另外报告。Verified还含帧内填毒/额外缓存开销，扣除CPU比较也不能当Production速度。
- 内部cycles来自PL统计，排除最后统计写回；PS观察时间可能因并行等待推迟，不能简单相加。

`prepare.py`使用独立NumPy像素/选点参考和原完整32档方向/BRIEF常量，不导入DUT/坐标ROM。normal/noise/zero分别产生280/942/0特征。主机mock检查调度、容量、重入、相邻层所有权、100帧状态和失败/非法命令；mock不证明真实缓存、AXI或FPGA性能。

## 入口

```powershell
# 在新Job目录生成独立参考，再构建；不覆盖任何现有elf目录。
python prepare.py --root <orb60绝对路径> --output <job>/normal --frame normal
pwsh -NoProfile -File build.ps1 -JobRoot <job>
# 核对pairing.json和golden.json全部SHA后，明确确认板卡安全再执行：
& C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd xsct <job>/inputs/run.tcl <job> normal 1 Verified
# 同一已验收bit/参考仍驻留，且未断电、未换程序时可重复：
& C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd xsct <已冻结rerun.tcl> <job> 100 Production
```

JTAG串号C306ACCBABCD、xc7z015 idcode0373b093必须匹配。调试器采用1MHz适配器频率，PL仍为100MHz；10秒后才读取DDR结果。停止后重复读取128字邮箱必须一致。除明确授权的SRAM/DDR下载外，不进行非易失操作。

## 已保留的失败

首次ARM构建遗漏SDK生成的Xilinx.spec，未生成ELF；修复作者构建入口，在新job成功。首次下载bit后XSCT预读PL寄存器因没有XSA地址映射被拒绝，CPU测试尚未运行；去掉调试器预读，由ARM按审计ABI执行，新job一帧精确通过。首次重复测试对已停止CPU执行stop被工具拒绝；新rerun入口允许已停止状态，保留失败日志。零点mock的“破坏特征”注入原先没有可破坏特征，修正为尾guard破坏；这是测试注入错误，不是FPGA或算法失败。

详细实际板测结果及新job路径另见BOARD_RESULT_100MHZ_ZH.md。固定图100帧不证明变化视频、942点压力、P99/max或整机视频60FPS。
