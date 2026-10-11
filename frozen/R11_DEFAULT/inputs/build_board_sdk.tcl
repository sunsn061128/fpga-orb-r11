# Build only a fresh SDK workspace owned by the calling isolated next board job.
if {![info exists ::env(NEXT_BOARD_ROOT)]} { error "Use next/scripts/build_board.ps1" }
set root [file normalize $::env(NEXT_BOARD_ROOT)]
set workspace [file join $root sdk]
set inputs [file join $root inputs]
set hardware [file join $root vivado next_system_wrapper.hdf]
if {[file exists $workspace]} { error "Refusing to overwrite an existing SDK workspace" }
foreach name {board_benchmark.c board_pipeline.inc board_contract.h next_backend.c next_backend.h backend_tables.h front_reference.inc full_reference.inc} {
    if {![file isfile [file join $inputs $name]]} { error "Missing frozen PS input: $name" }
}
if {![file isfile $hardware]} { error "Missing routed next hardware export" }
file mkdir $workspace
setws $workspace
createhw -name next_hw -hwspec $hardware
createbsp -name next_bsp -hwproject next_hw -proc ps7_cortexa9_0 -os standalone
createapp -name next_benchmark -hwproject next_hw -proc ps7_cortexa9_0 \
    -os standalone -lang C -app {Empty Application} -bsp next_bsp
foreach name {board_benchmark.c board_pipeline.inc board_contract.h next_backend.c next_backend.h backend_tables.h front_reference.inc full_reference.inc} {
    file copy [file join $inputs $name] [file join $workspace next_benchmark src $name]
}
configapp -app next_benchmark compiler-optimization {Optimize more (-O2)}
projects -build -type bsp -name next_bsp
projects -build -type app -name next_benchmark
set executable [file join $workspace next_benchmark Debug next_benchmark.elf]
if {![file isfile $executable]} { error "Missing compiled next benchmark ELF" }
set stamp [open [file join $workspace .sdk_success] w]
puts $stamp NEXT_BOARD_SDK_PASS
close $stamp
puts "NEXT_BOARD_SDK_PASS elf=$executable"
