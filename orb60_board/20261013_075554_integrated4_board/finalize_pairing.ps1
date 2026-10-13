[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidateSet(100,125)][int]$FrequencyMHz)
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "${FrequencyMHz}mhz"
$workspace=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$init=Join-Path $PSScriptRoot "psgen_v2_${FrequencyMHz}/psgen.srcs/sources_1/bd/s/ip/s_ps7_0/ps7_init.tcl"
$original=Join-Path $workspace 'orb60_board/20261013_071953_4ppc100mhz_2018_3/inputs'
$props=@{}
Get-Content -LiteralPath (Join-Path $PSScriptRoot "ps_properties_${FrequencyMHz}.tsv")|ForEach-Object{$values=$_ -split "`t",2;$props[$values[0]]=$values[1]}
if([double]$props['CONFIG.PCW_ACT_FPGA0_PERIPHERAL_FREQMHZ'] -ne $FrequencyMHz){throw 'Generated FCLK frequency mismatch'}
if($props['CONFIG.PCW_ACT_APU_PERIPHERAL_FREQMHZ'] -ne '666.666687'){throw 'PS timer clock mismatch'}
if($FrequencyMHz -eq 100 -and (Get-FileHash $init).Hash -ne (Get-FileHash (Join-Path $original 'ps7_init.tcl')).Hash){throw 'Generated 100 MHz init differs from accepted physical board'}
$initBody=[IO.File]::ReadAllText($init)
$fclk=[regex]::Match($initBody,'mask_write 0XF8000170 0x03F03F30 (0x[0-9A-Fa-f]+)').Groups[1].Value
$iopll=[regex]::Match($initBody,'mask_write 0XF8000108 0x0007F000 (0x[0-9A-Fa-f]+)').Groups[1].Value
if(!$fclk -or !$iopll){throw 'Generated FCLK mask not found'}
Copy-Item -LiteralPath $init -Destination (Join-Path $root 'inputs/ps7_init.tcl')
$script=[IO.File]::ReadAllText((Join-Path $original 'run.tcl'))
$script=$script.Replace('[llength $argv]!=4','[llength $argv]!=5').Replace('Expected job, normal/noise/zero, frames, mode','Expected job, normal/noise/zero, frames, mode, frequency').Replace('lassign $argv root frame iterations mode','lassign $argv root frame iterations mode frequency').Replace('orb60_4ppc.bit','orb60.bit')
$clockCheck=@'
    set fclk_register [expr {[mrd -value 0xf8000170] & 0x03f03f30}]
    set iopll_register [expr {[mrd -value 0xf8000108] & 0x0007f000}]
    if {$frequency != @FREQ@ || $fclk_register != @FCLK@ || $iopll_register != @IOPLL@} {error "PS clock pairing mismatch"}
    puts "ORB60_CLOCK_PROOF frequency=$frequency fclk_register=[format 0x%08x $fclk_register] iopll_register=[format 0x%08x $iopll_register]"
'@
$clockCheck=$clockCheck.Replace('@FREQ@',"$FrequencyMHz").Replace('@FCLK@',$fclk).Replace('@IOPLL@',$iopll)
$discover=@'
    set chain {}
    for {set probe 0} {$probe < 5} {incr probe} {
        set chain [jtag targets]
        if {[string first "Digilent JTAG-SMT2 C306ACCBABCD" $chain]>=0 && [regexp -all {xc7z015 \(idcode 0373b093} $chain]==1} {break}
        after 1000
    }
    puts "ORB60_JTAG_CHAIN=$chain"
'@
$script=$script.Replace('    set chain [jtag targets];puts $chain',$discover)
$script=$script.Replace('    fpga -file',$clockCheck+"`n    fpga -file")
[IO.File]::WriteAllText((Join-Path $root 'inputs/run.tcl'),$script)
$rerun=[IO.File]::ReadAllText((Join-Path $workspace 'orb60_4ppc/ps_board/rerun.tcl'))
$rerun=$rerun.Replace('[llength $argv]!=4','[llength $argv]!=6').Replace('lassign $argv root iterations mode preceding','lassign $argv root iterations mode preceding expected_features frequency').Replace('[lindex $header 3]!=280','[lindex $header 3]!=$expected_features').Replace('Normal oracle identity mismatch','Oracle identity mismatch')
$rerun=$rerun.Replace('    connect;set chain [jtag targets]',"    connect`n"+$discover)
$rerun=$rerun.Replace('    dow [file join $root elf orb60_board.elf]',$clockCheck+"`n    dow [file join `$root elf orb60_board.elf]")
[IO.File]::WriteAllText((Join-Path $root 'inputs/rerun.tcl'),$rerun)
$files=@(Get-ChildItem -LiteralPath (Join-Path $root 'inputs') -File -Recurse)+@(Get-Item -LiteralPath (Join-Path $root 'orb60.bit'),(Join-Path $root 'elf/orb60_board.elf'))
foreach($frame in @('normal','noise','zero')){$files+=@(Get-ChildItem -LiteralPath (Join-Path $root $frame) -File)}
[ordered]@{FrequencyMHz=$FrequencyMHz;Part='xc7z015clg485-2';Top='s_wrapper';BitSHA256=(Get-FileHash (Join-Path $root 'orb60.bit')).Hash;ELFSHA256=(Get-FileHash (Join-Path $root 'elf/orb60_board.elf')).Hash;GeneratedPSInit=$init;ExpectedFCLKRegister=$fclk;ExpectedIOPLLRegister=$iopll;Files=@($files|ForEach-Object{[ordered]@{Path=$_.FullName.Substring($root.Length+1);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}})}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $root 'manifest.json')
Write-Output "BOARD_PAIRING_READY frequency=$FrequencyMHz FCLK=$fclk IO_PLL=$iopll"
