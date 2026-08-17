#ifndef SI468X_UNIVERSAL_H
#define SI468X_UNIVERSAL_H

/*
 * Si468x Universal Driver -- single-header, platform-neutral C++11 library
 * -----------------------------------------------------------------------
 * Target devices: Si4682 / Si4683 / Si4684 / Si4685 / Si4688 / Si4689
 *
 * Design goals
 * ------------
 *  - one source file, no Arduino/ESP-IDF/AVR/Linux dependency;
 *  - no dynamic allocation, STL, exceptions, RTTI, String, filesystem or UI;
 *  - SPI and I2C are both supported through the same logical host interface;
 *  - caller owns all transfer/image buffers and chooses their size;
 *  - interrupt-driven operation is preferred, status polling remains available;
 *  - all commands and properties documented by Silicon Labs AN649 rev. 1.9
 *    are represented, including FM/FMHD, AM/AMHD, DAB/DAB+, DSRV,
 *    storage, diagnostics, BER test and NVSPI flash pass-through;
 *  - firmware files are deliberately NOT embedded in this library.
 *
 * Source basis
 * ------------
 * The command/property definitions and protocol behavior in this file are based
 * on AN649 "Si468x Programming Guide", Rev. 1.9.  Later firmware revisions may
 * add or alter behavior; raw command/property access is always available so the
 * driver does not artificially restrict later images.
 *
 * IMPORTANT TRANSPORT CONTRACT
 * ----------------------------
 * HostInterface::writeCommand() receives the command byte separately followed
 * by the exact AN649 argument bytes.  HostInterface::readReply() must return a
 * logical Si468x reply starting at STATUS0 in dst[0].  The transport adapter is
 * responsible for SPI chip-select/dummy-byte framing or I2C framing.  The core
 * driver therefore never needs to know which MCU or physical control bus is in
 * use.
 *
 * Typical migration from DABShield
 * --------------------------------
 * DABShield puts Arduino GPIO/SPI and the radio protocol in the same library.
 * This driver intentionally separates them.  Replace DABSpiMsg()/SPI calls by a
 * HostInterface adapter, provide a workspace with setWorkspace(), then use the
 * corresponding typed method (fmTune, dabTune, setProperty, etc.).  Display,
 * EEPROM, buttons and station storage stay entirely in the application.
 */

/*
 * ============================================================================
 * Si468x FAMILY CAPABILITY MATRIX -- HUMAN / AI QUICK REFERENCE
 * ============================================================================
 *
 * Device   FM   RDS   AM   HD-FM   HD-AM   DAB   DAB+
 * -----------------------------------------------------
 * Si4682   YES  YES   NO   YES     NO      NO    NO
 * Si4683   YES  YES   YES  YES     YES     NO    NO
 * Si4684   YES  YES   NO   NO      NO      YES   YES
 * Si4685   YES  YES   YES  NO      NO      YES   YES
 * Si4688   YES  YES   NO   YES     NO      YES   YES
 * Si4689   YES  YES   YES  YES     YES     YES   YES
 *
 * Source note:
 *   - Si4682/83/84/88/89 capability grouping follows AN649 Rev. 1.9.
 *   - Si4685 capability grouping follows the later Si468x family table in
 *     AN651 (Skyworks, Rev. 0.5).
 *
 * IMPORTANT -- SILICON CAPABILITY != CURRENT COMMAND AVAILABILITY
 * ---------------------------------------------------------------------------
 * A device may support several radio standards in silicon, but only commands
 * belonging to the firmware image currently running in RAM are usable.
 * Example: Si4689 supports FM, AM, HD and DAB, but DAB_TUNE_FREQ is usable only
 * while a DAB/DAB+ image is active.
 *
 * Runtime helpers:
 *   detectedPart()          -> cached silicon part from GET_PART_INFO
 *   activeImage()           -> cached image family from GET_SYS_STATE
 *   capabilities()          -> hardware capability matrix for the part
 *   partSupports(feature)   -> hardware-only capability check
 *   featureAvailability()   -> hardware + active-image availability
 *   commandAvailability()   -> command-level hardware + image check
 *
 * Documentation convention used throughout the public API:
 *
 *   SI468X-SUPPORT:  hardware members that can implement the function.
 *   SI468X-FIRMWARE: firmware/boot state required for the function.
 *   SI468X-AN649:    command/property used, or protocol role.
 *
 * These fixed labels are intentionally repetitive. They make the single-header
 * file easy to search and make isolated code snippets self-describing for both
 * human reviewers and AI/code-analysis tools.
 *
 * "ALL" means all currently documented family members listed above. A command
 * may still have a firmware-revision caveat stated in its nearby comment or in
 * AN649. The device's own ERR_CMD / NOT_SUPPORTED response remains authoritative.
 * ============================================================================
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace si468x {

static const uint16_t LIBRARY_VERSION_MAJOR = 0;
static const uint16_t LIBRARY_VERSION_MINOR = 9;
static const uint16_t LIBRARY_VERSION_PATCH = 3;

// -----------------------------------------------------------------------------
// 1. Small endian helpers.  Si468x command/reply multi-byte values are LSB first.
// -----------------------------------------------------------------------------

inline uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
inline uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline void writeLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu); p[1] = (uint8_t)(v >> 8);
}
inline void writeLe32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu); p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu); p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

// -----------------------------------------------------------------------------
// 2. Complete AN649 command and property identifiers.
// -----------------------------------------------------------------------------

enum class Command : uint8_t {
    // COMMON / BOOT / CONTROL -- SI468X-SUPPORT: ALL
    RD_REPLY = 0x00,
    POWER_UP = 0x01,
    HOST_LOAD = 0x04,
    FLASH_LOAD = 0x05,
    LOAD_INIT = 0x06,
    BOOT = 0x07,
    GET_PART_INFO = 0x08,
    GET_SYS_STATE = 0x09,
    GET_POWER_UP_ARGS = 0x0A,
    READ_OFFSET = 0x10,
    GET_FUNC_INFO = 0x12,
    SET_PROPERTY = 0x13,
    GET_PROPERTY = 0x14,
    // FM / FMHD -- SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689
    // SI468X-FIRMWARE: FM/FMHD
    FM_TUNE_FREQ = 0x30,
    FM_SEEK_START = 0x31,
    FM_RSQ_STATUS = 0x32,
    FM_ACF_STATUS = 0x33,
    FM_RDS_STATUS = 0x34,
    FM_RDS_BLOCKCOUNT = 0x35,
    // DIGITAL SERVICE TRANSPORT -- DAB and/or HD capable devices
    // SI468X-SUPPORT: ALL (each family member has DAB or HD capability)
    // SI468X-FIRMWARE: DAB/DAB+ or HD-capable FMHD/AMHD image
    GET_DIGITAL_SERVICE_LIST = 0x80,
    START_DIGITAL_SERVICE = 0x81,
    STOP_DIGITAL_SERVICE = 0x82,
    GET_DIGITAL_SERVICE_DATA = 0x84,
    // HD RADIO -- SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689
    // SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where applicable
    HD_DIGRAD_STATUS = 0x92,
    HD_GET_EVENT_STATUS = 0x93,
    HD_GET_STATION_INFO = 0x94,
    HD_GET_PSD_DECODE = 0x95,
    HD_GET_ALERT_MSG = 0x96,
    HD_PLAY_ALERT_TONE = 0x97,
    HD_TEST_GET_BER_INFO = 0x98,
    HD_SET_ENABLED_PORTS = 0x99,
    HD_GET_ENABLED_PORTS = 0x9A,
    // DIAGNOSTIC / TEST -- availability may depend on active image revision
    TEST_GET_RSSI = 0xE5,
    // ON-CHIP STORAGE ACCESS -- common command family
    WRITE_STORAGE = 0x15,
    READ_STORAGE = 0x16,
    // DAB / DAB+ -- SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689
    // SI468X-FIRMWARE: DAB/DAB+
    DAB_TUNE_FREQ = 0xB0,
    DAB_DIGRAD_STATUS = 0xB2,
    DAB_GET_EVENT_STATUS = 0xB3,
    DAB_GET_ENSEMBLE_INFO = 0xB4,
    DAB_GET_SERVICE_LINKING_INFO = 0xB7,
    DAB_SET_FREQ_LIST = 0xB8,
    DAB_GET_FREQ_LIST = 0xB9,
    DAB_GET_COMPONENT_INFO = 0xBB,
    DAB_GET_TIME = 0xBC,
    DAB_GET_AUDIO_INFO = 0xBD,
    DAB_GET_SUBCHAN_INFO = 0xBE,
    DAB_GET_FREQ_INFO = 0xBF,
    DAB_GET_SERVICE_INFO = 0xC0,
    DAB_TEST_GET_BER_INFO = 0xE8,
    /*
     * AN649 mentions DAB_GET_ANNOUNCEMENT_INFO in the DAB event description
     * but Rev. 1.9 does not provide an opcode or command layout for it. It is
     * therefore intentionally not invented here; executeRaw() can access a
     * later documented implementation when its command definition is known.
     */
    // AM / AMHD -- SI468X-SUPPORT: Si4683 Si4685 Si4689
    // SI468X-FIRMWARE: AM/AMHD
    AM_TUNE_FREQ = 0x40,
    AM_SEEK_START = 0x41,
    AM_RSQ_STATUS = 0x42,
    AM_ACF_STATUS = 0x43,
};

enum class Property : uint16_t {
    // COMMON PROPERTIES -- SI468X-SUPPORT: ALL; active-image semantics apply
    INT_CTL_ENABLE = 0x0000,
    INT_CTL_REPEAT = 0x0001,
    DIGITAL_IO_OUTPUT_SELECT = 0x0200,
    DIGITAL_IO_OUTPUT_SAMPLE_RATE = 0x0201,
    DIGITAL_IO_OUTPUT_FORMAT = 0x0202,
    DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_1 = 0x0203,
    DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_2 = 0x0204,
    DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_3 = 0x0205,
    DIGITAL_IO_OUTPUT_FORMAT_OVERRIDES_4 = 0x0206,
    AUDIO_ANALOG_VOLUME = 0x0300,
    AUDIO_MUTE = 0x0301,
    AUDIO_OUTPUT_CONFIG = 0x0302,
    PIN_CONFIG_ENABLE = 0x0800,
    WAKE_TONE_ENABLE = 0x0900,
    WAKE_TONE_PERIOD = 0x0901,
    WAKE_TONE_FREQ = 0x0902,
    WAKE_TONE_AMPLITUDE = 0x0903,
    // FM / FMHD PROPERTIES -- SI468X-SUPPORT: ALL; SI468X-FIRMWARE: FM/FMHD
    FM_TUNE_FE_VARM = 0x1710,
    FM_TUNE_FE_VARB = 0x1711,
    FM_TUNE_FE_CFG = 0x1712,
    FM_SEEK_BAND_BOTTOM = 0x3100,
    FM_SEEK_BAND_TOP = 0x3101,
    FM_SEEK_FREQUENCY_SPACING = 0x3102,
    FM_VALID_MAX_TUNE_ERROR = 0x3200,
    FM_VALID_RSSI_TIME = 0x3201,
    FM_VALID_RSSI_THRESHOLD = 0x3202,
    FM_VALID_SNR_TIME = 0x3203,
    FM_VALID_SNR_THRESHOLD = 0x3204,
    FM_VALID_HDLEVEL_THRESHOLD = 0x3206,
    FM_RSQ_INTERRUPT_SOURCE = 0x3300,
    FM_RSQ_SNR_HIGH_THRESHOLD = 0x3301,
    FM_RSQ_SNR_LOW_THRESHOLD = 0x3302,
    FM_RSQ_RSSI_HIGH_THRESHOLD = 0x3303,
    FM_RSQ_RSSI_LOW_THRESHOLD = 0x3304,
    FM_RSQ_HD_DETECTION = 0x3307,
    FM_RSQ_HD_LEVEL_TIME_CONST = 0x3308,
    FM_RSQ_HDDETECTED_THD = 0x3309,
    FM_ACF_INTERRUPT_SOURCE = 0x3400,
    FM_ACF_SOFTMUTE_THRESHOLD = 0x3401,
    FM_ACF_HIGHCUT_THRESHOLD = 0x3402,
    FM_ACF_BLEND_THRESHOLD = 0x3403,
    FM_ACF_SOFTMUTE_TOLERANCE = 0x3404,
    FM_ACF_HIGHCUT_TOLERANCE = 0x3405,
    FM_ACF_BLEND_TOLERANCE = 0x3406,
    FM_SOFTMUTE_SNR_LIMITS = 0x3500,
    FM_SOFTMUTE_SNR_ATTENUATION = 0x3501,
    FM_SOFTMUTE_SNR_ATTACK_TIME = 0x3502,
    FM_SOFTMUTE_SNR_RELEASE_TIME = 0x3503,
    FM_HIGHCUT_RSSI_LIMITS = 0x3600,
    FM_HIGHCUT_RSSI_CUTOFF_FREQ = 0x3601,
    FM_HIGHCUT_RSSI_ATTACK_TIME = 0x3602,
    FM_HIGHCUT_RSSI_RELEASE_TIME = 0x3603,
    FM_HIGHCUT_SNR_LIMITS = 0x3604,
    FM_HIGHCUT_SNR_CUTOFF_FREQ = 0x3605,
    FM_HIGHCUT_SNR_ATTACK_TIME = 0x3606,
    FM_HIGHCUT_SNR_RELEASE_TIME = 0x3607,
    FM_HIGHCUT_MULTIPATH_LIMITS = 0x3608,
    FM_HIGHCUT_MULTIPATH_CUTOFF_FREQ = 0x3609,
    FM_HIGHCUT_MULTIPATH_ATTACK_TIME = 0x360A,
    FM_HIGHCUT_MULTIPATH_RELEASE_TIME = 0x360B,
    FM_BLEND_RSSI_LIMITS = 0x3700,
    FM_BLEND_RSSI_ATTACK_TIME = 0x3702,
    FM_BLEND_RSSI_RELEASE_TIME = 0x3703,
    FM_BLEND_SNR_LIMITS = 0x3704,
    FM_BLEND_SNR_ATTACK_TIME = 0x3706,
    FM_BLEND_SNR_RELEASE_TIME = 0x3707,
    FM_BLEND_MULTIPATH_LIMITS = 0x3708,
    FM_BLEND_MULTIPATH_ATTACK_TIME = 0x370A,
    FM_BLEND_MULTIPATH_RELEASE_TIME = 0x370B,
    FM_AUDIO_DE_EMPHASIS = 0x3900,
    FM_RDS_INTERRUPT_SOURCE = 0x3C00,
    FM_RDS_INTERRUPT_FIFO_COUNT = 0x3C01,
    FM_RDS_CONFIG = 0x3C02,
    FM_RDS_CONFIDENCE = 0x3C03,
    // DIGITAL SERVICE PROPERTIES -- DAB and HD data-service images
    DIGITAL_SERVICE_INT_SOURCE = 0x8100,
    DIGITAL_SERVICE_RESTART_DELAY = 0x8101,
    // HD RADIO PROPERTIES -- SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689
    // SI468X-FIRMWARE: FMHD / AMHD as applicable
    HD_BLEND_OPTIONS = 0x9101,
    HD_BLEND_ANALOG_TO_HD_TRANSITION_TIME = 0x9102,
    HD_BLEND_HD_TO_ANALOG_TRANSITION_TIME = 0x9103,
    HD_BLEND_DYNAMIC_GAIN = 0x9106,
    HD_BLEND_BLEND_DECISION_ANALOG_TO_DIGITAL_THD = 0x9109,
    HD_BLEND_BLEND_DECISION_ANALOG_TO_DIGITAL_DELAY = 0x910A,
    HD_BLEND_SERV_LOSS_RAMP_UP_TIME = 0x910B,
    HD_BLEND_SERV_LOSS_RAMP_DOWN_TIME = 0x910C,
    HD_BLEND_SERV_LOSS_NOISE_RAMP_UP_TIME = 0x910D,
    HD_BLEND_SERV_LOSS_NOISE_RAMP_DOWN_TIME = 0x910E,
    HD_BLEND_SERV_LOSS_NOISE_LEVEL = 0x910F,
    HD_BLEND_SERV_LOSS_NOISE_DAAI_THRESHOLD = 0x9110,
    HD_BLEND_SERV_LOSS_NOISE_AUDIO_START_DELAY = 0x9111,
    HD_BLEND_SERV_SWITCH_RAMP_UP_TIME = 0x9112,
    HD_BLEND_SERV_SWITCH_RAMP_DOWN_TIME = 0x9113,
    HD_DIGRAD_INTERRUPT_SOURCE = 0x9200,
    HD_DIGRAD_CDNR_LOW_THRESHOLD = 0x9201,
    HD_DIGRAD_CDNR_HIGH_THRESHOLD = 0x9202,
    HD_EVENT_INTERRUPT_SOURCE = 0x9300,
    HD_EVENT_SIS_CONFIG = 0x9301,
    HD_EVENT_ALERT_CONFIG = 0x9302,
    HD_PSD_ENABLE = 0x9500,
    HD_PSD_FIELD_MASK = 0x9501,
    HD_AUDIO_CTRL_FRAME_DELAY = 0x9700,
    HD_AUDIO_CTRL_PROGRAM_LOSS_THRESHOLD = 0x9701,
    HD_AUDIO_CTRL_BALL_GAME_ENABLE = 0x9702,
    HD_CODEC_MODE_0_BLEND_THRESHOLD = 0x9900,
    HD_CODEC_MODE_0_SAMPLES_DELAY = 0x9901,
    HD_CODEC_MODE_0_BLEND_RATE = 0x9902,
    HD_CODEC_MODE_2_BLEND_THRESHOLD = 0x9903,
    HD_CODEC_MODE_2_SAMPLES_DELAY = 0x9904,
    HD_CODEC_MODE_2_BLEND_RATE = 0x9905,
    HD_CODEC_MODE_10_BLEND_THRESHOLD = 0x9906,
    HD_CODEC_MODE_10_SAMPLES_DELAY = 0x9907,
    HD_CODEC_MODE_10_BLEND_RATE = 0x9908,
    HD_CODEC_MODE_13_BLEND_THRESHOLD = 0x9909,
    HD_CODEC_MODE_13_SAMPLES_DELAY = 0x990A,
    HD_CODEC_MODE_13_BLEND_RATE = 0x990B,
    HD_CODEC_MODE_1_BLEND_THRESHOLD = 0x990C,
    HD_CODEC_MODE_1_SAMPLES_DELAY = 0x990D,
    HD_CODEC_MODE_1_BLEND_RATE = 0x990E,
    HD_CODEC_MODE_3_BLEND_THRESHOLD = 0x990F,
    HD_CODEC_MODE_3_SAMPLES_DELAY = 0x9910,
    HD_CODEC_MODE_3_BLEND_RATE = 0x9911,
    HD_SERVICE_MODE_CONTROL_MP11_ENABLE = 0x9A00,
    HD_EZBLEND_ENABLE = 0x9B00,
    HD_EZBLEND_MPS_BLEND_THRESHOLD = 0x9B01,
    HD_EZBLEND_MPS_BLEND_RATE = 0x9B02,
    HD_EZBLEND_MPS_SAMPLES_DELAY = 0x9B03,
    HD_EZBLEND_SPS_BLEND_THRESHOLD = 0x9B04,
    HD_EZBLEND_SPS_BLEND_RATE = 0x9B05,
    HD_TEST_BER_CONFIG = 0xE800,
    HD_TEST_DEBUG_AUDIO = 0xE801,
    // DAB / DAB+ PROPERTIES -- SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689
    // SI468X-FIRMWARE: DAB/DAB+; property IDs may overlap FM IDs by image design
    DAB_TUNE_FE_VARM = 0x1710,
    DAB_TUNE_FE_VARB = 0x1711,
    DAB_TUNE_FE_CFG = 0x1712,
    DAB_DIGRAD_INTERRUPT_SOURCE = 0xB000,
    DAB_DIGRAD_RSSI_HIGH_THRESHOLD = 0xB001,
    DAB_DIGRAD_RSSI_LOW_THRESHOLD = 0xB002,
    DAB_VALID_RSSI_TIME = 0xB200,
    DAB_VALID_RSSI_THRESHOLD = 0xB201,
    DAB_VALID_ACQ_TIME = 0xB202,
    DAB_VALID_SYNC_TIME = 0xB203,
    DAB_VALID_DETECT_TIME = 0xB204,
    DAB_EVENT_INTERRUPT_SOURCE = 0xB300,
    DAB_EVENT_MIN_SVRLIST_PERIOD = 0xB301,
    DAB_EVENT_MIN_SVRLIST_PERIOD_RECONFIG = 0xB302,
    DAB_EVENT_MIN_FREQINFO_PERIOD = 0xB303,
    DAB_XPAD_ENABLE = 0xB400,
    DAB_DRC_OPTION = 0xB401,
    DAB_CTRL_DAB_MUTE_ENABLE = 0xB500,
    DAB_CTRL_DAB_MUTE_SIGNAL_LEVEL_THRESHOLD = 0xB501,
    DAB_CTRL_DAB_MUTE_WIN_THRESHOLD = 0xB502,
    DAB_CTRL_DAB_UNMUTE_WIN_THRESHOLD = 0xB503,
    DAB_CTRL_DAB_MUTE_SIGLOSS_THRESHOLD = 0xB504,
    DAB_CTRL_DAB_MUTE_SIGLOW_THRESHOLD = 0xB505,
    DAB_TEST_BER_CONFIG = 0xE800,
    // AM / AMHD PROPERTIES -- SI468X-SUPPORT: Si4683 Si4685 Si4689
    // SI468X-FIRMWARE: AM/AMHD
    AM_AVC_MIN_GAIN = 0x0500,
    AM_AVC_MAX_GAIN = 0x0501,
    AM_CHBW_SQ_LIMITS = 0x2200,
    AM_CHBW_SQ_CHBW = 0x2201,
    AM_CHBW_SQ_WIDENING_TIME = 0x2202,
    AM_CHBW_SQ_NARROWING_TIME = 0x2203,
    AM_CHBW_OVERRIDE_BW = 0x2204,
    AM_SEEK_BAND_BOTTOM = 0x4100,
    AM_SEEK_BAND_TOP = 0x4101,
    AM_SEEK_FREQUENCY_SPACING = 0x4102,
    AM_VALID_MAX_TUNE_ERROR = 0x4200,
    AM_VALID_RSSI_TIME = 0x4201,
    AM_VALID_RSSI_THRESHOLD = 0x4202,
    AM_VALID_SNR_TIME = 0x4203,
    AM_VALID_SNR_THRESHOLD = 0x4204,
    AM_VALID_HDLEVEL_THRESHOLD = 0x4205,
    AM_RSQ_INTERRUPT_SOURCE = 0x4300,
    AM_RSQ_SNR_HIGH_THRESHOLD = 0x4301,
    AM_RSQ_SNR_LOW_THRESHOLD = 0x4302,
    AM_RSQ_RSSI_HIGH_THRESHOLD = 0x4303,
    AM_RSQ_RSSI_LOW_THRESHOLD = 0x4304,
    AM_RSQ_HD_DETECTION = 0x4305,
    AM_RSQ_HD_LEVEL_TIME_CONST = 0x4306,
    AM_RSQ_HDDETECTED_THD = 0x4307,
    AM_ACF_INTERRUPT_SOURCE = 0x4400,
    AM_ACF_SOFTMUTE_THRESHOLD = 0x4401,
    AM_ACF_HIGHCUT_THRESHOLD = 0x4402,
    AM_ACF_SOFTMUTE_TOLERANCE = 0x4403,
    AM_ACF_HIGHCUT_TOLERANCE = 0x4404,
    AM_ACF_CONTROL_SOURCE = 0x4405,
    AM_SOFTMUTE_SQ_LIMITS = 0x4500,
    AM_SOFTMUTE_SQ_ATTENUATION = 0x4501,
    AM_SOFTMUTE_SQ_ATTACK_TIME = 0x4502,
    AM_SOFTMUTE_SQ_RELEASE_TIME = 0x4503,
    AM_HIGHCUT_SQ_LIMITS = 0x4600,
    AM_HIGHCUT_SQ_CUTOFF_FREQ = 0x4601,
    AM_HIGHCUT_SQ_ATTACK_TIME = 0x4602,
    AM_HIGHCUT_SQ_RELEASE_TIME = 0x4603,
    AM_DEMOD_AFC_RANGE = 0x4800,
    HD_BLEND_BWM_CTRL_THRES = 0x9120,
    HD_BLEND_BWM_CTRL_LEVEL = 0x9121,
    HD_BLEND_BWM_CTRL_RAMP_UP_TIME = 0x9122,
    HD_BLEND_BWM_CTRL_RAMP_DOWN_TIME = 0x9123,
    HD_BLEND_BWM_BLEND_THRES = 0x9124,
    HD_BLEND_BWM_BLEND_LEVEL = 0x9125,
    HD_BLEND_BWM_BLEND_RAMP_UP_TIME = 0x9126,
    HD_BLEND_BWM_BLEND_RAMP_DOWN_TIME = 0x9127,
    HD_ENHANCED_STREAM_HOLDOFF_CONFIG = 0x9F00,
    HD_ENHANCED_STREAM_HOLDOFF_THRESHOLDS = 0x9F01,
};

/* NVSPI flash pass-through uses a separate property namespace. */
/* Top-level INT_CTL_ENABLE / STATUS0 bit positions used across applications.
 * Some sources are reserved/not meaningful in some firmware modes; for
 * example RSQ/RDS/ACF are FM/AM-oriented while DAB uses DEVNT/DSRV/STC. */
enum InterruptMask : uint16_t {
    INTERRUPT_STC       = 0x0001u,
    INTERRUPT_ACF       = 0x0002u,
    INTERRUPT_RDS       = 0x0004u,
    INTERRUPT_RSQ       = 0x0008u,
    INTERRUPT_DSRV      = 0x0010u,
    INTERRUPT_DACQ      = 0x0020u,
    INTERRUPT_CMD_ERROR = 0x0040u,
    INTERRUPT_CTS       = 0x0080u,
    INTERRUPT_DEVICE_EVENT = 0x2000u
};

enum DigitalServiceInterruptMask : uint16_t {
    DSRV_INTERRUPT_PACKET_READY = 0x0001u,
    DSRV_INTERRUPT_OVERFLOW     = 0x0002u
};

enum class FlashProperty : uint16_t {
    SPI_CLOCK_FREQ_KHZ = 0x0001,
    SPI_MODE = 0x0002,
    READ_CMD = 0x0101,
    HIGH_SPEED_READ_CMD = 0x0102,
    HIGH_SPEED_READ_MAX_FREQ_MHZ = 0x0103,
    WRITE_CMD = 0x0201,
    ERASE_SECTOR_CMD = 0x0202,
    ERASE_CHIP_CMD = 0x0204
};

enum class FlashSubcommand : uint8_t {
    LOAD_IMAGE = 0x00,
    LOAD_IMAGE_CHECK_CRC32 = 0x01,
    CHECK_CRC32 = 0x02,
    GET_PROPERTY = 0x11,
    SET_PROPERTY_LIST = 0x10,
    WRITE_BLOCK = 0xF0,
    WRITE_BLOCK_READBACK_VERIFY = 0xF1,
    WRITE_BLOCK_PACKET_VERIFY = 0xF2,
    WRITE_BLOCK_READBACK_AND_PACKET_VERIFY = 0xF3,
    ERASE_SECTOR = 0xFE,
    ERASE_CHIP = 0xFF
};

// -----------------------------------------------------------------------------
// 3. Result, device state and capability types.
// -----------------------------------------------------------------------------

enum class Result : int8_t {
    Ok = 0,
    Pending = 1,
    Busy = 2,
    InvalidArgument = -1,
    NoTransport = -2,
    NoTimer = -3,
    Timeout = -4,
    TransportError = -5,
    DeviceError = -6,
    BufferTooSmall = -7,
    Unsupported = -8,
    MalformedReply = -9,
    EndOfData = -10
};

enum class Image : uint8_t {
    Bootloader = 0,
    FMHD = 1,
    DAB = 2,
    TDMB_or_DAB_DataOnly = 3,
    FMHD_Demod = 4,
    AMHD = 5,
    AMHD_Demod = 6,
    DAB_Demod = 7,
    Reserved16 = 16,
    Unknown = 0xFF
};

enum class PowerState : uint8_t {
    ResetWaitingPowerUp = 0,
    Reserved = 1,
    BootloaderRunning = 2,
    ApplicationRunning = 3
};

enum class Part : uint16_t {
    Unknown = 0,
    Si4682 = 4682,
    Si4683 = 4683,
    Si4684 = 4684,
    Si4685 = 4685,
    Si4688 = 4688,
    Si4689 = 4689
};

enum class Feature : uint8_t {
    FM, RDS, AM, HDFM, HDAM, DAB, DABPlus, DigitalServices
};

enum class Availability : uint8_t { Unknown=0, Unsupported=1, Supported=2 };

struct Capabilities {
    bool fm;
    bool rds;
    bool am;
    bool hdFm;
    bool hdAm;
    bool dab;
    bool dabPlus;
    bool digitalServices;

    Capabilities() : fm(false), rds(false), am(false), hdFm(false), hdAm(false),
                     dab(false), dabPlus(false), digitalServices(false) {}

    bool supports(Feature f) const {
        switch (f) {
            case Feature::FM: return fm;
            case Feature::RDS: return rds;
            case Feature::AM: return am;
            case Feature::HDFM: return hdFm;
            case Feature::HDAM: return hdAm;
            case Feature::DAB: return dab;
            case Feature::DABPlus: return dabPlus;
            case Feature::DigitalServices: return digitalServices;
        }
        return false;
    }
};

/*
 * Hardware-family capability map.  PART is returned by GET_PART_INFO as the
 * decimal product number (4682, 4683, ...), exactly as specified by AN649.
 * This table says what the silicon family member can support; the currently
 * loaded firmware image still determines which mode-specific commands are
 * available at any particular moment.
 */
inline Capabilities capabilitiesForPart(uint16_t part) {
    Capabilities c;
    switch (part) {
        case 4682: c.fm=true; c.rds=true; c.hdFm=true; break;
        case 4683: c.fm=true; c.rds=true; c.am=true; c.hdFm=true; c.hdAm=true; break;
        case 4684: c.fm=true; c.rds=true; c.dab=true; c.dabPlus=true; break;
        case 4685: c.fm=true; c.rds=true; c.am=true; c.dab=true; c.dabPlus=true; break;
        case 4688: c.fm=true; c.rds=true; c.hdFm=true; c.dab=true; c.dabPlus=true; break;
        case 4689: c.fm=true; c.rds=true; c.am=true; c.hdFm=true; c.hdAm=true;
                   c.dab=true; c.dabPlus=true; break;
        default: break;
    }
    c.digitalServices = c.dab || c.hdFm || c.hdAm;
    return c;
}

inline bool imageSupportsFeature(Image image, Feature feature) {
    switch (feature) {
        case Feature::FM: return image==Image::FMHD;
        case Feature::RDS: return image==Image::FMHD;
        case Feature::AM: return image==Image::AMHD;
        case Feature::HDFM: return image==Image::FMHD;
        case Feature::HDAM: return image==Image::AMHD;
        case Feature::DAB:
        case Feature::DABPlus: return image==Image::DAB;
        case Feature::DigitalServices: return image==Image::FMHD || image==Image::AMHD || image==Image::DAB;
    }
    return false;
}

/* Parsed four-byte status word returned at the start of every reply. */
struct Status {
    uint8_t status0;
    uint8_t status1;
    uint8_t status2;
    uint8_t status3;

    Status() : status0(0), status1(0), status2(0), status3(0) {}

    bool cts() const { return (status0 & 0x80u) != 0; }
    bool commandError() const { return (status0 & 0x40u) != 0; }
    bool dacqInt() const { return (status0 & 0x20u) != 0; }
    bool dsrvInt() const { return (status0 & 0x10u) != 0; }
    bool rsqInt() const { return (status0 & 0x08u) != 0; }
    bool rdsInt() const { return (status0 & 0x04u) != 0; }
    bool acfInt() const { return (status0 & 0x02u) != 0; }
    bool stcInt() const { return (status0 & 0x01u) != 0; }
    bool deviceEventInt() const { return (status1 & 0x20u) != 0; }

    PowerState powerState() const { return (PowerState)((status3 >> 6) & 0x03u); }
    bool dspError() const { return (status3 & 0x10u) != 0; }
    bool replyOverflowError() const { return (status3 & 0x08u) != 0; }
    bool commandOverflowError() const { return (status3 & 0x04u) != 0; }
    bool arbiterError() const { return (status3 & 0x02u) != 0; }
    bool nonRecoverableError() const { return (status3 & 0x01u) != 0; }
    bool fatal() const { return dspError() || replyOverflowError() || commandOverflowError() ||
                                arbiterError() || nonRecoverableError(); }
};

inline bool parseStatus(const uint8_t* reply, size_t length, Status& out) {
    if (!reply || length < 4) return false;
    out.status0=reply[0]; out.status1=reply[1]; out.status2=reply[2]; out.status3=reply[3];
    return true;
}

// -----------------------------------------------------------------------------
// 4. Platform-neutral host interface.
// -----------------------------------------------------------------------------

/*
 * writeCommand:
 *   Send one complete command transaction. `command` is the CMD byte and `args`
 *   contains ARG1..ARGn (or subcommand bytes where AN649 defines them).
 *
 * readReply:
 *   Read `length` logical reply bytes beginning with STATUS0 at dst[0].
 *   SPI adapters hide dummy/framing bytes here; I2C adapters hide bus framing.
 *
 * timeUs:
 *   Monotonic 32-bit microsecond timer. Unsigned wraparound is supported.
 *
 * idle:
 *   Optional cooperative yield hook used only by blocking convenience helpers.
 *
 * setReset / setPower:
 *   Optional board hooks. The bool passed to setReset means ASSERTED, independent
 *   of electrical polarity.  The bool passed to setPower means ENABLED.
 */
struct HostInterface {
    void* context;
    bool (*writeCommand)(void*, uint8_t, const uint8_t*, uint16_t);
    bool (*readReply)(void*, uint8_t*, uint16_t);
    uint32_t (*timeUs)(void*);
    void (*idle)(void*);
    void (*setReset)(void*, bool);
    void (*setPower)(void*, bool);

    HostInterface() : context(0), writeCommand(0), readReply(0), timeUs(0),
                      idle(0), setReset(0), setPower(0) {}
};

typedef void (*StatusCallback)(void* context, const Status& status);
typedef size_t (*ImageReader)(void* context, uint32_t offset, uint8_t* destination, size_t length);

// -----------------------------------------------------------------------------
// 5. Common typed replies and command arguments.
// -----------------------------------------------------------------------------

struct PowerUpConfig {
    bool ctsInterruptEnable;
    uint8_t clockMode;       // AN649 CLK_MODE[1:0]
    uint8_t trSize;          // AN649 TR_SIZE[3:0]
    uint8_t iBias;           // 0..127
    uint32_t crystalFrequencyHz;
    uint8_t cTune;           // 0..63
    uint8_t iBiasRun;        // 0..127

    PowerUpConfig() : ctsInterruptEnable(false), clockMode(0), trSize(0), iBias(0),
                      crystalFrequencyHz(0), cTune(0), iBiasRun(0) {}
};

struct PartInfo {
    uint8_t chipRevision;
    uint8_t romId;
    uint16_t partNumber;
    PartInfo() : chipRevision(0), romId(0), partNumber(0) {}
};

struct FunctionInfo {
    uint8_t major;
    uint8_t minor;
    uint8_t build;
    uint8_t flags;
    uint32_t svnId;
    FunctionInfo() : major(0), minor(0), build(0), flags(0), svnId(0) {}
};

struct SystemState {
    Image image;
    Status status;
    SystemState() : image(Image::Unknown) {}
};

struct PowerUpArgs {
    uint8_t clockMode;
    uint8_t trSize;
    uint8_t iBias;
    uint32_t crystalFrequencyHz;
    uint8_t cTune;
    uint8_t iBiasRun;
    PowerUpArgs() : clockMode(0), trSize(0), iBias(0), crystalFrequencyHz(0),
                    cTune(0), iBiasRun(0) {}
};

enum class Injection : uint8_t { Automatic=0, LowSide=1, HighSide=2 };
enum class TuneMode : uint8_t { AnalogOnly=0, Reserved=1, AnalogThenHD=2, HDOnly=3 };

struct FmRsqStatus {
    Status status;
    bool valid, afcRail, bandLimit, hdDetected, filteredHdDetected;
    uint16_t frequency10kHz;
    int8_t frequencyOffset;
    int8_t rssi;
    int8_t snr;
    uint8_t multipath;
    uint16_t antennaCap;
    uint8_t hdLevel;
    uint8_t filteredHdLevel;
    FmRsqStatus() : valid(false), afcRail(false), bandLimit(false), hdDetected(false),
                    filteredHdDetected(false), frequency10kHz(0), frequencyOffset(0),
                    rssi(0), snr(0), multipath(0), antennaCap(0), hdLevel(0),
                    filteredHdLevel(0) {}
};

struct AmRsqStatus {
    Status status;
    bool valid, afcRail, bandLimit, hdDetected, filteredHdDetected;
    uint16_t frequencyKHz;
    int8_t frequencyOffset;
    int8_t rssi;
    int8_t snr;
    uint8_t modulationPercent;
    uint16_t antennaCap;
    uint8_t hdLevel;
    uint8_t filteredHdLevel;
    AmRsqStatus() : valid(false), afcRail(false), bandLimit(false), hdDetected(false),
                    filteredHdDetected(false), frequencyKHz(0), frequencyOffset(0),
                    rssi(0), snr(0), modulationPercent(0), antennaCap(0), hdLevel(0),
                    filteredHdLevel(0) {}
};

struct FmAcfStatus {
    Status status;
    bool blendInterrupt, highCutInterrupt, softMuteInterrupt;
    bool blendConverged, highCutConverged, softMuteConverged;
    bool blendActive, highCutActive, softMuteActive;
    uint8_t attenuationDb;
    uint8_t highCut100Hz;
    bool pilot;
    uint8_t stereoBlendPercent;
    FmAcfStatus() : blendInterrupt(false), highCutInterrupt(false), softMuteInterrupt(false),
                    blendConverged(false), highCutConverged(false), softMuteConverged(false),
                    blendActive(false), highCutActive(false), softMuteActive(false),
                    attenuationDb(0), highCut100Hz(0), pilot(false), stereoBlendPercent(0) {}
};

struct AmAcfStatus {
    Status status;
    bool highCutInterrupt, softMuteInterrupt;
    bool highCutConverged, softMuteConverged;
    bool highCutActive, softMuteActive;
    uint8_t attenuationDb;
    uint8_t highCut100Hz;
    uint8_t lowCut100Hz;
    AmAcfStatus() : highCutInterrupt(false), softMuteInterrupt(false), highCutConverged(false),
                    softMuteConverged(false), highCutActive(false), softMuteActive(false),
                    attenuationDb(0), highCut100Hz(0), lowCut100Hz(0) {}
};

struct FmRdsGroup {
    Status status;
    bool tpPtyValid, piValid, sync, fifoLost;
    bool tp;
    uint8_t pty;
    uint16_t pi;
    uint8_t fifoUsed;
    uint8_t ble[4];
    uint16_t block[4];
    FmRdsGroup() : tpPtyValid(false), piValid(false), sync(false), fifoLost(false),
                   tp(false), pty(0), pi(0), fifoUsed(0) {
        ble[0]=ble[1]=ble[2]=ble[3]=0; block[0]=block[1]=block[2]=block[3]=0;
    }
    bool blockUsable(uint8_t index) const { return index < 4 && ble[index] < 3; }
};

struct FmRdsBlockCount {
    Status status;
    uint16_t expected, received, uncorrectable;
    FmRdsBlockCount() : expected(0), received(0), uncorrectable(0) {}
};

struct DabDigradStatus {
    Status status;
    bool hardMute, ficError, acquired, valid;
    int8_t rssi;
    uint8_t snr, ficQuality, cnr;
    uint16_t fibErrorCount;
    uint32_t tunedFrequencyKHz;
    uint8_t tuneIndex;
    int8_t fftOffset;
    uint16_t antennaCap;
    uint16_t cuLevel;
    uint8_t fastDetect;
    DabDigradStatus() : hardMute(false), ficError(false), acquired(false), valid(false),
                        rssi(0), snr(0), ficQuality(0), cnr(0), fibErrorCount(0),
                        tunedFrequencyKHz(0), tuneIndex(0), fftOffset(0), antennaCap(0),
                        cuLevel(0), fastDetect(0) {}
};


struct DabEventStatus {
    Status status;
    bool reconfiguration, reconfigurationWarning, announcement;
    bool frequencyInfoInterrupt, serviceListInterrupt;
    bool frequencyInfoAvailable, serviceListAvailable;
    uint16_t serviceListVersion;
    DabEventStatus() : reconfiguration(false), reconfigurationWarning(false), announcement(false),
                       frequencyInfoInterrupt(false), serviceListInterrupt(false),
                       frequencyInfoAvailable(false), serviceListAvailable(false), serviceListVersion(0) {}
};

struct DabEnsembleInfo {
    Status status;
    uint16_t ensembleId;
    char label[17];
    uint8_t ecc;
    uint16_t abbreviationMask;
    DabEnsembleInfo() : ensembleId(0), ecc(0), abbreviationMask(0) { memset(label,0,sizeof(label)); }
};

struct DabTimeInfo {
    Status status;
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    DabTimeInfo() : year(0), month(0), day(0), hour(0), minute(0), second(0) {}
};

struct DabAudioInfo {
    Status status;
    uint16_t bitRateKbps;
    uint16_t sampleRateHz;
    bool parametricStereo;
    bool spectralBandReplication;
    uint8_t audioMode;
    uint8_t drcGainQuarterDb;
    DabAudioInfo() : bitRateKbps(0), sampleRateHz(0), parametricStereo(false),
                     spectralBandReplication(false), audioMode(0), drcGainQuarterDb(0) {}
};

struct DabSubchannelInfo {
    Status status;
    uint8_t serviceMode;
    uint8_t protectionInfo;
    uint16_t bitRateKbps;
    uint16_t capacityUnits;
    uint16_t capacityUnitAddress;
    DabSubchannelInfo() : serviceMode(0), protectionInfo(0), bitRateKbps(0),
                          capacityUnits(0), capacityUnitAddress(0) {}
};

struct DabServiceInfo {
    Status status;
    bool serviceLinkingAvailable;
    uint8_t pty;
    bool dataService;
    bool local;
    uint8_t caId;
    uint8_t numberOfComponents;
    uint8_t charset;
    uint8_t ecc;
    char label[17];
    uint16_t abbreviationMask;
    DabServiceInfo() : serviceLinkingAvailable(false), pty(0), dataService(false), local(false),
                       caId(0), numberOfComponents(0), charset(0), ecc(0), abbreviationMask(0) {
        memset(label,0,sizeof(label));
    }
};

struct DabComponentInfo {
    Status status;
    uint8_t globalId;
    uint8_t language;
    uint8_t charset;
    char label[17];
    uint16_t abbreviationMask;
    uint8_t numberOfUserApplications;
    uint8_t userApplicationBytes;
    const uint8_t* userApplicationData; // points into caller-owned reply buffer
    DabComponentInfo() : globalId(0), language(0), charset(0), abbreviationMask(0),
                         numberOfUserApplications(0), userApplicationBytes(0), userApplicationData(0) {
        memset(label,0,sizeof(label));
    }
};

struct DabBerInfo {
    Status status;
    uint32_t errorBits;
    uint32_t totalBits;
    DabBerInfo() : errorBits(0), totalBits(0) {}
};

struct TestRssiInfo {
    Status status;
    int16_t dbuv256;  // signed dBµV value in units of 1/256 dB
    TestRssiInfo() : dbuv256(0) {}
};

struct DsrvHeader {
    Status status;
    uint8_t interruptSource;
    uint8_t buffersRemaining;
    uint8_t serviceState;
    uint8_t dataType;
    uint8_t dataSource;       // upper two bits of DATA_TYPE in DAB mode
    uint8_t dscType;          // lower six bits of DATA_TYPE in DAB mode
    uint32_t serviceId;
    uint32_t componentId;
    uint16_t byteCount;
    uint16_t segmentNumber;
    uint16_t numberOfSegments;

    DsrvHeader() : interruptSource(0), buffersRemaining(0), serviceState(0), dataType(0),
                   dataSource(0), dscType(0), serviceId(0), componentId(0), byteCount(0),
                   segmentNumber(0), numberOfSegments(0) {}

    bool dataReady() const { return (interruptSource & 0x01u) != 0; }
    bool overflow() const { return (interruptSource & 0x02u) != 0; }
    bool physicalError() const { return (interruptSource & 0x04u) != 0; }
    bool isStandardData() const { return dataSource == 0; }
    bool isPad() const { return dataSource == 1; }
    bool isDls() const { return dataSource == 2; }
    bool isMot() const { return (dataSource == 0 || dataSource == 1) && dscType == 60; }
    bool isMotPad() const { return dataSource == 1 && dscType == 60; }
    bool isTmc() const { return (dataSource == 0 || dataSource == 1) && dscType == 1; }
    bool isTpegOrTdc() const { return (dataSource == 0 || dataSource == 1) && dscType == 5; }
    bool serviceStopped() const { return serviceState == 1; }
    bool serviceOverflow() const { return serviceState == 2; }
    bool newDataObject() const { return serviceState == 3; }
    bool payloadReceivedWithErrors() const { return serviceState == 4; }
    bool isStream() const { return numberOfSegments == 0; }
};

/* DLS/DL+ prefix defined by AN649 section 7.14.2. */
struct DlsFrame {
    bool toggle;
    bool command;
    uint8_t commandId;      // AN649 explicitly identifies 0x02 as DL Plus TAGS.
    bool link;
    uint8_t charset;
    const uint8_t* body;
    uint16_t bodyLength;
    DlsFrame() : toggle(false), command(false), commandId(0), link(false), charset(0),
                 body(0), bodyLength(0) {}
    bool isMessage() const { return !command; }
    bool isDlPlusCommand() const { return command && commandId == 2; }
};

// -----------------------------------------------------------------------------
// 6. Main driver.
// -----------------------------------------------------------------------------

class DabServiceListParser;

class Si468x {
public:
    // SI468X-API: Si468x | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    Si468x() : _workspace(0), _workspaceSize(0), _statusCallback(0), _statusContext(0),
               _irqPending(0), _state(State::Idle), _lastResult(Result::Ok), _reply(0),
               _replyLength(0), _deadline(0), _nextCtsPoll(0), _nextIdlePoll(0),
               _ctsPollIntervalUs(1000), _idlePollIntervalUs(50000), _lastDeviceError(0),
               _detectedPart(Part::Unknown), _activeImage(Image::Unknown) {}

    // SI468X-API: Si468x | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    explicit Si468x(const HostInterface& host) : Si468x() { _host = host; }

    // SI468X-API: setHost | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    void setHost(const HostInterface& host) { _host = host; }
    // SI468X-API: host | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    const HostInterface& host() const { return _host; }

    // SI468X-API: setWorkspace | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    void setWorkspace(uint8_t* buffer, size_t size) {
        _workspace = buffer; _workspaceSize = size;
    }
    // SI468X-API: workspace | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    uint8_t* workspace() const { return _workspace; }
    // SI468X-API: workspaceSize | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    size_t workspaceSize() const { return _workspaceSize; }

    // SI468X-API: setStatusCallback | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    void setStatusCallback(StatusCallback cb, void* context=0) {
        _statusCallback=cb; _statusContext=context;
    }

    // SI468X-API: setCtsPollIntervalUs | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    void setCtsPollIntervalUs(uint32_t us) { _ctsPollIntervalUs=us; }
    // SI468X-API: setIdleStatusPollIntervalUs | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none (host-side configuration) | SI468X-AN649: host abstraction
    void setIdleStatusPollIntervalUs(uint32_t us) { _idlePollIntervalUs=us; }

    /* Optional board-control hooks. Electrical polarity belongs in the adapter. */
    // SI468X-API: setResetAsserted | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: boot/reset hardware control; board adapter dependent | SI468X-AN649: reset/power sequencing
    Result setResetAsserted(bool asserted) {
        if (!_host.setReset) return Result::Unsupported;
        _host.setReset(_host.context,asserted); return Result::Ok;
    }
    // SI468X-API: setPowerEnabled | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: boot/reset hardware control; board adapter dependent | SI468X-AN649: reset/power sequencing
    Result setPowerEnabled(bool enabled) {
        if (!_host.setPower) return Result::Unsupported;
        _host.setPower(_host.context,enabled); return Result::Ok;
    }

    /*
     * Generic board reset sequence using only platform callbacks.  The defaults
     * reproduce the conservative sequence used by the proven DABShield code;
     * applications may shorten the delays when their hardware timing has been
     * validated against the device data sheet.
     */
    // SI468X-API: hardwareReset | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: boot/reset hardware control; board adapter dependent | SI468X-AN649: reset/power sequencing
    Result hardwareReset(uint32_t powerSettleUs=100000UL,
                         uint32_t resetAssertUs=100000UL,
                         uint32_t resetReleaseUs=100000UL) {
        if (!_host.setReset || !_host.timeUs) return Result::Unsupported;
        if (_host.setPower) { _host.setPower(_host.context,true); Result r=delayUs(powerSettleUs); if (r!=Result::Ok) return r; }
        _host.setReset(_host.context,true);
        Result r=delayUs(resetAssertUs); if (r!=Result::Ok) return r;
        _host.setReset(_host.context,false);
        return delayUs(resetReleaseUs);
    }

    /* Call from the platform ISR.  Do not perform bus traffic in the ISR. */
    // SI468X-API: notifyInterrupt | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any state | SI468X-AN649: host-side state/event engine
    void notifyInterrupt() { _irqPending = 1; }

    // SI468X-API: busy | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any state | SI468X-AN649: host-side state/event engine
    bool busy() const { return _state != State::Idle; }
    // SI468X-API: lastResult | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any state | SI468X-AN649: host-side state/event engine
    Result lastResult() const { return _lastResult; }
    // SI468X-API: lastStatus | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any state | SI468X-AN649: host-side state/event engine
    const Status& lastStatus() const { return _lastStatus; }
    // SI468X-API: lastDeviceError | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any state | SI468X-AN649: host-side state/event engine
    uint8_t lastDeviceError() const { return _lastDeviceError; }

    /*
     * Runtime identity/capability helpers. getPartInfo() and getSystemState()
     * update these cached values automatically. Hardware capability and active
     * firmware mode are intentionally kept separate: a Si4689 can support DAB,
     * for example, while DAB commands are unavailable when an FMHD image is
     * currently running.
     */
    // SI468X-API: detectedPart | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any; cached identity requires GET_PART_INFO/GET_SYS_STATE where supported | SI468X-AN649: runtime capability model
    Part detectedPart() const { return _detectedPart; }
    // SI468X-API: activeImage | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any; cached identity requires GET_PART_INFO/GET_SYS_STATE where supported | SI468X-AN649: runtime capability model
    Image activeImage() const { return _activeImage; }
    // SI468X-API: capabilities | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any; cached identity requires GET_PART_INFO/GET_SYS_STATE where supported | SI468X-AN649: runtime capability model
    Capabilities capabilities() const { return capabilitiesForPart((uint16_t)_detectedPart); }
    // SI468X-API: partSupports | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any; cached identity requires GET_PART_INFO/GET_SYS_STATE where supported | SI468X-AN649: runtime capability model
    bool partSupports(Feature f) const { return capabilities().supports(f); }
    // SI468X-API: featureAvailability | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any; cached identity requires GET_PART_INFO/GET_SYS_STATE where supported | SI468X-AN649: runtime capability model
    Availability featureAvailability(Feature f) const {
        const Capabilities c=capabilities();
        if (_detectedPart==Part::Unknown) return Availability::Unknown;
        if (!c.supports(f)) return Availability::Unsupported;
        if (_activeImage==Image::Unknown || _activeImage==Image::Bootloader) return Availability::Unknown;
        return imageSupportsFeature(_activeImage,f) ? Availability::Supported : Availability::Unsupported;
    }

    // SI468X-API: commandAvailability | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: any; cached identity requires GET_PART_INFO/GET_SYS_STATE where supported | SI468X-AN649: runtime capability model
    Availability commandAvailability(Command command) const {
        Feature f=Feature::FM;
        bool modeSpecific=true;
        switch (command) {
            case Command::FM_TUNE_FREQ: case Command::FM_SEEK_START: case Command::FM_RSQ_STATUS:
            case Command::FM_ACF_STATUS: f=Feature::FM; break;
            case Command::FM_RDS_STATUS: case Command::FM_RDS_BLOCKCOUNT: f=Feature::RDS; break;
            case Command::AM_TUNE_FREQ: case Command::AM_SEEK_START: case Command::AM_RSQ_STATUS:
            case Command::AM_ACF_STATUS: f=Feature::AM; break;
            case Command::DAB_TUNE_FREQ: case Command::DAB_DIGRAD_STATUS: case Command::DAB_GET_EVENT_STATUS:
            case Command::DAB_GET_ENSEMBLE_INFO: case Command::DAB_GET_SERVICE_LINKING_INFO:
            case Command::DAB_SET_FREQ_LIST: case Command::DAB_GET_FREQ_LIST: case Command::DAB_GET_COMPONENT_INFO:
            case Command::DAB_GET_TIME: case Command::DAB_GET_AUDIO_INFO: case Command::DAB_GET_SUBCHAN_INFO:
            case Command::DAB_GET_FREQ_INFO: case Command::DAB_GET_SERVICE_INFO: case Command::DAB_TEST_GET_BER_INFO:
                f=Feature::DAB; break;
            case Command::HD_DIGRAD_STATUS: case Command::HD_GET_EVENT_STATUS: case Command::HD_GET_STATION_INFO:
            case Command::HD_GET_PSD_DECODE: case Command::HD_GET_ALERT_MSG: case Command::HD_PLAY_ALERT_TONE:
            case Command::HD_TEST_GET_BER_INFO: case Command::HD_SET_ENABLED_PORTS: case Command::HD_GET_ENABLED_PORTS:
                if (_activeImage==Image::AMHD) f=Feature::HDAM;
                else if (_activeImage==Image::FMHD) f=Feature::HDFM;
                else {
                    if (_detectedPart==Part::Unknown) return Availability::Unknown;
                    const Capabilities c=capabilities();
                    return (c.hdFm||c.hdAm)?Availability::Unknown:Availability::Unsupported;
                }
                break;
            case Command::GET_DIGITAL_SERVICE_LIST: case Command::START_DIGITAL_SERVICE:
            case Command::STOP_DIGITAL_SERVICE: case Command::GET_DIGITAL_SERVICE_DATA:
                f=Feature::DigitalServices; break;
            default: modeSpecific=false; break;
        }
        if (!modeSpecific) return Availability::Supported;
        return featureAvailability(f);
    }

    /* Read the currently available status/reply bytes without issuing a command. */
    // SI468X-API: readCurrentReply | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: command/state dependent | SI468X-AN649: common command/response engine
    Result readCurrentReply(uint8_t* destination, uint16_t length) {
        if (!_host.readReply) return Result::NoTransport;
        if (!destination || length < 4) return Result::InvalidArgument;
        if (!_host.readReply(_host.context, destination, length)) return Result::TransportError;
        parseStatus(destination, length, _lastStatus);
        if (_statusCallback) _statusCallback(_statusContext, _lastStatus);
        return _lastStatus.commandError() ? Result::DeviceError : Result::Ok;
    }

    // SI468X-API: readStatus | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: command/state dependent | SI468X-AN649: common command/response engine
    Result readStatus(Status& out) {
        uint8_t b[4];
        if (!_host.readReply) return Result::NoTransport;
        if (!_host.readReply(_host.context, b, 4)) return Result::TransportError;
        parseStatus(b, 4, out); _lastStatus=out;
        if (_statusCallback) _statusCallback(_statusContext, out);
        return out.commandError() ? Result::DeviceError : Result::Ok;
    }

    /*
     * Begin one raw command.  The physical command write occurs immediately.
     * Waiting for CTS and reading `replyLength` bytes is handled by service().
     * The caller owns the reply buffer until the operation completes.
     */
    // SI468X-API: startCommand | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: command/state dependent | SI468X-AN649: common command/response engine
    Result startCommand(Command command, const uint8_t* args, uint16_t argLength,
                        uint8_t* reply=0, uint16_t replyLength=0,
                        uint32_t timeoutUs=1000000UL) {
        if (busy()) return Result::Busy;
        if (!_host.writeCommand || !_host.readReply) return Result::NoTransport;
        if (argLength && !args) return Result::InvalidArgument;
        if (replyLength && (!reply || replyLength < 4)) return Result::InvalidArgument;
        if (!_host.writeCommand(_host.context, (uint8_t)command, args, argLength))
            return Result::TransportError;
        const uint32_t now = nowUs();
        _reply=reply; _replyLength=replyLength;
        _deadline = now + timeoutUs;
        _nextCtsPoll = now;
        _lastResult=Result::Pending;
        _lastDeviceError=0;
        _state=State::WaitCts;
        return Result::Pending;
    }

    /*
     * Cooperative maintenance function.  Call it frequently from the main loop.
     * With INTB connected, notifyInterrupt() causes immediate status service.
     * Without INTB, the configured polling intervals provide a fallback.
     */
    // SI468X-API: service | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: command/state dependent | SI468X-AN649: common command/response engine
    Result service() {
        const uint32_t now = nowUs();
        if (_state == State::WaitCts) {
            if (_host.timeUs && timeReached(now, _deadline)) {
                finish(Result::Timeout); return _lastResult;
            }
            // If the platform does not provide a timer, poll once per service() call.
            // This keeps the cooperative non-blocking API usable on very small bare-metal
            // hosts; only timeout enforcement then remains unavailable.
            if (_host.timeUs && !_irqPending && _ctsPollIntervalUs && !timeReached(now, _nextCtsPoll))
                return Result::Pending;
            _irqPending=0;
            _nextCtsPoll = now + _ctsPollIntervalUs;
            uint8_t s[4];
            if (!_host.readReply(_host.context, s, 4)) { finish(Result::TransportError); return _lastResult; }
            parseStatus(s, 4, _lastStatus);
            if (_statusCallback) _statusCallback(_statusContext, _lastStatus);
            if (!_lastStatus.cts()) return Result::Pending;

            if (_replyLength) {
                if (!_host.readReply(_host.context, _reply, _replyLength)) {
                    finish(Result::TransportError); return _lastResult;
                }
                parseStatus(_reply, _replyLength, _lastStatus);
                if (_statusCallback) _statusCallback(_statusContext, _lastStatus);
                if (_lastStatus.commandError() && _replyLength > 4) _lastDeviceError=_reply[4];
            } else if (_lastStatus.commandError()) {
                // AN649 places the command error reason in the first response byte
                // after the 4-byte status word. Re-read five logical reply bytes so
                // lastDeviceError() remains useful even for commands whose normal
                // response has no payload.
                uint8_t e[5];
                if (!_host.readReply(_host.context, e, sizeof(e))) {
                    finish(Result::TransportError); return _lastResult;
                }
                parseStatus(e, sizeof(e), _lastStatus);
                _lastDeviceError=e[4];
                if (_statusCallback) _statusCallback(_statusContext, _lastStatus);
            }
            finish(_lastStatus.commandError() ? Result::DeviceError : Result::Ok);
            return _lastResult;
        }

        // Idle event maintenance.  It does not acknowledge any sticky source;
        // mode-specific handlers must explicitly issue the appropriate command.
        if (_irqPending || (_host.timeUs && _idlePollIntervalUs && timeReached(now, _nextIdlePoll))) {
            _irqPending=0;
            _nextIdlePoll=now + _idlePollIntervalUs;
            uint8_t s[4];
            if (!_host.readReply || !_host.readReply(_host.context, s, 4)) return Result::TransportError;
            parseStatus(s, 4, _lastStatus);
            if (_statusCallback) _statusCallback(_statusContext, _lastStatus);
        }
        return Result::Ok;
    }

    /* Blocking convenience wrapper built on the same state machine. */
    // SI468X-API: executeCommand | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: command/state dependent | SI468X-AN649: common command/response engine
    Result executeCommand(Command command, const uint8_t* args, uint16_t argLength,
                          uint8_t* reply=0, uint16_t replyLength=0,
                          uint32_t timeoutUs=1000000UL) {
        if (!_host.timeUs) return Result::NoTimer;
        Result r=startCommand(command,args,argLength,reply,replyLength,timeoutUs);
        if (r != Result::Pending) return r;
        while (busy()) {
            r=service();
            if (!busy()) break;
            if (_host.idle) _host.idle(_host.context);
        }
        return _lastResult;
    }

    /*
     * Wait for one or more top-level status bits. This is a blocking convenience
     * for very small applications. Event-driven applications should use INTB,
     * notifyInterrupt(), service() and the status callback instead.
     */
    // SI468X-API: waitForStatus0 | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: command/state dependent | SI468X-AN649: common command/response engine
    Result waitForStatus0(uint8_t mask, Status& out, uint32_t timeoutUs=3000000UL) {
        if (!_host.timeUs) return Result::NoTimer;
        const uint32_t start=nowUs();
        for (;;) {
            uint8_t b[4];
            if (!_host.readReply || !_host.readReply(_host.context,b,4)) return Result::TransportError;
            parseStatus(b,4,out); _lastStatus=out;
            if (_statusCallback) _statusCallback(_statusContext,out);
            if (out.status0 & mask) return out.commandError()?Result::DeviceError:Result::Ok;
            if ((uint32_t)(nowUs()-start)>=timeoutUs) return Result::Timeout;
            if (_host.idle) _host.idle(_host.context);
        }
    }
    // SI468X-API: waitForStc | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: command/state dependent | SI468X-AN649: common command/response engine
    Result waitForStc(Status& out, uint32_t timeoutUs=3000000UL) { return waitForStatus0(0x01u,out,timeoutUs); }

    // SI468X-API: setVolume | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: running application image with audio output | SI468X-AN649: AUDIO_ANALOG_VOLUME (0x0300) / AUDIO_MUTE (0x0301)
    Result setVolume(uint8_t volume, uint32_t timeoutUs=1000000UL) {
        if (volume>63u) return Result::InvalidArgument;
        return setProperty(Property::AUDIO_ANALOG_VOLUME,volume,timeoutUs);
    }
    // SI468X-API: setMute | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: running application image with audio output | SI468X-AN649: AUDIO_ANALOG_VOLUME (0x0300) / AUDIO_MUTE (0x0301)
    Result setMute(bool left, bool right, uint32_t timeoutUs=1000000UL) {
        const uint16_t value=(uint16_t)((left?1u:0u)|(right?2u:0u));
        return setProperty(Property::AUDIO_MUTE,value,timeoutUs);
    }
    // SI468X-API: setInterruptEnable | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: running application image | SI468X-AN649: INT_CTL_ENABLE (0x0000) / INT_CTL_REPEAT (0x0001)
    Result setInterruptEnable(uint16_t mask, uint32_t timeoutUs=1000000UL) {
        return setProperty(Property::INT_CTL_ENABLE,mask,timeoutUs);
    }
    // SI468X-API: setInterruptRepeat | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: running application image | SI468X-AN649: INT_CTL_ENABLE (0x0000) / INT_CTL_REPEAT (0x0001)
    Result setInterruptRepeat(uint16_t mask, uint32_t timeoutUs=1000000UL) {
        return setProperty(Property::INT_CTL_REPEAT,mask,timeoutUs);
    }
    // SI468X-API: setDigitalServiceInterruptSource | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: DAB/DAB+ or HD data-service image | SI468X-AN649: DIGITAL_SERVICE_INT_SOURCE (0x8100)
    Result setDigitalServiceInterruptSource(uint16_t mask, uint32_t timeoutUs=1000000UL) {
        return setProperty(Property::DIGITAL_SERVICE_INT_SOURCE,mask,timeoutUs);
    }

    /* Generic raw access: every command in Command and every future numeric CMD. */
    // SI468X-API: executeRaw | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: caller-defined; device/firmware response is authoritative | SI468X-AN649: raw command access
    Result executeRaw(uint8_t command, const uint8_t* args, uint16_t argLength,
                      uint8_t* reply=0, uint16_t replyLength=0,
                      uint32_t timeoutUs=1000000UL) {
        return executeCommand((Command)command,args,argLength,reply,replyLength,timeoutUs);
    }

    // -------------------------------------------------------------------------
    // Core / bootloader commands
    // -------------------------------------------------------------------------

    // SI468X-API: powerUp | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader / boot transition | SI468X-AN649: POWER_UP / LOAD_INIT / BOOT
    Result powerUp(const PowerUpConfig& c, uint32_t timeoutUs=1000000UL) {
        uint8_t a[15]; memset(a,0,sizeof(a));
        a[0] = c.ctsInterruptEnable ? 0x80u : 0u;
        a[1] = (uint8_t)(((c.clockMode & 0x03u) << 4) | (c.trSize & 0x0Fu));
        a[2] = (uint8_t)(c.iBias & 0x7Fu);
        writeLe32(a+3,c.crystalFrequencyHz);
        a[7] = (uint8_t)(c.cTune & 0x3Fu);
        a[8] = 0x10u; // AN649 reserved/fixed POWER_UP argument.
        a[12] = (uint8_t)(c.iBiasRun & 0x7Fu);
        Result r=executeCommand(Command::POWER_UP,a,15,0,0,timeoutUs);
        if (r==Result::Ok) r=delayUs(20); // AN649 loading flowchart.
        return r;
    }

    // SI468X-API: loadInit | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader / boot transition | SI468X-AN649: POWER_UP / LOAD_INIT / BOOT
    Result loadInit(uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; return executeCommand(Command::LOAD_INIT,a,1,0,0,timeoutUs);
    }
    // SI468X-API: boot | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader / boot transition | SI468X-AN649: POWER_UP / LOAD_INIT / BOOT
    Result boot(uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; return executeCommand(Command::BOOT,a,1,0,0,timeoutUs);
    }

    // SI468X-API: getPartInfo | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: supported firmware/boot revisions; AN649 notes A0A limitation | SI468X-AN649: GET_PART_INFO (0x08)
    Result getPartInfo(PartInfo& out, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={0}, r[23];
        Result x=executeCommand(Command::GET_PART_INFO,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        out.chipRevision=r[4]; out.romId=r[5]; out.partNumber=readLe16(r+8);
        switch (out.partNumber) {
            case 4682: _detectedPart=Part::Si4682; break; case 4683: _detectedPart=Part::Si4683; break;
            case 4684: _detectedPart=Part::Si4684; break; case 4685: _detectedPart=Part::Si4685; break;
            case 4688: _detectedPart=Part::Si4688; break; case 4689: _detectedPart=Part::Si4689; break;
            default: _detectedPart=Part::Unknown; break;
        }
        return Result::Ok;
    }

    // SI468X-API: getSystemState | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: supported firmware revisions; AN649 notes A0A limitation | SI468X-AN649: GET_SYS_STATE (0x09)
    Result getSystemState(SystemState& out, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={0}, r[6];
        Result x=executeCommand(Command::GET_SYS_STATE,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        parseStatus(r,sizeof(r),out.status); out.image=(Image)r[4]; _activeImage=out.image; return Result::Ok;
    }

    // SI468X-API: getFunctionInfo | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: running application image | SI468X-AN649: GET_FUNC_INFO (0x12)
    Result getFunctionInfo(FunctionInfo& out, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={0}, r[12];
        Result x=executeCommand(Command::GET_FUNC_INFO,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        out.major=r[4]; out.minor=r[5]; out.build=r[6]; out.flags=r[7]; out.svnId=readLe32(r+8);
        return Result::Ok;
    }

    // SI468X-API: getPowerUpArgs | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: booted/powered device as supported by image | SI468X-AN649: GET_POWER_UP_ARGS (0x0A)
    Result getPowerUpArgs(PowerUpArgs& out, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={0}, r[18];
        Result x=executeCommand(Command::GET_POWER_UP_ARGS,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        out.clockMode=(uint8_t)((r[6]>>4)&3u); out.trSize=(uint8_t)(r[6]&0x0Fu);
        out.iBias=(uint8_t)(r[7]&0x7Fu); out.crystalFrequencyHz=readLe32(r+8);
        out.cTune=(uint8_t)(r[12]&0x3Fu); out.iBiasRun=(uint8_t)(r[17]&0x7Fu);
        return Result::Ok;
    }

    // SI468X-API: setProperty | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: depends on the selected Property and active image | SI468X-AN649: SET_PROPERTY (0x13) / GET_PROPERTY (0x14)
    Result setProperty(uint16_t property, uint16_t value, uint32_t timeoutUs=1000000UL) {
        uint8_t a[5]; a[0]=0; writeLe16(a+1,property); writeLe16(a+3,value);
        return executeCommand(Command::SET_PROPERTY,a,5,0,0,timeoutUs);
    }
    // SI468X-API: setProperty | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: depends on the selected Property and active image | SI468X-AN649: SET_PROPERTY (0x13) / GET_PROPERTY (0x14)
    Result setProperty(Property property, uint16_t value, uint32_t timeoutUs=1000000UL) {
        return setProperty((uint16_t)property,value,timeoutUs);
    }

    // SI468X-API: getProperty | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: depends on the selected Property and active image | SI468X-AN649: SET_PROPERTY (0x13) / GET_PROPERTY (0x14)
    Result getProperty(uint16_t property, uint16_t& value, uint32_t timeoutUs=1000000UL) {
        uint8_t a[3]; a[0]=1; writeLe16(a+1,property); uint8_t r[6];
        Result x=executeCommand(Command::GET_PROPERTY,a,3,r,sizeof(r),timeoutUs);
        if (x==Result::Ok) value=readLe16(r+4);
        return x;
    }
    // SI468X-API: getProperty | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: depends on the selected Property and active image | SI468X-AN649: SET_PROPERTY (0x13) / GET_PROPERTY (0x14)
    Result getProperty(Property property, uint16_t& value, uint32_t timeoutUs=1000000UL) {
        return getProperty((uint16_t)property,value,timeoutUs);
    }

    /* READ_OFFSET returns 4 status bytes followed by up to `dataLength` bytes. */
    // SI468X-API: readOffset | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: image/boot revision supporting READ_OFFSET | SI468X-AN649: READ_OFFSET (0x10)
    Result readOffset(uint16_t offset, uint8_t* reply, uint16_t dataLength,
                      uint32_t timeoutUs=1000000UL) {
        if ((offset & 3u) != 0 || !reply) return Result::InvalidArgument;
        uint8_t a[3]; a[0]=0; writeLe16(a+1,offset);
        return executeCommand(Command::READ_OFFSET,a,3,reply,(uint16_t)(dataLength+4u),timeoutUs);
    }

    // SI468X-API: writeStorage | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: image supporting storage commands | SI468X-AN649: WRITE_STORAGE (0x15) / READ_STORAGE (0x16)
    Result writeStorage(uint16_t offset, const uint8_t* data, uint16_t length,
                        uint32_t timeoutUs=1000000UL) {
        if (!data || !length || length>256u) return Result::InvalidArgument;
        if (!_workspace || _workspaceSize < (size_t)length+7u) return Result::BufferTooSmall;
        _workspace[0]=0; writeLe16(_workspace+1,offset); writeLe16(_workspace+3,length);
        _workspace[5]=0; _workspace[6]=0; memcpy(_workspace+7,data,length);
        return executeCommand(Command::WRITE_STORAGE,_workspace,(uint16_t)(length+7u),0,0,timeoutUs);
    }

    // SI468X-API: readStorage | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: image supporting storage commands | SI468X-AN649: WRITE_STORAGE (0x15) / READ_STORAGE (0x16)
    Result readStorage(uint16_t offset, uint8_t* reply, uint16_t replyLength,
                       uint32_t timeoutUs=1000000UL) {
        if (!reply || replyLength<5) return Result::InvalidArgument;
        uint8_t a[3]; a[0]=0; writeLe16(a+1,offset);
        return executeCommand(Command::READ_STORAGE,a,3,reply,replyLength,timeoutUs);
    }

    // -------------------------------------------------------------------------
    // Host image loading and generic boot sequences. Firmware bytes are supplied
    // by an ImageReader; this library has no dependency on any firmware package.
    // -------------------------------------------------------------------------

    // SI468X-API: hostLoadImage | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader; A10 requires appropriate boot patch before application boot | SI468X-AN649: HOST_LOAD / FLASH_LOAD boot flows
    Result hostLoadImage(ImageReader reader, void* readerContext, uint32_t imageSize,
                         uint32_t timeoutPerChunkUs=1000000UL) {
        if (!reader || !imageSize) return Result::InvalidArgument;
        if (!_workspace || _workspaceSize < 8u) return Result::BufferTooSmall;
        size_t maxData=_workspaceSize-3u;
        if (maxData>4096u) maxData=4096u;
        maxData &= ~(size_t)3u; // AN649 recommends multiples of four.
        if (maxData<4u) return Result::BufferTooSmall;
        uint32_t off=0;
        while (off<imageSize) {
            size_t n=(size_t)(imageSize-off); if (n>maxData) n=maxData;
            // Only the final chunk may be non-multiple-of-four if the image itself is.
            if (off+n<imageSize) n &= ~(size_t)3u;
            _workspace[0]=_workspace[1]=_workspace[2]=0;
            size_t got=reader(readerContext,off,_workspace+3,n);
            if (got!=n) return Result::EndOfData;
            Result x=executeCommand(Command::HOST_LOAD,_workspace,(uint16_t)(n+3u),0,0,timeoutPerChunkUs);
            if (x!=Result::Ok) return x;
            off+=(uint32_t)n;
        }
        return Result::Ok;
    }

    // SI468X-API: bootHostImage | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader; A10 requires appropriate boot patch before application boot | SI468X-AN649: HOST_LOAD / FLASH_LOAD boot flows
    Result bootHostImage(const PowerUpConfig& cfg,
                         ImageReader patchReader, void* patchContext, uint32_t patchSize,
                         ImageReader imageReader, void* imageContext, uint32_t imageSize,
                         uint32_t timeoutPerCommandUs=1000000UL) {
        Result x=powerUp(cfg,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=loadInit(timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=hostLoadImage(patchReader,patchContext,patchSize,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=delayUs(4000); if (x!=Result::Ok) return x;
        x=loadInit(timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=hostLoadImage(imageReader,imageContext,imageSize,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        return boot(timeoutPerCommandUs);
    }

    // SI468X-API: bootNvspiWithHostFullPatch | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader; A10 requires appropriate boot patch before application boot | SI468X-AN649: HOST_LOAD / FLASH_LOAD boot flows
    Result bootNvspiWithHostFullPatch(const PowerUpConfig& cfg,
                                      ImageReader fullPatchReader, void* patchContext, uint32_t patchSize,
                                      uint32_t firmwareFlashAddress,
                                      uint32_t timeoutPerCommandUs=1000000UL) {
        Result x=powerUp(cfg,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=loadInit(timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=hostLoadImage(fullPatchReader,patchContext,patchSize,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=delayUs(4000); if (x!=Result::Ok) return x;
        x=loadInit(timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=flashLoadImage(firmwareFlashAddress,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        return boot(timeoutPerCommandUs);
    }

    // SI468X-API: bootNvspiWithMiniPatch | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader; A10 requires appropriate boot patch before application boot | SI468X-AN649: HOST_LOAD / FLASH_LOAD boot flows
    Result bootNvspiWithMiniPatch(const PowerUpConfig& cfg,
                                  ImageReader miniPatchReader, void* miniContext, uint32_t miniPatchSize,
                                  uint32_t fullPatchFlashAddress, uint32_t firmwareFlashAddress,
                                  uint32_t timeoutPerCommandUs=1000000UL) {
        Result x=powerUp(cfg,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=loadInit(timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=hostLoadImage(miniPatchReader,miniContext,miniPatchSize,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=delayUs(4000); if (x!=Result::Ok) return x;
        x=loadInit(timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=flashLoadImage(fullPatchFlashAddress,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=loadInit(timeoutPerCommandUs); if (x!=Result::Ok) return x;
        x=flashLoadImage(firmwareFlashAddress,timeoutPerCommandUs); if (x!=Result::Ok) return x;
        return boot(timeoutPerCommandUs);
    }

    // -------------------------------------------------------------------------
    // FM / FMHD PUBLIC API
    // SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689
    // SI468X-FIRMWARE: FM/FMHD
    // Every public method below also carries a local SI468X-API support tag.
    // -------------------------------------------------------------------------
    // FM/FMHD typed command helpers.
    // -------------------------------------------------------------------------

    /*
     * Non-blocking FM tune start. The command bytes are written immediately and
     * service() completes the CTS phase. RF acquisition/tune completion is a
     * separate STC event; use INTB/status handling or waitForStc() if required.
     */
    // SI468X-API: startFmTune | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: FM/FMHD | SI468X-AN649: FM command set (0x30..0x33)
    Result startFmTune(uint16_t frequency10kHz, TuneMode mode=TuneMode::AnalogOnly,
                       Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                       uint8_t programId=0, bool dirTune=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[6];
        a[0]=(uint8_t)((dirTune?0x20u:0u) | (((uint8_t)mode&3u)<<2) | ((uint8_t)injection&3u));
        writeLe16(a+1,frequency10kHz); writeLe16(a+3,antennaCap); a[5]=programId;
        return startCommand(Command::FM_TUNE_FREQ,a,6,0,0,timeoutUs);
    }

    // SI468X-API: startFmSeek | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: FM/FMHD | SI468X-AN649: FM command set (0x30..0x33)
    Result startFmSeek(bool up, bool wrap, TuneMode mode=TuneMode::AnalogOnly,
                       Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                       bool forceWideband=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[5];
        a[0]=(uint8_t)((forceWideband?0x10u:0u) | (((uint8_t)mode&3u)<<2) | ((uint8_t)injection&3u));
        a[1]=(uint8_t)((up?2u:0u)|(wrap?1u:0u)); a[2]=0; writeLe16(a+3,antennaCap);
        return startCommand(Command::FM_SEEK_START,a,5,0,0,timeoutUs);
    }

    /*
     * Blocking convenience wrapper. It waits only until the command reaches CTS;
     * it does NOT wait for the later STC tune-complete event.
     */
    // SI468X-API: fmTune | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: FM/FMHD | SI468X-AN649: FM command set (0x30..0x33)
    Result fmTune(uint16_t frequency10kHz, TuneMode mode=TuneMode::AnalogOnly,
                  Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                  uint8_t programId=0, bool dirTune=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[6];
        a[0]=(uint8_t)((dirTune?0x20u:0u) | (((uint8_t)mode&3u)<<2) | ((uint8_t)injection&3u));
        writeLe16(a+1,frequency10kHz); writeLe16(a+3,antennaCap); a[5]=programId;
        return executeCommand(Command::FM_TUNE_FREQ,a,6,0,0,timeoutUs);
    }

    // SI468X-API: fmSeek | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: FM/FMHD | SI468X-AN649: FM command set (0x30..0x33)
    Result fmSeek(bool up, bool wrap, TuneMode mode=TuneMode::AnalogOnly,
                  Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                  bool forceWideband=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[5];
        a[0]=(uint8_t)((forceWideband?0x10u:0u) | (((uint8_t)mode&3u)<<2) | ((uint8_t)injection&3u));
        a[1]=(uint8_t)((up?2u:0u)|(wrap?1u:0u)); a[2]=0; writeLe16(a+3,antennaCap);
        return executeCommand(Command::FM_SEEK_START,a,5,0,0,timeoutUs);
    }

    // SI468X-API: fmRsqStatus | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: FM/FMHD | SI468X-AN649: FM command set (0x30..0x33)
    Result fmRsqStatus(FmRsqStatus& out, bool rsqAck=false, bool attune=false,
                       bool cancel=false, bool stcAck=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)((rsqAck?8u:0u)|(attune?4u:0u)|(cancel?2u:0u)|(stcAck?1u:0u))};
        uint8_t r[18]; Result x=executeCommand(Command::FM_RSQ_STATUS,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseFmRsqStatus(r,sizeof(r),out);
    }

    // SI468X-API: fmAcfStatus | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: FM/FMHD | SI468X-AN649: FM command set (0x30..0x33)
    Result fmAcfStatus(FmAcfStatus& out, bool ack=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)(ack?1u:0u)}; uint8_t r[10];
        Result x=executeCommand(Command::FM_ACF_STATUS,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseFmAcfStatus(r,sizeof(r),out);
    }

    // SI468X-API: fmRdsStatus | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: FM/FMHD with RDS enabled/configured | SI468X-AN649: FM_RDS_STATUS (0x34) / FM_RDS_BLOCKCOUNT (0x35)
    Result fmRdsStatus(FmRdsGroup& out, bool statusOnly=false, bool clearFifo=false,
                       bool interruptAck=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)((statusOnly?4u:0u)|(clearFifo?2u:0u)|(interruptAck?1u:0u))};
        uint8_t r[20]; Result x=executeCommand(Command::FM_RDS_STATUS,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseFmRdsStatus(r,sizeof(r),out);
    }

    // SI468X-API: fmRdsBlockCount | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: FM/FMHD with RDS enabled/configured | SI468X-AN649: FM_RDS_STATUS (0x34) / FM_RDS_BLOCKCOUNT (0x35)
    Result fmRdsBlockCount(FmRdsBlockCount& out, bool clear=false,
                           uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)(clear?1u:0u)}; uint8_t r[10];
        Result x=executeCommand(Command::FM_RDS_BLOCKCOUNT,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        parseStatus(r,sizeof(r),out.status);
        out.expected=readLe16(r+4); out.received=readLe16(r+6); out.uncorrectable=readLe16(r+8);
        return Result::Ok;
    }

    // -------------------------------------------------------------------------
    // AM / AMHD PUBLIC API
    // SI468X-SUPPORT: Si4683 Si4685 Si4689
    // SI468X-FIRMWARE: AM/AMHD
    // -------------------------------------------------------------------------
    // AM/AMHD typed command helpers.
    // -------------------------------------------------------------------------

    // SI468X-API: startAmTune | SI468X-SUPPORT: Si4683 Si4685 Si4689 | SI468X-FIRMWARE: AM/AMHD | SI468X-AN649: AM command set (0x40..0x43)
    Result startAmTune(uint16_t frequencyKHz, TuneMode mode=TuneMode::AnalogOnly,
                       Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                       uint32_t timeoutUs=1000000UL) {
        uint8_t a[5]; a[0]=(uint8_t)((((uint8_t)mode&3u)<<2)|((uint8_t)injection&3u));
        writeLe16(a+1,frequencyKHz); writeLe16(a+3,antennaCap);
        return startCommand(Command::AM_TUNE_FREQ,a,5,0,0,timeoutUs);
    }

    // SI468X-API: startAmSeek | SI468X-SUPPORT: Si4683 Si4685 Si4689 | SI468X-FIRMWARE: AM/AMHD | SI468X-AN649: AM command set (0x40..0x43)
    Result startAmSeek(bool up, bool wrap, TuneMode mode=TuneMode::AnalogOnly,
                       Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                       bool forceWideband=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[5]; a[0]=(uint8_t)((forceWideband?0x10u:0u)|(((uint8_t)mode&3u)<<2)|((uint8_t)injection&3u));
        a[1]=(uint8_t)((up?2u:0u)|(wrap?1u:0u)); a[2]=0; writeLe16(a+3,antennaCap);
        return startCommand(Command::AM_SEEK_START,a,5,0,0,timeoutUs);
    }

    // SI468X-API: amTune | SI468X-SUPPORT: Si4683 Si4685 Si4689 | SI468X-FIRMWARE: AM/AMHD | SI468X-AN649: AM command set (0x40..0x43)
    Result amTune(uint16_t frequencyKHz, TuneMode mode=TuneMode::AnalogOnly,
                  Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                  uint32_t timeoutUs=1000000UL) {
        uint8_t a[5]; a[0]=(uint8_t)((((uint8_t)mode&3u)<<2)|((uint8_t)injection&3u));
        writeLe16(a+1,frequencyKHz); writeLe16(a+3,antennaCap);
        return executeCommand(Command::AM_TUNE_FREQ,a,5,0,0,timeoutUs);
    }

    // SI468X-API: amSeek | SI468X-SUPPORT: Si4683 Si4685 Si4689 | SI468X-FIRMWARE: AM/AMHD | SI468X-AN649: AM command set (0x40..0x43)
    Result amSeek(bool up, bool wrap, TuneMode mode=TuneMode::AnalogOnly,
                  Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                  bool forceWideband=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[5]; a[0]=(uint8_t)((forceWideband?0x10u:0u)|(((uint8_t)mode&3u)<<2)|((uint8_t)injection&3u));
        a[1]=(uint8_t)((up?2u:0u)|(wrap?1u:0u)); a[2]=0; writeLe16(a+3,antennaCap);
        return executeCommand(Command::AM_SEEK_START,a,5,0,0,timeoutUs);
    }

    // SI468X-API: amRsqStatus | SI468X-SUPPORT: Si4683 Si4685 Si4689 | SI468X-FIRMWARE: AM/AMHD | SI468X-AN649: AM command set (0x40..0x43)
    Result amRsqStatus(AmRsqStatus& out, bool rsqAck=false, bool attune=false,
                       bool cancel=false, bool stcAck=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)((rsqAck?8u:0u)|(attune?4u:0u)|(cancel?2u:0u)|(stcAck?1u:0u))};
        uint8_t r[17]; Result x=executeCommand(Command::AM_RSQ_STATUS,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseAmRsqStatus(r,sizeof(r),out);
    }

    // SI468X-API: amAcfStatus | SI468X-SUPPORT: Si4683 Si4685 Si4689 | SI468X-FIRMWARE: AM/AMHD | SI468X-AN649: AM command set (0x40..0x43)
    Result amAcfStatus(AmAcfStatus& out, bool ack=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)(ack?1u:0u)}; uint8_t r[9];
        Result x=executeCommand(Command::AM_ACF_STATUS,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseAmAcfStatus(r,sizeof(r),out);
    }

    // -------------------------------------------------------------------------
    // DAB / DAB+ PUBLIC API
    // SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689
    // SI468X-FIRMWARE: DAB/DAB+
    // -------------------------------------------------------------------------
    // DAB/DAB+ typed command helpers.
    // -------------------------------------------------------------------------

    // SI468X-API: startDabTune | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB tune/status command set
    Result startDabTune(uint8_t frequencyIndex, Injection injection=Injection::Automatic,
                        uint16_t antennaCap=0, uint32_t timeoutUs=1000000UL) {
        uint8_t a[5]; a[0]=(uint8_t)((uint8_t)injection&3u); a[1]=frequencyIndex; a[2]=0;
        writeLe16(a+3,antennaCap);
        return startCommand(Command::DAB_TUNE_FREQ,a,5,0,0,timeoutUs);
    }

    // SI468X-API: dabTune | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB tune/status command set
    Result dabTune(uint8_t frequencyIndex, Injection injection=Injection::Automatic,
                   uint16_t antennaCap=0, uint32_t timeoutUs=1000000UL) {
        uint8_t a[5]; a[0]=(uint8_t)((uint8_t)injection&3u); a[1]=frequencyIndex; a[2]=0;
        writeLe16(a+3,antennaCap);
        return executeCommand(Command::DAB_TUNE_FREQ,a,5,0,0,timeoutUs);
    }

    // SI468X-API: dabDigradStatus | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB tune/status command set
    Result dabDigradStatus(DabDigradStatus& out, bool ack=false, bool attune=false,
                           bool stcAck=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)((ack?8u:0u)|(attune?4u:0u)|(stcAck?1u:0u))};
        uint8_t r[23]; Result x=executeCommand(Command::DAB_DIGRAD_STATUS,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseDabDigradStatus(r,sizeof(r),out);
    }

    // SI468X-API: dabSetFrequencyList | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB tune/status command set
    Result dabSetFrequencyList(const uint32_t* frequencyKHz, uint8_t count,
                               uint32_t timeoutUs=1000000UL) {
        if (!frequencyKHz || !count) return Result::InvalidArgument;
        size_t needed=3u+(size_t)count*4u;
        if (!_workspace || _workspaceSize<needed) return Result::BufferTooSmall;
        _workspace[0]=count; _workspace[1]=0; _workspace[2]=0;
        for (uint8_t i=0;i<count;++i) writeLe32(_workspace+3u+(size_t)i*4u,frequencyKHz[i]);
        return executeCommand(Command::DAB_SET_FREQ_LIST,_workspace,(uint16_t)needed,0,0,timeoutUs);
    }

    /*
     * START/STOP_DIGITAL_SERVICE is shared by DAB and HD applications.
     *
     * DAB/DAB+: AN649 section 7.6 explicitly states that SERTYPE is not used
     * and must be written as 0, including for DAB data components. Use the
     * startDabService()/stopDabService() convenience methods below.
     *
     * HD Radio: SERTYPE selects audio (0) or data (1). HD service/program/port
     * semantics are defined by the HD API references cited by AN649.
     */
    // SI468X-API: startDigitalService | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: DAB/DAB+ or HD-capable FMHD/AMHD image | SI468X-AN649: digital service transport (0x80..0x84)
    Result startDigitalService(uint8_t serviceType, uint32_t serviceId, uint32_t componentId,
                               uint32_t timeoutUs=1000000UL) {
        if (serviceType>1u) return Result::InvalidArgument;
        uint8_t a[11]; a[0]=serviceType; a[1]=a[2]=0; writeLe32(a+3,serviceId); writeLe32(a+7,componentId);
        return executeCommand(Command::START_DIGITAL_SERVICE,a,11,0,0,timeoutUs);
    }
    // SI468X-API: stopDigitalService | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: DAB/DAB+ or HD-capable FMHD/AMHD image | SI468X-AN649: digital service transport (0x80..0x84)
    Result stopDigitalService(uint8_t serviceType, uint32_t serviceId, uint32_t componentId,
                              uint32_t timeoutUs=1000000UL) {
        if (serviceType>1u) return Result::InvalidArgument;
        uint8_t a[11]; a[0]=serviceType; a[1]=a[2]=0; writeLe32(a+3,serviceId); writeLe32(a+7,componentId);
        return executeCommand(Command::STOP_DIGITAL_SERVICE,a,11,0,0,timeoutUs);
    }
    // SI468X-API: startDabService | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB service helper over digital-service commands
    Result startDabService(uint32_t serviceId, uint32_t componentId, uint32_t timeoutUs=1000000UL) {
        return startDigitalService(0,serviceId,componentId,timeoutUs);
    }
    // SI468X-API: stopDabService | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB service helper over digital-service commands
    Result stopDabService(uint32_t serviceId, uint32_t componentId, uint32_t timeoutUs=1000000UL) {
        return stopDigitalService(0,serviceId,componentId,timeoutUs);
    }

    /* Tune an ensemble and start the selected DAB component. */
    // SI468X-API: dabTuneService | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB service helper over digital-service commands
    Result dabTuneService(uint8_t frequencyIndex, uint32_t serviceId, uint32_t componentId,
                          Injection injection=Injection::Automatic, uint16_t antennaCap=0,
                          uint32_t tuneTimeoutUs=3000000UL, uint32_t commandTimeoutUs=1000000UL) {
        Result r=dabTune(frequencyIndex,injection,antennaCap,commandTimeoutUs);
        if (r!=Result::Ok) return r;
        Status st; r=waitForStc(st,tuneTimeoutUs);
        if (r!=Result::Ok) return r;
        return startDabService(serviceId,componentId,commandTimeoutUs);
    }

    /*
     * Retrieve and stream the complete DAB service list through READ_OFFSET.
     * The caller controls RAM usage only by the workspace supplied to Si468x.
     * The parser itself needs only a small fixed record buffer.
     */
    // SI468X-API: readDabServiceList | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB service helper over digital-service commands
    Result readDabServiceList(DabServiceListParser& parser, uint16_t preferredChunk=0,
                              uint32_t timeoutUs=1000000UL);

    /*
     * Request a digital service list. SERTYPE is a two-bit field in AN649:
     *   0 = DAB complete ensemble list / default HD form
     *   1 = HD data service list
     *   2 = HD audio information list
     *   3 = HD data information list
     * The DAB/DMB application always uses 0. HD list payload layouts are
     * specified by external HD Radio documents, therefore this method keeps
     * the response raw.
     */
    // SI468X-API: getDigitalServiceList | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: DAB/DAB+ or HD-capable FMHD/AMHD image | SI468X-AN649: digital service transport (0x80..0x84)
    Result getDigitalServiceList(uint8_t serviceType, uint8_t* reply, uint16_t replyLength,
                                 uint32_t timeoutUs=1000000UL) {
        if (serviceType>3u) return Result::InvalidArgument;
        uint8_t a[1]={(uint8_t)(serviceType&0x03u)};
        return executeCommand(Command::GET_DIGITAL_SERVICE_LIST,a,1,reply,replyLength,timeoutUs);
    }

    /*
     * Raw DSRV transaction. For large payloads, a memory-constrained host can
     * first call getDigitalServiceDataHeader() and then retrieve the preserved
     * response in chunks with READ_OFFSET instead of allocating the full block.
     */
    // SI468X-API: getDigitalServiceData | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: DAB/DAB+ or HD-capable FMHD/AMHD image | SI468X-AN649: digital service transport (0x80..0x84)
    Result getDigitalServiceData(bool statusOnly, bool ack, uint8_t* reply, uint16_t replyLength,
                                 uint32_t timeoutUs=1000000UL) {
        if (!reply || replyLength<4u) return Result::InvalidArgument;
        uint8_t a[1]={(uint8_t)((statusOnly?0x10u:0u)|(ack?1u:0u))};
        return executeCommand(Command::GET_DIGITAL_SERVICE_DATA,a,1,reply,replyLength,timeoutUs);
    }

    /* Header-first DSRV read. The caller may then re-read the full current reply. */
    // SI468X-API: getDigitalServiceDataHeader | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: DAB/DAB+ or HD-capable FMHD/AMHD image | SI468X-AN649: digital service transport (0x80..0x84)
    Result getDigitalServiceDataHeader(DsrvHeader& header, bool statusOnly=false, bool ack=true,
                                       uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)((statusOnly?0x10u:0u)|(ack?1u:0u))};
        uint8_t r[24]; Result x=executeCommand(Command::GET_DIGITAL_SERVICE_DATA,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseDsrvHeader(r,sizeof(r),header);
    }

    /* Raw DAB information commands. Their complete replies are passed untouched. */
    // SI468X-API: dabGetEventStatus | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetEventStatus(DabEventStatus& out, bool ack=false, uint32_t timeoutUs=1000000UL) {
        uint8_t a[1]={(uint8_t)(ack?1u:0u)}, r[8];
        Result x=executeCommand(Command::DAB_GET_EVENT_STATUS,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseDabEventStatus(r,sizeof(r),out);
    }
    // SI468X-API: dabGetEventStatusRaw | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetEventStatusRaw(uint8_t arg1, uint8_t* reply, uint16_t replyLength, uint32_t timeoutUs=1000000UL) {
        return executeCommand(Command::DAB_GET_EVENT_STATUS,&arg1,1,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetEnsembleInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetEnsembleInfo(DabEnsembleInfo& out, uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; uint8_t r[26];
        Result x=executeCommand(Command::DAB_GET_ENSEMBLE_INFO,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseDabEnsembleInfo(r,sizeof(r),out);
    }
    // SI468X-API: dabGetEnsembleInfoRaw | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetEnsembleInfoRaw(uint8_t* reply, uint16_t replyLength, uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; return executeCommand(Command::DAB_GET_ENSEMBLE_INFO,a,1,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetServiceLinkingInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetServiceLinkingInfo(uint32_t serviceId, uint8_t* reply, uint16_t replyLength,
                                    uint32_t timeoutUs=1000000UL) {
        uint8_t a[7]={0,0,0,0,0,0,0}; writeLe32(a+3,serviceId);
        return executeCommand(Command::DAB_GET_SERVICE_LINKING_INFO,a,7,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetFrequencyList | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetFrequencyList(uint8_t* reply, uint16_t replyLength, uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; return executeCommand(Command::DAB_GET_FREQ_LIST,a,1,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetComponentInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetComponentInfo(uint32_t serviceId, uint32_t componentId, uint8_t* reply, uint16_t replyLength,
                               uint32_t timeoutUs=1000000UL) {
        uint8_t a[11]; a[0]=a[1]=a[2]=0; writeLe32(a+3,serviceId); writeLe32(a+7,componentId);
        return executeCommand(Command::DAB_GET_COMPONENT_INFO,a,11,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetTime | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetTime(uint8_t timeType, DabTimeInfo& out, uint32_t timeoutUs=1000000UL) {
        if (timeType > 1u) return Result::InvalidArgument;
        uint8_t r[11]; Result x=executeCommand(Command::DAB_GET_TIME,&timeType,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseDabTime(r,sizeof(r),out);
    }
    // SI468X-API: dabGetTimeRaw | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetTimeRaw(uint8_t timeType, uint8_t* reply, uint16_t replyLength, uint32_t timeoutUs=1000000UL) {
        if (timeType > 1u) return Result::InvalidArgument;
        return executeCommand(Command::DAB_GET_TIME,&timeType,1,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetAudioInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetAudioInfo(DabAudioInfo& out, uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; uint8_t r[10];
        Result x=executeCommand(Command::DAB_GET_AUDIO_INFO,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseDabAudioInfo(r,sizeof(r),out);
    }
    // SI468X-API: dabGetAudioInfoRaw | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetAudioInfoRaw(uint8_t* reply, uint16_t replyLength, uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; return executeCommand(Command::DAB_GET_AUDIO_INFO,a,1,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetSubchannelInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetSubchannelInfo(uint32_t serviceId, uint32_t componentId, DabSubchannelInfo& out,
                                uint32_t timeoutUs=1000000UL) {
        uint8_t a[11], r[12]; a[0]=a[1]=a[2]=0; writeLe32(a+3,serviceId); writeLe32(a+7,componentId);
        Result x=executeCommand(Command::DAB_GET_SUBCHAN_INFO,a,11,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseDabSubchannelInfo(r,sizeof(r),out);
    }
    // SI468X-API: dabGetSubchannelInfoRaw | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetSubchannelInfoRaw(uint32_t serviceId, uint32_t componentId, uint8_t* reply, uint16_t replyLength,
                                   uint32_t timeoutUs=1000000UL) {
        uint8_t a[11]; a[0]=a[1]=a[2]=0; writeLe32(a+3,serviceId); writeLe32(a+7,componentId);
        return executeCommand(Command::DAB_GET_SUBCHAN_INFO,a,11,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetFrequencyInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetFrequencyInfo(uint8_t* reply, uint16_t replyLength, uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; return executeCommand(Command::DAB_GET_FREQ_INFO,a,1,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabGetServiceInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetServiceInfo(uint32_t serviceId, DabServiceInfo& out, uint32_t timeoutUs=1000000UL) {
        uint8_t a[7], r[26]; a[0]=a[1]=a[2]=0; writeLe32(a+3,serviceId);
        Result x=executeCommand(Command::DAB_GET_SERVICE_INFO,a,7,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        return parseDabServiceInfo(r,sizeof(r),out);
    }
    // SI468X-API: dabGetServiceInfoRaw | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabGetServiceInfoRaw(uint32_t serviceId, uint8_t* reply, uint16_t replyLength,
                                uint32_t timeoutUs=1000000UL) {
        uint8_t a[7]; a[0]=a[1]=a[2]=0; writeLe32(a+3,serviceId);
        return executeCommand(Command::DAB_GET_SERVICE_INFO,a,7,reply,replyLength,timeoutUs);
    }
    // SI468X-API: dabTestGetBerInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabTestGetBerInfo(DabBerInfo& out, uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; uint8_t r[12];
        Result x=executeCommand(Command::DAB_TEST_GET_BER_INFO,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        parseStatus(r,sizeof(r),out.status); out.errorBits=readLe32(r+4); out.totalBits=readLe32(r+8);
        return Result::Ok;
    }

    /* Raw overload retained for firmware/test revisions with additional arguments. */
    // SI468X-API: dabTestGetBerInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: DAB/DAB+ | SI468X-AN649: DAB command set (0xB3..0xE8 as applicable)
    Result dabTestGetBerInfo(const uint8_t* args, uint16_t argLength, uint8_t* reply, uint16_t replyLength,
                             uint32_t timeoutUs=1000000UL) {
        return executeCommand(Command::DAB_TEST_GET_BER_INFO,args,argLength,reply,replyLength,timeoutUs);
    }

    // -------------------------------------------------------------------------
    // HD Radio commands. The command transport is complete; payloads whose
    // meaning AN649 delegates to iBiquity specifications are intentionally raw.
    // -------------------------------------------------------------------------

    // SI468X-API: hdDigradStatus | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdDigradStatus(const uint8_t* args, uint16_t argLength, uint8_t* reply, uint16_t replyLength,
                          uint32_t timeoutUs=1000000UL) { return executeCommand(Command::HD_DIGRAD_STATUS,args,argLength,reply,replyLength,timeoutUs); }
    // SI468X-API: hdGetEventStatus | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdGetEventStatus(const uint8_t* args, uint16_t argLength, uint8_t* reply, uint16_t replyLength,
                            uint32_t timeoutUs=1000000UL) { return executeCommand(Command::HD_GET_EVENT_STATUS,args,argLength,reply,replyLength,timeoutUs); }
    // SI468X-API: hdGetStationInfo | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdGetStationInfo(uint8_t infoSelect, uint8_t* reply, uint16_t replyLength,
                            uint32_t timeoutUs=1000000UL) { return executeCommand(Command::HD_GET_STATION_INFO,&infoSelect,1,reply,replyLength,timeoutUs); }
    // SI468X-API: hdGetPsdDecode | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdGetPsdDecode(uint8_t program, uint8_t field, uint8_t* reply, uint16_t replyLength,
                          uint32_t timeoutUs=1000000UL) { uint8_t a[2]={program,field}; return executeCommand(Command::HD_GET_PSD_DECODE,a,2,reply,replyLength,timeoutUs); }
    // SI468X-API: hdGetAlertMessage | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdGetAlertMessage(const uint8_t* args, uint16_t argLength, uint8_t* reply, uint16_t replyLength,
                             uint32_t timeoutUs=1000000UL) { return executeCommand(Command::HD_GET_ALERT_MSG,args,argLength,reply,replyLength,timeoutUs); }
    // SI468X-API: hdPlayAlertTone | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdPlayAlertTone(const uint8_t* args, uint16_t argLength, uint8_t* reply, uint16_t replyLength,
                           uint32_t timeoutUs=1000000UL) { return executeCommand(Command::HD_PLAY_ALERT_TONE,args,argLength,reply,replyLength,timeoutUs); }
    // SI468X-API: hdTestGetBerInfo | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdTestGetBerInfo(const uint8_t* args, uint16_t argLength, uint8_t* reply, uint16_t replyLength,
                            uint32_t timeoutUs=1000000UL) { return executeCommand(Command::HD_TEST_GET_BER_INFO,args,argLength,reply,replyLength,timeoutUs); }
    // SI468X-API: hdSetEnabledPorts | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdSetEnabledPorts(const uint8_t* args, uint16_t argLength, uint8_t* reply=0, uint16_t replyLength=0,
                             uint32_t timeoutUs=1000000UL) { return executeCommand(Command::HD_SET_ENABLED_PORTS,args,argLength,reply,replyLength,timeoutUs); }
    // SI468X-API: hdGetEnabledPorts | SI468X-SUPPORT: Si4682 Si4683 Si4688 Si4689 | SI468X-FIRMWARE: FMHD; AMHD additionally on Si4683/Si4689 where command applies | SI468X-AN649: HD Radio command set (0x92..0x9A)
    Result hdGetEnabledPorts(const uint8_t* args, uint16_t argLength, uint8_t* reply, uint16_t replyLength,
                             uint32_t timeoutUs=1000000UL) { return executeCommand(Command::HD_GET_ENABLED_PORTS,args,argLength,reply,replyLength,timeoutUs); }

    // SI468X-API: testGetRssi | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: diagnostic/test support depends on active image revision | SI468X-AN649: TEST_GET_RSSI (0xE5)
    Result testGetRssi(TestRssiInfo& out, uint32_t timeoutUs=1000000UL) {
        const uint8_t a[1]={0}; uint8_t r[6];
        Result x=executeCommand(Command::TEST_GET_RSSI,a,1,r,sizeof(r),timeoutUs);
        if (x!=Result::Ok) return x;
        parseStatus(r,sizeof(r),out.status); out.dbuv256=(int16_t)readLe16(r+4); return Result::Ok;
    }

    /* Raw overload for image-specific test arguments/replies. */
    // SI468X-API: testGetRssi | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: diagnostic/test support depends on active image revision | SI468X-AN649: TEST_GET_RSSI (0xE5)
    Result testGetRssi(const uint8_t* args, uint16_t argLength, uint8_t* reply, uint16_t replyLength,
                       uint32_t timeoutUs=1000000UL) { return executeCommand(Command::TEST_GET_RSSI,args,argLength,reply,replyLength,timeoutUs); }

    // -------------------------------------------------------------------------
    // NVSPI flash pass-through. Requires an appropriately patched bootloader.
    // -------------------------------------------------------------------------

    // SI468X-API: flashLoadImage | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashLoadImage(uint32_t address, uint32_t timeoutUs=1000000UL) {
        uint8_t a[7]={0,0,0,0,0,0,0}; writeLe32(a+3,address);
        return executeCommand(Command::FLASH_LOAD,a,7,0,0,timeoutUs);
    }

    // SI468X-API: flashLoadImageChecked | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashLoadImageChecked(uint32_t address, uint32_t size, uint32_t crc32,
                                 uint32_t timeoutUs=1000000UL) {
        uint8_t a[15]={1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        writeLe32(a+3,crc32); writeLe32(a+7,address); writeLe32(a+11,size);
        return executeCommand(Command::FLASH_LOAD,a,15,0,0,timeoutUs);
    }

    // SI468X-API: flashCheckCrc32 | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashCheckCrc32(uint32_t address, uint32_t size, uint32_t crc32,
                           uint32_t timeoutUs=1000000UL) {
        uint8_t a[15]={2,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        writeLe32(a+3,crc32); writeLe32(a+7,address); writeLe32(a+11,size);
        return executeCommand(Command::FLASH_LOAD,a,15,0,0,timeoutUs);
    }

    // SI468X-API: flashEraseChip | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashEraseChip(uint32_t timeoutUs=30000000UL) {
        const uint8_t a[3]={0xFF,0xDE,0xC0};
        return executeCommand(Command::FLASH_LOAD,a,3,0,0,timeoutUs);
    }
    // SI468X-API: flashEraseSector | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashEraseSector(uint32_t sectorAddress, uint32_t timeoutUs=5000000UL) {
        uint8_t a[7]={0xFE,0xC0,0xDE,0,0,0,0}; writeLe32(a+3,sectorAddress);
        return executeCommand(Command::FLASH_LOAD,a,7,0,0,timeoutUs);
    }

    // SI468X-API: flashGetProperty | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashGetProperty(uint16_t property, uint16_t& value, uint32_t timeoutUs=1000000UL) {
        uint8_t a[3]={0x11,0,0}; writeLe16(a+1,property); uint8_t r[6];
        Result x=executeCommand(Command::FLASH_LOAD,a,3,r,sizeof(r),timeoutUs);
        if (x==Result::Ok) value=readLe16(r+4);
        return x;
    }
    // SI468X-API: flashGetProperty | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashGetProperty(FlashProperty property, uint16_t& value, uint32_t timeoutUs=1000000UL) {
        return flashGetProperty((uint16_t)property,value,timeoutUs);
    }

    struct FlashPropertyValue { uint16_t property; uint16_t value; };

    // SI468X-API: flashSetPropertyList | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashSetPropertyList(const FlashPropertyValue* list, uint8_t count,
                                uint32_t timeoutUs=1000000UL) {
        if (!list || !count) return Result::InvalidArgument;
        const size_t needed=3u+(size_t)count*4u;
        if (!_workspace || _workspaceSize<needed) return Result::BufferTooSmall;
        _workspace[0]=0x10; _workspace[1]=0; _workspace[2]=0;
        for (uint8_t i=0;i<count;++i) {
            writeLe16(_workspace+3u+(size_t)i*4u,list[i].property);
            writeLe16(_workspace+5u+(size_t)i*4u,list[i].value);
        }
        return executeCommand(Command::FLASH_LOAD,_workspace,(uint16_t)needed,0,0,timeoutUs);
    }

    // SI468X-API: flashWriteBlock | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: bootloader patched for NVSPI pass-through; A10: ROM0.016 path | SI468X-AN649: FLASH_LOAD (0x05) subcommands
    Result flashWriteBlock(uint32_t address, const uint8_t* data, uint16_t length,
                           FlashSubcommand verification=FlashSubcommand::WRITE_BLOCK_READBACK_AND_PACKET_VERIFY,
                           uint32_t crc32=0, uint32_t timeoutUs=5000000UL) {
        if (!data || !length || length>4084u) return Result::InvalidArgument;
        if (verification!=FlashSubcommand::WRITE_BLOCK &&
            verification!=FlashSubcommand::WRITE_BLOCK_READBACK_VERIFY &&
            verification!=FlashSubcommand::WRITE_BLOCK_PACKET_VERIFY &&
            verification!=FlashSubcommand::WRITE_BLOCK_READBACK_AND_PACKET_VERIFY)
            return Result::InvalidArgument;
        const size_t needed=15u+(size_t)length;
        if (!_workspace || _workspaceSize<needed) return Result::BufferTooSmall;
        _workspace[0]=(uint8_t)verification; _workspace[1]=0x0C; _workspace[2]=0xED;
        writeLe32(_workspace+3, verification==FlashSubcommand::WRITE_BLOCK ? 0u : crc32);
        writeLe32(_workspace+7,address); writeLe32(_workspace+11,(uint32_t)length);
        memcpy(_workspace+15,data,length);
        return executeCommand(Command::FLASH_LOAD,_workspace,(uint16_t)needed,0,0,timeoutUs);
    }

    /*
     * Utility CRC-32/ISO-HDLC (reflected polynomial 0xEDB88320). The firmware
     * pack supplied with this project uses this conventional CRC32 for its
     * metadata. AN649 requires CRC32 values for several NVSPI verify commands
     * but does not define the polynomial in the Programming Guide itself; use
     * the CRC value supplied with a firmware release when one is provided.
     */
    // SI468X-API: crc32Ieee | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none; host-side utility only | SI468X-AN649: CRC32 helper for flash verification
    static uint32_t crc32Ieee(const uint8_t* data, size_t length, uint32_t seed=0xFFFFFFFFUL) {
        uint32_t c=seed;
        for (size_t i=0;i<length;++i) {
            c^=data[i];
            for (uint8_t b=0;b<8;++b) c=(c>>1)^((c&1u)?0xEDB88320UL:0u);
        }
        return c^0xFFFFFFFFUL;
    }

    // -------------------------------------------------------------------------
    // Static reply parsers: useful even when commands are issued by another
    // scheduler or when recorded replies are tested on a desktop machine.
    // -------------------------------------------------------------------------

    // SI468X-API: parseFmRsqStatus | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for FM/FMHD replies | SI468X-AN649: reply parser
    static Result parseFmRsqStatus(const uint8_t* r, size_t n, FmRsqStatus& o) {
        if (!r || n<17) return Result::MalformedReply;
        parseStatus(r,n,o.status);
        o.bandLimit=(r[5]&0x80u)!=0; o.hdDetected=(r[5]&0x20u)!=0;
        o.filteredHdDetected=(r[5]&0x10u)!=0; o.afcRail=(r[5]&0x02u)!=0; o.valid=(r[5]&0x01u)!=0;
        o.frequency10kHz=readLe16(r+6); o.frequencyOffset=(int8_t)r[8]; o.rssi=(int8_t)r[9];
        o.snr=(int8_t)r[10]; o.multipath=r[11]; o.antennaCap=readLe16(r+12);
        o.hdLevel=(n>15)?r[15]:0; o.filteredHdLevel=(n>16)?r[16]:0; return Result::Ok;
    }

    // SI468X-API: parseAmRsqStatus | SI468X-SUPPORT: Si4683 Si4685 Si4689 | SI468X-FIRMWARE: none; host-side parser for AM/AMHD replies | SI468X-AN649: reply parser
    static Result parseAmRsqStatus(const uint8_t* r, size_t n, AmRsqStatus& o) {
        if (!r || n<17) return Result::MalformedReply;
        parseStatus(r,n,o.status);
        o.bandLimit=(r[5]&0x80u)!=0; o.hdDetected=(r[5]&0x20u)!=0;
        o.filteredHdDetected=(r[5]&0x10u)!=0; o.afcRail=(r[5]&0x02u)!=0; o.valid=(r[5]&0x01u)!=0;
        o.frequencyKHz=readLe16(r+6); o.frequencyOffset=(int8_t)r[8]; o.rssi=(int8_t)r[9];
        o.snr=(int8_t)r[10]; o.modulationPercent=r[11]; o.antennaCap=readLe16(r+12);
        o.hdLevel=r[15]; o.filteredHdLevel=r[16]; return Result::Ok;
    }

    // SI468X-API: parseFmAcfStatus | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for FM/FMHD replies | SI468X-AN649: reply parser
    static Result parseFmAcfStatus(const uint8_t* r, size_t n, FmAcfStatus& o) {
        if (!r || n<9) return Result::MalformedReply;
        parseStatus(r,n,o.status);
        o.blendInterrupt=(r[4]&0x04u)!=0; o.highCutInterrupt=(r[4]&0x02u)!=0; o.softMuteInterrupt=(r[4]&0x01u)!=0;
        o.blendConverged=(r[5]&0x20u)!=0; o.highCutConverged=(r[5]&0x10u)!=0; o.softMuteConverged=(r[5]&0x08u)!=0;
        o.blendActive=(r[5]&0x04u)!=0; o.highCutActive=(r[5]&0x02u)!=0; o.softMuteActive=(r[5]&0x01u)!=0;
        o.attenuationDb=(uint8_t)(r[6]&0x1Fu); o.highCut100Hz=r[7]; o.pilot=(r[8]&0x80u)!=0;
        o.stereoBlendPercent=(uint8_t)(r[8]&0x7Fu); return Result::Ok;
    }

    // SI468X-API: parseAmAcfStatus | SI468X-SUPPORT: Si4683 Si4685 Si4689 | SI468X-FIRMWARE: none; host-side parser for AM/AMHD replies | SI468X-AN649: reply parser
    static Result parseAmAcfStatus(const uint8_t* r, size_t n, AmAcfStatus& o) {
        if (!r || n<9) return Result::MalformedReply;
        parseStatus(r,n,o.status);
        o.highCutInterrupt=(r[4]&0x02u)!=0; o.softMuteInterrupt=(r[4]&0x01u)!=0;
        o.highCutConverged=(r[5]&0x20u)!=0; o.softMuteConverged=(r[5]&0x10u)!=0;
        o.highCutActive=(r[5]&0x02u)!=0; o.softMuteActive=(r[5]&0x01u)!=0;
        o.attenuationDb=(uint8_t)(r[6]&0x1Fu); o.highCut100Hz=r[7]; o.lowCut100Hz=r[8]; return Result::Ok;
    }

    // SI468X-API: parseFmRdsStatus | SI468X-SUPPORT: Si4682 Si4683 Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for FM/FMHD replies | SI468X-AN649: reply parser
    static Result parseFmRdsStatus(const uint8_t* r, size_t n, FmRdsGroup& o) {
        if (!r || n<20) return Result::MalformedReply;
        parseStatus(r,n,o.status);
        o.tpPtyValid=(r[5]&0x10u)!=0; o.piValid=(r[5]&0x08u)!=0; o.sync=(r[5]&0x02u)!=0; o.fifoLost=(r[5]&0x01u)!=0;
        o.tp=(r[6]&0x20u)!=0; o.pty=(uint8_t)(r[6]&0x1Fu); o.pi=readLe16(r+8); o.fifoUsed=r[10];
        o.ble[0]=(uint8_t)((r[11]>>6)&3u); o.ble[1]=(uint8_t)((r[11]>>4)&3u);
        o.ble[2]=(uint8_t)((r[11]>>2)&3u); o.ble[3]=(uint8_t)(r[11]&3u);
        o.block[0]=readLe16(r+12); o.block[1]=readLe16(r+14); o.block[2]=readLe16(r+16); o.block[3]=readLe16(r+18);
        return Result::Ok;
    }

    // SI468X-API: parseDabDigradStatus | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for DAB replies | SI468X-AN649: reply parser
    static Result parseDabDigradStatus(const uint8_t* r, size_t n, DabDigradStatus& o) {
        if (!r || n<23) return Result::MalformedReply;
        parseStatus(r,n,o.status);
        o.hardMute=(r[5]&0x10u)!=0; o.ficError=(r[5]&0x08u)!=0; o.acquired=(r[5]&0x04u)!=0; o.valid=(r[5]&0x01u)!=0;
        o.rssi=(int8_t)r[6]; o.snr=r[7]; o.ficQuality=r[8]; o.cnr=r[9]; o.fibErrorCount=readLe16(r+10);
        o.tunedFrequencyKHz=readLe32(r+12); o.tuneIndex=r[16]; o.fftOffset=(int8_t)r[17];
        o.antennaCap=readLe16(r+18); o.cuLevel=readLe16(r+20); o.fastDetect=r[22]; return Result::Ok;
    }

    // SI468X-API: parseDabEventStatus | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for DAB replies | SI468X-AN649: reply parser
    static Result parseDabEventStatus(const uint8_t* r, size_t n, DabEventStatus& o) {
        if (!r || n<8) return Result::MalformedReply;
        parseStatus(r,n,o.status);
        o.reconfiguration=(r[4]&0x80u)!=0; o.reconfigurationWarning=(r[4]&0x40u)!=0;
        o.announcement=(r[4]&0x08u)!=0; o.frequencyInfoInterrupt=(r[4]&0x02u)!=0;
        o.serviceListInterrupt=(r[4]&0x01u)!=0; o.frequencyInfoAvailable=(r[5]&0x02u)!=0;
        o.serviceListAvailable=(r[5]&0x01u)!=0; o.serviceListVersion=readLe16(r+6);
        return Result::Ok;
    }

    // SI468X-API: parseDabEnsembleInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for DAB replies | SI468X-AN649: reply parser
    static Result parseDabEnsembleInfo(const uint8_t* r, size_t n, DabEnsembleInfo& o) {
        if (!r || n<26) return Result::MalformedReply;
        parseStatus(r,n,o.status); o.ensembleId=readLe16(r+4); memcpy(o.label,r+6,16); o.label[16]='\0';
        o.ecc=r[22]; o.abbreviationMask=readLe16(r+24); return Result::Ok;
    }

    // SI468X-API: parseDabTime | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for DAB replies | SI468X-AN649: reply parser
    static Result parseDabTime(const uint8_t* r, size_t n, DabTimeInfo& o) {
        if (!r || n<11) return Result::MalformedReply;
        parseStatus(r,n,o.status); o.year=readLe16(r+4); o.month=r[6]; o.day=r[7];
        o.hour=r[8]; o.minute=r[9]; o.second=r[10]; return Result::Ok;
    }

    // SI468X-API: parseDabAudioInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for DAB replies | SI468X-AN649: reply parser
    static Result parseDabAudioInfo(const uint8_t* r, size_t n, DabAudioInfo& o) {
        if (!r || n<10) return Result::MalformedReply;
        parseStatus(r,n,o.status); o.bitRateKbps=readLe16(r+4); o.sampleRateHz=readLe16(r+6);
        o.parametricStereo=(r[8]&0x08u)!=0; o.spectralBandReplication=(r[8]&0x04u)!=0;
        o.audioMode=(uint8_t)(r[8]&0x03u); o.drcGainQuarterDb=r[9]; return Result::Ok;
    }

    // SI468X-API: parseDabSubchannelInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for DAB replies | SI468X-AN649: reply parser
    static Result parseDabSubchannelInfo(const uint8_t* r, size_t n, DabSubchannelInfo& o) {
        if (!r || n<12) return Result::MalformedReply;
        parseStatus(r,n,o.status); o.serviceMode=r[4]; o.protectionInfo=r[5]; o.bitRateKbps=readLe16(r+6);
        o.capacityUnits=readLe16(r+8); o.capacityUnitAddress=readLe16(r+10); return Result::Ok;
    }

    // SI468X-API: parseDabServiceInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for DAB replies | SI468X-AN649: reply parser
    static Result parseDabServiceInfo(const uint8_t* r, size_t n, DabServiceInfo& o) {
        if (!r || n<26) return Result::MalformedReply;
        parseStatus(r,n,o.status); o.serviceLinkingAvailable=(r[4]&0x40u)!=0; o.pty=(uint8_t)((r[4]>>1)&0x1Fu);
        o.dataService=(r[4]&0x01u)!=0; o.local=(r[5]&0x80u)!=0; o.caId=(uint8_t)((r[5]>>4)&7u);
        o.numberOfComponents=(uint8_t)(r[5]&0x0Fu); o.charset=(uint8_t)(r[6]&0x0Fu); o.ecc=r[7];
        memcpy(o.label,r+8,16); o.label[16]='\0'; o.abbreviationMask=readLe16(r+24); return Result::Ok;
    }

    // SI468X-API: parseDabComponentInfo | SI468X-SUPPORT: Si4684 Si4685 Si4688 Si4689 | SI468X-FIRMWARE: none; host-side parser for DAB replies | SI468X-AN649: reply parser
    static Result parseDabComponentInfo(const uint8_t* r, size_t n, DabComponentInfo& o) {
        if (!r || n<28) return Result::MalformedReply;
        parseStatus(r,n,o.status); o.globalId=r[4]; o.language=(uint8_t)(r[6]&0x3Fu); o.charset=(uint8_t)(r[7]&0x3Fu);
        memcpy(o.label,r+8,16); o.label[16]='\0'; o.abbreviationMask=readLe16(r+24);
        o.numberOfUserApplications=r[26]; o.userApplicationBytes=r[27];
        if ((size_t)28u+o.userApplicationBytes>n) return Result::MalformedReply;
        o.userApplicationData=o.userApplicationBytes?(r+28):0; return Result::Ok;
    }

    // SI468X-API: parseDsrvHeader | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none; host-side DSRV/DLS parser; payload originates from DAB/HD service | SI468X-AN649: DSRV/DLS parser
    static Result parseDsrvHeader(const uint8_t* r, size_t n, DsrvHeader& o) {
        if (!r || n<24) return Result::MalformedReply;
        parseStatus(r,n,o.status);
        o.interruptSource=r[4]; o.buffersRemaining=r[5]; o.serviceState=r[6]; o.dataType=r[7];
        o.dataSource=(uint8_t)(r[7]>>6); o.dscType=(uint8_t)(r[7]&0x3Fu);
        o.serviceId=readLe32(r+8); o.componentId=readLe32(r+12); o.byteCount=readLe16(r+18);
        o.segmentNumber=readLe16(r+20); o.numberOfSegments=readLe16(r+22); return Result::Ok;
    }

    // SI468X-API: parseDlsPayload | SI468X-SUPPORT: ALL | SI468X-FIRMWARE: none; host-side DSRV/DLS parser; payload originates from DAB/HD service | SI468X-AN649: DSRV/DLS parser
    static Result parseDlsPayload(const uint8_t* payload, uint16_t length, DlsFrame& o) {
        if (!payload || length<2) return Result::MalformedReply;
        o.toggle=(payload[0]&0x80u)!=0; o.command=(payload[0]&0x10u)!=0;
        o.commandId=o.command?(uint8_t)(payload[0]&0x0Fu):0;
        o.link=o.command?((payload[1]&0x10u)!=0):false;
        o.charset=o.command?0:(uint8_t)((payload[1]>>4)&0x0Fu);
        o.body=payload+2; o.bodyLength=(uint16_t)(length-2); return Result::Ok;
    }

private:
    enum class State : uint8_t { Idle, WaitCts };
    HostInterface _host;
    uint8_t* _workspace;
    size_t _workspaceSize;
    StatusCallback _statusCallback;
    void* _statusContext;
    volatile uint8_t _irqPending;
    State _state;
    Result _lastResult;
    Status _lastStatus;
    uint8_t* _reply;
    uint16_t _replyLength;
    uint32_t _deadline, _nextCtsPoll, _nextIdlePoll;
    uint32_t _ctsPollIntervalUs, _idlePollIntervalUs;
    uint8_t _lastDeviceError;
    Part _detectedPart;
    Image _activeImage;

    uint32_t nowUs() const { return _host.timeUs ? _host.timeUs(_host.context) : 0u; }
    static bool timeReached(uint32_t now, uint32_t target) { return (int32_t)(now-target)>=0; }
    void finish(Result r) { _lastResult=r; _state=State::Idle; _reply=0; _replyLength=0; }

    Result delayUs(uint32_t us) {
        if (!_host.timeUs) return Result::NoTimer;
        const uint32_t start=nowUs();
        while ((uint32_t)(nowUs()-start)<us) { if (_host.idle) _host.idle(_host.context); }
        return Result::Ok;
    }
};

// -----------------------------------------------------------------------------
// 7. DAB service-list streaming parser.
// -----------------------------------------------------------------------------

struct DabServiceListHeader {
    uint16_t listSize;
    uint16_t version;
    uint8_t numberOfServices;
};

struct DabServiceEntry {
    uint32_t serviceId;
    bool programService;       // P/D flag == 0 means programme service.
    uint8_t pty;
    bool serviceLinking;
    bool local;
    uint8_t caId;
    uint8_t numberOfComponents;
    uint8_t labelCharset;
    char label[17];
};

struct DabComponentEntry {
    uint8_t serviceIndex;
    uint8_t componentIndex;
    /*
     * Exact four-byte component entry returned by GET_DIGITAL_SERVICE_LIST.
     * Pass this value unchanged as COMP_ID to START_DIGITAL_SERVICE and the
     * DAB component/sub-channel information commands. This intentionally
     * preserves the information bytes above the 16-bit TMId/reference field.
     */
    uint32_t componentId;
    uint16_t rawComponentField;
    uint16_t componentReference; // SubChId/FIDCId/SCId decoded from rawComponentField.
    uint8_t transportModeId;
    bool dataGroupFlag;
    uint8_t componentType;       // ASCTy or DSCTy depending on TMId.
    bool primary;
    bool conditionalAccess;
    bool userApplicationInfoValid;
};

struct DabServiceListSink {
    void* context;
    void (*onHeader)(void*, const DabServiceListHeader&);
    void (*onService)(void*, const DabServiceEntry&);
    void (*onComponent)(void*, const DabComponentEntry&);
    DabServiceListSink() : context(0), onHeader(0), onService(0), onComponent(0) {}
};

/*
 * Streaming parser for the DAB form of GET_DIGITAL_SERVICE_LIST.
 * Feed bytes beginning at RESP4 (the first list-size byte), i.e. do not feed the
 * four status bytes.  Chunks may be arbitrarily small but must be supplied in
 * order. Only 24 bytes of parser storage are required regardless of list size.
 * Component callbacks expose both the exact 32-bit list entry (componentId)
 * and decoded TMId/reference fields; the exact value is the safe value to feed
 * back to Si468x component/service commands.
 */
class DabServiceListParser {
public:
    DabServiceListParser() { reset(); }
    void setSink(const DabServiceListSink& s) { _sink=s; }
    void reset() {
        _phase=Phase::Header; _have=0; _need=8; _serviceIndex=0; _componentIndex=0;
        _componentsRemaining=0; _servicesExpected=0; _error=false;
    }
    bool error() const { return _error; }
    bool complete() const { return _phase==Phase::Done && !_error; }

    Result feed(const uint8_t* data, size_t length) {
        if (!data && length) return Result::InvalidArgument;
        size_t pos=0;
        while (pos<length && !_error && _phase!=Phase::Done) {
            size_t take=_need-_have; if (take>length-pos) take=length-pos;
            memcpy(_buf+_have,data+pos,take); _have+=(uint8_t)take; pos+=take;
            if (_have==_need) processRecord();
        }
        return _error?Result::MalformedReply:Result::Ok;
    }

private:
    enum class Phase : uint8_t { Header, Service, Component, Done };
    DabServiceListSink _sink;
    Phase _phase;
    uint8_t _buf[24];
    uint8_t _have,_need,_serviceIndex,_componentIndex,_componentsRemaining,_servicesExpected;
    bool _error;

    void nextServiceOrDone() {
        ++_serviceIndex; _componentIndex=0;
        if (_serviceIndex>=_servicesExpected) { _phase=Phase::Done; _have=0; _need=0; }
        else { _phase=Phase::Service; _have=0; _need=24; }
    }

    void processRecord() {
        if (_phase==Phase::Header) {
            DabServiceListHeader h; h.listSize=readLe16(_buf); h.version=readLe16(_buf+2); h.numberOfServices=_buf[4];
            if (h.listSize>2694u || h.numberOfServices>=32u) { _error=true; return; }
            _servicesExpected=h.numberOfServices;
            if (_sink.onHeader) _sink.onHeader(_sink.context,h);
            _have=0;
            if (!_servicesExpected) { _phase=Phase::Done; _need=0; } else { _phase=Phase::Service; _need=24; }
            return;
        }
        if (_phase==Phase::Service) {
            DabServiceEntry s; memset(&s,0,sizeof(s));
            s.serviceId=readLe32(_buf); const uint8_t info1=_buf[4], info2=_buf[5], info3=_buf[6];
            s.programService=(info1&1u)==0; s.pty=(uint8_t)((info1>>1)&0x1Fu); s.serviceLinking=(info1&0x40u)!=0;
            s.local=(info2&0x80u)!=0; s.caId=(uint8_t)((info2>>4)&7u); s.numberOfComponents=(uint8_t)(info2&0x0Fu);
            s.labelCharset=(uint8_t)(info3&0x0Fu); memcpy(s.label,_buf+8,16); s.label[16]='\0';
            if (_sink.onService) _sink.onService(_sink.context,s);
            _componentsRemaining=s.numberOfComponents; _componentIndex=0; _have=0;
            if (_componentsRemaining) { _phase=Phase::Component; _need=4; } else nextServiceOrDone();
            return;
        }
        if (_phase==Phase::Component) {
            DabComponentEntry c; memset(&c,0,sizeof(c)); const uint16_t raw=readLe16(_buf);
            c.serviceIndex=_serviceIndex; c.componentIndex=_componentIndex; c.componentId=readLe32(_buf);
            c.rawComponentField=raw; c.transportModeId=(uint8_t)(raw>>14);
            c.dataGroupFlag=(c.transportModeId==3u)&&((raw&0x2000u)!=0);
            c.componentReference=(c.transportModeId==3u)?(uint16_t)(raw&0x0FFFu):(uint16_t)(raw&0x003Fu);
            c.componentType=(uint8_t)(_buf[2]>>2); c.primary=(_buf[2]&0x02u)==0;
            c.conditionalAccess=(_buf[2]&0x01u)!=0; c.userApplicationInfoValid=(_buf[3]&0x01u)!=0;
            if (_sink.onComponent) _sink.onComponent(_sink.context,c);
            ++_componentIndex; if (_componentsRemaining) --_componentsRemaining; _have=0;
            if (_componentsRemaining) _need=4; else nextServiceOrDone();
        }
    }
};

inline Result Si468x::readDabServiceList(DabServiceListParser& parser, uint16_t preferredChunk,
                                         uint32_t timeoutUs) {
    if (!_workspace || _workspaceSize < 12u) return Result::BufferTooSmall;

    /* First read just STATUS + SIZE so the full preserved response length is known. */
    uint8_t first[6];
    Result r=getDigitalServiceList(0,first,sizeof(first),timeoutUs);
    if (r!=Result::Ok) return r;
    const uint16_t listSize=readLe16(first+4);
    if (listSize>2694u) return Result::MalformedReply;
    uint32_t remaining=(uint32_t)listSize+2u; /* LIST_SIZE excludes its own two bytes. */

    size_t capacity=_workspaceSize-4u;
    uint16_t chunk=(preferredChunk && preferredChunk<capacity)?preferredChunk:(uint16_t)capacity;
    if (chunk>4092u) chunk=4092u;
    chunk=(uint16_t)(chunk & 0xFFFCu);
    if (chunk<4u) return Result::BufferTooSmall;

    parser.reset();
    uint16_t offset=0;
    while (remaining) {
        uint16_t n=(remaining<chunk)?(uint16_t)remaining:chunk;
        r=readOffset(offset,_workspace,n,timeoutUs);
        if (r!=Result::Ok) return r;
        r=parser.feed(_workspace+4,n);
        if (r!=Result::Ok) return r;
        remaining-=n;
        offset=(uint16_t)(offset+n);
    }
    return parser.complete()?Result::Ok:Result::MalformedReply;
}

// -----------------------------------------------------------------------------
// 8. Lightweight RDS decoder. No display encoding or dynamic memory is assumed.
// -----------------------------------------------------------------------------

struct RdsClockTime {
    bool valid;
    uint16_t year;
    uint8_t month, day, hour, minute;
    int8_t localOffsetHalfHours;
    RdsClockTime() : valid(false), year(0), month(0), day(0), hour(0), minute(0), localOffsetHalfHours(0) {}
};

class RdsDecoder {
public:
    RdsDecoder() { reset(); }
    void reset() {
        memset(_ps,' ',8); _ps[8]='\0'; _psMask=0;
        memset(_rt,' ',64); _rt[64]='\0'; _rtMask=0; _rtAb=0xFF;
        _pi=0; _pty=0; _tp=false; _ta=false; _ecc=0; _clock=RdsClockTime();
    }
    void feed(const FmRdsGroup& g) {
        if (g.piValid) _pi=g.pi;
        if (g.tpPtyValid) { _tp=g.tp; _pty=g.pty; }
        if (!g.blockUsable(1)) return;
        const uint16_t b=g.block[1]; const uint8_t type=(uint8_t)((b>>12)&0x0Fu); const bool versionB=(b&0x0800u)!=0;
        _tp=(b&0x0400u)!=0; _pty=(uint8_t)((b>>5)&0x1Fu);
        if (type==0) {
            _ta=(b&0x0010u)!=0; const uint8_t seg=(uint8_t)(b&3u);
            if (g.blockUsable(3)) { _ps[seg*2]=(char)(g.block[3]>>8); _ps[seg*2+1]=(char)(g.block[3]&0xFFu); _psMask|=(uint8_t)(1u<<seg); }
        } else if (type==2) {
            const uint8_t ab=(uint8_t)((b>>4)&1u); if (_rtAb!=ab) { memset(_rt,' ',64); _rt[64]='\0'; _rtMask=0; _rtAb=ab; }
            const uint8_t seg=(uint8_t)(b&0x0Fu);
            if (!versionB) {
                if (g.blockUsable(2)&&g.blockUsable(3)) { const uint8_t p=(uint8_t)(seg*4u); _rt[p]=(char)(g.block[2]>>8); _rt[p+1]=(char)g.block[2]; _rt[p+2]=(char)(g.block[3]>>8); _rt[p+3]=(char)g.block[3]; _rtMask|=(uint16_t)(1u<<seg); }
            } else if (g.blockUsable(3)) {
                const uint8_t p=(uint8_t)(seg*2u); _rt[p]=(char)(g.block[3]>>8); _rt[p+1]=(char)g.block[3]; _rtMask|=(uint16_t)(1u<<seg);
            }
        } else if (type==4 && !versionB && g.blockUsable(2) && g.blockUsable(3)) {
            decodeClock(b,g.block[2],g.block[3]);
        } else if (type==1 && !versionB && g.blockUsable(2)) {
            const uint8_t variant=(uint8_t)((g.block[2]>>12)&7u); if (variant==0) _ecc=(uint8_t)(g.block[2]&0xFFu);
        }
    }
    const char* programService() const { return _ps; }
    bool programServiceComplete() const { return _psMask==0x0Fu; }
    const char* radioText() const { return _rt; }
    uint16_t radioTextSegmentMask() const { return _rtMask; }
    uint16_t pi() const { return _pi; }
    uint8_t pty() const { return _pty; }
    bool tp() const { return _tp; }
    bool ta() const { return _ta; }
    uint8_t ecc() const { return _ecc; }
    const RdsClockTime& clockTime() const { return _clock; }
private:
    char _ps[9],_rt[65]; uint8_t _psMask,_rtAb,_pty,_ecc; uint16_t _rtMask,_pi; bool _tp,_ta; RdsClockTime _clock;
    static bool isLeap(uint16_t y) { return (y%4u==0u)&&((y%100u)!=0u||(y%400u)==0u); }
    static uint8_t monthDays(uint16_t y,uint8_t m) { static const uint8_t d[12]={31,28,31,30,31,30,31,31,30,31,30,31}; return (m==2&&isLeap(y))?29:d[m-1]; }
    static void mjdToDate(uint32_t mjd,uint16_t& y,uint8_t& m,uint8_t& d) {
        // Integer conversion based on the standard Julian-day relation.
        uint32_t jd=mjd+2400001UL; uint32_t l=jd+68569UL; uint32_t n=4UL*l/146097UL;
        l=l-(146097UL*n+3UL)/4UL; uint32_t i=4000UL*(l+1UL)/1461001UL;
        l=l-1461UL*i/4UL+31UL; uint32_t j=80UL*l/2447UL; d=(uint8_t)(l-2447UL*j/80UL);
        l=j/11UL; m=(uint8_t)(j+2UL-12UL*l); y=(uint16_t)(100UL*(n-49UL)+i+l);
    }
    void decodeClock(uint16_t b,uint16_t c,uint16_t dword) {
        const uint32_t mjd=((uint32_t)(b&3u)<<15)|(uint32_t)(c>>1); uint16_t y; uint8_t m,d;
        mjdToDate(mjd,y,m,d); uint8_t hour=(uint8_t)(((c&1u)<<4)|((dword>>12)&0x0Fu)); uint8_t minute=(uint8_t)((dword>>6)&0x3Fu);
        int8_t off=(int8_t)(dword&0x1Fu); if (dword&0x20u) off=(int8_t)-off;
        _clock.valid=true; _clock.year=y; _clock.month=m; _clock.day=d; _clock.hour=hour; _clock.minute=minute; _clock.localOffsetHalfHours=off;
    }
};

// -----------------------------------------------------------------------------
// 9. DSRV classification helpers. Unknown/proprietary payloads remain raw.
// -----------------------------------------------------------------------------

inline bool dsrvIsDlsOrDlPlus(const DsrvHeader& h) { return h.dataSource==2; }
inline bool dsrvIsMotPad(const DsrvHeader& h) { return h.dataSource==1 && h.dscType==60; }
inline bool dsrvIsTmc(const DsrvHeader& h) { return (h.dataSource==0 || h.dataSource==1) && h.dscType==1; }
inline bool dsrvIsTdcOrTpeg(const DsrvHeader& h) { return (h.dataSource==0 || h.dataSource==1) && h.dscType==5; }

/*
 * MOT note: AN649 forwards MOT at the MSC data-group level and explicitly leaves
 * MOT object/segment assembly to a host-side MOT decoder.  This library therefore
 * preserves the complete raw DSRV payload, service/component IDs and DSRV segment
 * metadata but does not fabricate an ETSI MOT decoder from undocumented details.
 * The same rule applies to proprietary HD AAS/PSD formats delegated by AN649 to
 * external iBiquity specifications.
 */

} // namespace si468x

#endif // SI468X_UNIVERSAL_H
