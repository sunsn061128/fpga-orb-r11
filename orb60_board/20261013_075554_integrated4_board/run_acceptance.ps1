[CmdletBinding()]
param([string]$Accepted100NormalResult,[string]$ResumeStatus)
$ErrorActionPreference='Stop'
$state=[ordered]@{State='RUNNING';StartedLocal=(Get-Date -Format o);ProcessId=$PID;Results=@();Current=''}
if($ResumeStatus){
    $prior=Get-Content -LiteralPath $ResumeStatus -Raw|ConvertFrom-Json
    foreach($result in $prior.Results){
        $manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot "$($result.FrequencyMHz)mhz/manifest.json") -Raw|ConvertFrom-Json
        if($result.State -ne 'SUCCEEDED' -or $result.BitSHA256 -ne $manifest.BitSHA256 -or $result.ELFSHA256 -ne $manifest.ELFSHA256){throw 'Resume has unaccepted or mismatched artifacts'}
        $state.Results+=,$result
    }
}
$statusPath=Join-Path $PSScriptRoot 'board_status.json'
function Save-BoardStatus {$state|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $statusPath}
function Get-LatestBoardResult([int]$frequency,[string]$frame,[string]$mode,[int]$frames){
    $file=Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot "${frequency}mhz") -Filter "*_${frame}_${mode}_${frames}.json"|Sort-Object LastWriteTime -Descending|Select-Object -First 1
    if(!$file){throw 'Missing successful result JSON'}
    $result=Get-Content -LiteralPath $file.FullName -Raw|ConvertFrom-Json
    if($result.State -ne 'SUCCEEDED'){throw 'Unaccepted board result'}
    $state.Results+=,$result;Save-BoardStatus
    return $file.FullName
}
Save-BoardStatus
try{
    foreach($frequency in @(100,125)){
        foreach($frame in @('normal','noise','zero')){
            $verifiedFrames=if($frame -eq 'normal'){100}else{10}
            $alreadyComplete=@($state.Results|Where-Object {$_.FrequencyMHz -eq $frequency -and $_.Frame -eq $frame -and $_.Mode -eq 'Production' -and $_.Frames -eq 1000})
            if($alreadyComplete.Count){continue}
            $state.Current="${frequency}MHz $frame Verified $verifiedFrames";Save-BoardStatus
            $priorVerified=@($state.Results|Where-Object {$_.FrequencyMHz -eq $frequency -and $_.Frame -eq $frame -and $_.Mode -eq 'Verified' -and $_.Frames -ge $verifiedFrames})
            if($priorVerified.Count){
                $verified=$priorVerified[-1].Log -replace '\.log$','.json'
            }elseif($frequency -eq 100 -and $frame -eq 'normal' -and $Accepted100NormalResult){
                $accepted=Get-Content -LiteralPath $Accepted100NormalResult -Raw|ConvertFrom-Json
                if($accepted.State -ne 'SUCCEEDED' -or $accepted.Mode -ne 'Verified' -or $accepted.Frames -lt 100 -or $accepted.FrequencyMHz -ne 100 -or $accepted.Frame -ne 'normal'){throw 'Invalid resume result'}
                $verified=$Accepted100NormalResult;$state.Results+=,$accepted;Save-BoardStatus
            }else{
                & (Join-Path $PSScriptRoot 'run_board.ps1') -FrequencyMHz $frequency -Frame $frame -Frames $verifiedFrames -Mode Verified
                $verified=Get-LatestBoardResult $frequency $frame 'Verified' $verifiedFrames
            }
            $state.Current="${frequency}MHz $frame Production 1000";Save-BoardStatus
            & (Join-Path $PSScriptRoot 'run_board.ps1') -FrequencyMHz $frequency -Frame $frame -Frames 1000 -Mode Production -PrecedingResult $verified
            $null=Get-LatestBoardResult $frequency $frame 'Production' 1000
        }
    }
    $state.State='SUCCEEDED';$state.Current='Complete'
}catch{
    $state.State='FAILED';$state['Error']=$_.Exception.Message;throw
}finally{
    $state['FinishedLocal']=(Get-Date -Format o);Save-BoardStatus
}
Write-Output 'INTEGRATED4_PHYSICAL_BOARD_SUITE_COMPLETED both_frequencies=1 inputs=3 exact_and_production=1'
