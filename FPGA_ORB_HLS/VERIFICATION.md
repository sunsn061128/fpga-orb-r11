# Verification status

Date: 2026-10-08

## Bit-exact regression

The HLS C implementation was replayed against every archived configuration in
`../FPGA_ORB_SHARED_v1.0`:

- 172/172 configurations passed across 43 input images.
- The set includes odd dimensions, blank images, equal-score ties, boundary corners,
  EuRoC frames, and 960x720 flight frames.
- L4, L8, normal (1000 features), initialization (5000 features), single threshold,
  and T2 dual threshold were covered.
- Each comparison was exact for the B1 packet, cross-level output order, and all 12
  schema2 per-level trace stages.

No numeric tolerances were used.

The focused repository test suite also passed 4/4 tests: the level T2 capability
contract, one v1.0 replay fixture, its byte-exact replay, and rejection of unsupported
grid top-K values above 1.

## HLS synthesis

Vivado HLS 2018.3 C synthesis completed for `xc7z015clg485-2` with a 10 ns clock constraint.

| Top | Estimated period | BRAM18K | DSP48E | FF | LUT |
|---|---:|---:|---:|---:|---:|
| `orb_resize_hls` | 8.777 ns | 4 | 22 | 3,770 | 3,863 |
| `orb_level_hls` | 8.750 ns | 123 | 70 | 22,469 | 24,223 |

These are HLS estimates, not post-place-and-route numbers. With HLS's 1.25 ns clock
uncertainty, the resize helper's 8.7775 ns critical path is 0.0275 ns above the
8.75 ns effective scheduling budget and is reported as a warning. The level kernel
is estimated at the effective budget. The level kernel also has data-dependent loops,
so the first synthesis cannot report a fixed frame latency.

## RTL co-simulation

No C/RTL co-simulation pass is claimed. Vivado HLS 2018.3 XSIM repeatedly stops making
progress at 125 ns in the AXI co-simulation harness, including runs with reduced interface
depths and scalar FAST/Gaussian fallbacks. The full-depth C model and focused C testbench
do pass the frozen resize and level digests, but those results do not prove RTL simulation.
Both synthesized solutions were exported as Vivado IP-catalog components and exercised
through the implemented PS/PL design on the physical board as described below.

## Vivado PS/PL implementation

Vivado 2018.3 completed synthesis, placement, routing, timing signoff, and bitstream
generation for the ACZ7015 (`xc7z015clg485-2`) at 100 MHz. The block design uses
`M_AXI_GP0` for control, HP0 through HP2 for the first three level-kernel masters,
and HP3 for the feature master plus both resize masters.

| Result | Value | Device utilization |
|---|---:|---:|
| Slice LUTs | 25,389 | 54.95% |
| Slice registers | 28,085 | 30.40% |
| Block RAM tiles | 16 | 16.84% |
| DSPs | 88 | 55.00% |
| Setup WNS | +0.201 ns | constraints met |
| Hold WHS | +0.044 ns | constraints met |

Routing completed with zero failed, unrouted, or partially routed nets. Post-route DRC
reported zero errors and 191 warnings. The warnings are primarily HLS/AXI RAM asynchronous
control checks and DSP input/output pipeline recommendations; they do not prevent bitstream
generation, but the small setup margin and these warnings should be included in board testing.
Timing analysis reports zero failing setup/hold endpoints, zero unconstrained internal
endpoints, and a single 100 MHz `clk_fpga_0` timing domain.

The generated bitstream is
`vivado_build/orb_fpga.runs/impl_1/orb_system_wrapper.bit`.

This routed bitstream contains the v1.0 level IP with the version, dual-threshold
capability, and `max_K=1` metadata fields verified by C regression and board readback.

## PS software build

Xilinx SDK 2018.3 successfully regenerated a standalone BSP from the newly implemented HDF,
included both generated HLS drivers, and compiled and linked
`sdk_build/orb_selftest/Debug/orb_selftest.elf`. The ELF contains bounded polling
drivers with explicit cache maintenance and compares both accelerator outputs against
the same resize digest, level counts, capability metadata, and full level digest as
the focused C testbench.

The resize DMA buffers are padded to complete 32-byte cache lines. This prevents the
standalone BSP's partial-line invalidate path from writing stale adjacent cache data
back over the final accelerator-produced pixels.

## Physical-board execution

The matching bitstream and ELF were downloaded through the Digilent JTAG-SMT2 cable to
the ACZ7015 on 2026-10-07. The run was volatile: it configured the PL and loaded DDR,
without writing QSPI, SD, eFUSE, or other non-volatile storage.

- JTAG device: `xc7z015`, IDCODE `0373b093`.
- The DDR mailbox reported `PASS`, error code zero, and 1000 completed iterations.
- DDR metadata readback reported 262 candidates, 40 selected features, algorithm
  version `0x00010000`, dual-threshold capability `1`, and `max_K=1`.

The mailbox reported bit-exact resize digest `0785608cb8d896a4` and full level digest
`6828af1c938eda82` on the physical PS/PL system without relying on DCC output or a
debugger breakpoint.

The fixed-DDR mailbox benchmark then completed 1000/1000 consecutive transactions with
zero initialization, timeout, accelerator, digest, cache-coherency, or AXI failures:

| Measurement | Average | Minimum | Maximum | Throughput |
|---|---:|---:|---:|---:|
| Resize accelerator wait | 2.680 ms | — | — | — |
| ORB level accelerator wait | 12.209 ms | — | — | — |
| Combined accelerator chain | 14.895 ms | — | — | 67.136 FPS |
| End-to-end with bit-exact verification | 17.640 ms | — | — | 56.689 FPS |

Each frame reproduced 262 candidates, 40
selected features, version `0x00010000`, dual-threshold capability, `max_K=1`, and both
frozen digests. This benchmark is a fixed 96x80, single-level sample; it is a stability
and bit-exactness result, not evidence that the full 960x720, eight-level pipeline reaches
30 FPS. The wall-time-derived figures include AXI and polling latency rather than
representing an internal PL-only cycle counter.

## Remaining hardware qualification

- Measure board power and temperature under sustained load.
- Benchmark larger image sizes and representative application frames.
