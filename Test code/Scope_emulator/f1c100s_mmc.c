//----------------------------------------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f1c100s.h"
#include "f1c100s_ccu.h"
#include "f1c100s_mmc.h"
#include "f1c100s_log.h"

#include "qemu_defs.h"
#include "sd.h"
#include "sd_trace.h"

//----------------------------------------------------------------------------------------------------------------------------------
#define MAX_DESC_SIZE 512
#define MEMTXATTRS_UNSPECIFIED 0
//----------------------------------------------------------------------------------------------------------------------------------

void *F1C100sMMC(F1C100S_MMC *registers, uint32_t address, uint32_t mode);
void  F1C100sMMCRead(F1C100S_MMC *registers, uint32_t address, uint32_t mode);
void  F1C100sMMCWrite(F1C100S_MMC *registers, uint32_t address, uint32_t mode);

//----------------------------------------------------------------------------------------------------------------------------------
static void allwinner_sdhost_reset(F1C100S_MMC *s)
{ 
    s->global_ctl.m_32bit = REG_GCTL_RST;
    s->clock_ctl.m_32bit = REG_CKCR_RST;
    s->timeout.m_32bit = REG_TMOR_RST;
    s->bus_width.m_32bit = REG_BWDR_RST;
    s->block_size.m_32bit = REG_BKSR_RST;
    s->byte_count.m_32bit = REG_BYCR_RST;
    s->transfer_cnt = 0;

    s->command.m_32bit = REG_CMDR_RST;
    s->command_arg.m_32bit = REG_CARG_RST;

    for (int i = 0; i < ARRAY_SIZE(s->response); i++) {
        s->response[i].m_32bit = REG_RESP_RST;
    }

    s->irq_mask.m_32bit = REG_IMKR_RST;
    s->irq_status.m_32bit = REG_RISR_RST;
    s->irq = s->irq_status.m_32bit;
    s->status.m_32bit = REG_STAR_RST;

    s->fifo_wlevel.m_32bit = REG_FWLR_RST;
    s->fifo_func_sel.m_32bit = REG_FUNS_RST;
    s->debug_enable.m_32bit = REG_DBGC_RST;
    s->auto12_arg.m_32bit = REG_A12A_RST;
    s->newtiming_set.m_32bit = REG_NTSR_RST;
    s->newtiming_debug.m_32bit = REG_SDBG_RST;
    s->hardware_rst.m_32bit = REG_HWRST_RST;
    s->dmac.m_32bit = REG_DMAC_RST;
    s->desc_base.m_32bit = REG_DLBA_RST;
    s->dma_status = s->dmac_status.m_32bit = REG_IDST_RST;
    s->dmac_irq.m_32bit = REG_IDIE_RST;
    s->card_threshold.m_32bit = REG_THLDC_RST;
    s->startbit_detect.m_32bit = REG_DSBD_RST;
    s->response_crc.m_32bit = REG_RES_CRC_RST;

    for (int i = 0; i < ARRAY_SIZE(s->data_crc); i++) {
        s->data_crc[i].m_32bit = REG_DATA_CRC_RST;
    }

    s->status_crc.m_32bit = REG_CRC_STA_RST;
}

static void allwinner_sdhost_send_command(F1C100S_MMC *s)
{
    SDRequest request;
    uint8_t   resp[16];
    int rlen;

    //s->irq_status.m_32bit = s->irq;
    /* Auto clear load flag */
    s->command.m_32bit &= ~SD_CMDR_LOAD;

    /* Clock change does not actually interact with the SD bus */
    if (!(s->command.m_32bit & SD_CMDR_CLKCHANGE)) {

        /* Prepare request */
        request.cmd = s->command.m_32bit & SD_CMDR_CMDID_MASK;
        request.arg = s->command_arg.m_32bit;

    	f1c100s_log(" %08x SD SEND CMD%d(%x)\n", s->command.m_32bit, request.cmd, request.arg);
        /* Send request to SD bus */
        rlen = sd_do_command(s->sd, &request, resp);
        if (rlen < 0) {
            goto error;
        }

        /* If the command has a response, store it in the response registers */
        if (s->command.m_32bit & SD_CMDR_RESPONSE) {
            if (rlen == 4) {
                if (!(s->command.m_32bit & SD_CMDR_RESPONSE_LONG)) {
                    s->response[0].m_32bit = ldl_be_p(&resp[0]);
                    s->response[1].m_32bit =
                    s->response[2].m_32bit =
                    s->response[3].m_32bit = 0;
                } else {
                    s->response[3].m_32bit = ldl_be_p(&resp[0]);
                    s->response[2].m_32bit =
                    s->response[1].m_32bit =
                    s->response[0].m_32bit = 0;
                }
            } else if (rlen == 16 && (s->command.m_32bit & SD_CMDR_RESPONSE_LONG)) {
                s->response[0].m_32bit = ldl_be_p(&resp[12]);
                s->response[1].m_32bit = ldl_be_p(&resp[8]);
                s->response[2].m_32bit = ldl_be_p(&resp[4]);
                s->response[3].m_32bit = ldl_be_p(&resp[0]);
            } else {
                goto error;
            }
        }
    } else {
    	f1c100s_log("<-(%x)[%x]\n", s->command.m_32bit, s->command_arg.m_32bit);    	
    }

    /* Set interrupt status bits */
    s->irq_status.m_32bit |= SD_RISR_CMD_COMPLETE;
    return;

error:
    s->irq_status.m_32bit |= SD_RISR_NO_RESPONSE;
}

//----------------------------------------------------------------------------------------------------------------------------------
static void allwinner_sdhost_update_transfer_cnt(F1C100S_MMC *s,
                                                 uint32_t bytes)
{
    if (s->transfer_cnt > bytes) {
        s->transfer_cnt -= bytes;
    } else {
        s->transfer_cnt = 0;
    }

    if (!s->transfer_cnt) {
        s->irq_status.m_32bit |= SD_RISR_DATA_COMPLETE;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
static void dma_memory_read(uint32_t *dma_as,
                            uint32_t addr, void *dest,
                            uint32_t size, uint32_t attr)
{
    // Only DRAM address space ATM
    if ((addr & 0x8E000000) == 0x80000000) {
        uint32_t idx;
        idx = addr & ~0x8E000000;
        memcpy(dest, &dma_as[idx >> 2], size);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
static void dma_memory_write(uint32_t *dma_as,
                             uint32_t addr, void *src,
                             uint32_t size, uint32_t attr)
{
    // Only DRAM address space
    if ((addr & 0x8E000000) == 0x80000000) {
        uint32_t idx;
        idx = addr & ~0x8E000000;
        memcpy(&dma_as[idx >> 2], src, size);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
static uint32_t allwinner_sdhost_process_desc(F1C100S_MMC *s,
                                              uint32_t desc_addr,
                                              TransferDescriptor *desc,
                                              bool is_write, uint32_t max_bytes)
{
    uint32_t num_done = 0;
    uint32_t num_bytes = max_bytes;
    uint8_t buf[1024];
    uint32_t i;

    /* Read descriptor */
    dma_memory_read(s->dma_as, desc_addr, desc, sizeof(*desc),
                    MEMTXATTRS_UNSPECIFIED);
    if (desc->size == 0) {
        desc->size = MAX_DESC_SIZE;
    } else if (desc->size > MAX_DESC_SIZE) {
        f1c100s_log_mask(LOG_GUEST_ERROR, "%s: DMA descriptor buffer size "
                         " is out-of-bounds: %u > %zu",
                         __func__, desc->size, MAX_DESC_SIZE);
        desc->size = MAX_DESC_SIZE;
    }
    if (desc->size < num_bytes) {
        num_bytes = desc->size;
    }

    trace_allwinner_sdhost_process_desc(desc_addr, desc->size,
                                        is_write, max_bytes);

    while (num_done < num_bytes) {
        /* Try to completely fill the local buffer */
        uint32_t buf_bytes = num_bytes - num_done;
        if (buf_bytes > sizeof(buf)) {
            buf_bytes = sizeof(buf);
        }

        /* Write to SD bus */
        if (is_write) {
            dma_memory_read(s->dma_as,
                            (desc->addr & DESC_SIZE_MASK) + num_done, buf,
                            buf_bytes, MEMTXATTRS_UNSPECIFIED);
            for (i = 0; i < buf_bytes; i++)
                sd_write_byte(s->sd, buf[i]);
            // sdbus_write_data(&s->sdbus, buf, buf_bytes);

        /* Read from SD bus */
        } else {
            //sdbus_read_data(&s->sdbus, buf, buf_bytes);
            for (i = 0; i < buf_bytes; i++)
                buf[i] = sd_read_byte(s->sd);
            dma_memory_write(s->dma_as,
                             (desc->addr & DESC_SIZE_MASK) + num_done, buf,
                             buf_bytes, MEMTXATTRS_UNSPECIFIED);
        }
        num_done += buf_bytes;
    }

    /* Clear hold flag and flush descriptor */
    desc->status &= ~DESC_STATUS_HOLD;
    dma_memory_write(s->dma_as, desc_addr, desc, sizeof(*desc),
                     MEMTXATTRS_UNSPECIFIED);

    return num_done;
}

//----------------------------------------------------------------------------------------------------------------------------------
static void allwinner_sdhost_dma(F1C100S_MMC *s)
{
    TransferDescriptor desc;
    uint32_t desc_addr = s->desc_base.m_32bit;
    bool is_write = (s->command.m_32bit & SD_CMDR_WRITE);
    uint32_t bytes_done = 0;

    /* Check if DMA can be performed */
    if (s->byte_count.m_32bit == 0 || s->block_size.m_32bit == 0 ||
      !(s->global_ctl.m_32bit & SD_GCTL_DMA_ENB)) {
        return;
    }

    /*
     * For read operations, data must be available on the SD bus
     * If not, it is an error and we should not act at all
     */
    if (!is_write && !sd_data_ready(s->sd)) {
        return;
    }

    /* Process the DMA descriptors until all data is copied */
    while (s->byte_count.m_32bit > 0) {
        bytes_done = allwinner_sdhost_process_desc(s, desc_addr, &desc,
                                                   is_write, s->byte_count.m_32bit);
        allwinner_sdhost_update_transfer_cnt(s, bytes_done);

        if (bytes_done <= s->byte_count.m_32bit) {
            s->byte_count.m_32bit -= bytes_done;
        } else {
            s->byte_count.m_32bit = 0;
        }

        if (desc.status & DESC_STATUS_LAST) {
            break;
        } else {
            desc_addr = desc.next;
        }
    }

    /* Raise IRQ to signal DMA is completed */
    s->irq_status.m_32bit |= SD_RISR_DATA_COMPLETE | SD_RISR_SDIO_INTR;

    /* Update DMAC bits */
    s->dmac_status.m_32bit |= SD_IDST_INT_SUMMARY;

    if (is_write) {
        s->dmac_status.m_32bit |= SD_IDST_TRANSMIT_IRQ;
    } else {
        s->dmac_status.m_32bit |= SD_IDST_RECEIVE_IRQ;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
static void allwinner_sdhost_auto_stop(F1C100S_MMC *s)
{
    /*
     * The stop command (CMD12) ensures the SD bus
     * returns to the transfer state.
     */
    if ((s->command.m_32bit & SD_CMDR_AUTOSTOP) && (s->transfer_cnt == 0)) {
        /* First save current command registers */
        uint32_t saved_cmd = s->command.m_32bit;
        uint32_t saved_arg = s->command_arg.m_32bit;

        /* Prepare stop command (CMD12) */
        s->command.m_32bit &= ~SD_CMDR_CMDID_MASK;
        s->command.m_32bit |= 12; /* CMD12 */
        s->command_arg.m_32bit = 0;

        /* Put the command on SD bus */
        allwinner_sdhost_send_command(s);

        /* Restore command values */
        s->command.m_32bit     = saved_cmd;
        s->command_arg.m_32bit = saved_arg;

        /* Set IRQ status bit for automatic stop done */
        s->irq_status.m_32bit |= SD_RISR_AUTOCMD_DONE;
    }
}
//----------------------------------------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------------------------------------
void F1C100sMMC0Init(PARMV5TL_CORE core)
{
  core->f1c100s_mmc[0].dma_as = &core->dram[0].m_32bit;
  core->f1c100s_mmc[0].sd = sd_init(NULL, false);
}
//----------------------------------------------------------------------------------------------------------------------------------
//MMC0 control registers
void *F1C100sMMC0(PARMV5TL_CORE core, uint32_t address, uint32_t mode)
{
  //Call the SPI handler with the registers for SPI0
  return(F1C100sMMC(&core->f1c100s_mmc[0], address, mode));
}

//----------------------------------------------------------------------------------------------------------------------------------
//MMC0 control registers read
void F1C100sMMC0Read(PARMV5TL_CORE core, uint32_t address, uint32_t mode)
{
  //Call the SPI read handler with the registers for SPI0
  F1C100sMMCRead(&core->f1c100s_mmc[0], address, mode);
}

//----------------------------------------------------------------------------------------------------------------------------------
//MMC0 control registers write
void F1C100sMMC0Write(PARMV5TL_CORE core, uint32_t address, uint32_t mode)
{
  //Call the SPI write handler with the registers for SPI0
  F1C100sMMCWrite(&core->f1c100s_mmc[0], address, mode);
}

//----------------------------------------------------------------------------------------------------------------------------------
//MMC control registers pointer
void *F1C100sMMC(F1C100S_MMC *registers, uint32_t address, uint32_t mode)
{
  F1C100S_MEMORY *ptr = NULL;
  
  //Select the target register based on word address
  switch(address & 0x00000FFC)
  {
    case SD_GCTL:
      f1c100s_log("MMC_GCTL");
      ptr = &registers->global_ctl;
      break;
    case SD_CKCR:
      f1c100s_log("MMC_CKCR");
      ptr = &registers->clock_ctl;
      break;
    case SD_TMOR:
      f1c100s_log("MMC_TMOR");
      ptr = &registers->timeout;
      break;
    case SD_BWDR:
      f1c100s_log("MMC_BWDR");
      ptr = &registers->bus_width;
      break;
    case SD_BKSR:
      f1c100s_log("MMC_BKSR");
      ptr = &registers->block_size;
      break;
    case SD_BYCR:
      f1c100s_log("MMC_BYCR");
      ptr = &registers->byte_count;
      break;
    case SD_CMDR:
      f1c100s_log("MMC_CMDR");
      ptr = &registers->command;
      break;
    case SD_CARG:
      f1c100s_log("MMC_CARG");
      ptr = &registers->command_arg;
      break;
    case SD_RESP0:
      f1c100s_log("MMC_RESP0");
      ptr = &registers->response[0];
      break;
    case SD_RESP1:
      f1c100s_log("MMC_RESP1");
      ptr = &registers->response[1];
      break;
    case SD_RESP2:
      f1c100s_log("MMC_RESP2");
      ptr = &registers->response[2];
      break;
    case SD_RESP3:
      f1c100s_log("MMC_RESP3");
      ptr = &registers->response[3];
      break;
    case SD_IMKR:
      f1c100s_log("MMC_IMKR");
      ptr = &registers->irq_mask;
      break;
    case SD_MISR:
      f1c100s_log("MMC_MISR");
      ptr = &registers->irq_status;
      break;
    case SD_RISR:
      f1c100s_log("MMC_RISR");
      ptr = &registers->irq_status;
      break;
    case SD_STAR:
      f1c100s_log("MMC_STAR");
      ptr = &registers->status;
      break;
    case SD_FWLR:
      f1c100s_log("MMC_FWLR");
      ptr = &registers->fifo_wlevel;
      break;
    case SD_FUNS:
      f1c100s_log("MMC_FUNS");
      ptr = &registers->fifo_func_sel;
      break;
    case SD_DBGC:
      f1c100s_log("MMC_DBGC");
      ptr = &registers->debug_enable;
      break;
    case SD_A12A:
      f1c100s_log("MMC_A12A");
      ptr = &registers->auto12_arg;
      break;
    case SD_NTSR:
      f1c100s_log("MMC_NTSR");
      ptr = &registers->newtiming_set;
      break;
    case SD_SDBG:
      f1c100s_log("MMC_SDBG");
      ptr = &registers->newtiming_debug;
      break;
    case SD_HWRST:
      f1c100s_log("MMC_HWRST");
      ptr = &registers->hardware_rst;
      break;
    case SD_DMAC:
      f1c100s_log("MMC_DMAC");
      ptr = &registers->dmac;
      break;
    case SD_DLBA:
      f1c100s_log("MMC_DLBA");
      ptr = &registers->desc_base;
      break;
    case SD_IDST:
      f1c100s_log("MMC_IDST");
      ptr = &registers->dmac_status;
      break;
    case SD_IDIE:
      f1c100s_log("MMC_IDIE");
      ptr = &registers->dmac_irq;
      break;
    case SD_THLDC:
      f1c100s_log("MMC_THLDC");
      ptr = &registers->card_threshold;
      break;
    case SD_DSBD:
      f1c100s_log("MMC_DSBD");
      ptr = &registers->startbit_detect;
      break;
    case SD_RES_CRC:
      f1c100s_log("MMC_RES_CRC");
      ptr = &registers->response_crc;
      break;
    case SD_DATA7_CRC:
      f1c100s_log("MMC_DATA7_CRC");
      ptr = &registers->data_crc[7];
      break;
    case SD_DATA6_CRC:
      f1c100s_log("MMC_DATA6_CRC");
      ptr = &registers->data_crc[6];
      break;
    case SD_DATA5_CRC:
      f1c100s_log("MMC_DATA5_CRC");
      ptr = &registers->data_crc[5];
      break;
    case SD_DATA4_CRC:
      f1c100s_log("MMC_DATA4_CRC");
      ptr = &registers->data_crc[4];
      break;
    case SD_DATA3_CRC:
      f1c100s_log("MMC_DATA3_CRC");
      ptr = &registers->data_crc[3];
      break;
    case SD_DATA2_CRC:
      f1c100s_log("MMC_DATA2_CRC");
      ptr = &registers->data_crc[2];
      break;
    case SD_DATA1_CRC:
      f1c100s_log("MMC_DATA1_CRC");
      ptr = &registers->data_crc[1];
      break;
    case SD_DATA0_CRC:
      f1c100s_log("MMC_DATA0_CRC");
      ptr = &registers->data_crc[0];
      break;
    case SD_CRC_STA:
      f1c100s_log("MMC_CRC_STA");
      ptr = &registers->status_crc;
      break;
  }
  
  //Check if valid address has been given
  if(ptr)
  {
    //Return the pointer based on the requested mode
    switch(mode & ARM_MEMORY_MASK)
    {
      case ARM_MEMORY_WORD:
        //Return the word aligned data
        return(&ptr->m_32bit);

      case ARM_MEMORY_SHORT:
        f1c100s_log("[%dH]", address & 2);
        //Return the short aligned data
        return(&ptr->m_16bit[(address & 2) >> 1]);

      case ARM_MEMORY_BYTE:
        f1c100s_log("[%dB]", address & 3);
        //Return the byte aligned data
        return(&ptr->m_8bit[address & 3]);
    }
  }

  return(NULL); 
}

//----------------------------------------------------------------------------------------------------------------------------------
//MMC control registers read
void F1C100sMMCRead(F1C100S_MMC *registers, uint32_t address, uint32_t mode)
{
    //Select the target register based on word address
    switch(address & 0x00000FFC)
    {
    case SD_GCTL:
        f1c100s_log("->(%x)\n", registers->global_ctl.m_32bit);
    	  break;
    case SD_CKCR:
        f1c100s_log("->(%x)\n", registers->clock_ctl.m_32bit);
    	  break;
    case SD_CMDR:
        f1c100s_log("->(%x)\n", registers->command.m_32bit);
    	  break;
    case SD_RESP0:
        f1c100s_log("->(%x)\n", registers->response[0].m_32bit);
    	  break;
    case SD_RESP1:
        f1c100s_log("->(%x)\n", registers->response[1].m_32bit);
    	  break;
    case SD_RESP2:
        f1c100s_log("->(%x)\n", registers->response[2].m_32bit);
    	  break;
    case SD_RESP3:
        f1c100s_log("->(%x)\n", registers->response[3].m_32bit);
    	  break;
    case SD_HWRST:
        f1c100s_log("->(%x)\n", registers->hardware_rst.m_32bit);
        break;
    case SD_RISR:
        f1c100s_log("->(%x)\n", registers->irq_status.m_32bit);
    	  break;
    case SD_IDST: /* Internal DMA Controller Status */
       	f1c100s_log("->(%x)\n", registers->dmac_status.m_32bit);
    	  break;
    default:
      f1c100s_log("->\n");
    }
    f1c100s_log_flush();
}

//----------------------------------------------------------------------------------------------------------------------------------
//MMC control registers write
void F1C100sMMCWrite(F1C100S_MMC *registers, uint32_t address, uint32_t mode)
{
    uint32_t value;
    uint32_t *ptr;
    //Select the target register based on word address
    switch(address & 0x00000FFC)
    {
    case SD_GCTL:
      f1c100s_log("<-(%x)\n", registers->global_ctl.m_32bit);
  	  break;
  	case SD_CKCR:
      f1c100s_log("<-(%x)\n", registers->clock_ctl.m_32bit);
  	  break;
    case SD_BKSR:
     	f1c100s_log("<-(%x)\n", registers->block_size.m_32bit);
  	  break;
    case SD_BYCR:
     	f1c100s_log("<-(%x)\n", registers->byte_count.m_32bit);
  	  break;
  	case SD_CMDR:
  	  value = registers->command.m_32bit;
  	  if (value & SD_CMDR_SEND_INIT_SEQ) {
  	  }
      if (value & SD_CMDR_LOAD) {
        allwinner_sdhost_send_command(registers);
		    allwinner_sdhost_dma(registers);
		    allwinner_sdhost_auto_stop(registers);
        registers->irq = registers->irq_status.m_32bit;
        registers->dma_status = registers->dmac_status.m_32bit;
      } else {
      	f1c100s_log("<-(%x)\n", value);      	
      }
  	  break;
    case SD_CARG:
      f1c100s_log("<-(%x)\n", registers->command_arg.m_32bit);
      break;
  	case SD_RISR:
      f1c100s_log("<-(%x)\n", registers->irq_status.m_32bit);
      registers->irq &= ~(registers->irq_status.m_32bit);
      registers->irq_status.m_32bit = registers->irq;
  	  break;
    case SD_FWLR: /* FIFO Water Level */
      f1c100s_log("<-(%x)\n", registers->fifo_wlevel.m_32bit);
      break;
    case SD_HWRST:
      f1c100s_log("<-(%x)\n", registers->hardware_rst.m_32bit);
      if (registers->hardware_rst.m_32bit)
      	allwinner_sdhost_reset(registers);
      break;
    case SD_DMAC: /* Internal DMA Controller Control */
     	f1c100s_log("<-(%x)\n", registers->dmac.m_32bit);
     	registers->dmac.m_32bit = 0;
  	  break;

    case SD_DLBA: /* Descriptor List Base Address */
      value = registers->desc_base.m_32bit;
     	f1c100s_log("<-(%x)\n", value);
     	if ((value & 0x8E000000) == 0x80000000) {
     	  value &=  ~0x8E000000;
     	  uint32_t *dram_ptr = registers->dma_as;
     	  TransferDescriptor *desc = (TransferDescriptor *)&dram_ptr[value >> 2];
        /* Status flags */
        /* Data buffer size */
        /* Data buffer address */
        /* Physical address of next descriptor */
     	  f1c100s_log("DMA desc status:%08x size:%d addr:%08x next:%08x\n",
     	              desc->status, desc->size, desc->addr, desc->next);
     	}
  	  break;

    case SD_IDST: /* Internal DMA Controller Status */
     	f1c100s_log("<-(%x)\n", registers->dmac_status.m_32bit);
      registers->dma_status &= ~(registers->dmac_status.m_32bit);
      registers->dmac_status.m_32bit = registers->dma_status;
  	  break;

    case SD_IDIE: /* Internal DMA Controller IRQ Enable */
     	f1c100s_log("<-(%x)\n", registers->dmac_irq.m_32bit);
  	  break;

    default:
      f1c100s_log("<-\n");
    }
    f1c100s_log_flush();
}

//----------------------------------------------------------------------------------------------------------------------------------
