<#
.SYNOPSIS
Builds the bare-metal self-test against a selected implemented hardware design.

.PARAMETER HardwareSpec
Path to the HDF exported by the matching Vivado implementation.

.PARAMETER Workspace
Isolated SDK workspace that will receive the BSP, application, and ELF.

.PARAMETER ApplicationSource
Bare-metal C source copied into the generated SDK application.
#>
[CmdletBinding()]
param(
    [string]$HardwareSpec,
    [string]$Workspace,
    [string]$ApplicationSource
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$defaultSysdef = Join-Path $projectRoot 'vivado_build\orb_fpga.runs\impl_1\orb_system_wrapper.sysdef'
if (-not $HardwareSpec) {
    $HardwareSpec = Join-Path $projectRoot 'vivado_build\orb_system_wrapper.hdf'
    if (-not (Test-Path -LiteralPath $HardwareSpec) -and (Test-Path -LiteralPath $defaultSysdef)) {
        Copy-Item -LiteralPath $defaultSysdef -Destination $HardwareSpec -Force
    }
}
if (-not $Workspace) {
    $Workspace = Join-Path $projectRoot 'sdk_build'
}
if (-not $ApplicationSource) {
    $ApplicationSource = Join-Path $projectRoot 'ps\orb_baremetal_selftest.c'
}
$HardwareSpec = [System.IO.Path]::GetFullPath($HardwareSpec)
$Workspace = [System.IO.Path]::GetFullPath($Workspace)
$ApplicationSource = [System.IO.Path]::GetFullPath($ApplicationSource)
$successStamp = Join-Path $Workspace '.sdk_success'
$previousHardwareSpec = $env:ORB_HARDWARE_SPEC
$previousSdkWorkspace = $env:ORB_SDK_WORKSPACE
$previousApplicationSource = $env:ORB_APP_SOURCE

if (-not (Test-Path -LiteralPath $HardwareSpec)) {
    throw "Missing implemented hardware definition: $HardwareSpec"
}
if (-not (Test-Path -LiteralPath $ApplicationSource)) {
    throw "Missing SDK application source: $ApplicationSource"
}

try {
    $env:ORB_HARDWARE_SPEC = $HardwareSpec
    $env:ORB_SDK_WORKSPACE = $Workspace
    $env:ORB_APP_SOURCE = $ApplicationSource
    & $toolchain xsct (Join-Path $projectRoot 'ps\build_sdk.tcl')
    if ($LASTEXITCODE -ne 0) {
        throw "XSCT returned exit code $LASTEXITCODE"
    }
}
finally {
    if ($null -eq $previousHardwareSpec) {
        Remove-Item Env:ORB_HARDWARE_SPEC -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_HARDWARE_SPEC = $previousHardwareSpec
    }
    if ($null -eq $previousSdkWorkspace) {
        Remove-Item Env:ORB_SDK_WORKSPACE -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_SDK_WORKSPACE = $previousSdkWorkspace
    }
    if ($null -eq $previousApplicationSource) {
        Remove-Item Env:ORB_APP_SOURCE -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_APP_SOURCE = $previousApplicationSource
    }
}
if (-not (Test-Path -LiteralPath $successStamp)) {
    throw 'XSCT exited without producing the verified success stamp.'
}

Write-Host "SDK bare-metal self-test built successfully: $Workspace"
