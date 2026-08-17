# Si468x Protocol Quick Reference

This reference accompanies `Si468x.h` and is intentionally protocol-only. It summarizes the AN649 Rev. 1.9 command/property surface used by the driver. Broadcast-content formats such as RDS group semantics, DLS/DL+, MOT and EPG are application-layer concerns.

## Command error codes

When `STATUS0.ERR_CMD` is set, the first response byte after the four status bytes is the command-error code. `Si468x.h` exposes the raw byte with `lastDeviceError()` and the typed `CommandErrorReason` through `lastCommandErrorReason()`.

| Code | `CommandErrorReason` | AN649 meaning |
|---:|---|---|
| `0x01` | `Unspecified` | unspecified |
| `0x02` | `ReplyOverflow` | reply overflow |
| `0x03` | `NotAvailable` | not available |
| `0x04` | `NotSupported` | not supported |
| `0x05` | `BadFrequency` | bad frequency |
| `0x10` | `CommandNotFound` | command not found |
| `0x11` | `BadArg1` | bad argument 1 |
| `0x12` | `BadArg2` | bad argument 2 |
| `0x13` | `BadArg3` | bad argument 3 |
| `0x14` | `BadArg4` | bad argument 4 |
| `0x15` | `BadArg5` | bad argument 5 |
| `0x16` | `BadArg6` | bad argument 6 |
| `0x17` | `BadArg7` | bad argument 7 |
| `0x18` | `CommandBusy` | command busy |
| `0x19` | `AtBandLimit` | at band limit |
| `0x20` | `BadNvm` | bad NVM |
| `0x30` | `BadPatch` | bad patch |
| `0x31` | `BadBootMode` | bad boot mode |
| `0x40` | `BadProperty` | bad property |
| `0x50` | `NotAcquired` | not acquired |
| `0xFF` | `AppNotSupported` | application not supported |

Any other value maps to `Unknown`; the original byte is still available from `lastDeviceError()`.

## Commands

- `0x00` **RD_REPLY** — RD_REPLY command must be called to return the status byte and data for the last command sent to the device. This command is also used to poll the status byte as needed. To poll the status byte, send the RD_REPLY command and read the status byte. This can be done regardless of the state of the CTS bit in the status register. Please refer to individual command descriptions for the format of returned data. RD_REPLY is a hardware command and can be issued while device is powered down. For commands where the size of the response is returned, the user should send the RD_REPLY command to read the SIZE first. Each time the RD_REPLY command is sent, the STAUS bytes will still be returned.
- `0x01` **POWER_UP** — The POWER_UP initiates the boot process to move the device from power down to power up mode. There are two possible boot scenarios: Host image load and FLASH image load. When the host is loading the image the host first executes the POWER_UP command to set the system settings REF_CLK, etc). A LOAD_INIT command then prepares the bootloader to receive a new image. After the LOAD_INIT command, using the HOST_LOAD command loads the image into the device RAM. After the RAM is loaded the host issues the BOOT command. When booting a FLASH image the host issues the POWER_UP command to set the system settings. Then issues the FLASH_LOAD command to select and load the image from FLASH. Once the image is loaded the host sends the BOOT command to boot the application. Power-up is complete when the CTS bit is set. This command may only be sent while the device is powered down. Note: FLASH_LOAD is not supported in A0A or A0B revisions.
- `0x04` **HOST_LOAD** — HOST_LOAD loads an image from HOST over command interface. It sends up to 4096 bytes of application image to the bootloader. Note: This command is much more efficient when the image is sent as multiples of 4 bytes.
- `0x05` **FLASH_LOAD** — FLASH_LOAD loads the firmware image from an externally attached SPI flash over the secondary SPI bus. The image must be contiguous on the flash.
- `0x06` **LOAD_INIT** — LOAD_INIT prepares the bootloader to receive a new image. It will force the bootloader state to waiting for a new LOAD command (HOST_LOAD or FLASH_LOAD.) LOAD_INIT command must always be sent prior to a HOST_LOAD or a FLASH_LOAD command.
- `0x07` **BOOT** — BOOT command boots the image currently loaded in RAM.
- `0x08` **GET_PART_INFO** — GET_PART_INFO reports basic information about the device such as Part Number, Part Version, ROM ID, etc. This command will hold CTS until the reply is available.
- `0x09` **GET_SYS_STATE** — GET_SYS_STATE reports basic system state information such as which mode is active; FM, DAB, etc.
- `0x0A` **GET_POWER_UP_ARGS** — GET_POWER_UP_ARGS reports basic information about the device such as which parameters were used during power up. This command will hold CTS until the reply is available.
- `0x10` **READ_OFFSET** — READ_OFFSET is used for applications that cannot read the entire response buffer. This type of application can use this command to read the response buffer in segments. The host must pass in an offset from the beginning of the response buffer to indicate the starting point from which to read. This offset must be modulo 4. The response buffer remains intact as in the READ_REPLY command so that the response can be read again if needed. This function is available for both I2C and SPI mode. This is a software command, therefore it is best to read as much data in each calling as possible. This will reduce the overhead associated with using this command. It is recommended that the minimum reply size be on the order of 512 bytes. This means that for APIs that return less the 512 bytes the standard READ_REPLY should be used.
- `0x12` **GET_FUNC_INFO** — GET_FUNC_INFO returns the function revision number for currently loaded firmware (FMHD, AM etc.) as opposed to GET_PART_INFO command that provides the revision number for the combo firmware. For example, GET_PART_INFO would return A0B is the firmware revision while GET_FUNC_INFO would return 1.0.4 for FM function revision if the currently running firmware function is FM.
- `0x13` **SET_PROPERTY** — SET_PROPERTY sets the value of a property.
- `0x14` **GET_PROPERTY** — GET_PROPERTY retrieves the value of a property or properties. The host may read as many properties as desired up to the end of a given property group. An attempt to read passed the end of the property group will result in zeros being read.
- `0x30` **FM_TUNE_FREQ** — FM_TUNE_FREQ tunes the FM receiver to a frequency in 10 kHz steps. The optional STC interrupt is set when the command completes the tune. Sending this command clears any pending STCINT, RSQINT, or RDSINT bit in STATUS.
- `0x31` **FM_SEEK_START** — FM_SEEK_START begins searching for a valid station. The search starts at FM_RSQ_STATUS:READFREQ + FM_SEEK_FREQUENCY_SPACING in the specified direction. In order for a station to be considered valid, each of the following thresholds must be met: FM_VALID_SNR_THRESHOLD, FM_VALID_RSSI_THRESHOLD, FM_VALID_MAX_TUNE_ERROR, and FM_VALID_HDLEVEL_THRESHOLD (if the value is non-zero, which indicates an HD seek). Clears any pending STCINT, RSQINT, or RDSINT interrupt status. Seek can be cancelled through setting the CANCEL bit in the FM_RSQ_STATUS command. The optional STC interrupt is set when the command completes.
- `0x32` **FM_RSQ_STATUS** — FM_RSQ_STATUS returns status information about the received signal quality. This command returns the Received Signal Strength Indicator (RSSI), Signal to Noise Ratio (SNR), frequency offset (FREQOFF), and Multipath (MULT) associated with the desired channel. It also indicates valid channel (VALID) and AFC rail status (AFCRL). This command can be used to check if the received signal is above the RSSI high threshold as reported by RSSIHINT or below the RSSI low threshold as reported by RSSILINT. It can also be used to check if the signal is above the SNR high threshold as reported by SNRHINT or below the SNR low threshold as reported by SNRLINT. It can be used to check if the detected multipath is above the Multipath high threshold as reported by MULTHINT or below the Multipath low threshold as reported by MULTLINT. The command clears the RSQINT, BLENDINT, SNRHINT, SNRLINT, RSSIHINT, RSSILINT, MULTHINT, and MULTLINT interrupt bits when the RSQACK bit of ARG1 is set. These are sticky meaning they will remain set until RSQACK is set. If the condition is still true after the interrupt is cleared another interrupt will fire assuming that bit is enabled in FM_RSQ_INTERRUPT_SOURCE.
- `0x33` **FM_ACF_STATUS** — FM_ACF_STATUS returns status information about automatically controlled features of the device. The automatically controlled features include blend, high cut, and softmute. The bits BLEND_INT, HIGHCUT_INT, and SMUTE_INT are sticky meaning they will remain set until ACFACK is set. If the condition is still true after the interrupt is cleared another interrupt will fire. See the FM_ACF_INTERRUPT_SOURCE property for information on enabling the ACFINT
- `0x34` **FM_RDS_STATUS** — FM_RDS_STATUS returns RDS information for current channel and reads an entry from the RDS FIFO.
- `0x35` **FM_RDS_BLOCKCOUNT** — FM_RDS_BLOCKCOUNT command queries the block statistic info of RDS decoder. This command returns RDS expected, received and uncorrectable, block statistic information. Information from this command can be reset by setting CLEAR bit or sending FM_TUNE_FREQ command. Once EXPECTED saturates at 65535, all other block count statistics will be frozen until the counts are cleared.
- `0x80` **GET_DIGITAL_SERVICE_LIST** — GET_DIGITAL_SERVICE_LIST gets a service list of the ensemble. This command should be issued each time an audio or data service list is updated as indicated by the ASRVLISTINT or DSRVLISTINT bit of the HD_GET_EVENT_STATUS command. This occurs shortly after tune time when a digital radio tuning mode is selected and the ensemble has been acquired. Please refer to iBiquity document: RX_IDD_2206 Appendix L (Get_All_Data_Services_Info and Get_All_Audio_Services_Info) for the format of the HD Radio Service List. In the case of HD this command also retrieves the audio or data service info when the appropriate service type option is selected. This service info is available whenever the AINFO or DINFO bit(s) are set in the HD_GET_EVENT_STATUS response. This audio and data information provides a quick look at the services in the ensemble and can be used to reduce scan time as this information is ready for parsing well before the service lists. The payload of these responses are defined in Table 5-4 of the RX_IDD_2206 main document.
- `0x81` **START_DIGITAL_SERVICE** — START_DIGITAL_SERVICE starts an audio or data service. This command is used for HD audio and data services. To determine what services exist in an ensemble please use the GET_DIGITAL_SERVICE_LIST command. In HD radio applications the broadcaster does not always transmit this service information. In this case no data services are available but there may be multiple audio programs available. To view which audio services are available use the HD_DIGRAD_STATUS command's AUDIO_PROG_AVAIL field to see which audio programs can be selected. In addition the SERVICE_ID (service number) is not required when selecting an audio or data service. In this case please set the SERVICE_ID parameter to 0. I the case of starting an audio service, it is not required to stop a currently running audio service/program before starting a new one. The currently running audio service will be stopped automatically when the new service is requested.
- `0x82` **STOP_DIGITAL_SERVICE** — STOP_DIGITAL_SERVICE stops an audio or data service.
- `0x84` **GET_DIGITAL_SERVICE_DATA** — GET_DIGITAL_SERVICE_DATA gets a block of data associated with one of the enabled data components of a digital service. Information about this block of data is found in the data header that is returned at the beginning of the data block. In order to determine the ideal number of PAYLOAD bytes to read, the header information can be read first followed by a second read of the full (header + PAYLOAD) length - it is unnecessary to call GET_DIGITAL_SERVICE_DATA twice to use this method. The data associated with this transaction will be discarded at the receipt of a next GET_DIGITAL_SERVICE_DATA command if STATUS_ONLY = 0. Reading past the end of the buffer will result in zeros for the invalid bytes. Please refer to iBiquity document: SY_IDD_1019s Rev F (sections 5 and 6) for the format of the HD Radio data service data.
- `0x92` **HD_DIGRAD_STATUS** — HD_DIGRAD_STATUS returns status information about the digital radio and ensemble. The bits AERRHINT, AEERLINT, CDNRHINT, CDNRLINT, and ACQINT are sticky meaning they will remain set until DIGRAD_ACK is set. If the condition is still true after the interrupt is cleared another interrupt will fire assuming that bit is enabled in HD_DIGRAD_INTERRUPT_SOURCE.
- `0x93` **HD_GET_EVENT_STATUS** — HD_GET_EVENT_STATUS retrieves the status of HD related events. This includes items such as new alarms available, new PSD, New station info, etc.
- `0x94` **HD_GET_STATION_INFO** — HD_GET_STATION_INFO retrieves information about the ensemble broadcaster. The station information is defined in the 2206 standard.
- `0x95` **HD_GET_PSD_DECODE** — Retrieves PSD information.
- `0x96` **HD_GET_ALERT_MSG** — HD_GET_ALERT_MSG retrieves alert message. Alerts are special messages provided by the broadcaster that may signal important information about emergencies or events. Full details about Alerts can be found in the 2206 standard. This API is used to collect the alert data and is used in response to an alert event. See the HD_GET_EVENT_STATUS command for details on the alert event.
- `0x97` **HD_PLAY_ALERT_TONE** — HD_PLAY_ALERT_TONE plays the alert tone. Alerts are special messages provided by the broadcaster that may signal important information about emergencies or events. Full details about Alerts can be found in the 2206 standard. This API is used to play an alert tone at the host's discretion. It is recommended that the host play this tone for each unique alert message it receives. If the host chooses it can also have these tones played automatically on every alert message. See the HD_EVENT_ALERT_CONFIG property for details on playing alert tones automatically. Also see the HD_GET_EVENT_STATUS command for details on the alert event.
- `0x98` **HD_TEST_GET_BER_INFO** — HD_TEST_GET_BER_INFO reads the current BER information for the HD digital demod. The information returned by this command is only meaningful if the BER test vector (IB_FMr208c_e1wfc204 for FMHD, IB_AMr208a_e1awfb00 for AMHD) is being received.
- `0x99` **HD_SET_ENABLED_PORTS** — HD_SET_ENABLED_PORTS sets the default HD ports retrieved/enabled when HD has been acquired.
- `0x9A` **HD_GET_ENABLED_PORTS** — HD_GET_ENABLED_PORTS gets the default HD ports retrieved when HD has been acquired.
- `0xE5` **TEST_GET_RSSI** — TEST_GET_RSSI returns the reported RSSI in 8.8 format. This command is used to help calibrate the frontend tracking circuit. It returns the RSSI value in dBµV to 1/256 of a dB.
- `0x15` **WRITE_STORAGE** — DAB/DAB+ API in AN649 Rev. 1.9; writes up to 256 bytes to the on-board storage area at a byte offset.
- `0x16` **READ_STORAGE** — DAB/DAB+ API in AN649 Rev. 1.9; reads the on-board storage area from a byte offset.
- `0xB0` **DAB_TUNE_FREQ** — DAB_TUNE_FREQ sets the DAB Receiver to tune to a frequency between 168.16 MHz and 239.20 MHz defined by the table through DAB_SET_FREQ_LIST. The optional STC interrupt is set when the command completes the tune. Sending this command clears any pending STCINT bit in the STATUS. The default list that will be used by the tuner is the European frequency list. To change this list (example: for T-DMB), the user must first call DAB_SET_FREQ_LIST before calling the DAB_TUNE_FREQ command.
- `0xB2` **DAB_DIGRAD_STATUS** — DAB_DIGRAD_STATUS returns status information about the digital radio and ensemble including a change in ensemble acquisition state, current estimates for ensemble's MSC (Main Service Channel) BER (bit error rate), FIC (Fast Information Channel) BER along with number of FIBs (Fast Information Block) that failed a CRC check and number of Reed-Solomon decoder errors (DAB+ and DMB only). The bits RSSILINT, RSSIHINT, ACQINT are sticky meaning they will remain set until DIGRAD_ACK is set. If the condition is still true after the interrupt is cleared another interrupt will fire assuming that bit is enabled in DAB_DIGRAD_INTERRUPT_SOURCE.
- `0xB3` **DAB_GET_EVENT_STATUS** — DAB_GET_EVENT_STATUS gets information about the various events related to the DAB radio. These events include signaling the reception of new PAD (Programme-Associated Data) data, service lists and announcements. The bits SVRLISTINT, ANNOINT, RECFGWRNINT, and RECFGINT are sticky meaning they will remain set until EVENT_ACK is set. If the condition is still true after the interrupt is cleared another interrupt will fire assuming that bit is enabled in DAB_EVENT_INTERRUPT_SOURCE.
- `0xB4` **DAB_GET_ENSEMBLE_INFO** — DAB_GET_ENSEMBLE_INFO gets information about the current ensemble such as the ensemble ID and label.
- `0xB7` **DAB_GET_SERVICE_LINKING_INFO** — DAB_GET_SERVICE_LINKING_INFO provides service linking info for the passed in service ID. Provides information on where to look for the alternate services or supplemental services relating to the passed in service ID. This could include another ensemble, another service within the current ensemble, or an FM broadcast. Please see clause 8.1.15 of ETSI 300-401 for further details. This command can return multiple links for a given service.
- `0xB8` **DAB_SET_FREQ_LIST** — DAB_SET_FREQ_LIST command sets the DAB frequency table. The frequencies are in units of 1 kHz. The table can be populated with a single entry or a regional list (for example 5 or 6 entries). It is recommended to make the list regional to increase scanning speed.
- `0xB9` **DAB_GET_FREQ_LIST** — DAB_GET_FREQ_LIST gets the DAB frequency table. All frequencies are in units of 1 kHz.
- `0xBB` **DAB_GET_COMPONENT_INFO** — DAB_GET_COMPONENT_INFO gets information about components within the ensemble if available.
- `0xBC` **DAB_GET_TIME** — DAB_GET_TIME gets the ensemble time adjusted for the local time offset.
- `0xBD` **DAB_GET_AUDIO_INFO** — DAB_GET_AUDIO_INFO gets information about the current audio service (decoder bps, audio mode).
- `0xBE` **DAB_GET_SUBCHAN_INFO** — DAB_GET_SUBCHAN_INFO gets information about the sub-channel (service mode, protection, subchannel bps).
- `0xBF` **DAB_GET_FREQ_INFO** — DAB_GET_FREQ_INFO gets radio Frequency Information (FI) about the ensemble.
- `0xC0` **DAB_GET_SERVICE_INFO** — Gets information about a DAB service.
- `0xE8` **DAB_TEST_GET_BER_INFO** — DAB_TEST_GET_BER_INFO reads the current BER rate using debug information that was sent to the test port.
- `0x40` **AM_TUNE_FREQ** — AM_TUNE_FREQ tunes the AM receiver to a frequency in 1 kHz steps. The optional STC interrupt is set when the command completes the tune. Sending this command clears any pending STCINT or RSQINT bit in STATUS.
- `0x41` **AM_SEEK_START** — AM_SEEK_START begins searching for a valid station. The search starts at AM_RSQ_STATUS:READFREQ + AM_SEEK_FREQUENCY_SPACING in the specified direction. In order for a station to be considered valid, each of the following thresholds must be met: AM_VALID_SNR_THRESHOLD, AM_VALID_RSSI_THRESHOLD, AM_VALID_MAX_TUNE_ERROR, and AM_VALID_HDLEVEL_THRESHOLD (if the value is non-zero, which indicates an HD seek). Clears any pending STCINT or RSQINT interrupt status. Seek can be canceled through setting the CANCEL bit in the AM_RSQ_STATUS command. The optional STC interrupt is set when the command completes.
- `0x42` **AM_RSQ_STATUS** — AM_RSQ_STATUS returns status information about the received signal quality. This command returns the Received Signal Strength Indicator (RSSI), Signal to Noise Ratio (SNR), frequency offset (FREQOFF), and Multipath (MULT) associated with the desired channel. It also indicates valid channel (VALID) and AFC rail status (AFCRL). This command can be used to check if the received signal is above the RSSI high threshold as reported by RSSIHINT or below the RSSI low threshold as reported by RSSILINT. It can also be used to check if the signal is above the SNR high threshold as reported by SNRHINT or below the SNR low threshold as reported by SNRLINT. It can be used to check if the detected multipath is above the Multipath high threshold as reported by MULTHINT or below the Multipath low threshold as reported by MULTLINT. The command clears the RSQINT, BLENDINT, SNRHINT, SNRLINT, RSSIHINT, RSSILINT, MULTHINT, and MULTLINT interrupt bits when the RSQACK bit of ARG1 is set. These are sticky meaning they will remain set until RSQACK is set. If the condition is still true after the interrupt is cleared another interrupt will fire assuming that bit is enabled in AM_RSQ_INTERRUPT_SOURCE.
- `0x43` **AM_ACF_STATUS** — AM_ACF_STATUS returns status information about automatically controlled features of the device. The automatically controlled features include blend, high cut, and softmute. The bits BLEND_INT, HIGHCUT_INT, and SMUTE_INT are sticky meaning they will remain set until ACFACK is set. If the condition is still true after the interrupt is cleared another interrupt will fire. See the AM_ACF_INTERRUPT_SOURCE property for information on enabling the ACFINT


### AN649 Rev. 1.9 internal inconsistency: DSRV physical-error interrupt

Section 7.7 Table 19 and its DSRV handling example define INTSRC bit 2 as `DSRVERRINT` / physical error, while some command/property tables in the same revision show bit 2 as reserved. `Si468x.h` exposes received bit 2 but treats enablement through property `0x8100` as firmware-revision dependent.


## Raw and repeated reply layouts

The driver uses logical reply offsets where byte `0` is `STATUS0`. Fixed replies that have typed structures are decoded directly by `Si468x.h`; the layouts below cover important variable/repeated responses that intentionally remain raw so the core does not allocate application-owned lists.

### DAB_GET_SERVICE_LINKING_INFO (`0xB7`)

Arguments are three zero bytes followed by `SERVICE_ID` as little-endian `uint32_t`. The reply begins with `RESP4 = NUM_LINKS`, `RESP5` reserved, and `RESP6..7 = LINK_BYTES`. The payload from `RESP8` consists of repeated link records. Each link starts with `ACTUATOR`, `HARDLINK`, `ILS`, `LINKTYPE`, then little-endian `LINKID` (`uint32_t`) and `NUMALTS` (`uint32_t`), followed by `NUMALTS` little-endian 32-bit alternate values. Use `LINK_BYTES` as the payload bound.

### DAB_GET_FREQ_LIST (`0xB9`)

`RESP4 = NUM_FREQS`, `RESP5..7` are reserved, and `RESP8...` contains `NUM_FREQS` little-endian 32-bit frequencies in **kHz**.

### DAB_GET_COMPONENT_INFO (`0xBB`)

The fixed prefix is represented by `DabComponentInfo`: `RESP4 GLOBAL_ID`, `RESP6 LANG[5:0]`, `RESP7 CHARSETID[5:0]`, `RESP8..23` the 16-byte label, `RESP24..25` the abbreviation mask, `RESP26 NUMUA`, and `RESP27 LENUA`. `userApplicationRaw` points at `RESP28`, the first `UATYPE` field. `LENUA` covers the complete user-application block including type, length, data, and alignment padding; the application may decode individual user-application standards.

### DAB_GET_FREQ_INFO (`0xBF`)

`RESP4..7 = LENGTH_FI_LIST` as little-endian `uint32_t`. Repeated FI records follow at `RESP8`: `FI_ID` (`uint32_t`), `FI_FREQ` (`uint32_t`, Hz), `FI_FREQ_INDEX` (`uint8_t`), `FI_RNM` (`uint8_t`), `FI_CONTINUITY` (`uint8_t`), and `FI_CONTROL` (`uint8_t`). Use `LENGTH_FI_LIST` as the payload bound.

### HD_GET_ENABLED_PORTS (`0x9A`)

`RESP4 = LENGTH` (maximum 64), `RESP5` reserved, and `RESP6...` contains `LENGTH` little-endian 16-bit port addresses. The matching `hdSetEnabledPorts()` helper accepts the same addresses as `uint16_t[]`.

### HD_GET_STATION_INFO / HD_GET_PSD_DECODE / HD_GET_ALERT_MSG

AN649 defines the command selectors and framing, but delegates the returned station-information, PSD, and alert content semantics to the referenced HD Radio specifications. `Si468x.h` therefore validates the AN649 selector ranges and returns the data bytes raw instead of inventing an external-format decoder.

### GET_DIGITAL_SERVICE_DATA (`0x84`)

The fixed 24-byte header is represented by `DsrvHeader`. `RESP4` is the interrupt-source byte, `RESP5` queue depth, `RESP6` service state, `RESP7` data type, `RESP8..11` service ID, `RESP12..15` component ID/port, `RESP16..17` reserved, `RESP18..19` payload byte count, `RESP20..21` segment/sequence number, and `RESP22..23` total segment count. Payload starts at `RESP24`. Broadcast-content interpretation remains outside the core.

`GET_DIGITAL_SERVICE_DATA` is suitable for a header-first read: read the 24-byte header to determine payload size, then re-read the preserved reply or use `READ_OFFSET` for the required bytes.

## NVSPI flash pass-through

AN649 Table 11 defines eleven supplemental `FLASH_LOAD (0x05)` subcommands for host access to the external NVSPI flash. The driver represents all eleven in `FlashSubcommand`: image load (`0x00`), image load with CRC32 (`0x01`), CRC32 check (`0x02`), property-list set (`0x10`), property get (`0x11`), four block-write/verify modes (`0xF0`..`0xF3`), sector erase (`0xFE`) and chip erase (`0xFF`). A10 devices require the documented ROM0.016 pass-through patch path before using these operations.

AN649 Table 12 defines all eight pass-through properties:

| ID | `FlashProperty` | Default | Meaning |
|---:|---|---:|---|
| `0x0001` | `SPI_CLOCK_FREQ_KHZ` | `25000` | NVSPI master clock in kHz; range 3000..40000 |
| `0x0002` | `SPI_MODE` | `3` | SPI mode 0..3 |
| `0x0101` | `READ_CMD` | `0x03` | flash read opcode |
| `0x0102` | `HIGH_SPEED_READ_CMD` | `0x0B` | fast-read opcode |
| `0x0103` | `HIGH_SPEED_READ_MAX_FREQ_MHZ` | `0` | flash-specific fast-read frequency limit |
| `0x0201` | `WRITE_CMD` | `0x02` | page-program/write opcode |
| `0x0202` | `ERASE_SECTOR_CMD` | `0x20` | sector-erase opcode |
| `0x0204` | `ERASE_CHIP_CMD` | `0xC7` | whole-chip erase opcode |

## Properties

Every property is accessible through `setProperty()` / `getProperty()`. Defaults, units and ranges below are taken from the AN649 property sections when stated.

- `0x0000` **INT_CTL_ENABLE** — INT_CTL_ENABLE property enables top-level interrupt sources. (default 0x0000)
- `0x0001` **INT_CTL_REPEAT** — INT_CTL_REPEAT is used to set repeat interrupt pulses for a given interrupt even if this particular interrupt was previously generated but not acknowledged. (default 0x0000)
- `0x0200` **DIGITAL_IO_OUTPUT_SELECT** — DIGITAL_IO_OUTPUT_SELECT configures the digital audio output to be I2S Master or Slave. (default 0)
- `0x0201` **DIGITAL_IO_OUTPUT_SAMPLE_RATE** — DIGITAL_IO_OUTPUT_SAMPLE_RATE sets output sample audio rate in units of 1Hz. (default 48000; units Hz)
- `0x0202` **DIGITAL_IO_OUTPUT_FORMAT** — DIGITAL_IO_OUTPUT_FORMAT configures the digital audio output format. This property may only be written before the first tune. (default 0x1800; range 8-24)
- `0x0203` **DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_1** — DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_1 sets alternate I2S format settings from the standard framing mode. (default 0)
- `0x0204` **DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_2** — DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_2 sets alternate I2S format settings from the standard framing mode. (default 0)
- `0x0205` **DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_3** — DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_3 sets alternate I2S format settings from the standard framing mode. (default 0; range 0-32767)
- `0x0206` **DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_4** — DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_4 sets alternate I2S format settings from the standard framing mode. (default 0; range 0-32767)
- `0x0300` **AUDIO_ANALOG_VOLUME** — AUDIO_ANALOG_VOLUME sets the analog audio volume. A value of 0 will mute the audio; a value of 1 applies 62 dB of attenuation, and a value of 63 applies no attenuation. (default 63; units dB; range 0-63)
- `0x0301` **AUDIO_MUTE** — AUDIO_MUTE property mutes/unmutes each audio output independently. (default 0x0000)
- `0x0302` **AUDIO_OUTPUT_CONFIG** — AUDIO_OUTPUT_CONFIG is used to configure various settings of the audio output. (default 0x0000)
- `0x0800` **PIN_CONFIG_ENABLE** — PIN_CONFIG is used to enable and disable the various I/O features of the device. (default 0x8001)
- `0x0900` **WAKE_TONE_ENABLE** — WAKE_TONE_ENABLE is used to enable the wake tone feature. The wake tone feature is a simple alert tone that can be used for various audible alarms such as a wake alarm. (default 0)
- `0x0901` **WAKE_TONE_PERIOD** — WAKE_TONE_PERIOD is used to configure the wake tone feature's on/off period. This property sets the on and off time periods in units of ms. (default 250; range 50-2000)
- `0x0902` **WAKE_TONE_FREQ** — WAKE_TONE_FREQ is the frequency of the wake tone in Hz. The wake tone is a simple square wave whose frequency is defined by this property. (default 750; range 100-2000)
- `0x0903` **WAKE_TONE_AMPLITUDE** — WAKE_TONE_AMPLITUDE sets the wake tone's output amplitude. (default 8; range 0-31)
- `0x1710` **FM_TUNE_FE_VARM** — FM_TUNE_FE_VARM FM Front End Varactor configuration slope (x 1000) which has been calculated for a particular board design. Both FM_TUNE_FE_VARB and FM_TUNE_FE_VARM must be configured. (default 0)
- `0x1711` **FM_TUNE_FE_VARB** — FM_TUNE_FE_VARB FM Front End Varactor configuration intercept which has been calculated for a particular board design. Both FM_TUNE_FE_VARB and FM_TUNE_FE_VARM must be configured. (default 0)
- `0x1712` **FM_TUNE_FE_CFG** — FM_TUNE_FE_CFG Additional configuration options for the front end. These take effect upon FM_TUNE_FREQ. (default 0x0000)
- `0x3100` **FM_SEEK_BAND_BOTTOM** — FM_SEEK_BAND_BOTTOM sets the lower seek boundary of the FM band in multiples of 10kHz. See FM_SEEK_START. (default 8750; units 10kHz; range 7600-10800)
- `0x3101` **FM_SEEK_BAND_TOP** — FM_SEEK_BAND_TOP sets the upper seek boundary of the FM band in multiples of 10kHz. See FM_SEEK_START. (default 10790; units 10kHz; range 7600-10800)
- `0x3102` **FM_SEEK_FREQUENCY_SPACING** — FM_SEEK_FREQUENCY_SPACING sets the frequency spacing for the FM band in multiples of 10kHz when performing a seek. (default 10; units 10kHz; range 1-31)
- `0x3200` **FM_VALID_MAX_TUNE_ERROR** — FM_VALID_MAX_TUNE_ERROR sets the maximum freq error allowed in units of bppm before setting the AFC rail indicator (AFCRL). This will take effect on the next tune. (default 114; units bppm)
- `0x3201` **FM_VALID_RSSI_TIME** — FM_VALID_RSSI_TIME sets the amount of time in ms to allow the RSSI/ISSI metrics to settle before evaluating. The reliability of the valid bit for identifying valid stations relies on this parameter being set properly. (default 15; units ms; range 0-63 - Specified in units ms.)
- `0x3202` **FM_VALID_RSSI_THRESHOLD** — Sets the RSSI threshold for a valid FM Seek/Tune. If the desired channel RSSI is above this threshold, then it is considered valid. (default 17; units dBµV)
- `0x3203` **FM_VALID_SNR_TIME** — FM_VALID_SNR_TIME sets the amount of time in ms to allow the SNR metric to settle before evaluating. The reliability of the valid bit for identifying valid stations relies on this parameter being set properly. (default 40; units ms; range 0-63 - Specified in units ms.)
- `0x3204` **FM_VALID_SNR_THRESHOLD** — FM_VALID_SNR_THRESHOLD sets the SNR threshold for a valid FM Seek/Tune. If the desired channel SNR is above this threshold, then it is considered valid. (default 10; units dB)
- `0x3206` **FM_VALID_HDLEVEL_THRESHOLD** — Sets the HDLEVEL threshold for FM Seek stop. If the desired channel HDLEVEL threshold is above this threshold, then it is considered valid. (default 0; units %)
- `0x3300` **FM_RSQ_INTERRUPT_SOURCE** — FM_RSQ_INTERRUPT_SOURCE configures interrupt related to Received Signal Quality metrics. See FM_RSQ_STATUS. (default 0)
- `0x3301` **FM_RSQ_SNR_HIGH_THRESHOLD** — FM_RSQ_SNR_HIGH_THRESHOLD sets the high threshold, which triggers the RSQ interrupt if the SNR is above this threshold. (default 127; units dB; range –128 to 127 - Specified in units of dB in 1 dB steps.)
- `0x3302` **FM_RSQ_SNR_LOW_THRESHOLD** — FM_RSQ_SNR_LOW_THRESHOLD sets the low threshold, which triggers the RSQ interrupt if the SNR is below this threshold. (default -128; units dB; range –128 to 127 - Specified in units of dB in 1 dB steps.)
- `0x3303` **FM_RSQ_RSSI_HIGH_THRESHOLD** — FM_RSQ_RSSI_HIGH_THRESHOLD sets the high threshold, which triggers the RSQ interrupt if the RSSI is above this threshold. (default 127; units dBµV; range –128 to 127 - Specified in units of dBµV in 1 dBµV steps.)
- `0x3304` **FM_RSQ_RSSI_LOW_THRESHOLD** — FM_RSQ_RSSI_LOW_THRESHOLD sets the low threshold, which triggers the RSQ interrupt if the RSSI is below this threshold. (default -128; units dBµV; range –128 to 127 - Specified in units of dBµV in 1 dBµV steps.)
- `0x3307` **FM_RSQ_HD_DETECTION** — Configures the Fast HD Detection routine. (default 0x000d; range 5-64)
- `0x3308` **FM_RSQ_HD_LEVEL_TIME_CONST** — Configures the Fast HD Detection Level Metric Filtering Time Constant. (default 32; range 1-255)
- `0x3309` **FM_RSQ_HDDETECTED_THD** — Configures the HD Level Detected Threshold. (default 0x1e1e; range 1-100)
- `0x3400` **FM_ACF_INTERRUPT_SOURCE** — FM_ACF_INTERRUPT_SOURCE Enables the ACF interrupt sources. When one of the interrupts is enabled, the ACFINT bit of the status word will be set when the controlling indicator crosses the threshold set its ACF threshold property. (default 0)
- `0x3401` **FM_ACF_SOFTMUTE_THRESHOLD** — FM_ACF_SOFTMUTE_THRESHOLD sets the softmute interrupt threshold. When softmute attenuation rises above the level set by this property the SMUTE_INT bit of the FM_ACF_STATUS command will be set. (default 31; units dB)
- `0x3402` **FM_ACF_HIGHCUT_THRESHOLD** — FM_ACF_HIGHCUT_THRESHOLD sets the high cut interrupt threshold. When the cutoff frequency falls below this threshold, the HIGHCUT_INT bit of FM_ACF_STATUS command will be asserted. (default 0; units 100Hz)
- `0x3403` **FM_ACF_BLEND_THRESHOLD** — FM_ACF_BLEND_THRESHOLD sets the Stereo Blend interrupt threshold. When the stereo separation falls below this threshold the BLEND_INT bit of the FM_ACF_STATUS command will be set. (default 0; units dB)
- `0x3404` **FM_ACF_SOFTMUTE_TOLERANCE** — FM_ACF_SOFTMUTE_TOLERANCE sets the distance from the final softmute value that triggers the softmute convergence flag. Convergence is indicated by setting the SMUTE_CONV flag in the FM_ACF_STATUS command reply. (default 2; units dB; range 0-31)
- `0x3405` **FM_ACF_HIGHCUT_TOLERANCE** — FM_ACF_HIGHCUT_TOLERANCE Sets the distance from the final high cut freq that triggers the high cut convergence flag. Convergence is indicated by a setting HIGHCUT_CONV flag of FM_ACF_STATUS command reply. (default 20; units 100Hz; range 0-200)
- `0x3406` **FM_ACF_BLEND_TOLERANCE** — FM_ACF_BLEND_TOLERANCE sets the distance from the final blend state that triggers the blend convergence flag. Blend convergence is indicated by setting the BLEND_CONV flag of the FM_ACF_STATUS command. (default 5; units dB; range 0-100)
- `0x3500` **FM_SOFTMUTE_SNR_LIMITS** — FM_SOFTMUTE_SNR_LIMITS sets the SNR limits for soft mute attenuation. (default 0x0602; units dB; range -20-64)
- `0x3501` **FM_SOFTMUTE_SNR_ATTENUATION** — FM_SOFTMUTE_SNR_ATTENUATION sets the SNR attenuation limits. (default 0x0008; units dB; range 0-31)
- `0x3502` **FM_SOFTMUTE_SNR_ATTACK_TIME** — FM_SOFTMUTE_SNR_ATTACK_TIME sets the attack time to mute the audio. The attack time is the time it takes the softmute attenuation to go from YMIM to YMAX if the SNR made a step change from XMAX to XMIN. (default 16; units ms; range 16–65535)
- `0x3503` **FM_SOFTMUTE_SNR_RELEASE_TIME** — FM_SOFTMUTE_SNR_RELEASE_TIME Sets the release time to unmute the audio. The release time is the time it takes the softmute attenuation to go from YMAX to YMIN if the SNR made a step change from XMIN to XMAX. (default 4000; units ms; range 16–65535)
- `0x3600` **FM_HIGHCUT_RSSI_LIMITS** — FM_HIGHCUT_RSSI_LIMITS sets the RSSI limits for RSSI based high cut. (default 0x0C06; units dBµV; range -20-120)
- `0x3601` **FM_HIGHCUT_RSSI_CUTOFF_FREQ** — FM_HIGHCUT_RSSI_CUTOFF_FREQ sets the audio cutoff frequencies for RSSI based high cut. (default 0xC828; units 100Hz; range 0-200)
- `0x3602` **FM_HIGHCUT_RSSI_ATTACK_TIME** — FM_HIGHCUT_RSSI_ATTACK_TIME sets the transition time for which RSSI based high cut lowers the cutoff frequency. The transition time is the time it will take the cutoff frequency to go from YMAX to YMIN assuming RSSI makes a step change from XMAX to XMIN. (default 16; units ms)
- `0x3603` **FM_HIGHCUT_RSSI_RELEASE_TIME** — FM_HIGHCUT_RSSI_RELEASE_TIME sets the transition time for which RSSI based high cut increases the cutoff frequency. The transition time is the time it will take the cutoff frequency to go from YMIN to YMAX assuming RSSI makes a step change from XMIN to XMAX. (default 4000; units ms)
- `0x3604` **FM_HIGHCUT_SNR_LIMITS** — FM_HIGHCUT_SNR_LIMITS sets the SNR limits for SNR based high cut. (default 0x0903; units dB; range -20-64)
- `0x3605` **FM_HIGHCUT_SNR_CUTOFF_FREQ** — FM_HIGHCUT_SNR_CUTOFF_FREQ sets the audio cutoff frequencies for SNR based high cut. (default 0xc828; units 100Hz; range 0-200)
- `0x3606` **FM_HIGHCUT_SNR_ATTACK_TIME** — FM_HIGHCUT_SNR_ATTACK_TIME sets the transition time for which SNR based high cut lowers the cutoff frequency. The transition time is the time it will take the cutoff frequency to go from YMAX to YMIN assuming SNR makes a step change from XMAX to XMIN. (default 16; units ms)
- `0x3607` **FM_HIGHCUT_SNR_RELEASE_TIME** — FM_HIGHCUT_SNR_RELEASE_TIME sets the transition time for which SNR based high cut increases the cutoff frequency. The transition time is the time it will take the cutoff frequency to go from YMIN to YMAX assuming SNR makes a step change from XMIN to XMAX. (default 4000; units ms)
- `0x3608` **FM_HIGHCUT_MULTIPATH_LIMITS** — FM_HIGHCUT_MULTIPATH_LIMITS sets the multipath limits for multipath controlled stereo separation. The limits are in % AM modulation at 1kHz. (default 0x2D3C; units %; range 0–255)
- `0x3609` **FM_HIGHCUT_MULTIPATH_CUTOFF_FREQ** — FM_HIGHCUT_MULTIPATH_CUTOFF_FREQ sets the audio cutoff frequencies for the multipath based high cut. (default 0xc828; units 100Hz; range 0-200)
- `0x360A` **FM_HIGHCUT_MULTIPATH_ATTACK_TIME** — FM_HIGHCUT_MULTIPATH_ATTACK_TIME sets the transition time for which multipath based high cut lowers the cutoff frequency. The transition time is the time it will take the cutoff frequency to go from YMAX to YMIN assuming multipath makes a step change from XMAX to XMIN. (default 16; units ms)
- `0x360B` **FM_HIGHCUT_MULTIPATH_RELEASE_TIME** — FM_HIGHCUT_MULTIPATH_RELEASE_TIME sets the transition time for which multipath based high cut increases the cutoff frequency. The transition time is the time it will take the cutoff frequency to go from YMIN to YMAX assuming multipath makes a step change from XMIN to XMAX. (default 4000; units ms)
- `0x3700` **FM_BLEND_RSSI_LIMITS** — FM_BLEND_RSSI_LIMITS sets the RSSI limits for RSSI controlled stereo separation. (default 0x2010; units dBµV; range -20-120)
- `0x3702` **FM_BLEND_RSSI_ATTACK_TIME** — FM_BLEND_RSSI_ATTACK_TIME ms (default 16; units ms)
- `0x3703` **FM_BLEND_RSSI_RELEASE_TIME** — FM_BLEND_RSSI_RELEASE_TIME sets the mono to stereo release time for RSSI based blend. The release time is the time it will take the stereo separation to go from YMIN to YMAX assuming RSSI makes a step change from XMIN to XMAX. (default 4000; units ms)
- `0x3704` **FM_BLEND_SNR_LIMITS** — FM_BLEND_SNR_LIMITS sets the SNR limits for SNR controlled stereo separation. (default 0x180F; units dB; range -20-64)
- `0x3706` **FM_BLEND_SNR_ATTACK_TIME** — FM_BLEND_SNR_ATTACK_TIME sets the stereo to mono attack time for SNR based blend. The attack time is the time it will take the stereo separation to go from YMAX to YMIN assuming SNR makes a step change from XMAX to XMIN. (default 16; units ms)
- `0x3707` **FM_BLEND_SNR_RELEASE_TIME** — FM_BLEND_SNR_RELEASE_TIME sets the mono to stereo release time for SNR based blend. The release time is the time it will take the stereo separation to go from YMIN to YMAX assuming SNR makes a step change from XMIN to XMAX. (default 4000; units ms)
- `0x3708` **FM_BLEND_MULTIPATH_LIMITS** — FM_BLEND_MULTIPATH_LIMITS sets the multipath limits for multipath controlled stereo separation. The limits are in % AM modulation at 1kHz. (default 0x2D3C; units %; range 0–255)
- `0x370A` **FM_BLEND_MULTIPATH_ATTACK_TIME** — FM_BLEND_MULTIPATH_ATTACK_TIME sets the stereo to mono attack time for multi-path based blend. The attack time is the time it will take the stereo separation to go from YMAX to YMIN assuming multipath makes a step change from XMIN to XMAX. (default 16; units ms)
- `0x370B` **FM_BLEND_MULTIPATH_RELEASE_TIME** — FM_BLEND_MULTIPATH_RELEASE_TIME sets the mono to stereo release time for multi-path based blend. The release time is the time it will take the stereo separation to go from YMIN to YMAX assuming multipath makes a step change from XMAX to XMIN. (default 4000; units ms)
- `0x3900` **FM_AUDIO_DE_EMPHASIS** — FM_AUDIO_DE_EMPHASIS property sets the FM Receive de-emphasis to 50 or 75 us. (default 0)
- `0x3C00` **FM_RDS_INTERRUPT_SOURCE** — FM_RDS_INTERRUPT_SOURCE configures interrupt related to RDS. (default 0x0000)
- `0x3C01` **FM_RDS_INTERRUPT_FIFO_COUNT** — FM_RDS_INTERRUPT_FIFO_COUNT sets the minimum number of RDS groups stored in the RDS FIFO before RDSRECV is set. RDSRECV is disabled if set to 0. (default 0x0000; range 0-25)
- `0x3C02` **FM_RDS_CONFIG** — FM_RDS_CONFIG configures RDS settings to enable RDS processing (RDSEN) and set RDS block error thresholds. When a RDS Group is received, all block errors must be less than or equal to the associated block error threshold for the group to be stored in the RDS FIFO. (default 0x0000)
- `0x3C03` **FM_RDS_CONFIDENCE** — FM_RDS_CONFIDENCE sets the confidence threshold for deciding if each RDS block is valid. (default 0x1111; range 1-15)
- `0x8100` **DIGITAL_SERVICE_INT_SOURCE** — DIGITAL_SERVICE_INT_SOURCE configures which digital service events will set the DSRVINT status bit. When one of the bits described below is set, the corresponding event will cause the DSRVINT bit of the status word to be set. (default 0x0000)
- `0x8101` **DIGITAL_SERVICE_RESTART_DELAY** — DIGITAL_SERVICE_RESTART_DELAY sets the delay time (in milliseconds) to restart digital service. When the system recovers from an acquisition loss, the service that had previously been started will be restarted after this delay. (default 8000; units ms; range 100-65535)
- `0x9101` **HD_BLEND_OPTIONS** — HD_BLEND_OPTIONS provides options to control HD/analog audio blend behavior. This property is only valid for Hybrid (non-All-Digital HD) Broadcasts. (default 0x000A)
- `0x9102` **HD_BLEND_ANALOG_TO_HD_TRANSITION_TIME** — HD_BLEND_ANALOG_TO_HD_TRANSITION_TIME sets the amount of time it takes in ms to blend from analog to HD. This property only applies to primary service channel. (default 750; units ms)
- `0x9103` **HD_BLEND_HD_TO_ANALOG_TRANSITION_TIME** — HD_BLEND_HD_TO_ANALOG_TRANSITION_TIME sets the amount of time it takes in ms to blend from HD to analog. This property only applies to primary service channel. (default 100; units ms)
- `0x9106` **HD_BLEND_DYNAMIC_GAIN** — HD_BLEND_DYNAMIC_GAIN sets the digital audio dynamic linear scaling factor. Setting DGAIN_OVERRIDE bit to 1 will override the broadcaster specified digital gain. (default 0; units Q0.7; range –128 to 127)
- `0x9109` **HD_BLEND_BLEND_DECISION_ANALOG_TO_DIGITAL_THD** — This property defines the analog to digital blend threshold. When Cd/No exceeds this threshold for HD_BLEND_BLEND_DECISION_ANALOG_TO_DIGITAL_DELAY milliseconds, blend to digital. (default 58; units dBHz)
- `0x910A` **HD_BLEND_BLEND_DECISION_ANALOG_TO_DIGITAL_DELAY** — This property defines the analog to digital blend delay. When Cd/No exceeds HD_BLEND_BLEND_DECISION_ANALOG_TO_DIGITAL_THD for the given period of milliseconds, blend to digital. (default 5000; units ms)
- `0x910B` **HD_BLEND_SERV_LOSS_RAMP_UP_TIME** — HD_BLEND_SERV_LOSS_RAMP_UP_TIME sets the audio service re-acquisition unmute time in ms. When audio is acquired the audio will ramp up to full level in the time programed. (default 750; units ms; range 50-2000)
- `0x910C` **HD_BLEND_SERV_LOSS_RAMP_DOWN_TIME** — HD_BLEND_SERV_LOSS_RAMP_DOWN_TIME sets the audio service lost mute time in ms. When audio is lost the audio will ramp down to mute in the time programed. (default 250; units ms; range 50-2000)
- `0x910D` **HD_BLEND_SERV_LOSS_NOISE_RAMP_UP_TIME** — HD_BLEND_SERV_LOSS_NOISE_RAMP_UP_TIME sets the comfort noise unmute time in ms. When audio is lost and the comfort noise is enabled the noise will ramp up to the level specified HD_BLEND_SERV_LOSS_NOISE_LEVEL in the time programed. (default 1000; units ms; range 50-2000)
- `0x910E` **HD_BLEND_SERV_LOSS_NOISE_RAMP_DOWN_TIME** — HD_BLEND_SERV_LOSS_NOISE_RAMP_DOWN_TIME sets the comfort noise mute time in ms. When audio is acquired and comfort noise is enabled the noise will ramp down to 0 in the time programed. (default 250; units ms; range 50-2000)
- `0x910F` **HD_BLEND_SERV_LOSS_NOISE_LEVEL** — HD_BLEND_SERV_LOSS_NOISE_LEVEL sets the unmuted comfort noise level as a fractional number between 0 and 1. Where 0 is off and 0x3FFF is 0dBFS. (default 512; range 0-16383)
- `0x9110` **HD_BLEND_SERV_LOSS_NOISE_DAAI_THRESHOLD** — HD_BLEND_SERV_LOSS_NOISE_DAAI_THRESHOLD sets the DAAI level below which comfort noise will engage and audio will ramp down (if loss ramping is enabled). A lower setting of this property will result more thrashing between audio and noise in poor signal conditions. (default 40; range 0-60)
- `0x9111` **HD_BLEND_SERV_LOSS_NOISE_AUDIO_START_DELAY** — HD_BLEND_SERV_LOSS_NOISE_AUDIO_START_DELAY sets the amount of time in 40ms increments to delay the audio once audio is available and DAAI is greater then the value set by HD_BLEND_SERV_LOSS_NOISE_DAAI_THRESHOLD. A lower setting of this property will result more thrashing between audio and noise in poor signal condit... (default 4; units 40ms; range 0-200)
- `0x9112` **HD_BLEND_SERV_SWITCH_RAMP_UP_TIME** — HD_BLEND_SERV_SWITCH_RAMP_UP_TIME sets the service switching unmute time in ms. The service switching ramp feature is enabled using the HD_BLEND_OPTIONS property. (default 184; units ms; range 50-2000)
- `0x9113` **HD_BLEND_SERV_SWITCH_RAMP_DOWN_TIME** — HD_BLEND_SERV_SWITCH_RAMP_DOWN_TIME sets the service switching mute time in ms. The service switching ramp feature is enabled using the HD_BLEND_OPTIONS property. (default 184; units ms; range 50-2000)
- `0x9200` **HD_DIGRAD_INTERRUPT_SOURCE** — HD_DIGRAD_INTERRUPT_SOURCE configures interrupts related to digital receiver (HD_DIGRAD_STATUS). (default 0)
- `0x9201` **HD_DIGRAD_CDNR_LOW_THRESHOLD** — HD_DIGRAD_CDNR_LOW_THRESHOLD sets the CDNR level (in dB) below which the CDNRLINT interrupt will occur. (default 0; units dB)
- `0x9202` **HD_DIGRAD_CDNR_HIGH_THRESHOLD** — HD_DIGRAD_CDNR_HIGH_THRESHOLD sets the CDNR level (in dB) above which the CDNRHINT interrupt will occur. (default 127; units dB)
- `0x9300` **HD_EVENT_INTERRUPT_SOURCE** — HD_EVENT_INTERRUPT_SOURCE property configures interrupts related to HD Events (see DEVENTINT status bit). (default 0)
- `0x9301` **HD_EVENT_SIS_CONFIG** — HD_EVENT_SIS_CONFIG configures which basic SIS information is returned by the HD_GET_STATION_INFO command BASICSIS option. Takes effect at tune time. (default 0x0017)
- `0x9302` **HD_EVENT_ALERT_CONFIG** — HD_EVENT_ALERT_CONFIG configures HD alerts. Alert information is returned by the HD_GET_ALERT_MSG command. (default 0x0001)
- `0x9500` **HD_PSD_ENABLE** — HD_PSD_ENABLE sets which audio services will provide program service data. The PSD data is forwarded through the data service DSRV interface. (default 0)
- `0x9501` **HD_PSD_FIELD_MASK** — This property sets which PSD fields will be decoded and available via HD_GET_PSD_DECODE. (default 0xFFFF)
- `0x9700` **HD_AUDIO_CTRL_FRAME_DELAY** — HD_AUDIO_CTRL_FRAME_DELAY controls the value of the delay of decoded digital audio samples relative to the output of the audio quality indicator. For CODEC modes 0 and 2, the actual delay value is a sum of this parameter and the Digital Audio Delay for a given codec mode, see HD_CODEC properties, The maximum hold-of... (default 6; units frames; range 4-21)
- `0x9701` **HD_AUDIO_CTRL_PROGRAM_LOSS_THRESHOLD** — HD_AUDIO_CTRL_PROGRAM_LOSS_THRESHOLD controls the duration before reverting to MPS audio after an SPS audio program is removed or lost. The same value applies to all SPS audio programs. (default 0; units frames; range 0-14)
- `0x9702` **HD_AUDIO_CTRL_BALL_GAME_ENABLE** — HD_AUDIO_CTRL_BALL_GAME_ENABLE selects the audio output for hybrid waveforms when the TX Blend Control Status (BCTL_EN of HD_DIGRAD_STATUS) bits are set to 01 (i.e., ballgame mode). Since analog diversity delay is not applied by the transmitter in this state, the receiver must disable audio blending and force either... (default 1; range 0-1)
- `0x9900` **HD_CODEC_MODE_0_BLEND_THRESHOLD** — HD_CODEC_MODE_0_BLEND_THRESHOLD sets the threshold for determining when to blend between the digital HD stream and the analog stream for codec mode 0. The same threshold applies to all audio programs that utilize codec mode 0. (default 3)
- `0x9901` **HD_CODEC_MODE_0_SAMPLES_DELAY** — HD_CODEC_MODE_0_SAMPLES_DELAY property is used to perform fine time alignment between the HD digital audio and analog audio to ensure phase aligned blending. Each unit of sample delay represents approximately 22.7us and this delay is applied to the HD audio. (default 3693; units audio samples)
- `0x9902` **HD_CODEC_MODE_0_BLEND_RATE** — HD_CODEC_MODE_0_BLEND_RATE configures the hysteresis in the blending process. Blend hysteresis has two main components affected by this property; a step size for the analog hold duration, and the digital duration required for state reset. (default 1; units s; range 1-8)
- `0x9903` **HD_CODEC_MODE_2_BLEND_THRESHOLD** — HD_CODEC_MODE_2_BLEND_THRESHOLD sets the threshold for determining when to blend between the digital HD stream and the analog stream for codec mode 2. The same threshold applies to all audio programs that utilize codec mode 2. (default 3)
- `0x9904` **HD_CODEC_MODE_2_SAMPLES_DELAY** — HD_CODEC_MODE_2_SAMPLES_DELAY property is used to perform fine time alignment between the HD digital audio and analog audio to ensure phase aligned blending. Each unit of sample delay represents approximately 22.7us and this delay is applied to the HD audio. (default 0; units audio samples)
- `0x9905` **HD_CODEC_MODE_2_BLEND_RATE** — HD_CODEC_MODE_2_BLEND_RATE configures the hysteresis in the blending process. Blend hysteresis has two main components affected by this property; a step size for the analog hold duration, and the digital duration required for state reset. (default 1; units s; range 1-8)
- `0x9906` **HD_CODEC_MODE_10_BLEND_THRESHOLD** — HD_CODEC_MODE_10_BLEND_THRESHOLD sets the threshold for determining when to blend between the digital HD stream and the analog stream for codec mode 10. The same threshold applies to all audio programs that utilize codec mode 10. (default 3)
- `0x9907` **HD_CODEC_MODE_10_SAMPLES_DELAY** — HD_CODEC_MODE_10_SAMPLES_DELAY property is used to perform fine time alignment between the HD digital audio and analog audio to ensure phase aligned blending. Each unit of sample delay represents approximately 22.7us and this delay is applied to the HD audio. (default 0; units audio samples)
- `0x9908` **HD_CODEC_MODE_10_BLEND_RATE** — HD_CODEC_MODE_10_BLEND_RATE configures the hysteresis in the blending process. Blend hysteresis has two main components affected by this property; a step size for the analog hold duration, and the digital duration required for state reset. (default 1; units s; range 1-8)
- `0x9909` **HD_CODEC_MODE_13_BLEND_THRESHOLD** — HD_CODEC_MODE_13_BLEND_THRESHOLD sets the threshold for determining when to blend between the digital HD stream and the analog stream for codec mode 13. The same threshold applies to all audio programs that utilize codec mode 13. (default 3)
- `0x990A` **HD_CODEC_MODE_13_SAMPLES_DELAY** — HD_CODEC_MODE_13_SAMPLES_DELAY property is used to perform fine time alignment between the HD digital audio and analog audio to ensure phase aligned blending. Each unit of sample delay represents approximately 22.7us and this delay is applied to the HD audio. (default 0; units audio samples)
- `0x990B` **HD_CODEC_MODE_13_BLEND_RATE** — HD_CODEC_MODE_13_BLEND_RATE configures the hysteresis in the blending process. Blend hysteresis has two main components affected by this property; a step size for the analog hold duration, and the digital duration required for state reset. (default 1; units s; range 1-8)
- `0x990C` **HD_CODEC_MODE_1_BLEND_THRESHOLD** — HD_CODEC_MODE_1_BLEND_THRESHOLD sets the threshold for determining when to blend between the digital HD stream and the analog stream for codec mode 1. The same threshold applies to all audio programs that utilize codec mode 1. (default 3)
- `0x990D` **HD_CODEC_MODE_1_SAMPLES_DELAY** — HD_CODEC_MODE_1_SAMPLES_DELAY property is used to perform fine time alignment between the HD digital audio and analog audio to ensure phase aligned blending. Each unit of sample delay represents approximately 22.7us and this delay is applied to the HD audio. (default 0; units audio samples)
- `0x990E` **HD_CODEC_MODE_1_BLEND_RATE** — HD_CODEC_MODE_1_BLEND_RATE configures the hysteresis in the blending process. Blend hysteresis has two main components affected by this property; a step size for the analog hold duration, and the digital duration required for state reset. (default 1; units s; range 1-8)
- `0x990F` **HD_CODEC_MODE_3_BLEND_THRESHOLD** — HD_CODEC_MODE_3_BLEND_THRESHOLD sets the threshold for determining when to blend between the digital HD stream and the analog stream for codec mode 3. The same threshold applies to all audio programs that utilize codec mode 3. (default 3)
- `0x9910` **HD_CODEC_MODE_3_SAMPLES_DELAY** — HD_CODEC_MODE_3_SAMPLES_DELAY property is used to perform fine time alignment between the HD digital audio and analog audio to ensure phase aligned blending. Each unit of sample delay represents approximately 22.7us and this delay is applied to the HD audio. (default 0; units audio samples)
- `0x9911` **HD_CODEC_MODE_3_BLEND_RATE** — HD_CODEC_MODE_3_BLEND_RATE configures the hysteresis in the blending process. Blend hysteresis has two main components affected by this property; a step size for the analog hold duration, and the digital duration required for state reset. (default 1; units s; range 1-8)
- `0x9A00` **HD_SERVICE_MODE_CONTROL_MP11_ENABLE** — This property Enables MP11 mode support. If MP11 support is disabled using this property the receiver will fall back to MP3 mode of operation when tuned to a station that is transmitting the MP11 subcarriers. (default 0x0000)
- `0x9B00` **HD_EZBLEND_ENABLE** — This property enables and disables HD EZ blend. (default 0)
- `0x9B01` **HD_EZBLEND_MPS_BLEND_THRESHOLD** — This property sets the threshold for determining when to blend between digital audio and analog audio for Hybrid MPS. (default 3; range 0-7)
- `0x9B02` **HD_EZBLEND_MPS_BLEND_RATE** — This property configures the hysteresis in the blending process for Hybrid MPS. (default 3)
- `0x9B03` **HD_EZBLEND_MPS_SAMPLES_DELAY** — This property is used to perform audio alignment between analog and Hybrid MPS digital audio. (default 3693)
- `0x9B04` **HD_EZBLEND_SPS_BLEND_THRESHOLD** — This property sets the threshold for determining when to blend between digital audio and mute for SPS programs as well as All Digital MPS programs. (default 4; range 0-7)
- `0x9B05` **HD_EZBLEND_SPS_BLEND_RATE** — This property configures the hysteresis in the blending process for SPS programs and All Digital MPS programs. (default 1; range 1-8)
- `0xE800` **HD_TEST_BER_CONFIG** — HD_TEST_BER_CONFIG Enables the HD BER test. The HD BER test requires a special test vector (IB_FMr208c_e1wfc204 for FMHD, IB_AMr208a_e1awfb00 for AMHD). (default 0)
- `0xE801` **HD_TEST_DEBUG_AUDIO** — HD_TEST_DEBUG_AUDIO is used to put the DAC audio output in to a special test mode for debug purposes. This is typically used for performing time alignment between the analog audio and the HD audio. (default 0; range 0-15)
- `0x1710` **DAB_TUNE_FE_VARM** — DAB_TUNE_FE_VARM DAB/DMB Front End Varactor configuration slope (x 1000) which has been calculated for a particular board design. Both DAB_TUNE_FE_VARB and DAB_TUNE_FE_VARM must be configured. (default 0)
- `0x1711` **DAB_TUNE_FE_VARB** — DAB_TUNE_FE_VARB DAB/DMB Front End Varactor configuration intercept which has been calculated for a particular board design. Both DAB_TUNE_FE_VARB and DAB_TUNE_FE_VARM must be configured. (default 0)
- `0x1712` **DAB_TUNE_FE_CFG** — Additional configuration options for the front end. These take effect upon DAB_TUNE_FREQ. (default 0x0001)
- `0xB000` **DAB_DIGRAD_INTERRUPT_SOURCE** — DAB_DIGRAD_INERRUPT_SOURCE configures interrupts related to digital receiver (DAB_DIGRAD_STATUS). (default 0)
- `0xB001` **DAB_DIGRAD_RSSI_HIGH_THRESHOLD** — DAB_DIGRAD_RSSI_HIGH_THRESHOLD sets the high threshold, which triggers the DIGRAD interrupt if the RSSI is above this threshold. (default 127)
- `0xB002` **DAB_DIGRAD_RSSI_LOW_THRESHOLD** — DAB_DIGRAD_RSSI_LOW_THRESHOLD sets the low threshold, which triggers the DIGRAD interrupt if the RSSI is below this threshold. (default -128)
- `0xB200` **DAB_VALID_RSSI_TIME** — DAB_VALID_RSSI_TIME sets the time in ms to allow the RSSI metric to settle before evaluating its validity during tune. If RSSI does not exceed DAB_VALID_RSSI_THRESHOLD by this time the tune will be aborted, and STC will be set, and the tune will be flagged as invalid. (default 30; units ms; range 0-63)
- `0xB201` **DAB_VALID_RSSI_THRESHOLD** — DAB_VALID_RSSI_THRESHOLD sets the RSSI threshold for a valid DAB Seek/Tune. If the desired channel RSSI is above this threshold, then it is considered valid. (default 12; units dBµV)
- `0xB202` **DAB_VALID_ACQ_TIME** — DAB_VALID_ACQ_TIME sets the time in ms to wait for acquisition before evaluating acquisition validity during tune. If system has not fully acquired by this time the tune will be aborted, STC will be set, and the tune will be flagged as invalid. (default 2000; units ms; range 0-2047)
- `0xB203` **DAB_VALID_SYNC_TIME** — DAB_VALID_SYNC_TIME sets the time in ms to wait for synchronization during tune. If the system has not synchronized by this time the tune will be aborted, STC will be set, and the tune will be flagged as invalid. (default 1200; units ms; range 0-2047)
- `0xB204` **DAB_VALID_DETECT_TIME** — DAB_VALID_DETECT_TIME sets the time in ms to wait for fast detect during tune. If the system has not detected by this time the tune will be aborted, STC will be set, and the tune will be flagged as invalid. (default 35; units ms; range 0-2047)
- `0xB300` **DAB_EVENT_INTERRUPT_SOURCE** — DAB_EVENT_INTERRUPT_SOURCE configures which dab events will set the DEVENTINT status bit. When one of the bits described below is set, the corresponding event will cause the DEVENTINT bit of the status word will be set. (default 0)
- `0xB301` **DAB_EVENT_MIN_SVRLIST_PERIOD** — DAB_EVENT_MIN_SVRLIST_PERIOD configures how often service list notifications can occur in units of 100ms. This property is used to reduce the number of service list update notifications received at initial tune when the service list is updated very frequently. (default 10; units 100ms)
- `0xB302` **DAB_EVENT_MIN_SVRLIST_PERIOD_RECONFIG** — DAB_EVENT_MIN_SVRLIST_PERIOD_RECONFIG configures how often service list notifications can occur in units of 100ms during reconfiguration. This property is used to reduce the number of service list update notifications received at initial tune when the service list is updated very frequently. (default 10; units 100ms)
- `0xB303` **DAB_EVENT_MIN_FREQINFO_PERIOD** — DAB_EVENT_MIN_FREQINFO_PERIOD configures how often frequency info notifications can occur in units of 100ms. This property is used to reduce the number of frequency info update notifications received at initial tune when frequency info is updated very frequently. (default 5; units 100ms)
- `0xB400` **DAB_XPAD_ENABLE** — DAB_PAD_ENABLE selects which PAD application data will be forwarded to the host when available. When an audio service is playing one of it's audio components this property is used to select which PAD services are forwarded to the host for decoding. (default 1)
- `0xB401` **DAB_DRC_OPTION** — DAB_DRC_OPTION defines option to apply DRC (dynamic range control) gain. DRC is a dynamic range control method defined for DAB. (default 0)
- `0xB500` **DAB_CTRL_DAB_MUTE_ENABLE** — DAB_MUTE_ENABLE enables the feature of hard muting audio when signal level is low. (default 1; units value; range 0-1)
- `0xB501` **DAB_CTRL_DAB_MUTE_SIGNAL_LEVEL_THRESHOLD** — DAB_MUTE_QUALITY_THRESHOLD set the threshold to mute audio when signal level is low. (default 98; units %; range 0-100)
- `0xB502` **DAB_CTRL_DAB_MUTE_WIN_THRESHOLD** — DAB_MUTE_WIN_THRESHOLD set the threshold to mute audio. (default 1000; units ms)
- `0xB503` **DAB_CTRL_DAB_UNMUTE_WIN_THRESHOLD** — DAB_UNMUTE_WIN_THRESHOLD set the threshold to unmute audio. (default 1500; units ms)
- `0xB504` **DAB_CTRL_DAB_MUTE_SIGLOSS_THRESHOLD** — DAB_MUTE_SIGLOSS_THRESHOLD set the threshold to mute audio when signal is loss. (default 6; units dBµV)
- `0xB505` **DAB_CTRL_DAB_MUTE_SIGLOW_THRESHOLD** — DAB_MUTE_SIGLOW_THRESHOLD set the SNR threshold. The fic_quality based audio mute operation only engages when signal SNR is below this threshold. (default 9; units dB)
- `0xE800` **DAB_TEST_BER_CONFIG** — DAB_TEST_BER_CONFIG sets up and enables the DAB BER test. The test is enabled by transitioning the ENABLE bit from 0 to 1. (default 0; range 0-15)
- `0x0500` **AM_AVC_MIN_GAIN** — AM_AVC_MIN_GAIN Sets the minimum gain the AVC can have. The minimum gain value is given by MINGAIN = g * 1024/6.02 or MINGAIN = g * 170 where g is the desired minimum AVC gain in dB. (default -2048; range -4096-3061 - -24dB to +18dB)
- `0x0501` **AM_AVC_MAX_GAIN** — AM_AVC_MAX_GAIN sets the maximum gain the AVC can have. The max gain value is given by MAXGAIN = g * 1024/6.02 or MAXGAIN = g * 170 where g is the desired maximum AVC gain in dB. (default 10220; range 0-32767 - 0 to 193dB)
- `0x2200` **AM_CHBW_SQ_LIMITS** — Sets the SNR/RSSI level in dB at which the maximum and minimum channel bandwidth will be applied. The maximum and minimum bandwidth is defined in the AM_CHBW_SQ_CHBW property. (default 0x1E0F; range –128 to 127)
- `0x2201` **AM_CHBW_SQ_CHBW** — Sets the SNR/RSSI controled maximum and minimum channel bandwidth in units of 100Hz. NOTE: To force the channel filter bandwidth to a set value, set the min and max to the same value (default 0x2314)
- `0x2202` **AM_CHBW_SQ_WIDENING_TIME** — AM_CHBW_SQ_WIDENING_TIME sets the time required in ms for the channel filter to go from minimum bandwidth to maximum bandwidth. The minimum and maximum bandwidths are defined in the AM_CHBW_SQ_CHBW property. (default 2048)
- `0x2203` **AM_CHBW_SQ_NARROWING_TIME** — AM_CHBW_SQ_NARROWING_TIME sets the time required in ms for the channel filter to go from maximum bandwidth to minimum bandwidth. The minimum and maximum bandwidths are defined in the AM_CHBW_SQ_CHBW property. (default 16)
- `0x2204` **AM_CHBW_OVERRIDE_BW** — AM_CHBW_OVERRIDE_BW is used to override the automatically controlled channel filter setting. Setting the proerty to a non-zero value will cause the override to take effect. (default 0)
- `0x4100` **AM_SEEK_BAND_BOTTOM** — AM_SEEK_BAND_BOTTOM sets the lower seek boundary of the AM band in multiples of 1kHz. See AM_SEEK_START. (default 520; units 1kHz; range 520-1710 - AM)
- `0x4101` **AM_SEEK_BAND_TOP** — AM_SEEK_BAND_TOP sets the upper seek boundary of the AM band in multiples of 1kHz. See AM_SEEK_START. (default 1710; units 1kHz; range 520-1710 - AM)
- `0x4102` **AM_SEEK_FREQUENCY_SPACING** — AM_SEEK_FREQUENCY_SPACING sets the frequency spacing for the AM band in multiples of 1kHz when performing a seek. (default 10; units 1kHz; range 1-31)
- `0x4200` **AM_VALID_MAX_TUNE_ERROR** — AM_VALID_MAX_TUNE_ERROR sets the maximum freq error allowed in units of bppm before setting the AFC rail indicator (AFCRL). This will take effect on the next tune. (default 75; units bppm)
- `0x4201` **AM_VALID_RSSI_TIME** — AM_VALID_RSSI_TIME sets the amount of time in ms to allow the RSSI/ISSI metrics to settle before evaluating. The reliability of the valid bit for identifying valid stations relies on this parameter being set properly. (default 8; units ms; range 0-63 - Specified in units ms.)
- `0x4202` **AM_VALID_RSSI_THRESHOLD** — Sets the RSSI threshold for a valid AM Seek/Tune. If the desired channel RSSI is above this threshold, then it is considered valid. (default 35; units dBµV)
- `0x4203` **AM_VALID_SNR_TIME** — AM_VALID_SNR_TIME sets the amount of time in ms to allow the SNR metric to settle before evaluating. The reliability of the valid bit for identifying valid stations relies on this parameter being set properly. (default 40; units ms; range 17-63 - Specified in units ms.)
- `0x4204` **AM_VALID_SNR_THRESHOLD** — AM_VALID_SNR_THRESHOLD sets the SNR threshold for a valid AM Seek/Tune. If the desired channel SNR is above this threshold, then it is considered valid. (default 4; units dB)
- `0x4205` **AM_VALID_HDLEVEL_THRESHOLD** — Sets the HDLEVEL threshold for AM Seek stop. If the desired channel HDLEVEL threshold is above this threshold, then it is considered valid. (default 0; units %)
- `0x4300` **AM_RSQ_INTERRUPT_SOURCE** — AM_RSQ_INTERRUPT_SOURCE configures interrupt related to Received Signal Quality metrics. See AM_RSQ_STATUS. (default 0)
- `0x4301` **AM_RSQ_SNR_HIGH_THRESHOLD** — AM_RSQ_SNR_HIGH_THRESHOLD sets the high threshold, which triggers the RSQ interrupt if the SNR is above this threshold. (default 127; units dB; range –128 to 127 - Specified in units of dB in 1 dB steps.)
- `0x4302` **AM_RSQ_SNR_LOW_THRESHOLD** — AM_RSQ_SNR_LOW_THRESHOLD sets the low threshold, which triggers the RSQ interrupt if the SNR is below this threshold. (default -128; units dB; range –128 to 127 - Specified in units of dB in 1 dB steps.)
- `0x4303` **AM_RSQ_RSSI_HIGH_THRESHOLD** — AM_RSQ_RSSI_HIGH_THRESHOLD sets the high threshold, which triggers the RSQ interrupt if the RSSI is above this threshold. (default 127; units dBµV; range –128 to 127 - Specified in units of dBµV in 1 dBµV steps.)
- `0x4304` **AM_RSQ_RSSI_LOW_THRESHOLD** — AM_RSQ_RSSI_LOW_THRESHOLD sets the low threshold, which triggers the RSQ interrupt if the RSSI is below this threshold. (default -128; units dBµV; range –128 to 127 - Specified in units of dBµV in 1 dBµV steps.)
- `0x4305` **AM_RSQ_HD_DETECTION** — Number of HD OFDM symbols examined by fast HD detection; value 0 disables fast detect. (default 48; documented active range 20-64)
- `0x4306` **AM_RSQ_HD_LEVEL_TIME_CONST** — HDLEVEL moving-average filter time constant. (default 32; range 1-255)
- `0x4307` **AM_RSQ_HDDETECTED_THD** — High byte is HDDETECTED threshold; low byte is filtered-HDDETECTED threshold. (default 0x1E1E; each threshold range 1-100)
- `0x4400` **AM_ACF_INTERRUPT_SOURCE** — AM_ACF_INTERRUPT_SOURCE Enables the ACF interrupt sources. When one of the interrupts is enabled, the ACFINT bit of the status word will be set when the controlling indicator crosses the threshold set its ACF threshold property. (default 0)
- `0x4401` **AM_ACF_SOFTMUTE_THRESHOLD** — AM_ACF_SOFTMUTE_THRESHOLD sets the softmute interrupt threshold. When softmute attenuation rises above the level set by this property the SMUTE_INT bit of the AM_ACF_STATUS command will be set. (default 31; units dB)
- `0x4402` **AM_ACF_HIGHCUT_THRESHOLD** — AM_ACF_HIGHCUT_THRESHOLD sets the high cut interrupt threshold. When the cutoff frequency falls below this threshold, the HIGHCUT_INT bit of AM_ACF_STATUS command will be asserted. (default 0; units 100Hz)
- `0x4403` **AM_ACF_SOFTMUTE_TOLERANCE** — AM_ACF_SOFTMUTE_TOLERANCE sets the distance from the final softmute value that triggers the softmute convergence flag. Convergence is indicated by setting the SMUTE_CONV flag in the AM_ACF_STATUS command reply. (default 2; units dB; range 0-31)
- `0x4404` **AM_ACF_HIGHCUT_TOLERANCE** — AM_ACF_HIGHCUT_TOLERANCE Sets the distance from the final high cut freq that triggers the high cut convergence flag. Convergence is indicated by a setting HIGHCUT_CONV flag of AM_ACF_STATUS command reply. (default 20; units 100Hz; range 0-200)
- `0x4405` **AM_ACF_CONTROL_SOURCE** — Determines if SNR or RSSI will be used as the controlling metric for ACF features. This will affect all automatically controlled features that are controlled by SNR. (default 0)
- `0x4500` **AM_SOFTMUTE_SQ_LIMITS** — AM_SOFTMUTE_SQ_LIMITS sets the SNR limits for soft mute attenuation. (default 0x0800; units dB; range -20-64)
- `0x4501` **AM_SOFTMUTE_SQ_ATTENUATION** — AM_SOFTMUTE_SQ_ATTENUATION sets the softmute attenuation limits. (default 0x000C; units dB; range 0-31)
- `0x4502` **AM_SOFTMUTE_SQ_ATTACK_TIME** — AM_SOFTMUTE_SQ_ATTACK_TIME sets the attack time to mute the audio. The attack time is the time it takes the softmute attenuation to go from YMIM to YMAX if the SNR made a step change from XMAX to XMIN. (default 120; units ms; range 16–65535)
- `0x4503` **AM_SOFTMUTE_SQ_RELEASE_TIME** — AM_SOFTMUTE_SQ_RELEASE_TIME Sets the release time to unmute the audio. The release time is the time it takes the softmute attenuation to go from YMAX to YMIN if the SNR made a step change from XMIN to XMAX. (default 500; units ms; range 16–65535)
- `0x4600` **AM_HIGHCUT_SQ_LIMITS** — Sets the SNR/RSSI level at which hi-cut begins to band limit. (default 0x0a06; range -20-64)
- `0x4601` **AM_HIGHCUT_SQ_CUTOFF_FREQ** — Sets the minimum and maximum high cut transition frequencies in units of 100Hz. When hi-cut is not engaged, the audio will be band limited to MAX. (default 0x280A)
- `0x4602` **AM_HIGHCUT_SQ_ATTACK_TIME** — Sets the transition time for which high cut lowers the cutoff frequency. (default 16)
- `0x4603` **AM_HIGHCUT_SQ_RELEASE_TIME** — Sets the transition time for which high cut increases the cutoff frequency. (default 2000)
- `0x4800` **AM_DEMOD_AFC_RANGE** — Allows the host to specify the range of the AM AFC in Hz. This allows for wideband AM. (default 0x0000; range 0-6000)
- `0x9120` **HD_BLEND_BWM_CTRL_THRES** — HD_BLEND_BWM_CTRL_THRES sets the signal quality threshold at which bandwidth management begins to engage. (default 0x003C; units dBHz; range 0-100)
- `0x9121` **HD_BLEND_BWM_CTRL_LEVEL** — HD_BLEND_CTRL_LEVEL sets the minimum and maximum bandwidth of digital audio signal in 100Hz. When the signal conditions are above the bandwidth step threshold the audio bandwidth will increment to full level in the attack time programed. (default 0x9628; units Hz)
- `0x9122` **HD_BLEND_BWM_CTRL_RAMP_UP_TIME** — HD_BLEND_BWM_CTRL_RAMP_UP_TIME Sets the transition time for which bandwidth management increases the cutoff frequency (default 5000; units ms)
- `0x9123` **HD_BLEND_BWM_CTRL_RAMP_DOWN_TIME** — HD_BLEND_BWM_CTRL_RAMP_DOWN_TIME Sets the transition time for which bandwidth management lowers the cutoff frequency (default 200; units ms)
- `0x9124` **HD_BLEND_BWM_BLEND_THRES** — HD_BLEND_BWM_BLEND_THRES sets the bandwidth threshold in 100Hz at which digital audio mono to stereo blending transition begins to engage. (default 0x0032; range 0-200)
- `0x9125` **HD_BLEND_BWM_BLEND_LEVEL** — HD_BLEND_BWM_BLEND_LEVEL sets minimum and maximum digital audio stereo separation in dB. (default 0x7F00; units dB)
- `0x9126` **HD_BLEND_BWM_BLEND_RAMP_UP_TIME** — HD_BLEND_BWM_BLEND_RAMP_UP_TIME Sets the transition time for which digital audio is forced to mono form stereo (default 400; units ms; range 1-32767)
- `0x9127` **HD_BLEND_BWM_BLEND_RAMP_DOWN_TIME** — HD_BLEND_BWM_BLEND_RAMP_DOWN_TIME Sets the transition time for which digital audio is blended into stereo from mono (default 16; units ms)
- `0x9F00` **HD_ENHANCED_STREAM_HOLDOFF_CONFIG** — When the ENABLE bit of HD_ENHANCED_STREAM_HOLDOFF_CONFIG is set to 1, then under weak signal conditions, a hold-off is applied to enhanced audio until the signal quality exceeds certain thresholds. These thresholds are set by property HD_ENHANCED_STREAM_HOLDOFF_THRESHOLDS. (default 0)
- `0x9F01` **HD_ENHANCED_STREAM_HOLDOFF_THRESHOLDS** — HD_ENHANCED_STREAM_HOLDOFF_THRESHOLDS sets the C/No thresholds for both hybrid mode and all digital mode enhanced stream hold-off as described in HD_ENHANCED_STREAM_HOLDOFF_CONFIG. Note: When this property is changed, it will not take effect until after the next tune or acquisition command is issued. (default 0x2F2F; units dB-Hz; range 47-80)

## Property bit fields

The property summaries above describe each 16-bit value as a whole. This compact map lists non-reserved AN649 fields so values can be composed offline. Reserved fields are omitted and should be written as zero unless a nearby property note says otherwise.

- `0x0000` **INT_CTL_ENABLE** — `13` `DEVNTIEN`, `7` `CTSIEN`, `6` `ERR_CMDIEN`, `5` `DACQIEN`, `4` `DSRVIEN`, `3` `RSQIEN`, `2` `RDSIEN`, `1` `ACFIEN`, `0` `STCIEN`
- `0x0001` **INT_CTL_REPEAT** — `13` `DEVNTREP`, `5` `DACQREP`, `4` `DSRVREP`, `3` `RSQREP`, `2` `RDSREP`, `1` `ACFREP`, `0` `STCREP`
- `0x0200` **DIGITAL_IO_OUTPUT_SELECT** — `15` `MASTER`
- `0x0201` **DIGITAL_IO_OUTPUT_SAMPLE_RATE** — `15:0` `OUTPUT_SAMPLE_RATE[15:0]`
- `0x0202` **DIGITAL_IO_OUTPUT_FORMAT** — `13:8` `SAMPL_SIZE[5:0]`, `7:4` `SLOT_SIZE[3:0]`
- `0x0203` **DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_1** — `13` `FSLATE_EN`, `12` `FSINV_EN`, `11` `RJUST_EN`, `10` `CLKINV_EN`, `9` `SWAP_EN`, `8` `BITORDER_EN`, `5` `FSLATE`, `4` `FSINV`, `3` `RJUST`, `2` `CLKINV`, `1` `SWAP`, `0` `BITORDER`
- `0x0204` **DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_2** — `14` `FILL_EN`, `9` `SEQEN_EN`, `8` `FSEDGE_EN`, `7:6` `FILL[1:0]`, `1` `SEQEN`, `0` `FSEDGE`
- `0x0205` **DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_3** — `15` `FSH_EN`, `14:0` `FSH[14:0]`
- `0x0206` **DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_4** — `15` `FSL_EN`, `14:0` `FSL[14:0]`
- `0x0300` **AUDIO_ANALOG_VOLUME** — `5:0` `VOL[5:0]`
- `0x0301` **AUDIO_MUTE** — `1:0` `MUTE[1:0]`
- `0x0302` **AUDIO_OUTPUT_CONFIG** — `0` `MONO`
- `0x0800` **PIN_CONFIG_ENABLE** — `15` `INTBOUTEN`, `1` `I2SOUTEN`, `0` `DACOUTEN`
- `0x0900` **WAKE_TONE_ENABLE** — `0` `ENABLE`
- `0x0901` **WAKE_TONE_PERIOD** — `15:0` `PERIOD[15:0]`
- `0x0902` **WAKE_TONE_FREQ** — `15:0` `FREQ[15:0]`
- `0x0903` **WAKE_TONE_AMPLITUDE** — `4:0` `AMP[4:0]`
- `0x1710` **FM_TUNE_FE_VARM** — `15:0` `FE_VARM[15:0]`
- `0x1711` **FM_TUNE_FE_VARB** — `15:0` `FE_VARB[15:0]`
- `0x1712` **FM_TUNE_FE_CFG** — `1` `VHFCAPS`, `0` `VHFSW`
- `0x3100` **FM_SEEK_BAND_BOTTOM** — `15:0` `FMSKFREQL[15:0]`
- `0x3101` **FM_SEEK_BAND_TOP** — `15:0` `FMSKFREQH[15:0]`
- `0x3102` **FM_SEEK_FREQUENCY_SPACING** — `4:0` `FMSKSPACE[4:0]`
- `0x3201` **FM_VALID_RSSI_TIME** — `5:0` `SSIVALTIME[5:0]`
- `0x3202` **FM_VALID_RSSI_THRESHOLD** — `7:0` `FMVALRSSI[7:0]`
- `0x3203` **FM_VALID_SNR_TIME** — `5:0` `SNRVALTIME[5:0]`
- `0x3204` **FM_VALID_SNR_THRESHOLD** — `7:0` `FMVALSNR[7:0]`
- `0x3300` **FM_RSQ_INTERRUPT_SOURCE** — `7` `MULTHINT`, `6` `MULTLINT`, `3` `SNRHINT`, `2` `SNRLINT`, `1` `RSSIHINT`, `0` `RSSILINT`
- `0x3301` **FM_RSQ_SNR_HIGH_THRESHOLD** — `7:0` `SNRH[7:0]`
- `0x3302` **FM_RSQ_SNR_LOW_THRESHOLD** — `7:0` `SNRL[7:0]`
- `0x3303` **FM_RSQ_RSSI_HIGH_THRESHOLD** — `7:0` `RSSIH[7:0]`
- `0x3304` **FM_RSQ_RSSI_LOW_THRESHOLD** — `7:0` `RSSIL[7:0]`
- `0x3307` **FM_RSQ_HD_DETECTION** — `7:0` `SAMPLES[7:0]`
- `0x3308` **FM_RSQ_HD_LEVEL_TIME_CONST** — `7:0` `ing`
- `0x3309` **FM_RSQ_HDDETECTED_THD** — `15:8` `threshold`, `7:0` `ric`
- `0x3400` **FM_ACF_INTERRUPT_SOURCE** — `2` `BLEND_INTEN`, `1` `HIGHCUT_INTEN`, `0` `SMUTE_INTEN`
- `0x3404` **FM_ACF_SOFTMUTE_TOLERANCE** — `4:0` `SMUTE_TOL[4:0]`
- `0x3405` **FM_ACF_HIGHCUT_TOLERANCE** — `7:0` `flag`
- `0x3406` **FM_ACF_BLEND_TOLERANCE** — `6:0` `BLEND_TOL[6:0]`
- `0x3500` **FM_SOFTMUTE_SNR_LIMITS** — `15:8` `XMAX[7:0]`, `7:0` `XMIN[7:0]`
- `0x3501` **FM_SOFTMUTE_SNR_ATTENUATION** — `15:8` `ATTENMIN[7:0]`, `7:0` `ATTENMAX[7:0]`
- `0x3502` **FM_SOFTMUTE_SNR_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x3503` **FM_SOFTMUTE_SNR_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x3600` **FM_HIGHCUT_RSSI_LIMITS** — `15:8` `XMAX[7:0]`, `7:0` `XMIN[7:0]`
- `0x3601` **FM_HIGHCUT_RSSI_CUTOFF_FREQ** — `15:8` `YMAX[7:0]`, `7:0` `YMIN[7:0]`
- `0x3602` **FM_HIGHCUT_RSSI_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x3603` **FM_HIGHCUT_RSSI_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x3604` **FM_HIGHCUT_SNR_LIMITS** — `15:8` `XMAX[7:0]`, `7:0` `XMIN[7:0]`
- `0x3605` **FM_HIGHCUT_SNR_CUTOFF_FREQ** — `15:8` `YMAX[7:0]`, `7:0` `YMIN[7:0]`
- `0x3606` **FM_HIGHCUT_SNR_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x3607` **FM_HIGHCUT_SNR_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x3608` **FM_HIGHCUT_MULTIPATH_LIMITS** — `15:8` `XMAX[7:0]`, `7:0` `XMIN[7:0]`
- `0x3609` **FM_HIGHCUT_MULTIPATH_CUTOFF_FREQ** — `15:8` `YMAX[7:0]`, `7:0` `YMIN[7:0]`
- `0x360A` **FM_HIGHCUT_MULTIPATH_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x360B` **FM_HIGHCUT_MULTIPATH_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x3700` **FM_BLEND_RSSI_LIMITS** — `15:8` `XMAX[7:0]`, `7:0` `XMIN[7:0]`
- `0x3702` **FM_BLEND_RSSI_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x3703` **FM_BLEND_RSSI_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x3704` **FM_BLEND_SNR_LIMITS** — `15:8` `XMAX[7:0]`, `7:0` `XMIN[7:0]`
- `0x3706` **FM_BLEND_SNR_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x3707` **FM_BLEND_SNR_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x3708` **FM_BLEND_MULTIPATH_LIMITS** — `15:8` `XMAX[7:0]`, `7:0` `XMIN[7:0]`
- `0x370A` **FM_BLEND_MULTIPATH_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x370B` **FM_BLEND_MULTIPATH_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x3900` **FM_AUDIO_DE_EMPHASIS** — `1:0` `DE_EMPH[1:0]`
- `0x3C00` **FM_RDS_INTERRUPT_SOURCE** — `4` `RDSTPPTY`, `3` `RDSPI`, `1` `RDSSYNC`, `0` `RDSRECV`
- `0x3C01` **FM_RDS_INTERRUPT_FIFO_COUNT** — `7:0` `DEPTH[7:0]`
- `0x3C02` **FM_RDS_CONFIG** — `7:6` `BLETHB[1:0]`, `5:4` `BLETHCD[1:0]`, `0` `RDSEN`
- `0x8100` **DIGITAL_SERVICE_INT_SOURCE** — `1` `DSRVOVFLINT`, `0` `DSRVPCKTINT`
- `0x9101` **HD_BLEND_OPTIONS** — `6` `HD_BLEND_SERV_SWITCH_RAMP_DOWN_TIME`, `4` `HD_BLEND_SERV_LOSS_RAMP_UP_TIME`, `2` `BLEND_PIN_CTRL`, `1:0` `ACQ_LOSS[1:0]`
- `0x9102` **HD_BLEND_ANALOG_TO_HD_TRANSITION_TIME** — `15:0` `BLEND_TIME[15:0]`
- `0x9103` **HD_BLEND_HD_TO_ANALOG_TRANSITION_TIME** — `15:0` `BLEND_TIME[15:0]`
- `0x9106` **HD_BLEND_DYNAMIC_GAIN** — `8` `DGAIN_OVERRIDE`, `7:0` `DGAIN[7:0]`
- `0x9109` **HD_BLEND_BLEND_DECISION_ANALOG_TO_DIGITAL_THD** — `7:0` `A2D_THD[7:0]`
- `0x910A` **HD_BLEND_BLEND_DECISION_ANALOG_TO_DIGITAL_DELAY** — `15:0` `A2D_DELAY[15:0]`
- `0x910C` **HD_BLEND_SERV_LOSS_RAMP_DOWN_TIME** — `15:0` `P_DOWN_-`
- `0x910D` **HD_BLEND_SERV_LOSS_NOISE_RAMP_UP_TIME** — `15:0` `E_RAMP_UP_-`
- `0x910E` **HD_BLEND_SERV_LOSS_NOISE_RAMP_DOWN_TIME** — `15:0` `E_RAMP_DOWN_-`
- `0x910F` **HD_BLEND_SERV_LOSS_NOISE_LEVEL** — `15:0` `dBFS.`
- `0x9110` **HD_BLEND_SERV_LOSS_NOISE_DAAI_THRESHOLD** — `15:0` `E_DAAI_THRESH-`
- `0x9112` **HD_BLEND_SERV_SWITCH_RAMP_UP_TIME** — `15:0` `AMP_UP_-`
- `0x9113` **HD_BLEND_SERV_SWITCH_RAMP_DOWN_TIME** — `15:0` `AMP_DOWN_-`
- `0x9200` **HD_DIGRAD_INTERRUPT_SOURCE** — `7` `HDLOGOINTEN`, `6` `SRCANAINTEN`, `5` `SRCDIGINTEN`, `3` `AUDACQINTEN`, `2` `ACQINTEN`, `1` `CDNRHINTEN`, `0` `CDNRLINTEN`
- `0x9201` **HD_DIGRAD_CDNR_LOW_THRESHOLD** — `15:0` `occur.`
- `0x9202` **HD_DIGRAD_CDNR_HIGH_THRESHOLD** — `15:0` `occur.`
- `0x9300` **HD_EVENT_INTERRUPT_SOURCE** — `7` `DINFO_INTEN`, `6` `AINFO_INTEN`, `4` `ALERT_INTEN`, `3` `PSD_INTEN`, `2` `SIS_INTEN`, `1` `DSRVLIST_INTEN`, `0` `ASRVLIST_INTEN`
- `0x9301` **HD_EVENT_SIS_CONFIG** — `4` `LOCATION`, `3` `RSVD`, `2` `NAME_LF`, `1` `NAME_SF`, `0` `ID`
- `0x9302` **HD_EVENT_ALERT_CONFIG** — `1` `PLAY_TONE`, `0` `ENABLE`
- `0x9500` **HD_PSD_ENABLE** — `1` `SPS1`, `0` `MPS`
- `0x9501` **HD_PSD_FIELD_MASK** — `15` `ID`, `14` `OWNER`, `13` `DESC`, `12` `NAME`, `11` `RECV`, `10` `URL`, `9` `VALID`, `8` `PRICE`, `6` `TEXT`, `5` `SHORT`, `4` `LANG`, `3` `GENRE`, `2` `ALBUM`, `1` `ARTIST`, `0` `TITLE`
- `0x9700` **HD_AUDIO_CTRL_FRAME_DELAY** — `3:0` `DELAY[3:0]`
- `0x9701` **HD_AUDIO_CTRL_PROGRAM_LOSS_THRESHOLD** — `3:0` `TRESH[3:0]`
- `0x9702` **HD_AUDIO_CTRL_BALL_GAME_ENABLE** — `0` `MODE`
- `0x9900` **HD_CODEC_MODE_0_BLEND_THRESHOLD** — `2:0` `LEVEL[2:0]`
- `0x9901` **HD_CODEC_MODE_0_SAMPLES_DELAY** — `13:0` `COUNT[13:0]`
- `0x9902` **HD_CODEC_MODE_0_BLEND_RATE** — `7:0` `HOLD[7:0]`
- `0x9903` **HD_CODEC_MODE_2_BLEND_THRESHOLD** — `2:0` `LEVEL[2:0]`
- `0x9904` **HD_CODEC_MODE_2_SAMPLES_DELAY** — `13:0` `COUNT[13:0]`
- `0x9905` **HD_CODEC_MODE_2_BLEND_RATE** — `7:0` `HOLD[7:0]`
- `0x9906` **HD_CODEC_MODE_10_BLEND_THRESHOLD** — `2:0` `LEVEL[2:0]`
- `0x9907` **HD_CODEC_MODE_10_SAMPLES_DELAY** — `13:0` `COUNT[13:0]`
- `0x9908` **HD_CODEC_MODE_10_BLEND_RATE** — `7:0` `HOLD[7:0]`
- `0x9909` **HD_CODEC_MODE_13_BLEND_THRESHOLD** — `2:0` `LEVEL[2:0]`
- `0x990A` **HD_CODEC_MODE_13_SAMPLES_DELAY** — `13:0` `COUNT[13:0]`
- `0x990B` **HD_CODEC_MODE_13_BLEND_RATE** — `7:0` `HOLD[7:0]`
- `0x990C` **HD_CODEC_MODE_1_BLEND_THRESHOLD** — `2:0` `LEVEL[2:0]`
- `0x990D` **HD_CODEC_MODE_1_SAMPLES_DELAY** — `13:0` `COUNT[13:0]`
- `0x990E` **HD_CODEC_MODE_1_BLEND_RATE** — `7:0` `HOLD[7:0]`
- `0x990F` **HD_CODEC_MODE_3_BLEND_THRESHOLD** — `2:0` `LEVEL[2:0]`
- `0x9910` **HD_CODEC_MODE_3_SAMPLES_DELAY** — `13:0` `COUNT[13:0]`
- `0x9911` **HD_CODEC_MODE_3_BLEND_RATE** — `7:0` `HOLD[7:0]`
- `0x9A00` **HD_SERVICE_MODE_CONTROL_MP11_ENABLE** — `0` `ENABLE`
- `0x9B00` **HD_EZBLEND_ENABLE** — `0` `ENABLE`
- `0x9B01` **HD_EZBLEND_MPS_BLEND_THRESHOLD** — `7:0` `audio`
- `0x9B04` **HD_EZBLEND_SPS_BLEND_THRESHOLD** — `7:0` `SPS`
- `0x9B05` **HD_EZBLEND_SPS_BLEND_RATE** — `7:0` `MPS`
- `0xE800` **HD_TEST_BER_CONFIG** — `0` `ENABLE`
- `0xE801` **HD_TEST_DEBUG_AUDIO** — `1:0` `TESTMODE[1:0]`, `516` `bytes`
- `0x1710` **DAB_TUNE_FE_VARM** — `15:0` `FE_VARM[15:0]`
- `0x1711` **DAB_TUNE_FE_VARB** — `15:0` `FE_VARB[15:0]`
- `0x1712` **DAB_TUNE_FE_CFG** — `1` `VHFCAPS`, `0` `VHFSW`
- `0xB000` **DAB_DIGRAD_INTERRUPT_SOURCE** — `4` `HARDMUTEIEN`, `3` `FICERRIEN`, `2` `ACQIEN`, `1` `RSSIHIEN`, `0` `RSSILIEN`
- `0xB001` **DAB_DIGRAD_RSSI_HIGH_THRESHOLD** — `7:0` `HIGH_THRESHOLD[7:0]`
- `0xB002` **DAB_DIGRAD_RSSI_LOW_THRESHOLD** — `7:0` `LOW_THRESHOLD[7:0]`
- `0xB200` **DAB_VALID_RSSI_TIME** — `5:0` `MS[5:0]`
- `0xB201` **DAB_VALID_RSSI_THRESHOLD** — `7:0` `LEVEL[7:0]`
- `0xB202` **DAB_VALID_ACQ_TIME** — `10:0` `MS[10:0]`
- `0xB203` **DAB_VALID_SYNC_TIME** — `10:0` `MS[10:0]`
- `0xB204` **DAB_VALID_DETECT_TIME** — `10:0` `MS[10:0]`
- `0xB300` **DAB_EVENT_INTERRUPT_SOURCE** — `7` `RECFG_INTEN`, `1` `FREQINFO_INTEN`, `0` `SRVLIST_INTEN`
- `0xB302` **DAB_EVENT_MIN_SVRLIST_PERIOD_RECONFIG** — `15:0` `RIOD_RECON-`
- `0xB400` **DAB_XPAD_ENABLE** — `2` `TDC_ENABLE`, `1` `MOT_ENABLE`, `0` `DLS_ENABLE`
- `0xB401` **DAB_DRC_OPTION** — `1:0` `DRC_OPTION[1:0]`
- `0xB500` **DAB_CTRL_DAB_MUTE_ENABLE** — `0` `MS`
- `0xB501` **DAB_CTRL_DAB_MUTE_SIGNAL_LEVEL_THRESHOLD** — `15:0` `NAL_LEVEL_-`
- `0xB502` **DAB_CTRL_DAB_MUTE_WIN_THRESHOLD** — `15:0` `If`
- `0xB503` **DAB_CTRL_DAB_UNMUTE_WIN_THRESHOLD** — `15:0` `MUTE_WIN_-`
- `0xB504` **DAB_CTRL_DAB_MUTE_SIGLOSS_THRESHOLD** — `15:0` `GLOSS_THRESH-`
- `0xB505` **DAB_CTRL_DAB_MUTE_SIGLOW_THRESHOLD** — `15:0` `GLOW_THRESH-`
- `0xE800` **DAB_TEST_BER_CONFIG** — `8` `ENABLE`, `7:0` `PATTERN[7:0]`, `516` `bytes`, `20` `generally`
- `0x0500` **AM_AVC_MIN_GAIN** — `15:0` `MINGAIN[15:0]`
- `0x0501` **AM_AVC_MAX_GAIN** — `14:0` `MAXGAIN[14:0]`
- `0x2200` **AM_CHBW_SQ_LIMITS** — `15:8` `SQ_MAX[7:0]`, `7:0` `SQ_MIN[7:0]`
- `0x2201` **AM_CHBW_SQ_CHBW** — `15:8` `MAX[7:0]`, `7:0` `MIN[7:0]`
- `0x2202` **AM_CHBW_SQ_WIDENING_TIME** — `15:0` `WIDENING_TIME[15:0]`
- `0x2203` **AM_CHBW_SQ_NARROWING_TIME** — `15:0` `NARROWING_TIME[15:0]`
- `0x2204` **AM_CHBW_OVERRIDE_BW** — `7:0` `OVERRIDE_BW[7:0]`
- `0x4100` **AM_SEEK_BAND_BOTTOM** — `15:0` `AMSKFREQL[15:0]`
- `0x4101` **AM_SEEK_BAND_TOP** — `15:0` `AMSKFREQH[15:0]`
- `0x4102` **AM_SEEK_FREQUENCY_SPACING** — `4:0` `AMSKSPACE[4:0]`
- `0x4201` **AM_VALID_RSSI_TIME** — `5:0` `SSIVALTIME[5:0]`
- `0x4202` **AM_VALID_RSSI_THRESHOLD** — `7:0` `AMVALRSSI[7:0]`
- `0x4203` **AM_VALID_SNR_TIME** — `5:0` `SNRVALTIME[5:0]`
- `0x4204` **AM_VALID_SNR_THRESHOLD** — `7:0` `AMVALSNR[7:0]`
- `0x4300` **AM_RSQ_INTERRUPT_SOURCE** — `3` `SNRHINT`, `2` `SNRLINT`, `1` `RSSIHINT`, `0` `RSSILINT`
- `0x4301` **AM_RSQ_SNR_HIGH_THRESHOLD** — `7:0` `SNRH[7:0]`
- `0x4302` **AM_RSQ_SNR_LOW_THRESHOLD** — `7:0` `SNRL[7:0]`
- `0x4303` **AM_RSQ_RSSI_HIGH_THRESHOLD** — `7:0` `RSSIH[7:0]`
- `0x4304` **AM_RSQ_RSSI_LOW_THRESHOLD** — `7:0` `RSSIL[7:0]`
- `0x4305` **AM_RSQ_HD_DETECTION** — `7:0` `SAMPLES[7:0]`
- `0x4306` **AM_RSQ_HD_LEVEL_TIME_CONST** — `7:0` `ing`
- `0x4307` **AM_RSQ_HDDETECTED_THD** — `15:8` `threshold`, `7:0` `ric`
- `0x4400` **AM_ACF_INTERRUPT_SOURCE** — `1` `HIGHCUT_INTEN`, `0` `SMUTE_INTEN`
- `0x4403` **AM_ACF_SOFTMUTE_TOLERANCE** — `4:0` `SMUTE_TOL[4:0]`
- `0x4404` **AM_ACF_HIGHCUT_TOLERANCE** — `7:0` `flag`
- `0x4405` **AM_ACF_CONTROL_SOURCE** — `3` `AFC_DIS`, `2` `AFC_SM`, `0` `USE_RSSI`
- `0x4500` **AM_SOFTMUTE_SQ_LIMITS** — `15:8` `XMAX[7:0]`, `7:0` `XMIN[7:0]`
- `0x4501` **AM_SOFTMUTE_SQ_ATTENUATION** — `15:8` `ATTENMIN[7:0]`, `7:0` `ATTENMAX[7:0]`
- `0x4502` **AM_SOFTMUTE_SQ_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x4503` **AM_SOFTMUTE_SQ_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x4600` **AM_HIGHCUT_SQ_LIMITS** — `15:8` `SQ_MAX[7:0]`, `7:0` `SQ_MIN[7:0]`
- `0x4601` **AM_HIGHCUT_SQ_CUTOFF_FREQ** — `15:8` `MAX[7:0]`, `7:0` `MIN[7:0]`
- `0x4602` **AM_HIGHCUT_SQ_ATTACK_TIME** — `15:0` `ATTACK[15:0]`
- `0x4603` **AM_HIGHCUT_SQ_RELEASE_TIME** — `15:0` `RELEASE[15:0]`
- `0x4800` **AM_DEMOD_AFC_RANGE** — `15:0` `RANGE[15:0]`
- `0x9120` **HD_BLEND_BWM_CTRL_THRES** — `15:0` `width`
- `0x9121` **HD_BLEND_BWM_CTRL_LEVEL** — `15:8` `MAX[7:0]`, `7:0` `MIN[7:0]`
- `0x9122` **HD_BLEND_BWM_CTRL_RAMP_UP_TIME** — `15:0` `TRL_RAMP_UP_-`
- `0x9123` **HD_BLEND_BWM_CTRL_RAMP_DOWN_TIME** — `15:0` `TRL_RAMP_DOW`
- `0x9124` **HD_BLEND_BWM_BLEND_THRES** — `15:0` `which`
- `0x9125` **HD_BLEND_BWM_BLEND_LEVEL** — `15:8` `MAX[7:0]`, `7:0` `MIN[7:0]`
- `0x9126` **HD_BLEND_BWM_BLEND_RAMP_UP_TIME** — `15:0` `digital`
- `0x9127` **HD_BLEND_BWM_BLEND_RAMP_DOWN_TIME** — `15:0` `MP_DOWN_-`
- `0x9F00` **HD_ENHANCED_STREAM_HOLDOFF_CONFIG** — `0` `ENABLE`
- `0x9F01` **HD_ENHANCED_STREAM_HOLDOFF_THRESHOLDS** — `15:8` `ALLDIG[7:0]`, `7:0` `HYBRID[7:0]`

## Deliberate boundary

`Si468x.h` does not decode RDS PS/RT/AF/CT, DLS/DL+, MOT, EPG, Journaline, TMC/TPEG or image formats. It exposes the raw RDS blocks and raw DSRV payload plus all Si468x transport metadata so that an application can implement those standards without modifying the driver.

AN649 also mentions `DAB_GET_ANNOUNCEMENT_INFO` without defining an opcode/layout in Rev. 1.9; the driver does not invent one. Future documented commands remain reachable through `executeRaw()`.
