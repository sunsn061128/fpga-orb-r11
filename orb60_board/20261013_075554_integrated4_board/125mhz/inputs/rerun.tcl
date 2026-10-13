# Bounded same-session rerun after CPU-exact verification of this exact bit/ELF.
# Never resets/programs PL or reloads reference data. Only reloads the same ELF.
# Caller must check pairing hashes and the immediately preceding exact log.
if {[llength $argv]!=6} {error "Expected paired job, frames, Verified/Production, preceding mailbox file"}
lassign $argv root iterations mode preceding expected_features frequency
if {$mode ni {Verified Production} || ![string is integer -strict $iterations] ||
    $iterations<1 || $iterations>1000} {error "Invalid rerun"}
set root [file normalize $root]
# @brief Capture all 128 stable result words.
proc mailbox {} {
    set out {};foreach v [mrd -value 0x01000000 128] {lappend out [expr {$v&0xffffffff}]}
    if {[llength $out]!=128} {error "Short mailbox"};return $out
}
if {[catch {
    connect
    set chain {}
    for {set probe 0} {$probe < 5} {incr probe} {
        set chain [jtag targets]
        if {[string first "Digilent JTAG-SMT2 C306ACCBABCD" $chain]>=0 && [regexp -all {xc7z015 \(idcode 0373b093} $chain]==1} {break}
        after 1000
    }
    puts "ORB60_JTAG_CHAIN=$chain"
    if {[string first "Digilent JTAG-SMT2 C306ACCBABCD" $chain]<0 ||
       [regexp -all {xc7z015 \(idcode 0373b093} $chain]!=1} {error "Cable/device mismatch"}
    jtag targets -set -filter {name =~ "*C306ACCBABCD*"}
    if {[lsearch -exact [jtag frequency -list] 1000000]<0} {error "Missing safe adapter frequency"}
    jtag frequency 1000000
    targets -set -nocase -filter {name =~ "ARM Cortex-A9 MPCore #0"}
    catch {stop};set old [mailbox]
    set f [open $preceding r];set expected_old [string trim [read $f]];close $f
    if {[llength $expected_old]!=128 || $old ne $expected_old} {error "Hardware session differs from the immediately preceding accepted result"}
    if {[lindex $old 2]!=0x60c00002 || [lindex $old 4] ni {2 4} || [lindex $old 5]!=0} {error "No prior verified/completed same-session image"}
    set header [mrd -value 0x06000000 8]
    if {[expr {[lindex $header 0]&0xffffffff}]!=0x60b30001 || [lindex $header 3]!=$expected_features} {error "Oracle identity mismatch"}
    set fclk_register [expr {[mrd -value 0xf8000170] & 0x03f03f30}]
    set iopll_register [expr {[mrd -value 0xf8000108] & 0x0007f000}]
    if {$frequency != 125 || $fclk_register != 0x00200400 || $iopll_register != 0x0001E000} {error "PS clock pairing mismatch"}
    puts "ORB60_CLOCK_PROOF frequency=$frequency fclk_register=[format 0x%08x $fclk_register] iopll_register=[format 0x%08x $iopll_register]"
    dow [file join $root elf orb60_board.elf]
    mwr 0x01000000 0x60c00001;mwr 0x01000004 $iterations;mwr 0x01000008 0;mwr 0x01000010 0
    mwr 0x0100001c [expr {$mode eq "Verified"?1:2}];con
    after 10000;set deadline [expr {[clock milliseconds]+60000+1000*$iterations}]
    while {[clock milliseconds]<$deadline} {
        set w [mailbox];if {[lindex $w 2]==0x60c00002 && [lindex $w 4] in {2 3 4}} {break};after 1000
    }
    stop;if {[mailbox] ne $w} {error "Unstable terminal result"}
    puts "ORB60_MAILBOX_WORDS=$w"
    set expected [expr {$mode eq "Verified"?2:4}]
    if {[lindex $w 2]!=0x60c00002 || [lindex $w 4]!=$expected || [lindex $w 5]!=0 || [lindex $w 6]!=$iterations} {error "Rerun failed $w"}
    puts "ORB60_BOARD_RESULT mode=$mode completed=[lindex $w 6]/$iterations status=[lindex $w 4] features=[lindex $w 16]"
    puts "processing_avg_us=[lindex $w 10] min_us=[lindex $w 13] max_us=[lindex $w 14] fps_milli=[lindex $w 15] wall_us=[lindex $w 11] verification_us=[lindex $w 12]"
    puts "input_contract=[expr {[lindex $w 78]==0x44445231 ? {direct_immutable_ddr} : {per_frame_cpu_copy}}] external_capture_transfer_excluded=1"
    for {set l 0} {$l<8} {incr l} {puts "level=$l layer_cycles=[lindex $w [expr {20+$l}]] descriptor_cycles=[lindex $w [expr {28+$l}]] layer_PS_observed_us=[lindex $w [expr {36+$l}]] descriptor_PS_observed_us=[lindex $w [expr {44+$l}]] count=[lindex $w [expr {52+$l}]] offset=[lindex $w [expr {60+$l}]]"}
    disconnect;puts "ORB60_BOARD_COMPLETED [expr {$mode eq "Verified"?"CPU_EXACT_PASS":"COMPLETE_UNVERIFIED"}]"
} failure]} {catch {stop};catch {disconnect};puts stderr "ORB60_RERUN_ERROR: $failure";exit 2}
exit 0
