typedef struct SDState SDState;

#define OUT_OF_RANGE            (1 << 31)
#define ADDRESS_ERROR           (1 << 30)
#define BLOCK_LEN_ERROR         (1 << 29)
#define ERASE_SEQ_ERROR         (1 << 28)
#define ERASE_PARAM             (1 << 27)
#define WP_VIOLATION            (1 << 26)
#define CARD_IS_LOCKED          (1 << 25)
#define LOCK_UNLOCK_FAILED      (1 << 24)
#define COM_CRC_ERROR           (1 << 23)
#define ILLEGAL_COMMAND         (1 << 22)
#define CARD_ECC_FAILED         (1 << 21)
#define CC_ERROR                (1 << 20)
#define SD_ERROR                (1 << 19)
#define CID_CSD_OVERWRITE       (1 << 16)
#define WP_ERASE_SKIP           (1 << 15)
#define CARD_ECC_DISABLED       (1 << 14)
#define ERASE_RESET             (1 << 13)
#define CURRENT_STATE           (7 << 9)
#define READY_FOR_DATA          (1 << 8)
#define APP_CMD                 (1 << 5)
#define AKE_SEQ_ERROR           (1 << 3)

enum SDPhySpecificationVersion {
    SD_PHY_SPECv1_10_VERS     = 1,
    SD_PHY_SPECv2_00_VERS     = 2,
    SD_PHY_SPECv3_01_VERS     = 3,
};

typedef enum {
    SD_VOLTAGE_0_4V     = 400,  /* currently not supported */
    SD_VOLTAGE_1_8V     = 1800,
    SD_VOLTAGE_3_0V     = 3000,
    SD_VOLTAGE_3_3V     = 3300,
} sd_voltage_mv_t;

typedef enum  {
    UHS_NOT_SUPPORTED   = 0,
    UHS_I               = 1,
    UHS_II              = 2,    /* currently not supported */
    UHS_III             = 3,    /* currently not supported */
} sd_uhs_mode_t;

typedef enum {
    sd_none = -1,
    sd_bc = 0,	/* broadcast -- no response */
    sd_bcr,	/* broadcast with response */
    sd_ac,	/* addressed -- no data transfer */
    sd_adtc,	/* addressed with data transfer */
} sd_cmd_type_t;

typedef struct {
    uint8_t  cmd;
    uint32_t arg;
    uint8_t  crc;
} SDRequest;

SDState *sd_init(SDState *sd, bool is_spi);
bool     sd_data_ready(SDState *sd);
void     sd_enable(SDState *sd, bool enable);
int      sd_do_command(SDState *sd, SDRequest *req, uint8_t *response);
uint8_t  sd_read_byte(SDState *sd);
void     sd_write_byte(SDState *sd, uint8_t value);
