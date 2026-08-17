# Verification Status

Version: `0.9.1`

The following checks were performed on the generated package:

- C++11 compile and execution of core smoke tests with `g++` using `-Wall -Wextra -Werror -pedantic`.
- C++11 compile and execution of the same core tests with `clang++` using `-Wall -Wextra -Werror -pedantic`.
- parser tests for FM RDS, DSRV, DLS prefix and chunked DAB service-list parsing;
- command-construction tests for all four `GET_DIGITAL_SERVICE_LIST` SERTYPE values, DAB service start and non-blocking FM tune start;
- syntax checks for the full firmware image umbrella header with both GCC and Clang;
- exact command enum value comparison against 53 command identifiers extracted from AN649 Rev. 1.9;
- exact property enum value comparison against 202 property identifiers extracted from AN649 Rev. 1.9;
- scan of `Si468x.h` for Arduino/SPI/Wire/STL/heap/platform-specific dependencies;
- byte-for-byte reconstruction of all five C/C++ firmware image headers against the supplied source firmware files;
- CRC32 verification of all five firmware images;
- byte-for-byte verification that the prepared sparse NVSPI Intel HEX contains both FullPatch copies and the FMHD/DAB/AMHD images at the documented addresses;
- verification that the prepared NVSPI image fits inside a 2 MiB address space.

## Not yet validated

This package has not yet been exercised on physical Si4682/83/84/85/88/89 hardware in this environment. In particular, production qualification should verify:

- board-specific SPI/I2C framing and maximum safe bus rate;
- POWER_UP crystal parameters and RF front-end properties for the actual board;
- every selected firmware/part combination;
- INTB electrical behavior and event acknowledgement;
- FM/AM tune and seek completion behavior;
- DAB service-list/component selection with real ensembles;
- sustained DSRV traffic and overflow recovery;
- NVSPI erase/write/verify operations on the selected JEDEC flash;
- reset/power-loss recovery during firmware updates.

The supplied application firmware revisions are newer than the images tabulated in AN649 Rev. 1.9. Their exact target compatibility must be checked against the corresponding firmware release information when available.
