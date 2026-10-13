<#
.SYNOPSIS
    Verify published source hashes and optionally the paired firmware assets.
.DESCRIPTION
    Read-only check against the canonical delivery provenance. This never builds
    software, programs hardware or declares new correctness/performance evidence.
.PARAMETER AssetDir
    Directory containing separately downloaded r11.bit/hdf/elf and ps7_init.tcl.
#>
param([string]$AssetDir)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$records=Get-Content -LiteralPath (Join-Path $root 'docs/PUBLIC_SOURCE_MANIFEST.json') -Raw | ConvertFrom-Json
$sourceCount=0
$assetCount=0
foreach($record in $records) {
    if($record.Kind -eq 'release_asset') {
        if(-not $AssetDir) { continue }
        $base=[IO.Path]::GetFullPath($AssetDir)
        $assetCount++
    } else { $base=$root; $sourceCount++ }
    $path=Join-Path $base $record.Path
    if(-not(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing published file: $path" }
    if((Get-Item -LiteralPath $path).Length -ne $record.Bytes -or
       (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $record.SHA256) {
        throw "Published file hash mismatch: $path"
    }
}
Write-Output "PUBLIC_HASH_PASS sources=$sourceCount assets=$assetCount no_JTAG=1"
Write-Output 'Archive/source verification only; no fresh RTL, SDK or physical-board test.'
