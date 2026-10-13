# The project and checkpoint retry flows use the same physical acceptance gates.
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
