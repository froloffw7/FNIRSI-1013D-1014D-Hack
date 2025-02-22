//----------------------------------------------------------------------------------------------------------------------------------

#ifndef F1C100S_STRUCTS_H
#define F1C100S_STRUCTS_H

#include <stdint.h>   // for fixed width integer types
#include <stdio.h>    // for FILE

//----------------------------------------------------------------------------------------------------------------------------------

typedef struct tagF1C100S_PERIPH_STATUS    F1C100S_PERIPH_STATUS;

typedef struct tagF1C100S_CCU              F1C100S_CCU;
typedef struct tagF1C100S_DRAMC            F1C100S_DRAMC;
typedef struct tagF1C100S_TIMER            F1C100S_TIMER;
typedef struct tagF1C100S_INTC             F1C100S_INTC;
typedef struct tagF1C100S_SPI              F1C100S_SPI;
typedef struct tagF1C100S_UART             F1C100S_UART;
typedef struct tagF1C100S_MMC              F1C100S_MMC;
typedef struct tagF1C100S_PIO_PORT         F1C100S_PIO_PORT;
typedef struct tagF1C100S_PIO_INT          F1C100S_PIO_INT;
typedef struct tagF1C100S_PIO_DDR          F1C100S_PIO_DDR;
typedef struct tagF1C100S_TCON             F1C100S_TCON;
typedef struct tagF1C100S_DEBE             F1C100S_DEBE;

//----------------------------------------------------------------------------------------------------------------------------------
//Not F1C100s peripherals but for simplicity implemented here for now

typedef struct tagFLASH_MEMORY             FLASH_MEMORY;

typedef struct tagDISPLAY_MEMORY           DISPLAY_MEMORY;

typedef struct tagTOUCH_PANEL_DATA         TOUCH_PANEL_DATA;

typedef struct tagFPGA_DATA                FPGA_DATA;

//----------------------------------------------------------------------------------------------------------------------------------

typedef union tagF1C100S_MEMORY            F1C100S_MEMORY;

//----------------------------------------------------------------------------------------------------------------------------------

typedef void (*PORTFUNC)(F1C100S_PIO_PORT *registers, uint32_t mode);

//----------------------------------------------------------------------------------------------------------------------------------

union tagF1C100S_MEMORY
{
  uint32_t  m_32bit;
  int32_t   s_32bit;
  uint16_t  m_16bit[2];
  uint8_t   m_8bit[4];
};

//----------------------------------------------------------------------------------------------------------------------------------
//Data for flash memory handling
struct tagFLASH_MEMORY
{
  uint32_t  commandstate;
  uint32_t  mode;
  uint32_t  readaddress;
};

//----------------------------------------------------------------------------------------------------------------------------------
//Data for display handling
struct tagDISPLAY_MEMORY
{
  uint32_t  startaddress;            //Start of the video buffer in the core memory
  uint32_t  linewidth;               //Number of bytes per line
  uint32_t  xsize;                   //Number of x pixels
  uint32_t  ysize;                   //Number of y pixels
  uint64_t  prevcycles;              //CPU cycles count last vertical sync was triggered on
  uint64_t  numcycles;               //Number of CPU cycles needed for a vertical trigger to occur
  uint32_t  linetime;                //Number of cpu cycles for a line
  uint32_t  verticaltime;            //Number of lines for vertical front and back porch
};

//----------------------------------------------------------------------------------------------------------------------------------
//Data for touch panel handling
struct tagTOUCH_PANEL_DATA
{
  uint8_t   i2c_currentbyte;
  uint8_t   i2c_currentbit;
  uint8_t   i2c_state;
  
  uint8_t   panel_state;              //Process state for the panel state machine
  uint8_t   panel_mode;               //Data direction mode for the current data stream
  uint16_t  panel_address;            //Internal address for panel read and write actions
  uint8_t   panel_data[0x200];        //Panel has a lot of registers. For easy implementation set to 0x200 (address range 0x8000 - 0x8200)
  
  uint8_t   prev_port_data;
  
  uint8_t   mouse_down;               //Signal from mouse touch panel that there is touch
  uint8_t   mouse_prev;               //Previous value to detect the change from touch to no touch
};

//----------------------------------------------------------------------------------------------------------------------------------
//Data for FPGA handling
struct tagFPGA_DATA
{
  uint8_t         current_command;
  uint16_t        read_count;
  const uint8_t  *read_ptr;
  uint16_t        write_count;
  uint8_t        *write_ptr;
  uint8_t         prev_ctrl_bits;
  
  //Data for parameter storage system
  uint8_t    param_state;
  uint8_t    param_crypt;
  uint8_t    param_mode;
  uint8_t    param_id;
  uint8_t    param_data[7];
  FILE      *param_file;
  FILE      *param_trace;
  
  uint8_t    cmd0x14count[2];

  uint8_t    cmd0x21data[2];
  
  
  uint8_t    tracedata[1500];
  uint32_t   tracecount;
  uint32_t   datamode;
  
  uint32_t   print_command;
  char       file_name[128];
  uint32_t   file_line_count;
  uint32_t   file_index;
  FILE      *trace_file;
};

//----------------------------------------------------------------------------------------------------------------------------------
//Peripheral reset status info
struct tagF1C100S_PERIPH_STATUS
{
  uint32_t spi0_reset;
  uint32_t spi1_reset;
};

//----------------------------------------------------------------------------------------------------------------------------------
//The clock control register set
struct tagF1C100S_CCU
{
  F1C100S_MEMORY pll_cpu_ctrl;
  F1C100S_MEMORY pll_audio_ctrl;
  F1C100S_MEMORY pll_video_ctrl;
  F1C100S_MEMORY pll_ve_ctrl;
  F1C100S_MEMORY pll_ddr_ctrl;
  F1C100S_MEMORY pll_periph_ctrl;
  F1C100S_MEMORY cpu_clk_src;
  F1C100S_MEMORY ahb_apb_cfg;
  F1C100S_MEMORY bus_clk_gate0;
  F1C100S_MEMORY bus_clk_gate1;
  F1C100S_MEMORY bus_clk_gate2;
  F1C100S_MEMORY sdmmc0_clk;
  F1C100S_MEMORY sdmmc1_clk;
  F1C100S_MEMORY daudio_clk;
  F1C100S_MEMORY spdif_clk;
  F1C100S_MEMORY i2s_clk;
  F1C100S_MEMORY usbphy_cfg;
  F1C100S_MEMORY dram_clk_gate;
  F1C100S_MEMORY debe_clk;
  F1C100S_MEMORY defe_clk;
  F1C100S_MEMORY lcd_clk;
  F1C100S_MEMORY deinterlace_clk;
  F1C100S_MEMORY tve_clk;
  F1C100S_MEMORY tvd_clk;
  F1C100S_MEMORY csi_clk;
  F1C100S_MEMORY ve_clk;
  F1C100S_MEMORY adda_clk;
  F1C100S_MEMORY avs_clk;
  F1C100S_MEMORY pll_stable_time0;
  F1C100S_MEMORY pll_stable_time1;
  F1C100S_MEMORY pll_cpu_bias;
  F1C100S_MEMORY pll_audio_bias;
  F1C100S_MEMORY pll_video_bias;
  F1C100S_MEMORY pll_ve_bias;
  F1C100S_MEMORY pll_ddr0_bias;
  F1C100S_MEMORY pll_periph_bias;
  F1C100S_MEMORY pll_cpu_tun;
  F1C100S_MEMORY pll_ddr_tun;
  F1C100S_MEMORY pll_audio_pat;
  F1C100S_MEMORY pll_video_pat;
  F1C100S_MEMORY pll_ddr0_pat;
  F1C100S_MEMORY bus_soft_rst0;
  F1C100S_MEMORY bus_soft_rst1;
  F1C100S_MEMORY bus_soft_rst2;
};

//----------------------------------------------------------------------------------------------------------------------------------
//The dram controller register set
struct tagF1C100S_DRAMC
{
  F1C100S_MEMORY sconr;
  F1C100S_MEMORY stmg0r;
  F1C100S_MEMORY stmg1r;
  F1C100S_MEMORY sctlr;
  F1C100S_MEMORY srefr;
  F1C100S_MEMORY sextmr;
  F1C100S_MEMORY ddlyr;
  F1C100S_MEMORY dadrr;
  F1C100S_MEMORY dvalr;
  F1C100S_MEMORY drptr0;
  F1C100S_MEMORY drptr1;
  F1C100S_MEMORY drptr2;
  F1C100S_MEMORY drptr3;
  F1C100S_MEMORY sefr;
  F1C100S_MEMORY mae;
  F1C100S_MEMORY aspr;
  F1C100S_MEMORY sdly0;
  F1C100S_MEMORY sdly1;
  F1C100S_MEMORY sdly2;
  F1C100S_MEMORY mcr0;
  F1C100S_MEMORY mcr1;
  F1C100S_MEMORY mcr2;
  F1C100S_MEMORY mcr3;
  F1C100S_MEMORY mcr4;
  F1C100S_MEMORY mcr5;
  F1C100S_MEMORY mcr6;
  F1C100S_MEMORY mcr7;
  F1C100S_MEMORY mcr8;
  F1C100S_MEMORY mcr9;
  F1C100S_MEMORY mcr10;
  F1C100S_MEMORY mcr11;
  F1C100S_MEMORY bwcr;
};

//----------------------------------------------------------------------------------------------------------------------------------
//The interrupt controller registers
struct tagF1C100S_INTC
{
  F1C100S_MEMORY vector;
  F1C100S_MEMORY base_addr;
  F1C100S_MEMORY nmi_int_ctrl;
  F1C100S_MEMORY pend0;
  F1C100S_MEMORY pend1;
  F1C100S_MEMORY en0;
  F1C100S_MEMORY en1;
  F1C100S_MEMORY mask0;
  F1C100S_MEMORY mask1;
  F1C100S_MEMORY resp0;
  F1C100S_MEMORY resp1;
  F1C100S_MEMORY ff0;
  F1C100S_MEMORY ff1;
  F1C100S_MEMORY prio0;
  F1C100S_MEMORY prio1;
  F1C100S_MEMORY prio2;
  F1C100S_MEMORY prio3;
  
  //Internal interrupt status bits
  uint32_t interruptstatus[2];
};

//----------------------------------------------------------------------------------------------------------------------------------
//The timer registers
struct tagF1C100S_TIMER
{
  F1C100S_MEMORY tmr_irq_en;
  F1C100S_MEMORY tmr_irq_sta;
  F1C100S_MEMORY tmr0_ctrl;
  F1C100S_MEMORY tmr0_intv_value;
  F1C100S_MEMORY tmr0_cur_value;
  F1C100S_MEMORY tmr1_ctrl;
  F1C100S_MEMORY tmr1_intv_value;
  F1C100S_MEMORY tmr1_cur_value;
  F1C100S_MEMORY tmr2_ctrl;
  F1C100S_MEMORY tmr2_intv_value;
  F1C100S_MEMORY tmr2_cur_value;
  F1C100S_MEMORY avs_cnt_ctl;
  F1C100S_MEMORY avs_cnt0;
  F1C100S_MEMORY avs_cnt1;
  F1C100S_MEMORY avs_cnt_div;
  F1C100S_MEMORY wdog_irq_en;
  F1C100S_MEMORY wdog_irq_sta;
  F1C100S_MEMORY wdog_ctrl;
  F1C100S_MEMORY wdog_cfg;
  F1C100S_MEMORY wdog_mode;
  
  //Pre scaler counters for the timers
  int32_t prescaler[5];
  int32_t prescalerreload[5];
  
  //Timer control previous values for start detect
  uint32_t ctrl_previous[5];
  
  //Timer interrupt status bits
  uint32_t interruptstatus;
  uint32_t interruptrequest;
};

//----------------------------------------------------------------------------------------------------------------------------------
//The SPI controller register set
struct tagF1C100S_SPI
{
  F1C100S_MEMORY gcr;
  F1C100S_MEMORY tcr;
  F1C100S_MEMORY ier;
  F1C100S_MEMORY isr;
  F1C100S_MEMORY fcr;
  F1C100S_MEMORY fsr;
  F1C100S_MEMORY wcr;
  F1C100S_MEMORY ccr;
  F1C100S_MEMORY mbc;
  F1C100S_MEMORY mtc;
  F1C100S_MEMORY bcc;
  F1C100S_MEMORY txd;
  F1C100S_MEMORY rxd;
  
  //Not directly addressable are the two fifo's each spi interface has
  uint8_t txfifo[64];
  uint8_t rxfifo[64];
};

//----------------------------------------------------------------------------------------------------------------------------------
//The UART controller register set
struct tagF1C100S_UART
{
  //Basic 16550 registers
  F1C100S_MEMORY reg0;     //rbr, thr, dll
  F1C100S_MEMORY reg1;     //ier, dlh
  F1C100S_MEMORY reg2;     //iir, fcr
  F1C100S_MEMORY lcr;
  F1C100S_MEMORY mcr;
  F1C100S_MEMORY lsr;
  F1C100S_MEMORY msr;
  F1C100S_MEMORY sch;
  
  //Additional registers
  F1C100S_MEMORY usr;
  F1C100S_MEMORY tfl;
  F1C100S_MEMORY rfl;
  F1C100S_MEMORY hsk;
  F1C100S_MEMORY halt;
  F1C100S_MEMORY dbg_dll;
  F1C100S_MEMORY dbg_dlh;

  //Internal registers
  uint32_t rbr;
  uint32_t thr;
  uint32_t dll;
  uint32_t dlh;
  uint32_t ier;
  uint32_t iir;
  uint32_t fcr;
  
  //Not directly addressable are the two fifo's each uart interface has
  uint8_t txfifo[64];
  uint8_t rxfifo[64];
};

//Allwinner SD Host Controller registers
struct tagF1C100S_MMC
{
  F1C100S_MEMORY global_ctl;        /**< Global Control */
  F1C100S_MEMORY clock_ctl;         /**< Clock Control */
  F1C100S_MEMORY timeout;           /**< Timeout */
  F1C100S_MEMORY bus_width;         /**< Bus Width */
  F1C100S_MEMORY block_size;        /**< Block Size */
  F1C100S_MEMORY byte_count;        /**< Byte Count */

  F1C100S_MEMORY command;           /**< Command */
  F1C100S_MEMORY command_arg;       /**< Command Argument */
  F1C100S_MEMORY response[4];       /**< Command Response */

  F1C100S_MEMORY irq_mask;          /**< Interrupt Mask */
  F1C100S_MEMORY irq_status;        /**< Raw Interrupt Status */
  F1C100S_MEMORY status;            /**< Status */

  F1C100S_MEMORY fifo_wlevel;       /**< FIFO Water Level */
  F1C100S_MEMORY fifo_func_sel;     /**< FIFO Function Select */
  F1C100S_MEMORY debug_enable;      /**< Debug Enable */
  F1C100S_MEMORY auto12_arg;        /**< Auto Command 12 Argument */
  F1C100S_MEMORY newtiming_set;     /**< SD New Timing Set */
  F1C100S_MEMORY newtiming_debug;   /**< SD New Timing Debug */
  F1C100S_MEMORY hardware_rst;      /**< Hardware Reset */
  F1C100S_MEMORY dmac;              /**< Internal DMA Controller Control */
  F1C100S_MEMORY desc_base;         /**< Descriptor List Base Address */
  F1C100S_MEMORY dmac_status;       /**< Internal DMA Controller Status */
  F1C100S_MEMORY dmac_irq;          /**< Internal DMA Controller IRQ Enable */
  F1C100S_MEMORY card_threshold;    /**< Card Threshold Control */
  F1C100S_MEMORY startbit_detect;   /**< eMMC DDR Start Bit Detection Control */
  F1C100S_MEMORY response_crc;      /**< Response CRC */
  F1C100S_MEMORY data_crc[8];       /**< Data CRC */
  F1C100S_MEMORY status_crc;        /**< Status CRC */
  F1C100S_MEMORY fifo;              /**< Read/Write FIFO */

  //Internal registers
  uint32_t       irq_bits;
  uint32_t       transfer_cnt;
  //
  uint32_t       *dma_as;
  uint32_t       dma_irq_bits;
  struct SDState *sd;
};

//----------------------------------------------------------------------------------------------------------------------------------
//The PIO registers
struct tagF1C100S_PIO_PORT
{
  F1C100S_MEMORY cfg0;
  F1C100S_MEMORY cfg1;
  F1C100S_MEMORY cfg2;
  F1C100S_MEMORY cfg3;
  F1C100S_MEMORY data;
  F1C100S_MEMORY drv0;
  F1C100S_MEMORY drv1;
  F1C100S_MEMORY pul0;
  F1C100S_MEMORY pul1;
  
  //For handling data read or written to the port data register
  PORTFUNC  porthandler;
  void     *portdata;
};

struct tagF1C100S_PIO_INT
{
  F1C100S_MEMORY cfg0;
  F1C100S_MEMORY cfg1;
  F1C100S_MEMORY cfg2;
  F1C100S_MEMORY cfg3;
  F1C100S_MEMORY ctrl;
  F1C100S_MEMORY sta;
  F1C100S_MEMORY deb;
};

struct tagF1C100S_PIO_DDR
{
  F1C100S_MEMORY paddrv;
  F1C100S_MEMORY padpul;
};

//----------------------------------------------------------------------------------------------------------------------------------
//LCD timing control registers
struct tagF1C100S_TCON
{
  F1C100S_MEMORY ctrl;
  F1C100S_MEMORY int0;
  F1C100S_MEMORY int1;
  F1C100S_MEMORY frm_ctrl;
  F1C100S_MEMORY frm_seed0_r;
  F1C100S_MEMORY frm_seed0_g;
  F1C100S_MEMORY frm_seed0_b;
  F1C100S_MEMORY frm_seed1_r;
  F1C100S_MEMORY frm_seed1_g;
  F1C100S_MEMORY frm_seed1_b;
  F1C100S_MEMORY frm_tbl0;
  F1C100S_MEMORY frm_tbl1;
  F1C100S_MEMORY frm_tbl2;
  F1C100S_MEMORY frm_tbl3;
  F1C100S_MEMORY clk_ctrl;
  F1C100S_MEMORY debug_info;
  F1C100S_MEMORY tcon0_ctrl;
  F1C100S_MEMORY tcon0_basic_timing0;
  F1C100S_MEMORY tcon0_basic_timing1;
  F1C100S_MEMORY tcon0_basic_timing2;
  F1C100S_MEMORY tcon0_basic_timing3;
  F1C100S_MEMORY tcon0_hv_timing;
  F1C100S_MEMORY tcon0_cpu_if;
  F1C100S_MEMORY tcon0_cpu_wr;
  F1C100S_MEMORY tcon0_cpu_rd;
  F1C100S_MEMORY tcon0_cpu_rd_nx;
  F1C100S_MEMORY tcon0_io_ctrl0;
  F1C100S_MEMORY tcon0_io_ctrl1;
  F1C100S_MEMORY tcon1_ctrl;
  F1C100S_MEMORY tcon1_basic_timing0;
  F1C100S_MEMORY tcon1_basic_timing1;
  F1C100S_MEMORY tcon1_basic_timing2;
  F1C100S_MEMORY tcon1_basic_timing3;
  F1C100S_MEMORY tcon1_basic_timing4;
  F1C100S_MEMORY tcon1_basic_timing5;
  F1C100S_MEMORY tcon1_io_ctrl0;
  F1C100S_MEMORY tcon1_io_ctrl1;
};

//----------------------------------------------------------------------------------------------------------------------------------
//Display engine back end registers
struct tagF1C100S_DEBE
{
  F1C100S_MEMORY mode_ctrl;
  F1C100S_MEMORY color_ctrl;
  F1C100S_MEMORY lay_size;
  F1C100S_MEMORY lay0_size;
  F1C100S_MEMORY lay1_size;
  F1C100S_MEMORY lay2_size;
  F1C100S_MEMORY lay3_size;
  F1C100S_MEMORY lay0_codnt;
  F1C100S_MEMORY lay1_codnt;
  F1C100S_MEMORY lay2_codnt;
  F1C100S_MEMORY lay3_codnt;
  F1C100S_MEMORY lay0_linewidth;
  F1C100S_MEMORY lay1_linewidth;
  F1C100S_MEMORY lay2_linewidth;
  F1C100S_MEMORY lay3_linewidth;
  F1C100S_MEMORY lay0_fb_addr1;
  F1C100S_MEMORY lay1_fb_addr1;
  F1C100S_MEMORY lay2_fb_addr1;
  F1C100S_MEMORY lay3_fb_addr1;
  F1C100S_MEMORY lay0_fb_addr2;
  F1C100S_MEMORY lay1_fb_addr2;
  F1C100S_MEMORY lay2_fb_addr2;
  F1C100S_MEMORY lay3_fb_addr2;
  F1C100S_MEMORY regbuff_ctrl;
  F1C100S_MEMORY ck_max;
  F1C100S_MEMORY ck_min;
  F1C100S_MEMORY ck_cfg;
  F1C100S_MEMORY lay0_att_ctrl0;
  F1C100S_MEMORY lay1_att_ctrl0;
  F1C100S_MEMORY lay2_att_ctrl0;
  F1C100S_MEMORY lay3_att_ctrl0;
  F1C100S_MEMORY lay0_att_ctrl1;
  F1C100S_MEMORY lay1_att_ctrl1;
  F1C100S_MEMORY lay2_att_ctrl1;
  F1C100S_MEMORY lay3_att_ctrl1;
  F1C100S_MEMORY hwc_ctrl;
  F1C100S_MEMORY hwcfb_ctrl;
  F1C100S_MEMORY wb_ctrl;
  F1C100S_MEMORY wb_addr;
  F1C100S_MEMORY wb_lw;
  F1C100S_MEMORY iyuv_ch_ctrl;
  F1C100S_MEMORY ch0_yuv_fb_addr;
  F1C100S_MEMORY ch1_yuv_fb_addr;
  F1C100S_MEMORY ch2_yuv_fb_addr;
  F1C100S_MEMORY ch0_yuv_blw;
  F1C100S_MEMORY ch1_yuv_blw;
  F1C100S_MEMORY ch2_yuv_blw;
  F1C100S_MEMORY coef00;
  F1C100S_MEMORY coef01;
  F1C100S_MEMORY coef02;
  F1C100S_MEMORY coef03;
  F1C100S_MEMORY coef10;
  F1C100S_MEMORY coef11;
  F1C100S_MEMORY coef12;
  F1C100S_MEMORY coef13;
  F1C100S_MEMORY coef20;
  F1C100S_MEMORY coef21;
  F1C100S_MEMORY coef22;
  F1C100S_MEMORY coef23;
};


//----------------------------------------------------------------------------------------------------------------------------------

#endif /* F1C100S_STRUCTS_H */

