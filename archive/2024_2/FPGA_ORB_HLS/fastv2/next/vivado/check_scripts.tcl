# Read-only Tcl completeness and Vivado 2018.3 command-option preflight.
set root [file normalize [file join [file dirname [info script]] ..]]
foreach relative {vivado/build_system.tcl vivado/timing_checks.tcl vivado/test_timing_checks.tcl hls/export_board_ip.tcl ps/build_board_sdk.tcl ps/run_board.tcl} {
    set input [open [file join $root $relative] r]
    set body [read $input]
    close $input
    if {![info complete $body]} { error "Incomplete Tcl script: $relative" }
}
puts [help check_timing]
puts [help assign_bd_address]
puts [help report_timing_summary]
source [file join $root vivado timing_checks.tcl]
next_check_timing_coverage "1. checking no_clock\n--------------------\n There are 0 register/latch pins with no clock.\n\n4. checking unconstrained_internal_endpoints\n--------------------------------------------\n There are 0 pins that are not constrained for maximum delay.\n"
puts NEXT_BOARD_TCL_COMPLETE_PASS
exit 0
