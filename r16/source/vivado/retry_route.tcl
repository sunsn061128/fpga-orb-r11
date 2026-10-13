set root [file normalize $::env(ORB4_BOARD_JOB)]
set inputs [file join $root inputs]
set output [file join $root vivado]
set_param general.maxThreads 8
if {[catch {
    open_checkpoint [file join $root linked_synth.dcp]
    if {[get_property PART [current_design]] ne "xc7z015clg485-2"} {error "Wrong linked target"}
    if {[llength [get_cells -hierarchical -quiet -filter {IS_BLACKBOX == 1}]]} {error "Linked checkpoint still has black boxes"}
    set clocks [get_clocks]
    if {[llength $clocks]!=1 || abs([get_property PERIOD $clocks]-10.0)>0.001} {error "Unexpected linked clock"}
    report_utilization -file [file join $output post_synth_utilization.rpt]
    report_timing_summary -file [file join $output post_synth_timing.rpt]
    if {$::env(ORB4_CARRY_REMAP)>0} {
        set_property CARRY_REMAP $::env(ORB4_CARRY_REMAP) [get_cells -hierarchical -filter {REF_NAME == CARRY4}]
    }
    if {$::env(ORB4_REMAP_CE)>0} {
        set by_enable [dict create]
        foreach cell [get_cells -hierarchical -filter {REF_NAME == FDRE}] {
            set pin [get_pins -quiet -of_objects $cell -filter {REF_PIN_NAME == CE}]
            set net [get_nets -quiet -of_objects $pin]
            if {[llength $net]==1} {dict lappend by_enable $net $cell}
        }
        set targets {}
        dict for {net cells} $by_enable {
            if {[llength $cells]<=$::env(ORB4_REMAP_CE)} {
                set_property CONTROL_SET_REMAP ENABLE $cells
                lappend targets {*}$cells
            }
        }
        set f [open [file join $output remapped_ce_cells.txt] w];puts $f [join $targets \n];close $f
        puts "ORB4_REMAP_CE_REGISTERS [llength $targets]"
        if {[llength $targets]==0} {error "No low-fanout CE targets"}
    }
    opt_design -directive ExploreWithRemap
    opt_design -control_set_merge
    report_utilization -file [file join $output post_opt_utilization.rpt]
    report_control_sets -verbose -file [file join $output post_opt_control_sets.rpt]
    write_checkpoint [file join $output s_wrapper_opt.dcp]
    if {$::env(ORB4_NO_FANOUT_OPT)} {place_design -no_fanout_opt} else {place_design}
    report_utilization -file [file join $output post_place_utilization.rpt]
    write_checkpoint [file join $output s_wrapper_placed.dcp]
    phys_opt_design -directive Explore
    route_design -directive Explore
    write_checkpoint [file join $output s_wrapper_routed.dcp]
    source [file join $inputs routed_checks.tcl]
    check_routed $output
    write_bitstream [file join $output s_wrapper.bit]
    check_routed $output
    set f [open [file join $output accepted.stamp] w];puts $f "ORB4_ROUTED_BIT_ACCEPTED";close $f
    puts "ORB4_ROUTED_BIT_ACCEPTED"
} failure]} {puts stderr "ORB4_ROUTE_FAILED $failure";exit 1}
exit 0
