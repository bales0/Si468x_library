# Porting `Si468x.h` to a New Platform

The core driver never touches a hardware register. A port consists of a small `HostInterface` adapter and application-owned buffers.

## 1. Implement `writeCommand`

Signature:

```cpp
bool writeCommand(void* context,
                  uint8_t command,
                  const uint8_t* args,
                  uint16_t argLength);
```

The adapter must send exactly one command transaction:

```text
CMD, ARG1, ARG2, ... ARGn
```

The core has already constructed all AN649 fields. The adapter must not reinterpret them.

### SPI adapter

A typical SPI adapter performs:

```text
begin bus transaction
assert Si468x SSB
transfer CMD
transfer argument bytes
release SSB
end bus transaction
```

Clock rate/mode are board/firmware decisions. Do not hard-code a frequency in the universal driver. AN649 reports fatal command/reply overflow conditions if the command interface is clocked faster than the internal path can sustain.

### I2C adapter

An I2C adapter writes the command and argument bytes using the device's host-control I2C framing/addressing. The universal driver sees the same logical `CMD + ARG` operation.

## 2. Implement `readReply`

Signature:

```cpp
bool readReply(void* context,
               uint8_t* destination,
               uint16_t length);
```

It must return:

```text
destination[0] = STATUS0
destination[1] = STATUS1
destination[2] = STATUS2
destination[3] = STATUS3
destination[4] = RESP4 / error byte / first payload byte
...
```

If the physical SPI protocol returns a dummy byte before STATUS0, strip it inside the adapter. This is the main difference from older Arduino DABShield code, where the raw SPI buffer often contains a physical framing byte at index zero.

## 3. Supply a monotonic microsecond timer

```cpp
uint32_t timeUs(void* context);
```

Unsigned 32-bit wraparound is supported.

It is required for:

- blocking convenience calls;
- command timeouts;
- programmed CTS polling intervals;
- boot helper delays.

The cooperative non-blocking engine can still be called without a timer; it then polls CTS once per `service()` call and does not enforce time-based expiration.

## 4. Optional idle hook

```cpp
void idle(void* context);
```

Used only while a blocking helper is waiting. It may feed a watchdog, yield to an RTOS, sleep until interrupt, or do nothing.

## 5. Optional reset/power hooks

```cpp
void setReset(void* context, bool asserted);
void setPower(void* context, bool enabled);
```

`asserted` and `enabled` are logical values. Electrical polarity belongs entirely in the platform adapter.

The driver exposes `setResetAsserted()` and `setPowerEnabled()` for direct board control. `hardwareReset()` provides the generic safe ordering used by the core: assert reset first, optionally enable board power while reset remains asserted, wait for supplies to settle, then release reset. The adapter still owns electrical polarity and any board-specific regulator/clock sequencing.

## 6. INTB

The GPIO ISR should be minimal:

```cpp
void irqHandler()
{
    radio.notifyInterrupt();
}
```

Do not perform Si468x SPI/I2C transfers inside the interrupt handler.

The main loop calls:

```cpp
radio.service();
```

A status callback may set application flags, but should not recursively send a command from inside `service()`.

## 7. Workspace

Set a buffer appropriate for the platform:

```cpp
radio.setWorkspace(buffer, sizeof(buffer));
```

The buffer is used only by operations that actually need it, such as streaming `HOST_LOAD`, DAB frequency-list generation and NVSPI block writes.

A small host can use `READ_OFFSET` for large replies. The DAB service-list parser needs only 24 bytes of its own record state.

## 8. Firmware source adapter

`ImageReader` is intentionally storage-neutral:

```cpp
size_t readImage(void* context,
                 uint32_t offset,
                 uint8_t* destination,
                 size_t length);
```

Possible implementations:

- AVR program flash (`pgm_read_byte*` in the adapter);
- memory-mapped MCU flash;
- SD/file read;
- external EEPROM/SPI flash controlled by the MCU;
- network/file service on Linux;
- any project-specific storage.

The driver never assumes `PROGMEM` or a unified address space.

## 9. Large response streaming example

For a DAB service list on a memory-constrained host:

```cpp
uint8_t status[4];
radio.getDigitalServiceList(0, status, sizeof(status));

si468x::DabServiceListParser parser;
parser.setSink(mySink);

uint16_t offset = 0;
while (!parser.complete()) {
    uint8_t chunk[4 + 64];
    radio.readOffset(offset, chunk, 64);
    parser.feed(chunk + 4, 64); // strip status word
    offset += 64;
}
```

Real code should use the returned list-size field to stop at the exact response length rather than blindly feed padding beyond the list.

## 10. Physical SPI/NVSPI bus ownership

The primary host-control interface and the Si468x NVSPI flash bus are separate functions. If a design also lets the MCU directly program the same NVSPI flash, the board must provide valid bus ownership/tri-state behavior. Keep that board policy outside `Si468x.h`.
