#include "qemu_defs.h"
#include "f1c100s_log.h"

void trace_sdcard_set_voltage(uint16_t millivolts) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Set card voltage: %d mV\n", millivolts);
}

void trace_sdcard_inserted() {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card inserted\n");
}

void trace_sdcard_ejected() {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card ejected\n");
}

void trace_sdcard_reset() {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card reset\n");
}

void trace_sdcard_powerup(){
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card powerup\n");
}

void trace_sdcard_set_blocklen(uint32_t arg) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Set card block len: %d\n", arg);
}

void trace_sdcard_response(const char *sd_response_name, int rsplen) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card response %s[%d]\n", sd_response_name, rsplen); 
}

void trace_sdcard_normal_command(const char *proto_name,
                                 const char *sd_cmd_name, uint8_t cmd,
                                 uint32_t arg, const char *sd_state_name) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card normal cmd %s:%s(%s)\n", proto_name, sd_cmd_name, sd_state_name);
}

void trace_sdcard_app_command(const char *proto_name,
                                 const char *sd_cmd_name, uint8_t cmd,
                                 uint32_t arg, const char *sd_state_name) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card application cmd %s:%s(%s)\n", proto_name, sd_cmd_name, sd_state_name);

}

void trace_sdcard_erase(uint64_t erase_start, uint64_t erase_end) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card erase %llx..%llx\n", erase_start, erase_end);
}

void trace_sdcard_inquiry_cmd41() {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card inquiry CMD41\n");
}

void trace_sdcard_lock() {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card lock\n");
}

void trace_sdcard_unlock() {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card unlock\n");

}

void trace_allwinner_sdhost_update_irq(uint32_t irq, uint32_t dma_irq) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "MMC IRQ/DMA status: %08x/%08x\n", irq, dma_irq);
    
}

void trace_allwinner_sdhost_process_desc(uint32_t desc_addr, int size,
                                         bool is_write, int max_bytes) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "DMA process descriptor %08x[%d]%s(%d)\n",
                     desc_addr, size, is_write?"<-":"->", max_bytes);
}

void trace_sdcard_read_data(const char *proto_name,
                            const char *sd_acmd_name,
                            uint8_t cmd, int io_len) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card read data %s:%s(%d) [%d]\n", proto_name, sd_acmd_name, cmd, io_len);
}

void trace_sdcard_write_data(const char *proto_name,
                             const char *sd_acmd_name,
                             uint8_t cmd, uint8_t value) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card write data %s:%s [%02x]\n", proto_name, sd_acmd_name, value);
}

void trace_sdcard_read_block(uint64_t addr, uint32_t len) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card read block %llx[%d]\n", addr, len);
}

void trace_sdcard_write_block(uint64_t addr, uint32_t len) {
    f1c100s_log_mask(LOG_GUEST_TRACE, "Card write block %llx[%d]\n", addr, len);
}
