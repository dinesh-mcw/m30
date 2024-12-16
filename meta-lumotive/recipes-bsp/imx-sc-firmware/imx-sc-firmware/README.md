## System Controller firmware (SCFW) required for the i.MX8QM platform

- `MT53D1024M32D4_scfw_tcm.bin`: Normal scfw with all features enabled
- `MT53D1024M32D4_DRC0_scfw_tcm.bin`: Contains modification to disable one of the two DRAM controllers on the NCB to reduce power consumption. This effectively halves the total system RAM.
