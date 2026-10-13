# Recover only the known authorized board's debug transport after failed download.
if {[catch {
    connect
    set chain [jtag targets];puts $chain
    if {[string first "Digilent JTAG-SMT2 C306ACCBABCD" $chain]<0 ||
        [regexp -all {xc7z015 \(idcode 0373b093} $chain]!=1} {error "Cable/device mismatch"}
    jtag targets -set -filter {name =~ "*C306ACCBABCD*"}
    if {[lsearch -exact [jtag frequency -list] 500000]<0} {error "Missing recovery rate"}
    jtag frequency 500000
    set sequence [jtag sequence]
    $sequence state RESET 10
    $sequence state IDLE 10
    $sequence run
    $sequence delete
    puts "ORB4_TAP_RESET_DONE"
    after 1000
    puts [targets]
    # Supported by the installed 2018.3 xsdb.tcl rst option table.
    targets -set -nocase -filter {name =~ "DAP*"}
    rst -dap
    puts "ORB4_DAP_RESET_DONE"
    after 1000
    puts [targets]
    targets -set -nocase -filter {name =~ "ARM Cortex-A9 MPCore #0"} -timeout 5
    puts "ORB4_TRANSPORT_RECOVERED"
    disconnect
} failure]} {catch {disconnect};puts stderr "ORB4_TRANSPORT_RECOVERY_FAILED $failure";exit 1}
exit 0
