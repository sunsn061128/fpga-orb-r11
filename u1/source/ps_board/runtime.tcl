# A new SDK workspace from the candidate's generated hardware definition.
set root [file normalize $::env(ORB4_PS_RUNTIME)]
set hardware [file join $root inputs ps_runtime.hdf]
set workspace [file join $root sdk]
if {[file exists $workspace]} {error "Refusing an existing SDK workspace"}
if {![file isfile $hardware]} {error "Missing candidate hardware definition"}
if {[catch {
    setws $workspace
    createhw -name next_hw -hwspec $hardware
    createbsp -name next_bsp -hwproject next_hw -proc ps7_cortexa9_0 -os standalone
    createapp -name next_benchmark -hwproject next_hw -proc ps7_cortexa9_0 -os standalone -lang C -app {Empty Application} -bsp next_bsp
    set f [open [file join $workspace next_benchmark src main.c] w]
    puts $f "int main(void) { return 0; }";close $f
    configapp -app next_benchmark compiler-optimization {Optimize more (-O2)}
    projects -build -type bsp -name next_bsp
    projects -build -type app -name next_benchmark
    foreach path {next_bsp/ps7_cortexa9_0/lib/libxil.a next_bsp/ps7_cortexa9_0/include/xparameters.h next_benchmark/src/lscript.ld next_benchmark/Debug/Xilinx.spec next_hw/ps7_init.tcl} {
        if {![file isfile [file join $workspace $path]]} {error "Missing generated SDK file $path"}
    }
} failure]} {puts stderr "ORB4_PS_RUNTIME_FAILED $failure";exit 1}
set f [open [file join $root accepted.stamp] w];puts $f ORB4_PS_RUNTIME_PASS;close $f
puts ORB4_PS_RUNTIME_PASS
exit 0
