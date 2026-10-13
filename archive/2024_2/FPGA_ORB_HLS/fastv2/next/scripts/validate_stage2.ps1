<#
.SYNOPSIS
Run bounded changing-input/profiled CPU regressions and the complete host gate.
.DESCRIPTION
Freeze relevant source hashes and preserve focused/full CTest evidence in a new
job. Each test has a 120-second watchdog. No SDK, vendor build, JTAG or retries.
#>
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$nextRoot=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'host_process.ps1')
$referenceRoot=[IO.Path]::GetFullPath((Join-Path $nextRoot '../../../FPGA_ORB_SHARED_v1.0'))
$jobName='{0}_stage2_{1}' -f (Get-Date -Format 'yyyyMMdd_HHmmss'),([guid]::NewGuid().ToString('N').Substring(0,8))
$jobRoot=Join-Path $nextRoot "optimization_build/jobs/$jobName"
New-Item -ItemType Directory -Path (Join-Path $jobRoot 'inputs') | Out-Null
$state=[ordered]@{State='RUNNING';Kind='STAGE2_BOARD_PROFILE_HOST';ProcessId=$PID;
    StartedUtc=[DateTime]::UtcNow.ToString('o');Stage='freeze';NoJTAG=$true;Inputs=@()}
$savedStagePath=$env:PATH
$jobCode=1
$buildRoot=Join-Path $nextRoot 'host_build'
$cmake='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest=Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
$ninja='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$gcc='C:/Users/dmsun/mingw/mingw64/bin/gcc.exe'
$gxx='C:/Users/dmsun/mingw/mingw64/bin/g++.exe'
# Same compiler/binutils as build_and_test.ps1; only normalize driver lookup.
$hostToolFlags='-B C:/Users/dmsun/mingw/mingw64/libexec/gcc/x86_64-w64-mingw32/14.2.0/ -B C:/Users/dmsun/mingw/mingw64/x86_64-w64-mingw32/bin/'

function Save-StageState {
    <# .SYNOPSIS Persist this owned job's progress; never touch old jobs or board pointers. #>
    [IO.File]::WriteAllText((Join-Path $jobRoot 'status.json'),($state | ConvertTo-Json -Depth 6))
}

function Assert-InputHashes {
    <# .SYNOPSIS Reject source drift during validation instead of certifying mixed inputs. #>
    foreach ($entry in $state.Inputs) {
        if ((Get-FileHash -LiteralPath $entry.Source).Hash -ne $entry.SHA256 -or
            (Get-FileHash -LiteralPath $entry.Frozen).Hash -ne $entry.SHA256) {
            throw "Validation input drift: $($entry.Source)"
        }
    }
}

Save-StageState
[IO.File]::WriteAllText((Join-Path $nextRoot 'optimization_build/last_stage2_job.json'),
    (@{JobRoot=$jobRoot;ProcessId=$PID;Kind=$state.Kind} | ConvertTo-Json))
try {
    $inputPaths=@('CMakeLists.txt','host/test_backend_changing.cpp','host/test_profile_frames.cpp',
        'include/next_frame_stats.h','ps/next_frame_stats.c','ps/next_backend.c',
        'include/next_backend.h','include/next_backend_profile.h','include/backend_tables.h','scripts/build_and_test.ps1',
        'scripts/validate_stage2.ps1','scripts/host_process.ps1',
        'ps/board_benchmark.c','ps/board_pipeline.inc','ps/board_contract.h',
        'ps/board_profile.inc','ps/board_profile_contract.h','ps/board_profile_sdk.c',
        'ps/backend_profile_sdk.c','host/board_mock.hpp','host/test_board_benchmark.cpp') | ForEach-Object { Join-Path $nextRoot $_ }
    $inputPaths+=@('golden.cpp','golden.hpp','constants.hpp') | ForEach-Object { Join-Path $referenceRoot $_ }
    foreach ($source in $inputPaths) {
        $frozen=Join-Path $jobRoot ('inputs/'+[IO.Path]::GetFileName($source))
        Copy-Item -LiteralPath $source -Destination $frozen
        $state.Inputs+=@{Source=$source;Frozen=$frozen;SHA256=(Get-FileHash -LiteralPath $source).Hash}
    }
    $env:PATH="C:\Users\dmsun\mingw\mingw64\bin;$savedStagePath"
    $version=(& $gcc -dumpfullversion | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $version -ne '14.2.0') { throw "Unexpected MinGW version: $version" }
    $state['CompilerVersion']=$version
    $state['SubprocessFlags']=$hostToolFlags
    $state['ToolHashes']=@()
    foreach ($tool in @($gcc,$gxx,
        'C:/Users/dmsun/mingw/mingw64/libexec/gcc/x86_64-w64-mingw32/14.2.0/cc1.exe',
        'C:/Users/dmsun/mingw/mingw64/libexec/gcc/x86_64-w64-mingw32/14.2.0/cc1plus.exe',
        'C:/Users/dmsun/mingw/mingw64/x86_64-w64-mingw32/bin/as.exe')) {
        $state.ToolHashes+=@{Path=$tool;SHA256=(Get-FileHash -LiteralPath $tool).Hash}
    }
    $state.Stage='focused_configure'; Save-StageState
    Invoke-LoggedHostProcess -Tool $cmake -Arguments @('-S',$nextRoot,'-B',$buildRoot,'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_CXX_COMPILER=$gxx","-DCMAKE_C_COMPILER=$gcc","-DCMAKE_C_FLAGS=$hostToolFlags","-DCMAKE_CXX_FLAGS=$hostToolFlags",'-DCMAKE_BUILD_TYPE=Release') -WorkingDirectory $nextRoot -LogRoot (Join-Path $jobRoot 'native_focused') -Name configure -TimeoutSeconds 120 |
        Tee-Object -FilePath (Join-Path $jobRoot 'focused_configure.log')
    $state.Stage='focused_compile'; Save-StageState
    Invoke-LoggedHostProcess -Tool $cmake -Arguments @('--build',$buildRoot,'--target','next_backend_changing_test','next_backend_profile_test','next_profile_frames_test','next_board_profile_test','--parallel','1') -WorkingDirectory $buildRoot -LogRoot (Join-Path $jobRoot 'native_focused') -Name compile -TimeoutSeconds 300 |
        Tee-Object -FilePath (Join-Path $jobRoot 'focused_build.log')
    $state['ExecutableSHA256']=(Get-FileHash -LiteralPath (Join-Path $buildRoot 'next_backend_changing_test.exe')).Hash
    $state['ProfileExecutableSHA256']=(Get-FileHash -LiteralPath (Join-Path $buildRoot 'next_backend_profile_test.exe')).Hash
    $state['FramesExecutableSHA256']=(Get-FileHash -LiteralPath (Join-Path $buildRoot 'next_profile_frames_test.exe')).Hash
    $state['BoardProfileExecutableSHA256']=(Get-FileHash -LiteralPath (Join-Path $buildRoot 'next_board_profile_test.exe')).Hash
    $state['DefaultObjectSHA256']=(Get-FileHash -LiteralPath (Join-Path $buildRoot 'CMakeFiles/next_ps_backend.dir/ps/next_backend.c.obj')).Hash
    # Recorded before adding compile-time-only hooks, using this same MinGW and flags.
    $state['ExpectedDefaultObjectSHA256']='26B09B685AA3DB02A82132CF31EBB1AC2755D5353F3ACFA8BD99CFB62BB86EB8'
    if ($state.DefaultObjectSHA256 -ne $state.ExpectedDefaultObjectSHA256) {
        throw 'Default PS host object changed with profiling disabled; inspect object/flags before acceptance.'
    }
    Assert-InputHashes
    $state.Stage='focused_test'; Save-StageState
    Invoke-LoggedHostProcess -Tool $ctest -Arguments @('--test-dir',$buildRoot,'-R','^(next_backend_(changing|profile)_exact|next_profile_frames_contract|next_board_profile_control)$','--output-on-failure','--timeout','120') -WorkingDirectory $buildRoot -LogRoot (Join-Path $jobRoot 'native_focused') -Name tests -TimeoutSeconds 510 |
        Tee-Object -FilePath (Join-Path $jobRoot 'focused_test.log')
    Copy-Item -LiteralPath (Join-Path $buildRoot 'Testing/Temporary/LastTest.log') -Destination (Join-Path $jobRoot 'focused_detailed.log')
    $state.Stage='full_host'; Save-StageState
    & (Join-Path $PSScriptRoot 'build_and_test.ps1') -BuildJobs 1 -LogRoot (Join-Path $jobRoot 'native_full') 2>&1 |
        Tee-Object -FilePath (Join-Path $jobRoot 'host.log')
    Copy-Item -LiteralPath (Join-Path $buildRoot 'Testing/Temporary/LastTest.log') -Destination (Join-Path $jobRoot 'host_detailed.log')
    Assert-InputHashes
    if (-not (Select-String -LiteralPath (Join-Path $jobRoot 'host.log') -SimpleMatch '100% tests passed, 0 tests failed out of 17' -Quiet) -or
        -not (Select-String -LiteralPath (Join-Path $jobRoot 'host_detailed.log') -SimpleMatch 'NEXT_BACKEND_CHANGING_PASS transactions=56 features=5624 bins=32 rejections=8 seed=0x523132' -Quiet) -or
        -not (Select-String -LiteralPath (Join-Path $jobRoot 'host_detailed.log') -SimpleMatch 'NEXT_BACKEND_PROFILE_PASS features=5624 clock_reads=22496 stages=3 wrap=1 disabled_exact=1' -Quiet) -or
        -not (Select-String -LiteralPath (Join-Path $jobRoot 'host_detailed.log') -SimpleMatch 'NEXT_PROFILE_FRAMES_PASS frames=100 layers=8 variants=2 exact_features=25600 clock_reads=51200 stats_max=1000 physical_test=0' -Quiet) -or
        -not (Select-String -LiteralPath (Join-Path $jobRoot 'host_detailed.log') -SimpleMatch 'NEXT_BOARD_PROFILE_HOST_PASS terminal_pairing=1 exact_outputs=1 failure_prefix=1 capacity=1000 overflow_guard=1 restart=1 physical_test=0' -Quiet)) {
        throw 'Missing complete 17-test/changing-input/profile/frame/board-profile acceptance markers.'
    }
    $csv=@(Get-Content -LiteralPath (Join-Path $jobRoot 'host_detailed.log') |
        Where-Object { $_ -match '^HOST_BACKEND_CSV ' } |
        ForEach-Object { $_ -replace '^HOST_BACKEND_CSV ','' })
    if ($csv.Count -ne 101) { throw "Expected CSV header and 100 actual host frames, got $($csv.Count)" }
    $csvPath=Join-Path $jobRoot 'host_backend_frames.csv'
    [IO.File]::WriteAllLines($csvPath,[string[]]$csv)
    $state['FramesCSV']=$csvPath; $state['FramesCSVSHA256']=(Get-FileHash -LiteralPath $csvPath).Hash
    $state.State='PASS'; $state.Stage='complete'; $jobCode=0
    Write-Output "NEXT_STAGE2_BOARD_PROFILE_PASS host_tests=17 job=$jobRoot physical_test=0"
} catch {
    $state.State='FAIL'; $state['Error']=$_.Exception.Message
    if ($state.Stage -eq 'focused_test' -and (Test-Path -LiteralPath (Join-Path $buildRoot 'Testing/Temporary/LastTest.log'))) {
        Copy-Item -LiteralPath (Join-Path $buildRoot 'Testing/Temporary/LastTest.log') -Destination (Join-Path $jobRoot 'focused_detailed.log')
    }
    if ($state.Stage -eq 'full_host' -and (Test-Path -LiteralPath (Join-Path $buildRoot 'Testing/Temporary/LastTest.log'))) {
        Copy-Item -LiteralPath (Join-Path $buildRoot 'Testing/Temporary/LastTest.log') -Destination (Join-Path $jobRoot 'host_detailed.log')
    }
    Write-Error $_.Exception.Message -ErrorAction Continue
} finally {
    $env:PATH=$savedStagePath
    $state['CompletedUtc']=[DateTime]::UtcNow.ToString('o'); $state['ExitCode']=$jobCode
    Save-StageState
}
exit $jobCode
