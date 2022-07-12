#include <stdio.h>

#include "qemu_defs.h"
#include "sd_blk.h"
#include "f1c100s_log.h"


bool blk_is_writable(FILE *blk) {
	if (blk) return true;
	return false;
}

bool blk_is_inserted(FILE *blk) {
	if (blk) return true;
	return false;
}

void blk_get_geometry(FILE *blk, uint64_t *sect) {
	if (!blk) {
		*sect = 0;
		return;
	}
	int64_t ofs;
	ofs = ftell(blk);
	fseek(blk, 0, SEEK_END);
	*sect = ftell(blk) >> 9;
	fseek(blk, ofs, SEEK_SET);
	f1c100s_log_mask(LOG_GUEST_TRACE, "blk_get_geometry: %lld\n", *sect);
}

int32_t blk_pread(FILE *blk, uint64_t addr, uint8_t *data, uint32_t len) {
	if (!blk) return -1;
	f1c100s_log_mask(LOG_GUEST_TRACE, "blk_read: %llx[%d]\n", addr, len);
	fseek(blk, addr, SEEK_SET);
	fread(data, 1, len, blk);
	return len;
}
int32_t blk_pwrite(FILE *blk, uint64_t addr, uint8_t *data, uint32_t len, int u) {
	if (!blk) return -1;
	f1c100s_log_mask(LOG_GUEST_TRACE, "blk_write: %llx[%d]\n", addr, len);
	fseek(blk, addr, SEEK_SET);
	fwrite(data, 1, len, blk);
	return len;
}
