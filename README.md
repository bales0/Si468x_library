# Si468x Universal Driver

`Si468x.h` is a **single-header, platform-neutral C++11 driver** for the Si468x digital-radio family. It is designed as a reusable device driver rather than as an Arduino radio application.

The driver is intentionally independent of:

- Arduino, PlatformIO, ESP-IDF, AVR-LibC, STM32 HAL, Pico SDK, Linux or any other platform framework;
- a specific MCU, CPU word size, SPI/I2C peripheral or GPIO implementation;
- any LCD/OLED/UI library;
- filesystems, SD cards, EEPROM libraries or station databases;
- dynamic allocation, STL, exceptions, RTTI and `String`-style classes;
- any particular Si468x firmware package.

All public code and comments are in English. The command/property definitions follow **AN649, Si468x Programming Guide Rev. 1.9**. Later firmware may add behavior that AN649 Rev. 1.9 does not document; numeric raw-command and raw-property access is therefore always available.

## Supported family

The family model includes Si4682, Si4683, Si4684, Si4685, Si4688 and Si4689. The driver itself contains every AN649 Rev. 1.9 command/property definition regardless of the selected part. Unsupported commands are rejected by the device/firmware, not removed at compile time.

`capabilitiesForPart()` provides a convenient product-family capability summary for FM, RDS, AM, HD-FM, HD-AM and DAB/DAB+, but it is not a substitute for checking the actual loaded image and firmware revision with `GET_PART_INFO`, `GET_SYS_STATE` and `GET_FUNC_INFO`.

## One file

Application code needs only:

```cpp
#include "Si468x.h"
```

There is no required `.cpp` file and there are no platform conditionals such as `#ifdef ESP32` or `#ifdef ARDUINO` in the driver.

## Host adapter contract

The application supplies a `si468x::HostInterface`:

```cpp
si468x::HostInterface host;
host.context      = myContext;
host.writeCommand = myWriteCommand;
host.readReply    = myReadReply;
host.timeUs       = myMonotonicMicrosecondTimer; // required by blocking/timeouts
host.idle         = myYieldHook;                  // optional
host.setReset     = myResetHook;                  // optional
host.setPower     = myPowerHook;                  // optional
```

The important abstraction boundary is:

```text
Si468x.h
    |
    | logical command: CMD + ARG bytes
    | logical reply:   STATUS0, STATUS1, STATUS2, STATUS3, RESP...
    v
HostInterface adapter
    |
    +-- SPI implementation
    +-- I2C implementation
    +-- desktop test transport
    +-- any other supported physical host transport
```

### `writeCommand`

Receives the command byte separately and the exact AN649 argument bytes. It must perform one complete physical command transaction.

### `readReply`

Must return logical Si468x reply bytes beginning with `STATUS0` at `destination[0]`.

This is important for SPI implementations: if the physical Si468x SPI transaction returns a leading dummy/framing byte, the platform adapter removes it. An I2C adapter can expose its reply directly. This keeps the radio driver independent of physical-bus framing.

## Memory model

The driver does **not** allocate a transfer buffer internally. The application selects the workspace size:

```cpp
uint8_t work[64];
si468x::Si468x radio(host);
radio.setWorkspace(work, sizeof(work));
```

or, on a larger target:

```cpp
uint8_t work[1024];
radio.setWorkspace(work, sizeof(work));
```

Typical requirements:

| Operation | Workspace requirement |
|---|---:|
| Ordinary fixed-size commands | none or only local small command arrays |
| DAB frequency list | `3 + 4 * frequency_count` bytes |
| `HOST_LOAD` | `3 + desired_chunk_size`; image data can be streamed |
| NVSPI block write | `15 + block_size` bytes |
| Full 4084-byte NVSPI block write | 4099-byte workspace |
| DAB service-list parser | 24 bytes of parser state, independent of complete list size |

Large replies do not require large RAM. `READ_OFFSET` allows a host to retrieve the preserved response in chunks. This is particularly useful on 8-bit MCUs.

## Cooperative/event-driven operation

The preferred mode is:

```cpp
void si468xInterruptHandler()
{
    radio.notifyInterrupt();   // flag only; no bus traffic in ISR
}

for (;;) {
    radio.service();
    // application/UI/storage work
}
```

`service()` handles command CTS completion and optional status polling. With INTB connected, `notifyInterrupt()` causes the status to be serviced promptly. Without a timer, the non-blocking engine can still poll once per `service()` call; blocking convenience functions and enforced timeouts require `HostInterface::timeUs`.

The status callback is intended to **record/schedule work only**. Do not issue a nested Si468x command from inside the callback while `service()` is processing the current command.

### Blocking convenience API

The same state machine is also used by blocking helpers:

```cpp
radio.fmTune(10170);       // 101.70 MHz
radio.dabTune(12);
```

These helpers wait for **CTS**, meaning the command has completed its command-processing phase. Tune/seek RF acquisition completion is a separate **STC** event. Use INTB/status handling or `waitForStc()` when the application must wait for final tune/seek completion.

Non-blocking typed starts are also provided:

```cpp
radio.startFmTune(10170);
while (radio.busy()) radio.service();
// later react to STCINT
```

Equivalent non-blocking helpers exist for FM seek, AM tune/seek and DAB tune.

## Interrupt masks

The library exposes the top-level INT_CTL bits as `INTERRUPT_*` constants, for example:

```cpp
radio.setInterruptEnable(
    si468x::INTERRUPT_STC |
    si468x::INTERRUPT_DSRV |
    si468x::INTERRUPT_DEVICE_EVENT
);
```

Not every interrupt source is meaningful in every image. FM/FMHD uses RSQ/RDS/ACF sources; DAB uses STC, DSRV and device-event sources among others.

For DSRV, configure both the top-level DSRV interrupt enable and the desired `DIGITAL_SERVICE_INT_SOURCE` bits.

## Boot and firmware loading

No firmware bytes are embedded in `Si468x.h`.

The driver implements the mechanisms needed by the AN649 architectures:

### HostLoad: patch and firmware from host storage

```text
POWER_UP
LOAD_INIT
HOST_LOAD FullPatch
wait 4 ms
LOAD_INIT
HOST_LOAD mode firmware
BOOT
```

Helper:

```cpp
radio.bootHostImage(...);
```

The supplied `ImageReader` may read bytes from MCU program flash, SD, a file, an external memory, or any other application-defined source.

### NVSPI with FullPatch supplied by host

```text
POWER_UP
LOAD_INIT
HOST_LOAD FullPatch
wait 4 ms
LOAD_INIT
FLASH_LOAD mode firmware
BOOT
```

Helper:

```cpp
radio.bootNvspiWithHostFullPatch(...);
```

### NVSPI with MiniPatch supplied by small host

```text
POWER_UP
LOAD_INIT
HOST_LOAD MiniPatch
wait 4 ms
LOAD_INIT
FLASH_LOAD FullPatch
LOAD_INIT
FLASH_LOAD mode firmware
BOOT
```

Helper:

```cpp
radio.bootNvspiWithMiniPatch(...);
```

The physical reset/power sequencing remains a board responsibility. Optional `setResetAsserted()` and `setPowerEnabled()` wrappers use the board callbacks without assuming GPIO polarity.

## NVSPI flash management

The driver contains the AN649 pass-through mechanisms, including:

- flash load and CRC-checked flash load;
- CRC32 checking;
- flash property get/set;
- chip erase and sector erase;
- block write;
- readback verify;
- packet verify;
- combined readback + packet verify.

The A10 bootloader must be patched appropriately before the NVSPI pass-through commands are used.

The included `crc32Ieee()` helper implements conventional reflected CRC-32/ISO-HDLC. AN649 requires CRC32 values for several flash commands but does not define the polynomial in the Programming Guide; when a firmware release supplies an authoritative CRC, use that value.

## FM / FMHD

Typed helpers include:

- tune and seek;
- RSQ status;
- ACF status;
- RDS status/FIFO groups;
- RDS block counters;
- audio volume/mute and full property access;
- all FM/FMHD properties through the complete `Property` enum.

`FmRsqStatus` exposes frequency, frequency offset, RSSI, SNR, multipath, validity, antenna capacitance and HD-level fields where returned by the image.

`FmAcfStatus` exposes soft-mute/high-cut/stereo-blend state and convergence information.

## RDS/RBDS

`RdsDecoder` is a lightweight no-allocation helper on top of `FM_RDS_STATUS`. It currently decodes the fields that can be implemented directly from the documented RDS group structure used here:

- PI;
- PTY;
- TP/TA;
- Program Service (PS);
- RadioText 2A/2B;
- Clock Time 4A;
- basic ECC handling.

The raw RDS A/B/C/D blocks and per-block BLE values remain available to applications that need additional RDS/RBDS group decoders.

## AM / AMHD

Typed tune, seek, RSQ and ACF methods are provided. Every AM/AMHD property documented by AN649 is also exposed through the `Property` enum and generic property API.

HD-specific command replies are kept raw where their application payload is defined by external HD Radio/iBiquity specifications rather than AN649.

## DAB / DAB+

Typed or raw access is available for the AN649 DAB command set, including:

- tuning and DIGRAD status;
- event status;
- ensemble information;
- service linking information;
- frequency-list set/get;
- component information;
- time;
- audio information;
- sub-channel information;
- frequency information;
- service information;
- DAB BER test information;
- digital-service start/stop/data.

### DAB service IDs and component IDs

`DabServiceListParser` streams a DAB/DMB `GET_DIGITAL_SERVICE_LIST` response without holding the entire list in RAM.

The parser deliberately exposes:

```cpp
component.componentId        // exact 32-bit 4-byte component entry
component.rawComponentField  // lower 16-bit component field
component.componentReference // decoded SubChId/FIDCId/SCId
```

Use the exact `componentId` value when feeding a component back to the Si468x service/component commands.

For DAB/DAB+, AN649 states that the `SERTYPE` argument of START/STOP_DIGITAL_SERVICE is not used and should be zero. Prefer:

```cpp
radio.startDabService(serviceId, componentId);
radio.stopDabService(serviceId, componentId);
```

`getDigitalServiceList()` preserves all two SERTYPE bits because HD modes use values 0 through 3; DAB uses value 0.

## DSRV and data services

`DsrvHeader` exposes:

- interrupt source;
- queued-buffer count;
- service state;
- DAB data source / DSCTy;
- service ID;
- component ID/port;
- byte count;
- segment number;
- number of segments.

`getDigitalServiceDataHeader()` supports the AN649 header-first workflow. The full preserved response may then be re-read or retrieved with `READ_OFFSET` in small chunks.

The driver does not automatically acknowledge or consume all DSRV packets because applications have different real-time and memory constraints. The application should service DSRV promptly and handle overflow as an error condition.

### Known/documented high-level handling

The single header provides DLS/DL+ prefix parsing as documented by AN649. AN649 states that the Si468x reconstructs DLS messages/commands before forwarding them. Semantic DL+ tag decoding itself is defined by ETSI TS 102 980, not by AN649, so the driver exposes the reconstructed command body rather than inventing an incomplete decoder.

MOT data is identified (`isMotPad()`) and delivered through the complete raw DSRV path. AN649 explicitly leaves assembly of MOT data groups/segments/objects to the host MOT decoder and points to external MOT specifications; therefore no undocumented MOT object parser is fabricated in this library.

Likewise, proprietary HD Radio data-service payloads remain raw when AN649 delegates their layout to external licensed specifications.

## Very small hosts

There is no compile-time feature switch. The **same header** is used on all hosts.

Because member functions are header-defined/inline and the library has no global feature tables requiring runtime allocation, a normal optimizing linker can discard unused code. The application controls RAM usage through workspace size and streaming.

For an ATmega8-class target, a practical architecture is normally:

```text
MCU program flash: MiniPatch or small FullPatch reader code
NVSPI SPI NOR:     FullPatch + selected mode firmware
small workspace:   command/response chunks
```

The same `Si468x.h` can also be used on an ESP32 with a much larger workspace and richer application/UI code.

## Display/UI independence

The driver has no concept of a display. A project can use ST7735/ST7789/ILI9341, SSD1306/SH1106, HD44780 16x2, a serial terminal, a web UI, or no display at all. UI refresh policy and text rendering belong to the application.

## Firmware pack

`Si468xFirmwarePack/` is a separate stand-alone package. It is not included by `Si468x.h` and can be used with other Si468x projects/drivers.

See:

- `Si468xFirmwarePack/README.md`
- `Si468xFirmwarePack/FIRMWARE_PROGRAMMING.md`

## Migration from DABShield

See `MIGRATION_FROM_DABSHIELD.md` for a conceptual mapping. There is deliberately no compatibility shim: new projects should use the clean API rather than preserve DABShield's blocking/platform-coupled internals.

## Validation status

This release has desktop C++11 compile tests and parser/command tests. It has **not yet been validated on every physical Si468x part or every firmware revision**. Hardware validation should include boot, tune/seek, interrupt timing, DSRV load, NVSPI programming and error recovery on the actual board.
