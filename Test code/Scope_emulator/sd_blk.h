bool blk_is_writable(FILE *blk);
bool blk_is_inserted(FILE *blk);
void blk_get_geometry(FILE *blk, uint64_t *sect);
int32_t blk_pread(FILE *blk, uint64_t addr, uint8_t *data, uint32_t len);
int32_t blk_pwrite(FILE *blk, uint64_t addr, uint8_t *data, uint32_t len, int u);


static inline bool blk_supports_write_perm(FILE *blk) { return true; }
static inline int64_t blk_getlength(FILE *blk) { return 4 * GiB; }
