#include <stdlib.h>
#include <string.h>

#include "f1c100s.h"
#include "f1c100s_log.h"

#include "qemu_defs.h"
#include "sd.h"
#include "sdmmc-internal.h"
#include "sd_blk.h"
#include "sd_trace.h"

//=======================================================================
static void sd_reset(SDState *sd);
static void sd_realize(SDState *sd);
//=======================================================================
static inline unsigned long *bitmap_new(long nbits)
{
    long bytes = ((nbits + 31) >> 5) << 2;
    unsigned long *ptr = malloc(bytes);
    if (ptr == NULL) {
        abort();
    }
    memset(ptr, 0, bytes);
    return ptr;
}
static inline void bitmap_zero(unsigned long *bmp, long nbits)
{
    long bytes = ((nbits + 31) >> 5) << 2;
    memset(bmp, 0, bytes);
}
//----------------------------------------------------------------------
static inline bool is_power_of_2(uint64_t value)
{
    if (!value) {
        return false;
    }

    return !(value & (value - 1));
}
/*
 * Return @value rounded up to the nearest power of two modulo 2^64.
 * This is *zero* for @value > 2^63, so be careful.
 */
static inline uint64_t pow2ceil(uint64_t value)
{
    int n = clz64(value - 1);

    if (!n) {
        /*
         * @value - 1 has no leading zeroes, thus @value - 1 >= 2^63
         * Therefore, either @value == 0 or @value > 2^63.
         * If it's 0, return 1, else return 0.
         */
        return !value;
    }
    return 0x8000000000000000ull >> (n - 1);
}
//=======================================================================

#define SDSC_MAX_CAPACITY   (2 * GiB)

#define INVALID_ADDRESS     UINT32_MAX

typedef enum {
    sd_r0 = 0,    /* no response */
    sd_r1,        /* normal response command */
    sd_r2_i,      /* CID register */
    sd_r2_s,      /* CSD register */
    sd_r3,        /* OCR register */
    sd_r6 = 6,    /* Published RCA response */
    sd_r7,        /* Operating voltage */
    sd_r1b = -1,
    sd_illegal = -2,
} sd_rsp_type_t;

enum SDCardModes {
    sd_inactive,
    sd_card_identification_mode,
    sd_data_transfer_mode,
};

enum SDCardStates {
    sd_inactive_state = -1,
    sd_idle_state = 0,
    sd_ready_state,
    sd_identification_state,
    sd_standby_state,
    sd_transfer_state,
    sd_sendingdata_state,
    sd_receivingdata_state,
    sd_programming_state,
    sd_disconnect_state,
};

struct SDState {
    //DeviceState parent_obj;

    /* If true, created by sd_init() for a non-qdevified caller */
    /* TODO purge them with fire */
    bool     me_no_qdev_me_kill_mammoth_with_rocks;

    /* SD Memory Card Registers */
    uint32_t ocr;
    uint8_t  scr[8];
    uint8_t  cid[16];
    uint8_t  csd[16];
    uint16_t rca;
    uint32_t card_status;
    uint8_t  sd_status[64];

    /* Static properties */

    uint8_t  spec_version;
    // BlockBackend *blk;
    FILE    *blk;
    bool     spi;

    /* Runtime changeables */

    uint32_t mode;    /* current card mode, one of SDCardModes */
    int32_t  state;    /* current card state, one of SDCardStates */
    uint32_t vhs;
    bool     wp_switch;
    unsigned long *wp_group_bmap;
    int32_t  wp_group_bits;
    uint64_t size;
    uint32_t blk_len;
    uint32_t multi_blk_cnt;
    uint32_t erase_start;
    uint32_t erase_end;
    uint8_t  pwd[16];
    uint32_t pwd_len;
    uint8_t  function_group[6];
    uint8_t  current_cmd;
    /* True if we will handle the next command as an ACMD. Note that this does
     * *not* track the APP_CMD status bit!
     */
    bool     expecting_acmd;
    uint32_t blk_written;
    uint64_t data_start;
    uint32_t data_offset;
    uint8_t  data[512];
    qemu_irq readonly_cb;
    qemu_irq inserted_cb;
    //QEMUTimer *ocr_power_timer;
    const char *proto_name;
    bool     enable;
    uint8_t  dat_lines;
    bool     cmd_line;
};

static const char *sd_state_name(enum SDCardStates state)
{
    static const char *state_name[] = {
        [sd_idle_state]             = "idle",
        [sd_ready_state]            = "ready",
        [sd_identification_state]   = "identification",
        [sd_standby_state]          = "standby",
        [sd_transfer_state]         = "transfer",
        [sd_sendingdata_state]      = "sendingdata",
        [sd_receivingdata_state]    = "receivingdata",
        [sd_programming_state]      = "programming",
        [sd_disconnect_state]       = "disconnect",
    };
    if (state == sd_inactive_state) {
        return "inactive";
    }
    assert(state < ARRAY_SIZE(state_name));
    return state_name[state];
}

static const char *sd_response_name(sd_rsp_type_t rsp)
{
    static const char *response_name[] = {
        [sd_r0]     = "RESP#0 (no response)",
        [sd_r1]     = "RESP#1 (normal cmd)",
        [sd_r2_i]   = "RESP#2 (CID reg)",
        [sd_r2_s]   = "RESP#2 (CSD reg)",
        [sd_r3]     = "RESP#3 (OCR reg)",
        [sd_r6]     = "RESP#6 (RCA)",
        [sd_r7]     = "RESP#7 (operating voltage)",
    };
    if (rsp == sd_illegal) {
        return "ILLEGAL RESP";
    }
    if (rsp == sd_r1b) {
        rsp = sd_r1;
    }
    assert(rsp < ARRAY_SIZE(response_name));
    return response_name[rsp];
}

static uint8_t sd_get_dat_lines(SDState *sd)
{
    return sd->enable ? sd->dat_lines : 0;
}

static bool sd_get_cmd_line(SDState *sd)
{
    return sd->enable ? sd->cmd_line : false;
}

static void sd_set_voltage(SDState *sd, uint16_t millivolts)
{
    switch (millivolts) {
    case 3001 ... 3600: /* SD_VOLTAGE_3_3V */
    case 2001 ... 3000: /* SD_VOLTAGE_3_0V */
        break;
    default:
        f1c100s_log_mask(LOG_GUEST_ERROR, "SD card voltage not supported: %.3fV",
                      millivolts / 1000.f);
    }
}

static void sd_set_mode(SDState *sd)
{
    switch (sd->state) {
    case sd_inactive_state:
        sd->mode = sd_inactive;
        break;

    case sd_idle_state:
    case sd_ready_state:
    case sd_identification_state:
        sd->mode = sd_card_identification_mode;
        break;

    case sd_standby_state:
    case sd_transfer_state:
    case sd_sendingdata_state:
    case sd_receivingdata_state:
    case sd_programming_state:
    case sd_disconnect_state:
        sd->mode = sd_data_transfer_mode;
        break;
    }
}

static const sd_cmd_type_t sd_cmd_type[SDMMC_CMD_MAX] = {
    sd_bc,   sd_none, sd_bcr,  sd_bcr,  sd_none, sd_none, sd_none, sd_ac,
    sd_bcr,  sd_ac,   sd_ac,   sd_adtc, sd_ac,   sd_ac,   sd_none, sd_ac,
    /* 16 */
    sd_ac,   sd_adtc, sd_adtc, sd_none, sd_none, sd_none, sd_none, sd_none,
    sd_adtc, sd_adtc, sd_adtc, sd_adtc, sd_ac,   sd_ac,   sd_adtc, sd_none,
    /* 32 */
    sd_ac,   sd_ac,   sd_none, sd_none, sd_none, sd_none, sd_ac,   sd_none,
    sd_none, sd_none, sd_bc,   sd_none, sd_none, sd_none, sd_none, sd_none,
    /* 48 */
    sd_none, sd_none, sd_none, sd_none, sd_none, sd_none, sd_none, sd_ac,
    sd_adtc, sd_none, sd_none, sd_none, sd_none, sd_none, sd_none, sd_none,
};

static const int sd_cmd_class[SDMMC_CMD_MAX] = {
    0,  0,  0,  0,  0,  9, 10,  0,  0,  0,  0,  1,  0,  0,  0,  0,
    2,  2,  2,  2,  3,  3,  3,  3,  4,  4,  4,  4,  6,  6,  6,  6,
    5,  5, 10, 10, 10, 10,  5,  9,  9,  9,  7,  7,  7,  7,  7,  7,
    7,  7, 10,  7,  9,  9,  9,  8,  8, 10,  8,  8,  8,  8,  8,  8,
};

static uint8_t sd_crc7(const void *message, size_t width)
{
    int i, bit;
    uint8_t shift_reg = 0x00;
    const uint8_t *msg = (const uint8_t *)message;

    for (i = 0; i < width; i ++, msg ++)
        for (bit = 7; bit >= 0; bit --) {
            shift_reg <<= 1;
            if ((shift_reg >> 7) ^ ((*msg >> bit) & 1))
                shift_reg ^= 0x89;
        }

    return shift_reg;
}

#define OCR_POWER_DELAY_NS      500000 /* 0.5ms */

FIELD(OCR, VDD_VOLTAGE_WINDOW,          0, 24)
FIELD(OCR, VDD_VOLTAGE_WIN_LO,          0,  8)
FIELD(OCR, DUAL_VOLTAGE_CARD,           7,  1)
FIELD(OCR, VDD_VOLTAGE_WIN_HI,          8, 16)
FIELD(OCR, ACCEPT_SWITCH_1V8,          24,  1) /* Only UHS-I */
FIELD(OCR, UHS_II_CARD,                29,  1) /* Only UHS-II */
FIELD(OCR, CARD_CAPACITY,              30,  1) /* 0:SDSC, 1:SDHC/SDXC */
FIELD(OCR, CARD_POWER_UP,              31,  1)

#define ACMD41_ENQUIRY_MASK     0x00ffffff
#define ACMD41_R3_MASK          (R_OCR_VDD_VOLTAGE_WIN_HI_MASK \
                               | R_OCR_ACCEPT_SWITCH_1V8_MASK \
                               | R_OCR_UHS_II_CARD_MASK \
                               | R_OCR_CARD_CAPACITY_MASK \
                               | R_OCR_CARD_POWER_UP_MASK)

static void sd_ocr_powerup(SDState *sd)
{
    trace_sdcard_powerup();
    assert(!FIELD_EX32(sd->ocr, OCR, CARD_POWER_UP));

    /* card power-up OK */
    sd->ocr = FIELD_DP32(sd->ocr, OCR, CARD_POWER_UP, 1);

    if (sd->size > SDSC_MAX_CAPACITY) {
        sd->ocr = FIELD_DP32(sd->ocr, OCR, CARD_CAPACITY, 1);
    }
}

static void sd_set_ocr(SDState *sd)
{
    /* All voltages OK */
    sd->ocr = R_OCR_VDD_VOLTAGE_WIN_HI_MASK;

    if (sd->spi) {
        /*
         * We don't need to emulate power up sequence in SPI-mode.
         * Thus, the card's power up status bit should be set to 1 when reset.
         * The card's capacity status bit should also be set if SD card size
         * is larger than 2GB for SDHC support.
         */
        sd_ocr_powerup(sd);
    }
}

static void sd_set_scr(SDState *sd)
{
    sd->scr[0] = 0 << 4;        /* SCR structure version 1.0 */
    if (sd->spec_version == SD_PHY_SPECv1_10_VERS) {
        sd->scr[0] |= 1;        /* Spec Version 1.10 */
    } else {
        sd->scr[0] |= 2;        /* Spec Version 2.00 or Version 3.0X */
    }
    sd->scr[1] = (2 << 4)       /* SDSC Card (Security Version 1.01) */
                 | 0b0101;      /* 1-bit or 4-bit width bus modes */
    sd->scr[2] = 0x00;          /* Extended Security is not supported. */
    if (sd->spec_version >= SD_PHY_SPECv3_01_VERS) {
        sd->scr[2] |= 1 << 7;   /* Spec Version 3.0X */
    }
    sd->scr[3] = 0x00;
    /* reserved for manufacturer usage */
    sd->scr[4] = 0x00;
    sd->scr[5] = 0x00;
    sd->scr[6] = 0x00;
    sd->scr[7] = 0x00;
}

#define MID	0xaa
#define OID	"XY"
#define PNM	"QEMU!"
#define PRV	0x01
#define MDT_YR	2006
#define MDT_MON	2

static void sd_set_cid(SDState *sd)
{
    sd->cid[0] = MID;		/* Fake card manufacturer ID (MID) */
    sd->cid[1] = OID[0];	/* OEM/Application ID (OID) */
    sd->cid[2] = OID[1];
    sd->cid[3] = PNM[0];	/* Fake product name (PNM) */
    sd->cid[4] = PNM[1];
    sd->cid[5] = PNM[2];
    sd->cid[6] = PNM[3];
    sd->cid[7] = PNM[4];
    sd->cid[8] = PRV;		/* Fake product revision (PRV) */
    sd->cid[9] = 0xde;		/* Fake serial number (PSN) */
    sd->cid[10] = 0xad;
    sd->cid[11] = 0xbe;
    sd->cid[12] = 0xef;
    sd->cid[13] = 0x00 |	/* Manufacture date (MDT) */
        ((MDT_YR - 2000) / 10);
    sd->cid[14] = ((MDT_YR % 10) << 4) | MDT_MON;
    sd->cid[15] = (sd_crc7(sd->cid, 15) << 1) | 1;
}

#define HWBLOCK_SHIFT	9			/* 512 bytes */
#define SECTOR_SHIFT	5			/* 16 kilobytes */
#define WPGROUP_SHIFT	7			/* 2 megs */
#define CMULT_SHIFT	9			/* 512 times HWBLOCK_SIZE */
#define WPGROUP_SIZE	(1 << (HWBLOCK_SHIFT + SECTOR_SHIFT + WPGROUP_SHIFT))

static const uint8_t sd_csd_rw_mask[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfe,
};

static void sd_set_csd(SDState *sd, uint64_t size)
{
    int hwblock_shift = HWBLOCK_SHIFT;
    uint32_t csize;
    uint32_t sectsize = (1 << (SECTOR_SHIFT + 1)) - 1;
    uint32_t wpsize = (1 << (WPGROUP_SHIFT + 1)) - 1;

    /* To indicate 2 GiB card, BLOCK_LEN shall be 1024 bytes */
    if (size == SDSC_MAX_CAPACITY) {
        hwblock_shift += 1;
    }
    csize = (size >> (CMULT_SHIFT + hwblock_shift)) - 1;

    if (size <= SDSC_MAX_CAPACITY) { /* Standard Capacity SD */
        sd->csd[0] = 0x00;	/* CSD structure */
        sd->csd[1] = 0x26;	/* Data read access-time-1 */
        sd->csd[2] = 0x00;	/* Data read access-time-2 */
        sd->csd[3] = 0x32;      /* Max. data transfer rate: 25 MHz */
        sd->csd[4] = 0x5f;	/* Card Command Classes */
        sd->csd[5] = 0x50 |	/* Max. read data block length */
            hwblock_shift;
        sd->csd[6] = 0xe0 |	/* Partial block for read allowed */
            ((csize >> 10) & 0x03);
        sd->csd[7] = 0x00 |	/* Device size */
            ((csize >> 2) & 0xff);
        sd->csd[8] = 0x3f |	/* Max. read current */
            ((csize << 6) & 0xc0);
        sd->csd[9] = 0xfc |	/* Max. write current */
            ((CMULT_SHIFT - 2) >> 1);
        sd->csd[10] = 0x40 |	/* Erase sector size */
            (((CMULT_SHIFT - 2) << 7) & 0x80) | (sectsize >> 1);
        sd->csd[11] = 0x00 |	/* Write protect group size */
            ((sectsize << 7) & 0x80) | wpsize;
        sd->csd[12] = 0x90 |	/* Write speed factor */
            (hwblock_shift >> 2);
        sd->csd[13] = 0x20 |	/* Max. write data block length */
            ((hwblock_shift << 6) & 0xc0);
        sd->csd[14] = 0x00;	/* File format group */
    } else {			/* SDHC */
        size /= 512 * KiB;
        size -= 1;
        sd->csd[0] = 0x40;
        sd->csd[1] = 0x0e;
        sd->csd[2] = 0x00;
        sd->csd[3] = 0x32;
        sd->csd[4] = 0x5b;
        sd->csd[5] = 0x59;
        sd->csd[6] = 0x00;
        sd->csd[7] = (size >> 16) & 0xff;
        sd->csd[8] = (size >> 8) & 0xff;
        sd->csd[9] = (size & 0xff);
        sd->csd[10] = 0x7f;
        sd->csd[11] = 0x80;
        sd->csd[12] = 0x0a;
        sd->csd[13] = 0x40;
        sd->csd[14] = 0x00;
    }
    sd->csd[15] = (sd_crc7(sd->csd, 15) << 1) | 1;
}

static void sd_set_rca(SDState *sd)
{
    sd->rca += 0x4567;
}


FIELD(CSR, AKE_SEQ_ERROR,               3,  1)
FIELD(CSR, APP_CMD,                     5,  1)
FIELD(CSR, FX_EVENT,                    6,  1)
FIELD(CSR, READY_FOR_DATA,              8,  1)
FIELD(CSR, CURRENT_STATE,               9,  4)
FIELD(CSR, ERASE_RESET,                13,  1)
FIELD(CSR, CARD_ECC_DISABLED,          14,  1)
FIELD(CSR, WP_ERASE_SKIP,              15,  1)
FIELD(CSR, CSD_OVERWRITE,              16,  1)
FIELD(CSR, DEFERRED_RESPONSE,          17,  1)
FIELD(CSR, ERROR,                      19,  1)
FIELD(CSR, CC_ERROR,                   20,  1)
FIELD(CSR, CARD_ECC_FAILED,            21,  1)
FIELD(CSR, ILLEGAL_COMMAND,            22,  1)
FIELD(CSR, COM_CRC_ERROR,              23,  1)
FIELD(CSR, LOCK_UNLOCK_FAILED,         24,  1)
FIELD(CSR, CARD_IS_LOCKED,             25,  1)
FIELD(CSR, WP_VIOLATION,               26,  1)
FIELD(CSR, ERASE_PARAM,                27,  1)
FIELD(CSR, ERASE_SEQ_ERROR,            28,  1)
FIELD(CSR, BLOCK_LEN_ERROR,            29,  1)
FIELD(CSR, ADDRESS_ERROR,              30,  1)
FIELD(CSR, OUT_OF_RANGE,               31,  1)

/* Card status bits, split by clear condition:
 * A : According to the card current state
 * B : Always related to the previous command
 * C : Cleared by read
 */
#define CARD_STATUS_A           (R_CSR_READY_FOR_DATA_MASK \
                               | R_CSR_CARD_ECC_DISABLED_MASK \
                               | R_CSR_CARD_IS_LOCKED_MASK)
#define CARD_STATUS_B           (R_CSR_CURRENT_STATE_MASK \
                               | R_CSR_ILLEGAL_COMMAND_MASK \
                               | R_CSR_COM_CRC_ERROR_MASK)
#define CARD_STATUS_C           (R_CSR_AKE_SEQ_ERROR_MASK \
                               | R_CSR_APP_CMD_MASK \
                               | R_CSR_ERASE_RESET_MASK \
                               | R_CSR_WP_ERASE_SKIP_MASK \
                               | R_CSR_CSD_OVERWRITE_MASK \
                               | R_CSR_ERROR_MASK \
                               | R_CSR_CC_ERROR_MASK \
                               | R_CSR_CARD_ECC_FAILED_MASK \
                               | R_CSR_LOCK_UNLOCK_FAILED_MASK \
                               | R_CSR_WP_VIOLATION_MASK \
                               | R_CSR_ERASE_PARAM_MASK \
                               | R_CSR_ERASE_SEQ_ERROR_MASK \
                               | R_CSR_BLOCK_LEN_ERROR_MASK \
                               | R_CSR_ADDRESS_ERROR_MASK \
                               | R_CSR_OUT_OF_RANGE_MASK)

static void sd_set_cardstatus(SDState *sd)
{
    sd->card_status = 0x00000100;
}

static void sd_set_sdstatus(SDState *sd)
{
    memset(sd->sd_status, 0, 64);
}

static int sd_req_crc_validate(SDRequest *req)
{
    uint8_t buffer[5];
    buffer[0] = 0x40 | req->cmd;
    stl_be_p(&buffer[1], req->arg);
    return 0;
    return sd_crc7(buffer, 5) != req->crc;	/* TODO */
}

static void sd_response_r1_make(SDState *sd, uint8_t *response)
{
    stl_be_p(response, sd->card_status);

    /* Clear the "clear on read" status bits */
    sd->card_status &= ~CARD_STATUS_C;
}

static void sd_response_r3_make(SDState *sd, uint8_t *response)
{
    stl_be_p(response, sd->ocr & ACMD41_R3_MASK);
    f1c100s_log_mask(LOG_GUEST_TRACE, "response R3: %08x[%08x]\n", *(uint32_t *)response, sd->ocr & ACMD41_R3_MASK);
}

static void sd_response_r6_make(SDState *sd, uint8_t *response)
{
    uint16_t status;

    status = ((sd->card_status >> 8) & 0xc000) |
             ((sd->card_status >> 6) & 0x2000) |
              (sd->card_status & 0x1fff);
    sd->card_status &= ~(CARD_STATUS_C & 0xc81fff);
    stw_be_p(response + 0, sd->rca);
    stw_be_p(response + 2, status);
}

static void sd_response_r7_make(SDState *sd, uint8_t *response)
{
    stl_be_p(response, sd->vhs);
}

static inline uint64_t sd_addr_to_wpnum(uint64_t addr)
{
    return addr >> (HWBLOCK_SHIFT + SECTOR_SHIFT + WPGROUP_SHIFT);
}

static void sd_reset(SDState *sd)
{
    uint64_t size;
    uint64_t sect;

    trace_sdcard_reset();
    if (sd->blk) {
        blk_get_geometry(sd->blk, &sect);
    } else {
        sect = 0;
    }
    size = sect << 9;

    sect = sd_addr_to_wpnum(size) + 1;

    sd->state = sd_idle_state;
    sd->rca = 0x0000;
    sd->size = size;
    sd_set_ocr(sd);
    sd_set_scr(sd);
    sd_set_cid(sd);
    sd_set_csd(sd, size);
    sd_set_cardstatus(sd);
    sd_set_sdstatus(sd);

    free(sd->wp_group_bmap);
    //froloff sd->wp_switch = sd->blk ? !blk_is_writable(sd->blk) : false;
    sd->wp_group_bits = sect;
    sd->wp_group_bmap = bitmap_new(sd->wp_group_bits);
    memset(sd->function_group, 0, sizeof(sd->function_group));
    sd->erase_start = INVALID_ADDRESS;
    sd->erase_end = INVALID_ADDRESS;
    sd->blk_len = 0x200;
    sd->pwd_len = 0;
    sd->expecting_acmd = false;
    sd->dat_lines = 0xf;
    sd->cmd_line = true;
    sd->multi_blk_cnt = 0;
}

static bool sd_get_inserted(SDState *sd)
{
    //froloff return sd->blk && blk_is_inserted(sd->blk);
    return true;
}

static bool sd_get_readonly(SDState *sd)
{
    return sd->wp_switch;
}

static void sd_cardchange(SDState *sd, bool load/*, Error **errp*/)
{
    //froloff DeviceState *dev = DEVICE(sd);
    //froloff SDBus *sdbus;
    bool inserted = sd_get_inserted(sd);
    bool readonly = sd_get_readonly(sd);

    if (inserted) {
        trace_sdcard_inserted(readonly);
        sd_reset(sd);
    } else {
        trace_sdcard_ejected();
    }

    if (sd->me_no_qdev_me_kill_mammoth_with_rocks) {
        qemu_set_irq(sd->inserted_cb, inserted);
        if (inserted) {
            qemu_set_irq(sd->readonly_cb, readonly);
        }
    } else {
        //froloff sdbus = SD_BUS(qdev_get_parent_bus(dev));
        //froloff sdbus_set_inserted(sdbus, inserted);
        if (inserted) {
        //froloff     sdbus_set_readonly(sdbus, readonly);
        }
    }
}

static bool sd_ocr_vmstate_needed(SDState *sd)
{
    /* Include the OCR state (and timer) if it is not yet powered up */
    return !FIELD_EX32(sd->ocr, OCR, CARD_POWER_UP);
}

static int sd_vmstate_pre_load(SDState *sd)
{
    /* If the OCR state is not included (prior versions, or not
     * needed), then the OCR must be set as powered up. If the OCR state
     * is included, this will be replaced by the state restore.
     */
    sd_ocr_powerup(sd);

    return 0;
}

/* Legacy initialization function for use by non-qdevified callers */
SDState *sd_init(SDState *sd, bool is_spi)
{
    //qdev_prop_set_bit(dev, "spi", is_spi);
    if (!sd) {
        sd = (SDState *)malloc(sizeof(SDState));
        memset(sd, 0, sizeof(SDState));
        sd->blk = fopen(SD_IMAGE, "r+");
	sd->spi = is_spi;
	sd->spec_version = SD_PHY_SPECv2_00_VERS;
	sd_realize(sd);
        sd_reset(sd);
        sd_enable(sd, true);
    }
    sd->me_no_qdev_me_kill_mammoth_with_rocks = true;
    return sd;
}

void sd_set_cb(SDState *sd, qemu_irq readonly, qemu_irq insert)
{
    sd->readonly_cb = readonly;
    sd->inserted_cb = insert;
    qemu_set_irq(readonly, sd->blk ? !blk_is_writable(sd->blk) : 0);
    qemu_set_irq(insert, sd->blk ? blk_is_inserted(sd->blk) : 0);
}

static void sd_blk_read(SDState *sd, uint64_t addr, uint32_t len)
{
    trace_sdcard_read_block(addr, len);
    if (!sd->blk || blk_pread(sd->blk, addr, sd->data, len) < 0) {
        fprintf(stderr, "sd_blk_read: read error on host side\n");
    }
}

static void sd_blk_write(SDState *sd, uint64_t addr, uint32_t len)
{
    trace_sdcard_write_block(addr, len);
    if (!sd->blk || blk_pwrite(sd->blk, addr, sd->data, len, 0) < 0) {
        fprintf(stderr, "sd_blk_write: write error on host side\n");
    }
}

#define BLK_READ_BLOCK(a, len)  sd_blk_read(sd, a, len)
#define BLK_WRITE_BLOCK(a, len) sd_blk_write(sd, a, len)
#define APP_READ_BLOCK(a, len)  memset(sd->data, 0xec, len)
#define APP_WRITE_BLOCK(a, len)

static void sd_erase(SDState *sd)
{
    uint64_t erase_start = sd->erase_start;
    uint64_t erase_end = sd->erase_end;
    bool sdsc = true;
    uint64_t wpnum;
    uint64_t erase_addr;
    int erase_len = 1 << HWBLOCK_SHIFT;

    trace_sdcard_erase(sd->erase_start, sd->erase_end);
    if (sd->erase_start == INVALID_ADDRESS
            || sd->erase_end == INVALID_ADDRESS) {
        sd->card_status |= ERASE_SEQ_ERROR;
        sd->erase_start = INVALID_ADDRESS;
        sd->erase_end = INVALID_ADDRESS;
        return;
    }

    if (FIELD_EX32(sd->ocr, OCR, CARD_CAPACITY)) {
        /* High capacity memory card: erase units are 512 byte blocks */
        erase_start *= 512;
        erase_end *= 512;
        sdsc = false;
    }

    if (erase_start > sd->size || erase_end > sd->size) {
        sd->card_status |= OUT_OF_RANGE;
        sd->erase_start = INVALID_ADDRESS;
        sd->erase_end = INVALID_ADDRESS;
        return;
    }

    sd->erase_start = INVALID_ADDRESS;
    sd->erase_end = INVALID_ADDRESS;
    sd->csd[14] |= 0x40;

    memset(sd->data, 0xff, erase_len);
    for (erase_addr = erase_start; erase_addr <= erase_end;
         erase_addr += erase_len) {
        if (sdsc) {
            /* Only SDSC cards support write protect groups */
            wpnum = sd_addr_to_wpnum(erase_addr);
            assert(wpnum < sd->wp_group_bits);
            if (test_bit(wpnum, sd->wp_group_bmap)) {
                sd->card_status |= WP_ERASE_SKIP;
                continue;
            }
        }
        BLK_WRITE_BLOCK(erase_addr, erase_len);
    }
}

static uint32_t sd_wpbits(SDState *sd, uint64_t addr)
{
    uint32_t i, wpnum;
    uint32_t ret = 0;

    wpnum = sd_addr_to_wpnum(addr);

    for (i = 0; i < 32; i++, wpnum++, addr += WPGROUP_SIZE) {
        if (addr >= sd->size) {
            /*
             * If the addresses of the last groups are outside the valid range,
             * then the corresponding write protection bits shall be set to 0.
             */
            continue;
        }
        assert(wpnum < sd->wp_group_bits);
        if (test_bit(wpnum, sd->wp_group_bmap)) {
            ret |= (1 << i);
        }
    }

    return ret;
}

static void sd_function_switch(SDState *sd, uint32_t arg)
{
    int i, mode, new_func;
    mode = !!(arg & 0x80000000);

    sd->data[0] = 0x00;		/* Maximum current consumption */
    sd->data[1] = 0x01;
    sd->data[2] = 0x80;		/* Supported group 6 functions */
    sd->data[3] = 0x01;
    sd->data[4] = 0x80;		/* Supported group 5 functions */
    sd->data[5] = 0x01;
    sd->data[6] = 0x80;		/* Supported group 4 functions */
    sd->data[7] = 0x01;
    sd->data[8] = 0x80;		/* Supported group 3 functions */
    sd->data[9] = 0x01;
    sd->data[10] = 0x80;	/* Supported group 2 functions */
    sd->data[11] = 0x43;
    sd->data[12] = 0x80;	/* Supported group 1 functions */
    sd->data[13] = 0x03;

    memset(&sd->data[14], 0, 3);
    for (i = 0; i < 6; i ++) {
        new_func = (arg >> (i * 4)) & 0x0f;
        if (mode && new_func != 0x0f)
            sd->function_group[i] = new_func;
        sd->data[16 - (i >> 1)] |= new_func << ((i % 2) * 4);
    }
    memset(&sd->data[17], 0, 47);
}

static inline bool sd_wp_addr(SDState *sd, uint64_t addr)
{
    return test_bit(sd_addr_to_wpnum(addr), sd->wp_group_bmap);
}

static void sd_lock_command(SDState *sd)
{
    int erase, lock, clr_pwd, set_pwd, pwd_len;
    erase = !!(sd->data[0] & 0x08);
    lock = sd->data[0] & 0x04;
    clr_pwd = sd->data[0] & 0x02;
    set_pwd = sd->data[0] & 0x01;

    if (sd->blk_len > 1)
        pwd_len = sd->data[1];
    else
        pwd_len = 0;

    if (lock) {
        trace_sdcard_lock();
    } else {
        trace_sdcard_unlock();
    }
    if (erase) {
        if (!(sd->card_status & CARD_IS_LOCKED) || sd->blk_len > 1 ||
                        set_pwd || clr_pwd || lock || sd->wp_switch ||
                        (sd->csd[14] & 0x20)) {
            sd->card_status |= LOCK_UNLOCK_FAILED;
            return;
        }
        bitmap_zero(sd->wp_group_bmap, sd->wp_group_bits);
        sd->csd[14] &= ~0x10;
        sd->card_status &= ~CARD_IS_LOCKED;
        sd->pwd_len = 0;
        /* Erasing the entire card here! */
        fprintf(stderr, "SD: Card force-erased by CMD42\n");
        return;
    }

    if (sd->blk_len < 2 + pwd_len ||
                    pwd_len <= sd->pwd_len ||
                    pwd_len > sd->pwd_len + 16) {
        sd->card_status |= LOCK_UNLOCK_FAILED;
        return;
    }

    if (sd->pwd_len && memcmp(sd->pwd, sd->data + 2, sd->pwd_len)) {
        sd->card_status |= LOCK_UNLOCK_FAILED;
        return;
    }

    pwd_len -= sd->pwd_len;
    if ((pwd_len && !set_pwd) ||
                    (clr_pwd && (set_pwd || lock)) ||
                    (lock && !sd->pwd_len && !set_pwd) ||
                    (!set_pwd && !clr_pwd &&
                     (((sd->card_status & CARD_IS_LOCKED) && lock) ||
                      (!(sd->card_status & CARD_IS_LOCKED) && !lock)))) {
        sd->card_status |= LOCK_UNLOCK_FAILED;
        return;
    }

    if (set_pwd) {
        memcpy(sd->pwd, sd->data + 2 + sd->pwd_len, pwd_len);
        sd->pwd_len = pwd_len;
    }

    if (clr_pwd) {
        sd->pwd_len = 0;
    }

    if (lock)
        sd->card_status |= CARD_IS_LOCKED;
    else
        sd->card_status &= ~CARD_IS_LOCKED;
}

static bool address_in_range(SDState *sd, const char *desc,
                             uint64_t addr, uint32_t length)
{
    if (addr + length > sd->size) {
        f1c100s_log_mask(LOG_GUEST_ERROR,
                      "%s offset %llu > card %llu [%%%u]\n",
                      desc, addr, sd->size, length);
        sd->card_status |= ADDRESS_ERROR;
        return false;
    }
    return true;
}

static sd_rsp_type_t sd_normal_command(SDState *sd, SDRequest req)
{
    uint32_t rca = 0x0000;
    uint64_t addr = (sd->ocr & R_OCR_CARD_CAPACITY_MASK) ? (uint64_t) req.arg << 9 : req.arg;

    /* CMD55 precedes an ACMD, so we are not interested in tracing it.
     * However there is no ACMD55, so we want to trace this particular case.
     */
    if (req.cmd != 55 || sd->expecting_acmd) {
        trace_sdcard_normal_command(sd->proto_name,
                                    sd_cmd_name(req.cmd), req.cmd,
                                    req.arg, sd_state_name(sd->state));
    }

    /* Not interpreting this as an app command */
    sd->card_status &= ~APP_CMD;

    if (sd_cmd_type[req.cmd] == sd_ac
        || sd_cmd_type[req.cmd] == sd_adtc) {
        rca = req.arg >> 16;
    }

    /* CMD23 (set block count) must be immediately followed by CMD18 or CMD25
     * if not, its effects are cancelled */
    if (sd->multi_blk_cnt != 0 && !(req.cmd == 18 || req.cmd == 25)) {
        sd->multi_blk_cnt = 0;
    }

    if (sd_cmd_class[req.cmd] == 6 && FIELD_EX32(sd->ocr, OCR, CARD_CAPACITY)) {
        /* Only Standard Capacity cards support class 6 commands */
        return sd_illegal;
    }

    switch (req.cmd) {
    /* Basic commands (Class 0 and Class 1) */
    case 0:	/* CMD0:   GO_IDLE_STATE */
        switch (sd->state) {
        case sd_inactive_state:
            return sd->spi ? sd_r1 : sd_r0;

        default:
            sd->state = sd_idle_state;
            sd_reset(sd);
            return sd->spi ? sd_r1 : sd_r0;
        }
        break;

    case 1:	/* CMD1:   SEND_OP_CMD */
        if (!sd->spi)
            goto bad_cmd;

        sd->state = sd_transfer_state;
        return sd_r1;

    case 2:	/* CMD2:   ALL_SEND_CID */
        if (sd->spi)
            goto bad_cmd;
        switch (sd->state) {
        case sd_ready_state:
            sd->state = sd_identification_state;
            return sd_r2_i;

        default:
            break;
        }
        break;

    case 3:	/* CMD3:   SEND_RELATIVE_ADDR */
        if (sd->spi)
            goto bad_cmd;
        switch (sd->state) {
        case sd_identification_state:
        case sd_standby_state:
            sd->state = sd_standby_state;
            sd_set_rca(sd);
            return sd_r6;

        default:
            break;
        }
        break;

    case 4:	/* CMD4:   SEND_DSR */
        if (sd->spi)
            goto bad_cmd;
        switch (sd->state) {
        case sd_standby_state:
            break;

        default:
            break;
        }
        break;

    case 5: /* CMD5: reserved for SDIO cards */
        return sd_illegal;

    case 6:	/* CMD6:   SWITCH_FUNCTION */
        switch (sd->mode) {
        case sd_data_transfer_mode:
            sd_function_switch(sd, req.arg);
            sd->state = sd_sendingdata_state;
            sd->data_start = 0;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 7:	/* CMD7:   SELECT/DESELECT_CARD */
        if (sd->spi)
            goto bad_cmd;
        switch (sd->state) {
        case sd_standby_state:
            if (sd->rca != rca)
                return sd_r0;

            sd->state = sd_transfer_state;
            return sd_r1b;

        case sd_transfer_state:
        case sd_sendingdata_state:
            if (sd->rca == rca)
                break;

            sd->state = sd_standby_state;
            return sd_r1b;

        case sd_disconnect_state:
            if (sd->rca != rca)
                return sd_r0;

            sd->state = sd_programming_state;
            return sd_r1b;

        case sd_programming_state:
            if (sd->rca == rca)
                break;

            sd->state = sd_disconnect_state;
            return sd_r1b;

        default:
            break;
        }
        break;

    case 8:	/* CMD8:   SEND_IF_COND */
        if (sd->spec_version < SD_PHY_SPECv2_00_VERS) {
            break;
        }
        if (sd->state != sd_idle_state) {
            break;
        }
        sd->vhs = 0;

        /* No response if not exactly one VHS bit is set.  */
        if (!(req.arg >> 8) || (req.arg >> (ctz32(req.arg & ~0xff) + 1))) {
            return sd->spi ? sd_r7 : sd_r0;
        }

        /* Accept.  */
        sd->vhs = req.arg;
        return sd_r7;

    case 9:	/* CMD9:   SEND_CSD */
        switch (sd->state) {
        case sd_standby_state:
            if (sd->rca != rca)
                return sd_r0;

            return sd_r2_s;

        case sd_transfer_state:
            if (!sd->spi)
                break;
            sd->state = sd_sendingdata_state;
            memcpy(sd->data, sd->csd, 16);
            sd->data_start = addr;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 10:	/* CMD10:  SEND_CID */
        switch (sd->state) {
        case sd_standby_state:
            if (sd->rca != rca)
                return sd_r0;

            return sd_r2_i;

        case sd_transfer_state:
            if (!sd->spi)
                break;
            sd->state = sd_sendingdata_state;
            memcpy(sd->data, sd->cid, 16);
            sd->data_start = addr;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 12:	/* CMD12:  STOP_TRANSMISSION */
        switch (sd->state) {
        case sd_sendingdata_state:
            sd->state = sd_transfer_state;
            return sd_r1b;

        case sd_receivingdata_state:
            sd->state = sd_programming_state;
            /* Bzzzzzzztt .... Operation complete.  */
            sd->state = sd_transfer_state;
            return sd_r1b;

        default:
            break;
        }
        break;

    case 13:	/* CMD13:  SEND_STATUS */
        switch (sd->mode) {
        case sd_data_transfer_mode:
            if (!sd->spi && sd->rca != rca) {
                return sd_r0;
            }

            return sd_r1;

        default:
            break;
        }
        break;

    case 15:	/* CMD15:  GO_INACTIVE_STATE */
        if (sd->spi)
            goto bad_cmd;
        switch (sd->mode) {
        case sd_data_transfer_mode:
            if (sd->rca != rca)
                return sd_r0;

            sd->state = sd_inactive_state;
            return sd_r0;

        default:
            break;
        }
        break;

    /* Block read commands (Classs 2) */
    case 16:	/* CMD16:  SET_BLOCKLEN */
        switch (sd->state) {
        case sd_transfer_state:
            if (req.arg > (1 << HWBLOCK_SHIFT)) {
                sd->card_status |= BLOCK_LEN_ERROR;
            } else {
                trace_sdcard_set_blocklen(req.arg);
                sd->blk_len = req.arg;
            }

            return sd_r1;

        default:
            break;
        }
        break;

    case 17:	/* CMD17:  READ_SINGLE_BLOCK */
    case 18:	/* CMD18:  READ_MULTIPLE_BLOCK */
        switch (sd->state) {
        case sd_transfer_state:

            if (!address_in_range(sd, "READ_BLOCK", addr, sd->blk_len)) {
                return sd_r1;
            }
            f1c100s_log_mask(LOG_GUEST_TRACE, "Data start:%08x\n", addr);

            sd->state = sd_sendingdata_state;
            sd->data_start = addr;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 19:    /* CMD19: SEND_TUNING_BLOCK (SD) */
        if (sd->spec_version < SD_PHY_SPECv3_01_VERS) {
            break;
        }
        if (sd->state == sd_transfer_state) {
            sd->state = sd_sendingdata_state;
            sd->data_offset = 0;
            return sd_r1;
        }
        break;

    case 23:    /* CMD23: SET_BLOCK_COUNT */
        if (sd->spec_version < SD_PHY_SPECv3_01_VERS) {
            break;
        }
        switch (sd->state) {
        case sd_transfer_state:
            sd->multi_blk_cnt = req.arg;
            return sd_r1;

        default:
            break;
        }
        break;

    /* Block write commands (Class 4) */
    case 24:	/* CMD24:  WRITE_SINGLE_BLOCK */
    case 25:	/* CMD25:  WRITE_MULTIPLE_BLOCK */
        switch (sd->state) {
        case sd_transfer_state:

            if (!address_in_range(sd, "WRITE_BLOCK", addr, sd->blk_len)) {
                return sd_r1;
            }

            sd->state = sd_receivingdata_state;
            sd->data_start = addr;
            sd->data_offset = 0;
            sd->blk_written = 0;

            if (sd->size <= SDSC_MAX_CAPACITY) {
                if (sd_wp_addr(sd, sd->data_start)) {
                    sd->card_status |= WP_VIOLATION;
                }
            }
            if (sd->csd[14] & 0x30) {
                sd->card_status |= WP_VIOLATION;
            }
            return sd_r1;

        default:
            break;
        }
        break;

    case 26:	/* CMD26:  PROGRAM_CID */
        if (sd->spi)
            goto bad_cmd;
        switch (sd->state) {
        case sd_transfer_state:
            sd->state = sd_receivingdata_state;
            sd->data_start = 0;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 27:	/* CMD27:  PROGRAM_CSD */
        switch (sd->state) {
        case sd_transfer_state:
            sd->state = sd_receivingdata_state;
            sd->data_start = 0;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    /* Write protection (Class 6) */
    case 28:	/* CMD28:  SET_WRITE_PROT */
        if (sd->size > SDSC_MAX_CAPACITY) {
            return sd_illegal;
        }

        switch (sd->state) {
        case sd_transfer_state:
            if (!address_in_range(sd, "SET_WRITE_PROT", addr, 1)) {
                return sd_r1b;
            }

            sd->state = sd_programming_state;
            set_bit(sd_addr_to_wpnum(addr), sd->wp_group_bmap);
            /* Bzzzzzzztt .... Operation complete.  */
            sd->state = sd_transfer_state;
            return sd_r1b;

        default:
            break;
        }
        break;

    case 29:	/* CMD29:  CLR_WRITE_PROT */
        if (sd->size > SDSC_MAX_CAPACITY) {
            return sd_illegal;
        }

        switch (sd->state) {
        case sd_transfer_state:
            if (!address_in_range(sd, "CLR_WRITE_PROT", addr, 1)) {
                return sd_r1b;
            }

            sd->state = sd_programming_state;
            clear_bit(sd_addr_to_wpnum(addr), sd->wp_group_bmap);
            /* Bzzzzzzztt .... Operation complete.  */
            sd->state = sd_transfer_state;
            return sd_r1b;

        default:
            break;
        }
        break;

    case 30:	/* CMD30:  SEND_WRITE_PROT */
        if (sd->size > SDSC_MAX_CAPACITY) {
            return sd_illegal;
        }

        switch (sd->state) {
        case sd_transfer_state:
            if (!address_in_range(sd, "SEND_WRITE_PROT",
                                  req.arg, sd->blk_len)) {
                return sd_r1;
            }

            sd->state = sd_sendingdata_state;
            *(uint32_t *) sd->data = sd_wpbits(sd, req.arg);
            sd->data_start = addr;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    /* Erase commands (Class 5) */
    case 32:	/* CMD32:  ERASE_WR_BLK_START */
        switch (sd->state) {
        case sd_transfer_state:
            sd->erase_start = req.arg;
            return sd_r1;

        default:
            break;
        }
        break;

    case 33:	/* CMD33:  ERASE_WR_BLK_END */
        switch (sd->state) {
        case sd_transfer_state:
            sd->erase_end = req.arg;
            return sd_r1;

        default:
            break;
        }
        break;

    case 38:	/* CMD38:  ERASE */
        switch (sd->state) {
        case sd_transfer_state:
            if (sd->csd[14] & 0x30) {
                sd->card_status |= WP_VIOLATION;
                return sd_r1b;
            }

            sd->state = sd_programming_state;
            sd_erase(sd);
            /* Bzzzzzzztt .... Operation complete.  */
            sd->state = sd_transfer_state;
            return sd_r1b;

        default:
            break;
        }
        break;

    /* Lock card commands (Class 7) */
    case 42:	/* CMD42:  LOCK_UNLOCK */
        switch (sd->state) {
        case sd_transfer_state:
            sd->state = sd_receivingdata_state;
            sd->data_start = 0;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 52 ... 54:
        /* CMD52, CMD53, CMD54: reserved for SDIO cards
         * (see the SDIO Simplified Specification V2.0)
         * Handle as illegal command but do not complain
         * on stderr, as some OSes may use these in their
         * probing for presence of an SDIO card.
         */
        return sd_illegal;

    /* Application specific commands (Class 8) */
    case 55:	/* CMD55:  APP_CMD */
        switch (sd->state) {
        case sd_ready_state:
        case sd_identification_state:
        case sd_inactive_state:
            return sd_illegal;
        case sd_idle_state:
            if (rca) {
                f1c100s_log_mask(LOG_GUEST_ERROR,
                              "SD: illegal RCA 0x%04x for APP_CMD\n", req.cmd);
            }
        default:
            break;
        }
        if (!sd->spi) {
            if (sd->rca != rca) {
                return sd_r0;
            }
        }
        sd->expecting_acmd = true;
        sd->card_status |= APP_CMD;
        return sd_r1;

    case 56:	/* CMD56:  GEN_CMD */
        switch (sd->state) {
        case sd_transfer_state:
            sd->data_offset = 0;
            if (req.arg & 1)
                sd->state = sd_sendingdata_state;
            else
                sd->state = sd_receivingdata_state;
            return sd_r1;

        default:
            break;
        }
        break;

    case 58:    /* CMD58:   READ_OCR (SPI) */
        if (!sd->spi) {
            goto bad_cmd;
        }
        return sd_r3;

    case 59:    /* CMD59:   CRC_ON_OFF (SPI) */
        if (!sd->spi) {
            goto bad_cmd;
        }
        return sd_r1;

    default:
    bad_cmd:
        f1c100s_log_mask(LOG_GUEST_ERROR, "SD: Unknown CMD%i\n", req.cmd);
        return sd_illegal;
    }

    f1c100s_log_mask(LOG_GUEST_ERROR, "SD: CMD%i in a wrong state: %s\n",
                  req.cmd, sd_state_name(sd->state));
    return sd_illegal;
}

static sd_rsp_type_t sd_app_command(SDState *sd, SDRequest req)
{
    trace_sdcard_app_command(sd->proto_name, sd_acmd_name(req.cmd),
                             req.cmd, req.arg, sd_state_name(sd->state));
    sd->card_status |= APP_CMD;
    switch (req.cmd) {
    case 6:	/* ACMD6:  SET_BUS_WIDTH */
        if (sd->spi) {
            goto unimplemented_spi_cmd;
        }
        switch (sd->state) {
        case sd_transfer_state:
            sd->sd_status[0] &= 0x3f;
            sd->sd_status[0] |= (req.arg & 0x03) << 6;
            return sd_r1;

        default:
            break;
        }
        break;

    case 13:	/* ACMD13: SD_STATUS */
        switch (sd->state) {
        case sd_transfer_state:
            sd->state = sd_sendingdata_state;
            sd->data_start = 0;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 22:	/* ACMD22: SEND_NUM_WR_BLOCKS */
        switch (sd->state) {
        case sd_transfer_state:
            *(uint32_t *) sd->data = sd->blk_written;

            sd->state = sd_sendingdata_state;
            sd->data_start = 0;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 23:	/* ACMD23: SET_WR_BLK_ERASE_COUNT */
        switch (sd->state) {
        case sd_transfer_state:
            return sd_r1;

        default:
            break;
        }
        break;

    case 41:	/* ACMD41: SD_APP_OP_COND */
        if (sd->spi) {
            /* SEND_OP_CMD */
            sd->state = sd_transfer_state;
            return sd_r1;
        }
        if (sd->state != sd_idle_state) {
            break;
        }
        /* If it's the first ACMD41 since reset, we need to decide
         * whether to power up. If this is not an enquiry ACMD41,
         * we immediately report power on and proceed below to the
         * ready state, but if it is, we set a timer to model a
         * delay for power up. This works around a bug in EDK2
         * UEFI, which sends an initial enquiry ACMD41, but
         * assumes that the card is in ready state as soon as it
         * sees the power up bit set. */
        if (!FIELD_EX32(sd->ocr, OCR, CARD_POWER_UP)) {
            if ((req.arg & ACMD41_ENQUIRY_MASK) != 0) {
                //froloff timer_del(sd->ocr_power_timer);
                sd_ocr_powerup(sd);
            } else {
                trace_sdcard_inquiry_cmd41();
#if 0 //froloff
                if (!timer_pending(sd->ocr_power_timer)) {
                    timer_mod_ns(sd->ocr_power_timer,
                                 (qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
                                  + OCR_POWER_DELAY_NS));
                }
#endif
            }
        }

        f1c100s_log_mask(LOG_GUEST_TRACE,"ocr:%08x arg:%08x (%08x)\n", sd->ocr, req.arg, sd->ocr & req.arg);

        if (FIELD_EX32(sd->ocr & req.arg, OCR, VDD_VOLTAGE_WINDOW)) {
            /* We accept any voltage.  10000 V is nothing.
             *
             * Once we're powered up, we advance straight to ready state
             * unless it's an enquiry ACMD41 (bits 23:0 == 0).
             */
            sd->state = sd_ready_state;
        }

        return sd_r3;

    case 42:	/* ACMD42: SET_CLR_CARD_DETECT */
        switch (sd->state) {
        case sd_transfer_state:
            /* Bringing in the 50KOhm pull-up resistor... Done.  */
            return sd_r1;

        default:
            break;
        }
        break;

    case 51:	/* ACMD51: SEND_SCR */
        switch (sd->state) {
        case sd_transfer_state:
            sd->state = sd_sendingdata_state;
            sd->data_start = 0;
            sd->data_offset = 0;
            return sd_r1;

        default:
            break;
        }
        break;

    case 18:    /* Reserved for SD security applications */
    case 25:
    case 26:
    case 38:
    case 43 ... 49:
        /* Refer to the "SD Specifications Part3 Security Specification" for
         * information about the SD Security Features.
         */
        f1c100s_log_mask(LOG_UNIMP, "SD: CMD%i Security not implemented\n",
                      req.cmd);
        return sd_illegal;

    default:
        /* Fall back to standard commands.  */
        return sd_normal_command(sd, req);

    unimplemented_spi_cmd:
        /* Commands that are recognised but not yet implemented in SPI mode.  */
        f1c100s_log_mask(LOG_UNIMP, "SD: CMD%i not implemented in SPI mode\n",
                      req.cmd);
        return sd_illegal;
    }

    f1c100s_log_mask(LOG_GUEST_ERROR, "SD: ACMD%i in a wrong state\n", req.cmd);
    return sd_illegal;
}

static int cmd_valid_while_locked(SDState *sd, const uint8_t cmd)
{
    /* Valid commands in locked state:
     * basic class (0)
     * lock card class (7)
     * CMD16
     * implicitly, the ACMD prefix CMD55
     * ACMD41 and ACMD42
     * Anything else provokes an "illegal command" response.
     */
    if (sd->expecting_acmd) {
        return cmd == 41 || cmd == 42;
    }
    if (cmd == 16 || cmd == 55) {
        return 1;
    }
    return sd_cmd_class[cmd] == 0 || sd_cmd_class[cmd] == 7;
}

int sd_do_command(SDState *sd, SDRequest *req, uint8_t *response) {
    int last_state;
    sd_rsp_type_t rtype;
    int rsplen;

    //f1c100s_log_mask(LOG_GUEST_TRACE, "Card state: %s status: %08x expecting_acmd %d\n", sd_state_name(sd->state), sd->card_status, sd->expecting_acmd);

    if (!sd->blk || !blk_is_inserted(sd->blk) || !sd->enable) {
        return 0;
    }

    if (sd_req_crc_validate(req)) {
        sd->card_status |= COM_CRC_ERROR;
        rtype = sd_illegal;
        goto send_response;
    }

    if (req->cmd >= SDMMC_CMD_MAX) {
        f1c100s_log_mask(LOG_GUEST_ERROR, "SD: incorrect command 0x%02x\n",
                      req->cmd);
        req->cmd &= 0x3f;
    }

    if (sd->card_status & CARD_IS_LOCKED) {
        if (!cmd_valid_while_locked(sd, req->cmd)) {
            sd->card_status |= ILLEGAL_COMMAND;
            sd->expecting_acmd = false;
            f1c100s_log_mask(LOG_GUEST_ERROR, "SD: Card is locked\n");
            rtype = sd_illegal;
            goto send_response;
        }
    }

    last_state = sd->state;
    sd_set_mode(sd);

    if (sd->expecting_acmd) {
        sd->expecting_acmd = false;
        rtype = sd_app_command(sd, *req);
    } else {
        rtype = sd_normal_command(sd, *req);
    }

    if (rtype == sd_illegal) {
        sd->card_status |= ILLEGAL_COMMAND;
    } else {
        /* Valid command, we can update the 'state before command' bits.
         * (Do this now so they appear in r1 responses.)
         */
        sd->current_cmd = req->cmd;
        sd->card_status &= ~CURRENT_STATE;
        sd->card_status |= (last_state << 9);
    }

send_response:
    switch (rtype) {
    case sd_r1:
    case sd_r1b:
        sd_response_r1_make(sd, response);
        rsplen = 4;
        break;

    case sd_r2_i:
        memcpy(response, sd->cid, sizeof(sd->cid));
        rsplen = 16;
        break;

    case sd_r2_s:
        memcpy(response, sd->csd, sizeof(sd->csd));
        rsplen = 16;
        break;

    case sd_r3:
        sd_response_r3_make(sd, response);
        rsplen = 4;
        break;

    case sd_r6:
        sd_response_r6_make(sd, response);
        rsplen = 4;
        break;

    case sd_r7:
        sd_response_r7_make(sd, response);
        rsplen = 4;
        break;

    case sd_r0:
    case sd_illegal:
        rsplen = 0;
        break;
    default:
        assert("unknown sd response");
    }
    trace_sdcard_response(sd_response_name(rtype), rsplen);

    if (rtype != sd_illegal) {
        /* Clear the "clear on valid command" status bits now we've
         * sent any response
         */
        sd->card_status &= ~CARD_STATUS_B;
    }

#ifdef DEBUG_SD
    qemu_hexdump(stderr, "Response", response, rsplen);
#endif

    return rsplen;
}

void sd_write_byte(SDState *sd, uint8_t value)
{
    int i;

    if (!sd->blk || !blk_is_inserted(sd->blk) || !sd->enable)
        return;

    if (sd->state != sd_receivingdata_state) {
        f1c100s_log_mask(LOG_GUEST_ERROR,
                      "%s: not in Receiving-Data state\n", __func__);
        return;
    }

    if (sd->card_status & (ADDRESS_ERROR | WP_VIOLATION))
        return;

    //trace_sdcard_write_data(sd->proto_name,
    //                        (sd->card_status & APP_CMD)?sd_acmd_name(sd->current_cmd):sd_cmd_name(sd->current_cmd),
    //                        sd->current_cmd, value);
    switch (sd->current_cmd) {
    case 24:	/* CMD24:  WRITE_SINGLE_BLOCK */
        sd->data[sd->data_offset ++] = value;
        if (sd->data_offset >= sd->blk_len) {
            /* TODO: Check CRC before committing */
            sd->state = sd_programming_state;
            BLK_WRITE_BLOCK(sd->data_start, sd->data_offset);
            sd->blk_written ++;
            sd->csd[14] |= 0x40;
            /* Bzzzzzzztt .... Operation complete.  */
            sd->state = sd_transfer_state;
        }
        break;

    case 25:	/* CMD25:  WRITE_MULTIPLE_BLOCK */
        if (sd->data_offset == 0) {
            /* Start of the block - let's check the address is valid */
            if (!address_in_range(sd, "WRITE_MULTIPLE_BLOCK",
                                  sd->data_start, sd->blk_len)) {
                break;
            }
            if (sd->size <= SDSC_MAX_CAPACITY) {
                if (sd_wp_addr(sd, sd->data_start)) {
                    sd->card_status |= WP_VIOLATION;
                    break;
                }
            }
        }
        sd->data[sd->data_offset++] = value;
        if (sd->data_offset >= sd->blk_len) {
            /* TODO: Check CRC before committing */
            sd->state = sd_programming_state;
            BLK_WRITE_BLOCK(sd->data_start, sd->data_offset);
            sd->blk_written++;
            sd->data_start += sd->blk_len;
            sd->data_offset = 0;
            sd->csd[14] |= 0x40;

            /* Bzzzzzzztt .... Operation complete.  */
            if (sd->multi_blk_cnt != 0) {
                if (--sd->multi_blk_cnt == 0) {
                    /* Stop! */
                    sd->state = sd_transfer_state;
                    break;
                }
            }

            sd->state = sd_receivingdata_state;
        }
        break;

    case 26:	/* CMD26:  PROGRAM_CID */
        sd->data[sd->data_offset ++] = value;
        if (sd->data_offset >= sizeof(sd->cid)) {
            /* TODO: Check CRC before committing */
            sd->state = sd_programming_state;
            for (i = 0; i < sizeof(sd->cid); i ++)
                if ((sd->cid[i] | 0x00) != sd->data[i])
                    sd->card_status |= CID_CSD_OVERWRITE;

            if (!(sd->card_status & CID_CSD_OVERWRITE))
                for (i = 0; i < sizeof(sd->cid); i ++) {
                    sd->cid[i] |= 0x00;
                    sd->cid[i] &= sd->data[i];
                }
            /* Bzzzzzzztt .... Operation complete.  */
            sd->state = sd_transfer_state;
        }
        break;

    case 27:	/* CMD27:  PROGRAM_CSD */
        sd->data[sd->data_offset ++] = value;
        if (sd->data_offset >= sizeof(sd->csd)) {
            /* TODO: Check CRC before committing */
            sd->state = sd_programming_state;
            for (i = 0; i < sizeof(sd->csd); i ++)
                if ((sd->csd[i] | sd_csd_rw_mask[i]) !=
                    (sd->data[i] | sd_csd_rw_mask[i]))
                    sd->card_status |= CID_CSD_OVERWRITE;

            /* Copy flag (OTP) & Permanent write protect */
            if (sd->csd[14] & ~sd->data[14] & 0x60)
                sd->card_status |= CID_CSD_OVERWRITE;

            if (!(sd->card_status & CID_CSD_OVERWRITE))
                for (i = 0; i < sizeof(sd->csd); i ++) {
                    sd->csd[i] |= sd_csd_rw_mask[i];
                    sd->csd[i] &= sd->data[i];
                }
            /* Bzzzzzzztt .... Operation complete.  */
            sd->state = sd_transfer_state;
        }
        break;

    case 42:	/* CMD42:  LOCK_UNLOCK */
        sd->data[sd->data_offset ++] = value;
        if (sd->data_offset >= sd->blk_len) {
            /* TODO: Check CRC before committing */
            sd->state = sd_programming_state;
            sd_lock_command(sd);
            /* Bzzzzzzztt .... Operation complete.  */
            sd->state = sd_transfer_state;
        }
        break;

    case 56:	/* CMD56:  GEN_CMD */
        sd->data[sd->data_offset ++] = value;
        if (sd->data_offset >= sd->blk_len) {
            APP_WRITE_BLOCK(sd->data_start, sd->data_offset);
            sd->state = sd_transfer_state;
        }
        break;

    default:
        f1c100s_log_mask(LOG_GUEST_ERROR, "%s: unknown command\n", __func__);
        break;
    }
}

#define SD_TUNING_BLOCK_SIZE    64

static const uint8_t sd_tuning_block_pattern[SD_TUNING_BLOCK_SIZE] = {
    /* See: Physical Layer Simplified Specification Version 3.01, Table 4-2 */
    0xff, 0x0f, 0xff, 0x00,         0x0f, 0xfc, 0xc3, 0xcc,
    0xc3, 0x3c, 0xcc, 0xff,         0xfe, 0xff, 0xfe, 0xef,
    0xff, 0xdf, 0xff, 0xdd,         0xff, 0xfb, 0xff, 0xfb,
    0xbf, 0xff, 0x7f, 0xff,         0x77, 0xf7, 0xbd, 0xef,
    0xff, 0xf0, 0xff, 0xf0,         0x0f, 0xfc, 0xcc, 0x3c,
    0xcc, 0x33, 0xcc, 0xcf,         0xff, 0xef, 0xff, 0xee,
    0xff, 0xfd, 0xff, 0xfd,         0xdf, 0xff, 0xbf, 0xff,
    0xbb, 0xff, 0xf7, 0xff,         0xf7, 0x7f, 0x7b, 0xde,
};

uint8_t sd_read_byte(SDState *sd)
{
    /* TODO: Append CRCs */
    uint8_t ret;
    uint32_t io_len;

    if (!sd->blk || !blk_is_inserted(sd->blk) || !sd->enable)
        return 0x00;

    if (sd->state != sd_sendingdata_state) {
        f1c100s_log_mask(LOG_GUEST_ERROR,
                      "%s: not in Sending-Data state\n", __func__);
        return 0x00;
    }

    if (sd->card_status & (ADDRESS_ERROR | WP_VIOLATION))
        return 0x00;

    io_len = (sd->ocr & R_OCR_CARD_CAPACITY_MASK) ? 512 : sd->blk_len;

    //trace_sdcard_read_data(sd->proto_name,
    //                       (sd->card_status & APP_CMD)?sd_acmd_name(sd->current_cmd):sd_cmd_name(sd->current_cmd),
    //                       sd->current_cmd, io_len);
    switch (sd->current_cmd) {
    case 6:	/* CMD6:   SWITCH_FUNCTION */
        ret = sd->data[sd->data_offset ++];

        if (sd->data_offset >= 64)
            sd->state = sd_transfer_state;
        break;

    case 9:	/* CMD9:   SEND_CSD */
    case 10:	/* CMD10:  SEND_CID */
        ret = sd->data[sd->data_offset ++];

        if (sd->data_offset >= 16)
            sd->state = sd_transfer_state;
        break;

    case 13:	/* ACMD13: SD_STATUS */
        ret = sd->sd_status[sd->data_offset ++];

        if (sd->data_offset >= sizeof(sd->sd_status))
            sd->state = sd_transfer_state;
        break;

    case 17:	/* CMD17:  READ_SINGLE_BLOCK */
        if (sd->data_offset == 0)
            BLK_READ_BLOCK(sd->data_start, io_len);
        ret = sd->data[sd->data_offset ++];

        if (sd->data_offset >= io_len)
            sd->state = sd_transfer_state;
        break;

    case 18:	/* CMD18:  READ_MULTIPLE_BLOCK */
        if (sd->data_offset == 0) {
            if (!address_in_range(sd, "READ_MULTIPLE_BLOCK",
                                  sd->data_start, io_len)) {
                return 0x00;
            }
            BLK_READ_BLOCK(sd->data_start, io_len);
        }
        ret = sd->data[sd->data_offset ++];

        if (sd->data_offset >= io_len) {
            sd->data_start += io_len;
            sd->data_offset = 0;

            if (sd->multi_blk_cnt != 0) {
                if (--sd->multi_blk_cnt == 0) {
                    /* Stop! */
                    sd->state = sd_transfer_state;
                    break;
                }
            }
        }
        break;

    case 19:    /* CMD19:  SEND_TUNING_BLOCK (SD) */
        if (sd->data_offset >= SD_TUNING_BLOCK_SIZE - 1) {
            sd->state = sd_transfer_state;
        }
        ret = sd_tuning_block_pattern[sd->data_offset++];
        break;

    case 22:	/* ACMD22: SEND_NUM_WR_BLOCKS */
        ret = sd->data[sd->data_offset ++];

        if (sd->data_offset >= 4)
            sd->state = sd_transfer_state;
        break;

    case 30:	/* CMD30:  SEND_WRITE_PROT */
        ret = sd->data[sd->data_offset ++];

        if (sd->data_offset >= 4)
            sd->state = sd_transfer_state;
        break;

    case 51:	/* ACMD51: SEND_SCR */
        ret = sd->scr[sd->data_offset ++];

        if (sd->data_offset >= sizeof(sd->scr))
            sd->state = sd_transfer_state;
        break;

    case 56:	/* CMD56:  GEN_CMD */
        if (sd->data_offset == 0)
            APP_READ_BLOCK(sd->data_start, sd->blk_len);
        ret = sd->data[sd->data_offset ++];

        if (sd->data_offset >= sd->blk_len)
            sd->state = sd_transfer_state;
        break;

    default:
        f1c100s_log_mask(LOG_GUEST_ERROR, "%s: unknown command\n", __func__);
        return 0x00;
    }

    return ret;
}

bool sd_receive_ready(SDState *sd)
{
    return sd->state == sd_receivingdata_state;
}

bool sd_data_ready(SDState *sd)
{
    //f1c100s_log(" SD%s ready\n", sd->state == sd_sendingdata_state ? "":" not");
    return sd->state == sd_sendingdata_state;
}

void sd_enable(SDState *sd, bool enable)
{
    sd->enable = enable;
}

static void sd_instance_init(SDState *sd)
{
    sd->enable = true;
    //sd->ocr_power_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, sd_ocr_powerup, sd);
}

static void sd_instance_finalize(SDState *sd)
{
    //timer_free(sd->ocr_power_timer);
}

static void sd_realize(SDState *sd)
{
    int ret;

    sd->proto_name = sd->spi ? "SPI" : "SD";

    switch (sd->spec_version) {
    case SD_PHY_SPECv1_10_VERS
     ... SD_PHY_SPECv3_01_VERS:
        break;
    default:
        f1c100s_log_mask(LOG_GUEST_ERROR, "Invalid SD card Spec version: %u", sd->spec_version);
        return;
    }

    if (sd->blk) {
        int64_t blk_size;

        if (!blk_supports_write_perm(sd->blk)) {
            f1c100s_log_mask(LOG_GUEST_ERROR, "Cannot use read-only drive as SD card");
            return;
        }

        blk_size = blk_getlength(sd->blk);
        if (blk_size > 0 && !is_power_of_2(blk_size)) {
            int64_t blk_size_aligned = pow2ceil(blk_size);

            f1c100s_log_mask(LOG_GUEST_ERROR, "Invalid SD card size: %lld", blk_size);

            f1c100s_log(
                              "SD card size has to be a power of 2, e.g. %lld.\n"
                              "You can resize disk images with"
                              " 'qemu-img resize <imagefile> <new-size>'\n"
                              "(note that this will lose data if you make the"
                              " image smaller than it currently is).\n",
                              blk_size_aligned);
            return;
        }

        //froloff ret = blk_set_perm(sd->blk, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
        //froloff                    BLK_PERM_ALL, errp);
	ret = 0;
        if (ret < 0) {
            return;
        }
        //froloff blk_set_dev_ops(sd->blk, &sd_block_ops, sd);
    }
}

