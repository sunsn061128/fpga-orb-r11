"""Copy an actually accepted board experiment into a hash-checked portable package."""
from pathlib import Path
import argparse, datetime, hashlib, json, re, shutil

ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT.parent / 'orb60_board/20261012_172441_100mhz'

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--job', type=Path, required=True)
    args = parser.parse_args()
    job = args.job.resolve()
    state, pairing = read(job/'status.json'), read(job/'pairing.json')
    assert state['state'] == 'SUCCEEDED' and (job/'vivado/accepted.stamp').is_file()
    assert pairing['State'] == 'PASS'
    for item in pairing['Files']:
        assert digest(Path(item['Path'])) == item['SHA256']
    runs = [read(p) | {'status_file': str(p)} for p in job.glob('*.json')
            if '_Verified_' in p.name or '_Production_' in p.name]
    accepted = {}
    for frame, mode, minimum in [('normal','Verified',100),('normal','Production',100),
                                  ('noise','Verified',10),('zero','Verified',10)]:
        candidates = [r for r in runs if r['state']=='SUCCEEDED' and r['frame']==frame
                      and r['mode']==mode and r['frames']>=minimum and r['job']==str(job)]
        assert candidates, (frame, mode, 'No physical acceptance')
        chosen = max(candidates, key=lambda r: r['status_file'])
        match = re.search(r'ORB60_MAILBOX_WORDS=([^\r\n]+)', Path(chosen['log']).read_text())
        assert match and [int(v,0) for v in match[1].split()] == chosen['mailbox']
        assert chosen['mailbox'][4] == (2 if mode=='Verified' else 4)
        assert chosen['mailbox'][5] == 0 and chosen['mailbox'][6] == chosen['frames']
        accepted[frame+'_'+mode] = chosen
    target = ROOT.parent/'orb60_board'/(
        datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'_4ppc100mhz_2018_3')
    target.mkdir()  # A published package is never overwritten.
    (target/'inputs').mkdir(); (target/'elf').mkdir(); (target/'reports').mkdir()
    shutil.copy2(job/'orb60_4ppc.bit', target/'orb60_4ppc.bit')
    shutil.copy2(job/'elf/orb60_board.elf', target/'elf/orb60_board.elf')
    for name in ('run.tcl','ps7_init.tcl'):
        shutil.copy2(job/'inputs'/name, target/'inputs'/name)
    for name in ('normal','noise','zero'):
        shutil.copytree(job/name, target/name)
    for name in ('board.c','xfastv2_layer4_hls_hw.h','xfastv2_descriptor_hls_hw.h'):
        shutil.copy2(job/'ps_inputs'/name, target/'inputs'/name)
    for name in ('status.json','pairing.json','arm_compile.log',
                 'mock_compile.log','mock_normal.log','mock_noise.log','mock_zero.log'):
        shutil.copy2(job/name, target/'reports'/('source_'+name))
    manifest_job = job if (job/'input_manifest.json').exists() else Path(state['reused_routed_from'])
    for item in read(manifest_job/'input_manifest.json'):
        assert digest(manifest_job/item['path']).lower() == item['sha256'].lower()
    shutil.copy2(manifest_job/'input_manifest.json',target/'reports/source_input_manifest.json')
    for path in (job/'vivado').glob('post_route_*'):
        if path.is_file(): shutil.copy2(path, target/'reports'/path.name)
    for key, run in accepted.items():
        shutil.copy2(Path(run['status_file']),target/'reports'/(key+'.json'))
        shutil.copy2(Path(run['log']),target/'reports'/(key+'.log'))
    failed = [run for run in runs if run['state']=='FAILED']
    if failed:
        (target/'reports/failed_sessions').mkdir()
        for run in failed:
            path = Path(run['status_file'])
            shutil.copy2(path,target/'reports/failed_sessions'/path.name)
            if path.with_suffix('.log').exists():
                shutil.copy2(path.with_suffix('.log'),target/'reports/failed_sessions'/path.with_suffix('.log').name)
        for path in (ROOT/'evidence').glob('*'):
            if path.is_file() and (path.name.startswith('link_') or path.name.startswith('target_probe_after_')):
                shutil.copy2(path,target/'reports/failed_sessions'/path.name)
    shutil.copy2(BASELINE/'acceptance.json',target/'reports/baseline_2ppc_acceptance.json')
    for kernel in ('layer4','descriptor'):
        hls = Path(state['full_layer_cosim' if kernel=='layer4' else 'full_descriptor_cosim'])
        # The working tree may already contain the next experiment. Publish
        # the native proof and author manifest frozen with this accepted RTL.
        for item in read(hls/'source_manifest.json'):
            assert digest(hls/item['path']).lower() == item['sha256'].lower()
        shutil.copy2(hls/'generated/status.json',target/'reports'/(kernel+'_host_status.json'))
        shutil.copy2(hls/'source_manifest.json',target/'reports'/(kernel+'_source_manifest.json'))
        cosim = hls/f'project/solution1/sim/report/fastv2_{kernel}_hls_cosim.rpt'
        assert re.search(r'\|\s*Verilog\s*\|\s*Pass\s*\|',cosim.read_text())
        shutil.copy2(cosim,target/'reports'/(kernel+'_cosim.rpt'))
        shutil.copy2(hls/'status.json',target/'reports'/(kernel+'_hls_status.json'))
        shutil.copy2(hls/f'project/solution1/csim/report/fastv2_{kernel}_hls_csim.log',
                     target/'reports'/(kernel+'_csim.log'))
        (target/'reports'/(kernel+'_synthesis')).mkdir()
        for path in (hls/'project/solution1/syn/report').glob('*.rpt'):
            shutil.copy2(path,target/'reports'/(kernel+'_synthesis')/path.name)
        axi = Path(state['axi_layer' if kernel=='layer4' else 'axi_descriptor'])
        for name in ('status.json','result.json','rtl_manifest.json'):
            shutil.copy2(axi/name,target/'reports'/(kernel+'_axi_'+name))
    metrics = dict(line.split('\t') for line in
                   (job/'vivado/post_route_metrics.tsv').read_text().splitlines())
    utilization = (job/'vivado/post_route_utilization.rpt').read_text()
    resources = {}
    for name in ('Slice LUTs','Slice Registers','Block RAM Tile','DSPs','Slice'):
        match = re.search(r'\|\s*'+re.escape(name)+r'\s*\|\s*([0-9.]+)\s*\|',utilization)
        assert match, name
        resources[name] = float(match[1])
    old = read(BASELINE/'acceptance.json')['Results']['production']['Words']
    prod = accepted['normal_Production']
    direct_ddr = prod['mailbox'][78] == 0x44445231
    input_contract = ('direct immutable resident DDR; producer handoff before invocation; external capture/transfer excluded'
                      if direct_ddr else 'per-frame copy from resident fixture; external capture/transfer excluded')
    implementation = state.get('implementation','Vivado Performance_Explore integrated implementation')
    if state.get('no_fanout_opt'):
        implementation += '; place -no_fanout_opt'
    if state.get('carry_remap') is not None:
        implementation += '; CARRY_REMAP='+str(state['carry_remap'])
    comparison = dict(baseline_processing_ms=old[10]/1000,
                      processing_ms=prod['processing_avg_us']/1000,
                      baseline_wall_ms_per_frame=old[11]/old[6]/1000,
                      wall_ms_per_frame=prod['wall_us']/prod['frames']/1000,
                      baseline_wall_fps=old[6]*1e6/old[11],
                      wall_fps=prod['frames']*1e6/prod['wall_us'],
                      processing_speedup=old[10]/prod['processing_avg_us'],
                      wall_speedup=(old[11]/old[6])/(prod['wall_us']/prod['frames']))
    previous_4ppc = {}
    for label, directory in (
        ('original_4ppc', '20261012_223016_4ppc100mhz_2018_3'),
        ('ps_direct_ddr', '20261012_235156_4ppc100mhz_2018_3')):
        release = ROOT.parent/'orb60_board'/directory
        prior = read(release/'acceptance.json')
        item = next(item for item in read(release/'manifest.json')['Files']
                    if item['Path'] == 'acceptance.json')
        assert digest(release/'acceptance.json') == item['SHA256']
        assert prior['State']=='PASS' and prior['Part']==state['part'] and prior['PLMHz']==100
        run = prior['PhysicalResults']['normal_Production']
        assert run['state']=='SUCCEEDED' and run['frames']>=100 and run['features']==prod['features']
        fps = run['frames']*1e6/run['wall_us']
        previous_4ppc[label] = dict(source_release=str(release),
            processing_ms=run['processing_avg_us']/1000,
            wall_ms_per_frame=run['wall_us']/run['frames']/1000, wall_fps=fps,
            wall_gain_percent=(comparison['wall_fps']/fps-1)*100)
        shutil.copy2(release/'acceptance.json',target/'reports'/(label+'_acceptance.json'))
    acceptance = dict(State='PASS',Board='ACZ7015',Cable='C306ACCBABCD',Part=state['part'],
                      PLMHz=100,Toolchain=pairing['Toolchain'],SourceJob=str(job),
                      Metrics=metrics,Resources=resources,PhysicalResults=accepted,
                      Comparison=comparison,VolatileSRAMOnly=True,
                      Previous4PPC=previous_4ppc,
                      ProductionResult='COMPLETE_UNVERIFIED',
                      InputContract=input_contract,
                      Implementation=implementation,
                      HardwareDefinition=state.get('hardware_definition','Generated board HDF; exact PS init and BSP configuration pairing'))
    (target/'acceptance.json').write_text(json.dumps(acceptance,indent=2)+'\n')
    (target/'pairing.json').write_text(json.dumps(dict(State='PASS',SourceJob=str(job),
        SourcePairing='reports/source_pairing.json',PackageHashes='manifest.json'),indent=2)+'\n')
    (target/'run_verified.ps1').write_text(r'''[CmdletBinding()]
param([ValidateSet('normal','noise','zero')][string]$Frame='normal',
      [ValidateRange(1,1000)][int]$Frames=100)
$ErrorActionPreference='Stop'
$orb4Package=$PSScriptRoot
$orb4Manifest=Get-Content -LiteralPath (Join-Path $orb4Package 'manifest.json') -Raw|ConvertFrom-Json
foreach($orb4Item in $orb4Manifest.Files){
    $orb4File=Join-Path $orb4Package $orb4Item.Path
    if((Get-FileHash -LiteralPath $orb4File -Algorithm SHA256).Hash -ne $orb4Item.SHA256){throw "Package hash mismatch: $orb4File"}
}
$orb4Log=Join-Path $orb4Package ((Get-Date -Format 'yyyyMMdd_HHmmss')+"_${Frame}_Verified_${Frames}.log")
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd' xsct (Join-Path $orb4Package 'inputs/run.tcl') $orb4Package $Frame $Frames Verified *> $orb4Log
$orb4Text=[IO.File]::ReadAllText($orb4Log)
$orb4Match=[regex]::Match($orb4Text,'ORB60_MAILBOX_WORDS=([^\r\n]+)')
if(-not $orb4Match.Success -or $orb4Text.Contains('ERROR:')){throw "Board run failed; see $orb4Log"}
$orb4Words=@($orb4Match.Groups[1].Value.Trim() -split '\s+'|ForEach-Object{[UInt64]::Parse($_)})
$orb4Golden=[IO.File]::ReadAllBytes((Join-Path $orb4Package "$Frame/golden.bin"))
if($orb4Words.Count -ne 128 -or $orb4Words[2] -ne 0x60c00002 -or $orb4Words[4] -ne 2 -or $orb4Words[5] -ne 0 -or $orb4Words[6] -ne $Frames -or $orb4Words[16] -ne [BitConverter]::ToUInt32($orb4Golden,12)){throw "Board exact check failed; see $orb4Log"}
Write-Output "CPU_EXACT_PASS frame=$Frame frames=$Frames features=$($orb4Words[16]) log=$orb4Log"
''',encoding='utf-8')
    text = f'''# ORB60 4PPC 板测结果

100 MHz / XC7Z015CLG485-2 / Vivado HLS、Vivado、SDK 2018.3。bit 和 ELF 已在 ACZ7015 上实际运行。

| 输入 | 模式 | 完成帧数 | 每帧特征数 | 结果 |
|---|---|---:|---:|---|
'''
    for run in accepted.values():
        text += f"| {run['frame']} | {run['mode']} | {run['frames']} | {run['features']} | {run['result']} |\n"
    text += f'''
Verified 逐字节比较八层图像、选点、摘要、描述子和未写区域，同时检查新核的 4PPC 标志。Production 测试沿用刚通过校验的硬件配置及 normal 输入，独立结果为 COMPLETE_UNVERIFIED。

| normal，100 帧 Production | 原 2PPC / 2024.2 bit | 本版 4PPC / 2018.3 bit |
|---|---:|---:|
| 处理平均时间 | {comparison['baseline_processing_ms']:.3f} ms | {comparison['processing_ms']:.3f} ms |
| 程序循环总时间 / 帧 | {comparison['baseline_wall_ms_per_frame']:.5f} ms | {comparison['wall_ms_per_frame']:.5f} ms |
| 程序循环吞吐 | {comparison['baseline_wall_fps']:.2f} FPS | {comparison['wall_fps']:.2f} FPS |

处理速度为原版的 {comparison['processing_speedup']:.3f} 倍，程序循环吞吐为原版的 {comparison['wall_speedup']:.3f} 倍。处理时间从首层核启动计时，含后续缓存维护、寄存器配置、等待和调度；程序循环总时间还包含启动前准备，不含 JTAG 下载及外部采集/传输。本版输入合同：{input_contract}。两版同时存在 PPC、工具版本和实现变化，因此该比较不能单独归因于 PPC。

Production 的处理时间最小 {prod['processing_min_us']/1000:.3f} ms，最大 {prod['processing_max_us']/1000:.3f} ms，100 帧程序循环总时间 {prod['wall_us']/1000:.3f} ms。下表为核内实际计数器的每帧平均周期，未包含末尾统计写回；完整处理时间以 PS 计时为准。

同为 2018.3 / 100 MHz / normal 八层 280 特征、100 帧 Production 的已验收阶段：

| 阶段 | 处理平均时间 | 程序循环时间 / 帧 | 程序循环吞吐 |
|---|---:|---:|---:|
| 原 4PPC | {previous_4ppc['original_4ppc']['processing_ms']:.3f} ms | {previous_4ppc['original_4ppc']['wall_ms_per_frame']:.5f} ms | {previous_4ppc['original_4ppc']['wall_fps']:.2f} FPS |
| 4PPC + PS 直接 DDR 输入 | {previous_4ppc['ps_direct_ddr']['processing_ms']:.3f} ms | {previous_4ppc['ps_direct_ddr']['wall_ms_per_frame']:.5f} ms | {previous_4ppc['ps_direct_ddr']['wall_fps']:.2f} FPS |
| 本版 | {comparison['processing_ms']:.3f} ms | {comparison['wall_ms_per_frame']:.5f} ms | {comparison['wall_fps']:.2f} FPS |

本版相对原 4PPC 的程序循环吞吐变化为 {previous_4ppc['original_4ppc']['wall_gain_percent']:+.2f}%，相对 PS 直接 DDR 版本为 {previous_4ppc['ps_direct_ddr']['wall_gain_percent']:+.2f}%。历史验收记录及哈希已核对，副本保留于 reports/*_acceptance.json。后两版采用相同的常驻、不可变 DDR 输入合同；原 4PPC 的每帧准备仍包含 CPU 输入复制。

| 层 | 2PPC 图像周期 | 4PPC 图像周期 | 原版描述子周期 | 本版描述子周期 |
|---|---:|---:|---:|---:|
'''
    for level in range(8):
        text += f"| {level} | {old[20+level]} | {prod['mailbox'][20+level]} | {old[28+level]} | {prod['mailbox'][28+level]} |\n"
    text += f'''
核内周期与整帧计时口径不同，整帧包含缓存维护、配置、等待和调度。数值一致已由独立仿真及板测确认；版本迁移不保证相同资源、延迟或物理实现。

最终 routed WNS={metrics['wns_ns']} ns，WHS={metrics['whs_ns']} ns；无未约束内部路径、路由错误及 Error/Critical Warning DRC。LUT {resources['Slice LUTs']:g}/46200，FF {resources['Slice Registers']:g}/92400，BRAM {resources['Block RAM Tile']:g}/95，DSP {resources['DSPs']:g}/160，Slice {resources['Slice']:g}/11550。Slice 利用率很高，后续加逻辑需要重新检查装箱及路由。

普通 DRC Warning 的具体数量、类别和路径保留于 reports/post_route_drc.rpt；接受门槛拒绝 Error/Critical Warning。setup/hold 通过不表示异步复位已被 STA 覆盖；独立 HLS 核的复位中断测试通过，整机板测采用初始化后的固定配置，未测试在运行中复位整个 PS/AXI 互联。

验证还包括独立 CPU 精确测试、完整 Verilog C/RTL 联合仿真（图像 21 用例、描述子 30 用例 / 1222 特征）及真实 IP 的随机 AXI 背压、延迟写响应、4 KB 边界、复位中断和最终响应完成屏障。AXI 仿真是独立核测试；整机共用 DDR 的验证依据是这里的实际板测。

实际接受实现：{implementation}。{'本次仅更换 PS 程序，复用已接受 routed bit：'+state['reused_routed_from'] if state.get('reused_routed_from') else '详细构建来源见 reports/source_status.json。'} 2018.3 IP 日期 revision 溢出仅在生成目录内重打包修复。复用的 PS standalone 库经过 876 项 PS 配置比较；相应工程的 PS init 与该库对应配置逐字节一致。PL 寄存器 ABI 来自与 bit 配对的 IP 导出头文件。硬件定义来源：{acceptance['HardwareDefinition']}。

源码工程：`{ROOT}`。实际接受构建：`{job}`。`reports/source_pairing.json` 保存原始绝对路径来源；本包内容校验使用 `manifest.json` 的相对路径 SHA-256。

此构建的失败板测会话如有发生，保留于 reports/failed_sessions；仅 SUCCEEDED 的完整物理校验作为接受依据。

复测：在 Windows PowerShell 中运行 `./run_verified.ps1 -Frame normal -Frames 100`（也可选 noise/zero）。启动器会检查包内哈希、指定 JTAG 线和器件，执行易失性 SRAM 下载并检查稳定终态。依赖本机隔离的 2018.3 profile；无需新 HDF。不写 Flash、SD 或 eFUSE。
'''
    (target/'RESULTS.md').write_text(text,encoding='utf-8')
    files = [dict(Path=p.relative_to(target).as_posix(),SHA256=digest(p))
             for p in sorted(target.rglob('*')) if p.is_file()]
    (target/'manifest.json').write_text(json.dumps(dict(Files=files),indent=2)+'\n')
    (ROOT/'evidence/latest_release.json').write_text(json.dumps(
        dict(Path=str(target),Acceptance=str(target/'acceptance.json'),Comparison=comparison),indent=2)+'\n')
    print(json.dumps(dict(state='RELEASE_EXPORTED',path=str(target),comparison=comparison)),flush=True)

if __name__ == '__main__':
    main()
