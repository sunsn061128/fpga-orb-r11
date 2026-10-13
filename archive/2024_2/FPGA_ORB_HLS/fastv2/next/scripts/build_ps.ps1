<#
.SYNOPSIS
Build an isolated schema-5 PS ELF using accepted, unchanged next bit/HDF artifacts.
.DESCRIPTION
Runs the host regression, freezes PS inputs, and builds SDK 2018.3 only.
Never synthesizes, routes, programs hardware, or overwrites a previous job.
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$HardwareJobRoot, [switch]$PreflightOnly, [switch]$Profile,
      [ValidateRange(1,30)][int]$TimeoutMinutes=5)
$ErrorActionPreference='Stop'
$nextRoot=Split-Path -Parent $PSScriptRoot
$repoRoot=Split-Path -Parent (Split-Path -Parent $nextRoot)
$HardwareJobRoot=[IO.Path]::GetFullPath($HardwareJobRoot)
$allowed=[IO.Path]::GetFullPath((Join-Path $nextRoot 'board_build/jobs'))+'\'
if (-not $HardwareJobRoot.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Not an isolated accepted hardware job.' }
$parentState=Get-Content -LiteralPath (Join-Path $HardwareJobRoot 'status.json') -Raw | ConvertFrom-Json
if ($parentState.State -ne 'PASS' -or $parentState.Kind -eq 'PS_ONLY') { throw 'Use an accepted full Vivado/SDK hardware job as parent.' }
& (Join-Path $PSScriptRoot 'run_board.ps1') -JobRoot $HardwareJobRoot -ArtifactsOnly
foreach ($kernel in @('front','resize','gaussian')) {
    $proof=Get-Content -LiteralPath (Join-Path $HardwareJobRoot "inputs/hls_${kernel}_manifest.json") -Raw | ConvertFrom-Json
    $source=if ($kernel -eq 'front') { 'src/fastv2_hls.cpp' } else { "src/${kernel}_hls.cpp" }
    foreach ($relative in @("FPGA_ORB_HLS/fastv2/next/$source", 'FPGA_ORB_HLS/fastv2/next/include/next_stream.hpp',
                            'FPGA_ORB_HLS/fastv2/include/fastv2.hpp', 'FPGA_ORB_HLS/fastv2/include/fastv2_profile_rom.hpp')) {
        $record=@($proof | Where-Object RelativePath -eq $relative)
        $path=Join-Path $repoRoot $relative.Substring('FPGA_ORB_HLS/'.Length)
        if ($record.Count -ne 1 -or (Get-FileHash -LiteralPath $path).Hash -ne $record[0].SHA256) { throw "PL input changed: $relative; rebuild hardware instead." }
    }
}
$oldContract=Get-Content -LiteralPath (Join-Path $HardwareJobRoot 'inputs/board_contract.h') -Raw
$newContract=Get-Content -LiteralPath (Join-Path $nextRoot 'ps/board_contract.h') -Raw
if (($oldContract -split 'NEXT_SCHEMA',2)[0] -cne ($newContract -split 'NEXT_SCHEMA',2)[0]) {
    throw 'MMIO or DDR ABI differs from accepted hardware.'
}
if ((Get-FileHash -LiteralPath (Join-Path $HardwareJobRoot 'inputs/next_backend.h')).Hash -ne
    (Get-FileHash -LiteralPath (Join-Path $nextRoot 'include/next_backend.h')).Hash) { throw 'Point/feature ABI changed.' }
$wrapper='C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
if (-not (Test-Path -LiteralPath $wrapper)) { throw 'Missing isolated Xilinx 2018.3 profile.' }
if ($PreflightOnly) { Write-Output 'NEXT_PS_PREFLIGHT_PASS unchanged_PL=1 unchanged_MMIO_DDR_ABI=1 no_JTAG=1'; return }
$jobName='{0}_ps_{1}' -f (Get-Date -Format 'yyyyMMdd_HHmmss'),([guid]::NewGuid().ToString('N').Substring(0,8))
$jobRoot=Join-Path $nextRoot "board_build/jobs/$jobName"
New-Item -ItemType Directory -Path (Join-Path $jobRoot 'inputs') | Out-Null
$state=[ordered]@{State='RUNNING';Kind='PS_ONLY';Stage='host';ProcessId=$PID;
    StartedUtc=[DateTime]::UtcNow.ToString('o');HardwareJobRoot=$HardwareJobRoot;
    Part='xc7z015clg485-2';Top='next_system_wrapper';FabricHz=100000000;Profile=$Profile.IsPresent}
function Save-PsState {
    <# .SYNOPSIS Publish generated state for this new PS-only job. #>
    [IO.File]::WriteAllText((Join-Path $jobRoot 'status.json'),($state | ConvertTo-Json))
}
Save-PsState
$launch=[ordered]@{JobRoot=$jobRoot;ProcessId=$PID;Kind='PS_ONLY';HardwareJobRoot=$HardwareJobRoot;Profile=$Profile.IsPresent}
[IO.File]::WriteAllText((Join-Path $nextRoot 'board_build/last_ps_job.json'),($launch | ConvertTo-Json))
Write-Output "NEXT_PS_JOB=$jobRoot"
$savedNextBoardRoot=$env:NEXT_BOARD_ROOT
$code=1
try {
    $hostWitness=@('CMakeLists.txt','ps/board_benchmark.c','ps/board_pipeline.inc','ps/board_contract.h',
        'ps/next_backend.c','include/next_backend.h','include/backend_tables.h','ps/full_reference.inc',
        'host/test_board_benchmark.cpp','host/board_mock.hpp','host/test_backend_changing.cpp','host/test_profile_frames.cpp',
        'include/next_backend_profile.h','include/next_frame_stats.h','ps/next_frame_stats.c',
        'ps/board_profile.inc','ps/board_profile_contract.h') | ForEach-Object {
        $path=Join-Path $nextRoot $_
        [ordered]@{Source=$path;SHA256=(Get-FileHash -LiteralPath $path).Hash}
    }
    & (Join-Path $PSScriptRoot 'build_and_test.ps1') -LogRoot (Join-Path $jobRoot 'native_host') 2>&1 | Tee-Object -FilePath (Join-Path $jobRoot 'host.log')
    # Native helper throws on failure; LASTEXITCODE is not set by Start-Process.
    Copy-Item -LiteralPath (Join-Path $nextRoot 'host_build/Testing/Temporary/LastTest.log') -Destination (Join-Path $jobRoot 'host_detailed.log')
    if (-not (Select-String -LiteralPath (Join-Path $jobRoot 'host.log') -SimpleMatch '100% tests passed, 0 tests failed out of 17' -Quiet)) {
        throw 'Missing full 17-test host acceptance before SDK.'
    }
    $profileAcceptance='NEXT_BOARD_PROFILE_HOST_PASS terminal_pairing=1 exact_outputs=1 failure_prefix=1 capacity=1000 overflow_guard=1 restart=1 physical_test=0'
    if ($Profile -and -not (Select-String -LiteralPath (Join-Path $jobRoot 'host_detailed.log') -SimpleMatch $profileAcceptance -Quiet)) {
        throw 'Missing Profile DDR/control acceptance before SDK.'
    }
    foreach ($entry in $hostWitness) {
        if ((Get-FileHash -LiteralPath $entry.Source).Hash -ne $entry.SHA256) { throw "Host input drift: $($entry.Source)" }
    }
    [IO.File]::WriteAllText((Join-Path $jobRoot 'host_source_manifest.json'),(ConvertTo-Json -InputObject @($hostWitness)))
    $state.Stage='freeze'; Save-PsState
    $parentArtifacts=Get-Content -LiteralPath (Join-Path $HardwareJobRoot 'artifact_manifest.json') -Raw | ConvertFrom-Json
    $hardwareProof=@($parentArtifacts | Where-Object { $_.RelativePath -like 'vivado/*' })
    foreach ($relative in @('vivado/next_system_wrapper.hdf','vivado/next_fpga.runs/impl_1/next_system_wrapper.bit',
                            'vivado/.vivado_success','vivado/post_route_timing.rpt','vivado/post_route_check_timing.rpt',
                            'vivado/post_route_utilization.rpt','vivado/post_route_drc.rpt')) {
        $destination=Join-Path $jobRoot $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $HardwareJobRoot $relative) -Destination $destination
    }
    $parentProof=[ordered]@{JobRoot=$HardwareJobRoot;
        ManifestSHA256=(Get-FileHash -LiteralPath (Join-Path $HardwareJobRoot 'artifact_manifest.json')).Hash;
        Artifacts=$hardwareProof;Note='Reused accepted hardware, no new synthesis or route'}
    [IO.File]::WriteAllText((Join-Path $jobRoot 'hardware_parent.json'),($parentProof | ConvertTo-Json -Depth 5))
    $files=[ordered]@{
        'build_board_sdk.tcl'=(Join-Path $nextRoot 'ps/build_board_sdk.tcl')
        'run_board.tcl'=(Join-Path $nextRoot 'ps/run_board.tcl')
        'board_result.tcl'=(Join-Path $nextRoot 'ps/board_result.tcl')
        'board_benchmark.c'=(Join-Path $nextRoot 'ps/board_benchmark.c')
        'board_pipeline.inc'=(Join-Path $nextRoot 'ps/board_pipeline.inc')
        'board_contract.h'=(Join-Path $nextRoot 'ps/board_contract.h')
        'next_backend.c'=(Join-Path $nextRoot 'ps/next_backend.c')
        'next_backend.h'=(Join-Path $nextRoot 'include/next_backend.h')
        'backend_tables.h'=(Join-Path $nextRoot 'include/backend_tables.h')
        'full_reference.inc'=(Join-Path $nextRoot 'ps/full_reference.inc')
        'front_reference.inc'=(Join-Path $HardwareJobRoot 'inputs/front_reference.inc')
        'reference_raw.bin'=(Join-Path $HardwareJobRoot 'inputs/reference_raw.bin')
        'reference_blur.bin'=(Join-Path $HardwareJobRoot 'inputs/reference_blur.bin')
        'input.pgm'=(Join-Path $HardwareJobRoot 'inputs/input.pgm')
    }
    if ($Profile) {
        foreach ($name in @('board_profile_sdk.c','backend_profile_sdk.c','board_profile.inc','board_profile_contract.h')) {
            $files[$name]=Join-Path $nextRoot "ps/$name"
        }
        $files['next_frame_stats.c']=Join-Path $nextRoot 'ps/next_frame_stats.c'
        foreach ($name in @('next_backend_profile.h','next_frame_stats.h')) {
            $files[$name]=Join-Path $nextRoot "include/$name"
        }
    }
    # Reject stale/current CPU tables that do not describe the accepted archived inputs.
    foreach ($name in @('backend_tables.h','full_reference.inc')) {
        if ((Get-FileHash -LiteralPath $files[$name]).Hash -ne
            (Get-FileHash -LiteralPath (Join-Path $HardwareJobRoot "inputs/$name")).Hash) { throw "CPU reference pairing changed: $name" }
    }
    $proof=foreach ($name in $files.Keys) {
        $target=Join-Path $jobRoot "inputs/$name"
        Copy-Item -LiteralPath $files[$name] -Destination $target
        [ordered]@{Name=$name;Source=$files[$name];SHA256=(Get-FileHash -LiteralPath $target).Hash}
    }
    [IO.File]::WriteAllText((Join-Path $jobRoot 'input_manifest.json'),(ConvertTo-Json -InputObject @($proof)))
    foreach ($entry in $hostWitness) {
        $frozen=Join-Path $jobRoot ('inputs/'+[IO.Path]::GetFileName($entry.Source))
        if (Test-Path -LiteralPath $frozen) {
            if ((Get-FileHash -LiteralPath $frozen).Hash -ne $entry.SHA256) { throw "Frozen SDK differs from tested source: $frozen" }
        }
    }
    $state.Stage='sdk'; Save-PsState
    $env:NEXT_BOARD_ROOT=$jobRoot
    $arguments='/d /s /c ""{0}" xsct "{1}""' -f $wrapper,(Join-Path $jobRoot 'inputs/build_board_sdk.tcl')
    $child=Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/cmd.exe') -ArgumentList $arguments -WorkingDirectory $jobRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $jobRoot 'sdk.stdout.log') -RedirectStandardError (Join-Path $jobRoot 'sdk.stderr.log')
    $state['ToolProcessId']=$child.Id; Save-PsState
    $null=$child.Handle
    $deadline=[DateTime]::UtcNow.AddMinutes($TimeoutMinutes)
    while (-not $child.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -ge $deadline) {
            & (Join-Path $env:SystemRoot 'System32/taskkill.exe') /PID $child.Id /T /F | Out-Null
            throw 'PS-only SDK watchdog expired; only owned child terminated.'
        }
    }
    $child.WaitForExit()
    if ($null -eq $child.ExitCode -or $child.ExitCode -ne 0 -or
        -not (Test-Path -LiteralPath (Join-Path $jobRoot 'sdk/.sdk_success'))) { throw 'SDK build failed; inspect SDK logs.' }
    $artifacts=foreach ($relative in @('vivado/next_system_wrapper.hdf','vivado/next_fpga.runs/impl_1/next_system_wrapper.bit',
        'sdk/next_benchmark/Debug/next_benchmark.elf','hardware_parent.json')) {
        [ordered]@{RelativePath=$relative;SHA256=(Get-FileHash -LiteralPath (Join-Path $jobRoot $relative)).Hash}
    }
    [IO.File]::WriteAllText((Join-Path $jobRoot 'artifact_manifest.json'),(ConvertTo-Json -InputObject @($artifacts)))
    $state.State='PASS'; $state.Stage='complete'; Save-PsState
    & (Join-Path $PSScriptRoot 'run_board.ps1') -JobRoot $jobRoot -ArtifactsOnly
    $code=0
    Write-Output "NEXT_PS_BUILD_PASS job=$jobRoot reused_bit=1 profile=$($Profile.IsPresent) no_JTAG=1"
} catch {
    # Preserve detailed failing tests too; do not leave the global CTest log as
    # the only evidence, since the next independent regression will replace it.
    $failedCTestLog=Join-Path $nextRoot 'host_build/Testing/Temporary/LastTest.log'
    if ($state.Stage -eq 'host' -and (Test-Path -LiteralPath $failedCTestLog)) {
        Copy-Item -LiteralPath $failedCTestLog -Destination (Join-Path $jobRoot 'host_detailed.log')
    }
    $state.State='FAIL'; $state['Error']=$_.Exception.Message
    Write-Error $_.Exception.Message -ErrorAction Continue
} finally {
    $env:NEXT_BOARD_ROOT=$savedNextBoardRoot
    $state['CompletedUtc']=[DateTime]::UtcNow.ToString('o'); $state['ExitCode']=$code; Save-PsState
}
exit $code
