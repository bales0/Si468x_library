# Verification Status

Version: `0.9.5`

The following desktop/static checks were performed on the generated package:

- GCC and Clang C++11 compile/run of core smoke, parser, command and capability tests with `-Wall -Wextra -Werror -pedantic`;
- GCC and Clang C++11 syntax checks of the full firmware-header umbrella;
- exact enum comparison against all **53 command identifiers** and **202 property identifiers** extracted from AN649 Rev. 1.9;
- explicit audit of all **11 NVSPI pass-through subcommands** and **8 NVSPI pass-through properties** in AN649 Tables 11/12;
- platform-dependency scan of `Si468x.h` (no Arduino/SPI/Wire/STL/heap/filesystem/framework dependency);
- core/application boundary scan: no high-level `RdsDecoder`, `DlsFrame`, MOT/EPG decoder or UI/filesystem code remains in the core;
- DAB service-list streaming tests with odd input chunk boundaries, 32-bit component-entry preservation, trailing-data rejection and the AN649 `M < 15` component-count rule;
- command-construction tests for all four `GET_DIGITAL_SERVICE_LIST` SERTYPE values, DAB service start and non-blocking FM tune;
- argument-range tests for FM/AM/DAB tune parameters and DAB frequency-list count;
- ERR_CMD diagnostic tests, including recovery of the fifth-byte reason when the caller requested only the four-byte status word;
- complete AN649 command-error-code mapping through `CommandErrorReason`, including unknown-code fallback to the preserved raw byte;
- typed-reply bit-layout tests for FM RSQ, FM ACF, FM RDS, AM RSQ, DAB DIGRAD, HD DIGRAD, HD event status, HD BER and DSRV;
- `GET_PROPERTY` multi-property COUNT construction and reply-size validation;
- fixed HD command layout/range tests for station info, PSD selection, alert tone and enabled-port programming;
- reset/power callback ordering test confirming RSTB is asserted before an optional board power-enable transition.

Protocol audit fixes made in 0.9.5 include:

- corrected FM ACF convergence bits to RESP5 bits 6/5/4;
- exposed interrupt-hint fields omitted from typed FM/AM RSQ, FM RDS and DAB DIGRAD replies;
- decoded all documented `GET_FUNC_INFO` flag bits while retaining the raw flag byte;
- renamed the DAB component user-application pointer to `userApplicationRaw` because AN649 LENUA starts at UATYPE, not UADATA;
- rejected reserved/out-of-range tune, antenna-capacitance, DAB frequency-index and DAB frequency-count arguments instead of silently masking them;
- added typed AN649-level HD DIGRAD, HD event and HD BER replies while leaving external HD Radio content formats raw;
- exposed the documented `GET_PROPERTY` COUNT mechanism for reading consecutive properties;
- corrected `hardwareReset()` ordering so RSTB is asserted before an optional power-enable transition and remains asserted while supplies settle;
- hardened `READ_OFFSET`, service-list chunk sizing and malformed-list validation;
- corrected `WRITE_STORAGE` / `READ_STORAGE` applicability to DAB/DAB+ in AN649 Rev. 1.9;
- retained DSRV physical-error INTSRC bit 2 with an explicit note about AN649 Rev. 1.9's conflicting command/property tables versus section 7.7/Table 19.
- added an offline non-reserved property bit-field map to `PROTOCOL_REFERENCE.md` so composite property values can be built without an external AN649 lookup.

The firmware package itself is unchanged from the previously verified image pack; its generated arrays and sparse NVSPI image were already byte-for-byte/CRC checked against the supplied firmware binaries.

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

