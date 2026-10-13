set project_root [file normalize [file join [file dirname [info script]] ..]]
set build_root [file join $project_root hls_build]
cd $build_root

# @brief Re-run RTL co-simulation from an existing small-depth level solution.
#
# This script intentionally does not reset the project or solution. Use it only
# after `scripts/synthesize.ps1 -Selection level -Cosim`; the m_axi depth values
# are compile-time properties and a full-depth solution can make XSIM wait at
# the first transaction when paired with the small co-simulation testbench.
if {[catch {
    open_project orb_level
    open_solution solution1
    cosim_design -rtl verilog -trace_level none
    close_project
} failure]} {
    puts stderr "HLS_COSIM_ONLY_FAILED: $failure"
    exit 1
}
exit 0
