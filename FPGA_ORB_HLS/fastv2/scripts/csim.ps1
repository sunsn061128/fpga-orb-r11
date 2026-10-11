[CmdletBinding()]
param(
    [ValidateSet('front', 'resize')]
    [string]$Selection = 'front'
)

$ErrorActionPreference = 'Stop'
$fastv2Root = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$savedStamp = $env:FASTV2_HLS_SUCCESS_STAMP
$savedSelection = $env:FASTV2_HLS_SELECTION
$stamp = Join-Path $fastv2Root "hls_build\.success_csim_$Selection"
Remove-Item -LiteralPath $stamp -Force -ErrorAction SilentlyContinue
try {
    $env:FASTV2_HLS_SUCCESS_STAMP = $stamp
    $env:FASTV2_HLS_SELECTION = $Selection
    & $toolchain vivado_hls -f (Join-Path $fastv2Root 'hls\run_csim.tcl')
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $stamp)) {
        throw 'fastv2 Vivado HLS C simulation failed'
    }
} finally {
    if ($null -eq $savedStamp) { Remove-Item Env:FASTV2_HLS_SUCCESS_STAMP -ErrorAction SilentlyContinue }
    else { $env:FASTV2_HLS_SUCCESS_STAMP = $savedStamp }
    if ($null -eq $savedSelection) { Remove-Item Env:FASTV2_HLS_SELECTION -ErrorAction SilentlyContinue }
    else { $env:FASTV2_HLS_SELECTION = $savedSelection }
}
