# ACZ7015 使用 Vivado 与 SDK 图形界面运行自检

本文说明如何使用 Vivado 2018.3 图形界面下载 PL 位流，再使用 Xilinx SDK
2018.3 图形界面初始化 PS、下载 ARM 裸机程序并运行 ORB 自检。

## 1. 操作目标

完整流程如下：

```text
连接并上电 ACZ7015
        ↓
Vivado Hardware Manager 下载 PL 位流
        ↓
Xilinx SDK 初始化 PS 和 DDR
        ↓
SDK 下载并运行 orb_selftest.elf
        ↓
在 Console 中检查 PASS 输出
```

该方法只执行易失性 JTAG 配置，不写入 QSPI、SD 卡、eFUSE 或其他非易失性
存储器。开发板断电后，PL 配置会消失。

## 2. 操作前检查

1. 将开发板设置为 JTAG 启动模式。当前 ACZ7015 使用 `SW1：1 上、2 下`。
2. 连接开发板电源。
3. 连接 Digilent USB-JTAG 调试接口。
4. 启动 Vivado 2018.3，不要使用其他 Vivado 版本自动升级工程或 IP。
5. 确认以下文件存在：

```text
C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\vivado_build\orb_fpga.xpr
C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\vivado_build\orb_fpga.runs\impl_1\orb_system_wrapper.bit
C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\sdk_build\orb_selftest\Debug\orb_selftest.elf
```

## 3. 使用 Vivado 下载 PL 位流

### 3.1 打开 Vivado 工程

在 Vivado 2018.3 中选择：

```text
File → Open Project
```

打开：

```text
C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\vivado_build\orb_fpga.xpr
```

### 3.2 连接开发板

在左侧 `Flow Navigator` 中依次选择：

```text
PROGRAM AND DEBUG
→ Open Hardware Manager
→ Open Target
→ Auto Connect
```

连接成功后，Hardware 窗口中应能看到 `xc7z015` 器件。如果没有看到器件，
先检查开发板电源、USB-JTAG 线、启动模式和 Digilent/Xilinx 驱动。

### 3.3 下载 bitstream

在 Hardware 窗口中右键 `xc7z015`，选择：

```text
Program Device
```

在 `Bitstream file` 中选择：

```text
C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\vivado_build\orb_fpga.runs\impl_1\orb_system_wrapper.bit
```

`Debug probes file` 留空，然后点击 `Program`。

下载完成后，如果 Vivado 显示以下信息：

```text
INFO: [Labtools 27-1434] Device xc7z015 ... is programmed with a design
that has no supported debug core(s) in it.
```

这是正常提示，不是错误。它表示位流中没有 ILA、VIO 或 Debug Hub 等 PL
内部调试核，不影响 FPGA 逻辑运行、PS/PL AXI 通信、SDK 下载 ELF 或 JTAG/DCC
输出。只有需要在 Vivado 中观察内部波形时，才需要加入 ILA 并重新生成位流。

## 4. 使用 Xilinx SDK 运行 PS 自检程序

### 4.1 启动 SDK

可以在 Vivado 菜单中选择：

```text
File → Launch SDK
```

如果软件询问 Workspace，选择：

```text
C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\sdk_build
```

也可以单独启动 Xilinx SDK 2018.3，然后选择同一个 Workspace。

### 4.2 检查 SDK 工程

SDK 左侧 `Project Explorer` 中应能看到：

```text
orb_hw
orb_bsp
orb_selftest
```

如果没有显示这些工程，选择：

```text
File
→ Import
→ General
→ Existing Projects into Workspace
```

根目录选择：

```text
C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\sdk_build
```

然后导入 `orb_hw`、`orb_bsp` 和 `orb_selftest`。

### 4.3 编译自检程序

右键 `orb_selftest`，选择：

```text
Build Project
```

编译成功后，应生成：

```text
orb_selftest
└─ Debug
   └─ orb_selftest.elf
```

如果已经存在最新的 ELF，这一步仍可执行，用于确认 BSP 和应用工程能够正常编译。

### 4.4 创建运行配置

右键 `orb_selftest`，选择：

```text
Run As → Run Configurations...
```

在左侧选择：

```text
Xilinx C/C++ application (System Debugger)
```

新建运行配置，并确认主要参数如下：

```text
Application：orb_selftest
Processor：ps7_cortexa9_0
Executable：Debug/orb_selftest.elf
```

在 `Target Setup` 页面中启用：

```text
Reset entire system
Run ps7_init
Run ps7_post_config
```

因为本流程已经在 Vivado 中下载了位流，所以 SDK 中的 `Program FPGA` 可以不勾选。
如果选择让 SDK 再次下载 PL，则必须指定同一个
`orb_system_wrapper.bit`，不要选择旧版本位流。

### 4.5 启动程序

点击 `Run`。SDK 将依次完成：

1. 复位 Zynq PS。
2. 执行 `ps7_init`，初始化时钟、MIO 和 DDR。
3. 执行 `ps7_post_config`，完成 PS/PL 接口配置。
4. 将 `orb_selftest.elf` 下载到 DDR。
5. 在 Cortex-A9 CPU0 上启动裸机程序。

## 5. 检查运行结果

当前 BSP 的标准输出连接到 ARM CoreSight DCC，而不是物理 UART。因此不需要连接
串口线，输出应显示在 SDK 底部的 `Console` 窗口中。

如果没有显示 Console，选择：

```text
Window → Show View → Console
```

未通过 XSCT 指定循环次数时，程序默认连续运行 100 帧。在当前板卡上约 15 秒完成，
正确输出为：

```text
ORB PS/PL benchmark start iterations=100 mailbox=01000000
ORB_RESIZE_PASS pixels=2255 digest=0785608cb8d896a4
ORB_SELFTEST_PASS candidates=262 selected=40 version=1.0 dual_threshold_supported=1 max_K=1 digest=6828af1c938eda82
ORB_BENCHMARK_PASS iterations=100 resize_us=... level_us=... pipeline_fps_milli=...
```

只有同时看到 `ORB_RESIZE_PASS` 和 `ORB_SELFTEST_PASS`，才能判定本次 PS/PL
自检通过。

## 6. 常见问题

### 6.1 Vivado 提示没有 Debug Core

```text
no supported debug core(s) in it
```

不影响本项目运行。当前位流没有加入 ILA/VIO，所以 Vivado 无法显示 PL 内部探针。

### 6.2 Hardware Manager 找不到 xc7z015

依次检查：

1. 开发板是否上电。
2. USB-JTAG 数据线是否正确连接。
3. SW1 是否设置为 JTAG 启动。
4. Digilent/Xilinx 驱动是否安装。
5. 是否有其他 Vivado、SDK 或 XSCT 实例占用了 JTAG 连接。

### 6.3 ELF 下载失败或 DDR 地址不可访问

通常表示 PS/DDR 尚未正确初始化。检查运行配置中是否启用了 `Reset entire system`、
`Run ps7_init` 和 `Run ps7_post_config`。

### 6.4 程序运行但 Console 没有输出

检查：

1. 运行目标是否为 `ps7_cortexa9_0`。
2. 是否打开了正确的 SDK Console。
3. BSP 的标准输出设备是否为 `ps7_coresight_comp_0`。
4. 是否误用了 UART Terminal；当前工程输出走 JTAG/DCC，不走物理串口。

### 6.5 自检输出 FAIL

记录完整的 FAIL 行，不要只记录最后的错误码。首先确认 bitstream 和 ELF 来自同一次
工程构建，避免新 ELF 搭配旧位流。然后根据 `init`、`timeout`、
`accelerator_status` 或 `digest` 字段定位问题。

## 7. 运行结束

运行结束后可以在 SDK 中点击红色停止按钮终止调试会话，再关闭 Hardware Manager。
这些操作不会擦除或写入非易失性存储。开发板断电后，本次 PL 配置自动消失。
