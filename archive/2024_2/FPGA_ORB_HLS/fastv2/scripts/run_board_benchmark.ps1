<#
.SYNOPSIS
Programs fastv2 in volatile PL SRAM and runs the fixed-input PS benchmark.
.PARAMETER Iterations
Consecutive verified eight-level frames, from 1 through 1000; default 100.
.PARAMETER ArtifactsOnly
Checks matching hardware/software files without connecting to the board.
#>
[CmdletBinding()]
param([ValidateRange(1,1000)][int]$Iterations = 100, [switch]$ArtifactsOnly)
$ErrorActionPreference = 'Stop'
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$workspace = Join-Path $projectRoot 'sdk_build_fastv2'
$hardware = Join-Path $projectRoot 'vivado_build_fastv2\fastv2_system_wrapper.hdf'
$bitstream = Join-Path $projectRoot 'vivado_build_fastv2\fastv2_fpga.runs\impl_1\fastv2_system_wrapper.bit'
foreach ($pair in @(
    @($hardware, (Join-Path $workspace 'fastv2_hw\system.hdf')),
    @($bitstream, (Join-Path $workspace 'fastv2_hw\fastv2_system_wrapper.bit')),
    @((Join-Path $projectRoot 'fastv2\ps\fastv2_benchmark.c'), (Join-Path $workspace 'fastv2_benchmark\src\fastv2_benchmark.c')),
    @((Join-Path $projectRoot 'fastv2\ps\fastv2_reference.inc'), (Join-Path $workspace 'fastv2_benchmark\src\fastv2_reference.inc')))) {
    foreach ($path in $pair) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing artifact: $path" }
    }
    if ((Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash) {
        throw "fastv2 SDK/hardware/source mismatch. Rebuild SDK before programming: $($pair[0])"
    }
}
foreach ($path in @(
    (Join-Path $workspace '.sdk_success'),
    (Join-Path $workspace 'fastv2_hw\ps7_init.tcl'),
    (Join-Path $workspace 'fastv2_benchmark\Debug\fastv2_benchmark.elf'),
    (Join-Path $projectRoot 'fastv2\ps\reference_pyramid.bin'),
    (Join-Path $projectRoot '..\FPGA_ORB_SHARED_v1.0\vectors\flight_20261004T184929_998838_6bb93c56_f0000\input.pgm'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing artifact: $path" }
}
Write-Output 'FASTV2_ARTIFACT_CHECK_PASS (no JTAG actions yet)'
if ($ArtifactsOnly) { return }
$script = Join-Path $PSScriptRoot '..\ps\run_board_benchmark.tcl'
& $toolchain xsct $script $Iterations 2>&1 | Tee-Object -Variable benchmarkLines
if ($LASTEXITCODE -ne 0 -or -not ($benchmarkLines -match '^status=PASS ')) {
    throw 'fastv2 physical-board benchmark did not publish PASS.'
}
