<#
.SYNOPSIS
Programs the volatile FAST design and reads its fixed-DDR benchmark mailbox.

.PARAMETER Iterations
Number of consecutive verified eight-level frames, from 1 through 1000.

.PARAMETER Bitstream
Path to the isolated FAST design bitstream.

.PARAMETER SdkWorkspace
SDK workspace built against the matching FAST hardware definition.

.PARAMETER InputFile
The exact 960x720 binary-PGM validation frame.
#>
[CmdletBinding()]
param(
    [ValidateRange(1, 1000)]
    [int]$Iterations = 10,
    [string]$Bitstream,
    [string]$SdkWorkspace,
    [string]$InputFile
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$script = Join-Path $PSScriptRoot 'run_fast_board_benchmark.tcl'
$previousBitstream = $env:ORB_BITSTREAM
$previousSdkWorkspace = $env:ORB_SDK_WORKSPACE
$previousInputFile = $env:ORB_INPUT_FILE

if (-not $Bitstream) {
    $Bitstream = Join-Path $projectRoot 'vivado_build_fast_stream\orb_fast_fpga.runs\impl_1\orb_fast_system_wrapper.bit'
}
if (-not $SdkWorkspace) {
    $SdkWorkspace = Join-Path $projectRoot 'sdk_build_fast_stream'
}
if (-not $InputFile) {
    $InputFile = Join-Path $projectRoot '..\FPGA_ORB_SHARED_v1.0\vectors\flight_20261004T184929_998838_6bb93c56_f0000\input.pgm'
}

try {
    $env:ORB_BITSTREAM = [System.IO.Path]::GetFullPath($Bitstream)
    $env:ORB_SDK_WORKSPACE = [System.IO.Path]::GetFullPath($SdkWorkspace)
    $env:ORB_INPUT_FILE = [System.IO.Path]::GetFullPath($InputFile)
    foreach ($artifact in @(
        $env:ORB_BITSTREAM,
        (Join-Path $env:ORB_SDK_WORKSPACE 'orb_hw\ps7_init.tcl'),
        (Join-Path $env:ORB_SDK_WORKSPACE 'orb_selftest\Debug\orb_selftest.elf'),
        $env:ORB_INPUT_FILE)) {
        if (-not (Test-Path -LiteralPath $artifact)) {
            throw "Missing FAST benchmark artifact: $artifact"
        }
    }
    & $toolchain xsct $script $Iterations 2>&1 | Tee-Object -Variable benchmarkLines
}
finally {
    if ($null -eq $previousBitstream) { Remove-Item Env:ORB_BITSTREAM -ErrorAction SilentlyContinue }
    else { $env:ORB_BITSTREAM = $previousBitstream }
    if ($null -eq $previousSdkWorkspace) { Remove-Item Env:ORB_SDK_WORKSPACE -ErrorAction SilentlyContinue }
    else { $env:ORB_SDK_WORKSPACE = $previousSdkWorkspace }
    if ($null -eq $previousInputFile) { Remove-Item Env:ORB_INPUT_FILE -ErrorAction SilentlyContinue }
    else { $env:ORB_INPUT_FILE = $previousInputFile }
}
if (-not ($benchmarkLines -match '^status=PASS ')) {
    throw 'FAST board benchmark did not publish a PASS result.'
}
