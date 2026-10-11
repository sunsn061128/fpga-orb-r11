set project_root [file normalize [file join [file dirname [info script]] ..]]
set reference_root [file normalize [file join $project_root .. FPGA_ORB_SHARED_v1.0]]
set source_file [file join $project_root src orb_hls.cpp]
set include_flags "-std=c++0x -I[file join $project_root include] -I$reference_root"
if {[info exists ::env(ORB_HLS_COSIM)] && $::env(ORB_HLS_COSIM) eq "1"} {
    append include_flags " -DORB_HLS_COSIM_SMALL_DEPTH"
}
set build_root [file join $project_root hls_build]
file mkdir $build_root
cd $build_root

# @brief Build and optionally RTL-co-simulate one HLS top.
# @param name Project directory name below hls_build.
# @param top Synthesizable C function used as the HLS top.
# @param source_file Authored implementation source path.
# @param include_flags Compiler flags shared by design and testbench sources.
proc build_ip {name top source_file include_flags} {
    open_project -reset $name
    set_top $top
    add_files $source_file -cflags $include_flags
    set test_name [string range $name 4 end]
    if {$name eq "orb_fast_stream"} {
        add_files -tb [file join $::project_root hls tb_${test_name}.cpp] \
            -cflags "$include_flags -DORB_FAST_HLS_CSIM"
    } else {
        add_files -tb [file join $::project_root hls tb_${test_name}.cpp] -cflags $include_flags
    }
    open_solution -reset solution1
    set_part {xc7z015clg485-2}
    create_clock -period 10 -name default
    csim_design -clean
    csynth_design
    if {[info exists ::env(ORB_HLS_COSIM)] && $::env(ORB_HLS_COSIM) eq "1"} {
        cosim_design -rtl verilog -trace_level none
    }
    close_project
}

if {[catch {
    set selection both
    if {[info exists ::env(ORB_HLS_SELECTION)]} {
        set selection $::env(ORB_HLS_SELECTION)
    }
    if {$selection eq "both" || $selection eq "resize"} {
        build_ip orb_resize orb_resize_hls $source_file $include_flags
    }
    if {$selection eq "both" || $selection eq "level"} {
        build_ip orb_level orb_level_hls $source_file $include_flags
    }
    if {$selection eq "fast"} {
        build_ip orb_fast_stream orb_fast_stream_hls $source_file $include_flags
    }
} failure]} {
    puts stderr "HLS_SCRIPT_FAILED: $failure"
    exit 1
}
if {[info exists ::env(ORB_HLS_SUCCESS_STAMP)]} {
    set stamp [open $::env(ORB_HLS_SUCCESS_STAMP) w]
    puts $stamp "HLS_SUCCESS"
    close $stamp
}
exit 0
