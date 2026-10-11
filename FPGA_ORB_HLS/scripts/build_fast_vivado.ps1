<#
.SYNOPSIS
Builds the isolated ACZ7015 resize plus streaming-FAST PS/PL design.

.PARAMETER BuildDirectory
Generated Vivado project directory; it must be separate from the v1.0 baseline.

.PARAMETER Jobs
Maximum parallel Vivado synthesis and implementation jobs.

.PARAMETER SetupOnly
Validate and generate the block design without running synthesis or implementation.
#>
[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [ValidateRange(1, 32)]
    [int]$Jobs = 8,
    [switch]$SetupOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $projectRoot 'vivado_build_fast_stream'
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$baselineDirectory = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'vivado_build_packed64_hp01'))
if ($BuildDirectory -eq $baselineDirectory) {
    throw 'The FAST build directory must not overwrite the preserved v1.0 baseline.'
}
$successStamp = Join-Path $BuildDirectory '.vivado_success'
$setupStamp = Join-Path $BuildDirectory '.vivado_setup_success'
$previousBuildDirectory = $env:ORB_VIVADO_BUILD_DIR
$previousJobs = $env:ORB_VIVADO_JOBS
$previousSetupOnly = $env:ORB_VIVADO_SETUP_ONLY

try {
    $env:ORB_VIVADO_BUILD_DIR = $BuildDirectory
    $env:ORB_VIVADO_JOBS = [string]$Jobs
    $env:ORB_VIVADO_SETUP_ONLY = if ($SetupOnly) { '1' } else { '0' }
    & $toolchain vivado -mode batch -source (Join-Path $projectRoot 'vivado\build_fast_system.tcl')
    if ($LASTEXITCODE -ne 0) {
        throw "Vivado returned exit code $LASTEXITCODE"
    }
}
finally {
    if ($null -eq $previousBuildDirectory) {
        Remove-Item Env:ORB_VIVADO_BUILD_DIR -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_VIVADO_BUILD_DIR = $previousBuildDirectory
    }
    if ($null -eq $previousJobs) {
        Remove-Item Env:ORB_VIVADO_JOBS -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_VIVADO_JOBS = $previousJobs
    }
    if ($null -eq $previousSetupOnly) {
        Remove-Item Env:ORB_VIVADO_SETUP_ONLY -ErrorAction SilentlyContinue
    }
    else {
        $env:ORB_VIVADO_SETUP_ONLY = $previousSetupOnly
    }
}
$requiredStamp = if ($SetupOnly) { $setupStamp } else { $successStamp }
if (-not (Test-Path -LiteralPath $requiredStamp)) {
    throw 'Vivado exited without producing the verified success stamp.'
}

if ($SetupOnly) {
    Write-Host "Vivado FAST block design validated successfully: $BuildDirectory"
}
else {
    Write-Host "Vivado FAST PS/PL build completed successfully: $BuildDirectory"
}
