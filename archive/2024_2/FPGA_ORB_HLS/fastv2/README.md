# fastv2 流式前端

`fastv2` 是与现有 v1.0 基线隔离的性能架构工程。当前实现固定八层精确 resize，以及完整帧单次扫描的 FAST、tile-local 3×3 NMS 和片上 top-1 网格筛选。分数图和原始候选点列表不写回 DDR；候选点先进入8192项片上缓冲，使像素流水不受网格赢家读改写依赖限制。

## 当前周期契约

- 目标器件：ALINX ACZ7015，`xc7z015clg485-2`。
- 时钟目标：100 MHz，单时钟域，`ap_ctrl_hs` 事务接口。
- 像素阶段目标 II：1；每行先突发读取 `ceil(width/8)` 个64位字，再逐像素处理。
- 只接受960×720金字塔的八组固定层尺寸/预算；tile、grid、resize坐标和Q0.11权重均由生成 ROM 提供，事务期间不做除法或建表。
- `fastv2_resize_hls` 以 `source_level=0..6` 选择相邻层转换，输出逐像素保持 v1.0 舍入语义。
- 输入在启动到完成期间保持不变；输出缓冲区不得与输入重叠。
- 无候选点丢弃。NMS 相等分数规则、tile边界和网格强度排序保持 v1.0 语义。
- 正常输出只包含网格筛选结果；完整候选 trace 由现有 v1.0 验证路径保留。

## 命令

```powershell
pwsh -File fastv2/scripts/build_and_test.ps1
pwsh -File fastv2/scripts/csim.ps1 -Selection front
pwsh -File fastv2/scripts/csim.ps1 -Selection resize
pwsh -File fastv2/scripts/synthesize.ps1 -Selection front
pwsh -File fastv2/scripts/synthesize.ps1 -Selection resize
pwsh -File fastv2/scripts/cosim.ps1 -Selection front
pwsh -File fastv2/scripts/cosim.ps1 -Selection resize
pwsh -File fastv2/scripts/export_ip.ps1
pwsh -File fastv2/scripts/build_vivado.ps1 -SetupOnly
pwsh -File fastv2/scripts/build_vivado.ps1 -Jobs 8
pwsh -File fastv2/scripts/build_sdk.ps1
pwsh -File fastv2/scripts/run_board_benchmark.ps1 -ArtifactsOnly
```

主机回归检查固定前端语义、七个resize转换的逐像素结果，以及一帧960×720归档验证图。`include/fastv2_profile_rom.hpp` 由 `scripts/generate_profile_rom.py` 生成，禁止手工修改。HLS和Vivado生成目录为 `fastv2/host_build/`、`fastv2/hls_build/` 和根目录 `vivado_build_fastv2/`，不得手工修改。

## 后续算法阶段

1. 加入精确 Gaussian 流和选中点局部块读取。
2. 接入方向、CORDIC和BRIEF。
3. 完整 ORB 阶段接入后扩展现有 PS 测速；当前前端 PS 调度、DDR 回读和自检见 [PS_BENCHMARK_ZH.md](PS_BENCHMARK_ZH.md)。已完成 100/1000 帧物理板 PASS，流水平均 65.250 ms，分层实测和下一步瓶颈见 [上板验收记录](BOARD_RESULT_20261009_ZH.md)。
