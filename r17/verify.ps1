$ErrorActionPreference='Stop'
Import-Module -Name (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1') -Force -ErrorAction Stop
$orbR17Manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'MANIFEST.json') -Raw|ConvertFrom-Json
foreach($orbR17Item in $orbR17Manifest.Files){
    $orbR17File=Join-Path $PSScriptRoot $orbR17Item.Path
    if((Get-FileHash -LiteralPath $orbR17File -Algorithm SHA256).Hash -ne $orbR17Item.SHA256){throw "r17 hash mismatch: $orbR17File"}
}
Write-Output "R17_BASELINE_HASH_PASS files=$($orbR17Manifest.Files.Count)"
