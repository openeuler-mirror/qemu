#ifndef UBMEM_VMMU_H
#define UBMEM_VMMU_H

#include "hw/sysbus.h"

#define TYPE_UBMEM_VMMU "ubmem_vmmu"
#define UBMEM_VMMU_REG_SIZE 0x1000
#define UBMEM_VMMU_MEM_SIZE 0x1000000
#define UBMEM_VMMU_PAGE_SIZE 0x1000

#define UBMEM_VMMU_DEV_PATH "/dev/ubmempfd"
#define UBMEM_VMMU_MAX_TID (64)
#define UBMEM_VMMU_ERR (-1)
#define UBMEM_VMMU_SUCCEED (0)
#define UBMEM_VMMU_DONE (1)
#define UBMEM_VMMU_RESULT_SHIFT (32)

#define UBMEM_VMMU_MAX_SLOTS 0x80
/* Maximum areas to prevent memory exhaustion */
#define UBMEM_VMMU_MAX_AREAS ((UBMEM_VMMU_MEM_SIZE - \
            (sizeof(uint64_t) * (1 + UBMEM_VMMU_MAX_SLOTS)) - \
            sizeof(struct UbmReq)) / \
            sizeof(struct UbmArea))

struct UbmemVMMUState {
    SysBusDevice parent;
    MemoryRegion mmio_region;
    MemoryRegion mem_region;
    void *request_ring;
    uint64_t *result_slots;
    uint64_t num_slots;
    int ubmemp_fd;
    uint32_t tid;
    uint64_t uba;
    uint64_t size;
};

struct UbmArea {
    uint64_t addr;
    uint64_t size;
} __attribute__((packed));

struct UbmReq {
    uint32_t opcode;
    uint32_t tid;
    uint64_t uba;
    uint64_t size;
    uint64_t areas_num;
    struct UbmArea areas[];
} __attribute__((packed));

struct UbmReqData {
    uint64_t slot_index;
    uint64_t gap_count;
    uint64_t max_areas;  /* Bounds for worker, set at entry */
    struct UbmemVMMUState *vmmu;
    struct UbmReq ubm_req;
} __attribute__((packed));

#endif /* UBMEM_VMMU_H */
