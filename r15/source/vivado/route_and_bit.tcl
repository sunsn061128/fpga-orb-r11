# Appended after authored block-design creation. Strict accepted routed hardware.
launch_runs synth_1 -jobs $jobs
wait_on_run synth_1
if {[get_property STATUS [get_runs synth_1]] ne "synth_design Complete!"} {error "Synthesis did not complete"}
open_run synth_1
report_utilization -file [file join $output post_synth_utilization.rpt]
report_timing_summary -file [file join $output post_synth_timing.rpt]
set_property strategy Performance_Explore [get_runs impl_1]
launch_runs impl_1 -to_step route_design -jobs $jobs
wait_on_run impl_1
set run_dir [get_property DIRECTORY [get_runs impl_1]]
if {![file exists [file join $run_dir .route_design.end.rst]] || [file exists [file join $run_dir .route_design.error.rst]] ||
    ![file exists [file join $run_dir s_wrapper_routed.dcp]]} {error "Missing complete routed run"}
open_run impl_1
source [file join $inputs timing_checks.tcl]
proc check_routed {output} {
    report_utilization -file [file join $output post_route_utilization.rpt]
    report_utilization -hierarchical -file [file join $output post_route_hierarchy.rpt]
    report_timing_summary -check_timing_verbose -report_unconstrained -file [file join $output post_route_timing.rpt]
    report_route_status -file [file join $output post_route_route_status.rpt]
    report_drc -file [file join $output post_route_drc.rpt]
    check_timing -verbose -file [file join $output post_route_check_timing.rpt]
    set setup [get_timing_paths -delay_type max -max_paths 1]
    set hold [get_timing_paths -delay_type min -max_paths 1]
    if {[llength $setup]!=1 || [llength $hold]!=1} {error "Missing routed paths"}
    set wns [get_property SLACK $setup];set whs [get_property SLACK $hold]
    set f [open [file join $output post_route_metrics.tsv] w]
    puts $f "part\txc7z015clg485-2\nfrequency_mhz\t100\nwns_ns\t$wns\nwhs_ns\t$whs";close $f
    if {$wns<0 || $whs<0} {error "Failed routed setup/hold"}
    set f [open [file join $output post_route_check_timing.rpt] r];set report [read $f];close $f
    next_check_timing_coverage $report
    set f [open [file join $output post_route_route_status.rpt] r];set routing [read $f];close $f
    foreach {name pattern} {
        routable {# of routable nets[. \t]*:[ \t]*([0-9]+)}
        routed {# of fully routed nets[. \t]*:[ \t]*([0-9]+)}
        errors {# of nets with routing errors[. \t]*:[ \t]*([0-9]+)}
    } {
        if {![regexp $pattern $routing unused value]} {error "Missing route count $name"}
        set counts($name) $value
    }
    if {$counts(routable)!=$counts(routed) || $counts(errors)!=0} {error "Incomplete/error routing"}
    foreach violation [get_drc_violations -quiet] {
        if {[get_property SEVERITY $violation] in {Error {Critical Warning}}} {error "DRC failure $violation"}
    }
}
check_routed $output
launch_runs impl_1 -to_step write_bitstream -jobs $jobs
wait_on_run impl_1
if {![file exists [file join $run_dir .write_bitstream.end.rst]] || [file exists [file join $run_dir .write_bitstream.error.rst]] ||
    ![file exists [file join $run_dir s_wrapper.bit]]} {error "No completed bitstream"}
open_run impl_1
check_routed $output
file copy [file join $run_dir s_wrapper.sysdef] [file join $output s_wrapper.hdf]
set f [open [file join $output accepted.stamp] w];puts $f "ORB4_ROUTED_BIT_ACCEPTED";close $f
puts "ORB4_ROUTED_BIT_ACCEPTED"
exit 0
