# Read-only command help and adapter rate inventory for transport recovery.
connect
puts [jtag targets]
jtag targets -set -filter {name =~ "*C306ACCBABCD*"}
puts "JTAG_RATES=[jtag frequency -list]"
puts [help jtag]
puts [help targets]
puts [help rst]
puts [help {jtag sequence}]
puts [help configparams]
disconnect
exit 0
