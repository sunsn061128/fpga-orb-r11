set project_root [file normalize [file join [file dirname [info script]] ..]]
set reference_project [file normalize [file join $project_root build reference_ps_project PL_GPIO.xpr]]
open_project $reference_project
open_bd_design [get_files system.bd]
set ps [get_bd_cells processing_system7_0]
set configuration {}
foreach property [lsort [list_property $ps]] {
    if {[string match "CONFIG.*" $property]} {
        lappend configuration $property [get_property $property $ps]
    }
}
set output_path [file join $project_root vivado ps_config.tcl]
set output [open $output_path w]
puts $output "# Board PS configuration copied from the confirmed ACZ7015 AXI GPIO example."
puts $output "set_property -dict [list $configuration] \$ps7"
close $output
close_project
exit 0
