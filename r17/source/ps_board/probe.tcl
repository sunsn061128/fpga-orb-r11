# Read-only chain and core inventory. No DDR access before confirmed PS init.
if {[catch {
    connect
    set chain [jtag targets];puts $chain
    if {[string first "Digilent JTAG-SMT2 C306ACCBABCD" $chain]<0 ||
        [regexp -all {xc7z015 \(idcode 0373b093} $chain]!=1} {error "Cable/device mismatch"}
    jtag targets -set -filter {name =~ "*C306ACCBABCD*"}
    jtag frequency 1000000
    targets -set -nocase -filter {name =~ "ARM Cortex-A9 MPCore #0"}
    puts [targets]
    disconnect
} failure]} {puts stderr "ORB4_READONLY_PROBE_FAILED $failure";exit 1}
puts "ORB4_READONLY_TARGET_MATCH"
exit 0
