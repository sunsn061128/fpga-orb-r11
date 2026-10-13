# Vivado 2018.3 batch build for the isolated resize plus streaming-FAST design.

set script_dir [file dirname [file normalize [info script]]]
set project_root [file normalize [file join $script_dir ..]]
if {[info exists ::env(ORB_VIVADO_BUILD_DIR)] && $::env(ORB_VIVADO_BUILD_DIR) ne ""} {
    set build_dir [file normalize $::env(ORB_VIVADO_BUILD_DIR)]
} else {
    set build_dir [file join $project_root vivado_build_fast_stream]
}
set build_jobs 8
if {[info exists ::env(ORB_VIVADO_JOBS)] && [string is integer -strict $::env(ORB_VIVADO_JOBS)]} {
    set build_jobs $::env(ORB_VIVADO_JOBS)
}
set project_name orb_fast_fpga
set design_name orb_fast_system
set part_name xc7z015clg485-2

if {[file exists $build_dir]} {
    file delete -force $build_dir
}
file mkdir $build_dir

create_project $project_name $build_dir -part $part_name -force
set_property target_language Verilog [current_project]

set resize_ip [file join $project_root hls_build orb_resize solution1 impl ip]
set fast_ip [file join $project_root hls_build orb_fast_stream solution1 impl ip]
foreach ip_dir [list $resize_ip $fast_ip] {
    if {![file exists [file join $ip_dir component.xml]]} {
        error "Missing packaged HLS IP at $ip_dir"
    }
}
set_property ip_repo_paths [list $resize_ip $fast_ip] [current_project]
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
set fast [create_bd_cell -type ip -vlnv xilinx.com:hls:orb_fast_stream_hls:1.0 orb_fast_stream]

set control_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 control_ic]
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {2}] $control_ic

# Keep every high-bandwidth master on a separate PS HP port. The independent
# paths permit resize of the next pyramid level to overlap FAST on this level.
set resize_read_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 resize_read_ic]
set resize_write_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 resize_write_ic]
set fast_read_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 fast_read_ic]
set fast_write_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 fast_write_ic]
foreach cell [list $resize_read_ic $resize_write_ic $fast_read_ic $fast_write_ic] {
    set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] $cell
}

set reset [create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 reset]
set_property -dict [list CONFIG.C_EXT_RESET_HIGH {0}] $reset

make_bd_intf_pins_external [get_bd_intf_pins $ps7/DDR]
make_bd_intf_pins_external [get_bd_intf_pins $ps7/FIXED_IO]

connect_bd_intf_net [get_bd_intf_pins $ps7/M_AXI_GP0] [get_bd_intf_pins $control_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $control_ic/M00_AXI] [get_bd_intf_pins $resize/s_axi_control]
connect_bd_intf_net [get_bd_intf_pins $control_ic/M01_AXI] [get_bd_intf_pins $fast/s_axi_control]

connect_bd_intf_net [get_bd_intf_pins $resize/m_axi_gmem0] [get_bd_intf_pins $resize_read_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $resize_read_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP0]
connect_bd_intf_net [get_bd_intf_pins $resize/m_axi_gmem1] [get_bd_intf_pins $resize_write_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $resize_write_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP1]
connect_bd_intf_net [get_bd_intf_pins $fast/m_axi_gmem0] [get_bd_intf_pins $fast_read_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $fast_read_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP2]
connect_bd_intf_net [get_bd_intf_pins $fast/m_axi_gmem1] [get_bd_intf_pins $fast_write_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $fast_write_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP3]

set fabric_clock [get_bd_pins $ps7/FCLK_CLK0]
set fabric_reset_n [get_bd_pins $reset/peripheral_aresetn]
connect_bd_net $fabric_clock [get_bd_pins $reset/slowest_sync_clk]
connect_bd_net [get_bd_pins $ps7/FCLK_RESET0_N] [get_bd_pins $reset/ext_reset_in]
connect_bd_net $fabric_clock [get_bd_pins $ps7/M_AXI_GP0_ACLK]
foreach index {0 1 2 3} {
    connect_bd_net $fabric_clock [get_bd_pins $ps7/S_AXI_HP${index}_ACLK]
}
connect_bd_net $fabric_clock [get_bd_pins $resize/ap_clk] [get_bd_pins $fast/ap_clk]
connect_bd_net $fabric_reset_n [get_bd_pins $resize/ap_rst_n] [get_bd_pins $fast/ap_rst_n]

# @brief Connect every AXI Interconnect clock and reset to the 100 MHz domain.
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

foreach cell [list $control_ic $resize_read_ic $resize_write_ic $fast_read_ic $fast_write_ic] {
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

# Permit a fast block-design validation pass before the long implementation.
if {[info exists ::env(ORB_VIVADO_SETUP_ONLY)] && $::env(ORB_VIVADO_SETUP_ONLY) eq "1"} {
    set stamp_file [open [file join $build_dir .vivado_setup_success] w]
    puts $stamp_file "VIVADO_SETUP_SUCCESS"
    close $stamp_file
    puts "ORB_FAST_VIVADO_SETUP_PASS"
    exit 0
}

launch_runs synth_1 -jobs $build_jobs
wait_on_run synth_1
if {[get_property STATUS [get_runs synth_1]] ne "synth_design Complete!"} {
    error "Vivado synthesis did not complete successfully"
}
open_run synth_1
report_utilization -file [file join $build_dir post_synth_utilization.rpt]
report_timing_summary -file [file join $build_dir post_synth_timing.rpt]

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
puts "ORB_FAST_VIVADO_BUILD_PASS"
