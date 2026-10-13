[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$JobRoot)
$ErrorActionPreference='Stop'
# Load the utility module from this PowerShell process, even when a parent
# application supplies module paths for a different PowerShell runtime.
Import-Module -Name (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1') -Force -ErrorAction Stop
$orb4Root=Split-Path -Parent $PSScriptRoot
$orb4Job=[IO.Path]::GetFullPath($JobRoot)
$orb4State=Get-Content -LiteralPath (Join-Path $orb4Job 'status.json') -Raw | ConvertFrom-Json
if($orb4State.state -ne 'SUCCEEDED' -or -not(Test-Path -LiteralPath (Join-Path $orb4Job 'vivado/accepted.stamp'))) {throw 'Require accepted routed board job'}
if(Test-Path -LiteralPath (Join-Path $orb4Job 'elf')) {throw 'Refuse existing ELF build'}
$orb4Runtime='C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\fastv2\next\board_build\jobs\20261012_012547_bf5bab91'
$orb4ReferenceRuntime=$orb4Runtime
$orb4Frequency=[int]$orb4State.frequency_mhz
if($orb4Frequency -notin @(100,125)) {throw 'Unsupported candidate frequency'}
$orb4PsGeneratedJob=if($orb4State.checkpoint_parent) {$orb4State.checkpoint_parent} else {$orb4Job}
if($orb4Frequency -eq 125) {
    $orb4Runtime=Join-Path $orb4PsGeneratedJob 'ps_runtime'
    $orb4RuntimeState=Get-Content -LiteralPath (Join-Path $orb4Runtime 'status.json') -Raw | ConvertFrom-Json
    if($orb4RuntimeState.state -ne 'SUCCEEDED' -or $orb4RuntimeState.frequency_mhz -ne 125 -or $orb4RuntimeState.job -ne $orb4PsGeneratedJob) {throw 'Require fresh candidate SDK runtime'}
    foreach($orb4File in @($orb4RuntimeState.inputs)+@($orb4RuntimeState.files)) {
        if((Get-FileHash -LiteralPath (Join-Path $orb4Runtime $orb4File.path)).Hash.ToLowerInvariant() -ne $orb4File.sha256) {throw 'Candidate SDK runtime hash drift'}
    }
}
$orb4Bsp=Join-Path $orb4Runtime 'sdk/next_bsp/ps7_cortexa9_0'
# Reuse only PS standalone services after exact PS configuration comparison.
$orb4Rx='CONFIG\.(PCW_\S+)\s+(\{[^}]*\}|\S+)'
$orb4Configs=@()
foreach($orb4Path in @((Join-Path $orb4Job 'inputs/ps_config.tcl'),(Join-Path $orb4ReferenceRuntime 'inputs/ps_config.tcl'))) {
    $orb4Dict=@{};[regex]::Matches([IO.File]::ReadAllText($orb4Path),$orb4Rx)|ForEach-Object{$orb4Dict[$_.Groups[1].Value]=$_.Groups[2].Value};$orb4Configs+=,$orb4Dict
}
if($orb4Configs[0].Count -ne $orb4Configs[1].Count) {throw 'PS configuration count mismatch'}
foreach($orb4Key in $orb4Configs[0].Keys) {if($orb4Configs[0][$orb4Key] -ne $orb4Configs[1][$orb4Key]) {throw "PS configuration mismatch: $orb4Key"}}
foreach($orb4Path in @((Join-Path $orb4Job 'inputs/build_system.tcl'),(Join-Path $orb4ReferenceRuntime 'inputs/build_system.tcl'))) {
    $orb4Text=[IO.File]::ReadAllText($orb4Path)
    foreach($orb4Hp in 0..3) {if($orb4Text -notmatch "CONFIG.PCW_USE_S_AXI_HP$orb4Hp\s+\{1\}") {throw 'HP override mismatch'}}
    if($orb4Text -notmatch 'CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ\s+(\{100\}|\$frequency)') {throw 'FCLK setup mismatch'}
}
$orb4CpuRx='#define\s+XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ\s+(\d+)'
$orb4CpuClocks=@()
foreach($orb4Include in @((Join-Path $orb4Bsp 'include'),(Join-Path $orb4ReferenceRuntime 'sdk/next_bsp/ps7_cortexa9_0/include'))) {
    $orb4CpuText=[IO.File]::ReadAllText((Join-Path $orb4Include 'xparameters.h'))
    $orb4CpuMatch=[regex]::Match($orb4CpuText,$orb4CpuRx)
    if(-not $orb4CpuMatch.Success) {throw 'Missing CPU timer clock'}
    $orb4CpuClocks+=,[long]$orb4CpuMatch.Groups[1].Value
}
if($orb4CpuClocks[0] -ne $orb4CpuClocks[1]) {throw 'CPU timer clock differs from baseline'}
$orb4Inputs=Join-Path $orb4Job 'ps_inputs';$orb4Elf=Join-Path $orb4Job 'elf'
New-Item -ItemType Directory -Path $orb4Inputs,$orb4Elf | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'board.c'),(Join-Path $PSScriptRoot 'test_mock.c'),(Join-Path $PSScriptRoot 'mock.h'),(Join-Path $PSScriptRoot 'build.ps1'),(Join-Path $PSScriptRoot 'prepare.py') -Destination $orb4Inputs
foreach($orb4Kernel in @('layer4','descriptor')) {
    $orb4Source=if($orb4Kernel -eq 'layer4') {$orb4State.layer_hls} else {$orb4State.descriptor_hls}
    Copy-Item -LiteralPath (Join-Path $orb4Source "project/solution1/impl/ip/drivers/fastv2_${orb4Kernel}_hls_v1_0/src/xfastv2_${orb4Kernel}_hls_hw.h") -Destination $orb4Inputs
}
Copy-Item -LiteralPath (Join-Path $orb4Runtime 'sdk/next_benchmark/src/lscript.ld'),(Join-Path $orb4Runtime 'sdk/next_benchmark/Debug/Xilinx.spec') -Destination $orb4Inputs
$orb4FreshInit=Join-Path $orb4PsGeneratedJob 'vivado/p.srcs/sources_1/bd/s/ip/s_ps7_0/ps7_init.tcl'
if((Get-FileHash -LiteralPath $orb4FreshInit).Hash -ne (Get-FileHash -LiteralPath (Join-Path $orb4Runtime 'sdk/next_hw/ps7_init.tcl')).Hash) {throw 'Generated PS init differs from the reused standalone runtime configuration'}
Copy-Item -LiteralPath $orb4FreshInit -Destination (Join-Path $orb4Job 'inputs')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'run.tcl'),(Join-Path $PSScriptRoot 'rerun.tcl'),(Join-Path $PSScriptRoot 'clock_check.tcl') -Destination (Join-Path $orb4Job 'inputs')
Copy-Item -LiteralPath (Join-Path $orb4Bsp 'include'),(Join-Path $orb4Bsp 'lib') -Destination $orb4Inputs -Recurse
$orb4Bit=if($orb4State.bit_path) {$orb4State.bit_path} else {Join-Path $orb4Job 'vivado/p.runs/impl_1/s_wrapper.bit'}
Copy-Item -LiteralPath $orb4Bit -Destination (Join-Path $orb4Job 'orb60_4ppc.bit')
foreach($orb4Label in @('normal','noise','zero')) {Copy-Item -LiteralPath (Join-Path $orb4Root "../orb60_board/20261012_172441_100mhz/$orb4Label") -Destination $orb4Job -Recurse}
$orb4Native='C:\Users\dmsun\mingw\mingw64\bin\gcc.exe'
$orb4SavedPath=$env:PATH
try {
$env:PATH=(Split-Path -Parent $orb4Native)+';'+$env:SystemRoot+'\System32;'+$env:SystemRoot
& $orb4Native -std=c11 -O2 -Wall -Wextra '-Wno-unused-function' "-I$orb4Inputs" (Join-Path $orb4Inputs 'test_mock.c') -o (Join-Path $orb4Job 'test_mock.exe') *> (Join-Path $orb4Job 'mock_compile.log')
if($LASTEXITCODE) {throw 'PS mock compile failed'}
foreach($orb4Label in @('normal','noise','zero')) {
    & (Join-Path $orb4Job 'test_mock.exe') (Join-Path $orb4Job "$orb4Label/golden.bin") *> (Join-Path $orb4Job "mock_$orb4Label.log")
    if($LASTEXITCODE) {throw "PS schedule/cache/guard mock failed: $orb4Label"}
}
$orb4Compiler='C:\Xilinx\SDK\2018.3\gnu\aarch32\nt\gcc-arm-none-eabi\bin\arm-none-eabi-gcc.exe'
$orb4Wrapper='C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$orb4Arguments=@('-std=c11','-O2','-g','-Wall','-Wextra','-mcpu=cortex-a9','-mfpu=vfpv3','-mfloat-abi=hard','-Wl,-build-id=none',('-specs="{0}"' -f (Join-Path $orb4Inputs 'Xilinx.spec')),('-I"{0}"' -f $orb4Inputs),('-I"{0}"' -f (Join-Path $orb4Inputs 'include')),('-L"{0}"' -f (Join-Path $orb4Inputs 'lib')),('-Wl,-T,"{0}"' -f (Join-Path $orb4Inputs 'lscript.ld')),('-Wl,-Map,"{0}"' -f (Join-Path $orb4Elf 'board.map')),('"{0}"' -f (Join-Path $orb4Inputs 'board.c')),'-Wl,--start-group,-lxil,-lgcc,-lc,--end-group','-o',('"{0}"' -f (Join-Path $orb4Elf 'orb60_board.elf')))
# Redirect in cmd so Windows PowerShell does not turn compiler diagnostics on
# stderr into terminating NativeCommandError exceptions. Check its real exit code.
& $env:ComSpec /d /s /c ('""{0}" "{1}" {2} >"{3}" 2>&1"' -f $orb4Wrapper,$orb4Compiler,($orb4Arguments -join ' '),(Join-Path $orb4Job 'arm_compile.log'))
if($LASTEXITCODE) {throw 'ARM compile failed'}
$orb4Symbols=& (Join-Path (Split-Path -Parent $orb4Compiler) 'arm-none-eabi-nm.exe') (Join-Path $orb4Elf 'orb60_board.elf')
$orb4Symbols | Set-Content -LiteralPath (Join-Path $orb4Job 'arm_symbols.txt')
$orb4End=@($orb4Symbols | Select-String '^([0-9a-fA-F]+)\s+\w\s+_end$')
if($orb4End.Count -ne 1 -or [Convert]::ToUInt32($orb4End[0].Matches[0].Groups[1].Value,16) -ge 0x01000000) {throw 'ELF overlaps mailbox'}
$orb4Files=@(Get-ChildItem -LiteralPath $orb4Inputs -File -Recurse)+@(Get-Item -LiteralPath (Join-Path $orb4Job 'orb60_4ppc.bit'),(Join-Path $orb4Elf 'orb60_board.elf'),(Join-Path $orb4Job 'inputs/ps7_init.tcl'),(Join-Path $orb4Job 'inputs/run.tcl'),(Join-Path $orb4Job 'inputs/rerun.tcl'),(Join-Path $orb4Job 'inputs/clock_check.tcl'))
foreach($orb4Label in @('normal','noise','zero')) {$orb4Files+=@(Get-ChildItem -LiteralPath (Join-Path $orb4Job $orb4Label) -File)}
[ordered]@{State='PASS';Part='xc7z015clg485-2';Top='s_wrapper';FrequencyMHz=$orb4Frequency;CpuClockHz=$orb4CpuClocks[0];Toolchain='Vivado HLS/Vivado/SDK 2018.3';PsRuntimeJob=$orb4Runtime;GeneratedPsInit=$orb4FreshInit;PSConfigProperties=$orb4Configs[0].Count;NoJTAG=$true;Files=@($orb4Files|ForEach-Object{[ordered]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}})}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $orb4Job 'pairing.json')
Write-Output "ORB4_ARM_BUILD_PASS $orb4Job/elf/orb60_board.elf"
} finally {
    $env:PATH=$orb4SavedPath
}
