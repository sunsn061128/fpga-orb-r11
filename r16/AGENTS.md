# r16 frozen FPGA baseline

- Device/top: ACZ7015 xc7z015clg485-2, s_wrapper; PS FCLK0 100 MHz. HLS tops fastv2_layer4_hls and fastv2_descriptor_hls.
- Required vendor tools: Vivado HLS/Vivado/SDK 2018.3; process-isolated profiles C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin. No global PATH changes or WSL.
- Build/test commands originate in source/AGENTS.md and source/README.md; rebuild in the development workspace with its recorded SDK dependency. Snapshot relocation rebuild has not been run. Package integrity: ./verify.ps1. Authorized volatile exact board test: ./release/run_verified.ps1 -Frame normal -Frames 100.
- Frozen boundaries: every file in this baseline is preserved; source/ is the author snapshot, frozen_hls/ and frozen_board/ are accepted compilation inputs/checkpoints, release/ is the accepted board package. New generated jobs and future releases belong outside r16.
- Acceptance: exact outputs/guards, AXI stalls/reset/completion, complete routed setup/hold/constraint/DRC checks, paired bit/ELF and physical normal V/P100 plus noise/zero V10. Compare new performance against r16 using identical input, budgets and full PS-loop timing; report baseline 100 MHz and candidate clock explicitly; exclude external capture/transfer. Production remains COMPLETE_UNVERIFIED.
