# Volatile ACZ7015 resize plus streaming-FAST benchmark.
# This script does not access QSPI, SD, eFUSE, or any non-volatile storage.

set script_dir [file dirname [file normalize [info script]]]
set project_root [file normalize [file join $script_dir ..]]
set bitstream [file join $project_root vivado_build_fast_stream orb_fast_fpga.runs impl_1 orb_fast_system_wrapper.bit]
set sdk_workspace [file join $project_root sdk_build_fast_stream]
set init_script [file join $sdk_workspace orb_hw ps7_init.tcl]
set executable [file join $sdk_workspace orb_selftest Debug orb_selftest.elf]
set input_file ""
set mailbox_address 0x01000000
set command_magic 0x4f524643
set result_magic 0x4f524652
set iterations 10

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
foreach required_file [list $bitstream $init_script $executable $input_file] {
    if {$required_file eq "" || ![file exists $required_file]} {
        puts stderr "Missing required artifact: $required_file"
        exit 2
    }
}

# @brief Read one unsigned 32-bit word from the fixed DDR mailbox.
# @param base Mailbox base address.
# @param index Zero-based word index.
# @return Unsigned 32-bit value.
proc read_word {base index} {
    set value [lindex [mrd -value [expr {$base + 4 * $index}] 1] 0]
    return [expr {$value & 0xffffffff}]
}

# @brief Format a value stored in units multiplied by 1000.
# @param value Fixed-point value to format.
# @return Decimal string with three fractional digits.
proc format_milli {value} {
    return [format "%d.%03d" [expr {$value / 1000}] [expr {$value % 1000}]]
}

# @brief Format integer microseconds as milliseconds.
# @param value Time in microseconds.
# @return Millisecond string with three fractional digits.
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

puts "=== PROGRAM_VOLATILE_FAST_PL ==="
fpga -file $bitstream
ps7_post_config
ps7_debug

puts "=== DOWNLOAD_FAST_BENCHMARK ==="
dow $executable
dow -data $input_file 0x02000000

mwr [expr {$mailbox_address + 8}] 0
mwr [expr {$mailbox_address + 16}] 0
mwr $mailbox_address $command_magic
mwr [expr {$mailbox_address + 4}] $iterations

puts "=== RUN_FAST_BENCHMARK iterations=$iterations ==="
con
set timeout_ms [expr {60000 + 5000 * $iterations}]
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
    error "FAST benchmark mailbox timed out after $timeout_ms ms"
}

set error_code [read_word $mailbox_address 5]
set completed [read_word $mailbox_address 6]
set failing_iteration [read_word $mailbox_address 7]
set candidates [read_word $mailbox_address 10]
set fast_digest_hi [read_word $mailbox_address 11]
set fast_digest_lo [read_word $mailbox_address 12]
set image_digest_hi [read_word $mailbox_address 13]
set image_digest_lo [read_word $mailbox_address 14]
set fast_config_us [read_word $mailbox_address 15]
set fast_wait_us [read_word $mailbox_address 16]
set fast_min_us [read_word $mailbox_address 17]
set fast_max_us [read_word $mailbox_address 18]
set fast_fps_milli [read_word $mailbox_address 19]
set resize_config_us [read_word $mailbox_address 20]
set resize_wait_us [read_word $mailbox_address 21]
set resize_min_us [read_word $mailbox_address 22]
set resize_max_us [read_word $mailbox_address 23]
set resize_fps_milli [read_word $mailbox_address 24]
set pipeline_us [read_word $mailbox_address 25]
set pipeline_fps_milli [read_word $mailbox_address 26]
set verification_us [read_word $mailbox_address 27]
set wall_us [read_word $mailbox_address 28]
set wall_fps_milli [read_word $mailbox_address 29]
set total_us_hi [read_word $mailbox_address 30]
set total_us_lo [read_word $mailbox_address 31]
set failure_detail [read_word $mailbox_address 32]
set total_us [expr {($total_us_hi << 32) | $total_us_lo}]

puts "=== FAST_BOARD_BENCHMARK_RESULT ==="
puts [format "status=%s error_code=%d completed=%d requested=%d failing_iteration=%d detail=%08x" \
    [expr {$terminal_status == 2 ? "PASS" : "FAIL"}] $error_code $completed $iterations \
    $failing_iteration $failure_detail]
puts [format "candidates=%d fast_digest=%08x%08x image_digest=%08x%08x" \
    $candidates $fast_digest_hi $fast_digest_lo $image_digest_hi $image_digest_lo]
puts [format "fast_config_ms=%s fast_wait_ms=%s min_ms=%s max_ms=%s fps=%s" \
    [format_us_as_ms $fast_config_us] [format_us_as_ms $fast_wait_us] \
    [format_us_as_ms $fast_min_us] [format_us_as_ms $fast_max_us] \
    [format_milli $fast_fps_milli]]
puts [format "resize_config_ms=%s resize_wait_ms=%s min_ms=%s max_ms=%s fps=%s" \
    [format_us_as_ms $resize_config_us] [format_us_as_ms $resize_wait_us] \
    [format_us_as_ms $resize_min_us] [format_us_as_ms $resize_max_us] \
    [format_milli $resize_fps_milli]]
puts [format "pipeline_ms=%s pipeline_fps=%s verification_ms=%s wall_ms=%s wall_fps=%s total_ms=%s" \
    [format_us_as_ms $pipeline_us] [format_milli $pipeline_fps_milli] \
    [format_us_as_ms $verification_us] [format_us_as_ms $wall_us] \
    [format_milli $wall_fps_milli] [format_us_as_ms $total_us]]

set detail_line "per_level_candidates="
for {set level 0} {$level < 8} {incr level} {
    append detail_line [format " L%d:%d" $level [read_word $mailbox_address [expr {33 + $level}]]]
}
puts $detail_line
set detail_line "per_level_fast_digest="
for {set level 0} {$level < 8} {incr level} {
    append detail_line [format " L%d:%08x%08x" $level \
        [read_word $mailbox_address [expr {41 + 2 * $level}]] \
        [read_word $mailbox_address [expr {42 + 2 * $level}]]]
}
puts $detail_line
set detail_line "per_level_fast_ms="
for {set level 0} {$level < 8} {incr level} {
    append detail_line [format " L%d:%s" $level \
        [format_us_as_ms [read_word $mailbox_address [expr {57 + $level}]]]]
}
puts $detail_line
set detail_line "per_level_resize_ms="
for {set level 1} {$level < 8} {incr level} {
    append detail_line [format " L%d:%s" $level \
        [format_us_as_ms [read_word $mailbox_address [expr {65 + $level}]]]]
}
puts $detail_line

disconnect
} failure]} {
    catch {disconnect}
    puts stderr "FAST_BOARD_BENCHMARK_ERROR: $failure"
    exit 2
}
if {$terminal_status != 2} {
    exit 2
}
exit 0
