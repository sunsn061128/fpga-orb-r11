[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$workspace=Split-Path -Parent (Split-Path -Parent $taskRoot)
$package=Join-Path $workspace 'orb60_4ppc_100_125mhz_source_reports_bit'
$runtime=Join-Path $workspace 'FPGA_ORB_HLS/fastv2/next/board_build/jobs/20261012_012547_bf5bab91'
$bsp=Join-Path $runtime 'sdk/next_bsp/ps7_cortexa9_0'
$original=Join-Path $workspace 'orb60_board/20261013_071953_4ppc100mhz_2018_3'
$profile='C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd'
$arm='C:/Xilinx/SDK/2018.3/gnu/aarch32/nt/gcc-arm-none-eabi/bin/arm-none-eabi-gcc.exe'
$native='C:/Users/dmsun/mingw/mingw64/bin/gcc.exe'
$rx='CONFIG\.(PCW_\S+)\s+(\{[^}]*\}|\S+)'
$configs=@()
foreach($path in @((Join-Path $package 'orb60/vivado/ps_config.tcl'),(Join-Path $runtime 'inputs/ps_config.tcl'))){
    $dict=@{};[regex]::Matches([IO.File]::ReadAllText($path),$rx)|ForEach-Object{$dict[$_.Groups[1].Value]=$_.Groups[2].Value};$configs+=,$dict
}
if($configs[0].Count -ne $configs[1].Count){throw 'PS property count differs'}
foreach($key in $configs[0].Keys){if($configs[0][$key] -ne $configs[1][$key]){throw "PS mismatch $key"}}
foreach($frequency in @(100,125)){
    $root=Join-Path $taskRoot "${frequency}mhz"
    if(Test-Path -LiteralPath $root){throw "Refuse overwrite $root"}
    $inputs=Join-Path $root 'inputs';$elf=Join-Path $root 'elf'
    New-Item -ItemType Directory -Path $inputs,$elf|Out-Null
    $source=[IO.File]::ReadAllText((Join-Path $original 'inputs/board.c'))
    $source=$source.Replace('New ORB60 100 MHz PS application.','ORB60 integrated4 100/125 MHz paired PS application.')
    $source=$source.Replace('xfastv2_layer4_hls_hw.h','xfastv2_layer2_hls_hw.h').Replace('XFASTV2_LAYER4_HLS','XFASTV2_LAYER2_HLS').Replace('v[0]!=0x600001','v[0]!=0x600003')
    $source=$source.Replace('count[l]=s[3];','memcpy(mail+80,v,64);count[l]=s[3];')
    $source=$source.Replace('s=(uint32_t*)MEM(dstats(l));','s=(uint32_t*)MEM(dstats(l));memcpy(mail+96,s,64);')
    [IO.File]::WriteAllText((Join-Path $inputs 'board.c'),$source)
    foreach($name in @('test_mock.c','mock.h','prepare.py')){Copy-Item -LiteralPath (Join-Path $workspace "orb60_4ppc/ps_board/$name") -Destination $inputs}
    $mock=[IO.File]::ReadAllText((Join-Path $inputs 'test_mock.c')).Replace('s[0]=0x600001','s[0]=0x600003')
    [IO.File]::WriteAllText((Join-Path $inputs 'test_mock.c'),$mock)
    foreach($name in @('xfastv2_layer2_hls_hw.h','xfastv2_descriptor_hls_hw.h')){
        Copy-Item -LiteralPath (Join-Path $package "orb60/evidence/integrated4_${frequency}mhz/registers/$name") -Destination $inputs
    }
    foreach($name in @('include','lib')){Copy-Item -LiteralPath (Join-Path $bsp $name) -Destination $inputs -Recurse}
    Copy-Item -LiteralPath (Join-Path $runtime 'sdk/next_benchmark/src/lscript.ld'),(Join-Path $runtime 'sdk/next_benchmark/Debug/Xilinx.spec') -Destination $inputs
    foreach($label in @('normal','noise','zero')){Copy-Item -LiteralPath (Join-Path $taskRoot "golden/$label") -Destination $root -Recurse}
    Copy-Item -LiteralPath (Join-Path $package "orb60/artifacts/integrated4_${frequency}mhz/orb60_${frequency}mhz.bit") -Destination (Join-Path $root 'orb60.bit')
    $bitHash=(Get-FileHash -LiteralPath (Join-Path $root 'orb60.bit')).Hash
    $expected=(Get-Content -LiteralPath (Join-Path $package "orb60/artifacts/integrated4_${frequency}mhz/artifact_manifest.json") -Raw|ConvertFrom-Json).bit_sha256
    if($bitHash -ne $expected){throw 'Bit pairing mismatch'}
    $savedPath=$env:PATH
    try{
        $env:PATH=(Split-Path -Parent $native)+';'+$env:SystemRoot+'/System32;'+$env:SystemRoot
        & $native -std=c11 -O2 -Wall -Wextra -Wno-unused-function "-I$inputs" (Join-Path $inputs 'test_mock.c') -o (Join-Path $root 'test_mock.exe') *> (Join-Path $root 'mock_compile.log')
        if($LASTEXITCODE){throw 'PS mock compile failed'}
        foreach($label in @('normal','noise','zero')){
            & (Join-Path $root 'test_mock.exe') (Join-Path $root "$label/golden.bin") *> (Join-Path $root "mock_$label.log")
            if($LASTEXITCODE){throw "Controller mock failed $label"}
        }
    }finally{$env:PATH=$savedPath}
    $args=@('-std=c11','-O2','-g','-Wall','-Wextra','-mcpu=cortex-a9','-mfpu=vfpv3','-mfloat-abi=hard','-Wl,-build-id=none',('-specs="{0}"' -f (Join-Path $inputs 'Xilinx.spec')),('-I"{0}"' -f $inputs),('-I"{0}"' -f (Join-Path $inputs 'include')),('-L"{0}"' -f (Join-Path $inputs 'lib')),('-Wl,-T,"{0}"' -f (Join-Path $inputs 'lscript.ld')),('-Wl,-Map,"{0}"' -f (Join-Path $elf 'board.map')),('"{0}"' -f (Join-Path $inputs 'board.c')),'-Wl,--start-group,-lxil,-lgcc,-lc,--end-group','-o',('"{0}"' -f (Join-Path $elf 'orb60_board.elf')))
    & $env:ComSpec /d /s /c ('""{0}" "{1}" {2}"' -f $profile,$arm,($args -join ' ')) *> (Join-Path $root 'arm_compile.log')
    if($LASTEXITCODE){throw 'ARM build failed'}
    $symbols=& (Join-Path (Split-Path -Parent $arm) 'arm-none-eabi-nm.exe') (Join-Path $elf 'orb60_board.elf')
    $symbols|Set-Content (Join-Path $root 'arm_symbols.txt')
    $end=@($symbols|Select-String '^([0-9a-fA-F]+)\s+\w\s+_end$')
    if($end.Count -ne 1 -or [Convert]::ToUInt32($end[0].Matches[0].Groups[1].Value,16) -ge 0x01000000){throw 'ELF overlaps mailbox'}
    [ordered]@{State='BUILT_NOT_PROGRAMMED';Part='xc7z015clg485-2';Top='s_wrapper';FrequencyMHz=$frequency;BitSHA256=$bitHash;PSConfigProperties=$configs[0].Count;PLTool='Published 2024.2 bit, unchanged';PSTool='SDK ARM GCC/standalone 2018.3 compatibility test';NoXsaClaim=$true;SourcePackage=$package;Runtime=$runtime}|ConvertTo-Json|Set-Content (Join-Path $root 'pairing.json')
    Write-Output "PAIRED_ELF_BUILT frequency=$frequency PS_properties=$($configs[0].Count) bit=$bitHash"
}
