<#
.SYNOPSIS
Launch isolated Vivado HLS 2018.3 C simulation, synthesis and optional RTL co-simulation.
.DESCRIPTION
Each invocation has a fresh output directory. Default background mode returns a
PID and log paths. The worker runs selected kernels serially and records its
status; successful tool exit alone is insufficient without the Tcl success stamp.
Selection image runs all-geometry resize and Gaussian regressions serially.
#>
[CmdletBinding()]
param(
    [ValidateSet('all', 'front', 'resize', 'gaussian', 'image')]
    [string]$Selection = 'all',
    [switch]$CsimOnly,
    [switch]$Cosim,
    [ValidateRange(1, 180)]
    [int]$TimeoutMinutes = 45,
    [switch]$Foreground,
    [string]$RunDirectory
)
$ErrorActionPreference = 'Stop'
if ($CsimOnly -and $Cosim) { throw 'CsimOnly and Cosim cannot be combined.' }
$nextRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $nextRoot 'hls_build'
$toolchain = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
if (-not (Test-Path -LiteralPath $toolchain -PathType Leaf)) { throw "Missing toolchain: $toolchain" }

if (-not $Foreground) {
    if ($RunDirectory) { throw 'RunDirectory is reserved for the isolated worker.' }
    $jobName = '{0}_{1}_{2}' -f (Get-Date -Format 'yyyyMMdd_HHmmss'), $Selection, ([guid]::NewGuid().ToString('N').Substring(0, 8))
    $jobRoot = Join-Path $buildRoot "jobs\$jobName"
    New-Item -ItemType Directory -Path $jobRoot -Force | Out-Null
    # Preserve the exact authored inputs independently of later working-tree edits.
    $repoRoot = Split-Path -Parent (Split-Path -Parent $nextRoot)
    $workspaceRoot = Split-Path -Parent $repoRoot
    $inputRoot = Join-Path $jobRoot 'inputs'
    $inputFiles = @(
        'FPGA_ORB_HLS/fastv2/next/src/fastv2_hls.cpp',
        'FPGA_ORB_HLS/fastv2/next/src/resize_hls.cpp',
        'FPGA_ORB_HLS/fastv2/next/src/gaussian_hls.cpp',
        'FPGA_ORB_HLS/fastv2/next/include/next_gaussian.hpp',
        'FPGA_ORB_HLS/fastv2/next/include/next_stream.hpp',
        'FPGA_ORB_HLS/fastv2/next/hls/tb_gaussian.cpp',
        'FPGA_ORB_HLS/fastv2/next/hls/tb_gaussian_stream.cpp',
        'FPGA_ORB_HLS/fastv2/next/hls/tb_resize_stream.cpp',
        'FPGA_ORB_HLS/fastv2/next/hls/image_test_reference.hpp',
        'FPGA_ORB_HLS/fastv2/next/host/test_image_reference.cpp',
        'FPGA_ORB_HLS/fastv2/next/hls/tb_front_stream.cpp',
        'FPGA_ORB_HLS/fastv2/next/hls/front_cases.hpp',
        'FPGA_ORB_HLS/fastv2/next/host_build/generated/front_vectors.inc',
        'FPGA_ORB_HLS/fastv2/next/host/generate_front_vectors.cpp',
        'FPGA_ORB_HLS/fastv2/next/hls/run_hls.tcl',
        'FPGA_ORB_HLS/fastv2/include/fastv2.hpp',
        'FPGA_ORB_HLS/fastv2/include/fastv2_profile_rom.hpp',
        'FPGA_ORB_HLS/fastv2/hls/tb_fastv2.cpp',
        'FPGA_ORB_HLS/fastv2/hls/tb_resize.cpp',
        'FPGA_ORB_SHARED_v1.0/golden.hpp',
        'FPGA_ORB_SHARED_v1.0/golden.cpp',
        'FPGA_ORB_SHARED_v1.0/constants.hpp'
    )
    $proof = foreach ($relative in $inputFiles) {
        $snapshotPath = Join-Path $inputRoot $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $snapshotPath) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $workspaceRoot $relative) -Destination $snapshotPath
        [ordered]@{ RelativePath = $relative; SHA256 = (Get-FileHash -LiteralPath $snapshotPath -Algorithm SHA256).Hash }
    }
    $sourceManifest = Join-Path $jobRoot 'source_manifest.json'
    [System.IO.File]::WriteAllText($sourceManifest, (ConvertTo-Json -InputObject @($proof)))
    $stdout = Join-Path $jobRoot 'stdout.log'
    $stderr = Join-Path $jobRoot 'stderr.log'
    $arguments = '-NoProfile -File "{0}" -Foreground -Selection {1} -RunDirectory "{2}" -TimeoutMinutes {3}' -f $PSCommandPath, $Selection, $jobRoot, $TimeoutMinutes
    if ($CsimOnly) { $arguments += ' -CsimOnly' }
    if ($Cosim) { $arguments += ' -Cosim' }
    $worker = Start-Process -FilePath (Get-Command pwsh -ErrorAction Stop).Source -ArgumentList $arguments -WorkingDirectory $nextRoot -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    $manifest = [ordered]@{
        Selection = $Selection; CsimOnly = [bool]$CsimOnly; ProcessId = $worker.Id
        Cosim = [bool]$Cosim; TimeoutMinutes = $TimeoutMinutes
        StartedUtc = [DateTime]::UtcNow.ToString('o'); JobRoot = $jobRoot
        Stdout = $stdout; Stderr = $stderr; Status = (Join-Path $jobRoot 'status.json')
        SuccessStamp = (Join-Path $jobRoot 'success.stamp')
        Toolchain = $toolchain; Part = 'xc7z015clg485-2'; ClockPeriodNs = 10
        Revision = 'bounded_resize_transfer_r8'; SourceManifest = $sourceManifest
        ToolStdout = (Join-Path $jobRoot 'tool_stdout.log')
        ToolStderr = (Join-Path $jobRoot 'tool_stderr.log')
        SourceRoot = (Join-Path $inputRoot 'FPGA_ORB_HLS/fastv2/next')
    }
    $json = $manifest | ConvertTo-Json
    [System.IO.File]::WriteAllText((Join-Path $jobRoot 'launcher.json'), $json)
    [System.IO.File]::WriteAllText((Join-Path $buildRoot 'last_job.json'), $json)
    Write-Output "NEXT_HLS_STARTED PID=$($worker.Id) selection=$Selection csim_only=$([bool]$CsimOnly) cosim=$([bool]$Cosim)"
    Write-Output "stdout: $stdout"
    Write-Output "stderr: $stderr"
    Write-Output "status: $($manifest.Status)"
    Write-Output "tool stdout: $($manifest.ToolStdout)"
    exit 0
}

if (-not $RunDirectory) { throw 'Foreground worker requires a fresh RunDirectory from the launcher.' }
$jobFullPath = [System.IO.Path]::GetFullPath($RunDirectory)
$allowedPrefix = [System.IO.Path]::GetFullPath((Join-Path $buildRoot 'jobs')) + [System.IO.Path]::DirectorySeparatorChar
if (-not $jobFullPath.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Worker output must be inside next/hls_build/jobs.'
}
if (-not (Test-Path -LiteralPath $jobFullPath -PathType Container)) { throw 'Launcher job directory is missing.' }
$projectOutput = Join-Path $jobFullPath 'projects'
if (Test-Path -LiteralPath $projectOutput) { throw 'Refusing to reuse an existing HLS output directory.' }
$stamp = Join-Path $jobFullPath 'success.stamp'
$statusPath = Join-Path $jobFullPath 'status.json'
$startedUtc = [DateTime]::UtcNow.ToString('o')
$state = [ordered]@{ State = 'RUNNING'; ProcessId = $PID; Selection = $Selection; CsimOnly = [bool]$CsimOnly; Cosim = [bool]$Cosim; StartedUtc = $startedUtc; TimeoutMinutes = $TimeoutMinutes }
[System.IO.File]::WriteAllText($statusPath, ($state | ConvertTo-Json))
$exitCode = 1
try {
    $env:NEXT_HLS_SELECTION = $Selection
    $env:NEXT_HLS_CSIM_ONLY = if ($CsimOnly) { '1' } else { '0' }
    $env:NEXT_HLS_COSIM = if ($Cosim) { '1' } else { '0' }
    $env:NEXT_HLS_BUILD_ROOT = $projectOutput
    $env:NEXT_HLS_SUCCESS_STAMP = $stamp
    $env:NEXT_HLS_SOURCE_ROOT = Join-Path $jobFullPath 'inputs/FPGA_ORB_HLS/fastv2/next'
    $env:NEXT_HLS_AUTHOR_ROOT = $nextRoot
    if (-not (Test-Path -LiteralPath (Join-Path $env:NEXT_HLS_SOURCE_ROOT 'src/resize_hls.cpp'))) {
        throw 'Isolated source snapshot is missing.'
    }
    $toolStdout = Join-Path $jobFullPath 'tool_stdout.log'
    $toolStderr = Join-Path $jobFullPath 'tool_stderr.log'
    $toolArguments = '/d /s /c ""{0}" vivado_hls -f "{1}""' -f $toolchain, (Join-Path $env:NEXT_HLS_SOURCE_ROOT 'hls/run_hls.tcl')
    $toolProcess = Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/cmd.exe') -ArgumentList $toolArguments -WorkingDirectory $jobFullPath -WindowStyle Hidden -RedirectStandardOutput $toolStdout -RedirectStandardError $toolStderr -PassThru
    $state['ToolProcessId'] = $toolProcess.Id
    $state['ToolStdout'] = $toolStdout
    $state['ToolStderr'] = $toolStderr
    [System.IO.File]::WriteAllText($statusPath, ($state | ConvertTo-Json))
    Write-Output "NEXT_TOOL_STARTED PID=$($toolProcess.Id) timeout_minutes=$TimeoutMinutes"
    $deadline = [DateTime]::UtcNow.AddMinutes($TimeoutMinutes)
    $null = $toolProcess.Handle
    while (-not $toolProcess.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -ge $deadline) {
            # This exact process was created above for this job; never kill by name.
            & (Join-Path $env:SystemRoot 'System32/taskkill.exe') /PID $toolProcess.Id /T /F
            throw "HLS/RTL watchdog expired after $TimeoutMinutes minutes; only this job's tool tree was stopped."
        }
    }
    $toolProcess.WaitForExit()
    $toolExitCode = $toolProcess.ExitCode
    if ($null -eq $toolExitCode) { throw 'Unable to read the owned tool process exit code.' }
    $state['ToolExitCode'] = $toolExitCode
    Get-Content -LiteralPath $toolStdout -Tail 40
    if (Test-Path -LiteralPath $toolStderr) { Get-Content -LiteralPath $toolStderr -Tail 15 }
    if ($toolExitCode -ne 0 -or -not (Test-Path -LiteralPath $stamp)) {
        throw "Vivado HLS did not complete: exit=$toolExitCode, success stamp required."
    }
    if ($Cosim -and (Select-String -LiteralPath $toolStdout -Pattern '\[XFORM 203-713\]|\[HLS 200-471\]|\[HLS 214-111\]' -Quiet)) {
        throw 'RTL tool completed, but unresolved DATAFLOW state/channel warnings fail this acceptance stage.'
    }
    $state.State = 'PASS'
    $exitCode = 0
} catch {
    $state.State = 'FAIL'
    $state['Error'] = $_.Exception.Message
    Write-Error -Message $_.Exception.Message -ErrorAction Continue
} finally {
    $state['CompletedUtc'] = [DateTime]::UtcNow.ToString('o')
    $state['ExitCode'] = $exitCode
    [System.IO.File]::WriteAllText($statusPath, ($state | ConvertTo-Json))
}
exit $exitCode
