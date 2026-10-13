# Read the PS FCLK0 divider and compare it with this paired generated init.
proc orb4_check_fclk0 {root} {
    set f [open [file join $root inputs ps7_init.tcl] r];set init [read $f];close $f
    set writes [regexp -all -inline -nocase {mask_write\s+0xF8000170\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)} $init]
    if {[llength $writes]<3 || [llength $writes]%3} {error "Missing generated FCLK0 configuration"}
    set mask [lindex $writes 1];set expected [lindex $writes 2]
    foreach {line m v} $writes {
        if {$m!=$mask || $v!=$expected} {error "Ambiguous generated FCLK0 configuration"}
    }
    set raw [mrd -value 0xF8000170]
    if {[llength $raw]!=1} {error "Missing PS FCLK0 readback"}
    set actual [expr {[lindex $raw 0]&0xffffffff}]
    if {($actual&$mask)!=($expected&$mask)} {error "PS FCLK0 divider differs from paired hardware"}
    puts [format "ORB4_FCLK0_READBACK actual=0x%08X mask=0x%08X expected=0x%08X" $actual $mask $expected]
    foreach {name address selected_mask} {ARM_PLL 0xF8000100 0x0007F000 DDR_PLL 0xF8000104 0x0007F000 IO_PLL 0xF8000108 0x0007F000 CPU_DIV 0xF8000120 0x1F003F30} {
        set writes [regexp -all -inline -nocase "mask_write\\s+$address\\s+(0x\[0-9a-f\]+)\\s+(0x\[0-9a-f\]+)" $init]
        set values {}
        foreach {line m v} $writes {if {$m==$selected_mask} {lappend values $v}}
        if {![llength $values]} {error "Missing generated clock register $name"}
        set expected [lindex $values 0]
        foreach value $values {if {$value!=$expected} {error "Ambiguous generated clock register $name"}}
        set raw [mrd -value $address]
        if {[llength $raw]!=1} {error "Missing PS clock readback $name"}
        set actual [expr {[lindex $raw 0]&0xffffffff}]
        if {($actual&$selected_mask)!=($expected&$selected_mask)} {error "PS clock register differs from paired hardware: $name"}
        puts [format "ORB4_PS_CLOCK_READBACK name=%s actual=0x%08X mask=0x%08X expected=0x%08X" $name $actual $selected_mask $expected]
    }
}
