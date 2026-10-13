<#
.SYNOPSIS
Validate an isolated conservative cache planner with stale CPU views and independent references.
.DESCRIPTION
New source/hash/log job; same MinGW14.2, each focus tool/test bounded to120s.
Then run the unchanged complete17-test host gate. No SDK, vendor tools, JTAG or retries.
The planner is not enabled in firmware and no host duration is interpreted as ARM time.
#>
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$nextRoot=Split-Path -Parent $PSScriptRoot
$workspaceRoot=[IO.Path]::GetFullPath((Join-Path $nextRoot '../../..'))
$referenceRoot=Join-Path $workspaceRoot 'FPGA_ORB_SHARED_v1.0'
. (Join-Path $PSScriptRoot 'host_process.ps1')
$jobName='{0}_stage3_{1}' -f (Get-Date -Format 'yyyyMMdd_HHmmss'),([guid]::NewGuid().ToString('N').Substring(0,8))
$jobRoot=Join-Path $nextRoot "optimization_build/jobs/$jobName"
$inputs=Join-Path $jobRoot 'inputs'
New-Item -ItemType Directory -Path $inputs | Out-Null
$state=[ordered]@{State='RUNNING';Kind='STAGE3_CACHE_PLAN_HOST';ProcessId=$PID;
    StartedUtc=[DateTime]::UtcNow.ToString('o');Stage='freeze';NoJTAG=$true;FirmwareEnabled=$false;Inputs=@()}
$savedStagePath=$env:PATH
$jobCode=1
$buildRoot=Join-Path $nextRoot 'host_build'
$gcc='C:/Users/dmsun/mingw/mingw64/bin/gcc.exe'
$gxx='C:/Users/dmsun/mingw/mingw64/bin/g++.exe'
$cmake='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$native=Join-Path $jobRoot 'native_focused'

function Save-StageState {
    <# .SYNOPSIS Save only this owned job, preserving all earlier evidence. #>
    [IO.File]::WriteAllText((Join-Path $jobRoot 'status.json'),($state|ConvertTo-Json -Depth 6))
}
function Assert-InputHashes {
    <# .SYNOPSIS Reject mixed frozen/live sources before declaring host acceptance. #>
    foreach($entry in $state.Inputs) {
        if((Get-FileHash -LiteralPath $entry.Source).Hash -ne $entry.SHA256 -or
            (Get-FileHash -LiteralPath $entry.Frozen).Hash -ne $entry.SHA256) {
            throw "Stage3 source/frozen drift: $($entry.Source)"
        }
    }
}
function Frozen-Path([string]$source) {
    <# .SYNOPSIS Resolve the preserved workspace-relative dependency into this job's snapshot. #>
    return Join-Path $inputs ([IO.Path]::GetRelativePath($workspaceRoot,$source))
}

Save-StageState
[IO.File]::WriteAllText((Join-Path $nextRoot 'optimization_build/last_stage3_job.json'),
    (@{JobRoot=$jobRoot;ProcessId=$PID;Kind=$state.Kind}|ConvertTo-Json))
try {
    $paths=@(Join-Path $nextRoot 'CMakeLists.txt')
    # Freeze all authored next dependencies, plus the unchanged baseline model and CPU oracle.
    foreach($folder in @('src','include','host','hls','ps','scripts','vivado')) {
        $paths+=@(rg --files (Join-Path $nextRoot $folder))
        if($LASTEXITCODE -ne 0) {throw "Cannot inventory authored next/$folder"}
    }
    foreach($folder in @('../src','../include','../hls')) {
        $paths+=@(rg --files (Join-Path $nextRoot $folder))
        if($LASTEXITCODE -ne 0) {throw "Cannot inventory baseline dependency $folder"}
    }
    $paths+=@('golden.cpp','golden.hpp','constants.hpp')|ForEach-Object {Join-Path $referenceRoot $_}
    $pgm=Join-Path $referenceRoot 'vectors/flight_20261004T184929_998838_6bb93c56_f0000/input.pgm'
    $paths+=@($pgm)
    foreach($source in @($paths|ForEach-Object {[IO.Path]::GetFullPath($_)}|Sort-Object -Unique)) {
        $frozen=Frozen-Path $source
        New-Item -ItemType Directory -Path (Split-Path -Parent $frozen) -Force|Out-Null
        Copy-Item -LiteralPath $source -Destination $frozen
        $state.Inputs+=@{Source=$source;Frozen=$frozen;SHA256=(Get-FileHash -LiteralPath $source).Hash}
    }
    Assert-InputHashes
    $env:PATH="C:\Users\dmsun\mingw\mingw64\bin;$savedStagePath"
    Invoke-LoggedHostProcess -Tool $gcc -Arguments @('-dumpfullversion') -WorkingDirectory $jobRoot -LogRoot $native -Name version -TimeoutSeconds 120
    $version=(Get-Content (Join-Path $native 'version.stdout.log') -Raw).Trim()
    if($version -ne '14.2.0') {throw "Unexpected MinGW version: $version"}
    $state['CompilerVersion']=$version
    $state['ToolHashes']=@($gcc,$gxx,
        'C:/Users/dmsun/mingw/mingw64/libexec/gcc/x86_64-w64-mingw32/14.2.0/cc1.exe',
        'C:/Users/dmsun/mingw/mingw64/libexec/gcc/x86_64-w64-mingw32/14.2.0/cc1plus.exe',
        'C:/Users/dmsun/mingw/mingw64/x86_64-w64-mingw32/bin/as.exe')|ForEach-Object {
            @{Path=$_;SHA256=(Get-FileHash -LiteralPath $_).Hash}
        }
    $frozenNext=Frozen-Path $nextRoot
    $frozenReference=Frozen-Path $referenceRoot
    # Direct GCC child startup failed even with canonical -B/file redirection.
    # Use the previously accepted CMake/Ninja process chain, unchanged tools/flags.
    # This is a bounded alternative, not proof of a root cause or a numerical PASS.
    $hostToolFlags='-B C:/Users/dmsun/mingw/mingw64/libexec/gcc/x86_64-w64-mingw32/14.2.0/ -B C:/Users/dmsun/mingw/mingw64/x86_64-w64-mingw32/bin/'
    $state['CompileChain']='CMake/Ninja serial frozen project'
    $state['SubprocessFlags']=$hostToolFlags
    $state.ToolHashes+=@($cmake,$ninja)|ForEach-Object {@{Path=$_;SHA256=(Get-FileHash -LiteralPath $_).Hash}}
    $focusedBuild=Join-Path $jobRoot 'focused_build'
    $state.Stage='focused_configure';Save-StageState
    $configureArgs=@('-S',(Join-Path $frozenNext 'scripts/cache_plan_host'),
        '-B',$focusedBuild,'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_C_COMPILER=$gcc",
        "-DCMAKE_CXX_COMPILER=$gxx","-DCMAKE_C_FLAGS=$hostToolFlags","-DCMAKE_CXX_FLAGS=$hostToolFlags",
        '-DCMAKE_BUILD_TYPE=Release',"-DNEXT_FROZEN_ROOT=$frozenNext","-DNEXT_FROZEN_REFERENCE=$frozenReference")
    Invoke-LoggedHostProcess -Tool $cmake -Arguments $configureArgs -WorkingDirectory $jobRoot -LogRoot $native -Name configure -TimeoutSeconds 120
    $state.Stage='focused_compile';Save-StageState
    Invoke-LoggedHostProcess -Tool $cmake -Arguments @('--build',$focusedBuild,'--parallel','1','--verbose') -WorkingDirectory $jobRoot -LogRoot $native -Name compile -TimeoutSeconds 120
    $exe=Join-Path $focusedBuild 'next_cache_plan_test.exe'
    $state['ExecutableSHA256']=(Get-FileHash -LiteralPath $exe).Hash
    Assert-InputHashes
    $state.Stage='focused_test';Save-StageState
    Invoke-LoggedHostProcess -Tool $exe -Arguments @((Frozen-Path $pgm)) -WorkingDirectory $jobRoot -LogRoot $native -Name tests -TimeoutSeconds 120 |
        Tee-Object -FilePath (Join-Path $jobRoot 'focused_test.log')
    $focusText=Get-Content (Join-Path $native 'tests.stdout.log') -Raw
    if($focusText -notmatch 'NEXT_CACHE_PLAN_HOST_PASS transactions=56 features=5624 bins=32 empty=8 sparse=40 fallback=8 rejection_checks=23 archive_layers=8 independent_all_bins=1 stale_views=1 guards=1 physical_test=0' -or
        @([regex]::Matches($focusText,'CACHE_CASE_PASS ')).Count -ne 65 -or
        $focusText -notmatch 'CACHE_TRAFFIC .*performance_claim=0') {throw 'Missing independent cache coverage/byte-exact/traffic markers'}
    if((Get-FileHash -LiteralPath $exe).Hash -ne $state.ExecutableSHA256) {throw 'Test executable changed'}
    $state.Stage='full_host';Save-StageState
    & (Join-Path $PSScriptRoot 'build_and_test.ps1') -BuildJobs 1 -LogRoot (Join-Path $jobRoot 'native_full') 2>&1 |
        Tee-Object -FilePath (Join-Path $jobRoot 'host.log')
    Copy-Item -LiteralPath (Join-Path $buildRoot 'Testing/Temporary/LastTest.log') -Destination (Join-Path $jobRoot 'host_detailed.log')
    Assert-InputHashes
    $hostText=Get-Content (Join-Path $jobRoot 'host.log') -Raw
    $detail=Get-Content (Join-Path $jobRoot 'host_detailed.log') -Raw
    foreach($marker in @('NEXT_BACKEND_CHANGING_PASS transactions=56 features=5624 bins=32 rejections=8 seed=0x523132',
        'NEXT_BACKEND_PROFILE_PASS features=5624 clock_reads=22496 stages=3 wrap=1 disabled_exact=1',
        'NEXT_PROFILE_FRAMES_PASS frames=100 layers=8 variants=2 exact_features=25600 clock_reads=51200 stats_max=1000 physical_test=0',
        'NEXT_BOARD_PROFILE_HOST_PASS terminal_pairing=1 exact_outputs=1 failure_prefix=1 capacity=1000 overflow_guard=1 restart=1 physical_test=0')) {
        if(-not $detail.Contains($marker)) {throw "Missing full host marker: $marker"}
    }
    if(-not $hostText.Contains('100% tests passed, 0 tests failed out of 17')) {throw 'Complete17-test gate missing'}
    $state['DefaultObjectSHA256']=(Get-FileHash (Join-Path $buildRoot 'CMakeFiles/next_ps_backend.dir/ps/next_backend.c.obj')).Hash
    if($state.DefaultObjectSHA256 -ne '26B09B685AA3DB02A82132CF31EBB1AC2755D5353F3ACFA8BD99CFB62BB86EB8') {
        throw 'Disabled-profile default backend object drift; inspect flags/source before acceptance'
    }
    $state['TrafficRecord']=@($focusText -split '\r?\n'|Where-Object {$_ -match '^CACHE_TRAFFIC '})
    $state.State='PASS';$state.Stage='complete';$jobCode=0
    Write-Output "NEXT_STAGE3_CACHE_PLAN_PASS full_host_tests=17 firmware_enabled=0 physical_test=0 job=$jobRoot"
} catch {
    $state.State='FAIL';$state['Error']=$_.Exception.Message
    if($state.Stage -eq 'full_host' -and (Test-Path (Join-Path $buildRoot 'Testing/Temporary/LastTest.log'))) {
        Copy-Item -LiteralPath (Join-Path $buildRoot 'Testing/Temporary/LastTest.log') -Destination (Join-Path $jobRoot 'host_detailed.log') -Force
    }
    Write-Error $_.Exception.Message -ErrorAction Continue
} finally {
    $env:PATH=$savedStagePath
    $state['CompletedUtc']=[DateTime]::UtcNow.ToString('o');$state['ExitCode']=$jobCode
    Save-StageState
}
exit $jobCode
