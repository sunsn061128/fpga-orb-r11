set project_root [file normalize [file join [file dirname [info script]] ..]]
set build_root [file join $project_root hls_build]
cd $build_root

# @brief Export one synthesized HLS solution as a Vivado IP-catalog component.
# @param name HLS project directory name below hls_build.
proc export_ip {name} {
    open_project $name
    open_solution solution1
    export_design -rtl verilog -format ip_catalog
    close_project
}

if {[catch {
    set selection both
    if {[info exists ::env(ORB_HLS_SELECTION)]} {
        set selection $::env(ORB_HLS_SELECTION)
    }
    if {$selection eq "both" || $selection eq "resize"} {
        export_ip orb_resize
    }
    if {$selection eq "both" || $selection eq "level"} {
        export_ip orb_level
    }
    if {$selection eq "fast"} {
        export_ip orb_fast_stream
    }
} failure]} {
    puts stderr "HLS_EXPORT_FAILED: $failure"
    exit 1
}
if {[info exists ::env(ORB_HLS_SUCCESS_STAMP)]} {
    set stamp [open $::env(ORB_HLS_SUCCESS_STAMP) w]
    puts $stamp "HLS_EXPORT_SUCCESS"
    close $stamp
}
exit 0
