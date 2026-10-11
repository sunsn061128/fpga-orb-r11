# FPGA ORB front end

> Current handoff (2026-10-10): use [HANDOFF_ZH.md](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/HANDOFF_ZH.md) and the [pinned R10 baseline](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/baselines/R10_20261010/README_ZH.md). The sections below describe the original root implementation, not the current `fastv2/next` three-core/PS pipeline or its accepted RTL and board results.

This project is the synthesizable counterpart of `../FPGA_ORB_SHARED_v1.0`.
It deliberately excludes video decode and the SLAM back end.

The split is:

- PL IP `orb_resize_hls`: sequential pyramid resize with the reference integer INTER_LINEAR contract.
- PL IP `orb_level_hls`: FAST-9 response, tiled strict NMS, one-per-grid-cell selection (including T2 fallback), Gaussian7, IC moments, quantized orientation, and rotated BRIEF-256.
- PS host: level scheduling, feature-budget calculation, DDR scratch allocation, cross-level output ordering, and B1 packet serialization.

All image and scratch buffers are external-memory interfaces. `orb_level_hls` exposes one
base pointer on each of four AXI masters: image, packed 16-bit pixel work, packed `Point`
work, and packed 64-byte feature records. Runtime capacity arguments define the contiguous
regions within the two work allocations. This first version prioritizes bit-exact behavior
and then improves throughput without changing the frozen v1.0 arithmetic contract.

## Build and verify

Run from this directory in PowerShell:

```powershell
pwsh -File scripts/build_and_test.ps1
```

Run the full 172-configuration replay with `pwsh -File scripts/replay_all.ps1`.

For a repeatable 960x720, eight-level host C-model timing breakdown, add
`--benchmark-iterations N --benchmark-warmup N` to `orb_hls_cli`. The benchmark reports
resize and ORB time per pyramid level plus average frame time. These values characterize
the software model only; they are not FPGA timing measurements.

For synthesis:

```powershell
pwsh -File scripts/synthesize.ps1
```

For RTL co-simulation, packaged-IP export, and the complete ACZ7015 PS/PL build:

```powershell
pwsh -File scripts/synthesize.ps1 -Selection both -Cosim
pwsh -File scripts/export_ip.ps1
pwsh -File scripts/build_vivado.ps1
pwsh -File scripts/build_sdk.ps1
```

The Vivado block design uses PS `M_AXI_GP0` for the two AXI-Lite control interfaces and
the four PS high-performance DDR ports for the PL memory masters. The generated design,
reports, hardware handoff, and bitstream are placed in `vivado_build/`.
The final command builds a standalone ARM self-test ELF in `sdk_build/`; see
`ps/README.md` for its on-board pass signature. For the complete Chinese Vivado + SDK
graphical procedure, see `VIVADO_SDK_GUI_GUIDE_ZH.md`.

With the ACZ7015 connected in JTAG boot mode, run the fixed-DDR mailbox regression and
performance benchmark without relying on DCC output:

```powershell
pwsh -File scripts/run_board_benchmark.ps1 -Iterations 100
```

`Iterations` accepts 1 through 1000. The script performs only volatile PL/DDR downloads.

The HLS CLI intentionally supports the validated hardware profile only: grid enabled,
fixed FAST, Gaussian enabled, angle A32, grid top-K 1, scale 6/5, and T2 enabled by
default. The level-kernel metadata reports algorithm version 1.0, dual-threshold support,
and `max_K=1`; requests with grid top-K above 1 are rejected before execution.

The current verification result is 172/172 archived configurations passing byte-for-byte.
Both top functions synthesize, and the packaged design implements to a bitstream. Vivado
HLS 2018.3 XSIM stalls in the AXI co-simulation harness, so no RTL co-simulation pass is
claimed. See `VERIFICATION.md` for coverage, synthesis estimates, and board-qualification
boundaries.
