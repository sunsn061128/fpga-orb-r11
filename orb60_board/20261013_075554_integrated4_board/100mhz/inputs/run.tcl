# Explicitly authorized volatile board test, no flash/SD/eFUSE operations.
# @param argv New paired job, oracle subdirectory, 1..1000 frames, Verified/Production.
if {[llength $argv]!=5} {error "Expected job, normal/noise/zero, frames, mode, frequency"}
lassign $argv root frame iterations mode frequency
if {$frame ni {normal noise zero} || $mode ni {Verified Production} ||
    ![string is integer -strict $iterations] || $iterations<1 || $iterations>1000} {error "Bad board command"}
set root [file normalize $root]
foreach file [list [file join $root inputs ps7_init.tcl] [file join $root orb60.bit] \
    [file join $root elf orb60_board.elf] [file join $root $frame golden.bin] [file join $root pairing.json]] {
    if {![file isfile $file]} {error "Missing paired input: $file"}
}
set mode_id [expr {$mode eq "Verified"?1:2}]
# @brief Read a complete mailbox and normalize unsigned XSCT words.
proc mailbox {} {
    set result {};set raw [mrd -value 0x01000000 128]
    if {[llength $raw]!=128} {error "Short mailbox"}
    foreach value $raw {lappend result [expr {$value&0xffffffff}]};return $result
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
    targets -set -nocase -filter {name =~ "APU"};rst -system;after 1000
    targets -set -nocase -filter {name =~ "ARM Cortex-A9 MPCore #0"}
    source [file join $root inputs ps7_init.tcl];ps7_init
    set fclk_register [expr {[mrd -value 0xf8000170] & 0x03f03f30}]
    set iopll_register [expr {[mrd -value 0xf8000108] & 0x0007f000}]
    if {$frequency != 100 || $fclk_register != 0x00300600 || $iopll_register != 0x00036000} {error "PS clock pairing mismatch"}
    puts "ORB60_CLOCK_PROOF frequency=$frequency fclk_register=[format 0x%08x $fclk_register] iopll_register=[format 0x%08x $iopll_register]"
    fpga -file [file join $root orb60.bit]
    ps7_post_config;ps7_debug
    # No debugger MMIO reads: without an XSA, XSCT has no PL memory map.
    # The ARM app uses the explicitly audited new generated register ABI.
    puts "ARM_MMIO image=0x43c00000 descriptor=0x43c10000 no_legacy_PL_driver=1"
    dow [file join $root elf orb60_board.elf]
    dow -data [file join $root $frame golden.bin] 0x06000000
    mwr 0x01000000 0x60c00001;mwr 0x01000004 $iterations
    mwr 0x01000008 0;mwr 0x01000010 0;mwr 0x0100001c $mode_id
    con
    # Quiet observation: first read after ten seconds, then bounded intervals.
    after 10000
    set deadline [expr {[clock milliseconds]+60000+1000*$iterations}];set words {}
    while {[clock milliseconds]<$deadline} {
        set words [mailbox]
        if {[lindex $words 2]==0x60c00002 && [lindex $words 4] in {2 3 4}} {break}
        after 1000
    }
    stop
    if {[lindex $words 2]!=0x60c00002 || [lindex $words 3]!=1 || [lindex $words 4] ni {2 3 4}} {error "Nonterminal mailbox $words"}
    set second [mailbox];if {$words ne $second} {error "Unstable terminal result"}
    puts "ORB60_MAILBOX_WORDS=$words"
    set status [lindex $words 4]
    puts "ORB60_BOARD_RESULT mode=$mode frame=$frame status=$status error=[lindex $words 5] completed=[lindex $words 6]/$iterations stage=[lindex $words 8] detail=[lindex $words 9]"
    puts "processing_avg_us=[lindex $words 10] min_us=[lindex $words 13] max_us=[lindex $words 14] fps_milli=[lindex $words 15] wall_us=[lindex $words 11] verify_us=[lindex $words 12] features=[lindex $words 16] candidates_total=[lindex $words 17]"
    puts "cache_total_us=[lindex $words 18] config_total_us=[lindex $words 19] INPUT_COPY_EXCLUDED=1 PL_INTERNAL_CYCLES_EXCLUDE_STATS_WRITE=1"
    puts "input_contract=[expr {[lindex $words 78]==0x44445231 ? {direct_immutable_ddr} : {per_frame_cpu_copy}}] external_capture_transfer_excluded=1"
    for {set l 0} {$l<8} {incr l} {
        puts "level=$l layer_cycles=[lindex $words [expr {20+$l}]] descriptor_cycles=[lindex $words [expr {28+$l}]] layer_PS_observed_us=[lindex $words [expr {36+$l}]] descriptor_PS_observed_us=[lindex $words [expr {44+$l}]] count=[lindex $words [expr {52+$l}]] offset=[lindex $words [expr {60+$l}]]"
    }
    disconnect
    if {$status==3 || [lindex $words 6]!=$iterations || ($mode eq "Verified" && $status!=2) || ($mode eq "Production" && $status!=4)} {error "Failed exact/terminal test"}
    puts "ORB60_BOARD_COMPLETED [expr {$mode eq "Verified"?"CPU_EXACT_PASS":"COMPLETE_UNVERIFIED"}]"
} failure]} {catch {stop};catch {disconnect};puts stderr "ORB60_BOARD_ERROR: $failure";exit 2}
exit 0
