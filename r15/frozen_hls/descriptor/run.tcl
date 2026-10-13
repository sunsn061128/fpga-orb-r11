set job [file normalize $::env(ORB4_JOB)]
set next [file join $job inputs FPGA_ORB_HLS fastv2 next]
set fast [file join $next ..]
set kernel $::env(ORB4_KERNEL)
set mode $::env(ORB4_MODE)
set flags "-std=c++0x -DFASTV2_HLS_CSIM -I[file join $fast include] -I[file join $next include] -I[file join $next hls] -I[file join $job generated]"
cd $job
if {[catch {
    open_project project
    if {$kernel eq "layer4"} {
        set top fastv2_layer4_hls
        set source layer4_hls.cpp
        set bench tb_layer4_stream.cpp
    } elseif {$kernel eq "descriptor"} {
        set top fastv2_descriptor_hls
        set source descriptor2018_hls.cpp
        set bench tb_descriptor2018.cpp
    } else {error "Unknown kernel"}
    set_top $top
    add_files [file join $next src $source] -cflags $flags
    add_files -tb [file join $next hls $bench] -cflags $flags
    open_solution solution1
    set_part xc7z015clg485-2
    create_clock -period 10 -name default
    if {![info exists ::env(ORB4_REUSE_SYNTH)]} {
        if {$kernel eq "descriptor"} {csim_design -clean -O -argv [file join $job descriptor_vectors]} else {csim_design -clean -O}
        if {$mode ne "csim"} {csynth_design}
    }
    if {($mode eq "cosim" || $mode eq "export") && ![info exists ::env(ORB4_REUSE_COSIM)]} {
        if {$kernel eq "descriptor"} {
            cosim_design -rtl verilog -trace_level none -argv [file join $job descriptor_vectors]
        } else {cosim_design -rtl verilog -trace_level none}
        set rpt [file join $job project solution1 sim report ${top}_cosim.rpt]
        set f [open $rpt r];set text [read $f];close $f
        if {![regexp {\|[ \t]*Verilog[ \t]*\|[ \t]*Pass[ \t]*\|} $text]} {error "No actual RTL Verilog Pass"}
    }
    if {$mode eq "export" || $mode eq "package"} {
        if {[catch {export_design -rtl verilog -format ip_catalog} pack_error]} {
            set ip [file join $job project solution1 impl ip]
            set pack [file join $ip run_ippack.tcl]
            set log [file join $ip vivado.log]
            if {![file exists $pack] || ![file exists $log]} {error $pack_error}
            set f [open $log r];set detail [read $f];close $f
            if {![regexp {core_revision|bad lexical cast|Revision} $detail]} {error $pack_error}
            set f [open $pack r];set original [read $f];close $f
            if {![regsub -line {^set Revision[ \t]+"[0-9]+"[ \t]*$} $original {set Revision "1"} patched]} {error "Unknown revision failure"}
            set f [open $pack w];puts -nonewline $f $patched;close $f
            cd $ip
            exec cmd.exe /d /c vivado -mode batch -source run_ippack.tcl > [file join $job revision_package.log] 2>@1
            cd $job
            set f [open [file join $job revision_package.log] r];set detail [read $f];close $f
            if {[regexp -line {^ERROR:} $detail]} {error "Revision repackaging failed"}
        }
        if {![file exists [file join $job project solution1 impl ip component.xml]]} {error "No packaged IP"}
    }
    close_project
} failure]} {puts stderr "ORB4_HLS_FAILED $failure";exit 1}
set f [open [file join $job success.stamp] w];puts $f "ORB4_HLS_SUCCESS kernel=$kernel mode=$mode";close $f
exit 0
