# TI AM6254 SBL for Bare-Metal Applications on PocketBeagle2 Board

## What is this ?

This is a modified TI SD-SBL for the PocketBeagle2 board that boots raw images (no headers required) on all available cores of the SoC (M4F, R5F, and the quad-core A53).

This SBL supports a `config.txt` file located at the root of the SD card. The file can contain some or all of the following parameters:

| Parameter     | Default Value (if not present in `config.txt`) | Description |
|-------------------|-----------------------------------------------|-------------|
| `m4f_start_address` | `0x05000000` | Specifies the start address of the M4F core. This address is hardcoded as `0x0000_0000` (mapped to `0x0500_0000` in the R5F's memory map) in the M4F's VTOR register. It must point to a valid interrupt vector table to correctly boot the core. If defined in `config.txt`, it **must** be `0x05000000`. |
| `r5f_start_address` | `0x70000000` | Specifies the start address for the R5F core. |
| `a53_start_address` | `0x80000000` | Specifies the common start address for all A53 cores (they all boot from the same address). |
| `m4f_clock_in_Hz`   | `400000000`  | Specifies the clock frequency for the M4F core (in Hz). |
| `r5f_clock_in_Hz`   | `400000000`  | Specifies the clock frequency for the R5F core (in Hz). |
| `a53_clock_in_Hz`   | `1400000000` | Specifies the clock frequency for the A53 cores (in Hz). |
| `m4f_binary_file`   | `app_m4f.bin` | Name of the M4F raw binary file to be loaded. |
| `r5f_binary_file`   | `app_r5f.bin` | Name of the R5F raw binary file to be loaded. |
| `a53_binary_file`   | `app_a53.bin` | Name of the A53 raw binary file to be loaded. |

The file `tiboot3.bin` is the prebuilt SBL binary used to boot raw images on the PocketBeagle2 board.

---

## Building the SBL

If you want to build your own SBL binary, you must install TI's MCU+ SDK and place the folder `pocketbeagle2_sbl_src` into the following location:

`${MCU_PLUS_SDK_PATH}\examples\drivers\boot\sbl_sd\am62x-sk`

then enter the build folder and run ***make***.

the folder *pocketbeagle2_sbl_src* contains the following :

| Content                  | Description                                                  |
| ------------------------ | ------------------------------------------------------------ |
| /build                   | Contains the Makefile, linker script, and other build-related files.   |
| board_ddrReginit.h       | Generated DDR configuration for the Kingston D2516AN9EXGXN (512MB). |
| DDR_pocketbeagle2.syscfg | DDR configuration file used with TI's SysConfig tool to generate the above header. |
| main.c                   | The SBL source code.                                          |
| pocketbeagle2.syscfg     | The main SBL configuration file, also used in SysConfig to generate source files during the build. |

