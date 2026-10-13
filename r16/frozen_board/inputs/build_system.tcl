# Timing integration only: accepted layer + exact descriptor, shared PS7 DDR.
set root [file normalize $::env(ORB4_BOARD_JOB)]
set inputs [file join $root inputs]
set output [file join $root vivado]
set part xc7z015clg485-2
set jobs $::env(ORB4_BOARD_JOBS)
set frequency 100
if {[file exists $output]} {error "Refusing existing integrated Vivado project"}
file mkdir $output
set_param general.maxThreads 8
create_project p $output -part $part
set_property target_language Verilog [current_project]
foreach name {layer desc} {
    if {![file exists [file join $root ip $name component.xml]]} {error "Missing accepted $name IP"}
}
set_property ip_repo_paths [list [file join $root ip layer] [file join $root ip desc]] [current_project]
update_ip_catalog
create_bd_design s
set ps7 [create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 ps7]
source [file join $inputs ps_config.tcl]
set_property -dict [list CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ $frequency \
    CONFIG.PCW_USE_S_AXI_HP0 {1} CONFIG.PCW_USE_S_AXI_HP1 {1} \
    CONFIG.PCW_USE_S_AXI_HP2 {1} CONFIG.PCW_USE_S_AXI_HP3 {1}] $ps7
set layer [create_bd_cell -type ip -vlnv xilinx.com:hls:fastv2_layer4_hls:1.0 layer]
set desc [create_bd_cell -type ip -vlnv xilinx.com:hls:fastv2_descriptor_hls:1.0 desc]
set reset [create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 reset]
set_property CONFIG.C_EXT_RESET_HIGH {0} $reset
set control [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 control]
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {2}] $control
make_bd_intf_pins_external [get_bd_intf_pins $ps7/DDR]
make_bd_intf_pins_external [get_bd_intf_pins $ps7/FIXED_IO]
connect_bd_intf_net [get_bd_intf_pins $ps7/M_AXI_GP0] [get_bd_intf_pins $control/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $control/M00_AXI] [get_bd_intf_pins $layer/s_axi_control]
connect_bd_intf_net [get_bd_intf_pins $control/M01_AXI] [get_bd_intf_pins $desc/s_axi_control]
set converters [list $control]
# Pair raw accesses HP0, blur accesses HP1, next-image/features HP2,
# points + statistics HP3. Each AMD interconnect arbitrates 2 AXI4 masters
# and supplies the required 64-bit AXI3 PS7 HP protocol conversion.
foreach {layer_bundle desc_bundle hp} {gmem0 gmem0 0 gmem2 gmem1 1 gmem3 gmem3 2 gmem1 gmem2 3} {
    set ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 mem${hp}]
    set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {1}] $ic
    connect_bd_intf_net [get_bd_intf_pins $layer/m_axi_$layer_bundle] [get_bd_intf_pins $ic/S00_AXI]
    connect_bd_intf_net [get_bd_intf_pins $desc/m_axi_$desc_bundle] [get_bd_intf_pins $ic/S01_AXI]
    connect_bd_intf_net [get_bd_intf_pins $ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP${hp}]
    lappend converters $ic
}
set clock [get_bd_pins $ps7/FCLK_CLK0]
set reset_n [get_bd_pins $reset/peripheral_aresetn]
connect_bd_net $clock [get_bd_pins $reset/slowest_sync_clk] [get_bd_pins $ps7/M_AXI_GP0_ACLK]
connect_bd_net [get_bd_pins $ps7/FCLK_RESET0_N] [get_bd_pins $reset/ext_reset_in]
foreach core [list $layer $desc] {
    connect_bd_net $clock [get_bd_pins $core/ap_clk]
    connect_bd_net $reset_n [get_bd_pins $core/ap_rst_n]
}
foreach hp {0 1 2 3} {connect_bd_net $clock [get_bd_pins $ps7/S_AXI_HP${hp}_ACLK]}
foreach ic $converters {
    foreach pin [get_bd_pins -quiet ${ic}/*ACLK] {connect_bd_net $clock $pin}
    foreach pin [get_bd_pins -quiet ${ic}/*ARESETN] {connect_bd_net $reset_n $pin}
}
assign_bd_address -offset 0x43C00000 -range 64K -target_address_space [get_bd_addr_spaces ps7/Data] [get_bd_addr_segs layer/s_axi_control/Reg]
assign_bd_address -offset 0x43C10000 -range 64K -target_address_space [get_bd_addr_spaces ps7/Data] [get_bd_addr_segs desc/s_axi_control/Reg]
assign_bd_address
validate_bd_design
save_bd_design
set bd [get_files [file join $output p.srcs sources_1 bd s s.bd]]
generate_target all $bd
add_files -norecurse [make_wrapper -files $bd -top]
set_property top s_wrapper [current_fileset]
update_compile_order -fileset sources_1
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
