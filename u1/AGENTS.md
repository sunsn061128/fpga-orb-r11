# u1 frozen FPGA baseline

- Device/top: ACZ7015 xc7z015clg485-2, s_wrapper; PS FCLK0 125 MHz. HLS tops fastv2_layer4_hls and fastv2_descriptor_hls.
- Tools: Vivado HLS/Vivado/SDK 2018.3; Windows process-isolated profiles C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin; no global PATH changes or WSL.
- Build/test: commands in source/AGENTS.md and source/README.md; accepted compiler inputs and SDK dependency are frozen separately. Relocation rebuild has not been run. Integrity: ./verify.ps1. Authorized volatile board test: ./release/run_verified.ps1 -Frame normal -Frames 100.
- Boundaries: every baseline file is frozen; new sources, generated jobs and releases belong outside u1. Historical r17/r16/r15 remain frozen.
- Acceptance: exact outputs/guards, AXI stalls/reset/completion, complete routed setup/hold/constraint/DRC, paired bit/ELF, actual normal V/P100 and noise/zero V10. Same input/budgets/full PS-loop timing; explicitly report clocks; external capture/transfer excluded. Production remains COMPLETE_UNVERIFIED.
