$ErrorActionPreference='Stop'
$orbR15Manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'MANIFEST.json') -Raw|ConvertFrom-Json
foreach($orbR15Item in $orbR15Manifest.Files){
    $orbR15File=Join-Path $PSScriptRoot $orbR15Item.Path
    if((Get-FileHash -LiteralPath $orbR15File -Algorithm SHA256).Hash -ne $orbR15Item.SHA256){throw "r15 hash mismatch: $orbR15File"}
}
Write-Output "R15_BASELINE_HASH_PASS files=$($orbR15Manifest.Files.Count)"
