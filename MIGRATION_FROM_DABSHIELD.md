# Migration from DABShield to `Si468x.h`

This is a conceptual migration guide, not a compatibility layer.

The goal is to keep application functionality while removing the platform coupling, fixed buffers and blocking polling patterns commonly found in DABShield-derived projects.

## Architecture change

Typical DABShield-style design:

```text
application
  -> DAB class
      -> Arduino SPI/GPIO/delay
      -> Si468x commands
      -> parser/state
```

New design:

```text
application/UI/storage
  -> Si468x.h
      -> logical command/response engine
      -> application-provided HostInterface
          -> Arduino SPI, ESP-IDF SPI, AVR registers, Linux spidev, I2C, ...
```

## Common mappings

| DABShield concept | `Si468x.h` |
|---|---|
| `begin(...)` boot path | explicit power/patch/image boot helper selected by project |
| fixed `SPIbuffer` | application-supplied workspace and caller reply buffers |
| `Set_Property()` | `setProperty()` |
| property read | `getProperty()` |
| FM frequency tune | `fmTune()` or `startFmTune()` |
| FM seek | `fmSeek()` or `startFmSeek()` |
| FM signal status | `fmRsqStatus()` |
| RDS raw groups | `fmRdsStatus()` |
| RDS application text | application decoder fed from `FmRdsGroup` raw blocks |
| DAB tune | `dabTune()` or `startDabTune()` |
| DAB signal status | `dabDigradStatus()` |
| service list | `getDigitalServiceList()` + `DabServiceListParser` |
| start selected DAB component | `startDabService(serviceId, componentId)` |
| DAB DLS/data polling | DSRV interrupt/status + `getDigitalServiceDataHeader()` / raw data read |
| firmware array hard-wired into library | independent `ImageReader` or NVSPI `FLASH_LOAD` |
| `delay()`/CTS loop | `service()` + INTB or bounded polling fallback |

## Important behavioral differences

### 1. CTS and STC are separate

A tune/seek command reaches CTS before RF tune/seek completion is necessarily finished. `fmTune()`, `amTune()` and `dabTune()` wait for command CTS only. Final tune completion is the STC event.

### 2. INTB is a first-class mechanism

The recommended ISR only calls:

```cpp
radio.notifyInterrupt();
```

Mode-specific event handling runs from the main context after `service()` reports status.

### 3. No automatic display/storage side effects

Station labels, favorites, EEPROM, TFT/OLED refresh, slideshow storage and audio-amplifier control are application responsibilities.

### 4. DAB service components are not truncated

The new streaming parser returns every component for every service and preserves the exact 32-bit component entry as `DabComponentEntry::componentId`.

Use that exact value for subsequent Si468x component/service commands.

### 5. DAB SERTYPE

Use `startDabService()` and `stopDabService()`. They write SERTYPE=0 as required by the DAB application definition in AN649.

The generic `startDigitalService()` remains available for HD modes where SERTYPE distinguishes audio/data.

### 6. Large replies are streamable

Do not copy the old 4 KB global buffer simply because the old library used one. `READ_OFFSET` and the streaming DAB service-list parser let a small host work with much smaller RAM.

### 7. Firmware is separate

The library can boot firmware from an application callback or load it from NVSPI. It does not know or care where the bytes came from.

## Suggested migration order

1. Implement the platform `HostInterface` while keeping the existing UI untouched.
2. Verify `GET_PART_INFO`, `GET_SYS_STATE` and the existing boot path.
3. Move volume/mute and generic property access.
4. Move FM tune/seek/RSQ/RDS.
5. Move DAB tune/DIGRAD/service-list/start-service.
6. Connect INTB and replace periodic heavy polling with event-driven maintenance.
7. Move DSRV/DLS/slideshow handling into the application/data-service layer.
8. Remove the old DABShield driver after behavior is verified on hardware.

This staged approach keeps display/UI changes independent from radio-protocol changes.
