void trace_sdcard_set_voltage(uint16_t millivolts);
void trace_sdcard_inserted();
void trace_sdcard_ejected();
void trace_sdcard_reset();
void trace_sdcard_powerup();
void trace_sdcard_set_blocklen(uint32_t arg);
void trace_sdcard_response(const char *sd_response_name, int rsplen);
void trace_sdcard_normal_command(const char *proto_name,
                                 const char *sd_cmd_name, uint8_t cmd,
                                 uint32_t arg, const char *sd_state_name);
void trace_sdcard_app_command(const char *proto_name,
                                 const char *sd_cmd_name, uint8_t cmd,
                                 uint32_t arg, const char *sd_state_name);
void trace_sdcard_erase(uint64_t erase_start, uint64_t erase_end);
void trace_sdcard_inquiry_cmd41();
void trace_sdcard_lock();
void trace_sdcard_unlock();
void trace_allwinner_sdhost_update_irq(uint32_t irq, uint32_t dma_irq);
void trace_allwinner_sdhost_process_desc(uint32_t desc_addr, int size,
                                         bool is_write, int max_bytes);
void trace_sdcard_read_data(const char *proto_name,
                            const char *sd_acmd_name,
                            uint8_t cmd, int io_len);
void trace_sdcard_write_data(const char *proto_name,
                             const char *sd_acmd_name,
                             uint8_t cmd, uint8_t value);
void trace_sdcard_read_block(uint64_t addr, uint32_t len);
void trace_sdcard_write_block(uint64_t addr, uint32_t len);
