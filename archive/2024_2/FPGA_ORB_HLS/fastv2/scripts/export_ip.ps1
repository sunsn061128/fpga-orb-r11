<#
.SYNOPSIS
Exports synthesized fastv2 HLS solutions as Vivado 2018.3 IP components.

.PARAMETER Selection
Selects the front end, resize kernel, or both synthesized solutions.
#>
[CmdletBinding()]
param(
    [ValidateSet('both', 'front', 'resize')]
    [string]$Selection = 'both'
)

$ErrorActionPreference = 'Stop'
$fastv2Root = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$savedSelection = $env:FASTV2_HLS_SELECTION
$savedStamp = $env:FASTV2_HLS_SUCCESS_STAMP
$successStamp = Join-Path $fastv2Root "hls_build\.export_success_$Selection"
Remove-Item -LiteralPath $successStamp -Force -ErrorAction SilentlyContinue

try {
    $projects = if ($Selection -eq 'both') {
        @('front', 'resize')
    } else {
        @($Selection)
    }
    foreach ($entry in $projects) {
        $project = "fastv2_$entry"
        $projectStamp = Join-Path $fastv2Root "hls_build\.export_success_$project"
        Remove-Item -LiteralPath $projectStamp -Force -ErrorAction SilentlyContinue
        $env:FASTV2_HLS_SELECTION = $entry
        $env:FASTV2_HLS_SUCCESS_STAMP = $projectStamp
        & $toolchain vivado_hls -f (Join-Path $fastv2Root 'hls\export_ip.tcl')
        if (-not (Test-Path -LiteralPath $projectStamp)) {
            $ipDirectory = Join-Path $fastv2Root "hls_build\$project\solution1\impl\ip"
            $packScript = Join-Path $ipDirectory 'run_ippack.tcl'
            if (-not (Test-Path -LiteralPath $packScript)) {
                throw "Vivado HLS export failed without generating $packScript"
            }
            $scriptText = [IO.File]::ReadAllText($packScript)
            $patchedText = [regex]::Replace(
                $scriptText, '(?m)^set Revision\s+"[0-9]+"\s*$', 'set Revision    "1"')
            [IO.File]::WriteAllText($packScript, $patchedText)
            Push-Location $ipDirectory
            try {
                & $toolchain vivado -mode batch -source $packScript
                if ($LASTEXITCODE -ne 0) {
                    throw "Vivado IP packaging failed for $project"
                }
            } finally {
                Pop-Location
            }
            if (-not (Test-Path -LiteralPath (Join-Path $ipDirectory 'component.xml'))) {
                throw "Vivado did not produce component.xml for $project"
            }
        }
    }
    Set-Content -LiteralPath $successStamp -Value 'FASTV2_EXPORT_SUCCESS'
} finally {
    if ($null -eq $savedSelection) {
        Remove-Item Env:FASTV2_HLS_SELECTION -ErrorAction SilentlyContinue
    } else {
        $env:FASTV2_HLS_SELECTION = $savedSelection
    }
    if ($null -eq $savedStamp) {
        Remove-Item Env:FASTV2_HLS_SUCCESS_STAMP -ErrorAction SilentlyContinue
    } else {
        $env:FASTV2_HLS_SUCCESS_STAMP = $savedStamp
    }
}
