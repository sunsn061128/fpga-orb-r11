[CmdletBinding()]
param(
    [string]$Output = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$referenceRoot = Join-Path (Split-Path -Parent $projectRoot) 'FPGA_ORB_SHARED_v1.0'
$python = 'C:\Users\dmsun\AppData\Local\Programs\Python\Python311\python.exe'
$replay = Join-Path $referenceRoot 'scripts\replay_vectors.py'
$binary = Join-Path $projectRoot 'host_build_v1\orb_hls_cli.exe'
if ($Output -eq '') { $Output = Join-Path $projectRoot ('build\replay_' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
if (Test-Path -LiteralPath $Output) { throw "Output already exists: $Output" }

& $python $replay --binary $binary --output $Output --all
if ($LASTEXITCODE -ne 0) { throw 'The v1.0 full replay failed' }
Write-Host 'HLS_FULL_REPLAY_PASS configurations=172 inputs=43 stages_per_level=12'
