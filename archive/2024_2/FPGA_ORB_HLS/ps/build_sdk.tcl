# Xilinx SDK 2018.3 batch build for the standalone PS/PL self-test.

set script_dir [file dirname [file normalize [info script]]]
set project_root [file normalize [file join $script_dir ..]]
set workspace [file join $project_root sdk_build]
set hardware_spec [file join $project_root vivado_build orb_system_wrapper.hdf]
set application_source [file join $script_dir orb_baremetal_selftest.c]

# Allow isolated SDK builds to consume an explicitly selected implemented design.
if {[info exists ::env(ORB_SDK_WORKSPACE)] && $::env(ORB_SDK_WORKSPACE) ne ""} {
    set workspace [file normalize $::env(ORB_SDK_WORKSPACE)]
}
if {[info exists ::env(ORB_HARDWARE_SPEC)] && $::env(ORB_HARDWARE_SPEC) ne ""} {
    set hardware_spec [file normalize $::env(ORB_HARDWARE_SPEC)]
}
if {[info exists ::env(ORB_APP_SOURCE)] && $::env(ORB_APP_SOURCE) ne ""} {
    set application_source [file normalize $::env(ORB_APP_SOURCE)]
}

if {![file exists $hardware_spec]} {
    error "Missing SDK hardware specification: $hardware_spec"
}
if {![file exists $application_source]} {
    error "Missing SDK application source: $application_source"
}
if {[file exists $workspace]} {
    file delete -force $workspace
}
file mkdir $workspace
setws $workspace

createhw -name orb_hw -hwspec $hardware_spec
createbsp -name orb_bsp -hwproject orb_hw -proc ps7_cortexa9_0 -os standalone
createapp -name orb_selftest -hwproject orb_hw -proc ps7_cortexa9_0 \
    -os standalone -lang C -app {Empty Application} -bsp orb_bsp

set app_source [file join $workspace orb_selftest src orb_baremetal_selftest.c]
file copy -force $application_source $app_source
projects -build -type bsp -name orb_bsp
projects -build -type app -name orb_selftest

set elf_file [file join $workspace orb_selftest Debug orb_selftest.elf]
if {![file exists $elf_file]} {
    error "SDK build did not produce $elf_file"
}
set stamp_file [open [file join $workspace .sdk_success] w]
puts $stamp_file "SDK_BUILD_SUCCESS"
close $stamp_file
puts "ORB_SDK_BUILD_PASS"
