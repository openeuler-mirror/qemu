/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include CONFIG_DEVICES /* CONFIG_IOMMUFD */
#include "qemu/range.h"
#include <linux/vfio.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "qemu/module.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_common.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_config.h"
#include "hw/ub/ub_acpi.h"
#include "hw/ub/ub_usi.h"
#include "hw/ub/ub_msg.h"
#include "hw/ub/ubus_instance.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "ub.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "exec/address-spaces.h"
#include "sysemu/iommufd.h"
#include "trace.h"

static KVMRouteChange vfio_route_change;

static Property vfio_ub_dev_properties[] = {
    DEFINE_PROP_UB_HOST_DEVADDR("host", VFIOUBDevice, host),
#ifdef CONFIG_IOMMUFD
    DEFINE_PROP_LINK("iommufd", VFIOUBDevice, vbasedev.iommufd,
                     TYPE_IOMMUFD_BACKEND, IOMMUFDBackend *),
#endif
    DEFINE_PROP_END_OF_LIST(),
};
static void vfio_disable_interrupts(VFIOUBDevice *vdev);
static void vfio_usi_disable(VFIOUBDevice *vdev);
static void vfio_ub_write_config(UBDevice *dev, uint64_t offset,
                                 uint32_t *val, uint32_t dw_mask);

static bool vfio_ub_needed(void *opaque)
{
    return 0;
}

static const VMStateDescription vfio_ub_vmstate = {
    .name = TYPE_VFIO_UB,
    .unmigratable = 1,
    .version_id = 0,
    .minimum_version_id = 0,
    .needed = vfio_ub_needed,
    .fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static void vfio_ub_reset(DeviceState *dev)
{
    VFIOUBDevice *vdev = VFIO_UB(dev);
    uint32_t val = 0;

    vfio_disable_interrupts(vdev);

    if (!ioctl(vdev->vbasedev.fd, VFIO_DEVICE_RESET)) {
        qemu_log("%s execute ELR done\n", dev->id);
    } else {
        qemu_log("%s execute ELR failed\n", dev->id);
    }

    /* need after ELR */
    vfio_ub_write_config(&vdev->udev, UB_CFG1_DEV_TOKEN_ID_OFFSET,
                         &val, UB_TOKEN_ID_MASK);
    vfio_ub_write_config(&vdev->udev, UB_CFG1_DEV_RS_ACCESS_EN_OFFSET,
                         &val, UB_DEV_RS_ACCESS_EN_MASK);
    vfio_ub_write_config(&vdev->udev, UB_CFG1_BUS_ACCESS_EN_OFFSET,
                         &val, UB_BUS_ACCESS_EN_MASK);
    qemu_log("ub device(%s %s) clear 'dev_token_id',"
             "'bus_access_en' and 'dev_rs_access_en'\n",
             vdev->udev.name, vdev->udev.qdev.id);
}

static void vfio_ub_compute_needs_reset(VFIODevice *vbasedev)
{
    if (!vbasedev->reset_works) {
        vbasedev->needs_reset = true;
    }
}

static int vfio_ub_hot_reset_multi(VFIODevice *vbasedev)
{
    /* do nothing now */
    return 0;
}

static void vfio_ub_eoi(VFIODevice *vbasedev)
{
    /* do nothing now */
}

static Object *vfio_ub_get_object(VFIODevice *vbasedev)
{
    VFIOUBDevice *vdev = container_of(vbasedev, VFIOUBDevice, vbasedev);

    return OBJECT(vdev);
}


static void vfio_ub_save_config(VFIODevice *vbasedev, QEMUFile *f)
{
    /* do nothing now */
}

static int vfio_ub_load_config(VFIODevice *vbasedev, QEMUFile *f)
{
    /* do nothing now */
    return 0;
}

static VFIODeviceOps vfio_ub_ops = {
    .vfio_compute_needs_reset = vfio_ub_compute_needs_reset,
    .vfio_hot_reset_multi = vfio_ub_hot_reset_multi,
    .vfio_eoi = vfio_ub_eoi,
    .vfio_get_object = vfio_ub_get_object,
    .vfio_save_config = vfio_ub_save_config,
    .vfio_load_config = vfio_ub_load_config,
};

static void vfio_populate_device(VFIOUBDevice *vdev, Error **errp)
{
    VFIODevice *vbasedev = &vdev->vbasedev;
    struct vfio_region_info *reg_info;
    struct vfio_irq_info irq_info = { .argsz = sizeof(irq_info) };
    int i;
    int ret = -1;

    if (vbasedev->num_regions < VFIO_UB_NUM_REGIONS) {
        error_setg(errp, "unexpected number of io regions %u",
                   vbasedev->num_regions);
        return;
    }
    qemu_log("vbasedev->num_irqs %u vbasedev->num_regions %u\n",
             vbasedev->num_irqs, vbasedev->num_regions);

    for (i = VFIO_UB_REGION0_INDEX; i < VFIO_UB_CONFIG_REGION_INDEX; i++) {
        char *name = g_strdup_printf("%s-ERS%d", vbasedev->name, i);

        ret = vfio_region_setup(OBJECT(vdev), vbasedev,
                                &vdev->ers[i].region, i, name);
        qemu_log("%s flags 0x%x size %zu fd_offset 0x%lx "
                 "nr_mmaps %u mmaps %p\n", name,
                 vdev->ers[i].region.flags,
                 vdev->ers[i].region.size,
                 vdev->ers[i].region.fd_offset,
                 vdev->ers[i].region.nr_mmaps,
                 vdev->ers[i].region.mmaps);
        g_free(name);

        if (ret) {
            error_setg_errno(errp, -ret, "failed to get region %d info", i);
            return;
        }

        QLIST_INIT(&vdev->ers[i].quirks);
    }
    ret = vfio_get_region_info(vbasedev, VFIO_UB_CONFIG_REGION_INDEX, &reg_info);
    if (ret) {
        error_setg_errno(errp, -ret, "failed to get ub config info");
        return;
    }
    vdev->config_offset = reg_info->offset;
    g_free(reg_info);
}

static void vfio_ers_prepare(VFIOUBDevice *vdev, uint8_t nr)
{
    VFIOERS *ers = &vdev->ers[nr];

    ers->size = ers->region.size;
}

static void vfio_ers_register(VFIOUBDevice *vdev, uint8_t nr)
{
    VFIOERS *ers = &vdev->ers[nr];
    char *name;

    if (!ers->size) {
        return;
    }

    ers->mr = g_new0(MemoryRegion, 1);
    name = g_strdup_printf("%s-ERS%u", vdev->vbasedev.name, nr);
    memory_region_init_io(ers->mr, OBJECT(vdev), NULL, NULL, name, ers->size);
    g_free(name);

    if (ers->region.size) {
        memory_region_add_subregion(ers->mr, 0, ers->region.mem);

        if (vfio_region_mmap(&ers->region)) {
            error_report("Failed to mmap %s ERS%u. Performance may be slow",
                         vdev->vbasedev.name, nr);
        }
    }
    for (int i = 0; i < ers->region.nr_mmaps; i++) {
        qemu_log("mmaps[%d].mmap %p fd_offset 0x%lx offset 0x%lx size %zu mem.name %s\n",
                 i, ers->region.mmaps[i].mmap,
                 ers->region.fd_offset,
                 ers->region.mmaps[i].offset,
                 ers->region.mmaps[i].size,
                 ers->region.mmaps[i].mem.name);
    }
    ub_register_ers(&vdev->udev, nr, ers->mr);
}

static int vfio_usi_setup(VFIOUBDevice *vdev, Error **errp)
{
    usi_init(&vdev->udev, vdev->usi->vec_table_num, vdev->usi->addr_table_num,
             vdev->usi->vec_table_start_addr, vdev->usi->addr_table_start_addr,
             vdev->usi->pend_table_start_addr, vdev->ers[VFIO_UB_REGION0_INDEX].mr);
    return 0;
}

static int vfio_add_capabilities(VFIOUBDevice *vdev, Error **errp)
{
    int ret;

    ret = vfio_usi_setup(vdev, errp);

    return ret;
}
static void vfio_ers_quirk_setup(VFIOUBDevice *vdev, uint8_t nr)
{
    /* Currently, no processing is required. */
}

static void vfio_ub_fixup_usi_region(VFIOUBDevice *vdev, Error **errp)
{
    uint64_t vec_table_start, vec_table_end;
    uint64_t addr_table_start, addr_table_end;
    VFIOUSIInfo *usi = vdev->usi;
    VFIORegion *region = &vdev->ers[VFIO_UB_REGION0_INDEX].region;
    int i;

    if (region->nr_mmaps != 1 || region->mmaps[0].offset ||
        region->size != region->mmaps[0].size) {
        error_setg(errp, "vfio ub device(%s) fixup usi region failed: region.nr_mmaps(%u), "
                   "region->mmaps[0].offset(%lu), region->mmaps[0].size(%ld), region.size(%ld).",
                   vdev->vbasedev.name, region->nr_mmaps, region->mmaps[0].offset,
                   region->mmaps[0].size, region->size);
        return;
    }

    /* usi table start and end aligned to host page size */
    vec_table_start = usi->vec_table_start_addr & qemu_real_host_page_mask();
    vec_table_end = vec_table_start + usi->vec_table_num * USI_VEC_TABLE_ENTRY_SIZE;
    vec_table_end = REAL_HOST_PAGE_ALIGN(vec_table_end);
    addr_table_start = usi->addr_table_start_addr & qemu_real_host_page_mask();
    addr_table_end = addr_table_start + usi->addr_table_num * USI_ADDR_TABLE_ENTRY_SIZE;
    addr_table_end = REAL_HOST_PAGE_ALIGN(addr_table_end);

    qemu_log("vfio ub device(%s) after host page aligned usi info: "
             "vec_table_start(0x%lx) vec_table_end(0x%lx) "
             "addr_table_start(0x%lx) addr_table_end(0x%lx).\n",
             vdev->vbasedev.name, vec_table_start, vec_table_end,
             addr_table_start, addr_table_end);

    /* vec table in ERO offset is 0 */
    if (!vec_table_start) {
        if (vec_table_end >= addr_table_start) {
            /* ER0
             * ----------------------------------
             * |                |               |
             * |    vec_table   |   addr_table  |
             * |                |               |
             * ----------------------------------
             */
            if (addr_table_end >= region->size) {
                region->nr_mmaps = 0;
                g_free(region->mmaps);
                region->mmaps = NULL;
                goto complete;
            }

            /* ER0
             * ---------------------------------------------------
             * |                |               |                |
             * |    vec_table   |   addr_table  |     mmap[0]    |
             * |                |               |                |
             * ---------------------------------------------------
             */
            region->mmaps[0].offset = addr_table_end;
            region->mmaps[0].size = region->size - addr_table_end;
            goto complete;
        }

        /* ER0
         * --------------------------------------------
         * |                |           |             |
         * |    vec_table   |  mmap[0]  | addr_table  |
         * |                |           |             |
         * --------------------------------------------
         */
        if (addr_table_end >= region->size) {
            region->mmaps[0].offset = vec_table_end;
            region->mmaps[0].size = addr_table_start - vec_table_end;
            goto complete;
        }

        /* ER0
         * -------------------------------------------------------
         * |                |           |             |          |
         * |    vec_table   |  mmap[0]  | addr_table  | mmap[1]  |
         * |                |           |             |          |
         * -------------------------------------------------------
         */
        region->nr_mmaps = 2;
        region->mmaps = g_renew(VFIOMmap, region->mmaps, 2);
        memcpy(&region->mmaps[1], &region->mmaps[0], sizeof(VFIOMmap));
        region->mmaps[0].size = addr_table_start - vec_table_end;
        region->mmaps[0].offset = vec_table_end;
        region->mmaps[1].size = region->size - addr_table_end;
        region->mmaps[1].offset = addr_table_end;
        goto complete;
    }

    /* the following case, vec_table in FER0 offset is not 0 */
    if (vec_table_end >= addr_table_start) {
        /* FER0
         * ------------------------------------------
         * |           |             |              |
         * |  mmap[0]  |  vec_table  |  addr_table  |
         * |           |             |              |
         * ------------------------------------------
         */
        if (addr_table_end >= region->size) {
            region->mmaps[0].size = vec_table_start;
            region->mmaps[0].offset = 0;
            goto complete;
        }

        /* ER0
         * ------------------------------------------------------
         * |           |             |              |           |
         * |  mmap[0]  |  vec_table  |  addr_table  |  mmap[1]  |
         * |           |             |              |           |
         * ------------------------------------------------------
         * */
        region->nr_mmaps = 2;
        region->mmaps = g_renew(VFIOMmap, region->mmaps, 2);
        memcpy(&region->mmaps[1], &region->mmaps[0], sizeof(VFIOMmap));
        region->mmaps[0].size = vec_table_start;
        region->mmaps[0].offset = 0;
        region->mmaps[1].size = region->size - addr_table_end;
        region->mmaps[1].offset = addr_table_end;
        goto complete;
    }

    /* ER0
     * ------------------------------------------------------
     * |           |             |           |              |
     * |  mmap[0]  |  vec_table  |  mmap[1]  |  addr_table  |
     * |           |             |           |              |
     * ------------------------------------------------------
     * */
    if (addr_table_end >= region->size) {
        region->nr_mmaps = 2;
        region->mmaps = g_renew(VFIOMmap, region->mmaps, 2);
        memcpy(&region->mmaps[1], &region->mmaps[0], sizeof(VFIOMmap));
        region->mmaps[0].size = vec_table_start;
        region->mmaps[0].offset = 0;
        region->mmaps[1].size = addr_table_start - vec_table_end;
        region->mmaps[1].offset = vec_table_end;
        goto complete;
    }

    /* ER0
     * -------------------------------------------------------------------
     * |           |             |           |              |            |
     * |  mmap[0]  |  vec_table  |  mmap[1]  |  addr_table  |  mmap[2]   |
     * |           |             |           |              |            |
     * -------------------------------------------------------------------
     * */
    region->nr_mmaps = 3;
    region->mmaps = g_renew(VFIOMmap, region->mmaps, 3);
    memcpy(&region->mmaps[1], &region->mmaps[0], sizeof(VFIOMmap));
    memcpy(&region->mmaps[2], &region->mmaps[0], sizeof(VFIOMmap));
    region->mmaps[0].size = vec_table_start;
    region->mmaps[0].offset = 0;
    region->mmaps[1].size = addr_table_start - vec_table_end;
    region->mmaps[1].offset = vec_table_end;
    region->mmaps[2].size = region->size - addr_table_end;
    region->mmaps[2].offset = addr_table_end;

complete:
    qemu_log("vfio ub device(%s) region after fix nr_mmaps is %u.\n",
             vdev->vbasedev.name, region->nr_mmaps);
    for (i = 0; i < region->nr_mmaps; i++) {
        qemu_log("region[%d].mmap: size(0x%lx), offset(0x%lx)\n",
                 i, region->mmaps[i].size, region->mmaps[i].offset);
    }
}

static void vfio_vector_init(VFIOUBDevice *vdev, uint32_t nr)
{
    VFIOUSIVector *vector = &vdev->usi_vectors[nr];

    vector->vdev = vdev;
    vector->virq = -1;
    if (event_notifier_init(&vector->interrupt, 0)) {
        error_report("vfio: Error: event_notifier_init failed");
    }
    vector->use = true;
}

static void vfio_update_kvm_usi_virq(VFIOUSIVector *vector, USIMessage msg, UBDevice *udev)
{
    KVMRouteChange c = kvm_irqchip_begin_route_changes(kvm_state);

    qemu_log("udev(%s %s) virq(%u) start update kvm usi virq\n",
             udev->name, udev->qdev.id, vector->virq);
    kvm_irqchip_update_usi_route(&c, vector->virq, msg, udev);
    kvm_irqchip_commit_routes(kvm_state);
}

static void vfio_add_kvm_usi_virq(VFIOUBDevice *vdev, VFIOUSIVector *vector, uint16_t nr)
{
    UBDevice *udev = &vdev->udev;

    qemu_log("ub device(%s %s) vector(%u) start add usi route.\n",
             udev->name, udev->qdev.id, nr);
    vector->virq = kvm_irqchip_add_usi_route(&vfio_route_change,
                                             usi_get_message(udev, nr),
                                             ub_interrupt_id(udev),
                                             udev);
}

static void vfio_connect_kvm_usi_virq(VFIOUBDevice *vdev, VFIOUSIVector *vector,
                                      const char *name, uint16_t nr)
{
    if (vector->virq < 0) {
        qemu_log("unexpect vector virq %d < 0.\n", vector->virq);
        return;
    }

    if (event_notifier_init(&vector->kvm_interrupt, 0)) {
        qemu_log("vector kvm_interupt notifier init failed.\n");
        goto fail_notifier;
    }

    if (kvm_irqchip_add_irqfd_notifier_gsi(kvm_state, &vector->kvm_interrupt,
                                           NULL, vector->virq) < 0) {
        qemu_log("vfio ub device(%s) failed to add irqfd notifiers gsi.\n", vdev->vbasedev.name);
        goto fail_kvm;
    }

    qemu_log("vfio ub device(%s) connect kvm success.\n", vdev->vbasedev.name);
    return;

fail_kvm:
    event_notifier_cleanup(&vector->kvm_interrupt);
fail_notifier:
    kvm_irqchip_release_virq(kvm_state, vector->virq);
    vector->virq = -1;
}

static int vfio_enable_vectors(VFIOUBDevice *vdev)
{
    struct vfio_irq_set *irq_set;
    int i, argsz, ret;
    int32_t *fds;

    argsz = sizeof(*irq_set) + (vdev->nr_vectors * sizeof(*fds));
    irq_set = g_malloc0(argsz);
    irq_set->argsz = argsz;
    irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
    irq_set->index = VFIO_UB_INTR_IRQ_INDEX;
    irq_set->start = 0;
    irq_set->count = vdev->nr_vectors;
    fds = (int32_t *)&irq_set->data;

    for (i = 0; i < vdev->nr_vectors; i++) {
        int fd = -1;

        if (vdev->usi_vectors[i].use) {
            fd = event_notifier_get_fd(&vdev->usi_vectors[i].kvm_interrupt);
        }

        fds[i] = fd;
    }

    ret = ioctl(vdev->vbasedev.fd, VFIO_DEVICE_SET_IRQS, irq_set);
    g_free(irq_set);

    return ret;
}

static int vfio_usi_vector_do_use(UBDevice *udev, uint16_t nr, USIMessage *msg,
                                  IOHandler *handler)
{
    VFIOUBDevice *vdev = VFIO_UB(udev);
    VFIOUSIVector *vector = NULL;
    int32_t fd;
    int ret;
    Error *err = NULL;

    if (!kvm_usi_via_irqfd_enabled()) {
        qemu_log("kvm usi via irqfd disabled.\n");
        return 0;
    }

    vector = &vdev->usi_vectors[nr];
    if (!vector->use) {
        vfio_vector_init(vdev, nr);
    }

    qemu_set_fd_handler(event_notifier_get_fd(&vector->interrupt),
                        handler, NULL, vector);

    if (vector->virq >= 0) {
        if (!msg) {
            qemu_log("vfio_remove_kvm_usi_virq %u\n", vector->virq);
        } else {
            vfio_update_kvm_usi_virq(vector, *msg, udev);
        }
    } else {
        if (msg) {
            vfio_route_change = kvm_irqchip_begin_route_changes(kvm_state);
            vfio_add_kvm_usi_virq(vdev, vector, nr);
            kvm_irqchip_commit_route_changes(&vfio_route_change);
            vfio_connect_kvm_usi_virq(vdev, vector, NULL, nr);
        }
    }

    if (vdev->nr_vectors < nr + 1) {
        vdev->nr_vectors = nr + 1;
        vfio_disable_irqindex(&vdev->vbasedev, VFIO_UB_INTR_IRQ_INDEX);
        ret = vfio_enable_vectors(vdev);
        if (ret < 0) {
            error_report("vfio: failed to enable vectors, %d", ret);
        }
    } else {
        fd = event_notifier_get_fd(&vector->kvm_interrupt);
        ret = vfio_set_irq_signaling(&vdev->vbasedev, VFIO_UB_INTR_IRQ_INDEX, nr,
                                     VFIO_IRQ_SET_ACTION_TRIGGER, fd, &err);
        if (ret) {
            error_reportf_err(err, VFIO_MSG_PREFIX, vdev->vbasedev.name);
        }
    }

    return 0;
}

static void vfio_usi_vector_release(UBDevice *udev, uint16_t nr)
{
    VFIOUBDevice *vdev = VFIO_UB(udev);
    VFIOUSIVector *vector = &vdev->usi_vectors[nr];

    if (vector->virq > 0) {
        int32_t fd = event_notifier_get_fd(&vector->interrupt);
        Error *err = NULL;

        qemu_log("udev(%s %s) start update fd to qemu.\n",
                 udev->name, udev->qdev.id);
        if (vfio_set_irq_signaling(&vdev->vbasedev, VFIO_UB_INTR_IRQ_INDEX, nr,
                                   VFIO_IRQ_SET_ACTION_TRIGGER, fd, &err)) {
            error_reportf_err(err, VFIO_MSG_PREFIX, vdev->vbasedev.name);
            return;
        }

        qemu_log("udev(%s %s) update fd to qemu success.\n",
                 udev->name, udev->qdev.id);
    }
}

static void vfio_usi_interrupt(void *opaque)
{
    VFIOUSIVector *vector = opaque;
    VFIOUBDevice *vdev = vector->vdev;
    int nr = vector - vdev->usi_vectors;

    if (!event_notifier_test_and_clear(&vector->interrupt)) {
        return;
    }

    if (usi_is_masked(&vdev->udev, nr)) {
        /* process for memory region pba */
    }

    usi_notify(&vdev->udev, nr);
}

static int vfio_usi_vector_use(UBDevice *udev, uint16_t nr, USIMessage msg)
{
    return vfio_usi_vector_do_use(udev, nr, &msg, vfio_usi_interrupt);
}

static void vfio_usi_early_setup(VFIOUBDevice *vdev, Error **errp)
{
    int fd = vdev->vbasedev.fd;
    uint64_t offset = vdev->config_offset;
    uint16_t vec_table_num;
    uint16_t addr_table_num;
    uint64_t vec_table_start_addr;
    uint64_t addr_table_start_addr;
    uint64_t pend_table_start_addr;
    VFIOUSIInfo *usi = NULL;

    if (pread(fd, &vec_table_num, sizeof(vec_table_num),
              offset + UB_CFG1_CAP4_INT_TYPE2_NUMOF_INT_VEC_OFFSET) != sizeof(vec_table_num)) {
        error_setg_errno(errp, errno, "failed to read NUMOF_INT_VEC");
        return;
    }

    if (pread(fd, &addr_table_num, sizeof(addr_table_num),
              offset + UB_CFG1_CAP4_INT_TYPE2_NUMOF_INT_ADDR_OFFSET) != sizeof(addr_table_num)) {
        error_setg_errno(errp, errno, "failed to read NUMOF_INT_ADDR");
        return;
    }

    if (pread(fd, &vec_table_start_addr, sizeof(vec_table_start_addr),
              offset + UB_CFG1_CAP4_INT_TYPE2_INT_VEC_TAB_OFFSET) != sizeof(vec_table_start_addr)) {
        error_setg_errno(errp, errno, "failed to read INT_VEC_TAB_START_ADDR");
        return;
    }

    if (pread(fd, &addr_table_start_addr, sizeof(addr_table_start_addr),
              offset + UB_CFG1_CAP4_INT_TYPE2_INT_ADDR_TAB_OFFSET) != sizeof(addr_table_start_addr)) {
        error_setg_errno(errp, errno, "failed to read INT_ADDR_TAB_START_ADDR");
        return;
    }

    if (pread(fd, &pend_table_start_addr, sizeof(pend_table_start_addr),
              offset + UB_CFG1_CAP4_INT_TYPE2_INT_PENDING_TAB_OFFSET) != sizeof(pend_table_start_addr)) {
        error_setg_errno(errp, errno, "failed to read INT_PENDING_TAB_START_ADDR");
        return;
    }

    vec_table_num = le16_to_cpu(vec_table_num);
    addr_table_num = le16_to_cpu(addr_table_num);
    vec_table_start_addr = le64_to_cpu(vec_table_start_addr);
    addr_table_start_addr = le64_to_cpu(addr_table_start_addr);
    pend_table_start_addr = le64_to_cpu(pend_table_start_addr);

    usi = g_malloc0(sizeof(VFIOUSIInfo));
    /* according to UB SPEC, vec_table_num&addr_table_num is base 0, so this value need +1 */
    usi->vec_table_num = vec_table_num + 1;
    usi->addr_table_num = addr_table_num + 1;
    usi->vec_table_start_addr = vec_table_start_addr;
    usi->addr_table_start_addr = addr_table_start_addr;
    usi->pend_table_start_addr = pend_table_start_addr;

    qemu_log("vfio ub device(%s %s) usi info: vec_table_num(%u), addr_table_num(%u), "
             "vec_table_start_addr(0x%lx), addr_table_start_addr(0x%lx), "
             "pend_table_start_addr(0x%lx).\n",
             vdev->vbasedev.name, vdev->udev.qdev.id, usi->vec_table_num,
             usi->addr_table_num, usi->vec_table_start_addr, usi->addr_table_start_addr,
             usi->pend_table_start_addr);

    vdev->usi = usi;
    vfio_ub_fixup_usi_region(vdev, errp);
}

static void vfio_ers_exit(VFIOUBDevice *vdev)
{
    int i;

    for (i = 0; i < UB_NUM_REGIONS; i++) {
        VFIOERS *er = &vdev->ers[i];

        vfio_region_exit(&er->region);
        if (er->region.size) {
            memory_region_del_subregion(er->mr, er->region.mem);
        }
    }
}

static void vfio_copy_config_space_slice(VFIOUBDevice *vdev, size_t offset, size_t read_size)
{
    int ret = 0;
    uint64_t emulated_offset;

    emulated_offset = ub_cfg_offset_to_emulated_offset(offset, false);
    if (emulated_offset == UINT64_MAX) {
        qemu_log("vfio copy cfg space slice out of emulated cfg range, "
                 "offset is 0x%lx\n", offset);
        return;
    }

    ret = pread(vdev->vbasedev.fd, vdev->udev.config + emulated_offset,
                read_size, vdev->config_offset + offset);
    if (ret < read_size) {
        qemu_log("failed to read device config space, ret: %d, offset: %lu, read_size: %lu\n",
                 ret, offset, read_size);
    }
}

static void vfio_copy_config_space_caps(VFIOUBDevice *vdev, uint8_t *bit_map, size_t base_off)
{
    size_t bit_idx = 1; // bit0 in bit_map is invalid
    size_t offset, rsize;
    unsigned long *addr = (unsigned long *)bit_map;

    for_each_set_bit_from(bit_idx, addr, BITS_PER_CAP_BIT_MAP) {
        offset = base_off + UB_SLICE_SZ * (bit_idx - 1);
        rsize = UB_SLICE_SZ;
        vfio_copy_config_space_slice(vdev, offset, rsize);
    }
}

static void vfio_copy_config_space(VFIOUBDevice *vdev)
{
    size_t offset;
    size_t rsize;
    uint64_t emulated_offset;
    UbCfg0Basic *cfg0_basic;
    UbCfg1Basic *cfg1_basic;

    /* copy cfg0 */
    offset = UB_CFG0_BASIC_START;
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    rsize = UB_SLICE_SZ;
    vfio_copy_config_space_slice(vdev, offset, rsize); // cfg0 basic
    cfg0_basic = (UbCfg0Basic *)(vdev->udev.config + emulated_offset);
    vfio_copy_config_space_caps(vdev, cfg0_basic->cap_bitmap,
                                UB_CFG0_BASIC_START + UB_SLICE_SZ); // cfg0 caps

    /* copy cfg1 */
    offset = UB_CFG1_BASIC_START;
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    vfio_copy_config_space_slice(vdev, offset, rsize); // cfg1 basic
    cfg1_basic = (UbCfg1Basic *)(vdev->udev.config + emulated_offset);
    vfio_copy_config_space_caps(vdev, cfg1_basic->cap_bitmap,
                                UB_CFG1_BASIC_START + UB_SLICE_SZ); // cfg1 caps
}

static void vfio_cfg1_idev_ubba_init(VFIOUBDevice *vdev, Error **errp)
{
    int i;
    uint64_t emulated_offset;
    hwaddr addr;
    UbCfg1Basic *cfg1_basic = NULL;

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(vdev->udev.config + emulated_offset);
    for (i = 0; i < UB_NUM_REGIONS; i++) {
        /* if config1:feat.ers0s is 0, no need init ubba? */
        addr = ub_idev_ers_alloc_address_space(cfg1_basic->ers_space_size[i],
                                                cfg1_basic->sys_pgs);
        if (addr == UINT64_MAX) {
            error_setg(errp, "failed to alloc address space for idev.");
            return;
        }

        qemu_log("vfio-ub idev(%s) ers[%d] alloc ubba: 0x%lx,"
                 "host ubba: 0x%lx.\n",
                 vdev->vbasedev.name, i, addr, cfg1_basic->ers_ubba[i]);
        cfg1_basic->ers_ubba[i] = addr;
    }
}

static void vfio_cfg1_ubba_init(VFIOUBDevice *vdev, Error **errp)
{
    uint64_t emulated_offset;
    UbCfg1Basic *cfg1_basic, *emulated_cfg1_basic;

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(vdev->udev.config + emulated_offset);
    emulated_cfg1_basic = (UbCfg1Basic *)(vdev->emulated_config_bits + emulated_offset);
    memset(&emulated_cfg1_basic->ers_ubba, 0xff,
           sizeof(emulated_cfg1_basic->ers_ubba));

    if (vdev->udev.dev_type == UB_TYPE_DEVICE) {
        /* clear UBBA from vfio, the value of UB_TYPE_DEVICE should come from guestOS */
        memset(&cfg1_basic->ers_ubba, 0, sizeof(cfg1_basic->ers_ubba));
    } else if (vdev->udev.dev_type == UB_TYPE_IDEVICE) {
        /* init ubba for idev */
        vfio_cfg1_idev_ubba_init(vdev, errp);
    }
}

static void vfio_cfg1_feature_init(VFIOUBDevice *vdev)
{
    UbCfg1Basic *cfg1_basic, *emulated_cfg1_basic;
    Cfg1SupportFeature *feat, *emulated_feat;
    uint64_t emulated_offset;

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(vdev->udev.config + emulated_offset);
    emulated_cfg1_basic = (UbCfg1Basic *)(vdev->emulated_config_bits + emulated_offset);
    feat = &cfg1_basic->support_feature;
    emulated_feat = &emulated_cfg1_basic->support_feature;
    feat->bits.ubbas = 1; /* UBBA must exist in VM for mmio mmap */
    emulated_feat->bits.ubbas = 1;
}

static void vfio_cfg1_init(VFIOUBDevice *vdev, Error **errp)
{
    uint64_t emulated_offset;
    UbCfg1IntType2Cap *emulate_cfg1_int_cap = NULL;

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2, true);
    emulate_cfg1_int_cap = (UbCfg1IntType2Cap *)(vdev->emulated_config_bits + emulated_offset);
    emulate_cfg1_int_cap->interrupt_id = ~0U;
    emulate_cfg1_int_cap->interrupt_enable = ~0;
    emulate_cfg1_int_cap->interrupt_mask = ~0;
    vfio_cfg1_feature_init(vdev);
    vfio_cfg1_ubba_init(vdev, errp);
}

static void vfio_cfg0_init(VFIOUBDevice *vdev)
{
    ConfigPortBasic *emulated_port_basic;
    UbCfg0Basic *cfg0_basic, *emulated_cfg0_basic;
    uint16_t port_num, port_idx;
    size_t offset;
    uint64_t emulated_offset;

    /* emulated feilds in cfg0 basic */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    cfg0_basic = (UbCfg0Basic *)(vdev->udev.config + emulated_offset);
    emulated_cfg0_basic = (UbCfg0Basic *)(vdev->emulated_config_bits + emulated_offset);
    memset(&emulated_cfg0_basic->total_num_of_ue, 0xff,
           sizeof(emulated_cfg0_basic->total_num_of_ue));
    cfg0_basic->total_num_of_ue = 1; // Only FE0 is supported in VM
    memset(&emulated_cfg0_basic->total_num_of_port, 0xff,
           sizeof(emulated_cfg0_basic->total_num_of_port));
    cfg0_basic->total_num_of_port = vdev->udev.port.port_num & UINT16_MASK;
    memset(&emulated_cfg0_basic->guid, 0xff, sizeof(UbGuid));
    memcpy(&cfg0_basic->guid, &vdev->udev.guid, sizeof(UbGuid));
    memset(&emulated_cfg0_basic->eid, 0xff, sizeof(UbEid));
    memset(&cfg0_basic->eid, 0, sizeof(UbEid));
    memset(&emulated_cfg0_basic->fm_eid, 0xff, sizeof(UbEid));
    memset(&cfg0_basic->fm_eid, 0, sizeof(UbEid));
    emulated_cfg0_basic->net_addr_info.primary_cna = 0xffffff;
    cfg0_basic->net_addr_info.primary_cna = 0;
    emulated_cfg0_basic->ueid_low = ~0UL;
    emulated_cfg0_basic->ueid_high = ~0UL;
    emulated_cfg0_basic->ucna = ~0;
    cfg0_basic->support_feature.bits.route_table_supported = 0;
    emulated_cfg0_basic->support_feature.bits.route_table_supported = ~0;
    /* port info need emulate */
    port_num = cfg0_basic->total_num_of_port;
    for (port_idx = 0; port_idx < port_num; ++port_idx) {
        offset = UB_PORT_SLICE_START + port_idx * UB_PORT_SZ;
        emulated_offset = ub_cfg_offset_to_emulated_offset(offset, true);
        emulated_port_basic = (ConfigPortBasic *)(vdev->emulated_config_bits + emulated_offset);
        memset(emulated_port_basic, 0xff, sizeof(ConfigPortBasic));
    }
}

static void vfio_emulate_bits_init(VFIOUBDevice *vdev, Error **errp)
{
    if (!vdev) {
        qemu_log("vfio init emulated bits fail, device null\n");
        return;
    }

    vdev->config_size = ub_emulated_config_size();
    vdev->emulated_config_bits = g_malloc0(vdev->config_size);
    vfio_cfg0_init(vdev);
    vfio_cfg1_init(vdev, errp);
}

static bool vfio_check_guid(VFIOUBDevice *vdev, Error **errp)
{
    UBDevice *udev = &vdev->udev;
    UbGuid *hguid = &vdev->host.guid;

    if (udev->guid.type != UB_GUID_TYPE_BUS_CONTROLLER &&
        udev->guid.type != UB_GUID_TYPE_IBUS_CONTROLLER) {
        qemu_log("%s device type set error, expect: %u or %u, actual: %u\n",
                 udev->qdev.id, UB_GUID_TYPE_BUS_CONTROLLER, UB_GUID_TYPE_IBUS_CONTROLLER, udev->guid.type);
        error_setg(errp, "%s device type set error, expect: %u or %u, actual: %u\n",
                   udev->qdev.id, UB_GUID_TYPE_BUS_CONTROLLER, UB_GUID_TYPE_IBUS_CONTROLLER, udev->guid.type);
        return false;
    }

    if (hguid->type != udev->guid.type ||
        hguid->vendor != udev->guid.vendor ||
        hguid->rsv != udev->guid.rsv ||
        hguid->device_id != udev->guid.device_id ||
        hguid->version != udev->guid.version) {
        qemu_log("%s guid and host are not matching\n", udev->qdev.id);
        error_setg(errp, "%s guid and host are not matching\n", udev->qdev.id);
        return false;
    }
    return true;
}

static void vfio_reinit_irq_handler(void *opaque)
{
    VFIOUBDevice *vdev = opaque;
    UBDevice *udev = &vdev->udev;
    UBBus *bus;
    BusControllerState *ubc;
    LinkChangeMsgPkt rsp_pkt;
    HiMsgCqe cqe;
    uint32_t scna;
    uint16_t port_idx;
    uint64_t offset;
    uint64_t emulated_offset;
    uint8_t sub_msg_code;
    ConfigNetAddrInfo *net_addr_info;

    if (!event_notifier_test_and_clear(&vdev->reinit_irq)) {
        return;
    }

    bus = ub_get_bus(udev);
    ubc = container_of_ubbus(bus);
    if (!ubc) {
        qemu_log("vfio ub device(%s %s) reinit irq handler: cannot find ubc\n",
                 udev->name, udev->qdev.id);
        return;
    }

    qemu_log("vfio ub device(%s %s) reinit irq handler: ubc %p\n",
             udev->name, udev->qdev.id, (void *)ubc);

    /* Only support LINK_UP */
    sub_msg_code = UB_LINK_UP;

    /* Get SCNA and port_idx from UBC's ConfigPortBasic (port 0) */
    offset = UB_PORT_SLICE_START;  /* port_idx = 0 */
    emulated_offset = ub_cfg_offset_to_emulated_offset(offset, true);
    if (emulated_offset == UINT64_MAX) {
        qemu_log("vfio ub: invalid port offset 0x%lx\n", offset);
        return;
    }

    net_addr_info = (ConfigNetAddrInfo *)(ubc->ubc_dev->parent.config + emulated_offset);
    scna = net_addr_info->primary_cna;
    port_idx = udev->port.neighbors->neighbor_port_idx;
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_NA_INFO_START, true);

    qemu_log("vfio ub device(%s %s): link change info - SCNA=%u, sport_idx=%u, event=%s\n",
             udev->name, udev->qdev.id, scna, port_idx,
             sub_msg_code == UB_LINK_UP ? "UP" : "DOWN");

    /* Build link change message request */
    memset(&rsp_pkt, 0, sizeof(LinkChangeMsgPkt));
    memset(&cqe, 0, sizeof(HiMsgCqe));

    /* Set header - use MSG_REQ since this is a notification to driver */
    rsp_pkt.header.msgetah.type = MSG_REQ;
    rsp_pkt.header.msgetah.msg_code = UB_MSG_CODE_LINK;
    rsp_pkt.header.msgetah.sub_msg_code = sub_msg_code;
    rsp_pkt.header.msgetah.plen = LINK_CHANGE_MSG_PLD_SIZE;

    /* Use UBC's CNA as both source and destination */
    rsp_pkt.header.nth.scna = ubc->ubc_dev->parent.cna;
    rsp_pkt.header.nth.dcna = ubc->ubc_dev->parent.cna;

    /* Use UBC's EID for both source and destination */
    rsp_pkt.header.seid_h = EID_HIGH(ubc->ubc_dev->parent.eid);
    rsp_pkt.header.seid_l = EID_LOW(ubc->ubc_dev->parent.eid);
    rsp_pkt.header.deid = ubc->ubc_dev->parent.eid;

    /* Build payload: 8 bytes (resvd + SCNA + resvd + port_idx) */
    rsp_pkt.pld.reserved1 = 0;
    rsp_pkt.pld.scna = scna & 0xFFFFFF;
    rsp_pkt.pld.reserved2 = 0;
    rsp_pkt.pld.port_idx = port_idx;

    /* Set CQE */
    cqe.type = MSG_REQ;
    cqe.msg_code = UB_MSG_CODE_LINK;
    cqe.sub_msg_code = sub_msg_code;
    cqe.msn = 0;
    cqe.p_len = MSG_LINK_CHANGE_PKT_SIZE;
    cqe.status = CQE_SUCCESS;

    /* Write to RQ and CQ atomically */
    fill_rq_cq(ubc, &rsp_pkt, sizeof(LinkChangeMsgPkt), &cqe);

    qemu_log("vfio ub: link change notification sent: SCNA=%u, port_idx=%u, event=%s\n",
             scna, port_idx, sub_msg_code == UB_LINK_UP ? "UP" : "DOWN");
}

static int vfio_enable_reinit_irq(VFIOUBDevice *vdev)
{
    int ret;
    int32_t fd;

    if (vdev->reinit_irq_enabled) {
        return 0;
    }

    ret = event_notifier_init(&vdev->reinit_irq, 0);
    if (ret) {
        error_report("vfio: reinit irq event_notifier_init failed");
        return ret;
    }

    qemu_set_fd_handler(event_notifier_get_fd(&vdev->reinit_irq),
                        vfio_reinit_irq_handler, NULL, vdev);

    fd = event_notifier_get_fd(&vdev->reinit_irq);
    ret = vfio_set_irq_signaling(&vdev->vbasedev, VFIO_UB_REINIT_IRQ_INDEX, 0,
                                 VFIO_IRQ_SET_ACTION_TRIGGER, fd, NULL);
    if (ret) {
        error_report("vfio: failed to enable reinit irq, ret=%d", ret);
        qemu_set_fd_handler(event_notifier_get_fd(&vdev->reinit_irq),
                            NULL, NULL, NULL);
        event_notifier_cleanup(&vdev->reinit_irq);
        return ret;
    }

    vdev->reinit_irq_enabled = true;
    qemu_log("vfio ub device(%s %s) reinit irq enabled\n",
             vdev->udev.name, vdev->udev.qdev.id);
    return 0;
}

static void vfio_realize(UBDevice *udev, Error **errp)
{
    VFIOUBDevice *vdev = VFIO_UB(udev);
    VFIODevice *vbasedev = &vdev->vbasedev;
    uint32_t id;
    int ret;
    uint8_t i;
    uint32_t bus_instance_eid;
    int bus_instance_type;
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    Error *err = NULL;

    if (!vfio_check_guid(vdev, errp)) {
        return;
    }
    if (!vdev->vbasedev.sysfsdev) {
        if (!ub_guid_initialized(&vdev->host.guid)) {
            error_setg(errp, "No provided host device");
            error_append_hint(errp, "Use -device vfio-ub,host="GUID_STR_EXAMPLE
                                    "or -device vfio-ub,sysfsdev=PATH_TO_DEVICE\n");
            return;
        }
        /* Obtain the actual dev id of the device using the guid through sysfs.  */
        id = sysfs_get_dev_number_by_guid(&vdev->host.guid);
        if (id == UINT32_MAX) {
            ub_device_get_str_from_guid(&vdev->host.guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);
            error_setg(errp, "not found device, guid: %s\n", guid_str);
            return;
        }
        vdev->vbasedev.sysfsdev = g_strdup_printf("/sys/bus/ub/devices/%05x", id);

        /* ub device get bus instance eid&type */
        bus_instance_eid = sysfs_get_ub_device_bus_instance_eid(vdev->vbasedev.sysfsdev);
        bus_instance_type = sysfs_get_bus_instance_type_by_eid(bus_instance_eid);
        if (!UBUS_INSTANCE_IS_DYNAMIC(bus_instance_type)) {
            error_setg(errp, "ub device(%s) not bind to dynamic bus instance\n", vdev->vbasedev.sysfsdev);
            return;
        }

        udev->bus_instance_eid = bus_instance_eid;
        ret = udev->bus_instance_verify(udev, errp);
        if (ret) {
            qemu_log("vfio ub device bus instance verify failed\n");
            return;
        }
        udev->host_dev = true;
    }
    qemu_log("sysfsdev %s\n", vdev->vbasedev.sysfsdev);

    vdev->vbasedev.name = g_path_get_basename(vdev->vbasedev.sysfsdev);
    if (ub_device_check_ummu_is_nested(udev) && !vdev->vbasedev.iommufd) {
        error_setg(errp, "iommufd is require for nested ummu.");
        goto error;
    }

    ret = vfio_attach_device(vdev->vbasedev.name, &vdev->vbasedev,
                             ub_device_iommu_address_space(udev), errp);
    if (ret) {
        goto error;
    }

    vfio_populate_device(vdev, &err);
    if (err) {
        error_propagate(errp, err);
        goto error;
    }

    /* Get a copy of config space */
    vfio_copy_config_space(vdev);

    /* Get dev type frome cfg1Basic and guid */
    udev->dev_type = ub_dev_get_type(udev);

    /* vfio emulates a lot for us, but some bits need extra love */
    vfio_emulate_bits_init(vdev, &err);
    if (err) {
        error_propagate(errp, err);
        goto error;
    }

    for (i = 0; i < UB_NUM_REGIONS; i++) {
        vfio_ers_prepare(vdev, i);
    }

    vfio_usi_early_setup(vdev, &err);
    if (err) {
        error_propagate(errp, err);
        goto error;
    }

    for (i = 0; i < UB_NUM_REGIONS; i++) {
        vfio_ers_register(vdev, i);
    }

    if (!vbasedev->mdev && vbasedev->iommufd) {
        ret = ub_device_set_iommu_device(udev, vbasedev->hiod, errp);
        if (ret) {
            error_prepend(errp, "Failed to set iommu_device: ");
            goto out_teardown;
        }
    }

    ret = vfio_add_capabilities(vdev, errp);
    if (ret) {
        goto out_unset_idev;
    }

    for (i = 0; i < UB_NUM_REGIONS; i++) {
        vfio_ers_quirk_setup(vdev, i);
    }

    ret = vfio_enable_reinit_irq(vdev);
    if (ret) {
        error_prepend(errp, "failed to enable reinit irq: ");
        goto out_unset_idev;
    }

    return;
out_unset_idev:
    ub_device_unset_iommu_device(udev);
out_teardown:
    vfio_ers_exit(vdev);

error:
    error_prepend(errp, VFIO_MSG_PREFIX, vdev->vbasedev.name);
}

static void vfio_cfg1_idev_ubba_deinit(UBDevice *udev)
{
    int i;
    UbCfg1Basic *cfg1_basic = NULL;
    uint64_t emulated_offset;

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(udev->config + emulated_offset);
    for (i = 0; i < UB_NUM_REGIONS; i++) {
        ub_idev_ers_free_address_space(cfg1_basic->ers_ubba[i]);
    }
}

static void vfio_teardown_usi(VFIOUBDevice *vdev)
{
    if (vdev->usi) {
        usi_uninit(&vdev->udev, vdev->ers[VFIO_UB_REGION0_INDEX].mr);
    }
}

static void vfio_exitfn(UBDevice *udev)
{
    VFIOUBDevice *vdev = VFIO_UB(udev);

    if (udev->dev_type == UB_TYPE_IDEVICE) {
        vfio_cfg1_idev_ubba_deinit(udev);
    }
    vfio_disable_interrupts(vdev);

    if (vdev->reinit_irq_enabled) {
        vfio_disable_irqindex(&vdev->vbasedev, VFIO_UB_REINIT_IRQ_INDEX);
        qemu_set_fd_handler(event_notifier_get_fd(&vdev->reinit_irq),
                            NULL, NULL, NULL);
        event_notifier_cleanup(&vdev->reinit_irq);
        vdev->reinit_irq_enabled = false;
    }

    vfio_teardown_usi(vdev);
    vfio_ers_exit(vdev);
    ub_device_unset_iommu_device(udev);
}

static void vfio_disable_interrupts(VFIOUBDevice *vdev)
{
    vfio_usi_disable(vdev);
}

static void vfio_usi_enable(VFIOUBDevice *vdev)
{
    vfio_disable_interrupts(vdev);

    vdev->usi_vectors = g_new0(VFIOUSIVector, vdev->usi->vec_table_num);
    usi_set_vector_notifiers(&vdev->udev, vfio_usi_vector_use, vfio_usi_vector_release, NULL);
    qemu_log("vfio ub device(%s %s) usi enable, vectors %d,"
             "vec_table_num %u, addr_table_num %u.\n",
             vdev->udev.name, vdev->udev.qdev.id, vdev->nr_vectors,
             vdev->usi->vec_table_num, vdev->usi->addr_table_num);
}

static void vfio_remove_kvm_usi_virq(VFIOUSIVector *vector)
{
    kvm_irqchip_remove_irqfd_notifier_gsi(kvm_state, &vector->kvm_interrupt,
                                          vector->virq);
    kvm_irqchip_release_virq(kvm_state, vector->virq);
    vector->virq = -1;
    event_notifier_cleanup(&vector->kvm_interrupt);
}

static void vfio_usi_disable_common(VFIOUBDevice *vdev)
{
    int i;
    VFIOUSIVector *vector = NULL;

    for (i = 0; i < vdev->nr_vectors; i++) {
        vector = &vdev->usi_vectors[i];
        if (!vector->use) {
            continue;
        }

        if (vector->virq >= 0) {
            vfio_remove_kvm_usi_virq(vector);
        }

        qemu_set_fd_handler(event_notifier_get_fd(&vector->interrupt),
                            NULL, NULL, NULL);
        event_notifier_cleanup(&vector->interrupt);
    }

    g_free(vdev->usi_vectors);
    vdev->usi_vectors = NULL;
    vdev->nr_vectors = 0;
}

static void vfio_usi_disable(VFIOUBDevice *vdev)
{
    usi_unset_vector_notifiers(&vdev->udev);

    if (vdev->nr_vectors) {
        vfio_disable_irqindex(&vdev->vbasedev, VFIO_UB_INTR_IRQ_INDEX);
    }
    qemu_log("vfio ub device(%s %s) usi disable, vectors %d.\n",
             vdev->udev.name, vdev->udev.qdev.id, vdev->nr_vectors);
    vfio_usi_disable_common(vdev);
}

static void vfio_ub_read_config(UBDevice *dev, uint64_t offset,
                                uint32_t *val, uint32_t dw_mask)
{
    VFIOUBDevice *vdev = VFIO_UB(dev);
    uint32_t emu_bits = 0;
    uint32_t emu_val = 0;
    uint32_t phys_val = 0;
    uint64_t emulated_offset;

    /* emu_bits bit_n: 0 means need read value from phy dev; 1 means read value from emulated config space */
    emulated_offset = ub_cfg_offset_to_emulated_offset(offset, false);
    if (emulated_offset != UINT64_MAX) {
        emu_bits = *(uint32_t *)(vdev->emulated_config_bits + emulated_offset);
        if (emu_bits) {
            ub_default_read_config(dev, offset, &emu_val, dw_mask);
        }
    }

    if (pread(vdev->vbasedev.fd, &phys_val, DWORD_SIZE, vdev->config_offset + offset)
        != DWORD_SIZE) {
        qemu_log("read value from phys dev: %s, addr: 0x%lx failed\n", vdev->vbasedev.name, offset);
        *val = 0;
        return;
    }
    phys_val &= dw_mask;
    *val = (emu_val & emu_bits) | (phys_val & ~emu_bits);
    trace_vfio_ub_read_config(offset, emu_val, phys_val, *val);
}

static void vfio_sub_page_ers_update_mapping(UBDevice *udev, int ers_id)
{
    /* do nothing now */
}

static void vfio_ub_write_config(UBDevice *dev, uint64_t offset,
                                 uint32_t *val, uint32_t dw_mask)
{
    VFIOUBDevice *vdev = VFIO_UB(dev);
    uint32_t write_val = *val;
    uint32_t phys_val;
    int is_enabled, was_enabled, is_masked, was_masked;

    /* get old phys_val */
    if (pread(vdev->vbasedev.fd, &phys_val, DWORD_SIZE,
              vdev->config_offset + offset)
        != DWORD_SIZE) {
        qemu_log("read value from phys dev: %s, addr: 0x%lx failed\n",
                 vdev->vbasedev.name, offset);
        return;
    }
    trace_vfio_ub_write_config(offset, *val, dw_mask, phys_val);

    phys_val = (write_val & dw_mask) | (phys_val & ~dw_mask);
    /* update value to phy config space */
    if (pwrite(vdev->vbasedev.fd, &phys_val, DWORD_SIZE, vdev->config_offset + offset)
        != DWORD_SIZE) {
        qemu_log("write value to phys dev: %s, addr: 0x%lx failed\n",
                 vdev->vbasedev.name, offset);
        return;
    }

    /* check whether the UBBA is updated by GuestOS */
    if (ranges_overlap(offset, DWORD_SIZE,
                       UB_CFG1_BASIC_START + offsetof(UbCfg1Basic, ers_ubba),
        UB_NUM_REGIONS * sizeof(uint64_t))) {
        uint64_t old_addr[UB_NUM_REGIONS];
        int ers_id;

        for (ers_id = 0; ers_id < UB_NUM_REGIONS; ers_id++) {
            old_addr[ers_id] = dev->io_regions[ers_id].addr;
        }

        /* update cfg */
        ub_default_write_config(dev, offset, val, dw_mask);
        for (ers_id = 0; ers_id < UB_NUM_REGIONS; ers_id++) {
            trace_vfio_ub_write_config_ioregion(ers_id, old_addr[ers_id],
                                                dev->io_regions[ers_id].addr,
                                                dev->io_regions[ers_id].size);
            trace_vfio_ub_write_config_ers(ers_id, vdev->ers[ers_id].region.fd_offset,
                                            vdev->ers[ers_id].region.size,
                                            vdev->ers[ers_id].region.flags,
                                            qemu_real_host_page_size());
            if (old_addr[ers_id] != dev->io_regions[ers_id].addr &&
                vdev->ers[ers_id].region.size > 0 &&
                vdev->ers[ers_id].region.size < qemu_real_host_page_size()) {
                vfio_sub_page_ers_update_mapping(dev, ers_id);
            }
        }

    /* check whether the INT CAP Enable is update */
    } else if (ranges_overlap(offset, DWORD_SIZE,
                              UB_CFG1_CAP4_INT_TYPE2_ENABLE_OFFSET, DWORD_SIZE)) {
        was_enabled = usi_enabled(dev);
        ub_default_write_config(dev, offset, val, dw_mask);
        is_enabled = usi_enabled(dev);
        trace_vfio_ub_write_config_int_cap_en(*val, was_enabled, is_enabled);
        if (is_enabled && !was_enabled) {
            vfio_usi_enable(vdev);
        } else if (was_enabled && !is_enabled) {
            vfio_usi_disable(vdev);
        }

    /* check whether the INT CAP FE Mask is update */
    } else if (ranges_overlap(offset, DWORD_SIZE,
                              UB_CFG1_CAP4_INT_TYPE2_MASK_OFFSET, DWORD_SIZE)) {
        was_masked = usi_ue_is_masked(dev);
        ub_default_write_config(dev, offset, val, dw_mask);
        is_masked = usi_ue_is_masked(dev);
        trace_vfio_ub_write_config_int_cap_mask(*val, was_masked, is_masked);
        usi_handle_ue_mask_update(dev, was_masked);
    } else {
        /* sync phy config space and write emulated value to emulated config space */
        ub_default_write_config(dev, offset, val, dw_mask);
    }
}

#ifdef CONFIG_IOMMUFD
static void vfio_ub_set_fd(Object *obj, const char *str, Error **errp)
{
    vfio_device_set_fd(&VFIO_UB(obj)->vbasedev, str, errp);
}
#endif

static void vfio_ub_dev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    UBDeviceClass *udc = UB_DEVICE_CLASS(klass);

    dc->reset = vfio_ub_reset;
    device_class_set_props(dc, vfio_ub_dev_properties);
#ifdef CONFIG_IOMMUFD
    object_class_property_add_str(klass, "fd", NULL, vfio_ub_set_fd);
#endif
    dc->vmsd = &vfio_ub_vmstate;
    dc->desc = "VFIO-based UB device assignment";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    udc->realize = vfio_realize;
    udc->exit = vfio_exitfn;
    udc->config_read = vfio_ub_read_config;
    udc->config_write = vfio_ub_write_config;
}

static void vfio_instance_init(Object *obj)
{
    VFIOUBDevice *vdev = VFIO_UB(obj);
    VFIODevice *vbasedev = &vdev->vbasedev;

    memset(&vdev->host, 0, sizeof(vdev->host));
    vfio_device_init(vbasedev, VFIO_DEVICE_TYPE_UB, &vfio_ub_ops,
                     DEVICE(vdev), false);
}

static void vfio_instance_finalize(Object *obj)
{
    VFIOUBDevice *vdev = VFIO_UB(obj);

    g_free(vdev->emulated_config_bits);
    g_free(vdev->usi);
    g_free(vdev->usi_vectors);
}

static const TypeInfo vfio_ub_dev_info = {
    .name = TYPE_VFIO_UB,
    .parent = TYPE_UB_DEVICE,
    .instance_size = sizeof(VFIOUBDevice),
    .class_init = vfio_ub_dev_class_init,
    .instance_init = vfio_instance_init,
    .instance_finalize = vfio_instance_finalize,
};

static void register_vfio_ub_dev_types(void)
{
    type_register_static(&vfio_ub_dev_info);
}

type_init(register_vfio_ub_dev_types)
