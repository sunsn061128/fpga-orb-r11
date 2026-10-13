<#
.SYNOPSIS
Builds the isolated fastv2 PS/DDR Vivado design for the ACZ7015.

.PARAMETER BuildDirectory
Generated project directory. The default never overlaps a preserved baseline.

.PARAMETER Jobs
Maximum parallel Vivado synthesis and implementation jobs.

.PARAMETER SetupOnly
Creates and validates the block design without synthesis or implementation.
#>
[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [ValidateRange(1, 32)]
    [int]$Jobs = 8,
    [switch]$SetupOnly
)

$ErrorActionPreference = 'Stop'
$fastv2Root = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent $fastv2Root
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $projectRoot 'vivado_build_fastv2'
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$protectedDirectories = @(
    [IO.Path]::GetFullPath((Join-Path $projectRoot 'vivado_build')),
    [IO.Path]::GetFullPath((Join-Path $projectRoot 'vivado_build_packed64_hp01')),
    [IO.Path]::GetFullPath((Join-Path $projectRoot 'vivado_build_fast_stream'))
)
if ($protectedDirectories -contains $BuildDirectory) {
    throw 'The fastv2 build directory must not overwrite any preserved baseline.'
}

$savedBuildDirectory = $env:FASTV2_VIVADO_BUILD_DIR
$savedJobs = $env:FASTV2_VIVADO_JOBS
$savedSetupOnly = $env:FASTV2_VIVADO_SETUP_ONLY
try {
    $env:FASTV2_VIVADO_BUILD_DIR = $BuildDirectory
    $env:FASTV2_VIVADO_JOBS = [string]$Jobs
    $env:FASTV2_VIVADO_SETUP_ONLY = if ($SetupOnly) { '1' } else { '0' }
    & $toolchain vivado -mode batch -source (Join-Path $fastv2Root 'vivado\build_system.tcl')
    if ($LASTEXITCODE -ne 0) {
        throw "Vivado returned exit code $LASTEXITCODE"
    }
} finally {
    if ($null -eq $savedBuildDirectory) {
        Remove-Item Env:FASTV2_VIVADO_BUILD_DIR -ErrorAction SilentlyContinue
    } else { $env:FASTV2_VIVADO_BUILD_DIR = $savedBuildDirectory }
    if ($null -eq $savedJobs) {
        Remove-Item Env:FASTV2_VIVADO_JOBS -ErrorAction SilentlyContinue
    } else { $env:FASTV2_VIVADO_JOBS = $savedJobs }
    if ($null -eq $savedSetupOnly) {
        Remove-Item Env:FASTV2_VIVADO_SETUP_ONLY -ErrorAction SilentlyContinue
    } else { $env:FASTV2_VIVADO_SETUP_ONLY = $savedSetupOnly }
}

$requiredStamp = Join-Path $BuildDirectory $(
    if ($SetupOnly) { '.vivado_setup_success' } else { '.vivado_success' })
if (-not (Test-Path -LiteralPath $requiredStamp)) {
    throw 'Vivado exited without producing the verified success stamp.'
}
Write-Host "fastv2 Vivado build passed: $BuildDirectory"
