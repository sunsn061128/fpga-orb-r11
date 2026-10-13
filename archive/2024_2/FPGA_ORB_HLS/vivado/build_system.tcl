# Vivado 2018.3 batch build for the ACZ7015 ORB accelerator subsystem.

set script_dir [file dirname [file normalize [info script]]]
set project_root [file normalize [file join $script_dir ..]]
if {[info exists ::env(ORB_VIVADO_BUILD_DIR)] && $::env(ORB_VIVADO_BUILD_DIR) ne ""} {
    set build_dir [file normalize $::env(ORB_VIVADO_BUILD_DIR)]
} else {
    set build_dir [file join $project_root vivado_build]
}
set build_jobs 8
if {[info exists ::env(ORB_VIVADO_JOBS)] && [string is integer -strict $::env(ORB_VIVADO_JOBS)]} {
    set build_jobs $::env(ORB_VIVADO_JOBS)
}
set project_name orb_fpga
set design_name orb_system
set part_name xc7z015clg485-2

if {[file exists $build_dir]} {
    file delete -force $build_dir
}
file mkdir $build_dir

create_project $project_name $build_dir -part $part_name -force
set_property target_language Verilog [current_project]

set resize_ip [file join $project_root hls_build orb_resize solution1 impl ip]
set level_ip [file join $project_root hls_build orb_level solution1 impl ip]
foreach ip_dir [list $resize_ip $level_ip] {
    if {![file exists [file join $ip_dir component.xml]]} {
        error "Missing packaged HLS IP at $ip_dir"
    }
}
set_property ip_repo_paths [list $resize_ip $level_ip] [current_project]
update_ip_catalog

create_bd_design $design_name

set ps7 [create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 ps7]
source [file join $script_dir ps_config.tcl]
set_property -dict [list \
    CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100} \
    CONFIG.PCW_USE_S_AXI_HP0 {1} \
    CONFIG.PCW_USE_S_AXI_HP1 {1} \
    CONFIG.PCW_USE_S_AXI_HP2 {1} \
    CONFIG.PCW_USE_S_AXI_HP3 {1}] $ps7

set resize [create_bd_cell -type ip -vlnv xilinx.com:hls:orb_resize_hls:1.0 orb_resize]
set level [create_bd_cell -type ip -vlnv xilinx.com:hls:orb_level_hls:1.0 orb_level]
set control_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 control_ic]
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {2}] $control_ic

set mem0_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 mem0_ic]
set mem1_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 mem1_ic]
set mem2_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 mem2_ic]
set mem3_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 mem3_ic]
# The PS invokes resize and level sequentially, so their source and destination
# masters can safely share HP0 and HP1. Keeping each resize master on a native
# 64-bit path avoids the costly 64-to-512-to-64 conversion previously inserted
# when the ports shared HP3 with the wide feature and Gaussian interfaces.
foreach cell [list $mem0_ic $mem1_ic] {
    set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {1}] $cell
}
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] $mem2_ic
# HP3 remains shared only by the level core's feature and Gaussian masters.
set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {1}] $mem3_ic

set reset [create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 reset]
set_property -dict [list CONFIG.C_EXT_RESET_HIGH {0}] $reset

make_bd_intf_pins_external [get_bd_intf_pins $ps7/DDR]
make_bd_intf_pins_external [get_bd_intf_pins $ps7/FIXED_IO]

connect_bd_intf_net [get_bd_intf_pins $ps7/M_AXI_GP0] [get_bd_intf_pins $control_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $control_ic/M00_AXI] [get_bd_intf_pins $resize/s_axi_control]
connect_bd_intf_net [get_bd_intf_pins $control_ic/M01_AXI] [get_bd_intf_pins $level/s_axi_control]

connect_bd_intf_net [get_bd_intf_pins $level/m_axi_gmem0] [get_bd_intf_pins $mem0_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $resize/m_axi_gmem0] [get_bd_intf_pins $mem0_ic/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins $mem0_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP0]
connect_bd_intf_net [get_bd_intf_pins $level/m_axi_gmem1] [get_bd_intf_pins $mem1_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $resize/m_axi_gmem1] [get_bd_intf_pins $mem1_ic/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins $mem1_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP1]
connect_bd_intf_net [get_bd_intf_pins $level/m_axi_gmem2] [get_bd_intf_pins $mem2_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $mem2_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP2]
connect_bd_intf_net [get_bd_intf_pins $level/m_axi_gmem3] [get_bd_intf_pins $mem3_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $level/m_axi_gmem4] [get_bd_intf_pins $mem3_ic/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins $mem3_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP3]

set fabric_clock [get_bd_pins $ps7/FCLK_CLK0]
set fabric_reset_n [get_bd_pins $reset/peripheral_aresetn]
connect_bd_net $fabric_clock [get_bd_pins $reset/slowest_sync_clk]
connect_bd_net [get_bd_pins $ps7/FCLK_RESET0_N] [get_bd_pins $reset/ext_reset_in]
connect_bd_net $fabric_clock [get_bd_pins $ps7/M_AXI_GP0_ACLK]
foreach index {0 1 2 3} {
    connect_bd_net $fabric_clock [get_bd_pins $ps7/S_AXI_HP${index}_ACLK]
}
connect_bd_net $fabric_clock [get_bd_pins $resize/ap_clk] [get_bd_pins $level/ap_clk]
connect_bd_net $fabric_reset_n [get_bd_pins $resize/ap_rst_n] [get_bd_pins $level/ap_rst_n]

# @brief Connect every AXI Interconnect clock and reset input to the common domain.
# @param cell AXI Interconnect block-design cell object.
proc connect_interconnect_domain {cell} {
    set clock_source [get_bd_pins ps7/FCLK_CLK0]
    set reset_source [get_bd_pins reset/peripheral_aresetn]
    foreach pin [get_bd_pins -quiet ${cell}/*ACLK] {
        connect_bd_net $clock_source $pin
    }
    foreach pin [get_bd_pins -quiet ${cell}/*ARESETN] {
        connect_bd_net $reset_source $pin
    }
}

foreach cell [list $control_ic $mem0_ic $mem1_ic $mem2_ic $mem3_ic] {
    connect_interconnect_domain $cell
}

assign_bd_address
validate_bd_design
save_bd_design

set bd_file [get_files [file join $build_dir $project_name.srcs sources_1 bd $design_name ${design_name}.bd]]
generate_target all $bd_file
make_wrapper -files $bd_file -top
add_files -norecurse [file join $build_dir $project_name.srcs sources_1 bd $design_name hdl ${design_name}_wrapper.v]
set_property top ${design_name}_wrapper [current_fileset]
update_compile_order -fileset sources_1

launch_runs synth_1 -jobs $build_jobs
wait_on_run synth_1
if {[get_property STATUS [get_runs synth_1]] ne "synth_design Complete!"} {
    error "Vivado synthesis did not complete successfully"
}
open_run synth_1
report_utilization -file [file join $build_dir post_synth_utilization.rpt]
report_timing_summary -file [file join $build_dir post_synth_timing.rpt]

# Use post-route physical optimization because the accelerator is routing-limited
# near 100 MHz. This strategy changes placement and routing only; it does not
# modify the HLS kernel interfaces or arithmetic behavior.
set_property strategy Performance_ExplorePostRoutePhysOpt [get_runs impl_1]
launch_runs impl_1 -to_step write_bitstream -jobs $build_jobs
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {
    error "Vivado implementation did not reach 100 percent"
}
file copy -force \
    [file join $build_dir $project_name.runs impl_1 ${design_name}_wrapper.sysdef] \
    [file join $build_dir ${design_name}_wrapper.hdf]
open_run impl_1
report_utilization -file [file join $build_dir post_route_utilization.rpt]
report_timing_summary -file [file join $build_dir post_route_timing.rpt]
report_drc -file [file join $build_dir post_route_drc.rpt]

set success_stamp [file join $build_dir .vivado_success]
set stamp_file [open $success_stamp w]
puts $stamp_file "VIVADO_BUILD_SUCCESS"
close $stamp_file
puts "ORB_VIVADO_BUILD_PASS"
