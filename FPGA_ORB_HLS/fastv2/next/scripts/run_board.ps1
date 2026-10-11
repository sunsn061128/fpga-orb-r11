<#
.SYNOPSIS
Validate isolated next bit/ELF pairing, then optionally run volatile JTAG tests.
.DESCRIPTION
Use ArtifactsOnly without board actions. Physical execution requires user authorization.
#>
[CmdletBinding()]
param([string]$JobRoot, [ValidateRange(1,1000)][int]$Iterations=100,
      [ValidateSet('Legacy','Verified','Production')][string]$Mode,
      [ValidateSet('Serial','Pipeline')][string]$Schedule='Serial', [switch]$ArtifactsOnly)
$ErrorActionPreference='Stop'
$nextRoot=Split-Path -Parent $PSScriptRoot
if (-not $JobRoot) {
    $launch=Get-Content -LiteralPath (Join-Path $nextRoot 'board_build/last_board_job.json') -Raw | ConvertFrom-Json
    $JobRoot=$launch.JobRoot
}
$JobRoot=[IO.Path]::GetFullPath($JobRoot)
$allowed=[IO.Path]::GetFullPath((Join-Path $nextRoot 'board_build/jobs'))+'\'
if (-not $JobRoot.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Not an isolated next board job.' }
$state=Get-Content -LiteralPath (Join-Path $JobRoot 'status.json') -Raw | ConvertFrom-Json
if ($state.State -ne 'PASS') { throw 'Next bit/ELF build is not complete and accepted.' }
$artifacts=Get-Content -LiteralPath (Join-Path $JobRoot 'artifact_manifest.json') -Raw | ConvertFrom-Json
foreach ($required in @('vivado/next_system_wrapper.hdf','vivado/next_fpga.runs/impl_1/next_system_wrapper.bit',
                       'sdk/next_benchmark/Debug/next_benchmark.elf')) {
    if (@($artifacts | Where-Object RelativePath -eq $required).Count -ne 1) { throw "Missing artifact proof: $required" }
}
foreach ($record in $artifacts) {
    if ((Get-FileHash -LiteralPath (Join-Path $JobRoot $record.RelativePath)).Hash -ne $record.SHA256) { throw "Produced board artifact changed: $($record.RelativePath)" }
}
$manifest=Get-Content -LiteralPath (Join-Path $JobRoot 'input_manifest.json') -Raw | ConvertFrom-Json
foreach ($record in $manifest) {
    if ((Get-FileHash -LiteralPath (Join-Path $JobRoot "inputs/$($record.Name)")).Hash -ne $record.SHA256) { throw "Frozen build input changed: $($record.Name)" }
}
foreach ($name in @('board_benchmark.c','board_contract.h','next_backend.c','next_backend.h','backend_tables.h','front_reference.inc','full_reference.inc')) {
    if (@($manifest | Where-Object Name -eq $name).Count -ne 1) { throw "Missing frozen PS proof: $name" }
    $compiledName=if ($state.Profile -and $name -eq 'board_benchmark.c') { 'board_benchmark_source.inc' } else { $name }
    if ((Get-FileHash -LiteralPath (Join-Path $JobRoot "inputs/$name")).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $JobRoot "sdk/next_benchmark/src/$compiledName")).Hash) { throw "Compiled PS source pairing mismatch: $name" }
}
$hasProfileSource=(@($manifest | Where-Object Name -eq 'board_profile_sdk.c').Count -eq 1)
if ([bool]$state.Profile -ne $hasProfileSource) { throw 'Profile job/source-kind mismatch.' }
if ($state.Profile) {
    foreach ($name in @('board_profile_sdk.c','backend_profile_sdk.c','board_profile.inc','board_profile_contract.h',
                       'next_backend_profile.h','next_frame_stats.c','next_frame_stats.h')) {
        if (@($manifest | Where-Object Name -eq $name).Count -ne 1 -or
            (Get-FileHash -LiteralPath (Join-Path $JobRoot "inputs/$name")).Hash -ne
            (Get-FileHash -LiteralPath (Join-Path $JobRoot "sdk/next_benchmark/src/$name")).Hash) {
            throw "Missing/mismatched Profile compiled source: $name"
        }
    }
    if (Test-Path -LiteralPath (Join-Path $JobRoot 'sdk/next_benchmark/src/board_benchmark.c')) {
        throw 'Profile has an unexpected second C main entry.'
    }
    foreach ($relative in @('sdk/.profile_sdk_success','sdk/next_benchmark/Debug/src/board_profile_sdk.o',
        'sdk/next_benchmark/Debug/src/backend_profile_sdk.o','sdk/next_benchmark/Debug/src/next_frame_stats.o')) {
        if (-not (Test-Path -LiteralPath (Join-Path $JobRoot $relative))) { throw "Missing Profile build evidence: $relative" }
    }
    $nm='C:/Xilinx/SDK/2018.3/gnu/aarch32/nt/gcc-arm-none-eabi/bin/arm-none-eabi-nm.exe'
    $symbols=@(& $nm -n --defined-only (Join-Path $JobRoot 'sdk/next_benchmark/Debug/next_benchmark.elf'))
    if ($LASTEXITCODE -ne 0) { throw 'SDK 2018.3 ARM ELF symbol inspection failed.' }
    foreach ($symbol in @('main','next_describe_profiled','next_backend_profile_configure','next_frame_stats','XTime_GetTime')) {
        if (-not ($symbols -match "\s[Tt]\s+$symbol`$")) { throw "Missing linked Profile/timer symbol: $symbol" }
    }
    $programEnd=@($symbols | Select-String -Pattern '^([0-9a-fA-F]+)\s+[A-Za-z]\s+_end$')
    if ($programEnd.Count -ne 1 -or [Convert]::ToUInt64($programEnd[0].Matches[0].Groups[1].Value,16) -ge 0x01000000) {
        throw 'Linked Profile program does not fit below the mailbox workspace.'
    }
    Write-Output 'NEXT_BOARD_PROFILE_ARTIFACT_PASS linked_ARM_timer=1 private_DDR=0x01010000 physical_test=0'
}
$contract=Get-Content -LiteralPath (Join-Path $JobRoot 'inputs/board_contract.h') -Raw
$schemaMatch=[regex]::Match($contract,'NEXT_SCHEMA\s*=\s*([345])\s*,')
if (-not $schemaMatch.Success) { throw 'Unsupported frozen PS schema.' }
$schema=[int]$schemaMatch.Groups[1].Value
if ($schema -ge 4) {
    foreach ($name in @('run_board.tcl','board_result.tcl','input.pgm','reference_raw.bin','reference_blur.bin')) {
        if (@($manifest | Where-Object Name -eq $name).Count -ne 1) { throw "Missing schema-$schema frozen input: $name" }
    }
}
if (-not $Mode) { $Mode=if ($schema -eq 3) { 'Legacy' } else { 'Verified' } }
if ($schema -eq 3 -and $Mode -ne 'Legacy') { throw 'Old schema-3 ELF supports only Legacy.' }
if ($Schedule -eq 'Pipeline' -and ($schema -lt 5 -or $Mode -eq 'Legacy')) { throw 'Pipeline requires schema 5 and Verified/Production.' }
if ($schema -eq 5) {
    $name='board_pipeline.inc'
    if (@($manifest | Where-Object Name -eq $name).Count -ne 1 -or
        (Get-FileHash -LiteralPath (Join-Path $JobRoot "inputs/$name")).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $JobRoot "sdk/next_benchmark/src/$name")).Hash) { throw 'Missing/mismatched compiled pipeline source.' }
}
if ($state.Kind -eq 'PS_ONLY') {
    if (@($artifacts | Where-Object RelativePath -eq 'hardware_parent.json').Count -ne 1) { throw 'Missing parent hardware proof.' }
    $parentProof=Get-Content -LiteralPath (Join-Path $JobRoot 'hardware_parent.json') -Raw | ConvertFrom-Json
    $parentRoot=[IO.Path]::GetFullPath($parentProof.JobRoot)
    if (-not $parentRoot.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid parent job directory.' }
    $parentState=Get-Content -LiteralPath (Join-Path $parentRoot 'status.json') -Raw | ConvertFrom-Json
    if ($parentState.State -ne 'PASS' -or $parentState.Kind -eq 'PS_ONLY') { throw 'Parent hardware acceptance is unavailable.' }
    foreach ($required in @('vivado/next_system_wrapper.hdf','vivado/next_fpga.runs/impl_1/next_system_wrapper.bit')) {
        if (@($parentProof.Artifacts | Where-Object RelativePath -eq $required).Count -ne 1) { throw 'Incomplete reused hardware proof.' }
    }
    if ((Get-FileHash -LiteralPath (Join-Path $parentProof.JobRoot 'artifact_manifest.json')).Hash -ne $parentProof.ManifestSHA256) {
        throw 'Accepted parent hardware manifest changed.'
    }
    foreach ($record in $parentProof.Artifacts) {
        if ((Get-FileHash -LiteralPath (Join-Path $JobRoot $record.RelativePath)).Hash -ne $record.SHA256 -or
            (Get-FileHash -LiteralPath (Join-Path $parentProof.JobRoot $record.RelativePath)).Hash -ne $record.SHA256) {
            throw 'Reused bit/HDF differs from accepted parent.'
        }
    }
}
foreach ($pair in @(
    @('vivado/next_system_wrapper.hdf','sdk/next_hw/system.hdf'),
    @('vivado/next_fpga.runs/impl_1/next_system_wrapper.bit','sdk/next_hw/next_system_wrapper.bit'))) {
    if ((Get-FileHash -LiteralPath (Join-Path $JobRoot $pair[0])).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $JobRoot $pair[1])).Hash) { throw 'Next SDK hardware/bit pairing mismatch.' }
}
foreach ($name in @('sdk/.sdk_success','vivado/.vivado_success','sdk/next_hw/ps7_init.tcl','sdk/next_benchmark/Debug/next_benchmark.elf')) {
    if (-not (Test-Path -LiteralPath (Join-Path $JobRoot $name))) { throw "Missing board artifact: $name" }
}
Write-Output 'NEXT_BOARD_ARTIFACT_PASS (no JTAG actions yet)'
if ($ArtifactsOnly) { return }
$wrapper='C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$runScript=Join-Path $JobRoot 'inputs/run_board.tcl'
if (-not (Test-Path -LiteralPath $runScript)) { $runScript=Join-Path $nextRoot 'ps/run_board.tcl' }
$runArgs=@($JobRoot,$Iterations,$Mode)
if ($schema -eq 5) { $runArgs += $Schedule }
& $wrapper xsct $runScript @runArgs 2>&1 | Tee-Object -Variable resultLines
$expected=if ($Mode -eq 'Production') { '^NEXT_BOARD_STATUS=COMPLETE_UNVERIFIED ' } else { '^NEXT_BOARD_STATUS=PASS ' }
if ($LASTEXITCODE -ne 0 -or -not ($resultLines -match $expected)) { throw 'Next board run did not commit the requested terminal status.' }
