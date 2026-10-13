# Generate only compatible PS7 initialization; published 2024.2 PL bits are unchanged.
if {[llength $argv] != 2} {error "Expected board job and source package"}
lassign $argv job package
foreach frequency {100 125} {
    set folder [file join $job psgen_v2_${frequency}]
    create_project psgen $folder -part xc7z015clg485-2 -force
    create_bd_design s
    set ps7 [create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 ps7]
    source [file join $package orb60 vivado ps_config.tcl]
    set_property -dict [list CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ $frequency CONFIG.PCW_USE_S_AXI_HP0 {1} CONFIG.PCW_USE_S_AXI_HP1 {1} CONFIG.PCW_USE_S_AXI_HP2 {1} CONFIG.PCW_USE_S_AXI_HP3 {1}] $ps7
    set record [open [file join $job ps_properties_${frequency}.tsv] w]
    foreach key [lsort [list_property $ps7]] {
        if {[string match CONFIG.PCW_* $key]} {puts $record "$key\t[get_property $key $ps7]"}
    }
    close $record
    make_bd_intf_pins_external [get_bd_intf_pins $ps7/DDR]
    make_bd_intf_pins_external [get_bd_intf_pins $ps7/FIXED_IO]
    connect_bd_net [get_bd_pins $ps7/FCLK_CLK0] [get_bd_pins $ps7/M_AXI_GP0_ACLK] [get_bd_pins $ps7/S_AXI_HP0_ACLK] [get_bd_pins $ps7/S_AXI_HP1_ACLK] [get_bd_pins $ps7/S_AXI_HP2_ACLK] [get_bd_pins $ps7/S_AXI_HP3_ACLK]
    save_bd_design
    generate_target all [get_files [file join $folder psgen.srcs sources_1 bd s s.bd]]
    puts "PS_INIT_GENERATED frequency=$frequency"
    close_project
}
exit
