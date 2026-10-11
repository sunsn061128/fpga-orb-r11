<#
.SYNOPSIS
Builds the isolated fastv2 Cortex-A9 benchmark with Xilinx SDK 2018.3.
.DESCRIPTION
Recreates only sdk_build_fastv2. Original SDK/Vivado baseline folders are untouched.
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$workspace = Join-Path $projectRoot 'sdk_build_fastv2'
$buildScript = Join-Path $PSScriptRoot '..\ps\build_sdk.tcl'
& $toolchain xsct $buildScript
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $workspace '.sdk_success'))) {
    throw 'fastv2 SDK build failed.'
}
