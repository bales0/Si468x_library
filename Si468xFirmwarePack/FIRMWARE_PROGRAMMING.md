# Si468x Firmware / NVSPI Programming Guide

This firmware pack is **stand-alone**. None of the files in this directory requires `Si468x.h`.

The same images can therefore be used by:

- `Si468x.h`;
- DABShield-derived projects;
- a custom bare-metal driver;
- a production flash-programming tool;
- a Raspberry Pi/Linux application;
- any other project that implements the required Si468x boot protocol.

## Prepared images

The original user-provided binary images have been converted to C/C++ byte-array headers under `images/`. The pack itself intentionally contains no `.bin` payload files.

For direct NVSPI programming, `nvspi_current_full_2MiB_sparse.hex` contains:

```text
0x002000  ROM0.016 FullPatch
0x004000  ROM0.016 FullPatch (second prepared slot)
0x006000  FMHD 5.3.3
0x092000  DAB 6.0.9
0x11E000  AMHD 3.0.6
```

The MiniPatch is intentionally not programmed into this NVSPI layout; it is normally supplied by the host when the MiniPatch bootstrap architecture is selected.

## Method A: external SPI-NOR programmer

This is the simplest manufacturing/programming path.

1. Select a JEDEC SPI NOR flash of at least 2 MiB (16 Mbit) for the prepared full layout.
2. Erase the flash to `0xFF`.
3. Program `nvspi_current_full_2MiB_sparse.hex`.
4. Verify the programmed ranges.
5. Install/connect the flash to the Si468x NVSPI bus.

The Intel HEX is sparse; unspecified addresses are intended to remain erased.

## Method B: MCU directly programs the NVSPI flash

Some boards connect the MCU to the same SPI NOR as the Si468x.

The project may use the arrays in `images/*.h` as programming data and write the addresses from `Si468xFirmwareLayout.h` directly with its own SPI-NOR driver.

Typical process:

```text
obtain exclusive NVSPI flash bus ownership
erase required sectors
program FullPatch / selected firmware image
verify data or CRC
return flash bus to Si468x ownership
tri-state/release MCU flash-driving pins as required by board topology
```

Bus ownership and level-shifting are board-specific and deliberately not represented in this pack.

## Method C: update NVSPI through the Si468x pass-through API

AN649 provides NVSPI pass-through subcommands after the A10 bootloader is patched to ROM0.016.

Generic sequence:

```text
reset / POWER_UP
load required patch path
configure NVSPI flash properties if defaults do not match the device
sector erase
write blocks
verify blocks / CRC
```

Available AN649 mechanisms include:

```text
FLASH_GET_PROPERTY
FLASH_SET_PROP_LIST
FLASH_ERASE_CHIP
FLASH_ERASE_SECTOR
FLASH_WRITE_BLOCK
FLASH_WRITE_BLOCK_READBACK_VERIFY
FLASH_WRITE_BLOCK_PACKET_VERIFY
FLASH_WRITE_BLOCK_READBACK_AND_PACKET_VERIFY
FLASH_CHECK_CRC32
```

Block data is limited by the command definition; AN649 states a maximum write block payload of 4084 bytes.

When FullPatch lives in the same SPI NOR, prefer targeted sector erase rather than chip erase so the bootstrapping content is not destroyed accidentally.

## Method D: HOST_LOAD from MCU program flash

Include only the image required by the project:

```cpp
#include "images/rom0_full_016.h"
#include "images/fmhd_5_3_3.h"
```

or for a small host using the NVSPI bootstrap path:

```cpp
#include <avr/pgmspace.h>
#define SI468X_FW_STORAGE PROGMEM
#include "images/rom0_mini_003.h"
```

The byte arrays do not implement a reader. The project chooses how program-memory bytes are read and sent through HOST_LOAD.

This is intentional: an AVR Harvard-memory reader is different from an ESP32 memory-mapped flash reader or a Linux file reader.

## Boot choices

### Host supplies FullPatch and firmware

```text
POWER_UP
LOAD_INIT
HOST_LOAD FullPatch
wait 4 ms
LOAD_INIT
HOST_LOAD selected firmware
BOOT
```

### Host supplies FullPatch; firmware is in NVSPI

```text
POWER_UP
LOAD_INIT
HOST_LOAD FullPatch
wait 4 ms
LOAD_INIT
FLASH_LOAD selected firmware address
BOOT
```

### Small host supplies MiniPatch; FullPatch and firmware are in NVSPI

```text
POWER_UP
LOAD_INIT
HOST_LOAD MiniPatch
wait 4 ms
LOAD_INIT
FLASH_LOAD FullPatch address
LOAD_INIT
FLASH_LOAD selected firmware address
BOOT
```

AN649's loading flow also specifies the reset/power-up timing steps around this sequence. Board-level reset/power control must be implemented by the target project.

## Image selection is a project decision

The pack deliberately does not bind a firmware image to `Si468x.h` or automatically choose an image based only on the chip number.

A project should decide:

```text
actual Si468x part
required radio mode
licensed/available features
available NVSPI/host storage
firmware release notes and compatibility
selected patch/bootstrap architecture
```

Then load the appropriate image.

The supplied FMHD 5.3.3, DAB 6.0.9 and AMHD 3.0.6 images are newer than the firmware revisions tabulated in AN649 Rev. 1.9. This pack records their exact sizes/checksums but does not assert blanket compatibility with every family member.

## Fixed addresses

Use `Si468xFirmwareLayout.h` rather than scattering numeric literals through application code:

```cpp
#include "Si468xFirmwareLayout.h"

uint32_t address = si468x_firmware::NVSPI_DAB_IMAGE;
```

These constants are independent of any Si468x driver.

## Integrity information

`Si468xFirmwareCatalog.h` and `manifest.json` contain the exact size, CRC32 and SHA-256 recorded when this pack was generated.

Use an authoritative CRC supplied by the firmware release/tool for Si468x CRC-check commands if it differs from the generic file CRC recorded here.
