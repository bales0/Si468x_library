# Si468x Firmware Pack (stand-alone)

This directory is intentionally **independent** of `Si468x.h`. It can be copied into any project or used without the universal driver. It contains no original `.bin` files; the supplied firmware images are converted to C/C++ byte-array headers and to a sparse Intel HEX image for NVSPI programming.

## Included images

| Image | Size | CRC32 | SHA-256 |
|---|---:|---:|---|
| rom00_patch_mini.003.bin (ROM0.MINI.003) | 940 | `0x8944B5AE` | `84ae1a5b82f984b671e785f4ccb281e414998ce02c9fc897ab9484ef500ee0eb` |
| rom00_patch.016.bin (ROM0.016) | 5796 | `0xA93227B5` | `15c5196132b921755ed5c3b00b875494c73e39a2013771a594957cebe04221f2` |
| fmhd_radio_5_3_3.bin (5.3.3) | 530856 | `0xA174D4AB` | `807b93a5693f8ac63698ecbac51f72a3f17aaa930e74f08fdaedf226cef22be9` |
| dab_radio_6_0_9.bin (6.0.9) | 499516 | `0x4BB3271D` | `48692a0c3c2430c5c49cfb35a3d06c1853246b752293715f38c1027281c98fa6` |
| amhd_radio_3_0_6.bin (3.0.6) | 533164 | `0x2168E5C7` | `9906d7cc5556f0eddb74faa1abb039fb07056af7e1d5f70ad791925addfdf2a1` |

These firmware revisions are newer than the firmware table in AN649 Rev.1.9. The pack therefore does **not** assert that every image is valid for every Si468x part. Select the FMHD / DAB / AMHD image according to the actual device, licensing/features and the release notes available for that image. The bootloader patch strategy is likewise chosen by the host application.

## NVSPI layout used by the prepared Intel HEX

The current images exceed the old 512 KiB image spacing shown in AN649 Rev.1.9. The prepared layout therefore follows the later SDK-style 560 KiB spacing found with this firmware family:

```text
0x000000  customer-specific / manifest area (8 KiB, left unprogrammed)
0x002000  ROM0.016 FullPatch primary (8 KiB slot)
0x004000  ROM0.016 FullPatch backup/second slot (8 KiB slot)
0x006000  FMHD 5.3.3              (560 KiB slot)
0x092000  DAB 6.0.9               (560 KiB slot)
0x11E000  AMHD 3.0.6              (560 KiB slot)
```

A 2 MiB (16 Mbit) JEDEC SPI NOR is sufficient for this layout. `nvspi_current_full_2MiB_sparse.hex` is sparse: erase the target flash to `0xFF` before programming or make sure the programmer treats unspecified areas as erased.

`ROM0.MINI.003` is normally kept by the host MCU because its purpose is to bootstrap the bootloader far enough to load the FullPatch from NVSPI. It is therefore not placed in the prepared NVSPI HEX.

## Using an image from MCU program flash

Include only the image required by the project:

```cpp
#include "images/rom0_full_016.h"
#include "images/dab_6_0_9.h"
```

The arrays are ordinary `const uint8_t[]`. If the compiler needs an address-space attribute, define `SI468X_FW_STORAGE` before including the image header. Example for an AVR toolchain that supports `PROGMEM`:

```cpp
#include <avr/pgmspace.h>
#define SI468X_FW_STORAGE PROGMEM
#include "images/rom0_mini_003.h"
```

Reading from program memory remains the responsibility of that platform/project. On ESP32 and most unified-address-space MCUs the default empty storage macro is normally sufficient. Large application images obviously cannot fit in small AVRs; use the MiniPatch + NVSPI method there.

## Boot strategies

The pack does not impose a boot strategy. Typical AN649 flows are:

```text
Host image:
POWER_UP -> LOAD_INIT -> HOST_LOAD FullPatch -> wait 4 ms
         -> LOAD_INIT -> HOST_LOAD firmware -> BOOT

NVSPI, FullPatch supplied by host:
POWER_UP -> LOAD_INIT -> HOST_LOAD FullPatch -> wait 4 ms
         -> LOAD_INIT -> FLASH_LOAD firmware address -> BOOT

NVSPI, MiniPatch supplied by small host:
POWER_UP -> LOAD_INIT -> HOST_LOAD MiniPatch -> wait 4 ms
         -> LOAD_INIT -> FLASH_LOAD FullPatch address
         -> LOAD_INIT -> FLASH_LOAD firmware address -> BOOT
```

AN649 also specifies a 3 ms wait after device reset and 20 us after `POWER_UP` in its loading flowchart. The universal driver exposes these boot mechanisms generically; this firmware pack does not depend on that driver.

## Updating NVSPI through Si468x

After the A10 bootloader is patched to ROM0.016, AN649 exposes NVSPI pass-through commands for flash property setup, sector/chip erase, block writes, CRC checking and readback/packet verification. The maximum flash write payload is 4084 bytes. Prefer sector erase when a FullPatch resides in the same flash so that an update cannot accidentally erase the bootstrap patch.

## Files

- `Si468xFirmwareLayout.h` -- address constants only.
- `Si468xFirmwareCatalog.h` -- metadata/checksums only; no byte arrays.
- `Si468xFirmwareImages.h` -- optional umbrella include for all byte arrays.
- `images/*.h` -- one independent byte-array header per image.
- `flash_map.csv` / `manifest.json` -- machine-readable layout and metadata.
- `nvspi_current_full_2MiB_sparse.hex` -- prepared external-flash programming image.
