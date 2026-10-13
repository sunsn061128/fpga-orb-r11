"""Summarize accepted physical board captures; never modify published evidence."""
from pathlib import Path
import json

root = Path(__file__).resolve().parent
state = json.loads((root / 'board_status.json').read_text(encoding='utf-8-sig'))
results = state['Results']
verified = [r for r in results if r['Mode'] == 'Verified']
production = [r for r in results if r['Mode'] == 'Production']
assert state['State'] == 'SUCCEEDED'
assert len(verified) == len(production) == 6
assert {(r['FrequencyMHz'], r['Frame']) for r in verified} == {(f, x) for f in (100, 125) for x in ('normal', 'noise', 'zero')}
assert {(r['FrequencyMHz'], r['Frame']) for r in production} == {(f, x) for f in (100, 125) for x in ('normal', 'noise', 'zero')}
for r in results:
    assert r['State'] == 'SUCCEEDED'
    assert r['Mailbox'][5] == 0 and r['Mailbox'][6] == r['Frames']
    assert r['Mailbox'][80] == 0x600003 and r['Mailbox'][96] == 0x600002
    assert r['Features'] == {'normal': 280, 'noise': 942, 'zero': 0}[r['Frame']]
    body = Path(r['Log']).read_text(encoding='utf-8-sig')
    assert f"ORB60_CLOCK_PROOF frequency={r['FrequencyMHz']} " in body

lines = [
    '# ORB60 integrated4 100/125 MHz 实际板测', '',
    '两版原始发布 bit 已在 ALINX ACZ7015 / XC7Z015 上完成实际运行。Digilent JTAG-SMT2 序列号 C306ACCBABCD，IDCODE 0373b093；JTAG 1 MHz，全部为易失性 SRAM 配置。没有写入 Flash、SD 或 eFUSE。', '',
    'PL 为包内 Vivado/Vitis HLS 2024.2 原始 bit，未重新综合或改写。新的两核 PS 测试 ELF 采用 SDK ARM GCC/standalone 2018.3 兼容流程；只使用 PS 服务和对应新核导出的寄存器头文件。不存在旧三核 ELF 配对或新完整 XSA 的声明。', '',
    'PS 基础配置的 876 项参数与已验收板级 runtime 相同；100 MHz 重新生成的 ps7_init.tcl 与原已验收初始化逐字节相同。125 MHz 与 100 MHz 的初始化仅改变 IO PLL 及其配置、PCAP 分频和 FCLK0 分频；ARM/DDR/MIO 配置相同。生成器记录的 FCLK0 为准确 100/125 MHz，每次运行还读回 FCLK0 与 IO PLL 配置寄存器进行配对检查。这里是寄存器配置证明，没有示波器实测频率。', '',
    '独立 CPU golden 从本包原始 constants.hpp 和图像重新生成；未导入 DUT 描述子偏移 ROM。Verified 逐字节比较八层 blur/resize、selected 点、summary、方向及 256-bit BRIEF feature 记录、追加前缀和未写区域，并检查新 4PPC 统计版本。连续运行包含描述子层 n 与图像层 n+1 的重叠。', '',
    '| 频率 | 输入 | Verified 帧数 | 特征/帧 | 精确结果 |',
    '|---|---|---:|---:|---|',
]
for r in verified:
    lines.append(f"| {r['FrequencyMHz']} MHz | {r['Frame']} | {r['Frames']} | {r['Features']} | CPU_EXACT_PASS |")
lines += ['',
    '性能采用同一刚通过精确检查的硬件、ELF 和 DDR 输入，核/输入不重新下载，仅重载同一 ELF。复测脚本比较立即前一完整 mailbox、bit/ELF 哈希、golden header 及时钟配置，避免混用会话。每项 Production 连续运行 1000 帧，稳定终态为 COMPLETE_UNVERIFIED；关闭逐字节比较后的性能运行本身不宣称精确 PASS。', '',
    '| 频率 | 输入 | 平均处理 ms | 最大处理 ms | 程序循环 ms/帧 | 程序循环 FPS |',
    '|---|---|---:|---:|---:|---:|',
]
for r in production:
    lines.append(f"| {r['FrequencyMHz']} MHz | {r['Frame']} | {r['ProcessingAvgUs']/1000:.3f} | {r['ProcessingMaxUs']/1000:.3f} | {r['WallUsPerFrame']/1000:.5f} | {r['WallFPS']:.2f} |")
normal = {r['FrequencyMHz']: r for r in production if r['Frame'] == 'normal'}
noise = {r['FrequencyMHz']: r for r in production if r['Frame'] == 'noise'}
lines += ['', f"125 MHz 相对 100 MHz 的程序循环吞吐：普通图提高 {(normal[125]['WallFPS']/normal[100]['WallFPS']-1)*100:.2f}%，噪声图提高 {(noise[125]['WallFPS']/noise[100]['WallFPS']-1)*100:.2f}%。"]
lines += ['',
    '处理时间从首个图像核启动计算，包含后续 cache 维护、MMIO 配置、等待与软件调度；程序循环时间还包含每帧启动前准备。输入为提前交接的常驻、不可变 DDR，未计外部采集、传输和 JTAG 下载，也未代表实际视频输入、视频 P99 或长期持续负载。Verified 的额外 poison 初始化与比对使时间不可用于生产帧率比较。核内周期排除末尾统计写回，以 PS 整帧时间作为板级性能口径。', '',
    '测试最终保留 125 MHz bit 于 SRAM，ARM 在测试终态被调试器停止，PL 核已完成。掉电会丢失 SRAM 配置。', '',
    '配对产物与完整捕获：', '',
]
for frequency in (100, 125):
    manifest = json.loads((root / f'{frequency}mhz/manifest.json').read_text(encoding='utf-8-sig'))
    lines += [f"- {frequency} MHz bit SHA256：`{manifest['BitSHA256'].lower()}`。",
              f"- {frequency} MHz ELF SHA256：`{manifest['ELFSHA256'].lower()}`。"]
lines += ['',
    '全部成功运行的 128-word mailbox、每层计数器、时钟读回、时延和原始日志路径见 board_status.json；各频率目录保留独立 .log/.json。上一轮下载器断开时的失败日志及 PS 初始化生成尝试保留，不计入成功结果。本轮 100 MHz 空图 Production 前也发生一次会话枚举失败，尚未启动该次运行；Windows USB 身份及新发现 JTAG 链正确后，增加最多五次有界发现重试，并比较完整前序 mailbox 恢复剩余测试。初次中止状态保留为 board_status_attempt1.json，没有重复执行已经完成的硬件门禁。', '',
    '复测命令（需保持对应 JTAG/开发板连接）：', '',
    '```powershell',
    'pwsh -NoProfile -File run_board.ps1 -FrequencyMHz 100 -Frame normal -Frames 100 -Mode Verified',
    'pwsh -NoProfile -File run_board.ps1 -FrequencyMHz 125 -Frame noise -Frames 10 -Mode Verified',
    'pwsh -NoProfile -File run_acceptance.ps1',
    '```', '',
    '所用工具：process-isolated C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd；Vivado/SDK/XSCT/hw_server 2018.3。ARM 编译器实测版本 Linaro GCC 7.3-2018.04-rc3 / 7.3.1 20180314，路径 C:/Xilinx/SDK/2018.3/gnu/aarch32/nt/gcc-arm-none-eabi/bin/arm-none-eabi-gcc.exe。Native PS controller mock 使用 C:/Users/dmsun/mingw/mingw64/bin/gcc.exe，实测 MinGW 14.2.0，包含名义调度、100 帧 Production、重叠、4 类故障及 5 类非法输入检查；mock 不证明硬件数学或性能。ARM ELF 构建、内存边界、配对哈希、新寄存器 ABI 与实际板测通过。上板测试未重跑包内已经通过的 HLS/RTL/路由门禁。', '',
    'PS 初始化生成的 BD 41-737 警告来自源配置导入只读派生频率参数；两版输出生成完成，关键实际频率、全部初始化差异与匹配源参数另行核对。首次生成缺少 AXI 时钟连接的失败已修复，采用 psgen_v2_* 的完成产物。',
]
(root / 'RESULTS_ZH.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
acceptance = dict(state='SUCCEEDED', board='ACZ7015', part='xc7z015clg485-2', top='s_wrapper', cable_serial='C306ACCBABCD',
                  physical_board_verified=True, verified_frames=sum(r['Frames'] for r in verified), production_frames=sum(r['Frames'] for r in production),
                  results=results, report=str(root / 'RESULTS_ZH.md'),
                  scope='Exact synthetic/fixture board checks plus repeated resident-DDR performance; external input transfer and representative-video P99 unmeasured')
(root / 'acceptance.json').write_text(json.dumps(acceptance, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('PHYSICAL_BOARD_REPORT_READY', root / 'RESULTS_ZH.md')
