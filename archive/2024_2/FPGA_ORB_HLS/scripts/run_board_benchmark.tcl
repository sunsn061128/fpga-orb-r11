# Volatile ACZ7015 PS/PL benchmark with fixed-DDR result polling.
# This script never accesses QSPI, SD, eFUSE, or other non-volatile storage.

set script_dir [file dirname [file normalize [info script]]]
set project_root [file normalize [file join $script_dir ..]]
set bitstream [file join $project_root vivado_build orb_fpga.runs impl_1 orb_system_wrapper.bit]
set init_script [file join $project_root sdk_build orb_hw ps7_init.tcl]
set executable [file join $project_root sdk_build orb_selftest Debug orb_selftest.elf]
set input_file ""
set mailbox_address 0x01000000
set command_magic 0x4f524243
set result_magic 0x4f524252
set iterations 100

# Select matching bitstream and SDK artifacts when an isolated build is used.
if {[info exists ::env(ORB_BITSTREAM)] && $::env(ORB_BITSTREAM) ne ""} {
    set bitstream [file normalize $::env(ORB_BITSTREAM)]
}
if {[info exists ::env(ORB_SDK_WORKSPACE)] && $::env(ORB_SDK_WORKSPACE) ne ""} {
    set sdk_workspace [file normalize $::env(ORB_SDK_WORKSPACE)]
    set init_script [file join $sdk_workspace orb_hw ps7_init.tcl]
    set executable [file join $sdk_workspace orb_selftest Debug orb_selftest.elf]
}
if {[info exists ::env(ORB_INPUT_FILE)] && $::env(ORB_INPUT_FILE) ne ""} {
    set input_file [file normalize $::env(ORB_INPUT_FILE)]
}

if {[llength $argv] >= 1} {
    set iterations [lindex $argv 0]
}
if {![string is integer -strict $iterations] || $iterations < 1 || $iterations > 1000} {
    puts stderr "Benchmark iterations must be an integer from 1 through 1000"
    exit 2
}
foreach required_file [list $bitstream $init_script $executable] {
    if {![file exists $required_file]} {
        puts stderr "Missing required artifact: $required_file"
        exit 2
    }
}
if {$input_file ne "" && ![file exists $input_file]} {
    puts stderr "Missing benchmark input: $input_file"
    exit 2
}

# Read one unsigned 32-bit word from the fixed mailbox.
proc read_word {base index} {
    set value [lindex [mrd -value [expr {$base + 4 * $index}] 1] 0]
    return [expr {$value & 0xffffffff}]
}

# Format a value stored as units multiplied by 1000.
proc format_milli {value} {
    return [format "%d.%03d" [expr {$value / 1000}] [expr {$value % 1000}]]
}

# Format integer microseconds as milliseconds with three decimal places.
proc format_us_as_ms {value} {
    return [format "%d.%03d" [expr {$value / 1000}] [expr {$value % 1000}]]
}

if {[catch {
connect
set chain [jtag targets]
puts "=== VERIFIED_JTAG_CHAIN ==="
puts $chain
if {![string match "*xc7z015 (idcode 0373b093*" $chain]} {
    disconnect
    error "Expected xc7z015 JTAG device was not found"
}

puts "=== RESET_AND_INITIALIZE_PS ==="
targets -set -nocase -filter {name =~ "APU"}
rst -system
after 1000
targets -set -nocase -filter {name =~ "ARM Cortex-A9 MPCore #0"}
source $init_script
ps7_init

puts "=== PROGRAM_VOLATILE_PL ==="
fpga -file $bitstream
ps7_post_config
ps7_debug

puts "=== DOWNLOAD_BENCHMARK_ELF ==="
dow $executable
if {$input_file ne ""} {
    puts "=== DOWNLOAD_BENCHMARK_INPUT ==="
    dow -data $input_file 0x02000000
}

# Clear the stale terminal markers, then publish the command before continuing the CPU.
mwr [expr {$mailbox_address + 8}] 0
mwr [expr {$mailbox_address + 16}] 0
mwr $mailbox_address $command_magic
mwr [expr {$mailbox_address + 4}] $iterations

puts "=== RUN_BENCHMARK iterations=$iterations ==="
con
set timeout_ms [expr {$input_file eq "" ? 60000 + 300 * $iterations : 60000 + 30000 * $iterations}]
set deadline [expr {[clock milliseconds] + $timeout_ms}]
set last_completed -1
set terminal_status 0

while {[clock milliseconds] < $deadline} {
    after 250
    set magic [read_word $mailbox_address 2]
    if {$magic != $result_magic} {
        continue
    }
    set status [read_word $mailbox_address 4]
    set completed [read_word $mailbox_address 6]
    if {$completed != $last_completed && ($completed == $iterations || ($completed % 10) == 0)} {
        puts "progress=$completed/$iterations"
        set last_completed $completed
    }
    if {$status == 2 || $status == 3} {
        set terminal_status $status
        break
    }
}

catch {stop}
if {$terminal_status == 0} {
    disconnect
    error "Benchmark mailbox timed out after $timeout_ms ms"
}

set error_code [read_word $mailbox_address 5]
set schema_version [read_word $mailbox_address 3]
set completed [read_word $mailbox_address 6]
set failing_iteration [read_word $mailbox_address 7]
set candidates [read_word $mailbox_address 10]
set selected [read_word $mailbox_address 11]
set algorithm_version [read_word $mailbox_address 12]
set capabilities [read_word $mailbox_address 13]
set max_grid_top_k [read_word $mailbox_address 14]
set resize_digest_hi [read_word $mailbox_address 15]
set resize_digest_lo [read_word $mailbox_address 16]
set level_digest_hi [read_word $mailbox_address 17]
set level_digest_lo [read_word $mailbox_address 18]
set resize_config_us [read_word $mailbox_address 19]
set resize_wait_us [read_word $mailbox_address 20]
set resize_min_us [read_word $mailbox_address 21]
set resize_max_us [read_word $mailbox_address 22]
set resize_cycles [read_word $mailbox_address 23]
set resize_fps_milli [read_word $mailbox_address 24]
set level_config_us [read_word $mailbox_address 25]
set level_wait_us [read_word $mailbox_address 26]
set level_min_us [read_word $mailbox_address 27]
set level_max_us [read_word $mailbox_address 28]
set level_cycles [read_word $mailbox_address 29]
set level_fps_milli [read_word $mailbox_address 30]
set pipeline_us [read_word $mailbox_address 31]
set pipeline_fps_milli [read_word $mailbox_address 32]
set verification_us [read_word $mailbox_address 33]
set wall_us [read_word $mailbox_address 34]
set wall_fps_milli [read_word $mailbox_address 35]
set total_us_hi [read_word $mailbox_address 36]
set total_us_lo [read_word $mailbox_address 37]
set failure_detail [read_word $mailbox_address 38]
set total_us [expr {($total_us_hi << 32) | $total_us_lo}]

puts "=== BOARD_BENCHMARK_RESULT ==="
puts [format "status=%s error_code=%d completed=%d requested=%d failing_iteration=%d detail=%08x" \
    [expr {$terminal_status == 2 ? "PASS" : "FAIL"}] $error_code $completed $iterations \
    $failing_iteration $failure_detail]
puts [format "resize_digest=%08x%08x level_digest=%08x%08x" \
    $resize_digest_hi $resize_digest_lo $level_digest_hi $level_digest_lo]
puts [format "candidates=%d selected=%d version=%08x capabilities=%08x max_K=%d" \
    $candidates $selected $algorithm_version $capabilities $max_grid_top_k]
puts [format "resize_config_ms=%s resize_wait_ms=%s min_ms=%s max_ms=%s est_cycles=%d fps=%s" \
    [format_us_as_ms $resize_config_us] [format_us_as_ms $resize_wait_us] \
    [format_us_as_ms $resize_min_us] [format_us_as_ms $resize_max_us] \
    $resize_cycles [format_milli $resize_fps_milli]]
puts [format "level_config_ms=%s level_wait_ms=%s min_ms=%s max_ms=%s est_cycles=%d fps=%s" \
    [format_us_as_ms $level_config_us] [format_us_as_ms $level_wait_us] \
    [format_us_as_ms $level_min_us] [format_us_as_ms $level_max_us] \
    $level_cycles [format_milli $level_fps_milli]]
puts [format "pipeline_ms=%s pipeline_fps=%s verification_ms=%s wall_ms=%s wall_fps=%s total_ms=%s" \
    [format_us_as_ms $pipeline_us] [format_milli $pipeline_fps_milli] \
    [format_us_as_ms $verification_us] [format_us_as_ms $wall_us] \
    [format_milli $wall_fps_milli] [format_us_as_ms $total_us]]
if {$schema_version >= 2} {
    set detail_line "per_level_resize_ms="
    for {set level 1} {$level < 8} {incr level} {
        append detail_line [format " L%d:%s" $level \
            [format_us_as_ms [read_word $mailbox_address [expr {38 + $level}]]]]
    }
    puts $detail_line
    set detail_line "per_level_orb_ms="
    for {set level 0} {$level < 8} {incr level} {
        append detail_line [format " L%d:%s" $level \
            [format_us_as_ms [read_word $mailbox_address [expr {46 + $level}]]]]
    }
    puts $detail_line
}
if {$schema_version >= 3} {
    set detail_line "isolated_resize_ms="
    for {set level 1} {$level < 8} {incr level} {
        append detail_line [format " L%d:%s" $level \
            [format_us_as_ms [read_word $mailbox_address [expr {53 + $level}]]]]
    }
    puts $detail_line
    set detail_line "isolated_orb_ms="
    for {set level 0} {$level < 8} {incr level} {
        append detail_line [format " L%d:%s" $level \
            [format_us_as_ms [read_word $mailbox_address [expr {61 + $level}]]]]
    }
    puts $detail_line
    puts [format "isolated_resize_total_ms=%s isolated_orb_total_ms=%s isolated_serial_total_ms=%s" \
        [format_us_as_ms [read_word $mailbox_address 69]] \
        [format_us_as_ms [read_word $mailbox_address 70]] \
        [format_us_as_ms [read_word $mailbox_address 71]]]
}
if {$schema_version >= 4} {
    set stage_names [list through_nms through_grid_start through_gaussian_tail full_orb]
    for {set stage 0} {$stage < 4} {incr stage} {
        set detail_line [format "cumulative_%s_ms=" [lindex $stage_names $stage]]
        for {set level 0} {$level < 8} {incr level} {
            append detail_line [format " L%d:%s" $level \
                [format_us_as_ms [read_word $mailbox_address \
                    [expr {72 + $stage * 8 + $level}]]]]
        }
        puts $detail_line
    }
    set through_nms [read_word $mailbox_address 104]
    set through_grid_start [read_word $mailbox_address 105]
    set through_gaussian_tail [read_word $mailbox_address 106]
    set full_orb [read_word $mailbox_address 107]
    puts [format "derived_orb_stages_ms= fast_gaussian_nms:%s candidate_grid:%s grid_gaussian_tail:%s direction_descriptor:%s" \
        [format_us_as_ms $through_nms] \
        [format_us_as_ms [expr {$through_grid_start - $through_nms}]] \
        [format_us_as_ms [expr {$through_gaussian_tail - $through_grid_start}]] \
        [format_us_as_ms [expr {$full_orb - $through_gaussian_tail}]]]
}

disconnect
} failure]} {
    catch {disconnect}
    puts stderr "BOARD_BENCHMARK_ERROR: $failure"
    exit 2
}
if {$terminal_status != 2} {
    exit 2
}
exit 0
