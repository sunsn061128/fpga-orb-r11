# Read-only JTAG-chain discovery for the ACZ7015 board.
# This script does not reset, program, download, or write device memory.

if {[catch {
    connect
    puts "=== JTAG_CHAIN ==="
    puts [jtag targets]
    disconnect
} failure]} {
    catch {disconnect}
    puts stderr "JTAG_CHECK_ERROR: $failure"
    exit 2
}
exit 0
