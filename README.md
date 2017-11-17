README
======

qspi_loader
-----------
qspi_loader is a flashrom serprog over CDC-ACM implementation able to write an additional QSPI flash inside a [NumWorks](https://www.numworks.com) calculator.

Tested with :
- Winbond W25Q128JV
- Adesto AT25SF641 (patch for flashrom here : flashrom_patch/AT25SF641.patch, python script for enabling QSPI here : tools/enable_qspi_AT25SF641.py)

multiboot
---------
multiboot allows to program the internal flash of the STM32 with a firmware image stored inside the external flash.
The goal is to be able to switch between several versions of the firmware without a computer.
