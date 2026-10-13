<# .SYNOPSIS Build a new 100 MHz two-core test ELF, never rebuild/program PL.
.DESCRIPTION Explicitly reuse only the verified PS runtime/initialization from
2018.3 after comparing the complete PS configuration and both HP override sets.
No XSA or regenerated 2024.2 BSP is claimed. Never use legacy PL drivers. #>
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$JobRoot,
 [string]$PsRuntimeJob='C:\Users\dmsun\Desktop\PLD\FPGA_ORB_HLS\fastv2\next\board_build\jobs\20261012_012547_bf5bab91')
$ErrorActionPreference='Stop'
$orb=Split-Path -Parent $PSScriptRoot
$job=[IO.Path]::GetFullPath($JobRoot)
if(Test-Path -LiteralPath (Join-Path $job 'elf')){throw 'Do not overwrite an ELF build'}
$old=[IO.Path]::GetFullPath($PsRuntimeJob)
$wrapper='C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$compiler='C:\Xilinx\SDK\2018.3\gnu\aarch32\nt\gcc-arm-none-eabi\bin\arm-none-eabi-gcc.exe'
$nm=Join-Path (Split-Path -Parent $compiler) 'arm-none-eabi-nm.exe'
$bsp=Join-Path $old 'sdk/next_bsp/ps7_cortexa9_0'
$rx='CONFIG\.(PCW_\S+)\s+(\{[^}]*\}|\S+)'
$configs=@()
foreach($path in @((Join-Path $orb 'vivado/ps_config.tcl'),(Join-Path $old 'inputs/ps_config.tcl'))){
 $dict=@{};[regex]::Matches((Get-Content -LiteralPath $path -Raw),$rx)|ForEach-Object{$dict[$_.Groups[1].Value]=$_.Groups[2].Value};$configs+=,$dict
}
if($configs[0].Count -ne $configs[1].Count){throw 'PS configuration property count mismatch'}
foreach($k in $configs[0].Keys){if($configs[0][$k] -ne $configs[1][$k]){throw "PS configuration mismatch: $k"}}
foreach($path in @((Join-Path $orb 'vivado/build_orb_system.tcl'),(Join-Path $old 'inputs/build_system.tcl'))){
 $s=Get-Content -LiteralPath $path -Raw
 foreach($hp in 0..3){if($s -notmatch "CONFIG.PCW_USE_S_AXI_HP$hp\s+\{1\}"){throw 'HP override mismatch'}}
 if($s -notmatch 'CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ\s+(\{100\}|\$frequency)'){throw 'Unknown FCLK setup'}
}
$inputs=Join-Path $job 'inputs';$elfRoot=Join-Path $job 'elf';New-Item -ItemType Directory -Path $inputs,$elfRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'board.c'),(Join-Path $PSScriptRoot 'build.ps1'),(Join-Path $PSScriptRoot 'run.tcl'),(Join-Path $PSScriptRoot 'prepare.py') -Destination $inputs
Copy-Item -LiteralPath (Join-Path $orb 'evidence/100mhz/hls/layer2/xfastv2_layer2_hls_hw.h'),(Join-Path $orb 'evidence/100mhz/hls/descriptor/xfastv2_descriptor_hls_hw.h') -Destination $inputs
Copy-Item -LiteralPath (Join-Path $old 'sdk/next_benchmark/src/lscript.ld'),(Join-Path $old 'sdk/next_hw/ps7_init.tcl'),(Join-Path $old 'sdk/next_benchmark/Debug/Xilinx.spec') -Destination $inputs
Copy-Item -LiteralPath (Join-Path $orb 'vivado/ps_config.tcl') -Destination $inputs
Copy-Item -LiteralPath (Join-Path $bsp 'include'),(Join-Path $bsp 'lib') -Destination $inputs -Recurse
Copy-Item -LiteralPath (Join-Path $orb 'artifacts/100mhz/orb60_100mhz.bit') -Destination $job
if((Get-FileHash -LiteralPath (Join-Path $job 'orb60_100mhz.bit')).Hash -ne 'A80FE3FDFF930A3266B2D8701A9DFBC36319010E56744BA0D20245EDA7C1303D'){throw 'Bit pairing mismatch'}
$arguments=@('-std=c11','-O2','-g','-Wall','-Wextra','-mcpu=cortex-a9','-mfpu=vfpv3','-mfloat-abi=hard',
 '-Wl,-build-id=none',('-specs="{0}"' -f (Join-Path $inputs 'Xilinx.spec')),('-I"{0}"' -f $inputs),('-I"{0}"' -f (Join-Path $inputs 'include')),
 ('-L"{0}"' -f (Join-Path $inputs 'lib')),('-Wl,-T,"{0}"' -f (Join-Path $inputs 'lscript.ld')),
 ('-Wl,-Map,"{0}"' -f (Join-Path $elfRoot 'board.map')),('"{0}"' -f (Join-Path $inputs 'board.c')),
 '-Wl,--start-group,-lxil,-lgcc,-lc,--end-group','-o',('"{0}"' -f (Join-Path $elfRoot 'orb60_board.elf')))
& $env:ComSpec /d /s /c ('""{0}" "{1}" {2}"' -f $wrapper,$compiler,($arguments -join ' ')) *> (Join-Path $job 'arm_compile.log')
if($LASTEXITCODE){throw "ARM compiler failed; see $job/arm_compile.log"}
$symbols=& $nm (Join-Path $elfRoot 'orb60_board.elf')
$symbols | Set-Content -LiteralPath (Join-Path $job 'arm_symbols.txt')
$last=@($symbols | Select-String '^([0-9a-fA-F]+)\s+\w\s+_end$')
if($last.Count -ne 1 -or [Convert]::ToUInt32($last[0].Matches[0].Groups[1].Value,16) -ge 0x01000000){throw 'ELF overlaps mailbox'}
$files=@(Get-ChildItem -LiteralPath $inputs -File -Recurse)+@(Get-Item -LiteralPath (Join-Path $job 'orb60_100mhz.bit'),(Join-Path $elfRoot 'orb60_board.elf'))
$manifest=[ordered]@{State='PASS';Part='xc7z015clg485-2';Top='s_wrapper';FrequencyMHz=100;
 Toolchain='PL: 2024.2 existing bit; PS: explicit SDK ARM GCC/standalone 2018.3 compatibility test';
 PsRuntimeJob=$old;PSConfigProperties=$configs[0].Count;NoXsaClaim=$true;NoJTAG=$true;
 Files=@($files|ForEach-Object{[ordered]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}})}
$manifest|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $job 'pairing.json')
Write-Output "ORB60_ARM_BUILD_PASS $job/elf/orb60_board.elf"
