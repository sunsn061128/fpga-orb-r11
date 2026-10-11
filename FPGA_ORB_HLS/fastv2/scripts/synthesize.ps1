[CmdletBinding()]
param(
    [ValidateSet('front', 'resize')]
    [string]$Selection = 'front',
    [switch]$Cosim
)

$ErrorActionPreference = 'Stop'
$fastv2Root = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$savedCosim = $env:FASTV2_HLS_COSIM
$savedStamp = $env:FASTV2_HLS_SUCCESS_STAMP
$savedSelection = $env:FASTV2_HLS_SELECTION
$stamp = Join-Path $fastv2Root "hls_build\.success_fastv2_$Selection"
Remove-Item -LiteralPath $stamp -Force -ErrorAction SilentlyContinue
try {
    $env:FASTV2_HLS_COSIM = if ($Cosim) { '1' } else { '0' }
    $env:FASTV2_HLS_SELECTION = $Selection
    $env:FASTV2_HLS_SUCCESS_STAMP = $stamp
    & $toolchain vivado_hls -f (Join-Path $fastv2Root 'hls\run_hls.tcl')
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $stamp)) {
        throw 'fastv2 Vivado HLS failed before producing its success stamp'
    }
} finally {
    if ($null -eq $savedCosim) { Remove-Item Env:FASTV2_HLS_COSIM -ErrorAction SilentlyContinue }
    else { $env:FASTV2_HLS_COSIM = $savedCosim }
    if ($null -eq $savedStamp) { Remove-Item Env:FASTV2_HLS_SUCCESS_STAMP -ErrorAction SilentlyContinue }
    else { $env:FASTV2_HLS_SUCCESS_STAMP = $savedStamp }
    if ($null -eq $savedSelection) { Remove-Item Env:FASTV2_HLS_SELECTION -ErrorAction SilentlyContinue }
    else { $env:FASTV2_HLS_SELECTION = $savedSelection }
}
