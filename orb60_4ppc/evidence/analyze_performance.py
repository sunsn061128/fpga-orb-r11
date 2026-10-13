"""Read accepted board evidence; write a diagnosis without changing either release."""
from pathlib import Path
import difflib
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT.parent / "orb60_board/20261012_172441_100mhz"
NEW = ROOT.parent / "orb60_board/20261012_223016_4ppc100mhz_2018_3"


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def metrics(words, mhz):
    frames = words[6]
    assert frames == 100 and words[4] == 4 and words[12] == 0
    image, descriptor = words[20:28], words[28:36]
    counts = words[52:60]
    assert sum(counts) == words[16] == 280
    critical_cycles = image[0] + sum(max(descriptor[i], image[i + 1]) for i in range(7)) + descriptor[7]
    wall_us = words[11] / frames
    return dict(
        frames=frames, features=words[16], image_cycles=image, descriptor_cycles=descriptor,
        counts=counts, processing_us=words[10], wall_per_frame_us=wall_us,
        input_preparation_and_loop_us=wall_us - words[10],
        cache_per_frame_us=words[18] / frames, config_per_frame_us=words[19] / frames,
        ideal_schedule_us=critical_cycles / mhz,
        processing_minus_ideal_schedule_us=words[10] - critical_cycles / mhz,
        descriptor_sum_us=sum(descriptor) / mhz,
        processing_only_fps=1e6 / words[10], whole_loop_fps=1e6 / wall_us,
    )


old_acceptance = read_json(OLD / "acceptance.json")
new_acceptance = read_json(NEW / "acceptance.json")
new_run = read_json(NEW / "reports/normal_Production.json")
manifest = {item["Path"]: item["SHA256"] for item in read_json(NEW / "manifest.json")["Files"]}
for relative in ["acceptance.json", "inputs/board.c", "reports/normal_Production.json",
                 "reports/descriptor_synthesis/p_anonymous_namespace_9_csynth.rpt"]:
    assert digest(NEW / relative) == manifest[relative]
assert old_acceptance["PLMHz"] == new_acceptance["PLMHz"] == 100
old = metrics(old_acceptance["Results"]["production"]["Words"], 100)
new = metrics(new_run["mailbox"], 100)
assert new_run["processing_avg_us"] == new["processing_us"]
assert old["counts"] == new["counts"]
assert all(new["descriptor_cycles"][i] > new["image_cycles"][i + 1] for i in range(7))

old_source = (OLD / "inputs/board.c").read_text().replace("layer2", "layer4").replace("LAYER2", "LAYER4")
new_source = (NEW / "inputs/board.c").read_text()
source_diff = "\n".join(difflib.unified_diff(old_source.splitlines(), new_source.splitlines(),
                                           fromfile="2PPC (layer2 name normalized)", tofile="4PPC", lineterm=""))
report = (NEW / "reports/descriptor_synthesis/p_anonymous_namespace_9_csynth.rpt").read_text()
copy_latencies = []
for name in ["CollectRawPatch", "CollectBlurPatch"]:
    match = re.search(r"\|\s*\+ " + name + r"\s*\|\s*(\d+)\|\s*(\d+)\|", report)
    assert match and match[1] == match[2]
    copy_latencies.append(int(match[1]))
assert copy_latencies == [158, 222]
copy_us = sum(copy_latencies) * new["features"] / 100
descriptor_increase_us = new["descriptor_sum_us"] - old["descriptor_sum_us"]

sources = [OLD / "acceptance.json", OLD / "inputs/board.c", NEW / "reports/normal_Production.json",
           NEW / "inputs/board.c", NEW / "reports/descriptor_synthesis/p_anonymous_namespace_9_csynth.rpt",
           ROOT / "scripts/migrate_descriptor.py",
           ROOT / "upstream/FPGA_ORB_HLS/fastv2/next/src/descriptor_hls.cpp",
           ROOT / "upstream/FPGA_ORB_HLS/fastv2/next/src/descriptor2018_hls.cpp"]
data = dict(
    scope="Existing normal Production 100-frame evidence and source/HLS review; no new board run.",
    clock_mhz=100, baseline=old, four_ppc=new,
    patch_copy_cycles_per_feature=sum(copy_latencies), patch_copy_work_us_per_frame=copy_us,
    descriptor_sum_increase_us=descriptor_increase_us,
    ps_source_diff_after_name_normalization=source_diff,
    sources=[dict(path=str(path), sha256=digest(path)) for path in sources],
    caveats=[
        "Production is COMPLETE_UNVERIFIED; adjacent Verified normal 100-frame run passed exact checks.",
        "Cache/config timings overlap other categories; do not add them to input preparation and processing.",
        "Schedule residual includes PS bookkeeping, register start offsets, hardware tail/stats writes and rounding; not pure CPU timing.",
        "Patch copy work is not a measured additive frame penalty or guaranteed recoverable time; loader/compute overlap changes the critical path.",
        "PL counters include memory stalls. Existing logs cannot separate DDR arbitration, transaction latency and internal waiting.",
        "2PPC PL used 2024.2; 4PPC PL used 2018.3 with a changed patch transport. This is not a same-source tool-version-only comparison.",
    ],
)
(ROOT / "evidence/performance_20261012.json").write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

rows = [
    ("程序循环 / 帧", "wall_per_frame_us"),
    ("输入准备及循环管理 / 帧", "input_preparation_and_loop_us"),
    ("处理 / 帧", "processing_us"),
    ("PL 计数重建的理想调度路径 / 帧", "ideal_schedule_us"),
    ("处理与理想调度路径差值 / 帧", "processing_minus_ideal_schedule_us"),
    ("缓存维护 / 帧（包含在上面的时间内）", "cache_per_frame_us"),
    ("寄存器配置 / 帧（包含在上面的时间内）", "config_per_frame_us"),
]
table = "\n".join(f"| {label} | {old[key]/1000:.5f} | {new[key]/1000:.5f} |" for label, key in rows)
layers = "\n".join(f"| {i} | {new['counts'][i]} | {old['image_cycles'][i]} | {new['image_cycles'][i]} | {old['descriptor_cycles'][i]} | {new['descriptor_cycles'][i]} |" for i in range(8))
text = f"""# 4PPC 性能诊断：PS 与 PL

依据已接受的 normal Production 各 100 帧数据、实际编译的两版 board.c 和 2018.3 HLS 报告。两版相邻 normal Verified 100 帧均通过精确比较；Production 自身标记为 COMPLETE_UNVERIFIED。本次没有重新上板或改动设计、ELF、bit、已接受发布包。

PS 没有出现新退化：两版 CPU 配置、PS 初始化和缓存/调度方式一致，descriptor(n) 与 image(n+1) 确实并行。PS 在处理阶段的额外调度没有形成毫秒级空洞。不过现有程序每帧复制 691200 字节输入并刷新缓存，连同准备和循环管理约 {new['input_preparation_and_loop_us']/1000:.5f} ms，占循环总时间 {100*new['input_preparation_and_loop_us']/new['wall_per_frame_us']:.2f}%。因此 PS 输入路径确实限制整帧吞吐，不能笼统说完全没有开销。

| 指标（ms） | 2PPC | 4PPC |
| --- | ---: | ---: |
{table}

缓存和寄存器时间包含在输入准备或处理中，不能再次相加。输入准备数字为循环总时间减平均处理时间，其中还包含少量循环管理；未分别测量 memcpy、输入 flush 和 metadata。处理差值也包含硬件收尾、stats 写回和计数口径差异，不能将它全称为 CPU 时间。

整帧程序循环吞吐从 {old['whole_loop_fps']:.2f} 增至 {new['whole_loop_fps']:.2f} FPS；处理时间对应 {old['processing_only_fps']:.2f} 与 {new['processing_only_fps']:.2f} FPS。消除现有输入准备开销而保持 PL 不变，只能接近 {new['processing_only_fps']:.2f} FPS。140 FPS 要求全帧 7.14286 ms，当前处理阶段自身仍超过这一目标。

在 100 MHz 下，根据每核计数重建调度：T = I0 + Σ(l=0..6) max(Dl, I(l+1)) + D7。4PPC 的每个 Dl 都长于与之并行的下一层图像核，因此路径变为首层图像 {new['image_cycles'][0]/100000:.5f} ms + 八层描述子 {new['descriptor_sum_us']/1000:.5f} ms = {new['ideal_schedule_us']/1000:.5f} ms，与实测处理 {new['processing_us']/1000:.3f} ms 接近。这里不能把所有图像时间与所有描述子时间直接相加。

| 层 | 点数 | 2PPC 图像拍数 | 4PPC 图像拍数 | 2PPC 描述子拍数 | 4PPC 描述子拍数 |
| --- | ---: | ---: | ---: | ---: | ---: |
{layers}

八层描述子合计从 {old['descriptor_sum_us']/1000:.5f} 增至 {new['descriptor_sum_us']/1000:.5f} ms，增加 {descriptor_increase_us/1000:.5f} ms（{100*descriptor_increase_us/old['descriptor_sum_us']:.2f}%）。最后一层没有图像核并发，描述子仍变慢，因此不能仅用两个核争用 DDR 解释全部变化。

明确的实现开销位于 scripts/migrate_descriptor.py：旧实现通过 stream_of_blocks 的 write_lock/read_lock 直接共享 patch 存储；2018.3 兼容实现先输出 64 位字 FIFO，再在 compute_descriptors 中执行 CollectRawPatch（158 拍）与 CollectBlurPatch（222 拍），然后才运行描述子运算。真实 HLS 报告证实两个收集循环 II=1，合计 380 拍/点；280 点对应 {copy_us/1000:.3f} ms 的本地复制工作。该工作位于消费者的串行路径，但不能直接认定删除后会节省相同整帧时间：加载和计算的重叠、FIFO 背压和 DDR 等待会改变限制因素。生产者每行也串行清零、DDR 读取、FIFO 输出，增加请求间隙。

四路图像的 FAST/NMS、Gaussian、Resize 主循环均已达到 II=1；首层实际仍为 {new['image_cycles'][0]} 拍，理想 960×720/4 为 172800 拍。其余开销包括行准备、流水线填充/排空、候选串行合并、选点和 AXI 等待。normal 的各层点数均小于预算，使用 EmitCellOrder，不执行 EmitStrengthOrder 的全网格反复排序；不能用该排序分支解释本次 normal 性能。

结论：PS 配置和调度没有新增明显退化，输入准备有稳定的约 2.70 ms 开销；当前处理关键路径主要落在 PL 描述子，2018.3 兼容 patch 传递引入了确定的本地复制开销。既有 PL 计数包含 DDR/AXI 等待，尚不能量化内部运算和平台访存各占多少。两版 PL 工具版本和 patch 传递实现同时变化，不能将描述子增长归因于工具版本本身。

后续优化应先让 patch 加载与描述子计算通过双缓冲 RAM 重叠，减少 FIFO 到 RAM 的串行搬运，并用精确比较和实际 DDR 板测检查收益；现有 Slice 占用已达 100%，需要同时核查资源和路由。PS 侧可进一步优化输入所有权交接和缓存维护，减少每帧复制；必须保持缓存一致性。

机器可读计算、输入哈希及 PS 差异见 [performance_20261012.json](performance_20261012.json)。复算命令：

```powershell
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd' evidence/analyze_performance.py
```

直接证据：[4PPC Production](../../orb60_board/20261012_223016_4ppc100mhz_2018_3/reports/normal_Production.json)、[2PPC acceptance](../../orb60_board/20261012_172441_100mhz/acceptance.json)、[实际 PS 源码](../../orb60_board/20261012_223016_4ppc100mhz_2018_3/inputs/board.c)、[HLS 收集循环报告](../../orb60_board/20261012_223016_4ppc100mhz_2018_3/reports/descriptor_synthesis/p_anonymous_namespace_9_csynth.rpt)、[迁移生成器](../scripts/migrate_descriptor.py)。
"""
(ROOT / "evidence/performance_20261012.md").write_text(text, encoding="utf-8")
print(json.dumps(dict(baseline=old, four_ppc=new, patch_copy_work_us=copy_us,
                      descriptor_sum_increase_us=descriptor_increase_us), indent=2))
