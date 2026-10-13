<#
.SYNOPSIS
Builds the ACZ7015 PS/PL design through bitstream generation.

.PARAMETER BuildDirectory
Generated Vivado project directory; use an isolated path to preserve earlier builds.

.PARAMETER Jobs
Maximum parallel Vivado synthesis and implementation jobs.
#>
[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [ValidateRange(1, 32)]
    [int]$Jobs = 8
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $projectRoot 'vivado_build'
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$successStamp = Join-Path $BuildDirectory '.vivado_success'
$previousBuildDirectory = $env:ORB_VIVADO_BUILD_DIR
$previousJobs = $env:ORB_VIVADO_JOBS

try {
    $env:ORB_VIVADO_BUILD_DIR = $BuildDirectory
    $env:ORB_VIVADO_JOBS = [string]$Jobs
    & $toolchain vivado -mode batch -source (Join-Path $projectRoot 'vivado\build_system.tcl')
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
}
if (-not (Test-Path -LiteralPath $successStamp)) {
    throw 'Vivado exited without producing the verified success stamp.'
}

Write-Host "Vivado PS/PL build completed successfully: $BuildDirectory"
