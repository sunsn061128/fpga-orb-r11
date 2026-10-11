# 待上板测试清单

状态：2026-10-10 后续用户授权“上板”，下列 R9/R10 单帧及 100 帧项目全部已执行。精确模式均 PASS，Production 均 COMPLETE_UNVERIFIED；原始日志、分项时间和边界见 [实体板卡报告](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/BOARD_RESULT_R9_R10_ZH.md)。本清单保留复现命令，不是定时任务；1000 帧及变化输入视频仍未测试，不自动操作 JTAG。

## R9 缓存优化 A/B（保留，不被后续流水版本替代）

固定任务：`C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_080031_ps_6ea0a406`。产物已编译并配对通过，新 ELF SHA256 `047C2028EFEA4166C351CD3F7B9D23C268223ED8EA93372B484A4C2FF05A2B24`；对应 bit 与 R8 相同。详见 [R9 说明](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/PS_OPT_R9_ZH.md)。

重新确认器件、线缆、供电和 I/O 安全，关闭其他 JTAG 会话后，先核对产物，再测试：

```powershell
Set-Location 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS'
$r9Job = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_080031_ps_6ea0a406'
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r9Job -ArtifactsOnly
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r9Job -Iterations 1 -Mode Verified
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r9Job -Iterations 100 -Mode Legacy
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r9Job -Iterations 100 -Mode Verified
# 上述精确测试 PASS 后，才运行独立生产测速；不能把此终态当成精确 PASS。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r9Job -Iterations 100 -Mode Production
```

验收及记录：精确模式必须 100/100 PASS、错误码 0、每帧候选 11078 / 特征 280；图像、点、摘要、方向/BRIEF、guard/padding 全部精确比较。保存原始 XSCT 日志和 processing/wall/verify/poison/cache/wait/backend/config/other、最小/最大耗时。Production 必须显示 `COMPLETE_UNVERIFIED`、`verified_frames=0`，仅评价生产处理时间。出现错误即停止扩大测试，不自动复位重跑。

R9 实板 100 帧 Legacy/Verified 平均 59.769/52.454 ms，均精确 PASS、摘要相同；Production 平均 44.387 ms，仅运行完成。下面 R10 流水 A/B 也已执行；再次复现需重新确认安全并获得上板授权，不能用主机 mock 结果填入验收。

## R10 流水 A/B（单帧及 100 帧实板已完成）

独立任务：`C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_082701_ps_554841b3`。schema 5，主机 12/12、SDK 2018.3 和文件配对 PASS，复用 R8 PL；ELF SHA256 `EF378C5A6EEC9F761C3A32B25B172F37E027252086FFD11FEBA32B9114F10E85`。本节命令已在本次授权下执行，保留供复现；精确 Serial/Pipeline 100 帧平均 52.309/40.904 ms，均 PASS，Production 44.330/31.749 ms，仅运行完成。Production 流水平均 31.497 FPS、最大 37.799 ms，不等于每帧满足 30 FPS。

```powershell
$r10Job = 'C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261010_082701_ps_554841b3'
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -ArtifactsOnly
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 1 -Mode Verified -Schedule Serial
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 1 -Mode Verified -Schedule Pipeline
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 100 -Mode Verified -Schedule Serial
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 100 -Mode Verified -Schedule Pipeline
# 上述精确模式全部 PASS 后，才进行无比较的生产测速 A/B。
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 100 -Mode Production -Schedule Serial
pwsh -NoProfile -File fastv2/next/scripts/run_board.ps1 -JobRoot $r10Job -Iterations 100 -Mode Production -Schedule Pipeline
```

要求：精确模式均 100/100 PASS，八层完整结果与 CPU 一致；每帧候选 11078 / 特征 280，Serial/Pipeline digest 完全相同；流水模式提前启动数为 `7*帧数`，`abort_drain_error=0`、`pending_core_mask=0`。Production 两模式均须 `COMPLETE_UNVERIFIED`，不能当成精确 PASS。

保留所有日志，重点比较同一 ELF 的 processing 平均毫秒/帧、min/max、cache、剩余 pl_wait 和 PS backend。流水 PL 分层时间标记为延迟观测上界，不能拿其总和证明真实核周期或吞吐。100 帧通过后可再授权扩展到 1000 帧；它仍是同一归档输入的重复测试，不是视频。

出错立即停止。尤其 PL 超时或收尾错误不表示 DMA 已停，不能自行修改工作缓冲、继续下载 ELF 或复位重跑；保留错误码/核掩码，等待新的诊断授权。详见 [R10 调度说明](C:/Users/dmsun/Desktop/PLD/FPGA_ORB_HLS/fastv2/next/PS_PIPELINE_R10_ZH.md)。
