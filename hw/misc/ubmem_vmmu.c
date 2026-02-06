#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/arm/virt.h"
#include "hw/qdev-properties.h"
#include "hw/platform-bus.h"
#include "sysemu/device_tree.h"
#include "exec/address-spaces.h"
#include "block/thread-pool.h"
#include "hw/misc/ubmem_vmmu.h"

typedef struct UbmemVMMUState UbmemVMMUState;
DECLARE_INSTANCE_CHECKER(UbmemVMMUState, UBMEM_VMMU, TYPE_UBMEM_VMMU)

static void ubmem_vmmu_fill_result(UbmemVMMUState *s,
                        uint64_t slot_index, int ret)
{
    s->result_slots[slot_index] = (uint64_t)ret << UBMEM_VMMU_RESULT_SHIFT;
    s->result_slots[slot_index] |= UBMEM_VMMU_DONE;
}

static uint64_t ubmem_vmmu_read(void *opaque, hwaddr addr, unsigned size)
{
    UbmemVMMUState *s = opaque;
    return s->num_slots;
}

static int worker_cb(void *opaque)
{
    int ret;
    struct UbmReqData *data = opaque;
    UbmemVMMUState *s = data->vmmu;
    void *host_addr;
    MemoryRegionSection section;
    hwaddr offset;
    MemoryRegion *mr;
    uint64_t gpa, remaining;
    size_t gap_count = data->gap_count;
    size_t req_len;
    size_t cur_areas = 0;

    if (s->ubmemp_fd < 0) {
        qemu_log("ubmem vmmu: invalid ubmemp fd\n");
        ret = UBMEM_VMMU_ERR;
        goto out;
    }

    for (size_t i = 0; i < data->ubm_req.areas_num; i++) {
        gpa = data->ubm_req.areas[i + gap_count].addr;
        remaining = data->ubm_req.areas[i + gap_count].size;
        if (gpa == 0) {
            data->ubm_req.areas[cur_areas].addr = 0;
            data->ubm_req.areas[cur_areas].size = remaining;
            cur_areas++;
            continue;
        }
        while (remaining > 0) {
            section = memory_region_find(get_system_memory(), gpa, remaining);
            if (section.mr == NULL) {
                qemu_log("ubmem vmmu: gpa not mapped: 0x%" PRIx64 "\n", gpa);
                ret = UBMEM_VMMU_ERR;
                goto out;
            }
            section.size = MIN(remaining, section.size);
            mr = section.mr;
            offset = section.offset_within_region;
            host_addr = memory_region_get_ram_ptr(mr) + offset;
            memory_region_unref(section.mr);
            data->ubm_req.areas[cur_areas].addr = (uint64_t)host_addr;
            data->ubm_req.areas[cur_areas].size = section.size;
            remaining -= section.size;
            gpa += section.size;
            cur_areas++;
        }
    }

    req_len = sizeof(struct UbmReq) + cur_areas * sizeof(struct UbmArea);
    data->ubm_req.areas_num = cur_areas;
    ret = write(s->ubmemp_fd, &data->ubm_req, req_len);
    if (ret < 0 || (size_t)ret != req_len) {
        qemu_log("ubmem vmmu: write to ubmemp failed, ret=%d, expected=%zu\n",
                    ret, req_len);
        /*
         * Directly pass through the error code from write()
         * to upper layer. Since the write() returns -1 on error,
         * the guest will receive the actual return value.
         */
    } else {
        ret = UBMEM_VMMU_SUCCEED;
    }
out:
    ubmem_vmmu_fill_result(s, data->slot_index, ret);
    g_free(data);
    return 0;
}

/* Check unsigned addition for overflow: returns true if overflow */
static inline bool check_uadd_overflow(uint64_t a, uint64_t b, uint64_t *res)
{
    *res = a + b;
    return *res < a;
}

static void ubmem_vmmu_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    UbmemVMMUState *s = opaque;
    struct UbmReqData *req_data;
    struct UbmReq *ubm_req;
    uint64_t avail_areas, uba_end;

    if (val >= s->num_slots) {
        qemu_log("ubmem vmmu: invalid slot index %" PRIu64 "\n", val);
        return;
    }

    s->result_slots[val] = 0;
    ubm_req = (struct UbmReq *)s->request_ring;

    if (ubm_req->tid != s->tid) {
        qemu_log("ubmem vmmu: invalid tid in request\n");
        ubmem_vmmu_fill_result(s, val, UBMEM_VMMU_ERR);
        return;
    }

    if (ubm_req->uba < s->uba) {
        qemu_log("ubmem vmmu: request uba below allowed range\n");
        ubmem_vmmu_fill_result(s, val, UBMEM_VMMU_ERR);
        return;
    }

    if (check_uadd_overflow(ubm_req->uba, ubm_req->size, &uba_end) ||
        uba_end > (s->uba + s->size)) {
        qemu_log("ubmem vmmu: request out of bounds\n");
        ubmem_vmmu_fill_result(s, val, UBMEM_VMMU_ERR);
        return;
    }

    if (ubm_req->size == 0 ||
        (ubm_req->size & (UBMEM_VMMU_PAGE_SIZE - 1)) != 0) {
        qemu_log("ubmem vmmu: invalid size\n");
        ubmem_vmmu_fill_result(s, val, UBMEM_VMMU_ERR);
        return;
    }

    avail_areas = ubm_req->size / UBMEM_VMMU_PAGE_SIZE;
    if (ubm_req->areas_num > avail_areas) {
        qemu_log("ubmem vmmu: too many areas in request\n");
        ubmem_vmmu_fill_result(s, val, UBMEM_VMMU_ERR);
        return;
    }

    /* Limit areas_num to prevent memory exhaustion */
    if (ubm_req->areas_num > UBMEM_VMMU_MAX_AREAS) {
        qemu_log("ubmem vmmu: areas_num exceeds maximum\n");
        ubmem_vmmu_fill_result(s, val, UBMEM_VMMU_ERR);
        return;
    }

    req_data = g_try_malloc(sizeof(struct UbmReqData) +
                            avail_areas * sizeof(struct UbmArea));
    if (!req_data) {
        qemu_log("ubmem vmmu: failed to allocate memory\n");
        ubmem_vmmu_fill_result(s, val, UBMEM_VMMU_ERR);
        return;
    }

    req_data->vmmu = s;
    req_data->gap_count = avail_areas - ubm_req->areas_num;
    req_data->slot_index = val;
    req_data->max_areas = avail_areas;
    req_data->ubm_req = *ubm_req;
    memcpy(req_data->ubm_req.areas + req_data->gap_count,
           ubm_req->areas, ubm_req->areas_num * sizeof(struct UbmArea));

    thread_pool_submit_aio(worker_cb, req_data, NULL, NULL);
}

static const MemoryRegionOps ubmem_vmmu_ops = {
    .read = ubmem_vmmu_read,
    .write = ubmem_vmmu_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 8,
        .max_access_size = 8,
    },
};

static Property ubmem_vmmu_properties[] = {
    DEFINE_PROP_UINT32("tid", UbmemVMMUState, tid, 0),
    DEFINE_PROP_UINT64("uba", UbmemVMMUState, uba, 0),
    DEFINE_PROP_UINT64("size", UbmemVMMUState, size, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void add_ubmem_vmmu_fdt_node(VirtMachineState *vms)
{
    char *node_name;
    const char compat_str[] = "custom,ubmem_vmmu";
    MachineState *ms = MACHINE(vms);

    node_name = g_strdup_printf("/ubmem_vmmu@%" PRIx64,
                               vms->memmap[VIRT_UBMEM_VMMU_REG].base);
    qemu_fdt_add_subnode(ms->fdt, node_name);
    qemu_fdt_setprop_string(ms->fdt, node_name, "compatible", compat_str);
    qemu_fdt_setprop_sized_cells(ms->fdt, node_name, "reg",
                                 2, vms->memmap[VIRT_UBMEM_VMMU_REG].base,
                                 2, vms->memmap[VIRT_UBMEM_VMMU_REG].size,
                                 2, vms->memmap[VIRT_UBMEM_VMMU_MEM].base,
                                 2, vms->memmap[VIRT_UBMEM_VMMU_MEM].size);
    g_free(node_name);
}

static void ubmem_vmmu_hold_reset(Object *obj)
{
    UbmemVMMUState *s = UBMEM_VMMU(obj);

    if (s->ubmemp_fd >= 0) {
        close(s->ubmemp_fd);
        qemu_log("ubmem vmmu: closed ubmem fd on reset\n");
        s->ubmemp_fd = -1;
    }

    s->ubmemp_fd = open(UBMEM_VMMU_DEV_PATH, O_RDWR);
    if (s->ubmemp_fd < 0) {
        qemu_log("ubmem vmmu: failed to reopen %s on reset\n",
                 UBMEM_VMMU_DEV_PATH);
        exit(1);
    }
    qemu_log("ubmem vmmu: reopened ubmemp fd on reset\n");
}

static void ubmem_vmmu_realize(DeviceState *dev, Error **errp)
{
    UbmemVMMUState *s = UBMEM_VMMU(dev);
    VirtMachineState *vms = VIRT_MACHINE(qdev_get_machine());
    SysBusDevice *sbdev = SYS_BUS_DEVICE(dev);

    if (!vms->ubmem_vmmu_mem || !vms->ubmem_vmmu_reg) {
        qemu_log("ubmem vmmu: both reg and mem windows must be enabled\n");
        exit(1);
    }

    if (vms->ubmem_vmmu_realized) {
        qemu_log("ubmem vmmu: only one ubmem vmmu device is supported\n");
        exit(1);
    }

    if (s->tid == 0 || s->tid > UBMEM_VMMU_MAX_TID) {
        qemu_log("ubmem vmmu: invalid tid %u", s->tid);
        exit(1);
    }

    if ((s->uba & (UBMEM_VMMU_PAGE_SIZE - 1)) != 0 ||
        (s->size & (UBMEM_VMMU_PAGE_SIZE - 1)) != 0 || s->size == 0) {
        qemu_log("ubmem vmmu: invalid uba base or size\n");
        exit(1);
    }

    if (s->uba + s->size < s->uba) {
        qemu_log("ubmem vmmu: uba base + size overflow\n");
        exit(1);
    }

    s->num_slots = cpu_list_generation_id_get();
    s->num_slots = MIN(s->num_slots, UBMEM_VMMU_MAX_SLOTS);
    s->num_slots = MAX(s->num_slots, 1);

    add_ubmem_vmmu_fdt_node(vms);

    memory_region_init_io(&s->mmio_region, OBJECT(s), &ubmem_vmmu_ops,
                             s, "ubmem_vmmu.mmio",
                             vms->memmap[VIRT_UBMEM_VMMU_REG].size);
    memory_region_init_ram(&s->mem_region, OBJECT(s), "ubmem_vmmu.mem",
                           vms->memmap[VIRT_UBMEM_VMMU_MEM].size, errp);
    sysbus_init_mmio(sbdev, &s->mmio_region);
    memory_region_add_subregion(get_system_memory(),
                                 vms->memmap[VIRT_UBMEM_VMMU_REG].base,
                                 &s->mmio_region);
    memory_region_add_subregion(get_system_memory(),
                                 vms->memmap[VIRT_UBMEM_VMMU_MEM].base,
                                 &s->mem_region);

    /*
     * Layout of shared memory:
     * [num_slots (8 bytes)][result slots (num_slots * 8 bytes)]
     * [request ring (rest of the memory)]
     */
    s->request_ring = memory_region_get_ram_ptr(&s->mem_region);
    *(uint64_t *)s->request_ring = s->num_slots;
    s->result_slots = (uint64_t *)s->request_ring + 1;
    s->request_ring = s->result_slots + s->num_slots;

    s->ubmemp_fd = open(UBMEM_VMMU_DEV_PATH, O_RDWR);
    if (s->ubmemp_fd < 0) {
        qemu_log("ubmem vmmu: failed to open %s\n", UBMEM_VMMU_DEV_PATH);
        exit(1);
    }

    vms->ubmem_vmmu_realized = true;
}

static void ubmem_vmmu_class_init(ObjectClass *klass, void *data)
{
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    DeviceClass *k = DEVICE_CLASS(klass);
    k->realize = ubmem_vmmu_realize;
    device_class_set_props(k, ubmem_vmmu_properties);
    k->desc = "Ubmem VMMU Device";
    rc->phases.hold = ubmem_vmmu_hold_reset;
    k->user_creatable = true;
}

static const TypeInfo ubmem_vmmu_info = {
    .name          = TYPE_UBMEM_VMMU,
    .parent        = TYPE_PLATFORM_BUS_DEVICE,
    .instance_size = sizeof(UbmemVMMUState),
    .class_init    = ubmem_vmmu_class_init,
};

static void ubmem_vmmu_register_types(void)
{
    type_register_static(&ubmem_vmmu_info);
}
type_init(ubmem_vmmu_register_types);
