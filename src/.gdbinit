target extended-remote /dev/ttyACM0
monitor swdp
file qspi_loader.elf
set mem inaccessible-by-default off
attach 1
