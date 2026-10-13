[CmdletBinding()]
param(
    [ValidateSet('both', 'resize', 'level', 'fast')]
    [string]$Selection = 'both'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
$oldSelection = $env:ORB_HLS_SELECTION
$oldSuccessStamp = $env:ORB_HLS_SUCCESS_STAMP
$successStamp = Join-Path $projectRoot "hls_build\.export_success_$Selection"
Remove-Item -LiteralPath $successStamp -Force -ErrorAction SilentlyContinue
try {
    $projects = if ($Selection -eq 'both') {
        @(@{ Name = 'orb_resize'; Selection = 'resize' },
          @{ Name = 'orb_level'; Selection = 'level' })
    } elseif ($Selection -eq 'resize') {
        @(@{ Name = 'orb_resize'; Selection = 'resize' })
    } elseif ($Selection -eq 'fast') {
        @(@{ Name = 'orb_fast_stream'; Selection = 'fast' })
    } else {
        @(@{ Name = 'orb_level'; Selection = 'level' })
    }

    foreach ($entry in $projects) {
        $project = $entry.Name
        $projectStamp = Join-Path $projectRoot "hls_build\.export_success_$project"
        Remove-Item -LiteralPath $projectStamp -Force -ErrorAction SilentlyContinue
        $env:ORB_HLS_SELECTION = $entry.Selection
        $env:ORB_HLS_SUCCESS_STAMP = $projectStamp

        & $toolchain vivado_hls -f (Join-Path $projectRoot 'hls\export_ip.tcl')
        if (-not (Test-Path -LiteralPath $projectStamp)) {
            $ipDirectory = Join-Path $projectRoot "hls_build\$project\solution1\impl\ip"
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
                if ($LASTEXITCODE -ne 0) { throw "Vivado IP packaging failed for $project" }
            } finally {
                Pop-Location
            }
            if (-not (Test-Path -LiteralPath (Join-Path $ipDirectory 'component.xml'))) {
                throw "Vivado did not produce component.xml for $project"
            }
        }
    }
    Set-Content -LiteralPath $successStamp -Value 'HLS_EXPORT_SUCCESS'
} finally {
    if ($null -eq $oldSelection) { Remove-Item Env:ORB_HLS_SELECTION -ErrorAction SilentlyContinue }
    else { $env:ORB_HLS_SELECTION = $oldSelection }
    if ($null -eq $oldSuccessStamp) { Remove-Item Env:ORB_HLS_SUCCESS_STAMP -ErrorAction SilentlyContinue }
    else { $env:ORB_HLS_SUCCESS_STAMP = $oldSuccessStamp }
}
