set root [file normalize C:/Users/dmsun/Desktop/PLD/orb60_4ppc]
cd C:/h/orb4
open_project timer_probe_2018
set_top timer_probe
add_files [file join $root tests timer_probe.cpp]
open_solution solution1
set_part xc7z015clg485-2
create_clock -period 10
csynth_design
exit 0
