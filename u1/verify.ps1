$ErrorActionPreference='Stop'
Import-Module -Name (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1') -Force -ErrorAction Stop
$orbU1Manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'MANIFEST.json') -Raw|ConvertFrom-Json
foreach($orbU1Item in $orbU1Manifest.Files){
    $orbU1File=Join-Path $PSScriptRoot $orbU1Item.Path
    if((Get-FileHash -LiteralPath $orbU1File -Algorithm SHA256).Hash -ne $orbU1Item.SHA256){throw "u1 hash mismatch: $orbU1File"}
}
Write-Output "U1_BASELINE_HASH_PASS files=$($orbU1Manifest.Files.Count)"
