$ErrorActionPreference='Stop'
$orbR16Manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'MANIFEST.json') -Raw|ConvertFrom-Json
foreach($orbR16Item in $orbR16Manifest.Files){
    $orbR16File=Join-Path $PSScriptRoot $orbR16Item.Path
    if((Get-FileHash -LiteralPath $orbR16File -Algorithm SHA256).Hash -ne $orbR16Item.SHA256){throw "r16 hash mismatch: $orbR16File"}
}
Write-Output "R16_BASELINE_HASH_PASS files=$($orbR16Manifest.Files.Count)"
