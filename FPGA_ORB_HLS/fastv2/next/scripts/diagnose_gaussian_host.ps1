<#
.SYNOPSIS
Run four bounded MinGW Gaussian startup/transaction checks without changing PL.
.DESCRIPTION
Preserve progress logs and attempt an owned-process GDB stack capture on timeout.
No automatic retry, SDK build, JTAG or acceptance-gate bypass is performed.
#>
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$nextRoot=Split-Path -Parent $PSScriptRoot
$diagName='{0}_gaussian_{1}' -f (Get-Date -Format 'yyyyMMdd_HHmmss'),([guid]::NewGuid().ToString('N').Substring(0,8))
$diagRoot=Join-Path $nextRoot "optimization_build/$diagName"
New-Item -ItemType Directory -Path $diagRoot | Out-Null
$savedDiagPath=$env:PATH
$diagState=[ordered]@{State='RUNNING';ProcessId=$PID;StartedUtc=[DateTime]::UtcNow.ToString('o');
    Kind='GAUSSIAN_HOST_DIAGNOSTIC';Runs=@();NoJTAG=$true}

function Save-DiagnosticState {
    <# .SYNOPSIS Persist only this diagnostic's generated state and launch pointer. #>
    [IO.File]::WriteAllText((Join-Path $diagRoot 'status.json'),($diagState | ConvertTo-Json -Depth 6))
}

Save-DiagnosticState
[IO.File]::WriteAllText((Join-Path $nextRoot 'optimization_build/last_diag.json'),
    (@{JobRoot=$diagRoot;ProcessId=$PID;Kind=$diagState.Kind} | ConvertTo-Json))
$diagCode=1
try {
    $env:PATH="C:\Users\dmsun\mingw\mingw64\bin;$savedDiagPath"
    $cmake='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    & $cmake --build (Join-Path $nextRoot 'host_build') --target next_gaussian_stream_test --parallel 1 2>&1 |
        Tee-Object -FilePath (Join-Path $diagRoot 'build.log')
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostic target compilation failed.' }
    $exe=Join-Path $nextRoot 'host_build/next_gaussian_stream_test.exe'
    $diagState['ExecutableSHA256']=(Get-FileHash -LiteralPath $exe).Hash
    $diagState['BenchSHA256']=(Get-FileHash -LiteralPath (Join-Path $nextRoot 'hls/tb_gaussian_stream.cpp')).Hash
    $diagState['KernelSHA256']=(Get-FileHash -LiteralPath (Join-Path $nextRoot 'src/gaussian_hls.cpp')).Hash
    $gdb='C:/Users/dmsun/mingw/mingw64/bin/gdb.exe'
    Save-DiagnosticState
    for ($index=0;$index -lt 4;++$index) {
        $out=Join-Path $diagRoot "run_$index.stdout.log"
        $err=Join-Path $diagRoot "run_$index.stderr.log"
        $test=Start-Process -FilePath $exe -WorkingDirectory (Join-Path $nextRoot 'host_build') -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
        $null=$test.Handle
        if (-not $test.WaitForExit(10000)) {
            $diagState.Runs+=@{Index=$index;ProcessId=$test.Id;State='TIMEOUT';TimeoutSeconds=10}
            Save-DiagnosticState
            try {
                $gdbArgs='-q -batch -p {0} -ex "set pagination off" -ex "thread apply all bt" -ex "detach" -ex "quit"' -f $test.Id
                $debugger=Start-Process -FilePath $gdb -ArgumentList $gdbArgs -WorkingDirectory $diagRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $diagRoot "run_$index.gdb.stdout.log") -RedirectStandardError (Join-Path $diagRoot "run_$index.gdb.stderr.log")
                $null=$debugger.Handle
                if (-not $debugger.WaitForExit(10000)) { Stop-Process -Id $debugger.Id -Force -ErrorAction SilentlyContinue }
            } finally {
                # Owned PID only: no process-name/global termination.
                if (-not $test.HasExited) { Stop-Process -Id $test.Id -Force -ErrorAction SilentlyContinue }
            }
            throw "Gaussian diagnostic timed out at run $index; inspect progress and GDB logs."
        }
        $test.WaitForExit()
        if ($test.ExitCode -ne 0 -or -not (Select-String -LiteralPath $out -SimpleMatch 'NEXT_GAUSSIAN_STREAM_PASS cases=14' -Quiet)) {
            throw "Gaussian diagnostic failed at run $index (exit $($test.ExitCode))."
        }
        $diagState.Runs+=@{Index=$index;ProcessId=$test.Id;State='PASS';ExitCode=$test.ExitCode}
        Save-DiagnosticState
    }
    $diagState.State='PASS'; $diagCode=0
    Write-Output "NEXT_GAUSSIAN_DIAGNOSTIC_PASS runs=4 cases_per_run=14 job=$diagRoot root_cause_not_proven=1"
} catch {
    $diagState.State='FAIL'; $diagState['Error']=$_.Exception.Message
    Write-Error $_.Exception.Message -ErrorAction Continue
} finally {
    $env:PATH=$savedDiagPath
    $diagState['CompletedUtc']=[DateTime]::UtcNow.ToString('o')
    $diagState['ExitCode']=$diagCode
    Save-DiagnosticState
}
exit $diagCode
