<#
.SYNOPSIS
Runs the volatile ACZ7015 PS/PL regression and reads results from the DDR mailbox.

.PARAMETER Iterations
Number of consecutive bit-exact transactions to run, from 1 through 1000.

.PARAMETER Bitstream
Path to the volatile PL bitstream to program.

.PARAMETER SdkWorkspace
SDK workspace containing the matching ps7_init.tcl and self-test ELF.

.PARAMETER InputFile
Optional PGM payload downloaded to the full-frame application's fixed DDR input address.
#>
[CmdletBinding()]
param(
    [ValidateRange(1, 1000)]
    [int]$Iterations = 100,
    [string]$Bitstream,
    [string]$SdkWorkspace,
    [string]$InputFile
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$script = Join-Path $PSScriptRoot 'run_board_benchmark.tcl'
$previousBitstream = $env:ORB_BITSTREAM
$previousSdkWorkspace = $env:ORB_SDK_WORKSPACE
$previousInputFile = $env:ORB_INPUT_FILE

try {
    if ($Bitstream) {
        $env:ORB_BITSTREAM = [System.IO.Path]::GetFullPath($Bitstream)
    }
    if ($SdkWorkspace) {
        $env:ORB_SDK_WORKSPACE = [System.IO.Path]::GetFullPath($SdkWorkspace)
    }
    if ($InputFile) {
        $resolvedInput = [System.IO.Path]::GetFullPath($InputFile)
        if (-not (Test-Path -LiteralPath $resolvedInput)) {
            throw "Missing benchmark input: $resolvedInput"
        }
        $env:ORB_INPUT_FILE = $resolvedInput
    }
    & $toolchain xsct $script $Iterations 2>&1 |
        Tee-Object -Variable benchmarkLines
}
finally {
    if ($null -eq $previousBitstream) {
        Remove-Item Env:ORB_BITSTREAM -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_BITSTREAM = $previousBitstream
    }
    if ($null -eq $previousSdkWorkspace) {
        Remove-Item Env:ORB_SDK_WORKSPACE -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_SDK_WORKSPACE = $previousSdkWorkspace
    }
    if ($null -eq $previousInputFile) {
        Remove-Item Env:ORB_INPUT_FILE -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_INPUT_FILE = $previousInputFile
    }
}
if (-not ($benchmarkLines -match '^status=PASS ')) {
    throw 'Board benchmark did not publish a PASS result.'
}
