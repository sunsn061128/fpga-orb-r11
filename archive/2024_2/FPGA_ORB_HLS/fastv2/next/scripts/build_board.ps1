<#
.SYNOPSIS
Prepare isolated three-core IP, routed hardware and PS backend ELF without JTAG.
.DESCRIPTION
Copies completed HLS projects before export, preserving active RTL regressions.
The default returns a hidden worker PID immediately; generated jobs are never reused.
#>
[CmdletBinding()]
param(
    [string]$FrontJobRoot,
    [string]$ImageJobRoot,
    [ValidateRange(1,16)][int]$Jobs = 8,
    [ValidateRange(5,720)][int]$TimeoutMinutes = 360,
    [switch]$SetupOnly,
    [switch]$PreflightOnly,
    [switch]$Foreground,
    [string]$RunDirectory
)
$ErrorActionPreference = 'Stop'
$nextRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent (Split-Path -Parent $nextRoot)
$buildRoot = Join-Path $nextRoot 'board_build'
$wrapper = 'C:\Users\dmsun\Documents\Codex\toolchains\fpga\bin\xilinx-2018.3.cmd'
if (-not (Test-Path -LiteralPath $wrapper)) { throw 'Missing Xilinx 2018.3 isolated profile.' }
if (-not $Foreground) {
    if ($RunDirectory) { throw 'RunDirectory is owned by the background worker.' }
    if (-not $FrontJobRoot) { $FrontJobRoot = Join-Path $nextRoot 'hls_build/jobs/20261009_223114_front_ee8ba228' }
    if (-not $ImageJobRoot) { $ImageJobRoot = Join-Path $nextRoot 'hls_build/jobs/20261009_212533_all_7b96c1af' }
    $sources = @(
        @{ Kernel='front'; Root=$FrontJobRoot; Source='src/fastv2_hls.cpp' },
        @{ Kernel='resize'; Root=$ImageJobRoot; Source='src/resize_hls.cpp' },
        @{ Kernel='gaussian'; Root=$ImageJobRoot; Source='src/gaussian_hls.cpp' }
    )
    foreach ($entry in $sources) {
        $entry.Root = [IO.Path]::GetFullPath($entry.Root)
        $allowedHls = [IO.Path]::GetFullPath((Join-Path $nextRoot 'hls_build/jobs')) + '\'
        if (-not $entry.Root.StartsWith($allowedHls, [StringComparison]::OrdinalIgnoreCase)) { throw 'HLS source must be an isolated next job.' }
        $state = Get-Content -LiteralPath (Join-Path $entry.Root 'status.json') -Raw | ConvertFrom-Json
        if ($state.State -ne 'PASS') { throw "HLS source job is not complete: $($entry.Root)" }
        $manifest = Get-Content -LiteralPath (Join-Path $entry.Root 'source_manifest.json') -Raw | ConvertFrom-Json
        foreach ($relative in @($entry.Source, 'include/next_stream.hpp')) {
            $key = "FPGA_ORB_HLS/fastv2/next/$relative"
            $record = @($manifest | Where-Object RelativePath -eq $key)
            if ($record.Count -ne 1 -or (Get-FileHash -LiteralPath (Join-Path $nextRoot $relative)).Hash -ne $record[0].SHA256) {
                throw "Source differs from synthesized $($entry.Kernel) input: $relative"
            }
        }
        foreach ($relative in @('fastv2/include/fastv2.hpp','fastv2/include/fastv2_profile_rom.hpp')) {
            $record = @($manifest | Where-Object RelativePath -eq "FPGA_ORB_HLS/$relative")
            if ($record.Count -ne 1 -or (Get-FileHash -LiteralPath (Join-Path $repoRoot $relative)).Hash -ne $record[0].SHA256) {
                throw "Public ABI/profile differs from synthesized input: $relative"
            }
        }
        $report = Join-Path $entry.Root "projects/next_$($entry.Kernel)/solution1/syn/report/fastv2_$($entry.Kernel)_hls_csynth.xml"
        [xml]$reportXml = Get-Content -LiteralPath $report -Raw
        if ($reportXml.profile.UserAssignments.TopModelName -ne "fastv2_$($entry.Kernel)_hls" -or
            $reportXml.profile.UserAssignments.Part -ne 'xc7z015clg485-2' -or
            [double]$reportXml.profile.UserAssignments.TargetClockPeriod -ne 10) { throw 'Wrong synthesized device or clock.' }
        $control = Get-Content -LiteralPath (Join-Path $entry.Root "projects/next_$($entry.Kernel)/solution1/syn/verilog/fastv2_$($entry.Kernel)_hls_control_s_axi.v") -Raw
        $offsets = if ($entry.Kernel -eq 'front') {
            @{ AP_RETURN_0=0x10; IMAGE_R_DATA_0=0x18; SELECTED_DATA_0=0x20; RESULT_DATA_0=0x28; WIDTH_DATA_0=0x30; HEIGHT_DATA_0=0x38; BUDGET_DATA_0=0x40; HIGH_THRESHOLD_DATA_0=0x48; LOW_THRESHOLD_DATA_0=0x50; DUAL_THRESHOLD_DATA_0=0x58; SELECTED_CAPACITY_DATA_0=0x60 }
        } else {
            $scalar = if ($entry.Kernel -eq 'resize') { 'SOURCE_LEVEL_DATA_0' } else { 'LEVEL_DATA_0' }
            $imageOffsets = @{ AP_RETURN_0=0x10; SOURCE_DATA_0=0x18; DESTINATION_DATA_0=0x20 }
            $imageOffsets[$scalar] = 0x28; $imageOffsets
        }
        foreach ($name in $offsets.Keys) {
            $match = [regex]::Match($control, "ADDR_$name\s*=\s*\d+'h([0-9a-fA-F]+)")
            if (-not $match.Success -or [Convert]::ToInt32($match.Groups[1].Value,16) -ne $offsets[$name]) {
                throw "Unexpected $($entry.Kernel) MMIO register: $name"
            }
        }
    }
    $contract = Get-Content -LiteralPath (Join-Path $nextRoot 'ps/board_contract.h') -Raw
    $expectedContract = @{ NEXT_FRONT_BASE=0x43C00000; NEXT_RESIZE_BASE=0x43C10000; NEXT_GAUSSIAN_BASE=0x43C20000;
        NEXT_CTRL=0; NEXT_RETURN=0x10; NEXT_SOURCE=0x18; NEXT_DESTINATION=0x20; NEXT_LEVEL=0x28;
        NEXT_FRONT_RESULT=0x28; NEXT_WIDTH=0x30; NEXT_HEIGHT=0x38; NEXT_BUDGET=0x40;
        NEXT_HIGH=0x48; NEXT_LOW=0x50; NEXT_DUAL=0x58; NEXT_CAPACITY=0x60 }
    foreach ($name in $expectedContract.Keys) {
        $match=[regex]::Match($contract,"\b$name\s*=\s*(0x[0-9a-fA-F]+|[0-9]+)")
        if (-not $match.Success) { throw "Missing PS register contract: $name" }
        $value=if ($match.Groups[1].Value.StartsWith('0x')) { [Convert]::ToInt32($match.Groups[1].Value.Substring(2),16) } else { [int]$match.Groups[1].Value }
        if ($value -ne $expectedContract[$name]) { throw "PS register contract differs from frozen RTL: $name" }
    }
    if ($PreflightOnly) { Write-Output 'NEXT_BOARD_PREFLIGHT_PASS kernels=3 source_hashes=PASS device=xc7z015clg485-2 clock_ns=10 MMIO=PASS'; return }
    $jobName = '{0}_{1}' -f (Get-Date -Format 'yyyyMMdd_HHmmss'), ([guid]::NewGuid().ToString('N').Substring(0,8))
    $jobRoot = Join-Path $buildRoot "jobs/$jobName"
    New-Item -ItemType Directory -Path (Join-Path $jobRoot 'inputs') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $jobRoot 'projects') -Force | Out-Null
    foreach ($entry in $sources) {
        Copy-Item -LiteralPath (Join-Path $entry.Root "projects/next_$($entry.Kernel)") -Destination (Join-Path $jobRoot 'projects') -Recurse
        Copy-Item -LiteralPath (Join-Path $entry.Root 'source_manifest.json') -Destination (Join-Path $jobRoot "inputs/hls_$($entry.Kernel)_manifest.json")
    }
    $files = [ordered]@{
        'ps_config.tcl' = (Join-Path $repoRoot 'vivado/ps_config.tcl')
        'build_system.tcl' = (Join-Path $nextRoot 'vivado/build_system.tcl')
        'timing_checks.tcl' = (Join-Path $nextRoot 'vivado/timing_checks.tcl')
        'export_board_ip.tcl' = (Join-Path $nextRoot 'hls/export_board_ip.tcl')
        'build_board_sdk.tcl' = (Join-Path $nextRoot 'ps/build_board_sdk.tcl')
        'run_board.tcl' = (Join-Path $nextRoot 'ps/run_board.tcl')
        'board_result.tcl' = (Join-Path $nextRoot 'ps/board_result.tcl')
        'board_benchmark.c' = (Join-Path $nextRoot 'ps/board_benchmark.c')
        'board_pipeline.inc' = (Join-Path $nextRoot 'ps/board_pipeline.inc')
        'board_contract.h' = (Join-Path $nextRoot 'ps/board_contract.h')
        'next_backend.c' = (Join-Path $nextRoot 'ps/next_backend.c')
        'next_backend.h' = (Join-Path $nextRoot 'include/next_backend.h')
        'backend_tables.h' = (Join-Path $nextRoot 'include/backend_tables.h')
        'full_reference.inc' = (Join-Path $nextRoot 'ps/full_reference.inc')
        'front_reference.inc' = (Join-Path $repoRoot 'fastv2/ps/fastv2_reference.inc')
        'reference_raw.bin' = (Join-Path $repoRoot 'fastv2/ps/reference_pyramid.bin')
        'reference_blur.bin' = (Join-Path $nextRoot 'host_build/reference_blur.bin')
        'input.pgm' = (Join-Path $repoRoot '../FPGA_ORB_SHARED_v1.0/vectors/flight_20261004T184929_998838_6bb93c56_f0000/input.pgm')
    }
    $proof = foreach ($name in $files.Keys) {
        $target = Join-Path $jobRoot "inputs/$name"
        Copy-Item -LiteralPath $files[$name] -Destination $target
        [ordered]@{ Name=$name; Source=$files[$name]; SHA256=(Get-FileHash -LiteralPath $target).Hash }
    }
    [IO.File]::WriteAllText((Join-Path $jobRoot 'input_manifest.json'), (ConvertTo-Json -InputObject @($proof)))
    $arguments = '-NoProfile -File "{0}" -Foreground -RunDirectory "{1}" -Jobs {2} -TimeoutMinutes {3}' -f $PSCommandPath,$jobRoot,$Jobs,$TimeoutMinutes
    if ($SetupOnly) { $arguments += ' -SetupOnly' }
    $worker = Start-Process -FilePath (Get-Command pwsh).Source -ArgumentList $arguments -WorkingDirectory $jobRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $jobRoot 'stdout.log') -RedirectStandardError (Join-Path $jobRoot 'stderr.log') -PassThru
    $launch = [ordered]@{ ProcessId=$worker.Id; JobRoot=$jobRoot; Status=(Join-Path $jobRoot 'status.json'); Jobs=$Jobs; SetupOnly=[bool]$SetupOnly; Sources=$sources; StartedUtc=[DateTime]::UtcNow.ToString('o'); Revision='three_core_board_r7'; Part='xc7z015clg485-2'; Top='next_system_wrapper'; FabricHz=100000000 }
    $json = $launch | ConvertTo-Json -Depth 5
    [IO.File]::WriteAllText((Join-Path $jobRoot 'launcher.json'),$json)
    [IO.File]::WriteAllText((Join-Path $buildRoot 'last_board_job.json'),$json)
    Write-Output "NEXT_BOARD_STARTED PID=$($worker.Id) jobs=$Jobs setup_only=$([bool]$SetupOnly)"
    Write-Output "status: $($launch.Status)"
    Write-Output "logs: $jobRoot"
    exit 0
}
$jobFull = [IO.Path]::GetFullPath($RunDirectory)
$allowed = [IO.Path]::GetFullPath((Join-Path $buildRoot 'jobs')) + '\'
if (-not $jobFull.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe next board worker directory.' }
$statusPath = Join-Path $jobFull 'status.json'
$state = [ordered]@{ State='RUNNING'; Stage='export'; ProcessId=$PID; Jobs=$Jobs; StartedUtc=[DateTime]::UtcNow.ToString('o') }
$deadline = [DateTime]::UtcNow.AddMinutes($TimeoutMinutes)

function Save-BoardState {
    <# .SYNOPSIS Publish only this job's state and current stage. #>
    [IO.File]::WriteAllText($statusPath, ($state | ConvertTo-Json))
}
function Invoke-OwnedTool([string]$Command, [string]$Name, [switch]$AllowFailure, [string]$WorkingDirectory) {
    <# .SYNOPSIS Run a bounded child tool; timeout kills only the process created here. #>
    $stdout = Join-Path $jobFull "$Name.stdout.log"; $stderr = Join-Path $jobFull "$Name.stderr.log"
    $arguments = '/d /s /c ""{0}" {1}"' -f $wrapper,$Command
    if (-not $WorkingDirectory) { $WorkingDirectory=$jobFull }
    $child = Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/cmd.exe') -ArgumentList $arguments -WorkingDirectory $WorkingDirectory -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    $state['ToolProcessId']=$child.Id; $state['ToolStdout']=$stdout; $state['ToolStderr']=$stderr; Save-BoardState
    $null=$child.Handle
    while (-not $child.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -ge $deadline) {
            & (Join-Path $env:SystemRoot 'System32/taskkill.exe') /PID $child.Id /T /F | Out-Null
            throw "Next board watchdog expired after $TimeoutMinutes minutes."
        }
    }
    $child.WaitForExit(); $toolCode=$child.ExitCode
    if ($null -eq $toolCode) { throw 'Cannot read tool exit code.' }
    $state['ToolExitCode']=$toolCode; Save-BoardState
    if ($toolCode -ne 0 -and -not $AllowFailure) { throw "$Name failed: tool exit=$toolCode; inspect $stdout" }
}
$code=1
try {
    Save-BoardState
    $env:NEXT_BOARD_ROOT=$jobFull; $env:NEXT_BOARD_JOBS=[string]$Jobs
    $env:NEXT_BOARD_SETUP_ONLY=if ($SetupOnly) { '1' } else { '0' }
    foreach ($kernel in @('front','resize','gaussian')) {
        $state.Stage="export_$kernel"; Save-BoardState
        $env:NEXT_EXPORT_PROJECT=Join-Path $jobFull "projects/next_$kernel"
        $env:NEXT_EXPORT_STAMP=Join-Path $jobFull "export_$kernel.success"
        Invoke-OwnedTool ('vivado_hls -f "{0}"' -f (Join-Path $jobFull 'inputs/export_board_ip.tcl')) "export_$kernel" -AllowFailure
        $ip=Join-Path $env:NEXT_EXPORT_PROJECT 'solution1/impl/ip'
        if (-not (Test-Path -LiteralPath $env:NEXT_EXPORT_STAMP)) {
            # Existing approved 2018.3 date/revision workaround, only on cloned outputs.
            $log=Get-Content -LiteralPath (Join-Path $jobFull "export_$kernel.stdout.log") -Raw
            $log += Get-Content -LiteralPath (Join-Path $jobFull "export_$kernel.stderr.log") -Raw
            if ($log -notmatch 'core_revision|bad lexical cast|Revision') {
                $diagnostic = @($log -split '\r?\n' | Where-Object { $_ -match 'ERROR:|NEXT_BOARD_EXPORT_FAILED:' }) -join ' | '
                throw "HLS export failed for ${kernel}: $diagnostic; inspect export_$kernel.stdout.log"
            }
            $pack=Join-Path $ip 'run_ippack.tcl'
            if (-not (Test-Path -LiteralPath $pack)) { throw "Revision failure without generated pack script: $kernel" }
            $original=[IO.File]::ReadAllText($pack)
            $patched=[regex]::Replace($original,'(?m)^set Revision\s+"[0-9]+"\s*$','set Revision    "1"')
            if ($original -eq $patched) { throw "No known IP revision assignment to patch: $kernel" }
            [IO.File]::WriteAllText($pack,$patched)
            # Packaging Tcl uses relative paths; it still runs as a bounded owned child.
            Invoke-OwnedTool ('vivado -mode batch -source "{0}"' -f $pack) "package_$kernel" -WorkingDirectory $ip
        }
        if (-not (Test-Path -LiteralPath (Join-Path $ip 'component.xml'))) { throw "Missing packaged IP: $kernel" }
    }
    $state.Stage='vivado'; Save-BoardState
    Invoke-OwnedTool ('vivado -mode batch -source "{0}"' -f (Join-Path $jobFull 'inputs/build_system.tcl')) 'vivado'
    $stamp=Join-Path $jobFull $(if ($SetupOnly) { 'vivado/.vivado_setup_success' } else { 'vivado/.vivado_success' })
    if (-not (Test-Path -LiteralPath $stamp)) { throw 'Missing verified Vivado success marker.' }
    if (-not $SetupOnly) {
        $state.Stage='sdk'; Save-BoardState
        Invoke-OwnedTool ('xsct "{0}"' -f (Join-Path $jobFull 'inputs/build_board_sdk.tcl')) 'sdk'
        if (-not (Test-Path -LiteralPath (Join-Path $jobFull 'sdk/.sdk_success'))) { throw 'Missing SDK success marker.' }
        $artifactProof=foreach ($relative in @('vivado/next_system_wrapper.hdf',
            'vivado/next_fpga.runs/impl_1/next_system_wrapper.bit',
            'sdk/next_benchmark/Debug/next_benchmark.elf')) {
            [ordered]@{ RelativePath=$relative; SHA256=(Get-FileHash -LiteralPath (Join-Path $jobFull $relative)).Hash }
        }
        [IO.File]::WriteAllText((Join-Path $jobFull 'artifact_manifest.json'),(ConvertTo-Json -InputObject @($artifactProof)))
    }
    $state.State='PASS'; $state.Stage='complete'; $code=0
} catch {
    $state.State='FAIL'; $state['Error']=$_.Exception.Message
    Write-Error $_.Exception.Message -ErrorAction Continue
} finally {
    $state['CompletedUtc']=[DateTime]::UtcNow.ToString('o'); $state['ExitCode']=$code; Save-BoardState
}
exit $code
