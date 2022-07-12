#include <stdint.h>
#include <assert.h>

#include "qemu/registerfields.h"
#include "bswap.h"

typedef int  bool;
#define false	0
#define true	1

#define OBJECT_DECLARE_TYPE(STRUCT, CLASS, BUS) \
typedef struct STRUCT STRUCT; \

#define ARRAY_SIZE(a)     (sizeof(a) / sizeof(*(a)))

#define KiB               (1024LL)
#define MiB               (1024*1024LL)
#define GiB               (1024*1024*1024LL)


//typedef struct DeviceState DeviceState;
//typedef struct BlockBackend BlockBackend;


typedef bool qemu_irq;
static inline void qemu_set_irq(qemu_irq irq, bool cond) {}