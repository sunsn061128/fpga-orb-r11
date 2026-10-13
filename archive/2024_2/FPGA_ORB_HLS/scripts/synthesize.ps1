[CmdletBinding()]
param(
    [ValidateSet('both', 'resize', 'level', 'fast')]
    [string]$Selection = 'both',
    [switch]$Cosim
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$oldSelection = $env:ORB_HLS_SELECTION
$oldCosim = $env:ORB_HLS_COSIM
$oldSuccessStamp = $env:ORB_HLS_SUCCESS_STAMP
$successStamp = Join-Path $projectRoot "hls_build\.success_$Selection"
Remove-Item -LiteralPath $successStamp -Force -ErrorAction SilentlyContinue
try {
    $env:ORB_HLS_SELECTION = $Selection
    $env:ORB_HLS_COSIM = if ($Cosim) { '1' } else { '0' }
    $env:ORB_HLS_SUCCESS_STAMP = $successStamp
    & $toolchain vivado_hls -f (Join-Path $projectRoot 'hls\run_hls.tcl')
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $successStamp)) {
        throw 'Vivado HLS failed before producing its success stamp'
    }
} finally {
    if ($null -eq $oldSelection) { Remove-Item Env:ORB_HLS_SELECTION -ErrorAction SilentlyContinue }
    else { $env:ORB_HLS_SELECTION = $oldSelection }
    if ($null -eq $oldCosim) { Remove-Item Env:ORB_HLS_COSIM -ErrorAction SilentlyContinue }
    else { $env:ORB_HLS_COSIM = $oldCosim }
    if ($null -eq $oldSuccessStamp) { Remove-Item Env:ORB_HLS_SUCCESS_STAMP -ErrorAction SilentlyContinue }
    else { $env:ORB_HLS_SUCCESS_STAMP = $oldSuccessStamp }
}
