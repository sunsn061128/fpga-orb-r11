<# .SYNOPSIS Shared file-redirected native host-tool launcher; no retries or JTAG. #>

function Invoke-LoggedHostProcess {
    <#
    .SYNOPSIS
    Run one owned native tool with file-backed stdout/stderr and a bounded timeout.
    .PARAMETER Tool
    Exact installed tool executable; no replacement compiler is selected.
    .PARAMETER Arguments
    Individual Windows arguments without embedded quotes or trailing backslashes.
    .PARAMETER WorkingDirectory
    Existing directory for this invocation, preserving CMake/Ninja semantics.
    .PARAMETER LogRoot
    New invocation logs; existing logs are never overwritten.
    .PARAMETER Name
    Log filename stem unique within LogRoot.
    .PARAMETER TimeoutSeconds
    Whole-process bound; CTest separately retains its 120-second per-test bound.
    .NOTES
    Start-Process redirects native output before launch. Tee-Object may consume
    finished log text, but must not wrap the native compiler/Ninja process itself.
    Only an owned process tree is terminated on timeout; no automatic retry.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Tool,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$WorkingDirectory,
        [Parameter(Mandatory)][string]$LogRoot,
        [Parameter(Mandatory)][ValidatePattern('^[a-z0-9_]+$')][string]$Name,
        [ValidateRange(1,3600)][int]$TimeoutSeconds=120
    )
    New-Item -ItemType Directory -Path $LogRoot -Force | Out-Null
    $stdout=Join-Path $LogRoot "$Name.stdout.log"
    $stderr=Join-Path $LogRoot "$Name.stderr.log"
    $resultPath=Join-Path $LogRoot "$Name.process.json"
    foreach ($path in @($stdout,$stderr,$resultPath)) {
        if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite native-tool evidence: $path" }
    }
    # Quote each argument independently; reject inputs outside this launcher's
    # narrow contract instead of silently changing Windows argument boundaries.
    $quotedArgs=@()
    foreach ($argument in $Arguments) {
        if ($argument.Contains('"') -or $argument.EndsWith('\')) {
            throw "Unsupported quoted/trailing-backslash host argument: $argument"
        }
        $quotedArgs+='"'+$argument+'"'
    }
    $record=[ordered]@{Tool=$Tool;Arguments=$Arguments;WorkingDirectory=$WorkingDirectory;
        StartedUtc=[DateTime]::UtcNow.ToString('o');State='RUNNING';TimeoutSeconds=$TimeoutSeconds}
    $process=$null
    try {
        $process=Start-Process -FilePath $Tool -ArgumentList $quotedArgs -WorkingDirectory $WorkingDirectory -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $null=$process.Handle
        $record['ProcessId']=$process.Id
        [IO.File]::WriteAllText($resultPath,($record | ConvertTo-Json -Depth 4))
        if (-not $process.WaitForExit($TimeoutSeconds*1000)) {
            # Still-running owned handle/PID only; never terminate by process name.
            & C:/Windows/System32/taskkill.exe /PID $process.Id /T /F | Out-Null
            $record.State='TIMEOUT'
            throw "$Name exceeded its $TimeoutSeconds-second tool watchdog."
        }
        $process.WaitForExit()
        $record['ExitCode']=$process.ExitCode
        $record.State=if ($process.ExitCode -eq 0) { 'PASS' } else { 'FAIL' }
        Write-Output "HOST_NATIVE_RESULT name=$Name pid=$($process.Id) exit=$($process.ExitCode) logs=$LogRoot"
        Get-Content -LiteralPath $stdout
        Get-Content -LiteralPath $stderr
        if ($process.ExitCode -ne 0) { throw "$Name failed with exit $($process.ExitCode); inspect $LogRoot." }
    } catch {
        if ($record.State -eq 'RUNNING') { $record.State='FAIL' }
        $record['Error']=$_.Exception.Message
        throw
    } finally {
        $record['CompletedUtc']=[DateTime]::UtcNow.ToString('o')
        [IO.File]::WriteAllText($resultPath,($record | ConvertTo-Json -Depth 4))
        if ($null -ne $process) { $process.Dispose() }
    }
}
