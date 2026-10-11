# R11 默认固件：单独文件分发

Release 单独提供 r11.bit、r11.hdf、r11.elf、ps7_init.tcl 和 SHA256SUMS.txt。
均来自已验收 R11 默认 job；bit/HDF 复用 R8，ELF 为默认版，不是 Profile。
文件仅改短名称，不改内容，SHA256 与本地冻结记录一致。

```powershell
pwsh -NoProfile -File tools/verify_public.ps1 -AssetDir C:/h/firmware
```

bit/HDF/ELF 的来源和哈希见 PUBLIC_SOURCE_MANIFEST.json；冻结 SDK 输入在
frozen/R11_DEFAULT/inputs。SDK、BSP、生成工程不在仓库中；HDF 是 2018.3
的硬件导出，不是新版 XSA，不应在新版本工具中自动升级 IP。

## 不等同于完整上板测试包

仅下载 bit/ELF 不会自动得到输入图和 DDR 参考数据。现有自检固件必须配合
本地归档的 input.pgm、reference_raw.bin、reference_blur.bin，且对应固定输入。
缺少这些素材时不能运行完整 CPU 精确自检，更不能宣称 PASS。
完整素材/历史手动入口仍在本地交接包中。

如果需要沿用 frozen/R11_DEFAULT/inputs/run_board.tcl，先在独立短目录按其
原 job 相对结构放好 inputs、sdk/next_hw/ps7_init.tcl、
sdk/next_benchmark/Debug/next_benchmark.elf 和
vivado/next_fpga.runs/impl_1/next_system_wrapper.bit。使用正确的本地数据，
不要混用其他 ELF、Profile 固件或后续工作区参考。

实体测试会复位 PS/PL、覆盖 SRAM/DDR，必须另行确认原 ACZ7015、供电、JTAG
及无危险外接负载。本次发布不授权任何自动下载；不写 Flash/SD/eFUSE。
Verified 才有 CPU 精确比较；Production 仅 COMPLETE_UNVERIFIED。
