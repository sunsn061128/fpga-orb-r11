[CmdletBinding()]
param([ValidateSet('normal','noise','zero')][string]$Frame='normal',
      [ValidateRange(1,1000)][int]$Frames=100)
$ErrorActionPreference='Stop'
$orb4Package=$PSScriptRoot
$orb4Manifest=Get-Content -LiteralPath (Join-Path $orb4Package 'manifest.json') -Raw|ConvertFrom-Json
foreach($orb4Item in $orb4Manifest.Files){
    $orb4File=Join-Path $orb4Package $orb4Item.Path
    if((Get-FileHash -LiteralPath $orb4File -Algorithm SHA256).Hash -ne $orb4Item.SHA256){throw "Package hash mismatch: $orb4File"}
}
$orb4Log=Join-Path $orb4Package ((Get-Date -Format 'yyyyMMdd_HHmmss')+"_${Frame}_Verified_${Frames}.log")
& 'C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd' xsct (Join-Path $orb4Package 'inputs/run.tcl') $orb4Package $Frame $Frames Verified *> $orb4Log
$orb4Text=[IO.File]::ReadAllText($orb4Log)
$orb4Match=[regex]::Match($orb4Text,'ORB60_MAILBOX_WORDS=([^\r\n]+)')
if(-not $orb4Match.Success -or $orb4Text.Contains('ERROR:')){throw "Board run failed; see $orb4Log"}
$orb4Words=@($orb4Match.Groups[1].Value.Trim() -split '\s+'|ForEach-Object{[UInt64]::Parse($_)})
$orb4Golden=[IO.File]::ReadAllBytes((Join-Path $orb4Package "$Frame/golden.bin"))
if($orb4Words.Count -ne 128 -or $orb4Words[2] -ne 0x60c00002 -or $orb4Words[4] -ne 2 -or $orb4Words[5] -ne 0 -or $orb4Words[6] -ne $Frames -or $orb4Words[16] -ne [BitConverter]::ToUInt32($orb4Golden,12)){throw "Board exact check failed; see $orb4Log"}
Write-Output "CPU_EXACT_PASS frame=$Frame frames=$Frames features=$($orb4Words[16]) log=$orb4Log"
