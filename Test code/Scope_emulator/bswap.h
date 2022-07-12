#define ldl_be_p(p) __builtin_bswap32(*(uint32_t*)p)


#define stw_be_p(p, v)  *(uint16_t*)(p) = __builtin_bswap16(v)
#define stl_be_p(p, v)  *(uint32_t*)(p) = __builtin_bswap32(v)
