//----------------------------------------------------------------------------------------------------------------------------------

#ifndef F1C100S_MMC_H
#define F1C100S_MMC_H

//----------------------------------------------------------------------------------------------------------------------------------
/* MMC Host register offsets */
enum {
    SD_GCTL       = 0x00,  /* Global Control */
    SD_CKCR       = 0x04,  /* Clock Control */
    SD_TMOR       = 0x08,  /* Timeout */
    SD_BWDR       = 0x0C,  /* Bus Width */
    SD_BKSR       = 0x10,  /* Block Size */
    SD_BYCR       = 0x14,  /* Byte Count */
    SD_CMDR       = 0x18,  /* Command */
    SD_CARG       = 0x1C,  /* Command Argument */
    SD_RESP0      = 0x20,  /* Response Zero */
    SD_RESP1      = 0x24,  /* Response One */
    SD_RESP2      = 0x28,  /* Response Two */
    SD_RESP3      = 0x2C,  /* Response Three */
    SD_IMKR       = 0x30,  /* Interrupt Mask */
    SD_MISR       = 0x34,  /* Masked Interrupt Status */
    SD_RISR       = 0x38,  /* Raw Interrupt Status */
    SD_STAR       = 0x3C,  /* Status */
    SD_FWLR       = 0x40,  /* FIFO Water Level */
    SD_FUNS       = 0x44,  /* FIFO Function Select */
    SD_DBGC       = 0x50,  /* Debug Enable */
    SD_A12A       = 0x58,  /* Auto command 12 argument */
    SD_NTSR       = 0x5C,  /* SD NewTiming Set */
    SD_SDBG       = 0x60,  /* SD newTiming Set Debug */
    SD_HWRST      = 0x78,  /* Hardware Reset Register */
    SD_DMAC       = 0x80,  /* Internal DMA Controller Control */
    SD_DLBA       = 0x84,  /* Descriptor List Base Address */
    SD_IDST       = 0x88,  /* Internal DMA Controller Status */
    SD_IDIE       = 0x8C,  /* Internal DMA Controller IRQ Enable */
    SD_THLDC      = 0x100, /* Card Threshold Control */
    SD_DSBD       = 0x10C, /* eMMC DDR Start Bit Detection Control */
    SD_RES_CRC    = 0x110, /* Response CRC from card/eMMC */
    SD_DATA7_CRC  = 0x114, /* CRC Data 7 from card/eMMC */
    SD_DATA6_CRC  = 0x118, /* CRC Data 6 from card/eMMC */
    SD_DATA5_CRC  = 0x11C, /* CRC Data 5 from card/eMMC */
    SD_DATA4_CRC  = 0x120, /* CRC Data 4 from card/eMMC */
    SD_DATA3_CRC  = 0x124, /* CRC Data 3 from card/eMMC */
    SD_DATA2_CRC  = 0x128, /* CRC Data 2 from card/eMMC */
    SD_DATA1_CRC  = 0x12C, /* CRC Data 1 from card/eMMC */
    SD_DATA0_CRC  = 0x130, /* CRC Data 0 from card/eMMC */
    SD_CRC_STA    = 0x134, /* CRC status from card/eMMC during write */
    SD_FIFO       = 0x200, /* Read/Write FIFO */
};


/* MMC Host register reset values */
enum {
    REG_GCTL_RST         = 0x00000300,
    REG_CKCR_RST         = 0x0,
    REG_TMOR_RST         = 0xFFFFFF40,
    REG_BWDR_RST         = 0x0,
    REG_BKSR_RST         = 0x00000200,
    REG_BYCR_RST         = 0x00000200,
    REG_CMDR_RST         = 0x0,
    REG_CARG_RST         = 0x0,
    REG_RESP_RST         = 0x0,
    REG_IMKR_RST         = 0x0,
    REG_MISR_RST         = 0x0,
    REG_RISR_RST         = 0x0,
    REG_STAR_RST         = 0x00000100,
    REG_FWLR_RST         = 0x000F0000,
    REG_FUNS_RST         = 0x0,
    REG_DBGC_RST         = 0x0,
    REG_A12A_RST         = 0x0000FFFF,
    REG_NTSR_RST         = 0x00000001,
    REG_SDBG_RST         = 0x0,
    REG_HWRST_RST        = 0x00000001,
    REG_DMAC_RST         = 0x0,
    REG_DLBA_RST         = 0x0,
    REG_IDST_RST         = 0x0,
    REG_IDIE_RST         = 0x0,
    REG_THLDC_RST        = 0x0,
    REG_DSBD_RST         = 0x0,
    REG_RES_CRC_RST      = 0x0,
    REG_DATA_CRC_RST     = 0x0,
    REG_CRC_STA_RST      = 0x0,
    REG_FIFO_RST         = 0x0,
};


/* SD Host register flags */
enum {
    SD_GCTL_FIFO_AC_MOD     = (1 << 31),
    SD_GCTL_DDR_MOD_SEL     = (1 << 10),
    SD_GCTL_CD_DBC_ENB      = (1 << 8),
    SD_GCTL_DMA_ENB         = (1 << 5),
    SD_GCTL_INT_ENB         = (1 << 4),
    SD_GCTL_DMA_RST         = (1 << 2),
    SD_GCTL_FIFO_RST        = (1 << 1),
    SD_GCTL_SOFT_RST        = (1 << 0),
};

enum {
    SD_CMDR_LOAD            = (1 << 31),
    SD_CMDR_CLKCHANGE       = (1 << 21),
    SD_CMDR_SEND_INIT_SEQ   = (1 << 15),
    SD_CMDR_AUTOSTOP        = (1 << 12),
    SD_CMDR_WRITE           = (1 << 10),
    SD_CMDR_DATA            = (1 << 9),
    SD_CMDR_RESPONSE_CRC    = (1 << 8),    
    SD_CMDR_RESPONSE_LONG   = (1 << 7),
    SD_CMDR_RESPONSE        = (1 << 6),
    SD_CMDR_CMDID_MASK      = (0x3f),
};

enum {
    SD_RISR_CARD_REMOVE     = (1 << 31),
    SD_RISR_CARD_INSERT     = (1 << 30),
    SD_RISR_SDIO_INTR       = (1 << 16),
    SD_RISR_AUTOCMD_DONE    = (1 << 14),
    SD_RISR_DATA_COMPLETE   = (1 << 3),
    SD_RISR_CMD_COMPLETE    = (1 << 2),
    SD_RISR_NO_RESPONSE     = (1 << 1),
};

enum {
    SD_STAR_FIFO_EMPTY      = (1 << 2),
    SD_STAR_FIFO_FULL       = (1 << 3),
    SD_STAR_CARD_PRESENT    = (1 << 8),
    SD_STAR_FIFO_LEVEL_1    = (1 << 17),
};

enum {
    SD_IDST_INT_SUMMARY     = (1 << 8),
    SD_IDST_RECEIVE_IRQ     = (1 << 1),
    SD_IDST_TRANSMIT_IRQ    = (1 << 0),
    SD_IDST_IRQ_MASK        = (1 << 1) | (1 << 0) | (1 << 8),
    SD_IDST_WR_MASK         = (0x3ff),
};

/* Data transfer descriptor for DMA */
typedef struct TransferDescriptor {
    uint32_t status; /* Status flags */
    uint32_t size;   /* Data buffer size */
    uint32_t addr;   /* Data buffer address */
    uint32_t next;   /* Physical address of next descriptor */
} TransferDescriptor;

/* Data transfer descriptor flags */
enum {
    DESC_STATUS_HOLD   = (1 << 31), /* Set when descriptor is in use by DMA */
    DESC_STATUS_ERROR  = (1 << 30), /* Set when DMA transfer error occurred */
    DESC_STATUS_CHAIN  = (1 << 4),  /* Indicates chained descriptor. */
    DESC_STATUS_FIRST  = (1 << 3),  /* Set on the first descriptor */
    DESC_STATUS_LAST   = (1 << 2),  /* Set on the last descriptor */
    DESC_STATUS_NOIRQ  = (1 << 1),  /* Skip raising interrupt after transfer */
    DESC_SIZE_MASK     = (0xfffffffc)
};


//----------------------------------------------------------------------------------------------------------------------------------

#endif /* F1C100S_MMC_H */

