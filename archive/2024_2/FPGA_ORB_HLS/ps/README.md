# PS bare-metal self-test

`orb_baremetal_selftest.c` is a Xilinx SDK 2018.3 standalone application for the
ACZ7015 block design. It runs 1 through 1000 consecutive resize and 96x80 level
transactions, performs a full digest check after every frame, measures PS configuration
overhead and accelerator wait time, and publishes all results in a fixed DDR mailbox.

Build the generated BSP and ELF from the repository root:

```powershell
pwsh -File scripts/build_sdk.ps1
```

The output is `sdk_build/orb_selftest/Debug/orb_selftest.elf`. Without an XSCT command,
the application defaults to 100 iterations. On a connected board, program the matching v1.0
`vivado_build/orb_fpga.runs/impl_1/orb_system_wrapper.bit`, initialize the PS, download
the ELF to `ps7_cortexa9_0`, and run it. A bit-exact transaction prints:

```text
ORB PS/PL benchmark start iterations=100 mailbox=01000000
ORB_RESIZE_PASS pixels=2255 digest=0785608cb8d896a4
ORB_SELFTEST_PASS candidates=262 selected=40 version=1.0 dual_threshold_supported=1 max_K=1 digest=6828af1c938eda82
ORB_BENCHMARK_PASS iterations=100 resize_us=... level_us=... pipeline_fps_milli=...
```

The application uses the ARM global timer for a 30-second per-transaction timeout and
prints explicit initialization, timeout, accelerator-status, capability, count, and
digest failures. The matching v1.0 bitstream and ELF have passed 1000 consecutive
bit-exact transactions on the physical ACZ7015.

## Automated DDR mailbox benchmark

Run the complete volatile JTAG flow from PowerShell:

```powershell
pwsh -File scripts/run_board_benchmark.ps1 -Iterations 100
```

Use `-Iterations 1000` for the long stability test. The wrapper programs the matching
bitstream, initializes PS/DDR, downloads the ELF, writes the requested iteration count,
polls DDR address `0x01000000`, and prints the result without DCC or a debugger breakpoint.

The mailbox is 256 bytes, aligned to cache lines, and lies above the linked ELF, heap,
stack, and static accelerator buffers. Major words are:

| Word | Meaning |
|---:|---|
| 0–1 | XSCT command magic and requested iterations |
| 2–7 | result magic, schema, RUNNING/PASS/FAIL, error and progress |
| 10–18 | counts, version, capabilities and both 64-bit digests |
| 19–30 | resize/level configuration, wait, min/max, estimated cycles and FPS |
| 31–38 | pipeline, verification, wall time, total time and failure detail |

The reported PL cycle values are estimates derived from ARM global-timer wall time and
the confirmed 100 MHz fabric clock. They include AXI and PS polling latency; they are not
an internal cycle counter inserted into the HLS cores.

## Manual volatile JTAG run

Set the board to JTAG boot, connect the Digilent USB-JTAG port and board power, then
open the Xilinx SDK 2018.3 XSCT console. Enter the following commands one at a time:

```tcl
set root C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS
connect
jtag targets
targets -set -nocase -filter {name =~ "APU"}
rst -system
after 1000
targets -set -nocase -filter {name =~ "ARM Cortex-A9 MPCore #0"}
source $root/sdk_build/orb_hw/ps7_init.tcl
ps7_init
fpga -file $root/vivado_build/orb_fpga.runs/impl_1/orb_system_wrapper.bit
ps7_post_config
ps7_debug
dow $root/sdk_build/orb_selftest/Debug/orb_selftest.elf
jtagterminal -start
con
```

When run manually without a mailbox command, wait for the 100-iteration default to finish
and check both `ORB_RESIZE_PASS` and `ORB_SELFTEST_PASS`. To stop and disconnect after the
output appears, press Ctrl+C in XSCT if necessary and enter:

```tcl
stop
jtagterminal -stop
disconnect
```

This procedure does not program QSPI, SD, eFUSE, or any other non-volatile memory. The
PL configuration is lost when power is removed.
