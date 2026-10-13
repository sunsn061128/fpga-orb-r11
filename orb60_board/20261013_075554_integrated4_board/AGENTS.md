# Paired board test facts

- Target: ALINX ACZ7015, xc7z015clg485-2, s_wrapper. Digilent JTAG-SMT2 C306ACCBABCD; SRAM configuration only.
- Published PL bits: original integrated4 100/125 MHz, Vivado/Vitis HLS 2024.2. PS compatibility flow: Vivado/SDK 2018.3 using C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd. Native controller checks: MinGW 14.2.0.
- Build new ELF pairs: pwsh -NoProfile -File prepare_board.ps1. Finalize generated PS init/hash manifests: pwsh -NoProfile -File finalize_pairing.ps1 -FrequencyMHz 100|125.
- Explicit live test: pwsh -NoProfile -File run_board.ps1 -FrequencyMHz 100|125 -Frame normal|noise|zero -Frames 1..1000 -Mode Verified|Production.
- Full physical suite: pwsh -NoProfile -File run_acceptance.ps1. Each input requires Verified before its 1000-frame Production run; -PrecedingResult allows reuse only after matching the immediately preceding hardware mailbox and exact bit/ELF/input. Report: fpga-python.cmd write_report.py after the suite succeeds.
- Authored helpers: root scripts. Generated: psgen*/; 100mhz/ and 125mhz/ ELF/BSP copies, input snapshots and manifests; golden/. Preserve previous results and published source/bit evidence.
- Acceptance: target/cable and configured FCLK readback match; exact eight-layer pixels, selected points, summaries, A32/BRIEF features and guards; stable terminal mailbox, zero errors, requested completion count and new 4PPC stats version. Production is COMPLETE_UNVERIFIED; report resident-DDR wall timing separately from external capture/transfer and representative-video P99.
