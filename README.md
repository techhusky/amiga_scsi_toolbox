# toolbox - Utility for SCSI emulators

Utility to list shared files, CD images, change CD etc.

## Requirements

AmigaOS 2.x and 68000 with a SCSI emulator, such as ZuluSCSI or BlueSCSI.

## Build

### SAS/C

This is developed using SAS/C and NDK 3.2r4. Typing "smake" should be
enough to build the program.

### vbcc

The project can also be built with vbcc. A GNU Makefile is provided for
this purpose. Typing "make" should be enough to build the program.

The Makefile defaults to the `+aos68k` target with `-cpu=68000`. These
can be adjusted by overriding the `TARGET` and `CPU` variables:

    make TARGET=+aos68k CPU=-cpu=68020
