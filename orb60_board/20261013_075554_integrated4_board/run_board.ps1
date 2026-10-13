[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidateSet(100,125)][int]$FrequencyMHz,
      [ValidateSet('normal','noise','zero')][string]$Frame='normal',
      [ValidateRange(1,1000)][int]$Frames=100,
      [ValidateSet('Verified','Production')][string]$Mode='Verified',
      [string]$PrecedingResult)
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "${FrequencyMHz}mhz"
$manifest=Get-Content -LiteralPath (Join-Path $root 'manifest.json') -Raw|ConvertFrom-Json
foreach($item in $manifest.Files){
    if((Get-FileHash -LiteralPath (Join-Path $root $item.Path)).Hash -ne $item.SHA256){throw "Paired file hash mismatch: $($item.Path)"}
}
$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
$log=Join-Path $root "${stamp}_${Frame}_${Mode}_${Frames}.log"
$profile='C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd'
if($PrecedingResult){
    $preceding=Get-Content -LiteralPath $PrecedingResult -Raw|ConvertFrom-Json
    if($preceding.State -ne 'SUCCEEDED' -or $preceding.Frame -ne $Frame -or $preceding.FrequencyMHz -ne $FrequencyMHz -or $preceding.BitSHA256 -ne $manifest.BitSHA256 -or $preceding.ELFSHA256 -ne $manifest.ELFSHA256){throw 'Previous hardware result is not this exact bit/ELF/input'}
    $mailboxFile=$log -replace '\.log$','.preceding_mailbox.txt'
    $preceding.Mailbox -join ' '|Set-Content -LiteralPath $mailboxFile
    & $profile xsct (Join-Path $root 'inputs/rerun.tcl') $root $Frames $Mode $mailboxFile $preceding.Features $FrequencyMHz *> $log
}else{
    & $profile xsct (Join-Path $root 'inputs/run.tcl') $root $Frame $Frames $Mode $FrequencyMHz *> $log
}
$exitCode=$LASTEXITCODE
$body=[IO.File]::ReadAllText($log)
$match=[regex]::Match($body,'ORB60_MAILBOX_WORDS=([^\r\n]+)')
if($exitCode -ne 0 -or !$match.Success -or $body -match 'ORB60_BOARD_ERROR|ORB60_RERUN_ERROR|ERROR:'){throw "Physical run failed: $log"}
$words=@($match.Groups[1].Value.Trim() -split '\s+'|ForEach-Object{[uint64]::Parse($_)})
$gold=[IO.File]::ReadAllBytes((Join-Path $root "$Frame/golden.bin"))
$status=if($Mode -eq 'Verified'){2}else{4}
if($words.Count -ne 128 -or $words[2] -ne 0x60c00002 -or $words[4] -ne $status -or $words[5] -ne 0 -or $words[6] -ne $Frames -or $words[16] -ne [BitConverter]::ToUInt32($gold,12) -or $words[78] -ne 0x44445231 -or $words[80] -ne 0x600003 -or $words[96] -ne 0x600002){throw "Physical acceptance failed: $log"}
$result=[ordered]@{State='SUCCEEDED';Scope=if($Mode -eq 'Verified'){'CPU_EXACT_PASS'}else{'COMPLETE_UNVERIFIED'};FrequencyMHz=$FrequencyMHz;Frame=$Frame;Mode=$Mode;Frames=$Frames;Features=$words[16];ProcessingAvgUs=$words[10];ProcessingMinUs=$words[13];ProcessingMaxUs=$words[14];WallTotalUs=$words[11];WallUsPerFrame=$words[11]/$Frames;WallFPS=1000000*$Frames/$words[11];VerificationUs=$words[12];CacheUs=$words[18];ConfigUs=$words[19];Log=$log;Mailbox=$words;BitSHA256=$manifest.BitSHA256;ELFSHA256=$manifest.ELFSHA256;FinishedLocal=(Get-Date -Format o)}
$result|ConvertTo-Json -Depth 5|Set-Content -LiteralPath ($log -replace '\.log$','.json')
Write-Output "$($result.Scope) frequency=$FrequencyMHz frame=$Frame frames=$Frames features=$($result.Features) avg_us=$($result.ProcessingAvgUs) max_us=$($result.ProcessingMaxUs) wall_fps=$([math]::Round($result.WallFPS,2)) log=$log"
