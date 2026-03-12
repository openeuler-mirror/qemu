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
#include <sys/file.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "hw/arm/virt.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_ummu.h"
#include "hw/ub/ub_config.h"
#include "hw/ub/hisi/ubc.h"
#include "hw/ub/hisi/ub_mem.h"
#include "hw/ub/hisi/ub_fm.h"
#include "migration/vmstate.h"
#include "hw/ub/ubus_instance.h"

static uint64_t ub_msgq_reg_read(void *opaque, hwaddr addr, unsigned len)
{
    BusControllerState *s = opaque;
    uint64_t val;

    switch (len) {
    case BYTE_SIZE:
        val = ub_get_byte(s->msgq_reg + addr);
        break;
    case WORD_SIZE:
        val = ub_get_word(s->msgq_reg + addr);
        break;
    case DWORD_SIZE:
        val = ub_get_long(s->msgq_reg + addr);
        break;
    default:
        qemu_log("invalid argument len 0x%x\n", len);
        val = ~0x0;
        break;
    }

    return val;
}

static void ub_msgq_reg_write(void *opaque, hwaddr addr, uint64_t val, unsigned len)
{
    BusControllerState *s = opaque;

    switch (len) {
    case BYTE_SIZE:
        ub_set_byte(s->msgq_reg + addr, val);
        break;
    case WORD_SIZE:
        ub_set_word(s->msgq_reg + addr, val);
        break;
    case DWORD_SIZE:
        ub_set_long(s->msgq_reg + addr, val);
        break;
    default:
        /* As length is under guest control, handle illegal values. */
        qemu_log("invalid argument len 0x%x val 0x%lx\n", len, val);
        return;
    }

    /* only support 1 queue */
    switch (addr) {
    case SQ_PI:
        msgq_process_task(s, val);
        break;
    case SQ_ADDR_H:
        msgq_sq_init(s);
        break;
    case CQ_ADDR_H:
        msgq_cq_init(s);
        break;
    case RQ_ADDR_H:
        msgq_rq_init(s);
        break;
    case MSGQ_RST:
        msgq_handle_rst(s);
        break;
    default:
        break;
    }
}

static const MemoryRegionOps ub_msgq_reg_ops = {
    .read = ub_msgq_reg_read,
    .write = ub_msgq_reg_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps ub_fm_msgq_reg_ops = {
    .read = ub_fm_msgq_reg_read,
    .write = ub_fm_msgq_reg_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void ub_reg_alloc(DeviceState *dev)
{
    BusControllerState *s = BUS_CONTROLLER(dev);

    s->msgq_reg = g_malloc0(s->msgq_reg_size);
    s->fm_msgq_reg = g_malloc0(s->fm_msgq_reg_size);
    pthread_spin_init(&s->rq_cq_lock, PTHREAD_PROCESS_PRIVATE);
    qemu_log("alloc ub reg mem size: msgq_reg %u, "
             "fm_msgq_reg %u\n",
             s->msgq_reg_size, s->fm_msgq_reg_size);
}

static void ub_reg_free(DeviceState *dev)
{
    BusControllerState *s = BUS_CONTROLLER(dev);

    g_free(s->msgq_reg);
    g_free(s->fm_msgq_reg);
    pthread_spin_destroy(&s->rq_cq_lock);
    qemu_log("free ub reg mem\n");
}

static void ub_bus_controller_realize(DeviceState *dev, Error **errp)
{
    BusControllerState *s = BUS_CONTROLLER(dev);
    SysBusDevice *sysdev = SYS_BUS_DEVICE(dev);
    static uint8_t NO = 0;
    char *name = g_strdup_printf("ubus.%u", NO);

    sysdev->parent_obj.id = g_strdup_printf("ubc.%u", NO++);
    /* for msgq reg */
    memory_region_init_io(&s->msgq_reg_mem, OBJECT(s), &ub_msgq_reg_ops,
                          s, TYPE_BUS_CONTROLLER, s->msgq_reg_size);
    sysbus_init_mmio(sysdev, &s->msgq_reg_mem);
    /* for fm msgq reg */
    memory_region_init_io(&s->fm_msgq_reg_mem, OBJECT(s), &ub_fm_msgq_reg_ops,
                          s, TYPE_BUS_CONTROLLER, s->fm_msgq_reg_size);
    sysbus_init_mmio(sysdev, &s->fm_msgq_reg_mem);
    ub_reg_alloc(dev);
    /* for ub controller mmio */
    memory_region_init(&s->io_mmio, OBJECT(s), "UB_MMIO", UINT64_MAX);
    sysbus_init_mmio(sysdev, &s->io_mmio);

    s->bus = ub_register_root_bus(dev, name, &s->io_mmio);
    ub_save_ubc_list(s);
    g_free(name);
}

static void ub_bus_instance_guid_unlock(BusControllerDev *ubc_dev);
static void ub_bus_controller_unrealize(DeviceState *dev)
{
    BusControllerState *s = BUS_CONTROLLER(dev);
    SysBusDevice *sysdev = SYS_BUS_DEVICE(dev);
    g_free(sysdev->parent_obj.id);
    QLIST_REMOVE(s, node);
    ub_unregister_root_bus(s->bus);
    ub_reg_free(dev);
    ub_bus_instance_guid_unlock(s->ubc_dev);
}

static bool ub_bus_controller_needed(void *opaque)
{
    BusControllerState *s = opaque;
    return s->mig_enabled;
}

static Property ub_bus_controller_properties[] = {
    DEFINE_PROP_UINT32("ub-msgq-reg-size", BusControllerState,
                       msgq_reg_size, 0),
    DEFINE_PROP_UINT32("ub-fm-msgq-reg-size", BusControllerState,
                       fm_msgq_reg_size, 0),
    DEFINE_PROP_BOOL("ub-migration-enabled", BusControllerState,
                     mig_enabled, true),
    DEFINE_PROP_END_OF_LIST(),
};

const VMStateDescription vmstate_ub_bus_controller = {
    .name = TYPE_BUS_CONTROLLER,
    .needed = ub_bus_controller_needed,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        /* support migration later */
        VMSTATE_END_OF_LIST()
    }
};

const VMStateDescription vmstate_ub_bus_controller_dev = {
    .name = TYPE_BUS_CONTROLLER_DEV,
    .needed = ub_bus_controller_needed,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        /* support migration later */
        VMSTATE_END_OF_LIST()
    }
};

static void ub_bus_controller_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);

    device_class_set_props(dc, ub_bus_controller_properties);
    dc->realize = ub_bus_controller_realize;
    dc->unrealize = ub_bus_controller_unrealize;
    dc->vmsd = &vmstate_ub_bus_controller;
}

static void ub_bus_controller_instance_init(Object *obj)
{
    /* do nothing now */
}

static void ub_bus_controller_instance_finalize(Object *obj)
{
    /* do nothing now */
}
static const TypeInfo ub_bus_controller_type_info = {
    .name = TYPE_BUS_CONTROLLER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BusControllerState),
    .instance_init = ub_bus_controller_instance_init,
    .instance_finalize = ub_bus_controller_instance_finalize,
    .class_size = sizeof(BusControllerClass),
    .class_init = ub_bus_controller_class_init,
};

static void ub_bus_controller_cfg0_route_table_init(UBDevice *ub_dev)
{
    uint64_t emulated_offset = ub_cfg_offset_to_emulated_offset(UB_ROUTE_TABLE_START, true);
    UbRouteTable *route_table = (UbRouteTable *)(ub_dev->config + emulated_offset);

    /* The prerequisite is that each device uses only one port.
     * The Ub controller own a CNA, each port own a CNA, and each device own a CNA. */
    route_table->entry_num = UB_DEV_MAX_NUM_OF_PORT * 2 + 1;
    route_table->ers = 1;  /* support exact route */
}

static void ub_bus_controller_space_cfg0_init(UBDevice *ub_dev)
{
    UbCfg0Basic *cfg0_basic;
    Cfg0SupportFeature *support_feature;
    UbCfg0ShpCap *shp_cap;
    UbSlotInfo *slot_info;
    uint64_t emulated_offset;

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    cfg0_basic = (UbCfg0Basic *)(ub_dev->config + emulated_offset);
    cfg0_basic->header.slice_version = UB_SLICE_VERSION;
    cfg0_basic->header.slice_used_size = UB_CFG0_BASIC_SLICE_USED_SIZE;
    cfg0_basic->total_num_of_port = ub_dev->port.port_num & UINT16_MASK;
    cfg0_basic->total_num_of_ue = 1;
    cfg0_basic->cap_bitmap[CFG0_CAP2_SHP_INDEX / BITS_PER_BYTE] =
        1 << (CFG0_CAP2_SHP_INDEX % BITS_PER_BYTE);
    support_feature = &cfg0_basic->support_feature;
    support_feature->bits.entity_available = 1;
    support_feature->bits.mtu_supported = 1;
    support_feature->bits.route_table_supported = SUPPORTED;
    support_feature->bits.upi_supported = SUPPORTED;
    support_feature->bits.switch_supported = SUPPORTED;
    support_feature->bits.cc_supported = NOT_SUPPORTED;
    /* SHP CAP */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_CAP2_SHP_START, true);
    shp_cap = (UbCfg0ShpCap *)(ub_dev->config + emulated_offset);
    shp_cap->slot_num = 1;
    shp_cap->header.slice_version = UB_SLICE_VERSION;
    shp_cap->header.slice_used_size = (shp_cap->slot_num * sizeof(UbSlotInfo) + sizeof(UbCfg0ShpCap)) / DWORD_SIZE;
    for (int i = 0; i < shp_cap->slot_num; ++i) {
        slot_info = (UbSlotInfo *)((uint8_t *)shp_cap->slot_info + i * sizeof(UbSlotInfo));
        slot_info->start_port_idx = 0;
        slot_info->end_port_idx = cfg0_basic->total_num_of_port - 1;
        slot_info->pp_ctrl = 1;
        slot_info->ms_ctrl = 1;
        slot_info->pd_ctrl = 1;
        slot_info->pds_ctrl = 1;
    }
    ub_bus_controller_cfg0_route_table_init(ub_dev);
}

static void ub_bus_controller_space_cfg1_init(UBDevice *ub_dev)
{
    UbCfg1Basic *cfg1_basic;
    Cfg1SupportFeature *support_feature;
    UbCfg1DecoderCap *dec_cap;
    uint64_t emulated_offset;

    /* basic */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(ub_dev->config + emulated_offset);
    cfg1_basic->header.slice_version = UB_SLICE_VERSION;
    cfg1_basic->header.slice_used_size = UB_CFG1_BASIC_SLICE_USED_SIZE;

    cfg1_basic->cap_bitmap[CFG1_DECODER_CAP_INDEX / BITS_PER_BYTE] |=
        1 << (CFG1_DECODER_CAP_INDEX % BITS_PER_BYTE);

    support_feature = &cfg1_basic->support_feature;
    support_feature->bits.mgs = SUPPORTED;
    support_feature->bits.ubbas = NOT_SUPPORTED;
    support_feature->bits.ers0s = SUPPORTED;
    support_feature->bits.ers1s = NOT_SUPPORTED;
    support_feature->bits.ers2s = SUPPORTED;
    support_feature->bits.matt_juris = UB_DRIVE;
    cfg1_basic->ers_space_size[0] = UBC_ERS0_SPACE_SIZE;
    cfg1_basic->ers_space_size[1] = UBC_ERS1_SPACE_SIZE;
    cfg1_basic->ers_space_size[2] = UBC_ERS2_SPACE_SIZE;
    cfg1_basic->ers_start_addr[0] = UBC_ERS0_SPACE_ADDR;
    cfg1_basic->ers_start_addr[1] = UBC_ERS1_SPACE_ADDR;
    cfg1_basic->ers_start_addr[2] = UBC_ERS2_SPACE_ADDR;
    cfg1_basic->eid_upi_ten = UBC_EID_UPI_TEN_DEFAULT_VAL;
    cfg1_basic->class_code = UBC_CLASS_CODE;
    /* decoder cap */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP1_DECODER, true);
    dec_cap = (UbCfg1DecoderCap *)(ub_dev->config + emulated_offset);
    dec_cap->header.slice_version = UB_SLICE_VERSION;
    dec_cap->header.slice_used_size = sizeof(UbCfg1DecoderCap) / DWORD_SIZE;
    dec_cap->decoder.event_size_sup = DECODER_CAP_EVENT_SIZE;
    dec_cap->decoder.cmd_size_sup = DECODER_CAP_CMD_SIZE;
    dec_cap->decoder.mmio_size_sup = DECODER_CAP_MMIO_SIZE;
}

static void ub_bus_controller_wmask_init(UBDevice *ub_dev)
{
    UbCfg1DecoderCap *dec_cap_mask;
    UbCfg0ShpCap *cfg0_shp_wmask, *cfg0_shp;
    uint64_t emulated_offset;

    /* cfg0 cap */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_CAP2_SHP_START, true);
    cfg0_shp_wmask = (UbCfg0ShpCap *)(ub_dev->wmask + emulated_offset);
    cfg0_shp = (UbCfg0ShpCap *)(ub_dev->config + emulated_offset);
    memset(cfg0_shp_wmask, 0, UB_SLICE_SZ);
    for (int i = 0; i < cfg0_shp->slot_num; ++i) {
        cfg0_shp_wmask->slot_info[i].pp_ctrl = ~0;
        cfg0_shp_wmask->slot_info[i].wl_ctrl = ~0;
        cfg0_shp_wmask->slot_info[i].pl_ctrl = ~0;
        cfg0_shp_wmask->slot_info[i].ms_ctrl = ~0;
        cfg0_shp_wmask->slot_info[i].pd_ctrl = ~0;
        cfg0_shp_wmask->slot_info[i].pds_ctrl = ~0;
        cfg0_shp_wmask->slot_info[i].pw_ctrl = ~0;
    }

    /* cfg1 cap */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP1_DECODER, true);
    dec_cap_mask = (UbCfg1DecoderCap *)(ub_dev->wmask + emulated_offset);
    memset(dec_cap_mask, 0, sizeof(UbCfg1DecoderCap));
    dec_cap_mask->decoder_ctrl.decoder_en = ~0;
    memset(&dec_cap_mask->dec_matt_ba, 0xff, sizeof(dec_cap_mask->dec_matt_ba));
    memset(&dec_cap_mask->dec_mmio_ba, 0xff, sizeof(dec_cap_mask->dec_mmio_ba));
    dec_cap_mask->decoder_cmdq_cfg.cmdq_en = ~0;
    dec_cap_mask->decoder_cmdq_cfg.cmdq_size_use = ~0;
    dec_cap_mask->decoder_cmdq_prod.cmdq_wr_idx = ~0;
    dec_cap_mask->decoder_cmdq_prod.cmdq_err_resp = ~0;
    dec_cap_mask->decoder_cmdq_cons.cmdq_rd_idx = ~0;
    dec_cap_mask->decoder_cmdq_cons.cmdq_err = ~0;
    dec_cap_mask->decoder_cmdq_cons.cmdq_err_res = ~0;
    dec_cap_mask->decoder_cmdq_ba.cmdq_ba = ~0;
    dec_cap_mask->decoder_evtq_cfg.evtq_en = ~0;
    dec_cap_mask->decoder_evtq_cfg.evtq_size_use = ~0;
    dec_cap_mask->decoder_evtq_prod.evtq_wr_idx = ~0;
    dec_cap_mask->decoder_evtq_cons.evtq_rd_idx = ~0;
    dec_cap_mask->decoder_evtq_ba.evtq_ba = ~0;
}

static void ub_bus_controller_w1cmask_init(UBDevice *ub_dev)
{
    UbCfg0ShpCap *cfg0_shp_w1cmask, *cfg0_shp;
    uint64_t emulated_offset;

    /* cfg0 cap */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_CAP2_SHP_START, true);
    cfg0_shp_w1cmask = (UbCfg0ShpCap *)(ub_dev->w1cmask + emulated_offset);
    cfg0_shp = (UbCfg0ShpCap *)(ub_dev->config + emulated_offset);
    memset(cfg0_shp_w1cmask, 0, UB_SLICE_SZ);
    for (int i = 0; i < cfg0_shp->slot_num; ++i) {
        cfg0_shp_w1cmask->slot_info[i].pp_st = ~0;
        cfg0_shp_w1cmask->slot_info[i].pdsc_st = ~0;
    }
}

static void ub_bus_controller_dev_config_space_init(UBDevice *dev)
{
    ub_bus_controller_space_cfg0_init(dev);
    ub_bus_controller_space_cfg1_init(dev);
    ub_bus_controller_wmask_init(dev);
    ub_bus_controller_w1cmask_init(dev);
}

static bool ub_ubc_is_empty(UBBus *bus)
{
    UBDevice *dev;
    QLIST_FOREACH(dev, &bus->devices, node) {
        if (dev->dev_type == UB_TYPE_IBUS_CONTROLLER) {
            return false;
        }
    }
    return true;
}

#define UB_BUSINSTANCE_GUID_LOCK_DIR "/run/libvirt/qemu"

static int ub_bus_instance_guid_lock(UbGuid *guid)
{
    char path[256] = {0};
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    int lock_fd;

    ub_device_get_str_from_guid(guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);
    snprintf(path, sizeof(path), "%s/ub-bus-instance-%s.lock",  UB_BUSINSTANCE_GUID_LOCK_DIR, guid_str);
    lock_fd = open(path, O_RDONLY | O_CREAT, 0600);
    if (lock_fd < 0) {
        qemu_log("failed to open lock file %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (flock(lock_fd, LOCK_EX | LOCK_NB)) {
        qemu_log("lock %s failed: %s\n", path, strerror(errno));
        close(lock_fd);
        return -1;
    }

    return lock_fd;
}

static void ub_bus_instance_guid_unlock(BusControllerDev *ubc_dev)
{
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};

    if (ubc_dev->bus_instance_lock_fd <= 0) {
        return;
    }

    ub_device_get_str_from_guid(&ubc_dev->bus_instance_guid, guid_str,
                                UB_DEV_GUID_STRING_LENGTH + 1);
    qemu_log("unlock ub bus instance lock for guid: %s\n", guid_str);
    if (flock(ubc_dev->bus_instance_lock_fd, LOCK_UN)) {
        qemu_log("failed to unlock for bus instance guid %s: %s\n",
                 guid_str, strerror(errno));
    }
    close(ubc_dev->bus_instance_lock_fd);
}

static int ub_bus_instance_process(BusControllerDev *ubc_dev, Error **errp)
{
    int lock_fd;

    if (ub_guid_is_none(&ubc_dev->bus_instance_guid)) {
        error_setg(errp, "ubc bus instance guid is required");
        return -1;
    }

    lock_fd = ub_bus_instance_guid_lock(&ubc_dev->bus_instance_guid);
    if (lock_fd < 0) {
        error_setg(errp, "ubc bus instance guid lock failed, it may used by other vm");
        return -1;
    }

    ubc_dev->bus_instance_lock_fd = lock_fd;
    return 0;
}

static void ub_bus_controller_dev_realize(UBDevice *dev, Error **errp)
{
    UBBus *bus = UB_BUS(qdev_get_parent_bus(DEVICE(dev)));
    BusControllerState *ubc = container_of_ubbus(bus);
    VirtMachineState *vms = VIRT_MACHINE(qdev_get_machine());

    vms->ub_bus = bus;

    if (!ub_ubc_is_empty(bus)) {
        qemu_log("ubc realize repetitively\n");
        error_setg(errp, "ubc realize repetitively");
        return;
    }

    ubc->ubc_dev = BUS_CONTROLLER_DEV(dev);
    if (dev->guid.type != UB_GUID_TYPE_IBUS_CONTROLLER) {
        qemu_log("%s device type set error, expect: %u, actual: %u\n",
                 dev->qdev.id, UB_GUID_TYPE_IBUS_CONTROLLER, dev->guid.type);
        error_setg(errp, "%s device type set error, expect: %u, actual: %u\n",
                   dev->qdev.id, UB_GUID_TYPE_IBUS_CONTROLLER, dev->guid.type);
        return;
    }

    dev->dev_type = UB_TYPE_IBUS_CONTROLLER;
    ub_bus_controller_dev_config_space_init(dev);
    if (0 > ummu_associating_with_ubc(ubc)) {
        qemu_log("failed to associating ubc with ummu. %s\n", dev->name);
    }

    qemu_log("set type UB_TYPE_CONTROLLER, ubc %p, "
             "ubc->ubc_dev %p, bus %p\n", ubc, ubc->ubc_dev, bus);
    if (ub_bus_instance_process(ubc->ubc_dev, errp)) {
        qemu_log("ub bus instance process failed\n");
        return;
    }
}

static Property ub_bus_controller_dev_properties[] = {
    DEFINE_PROP_UB_DEV_GUID("bus_instance_guid", BusControllerDev, bus_instance_guid),
    DEFINE_PROP_END_OF_LIST(),
};

static void ub_bus_controller_dev_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    UBDeviceClass *uc = UB_DEVICE_CLASS(class);

    device_class_set_props(dc, ub_bus_controller_dev_properties);
    uc->realize = ub_bus_controller_dev_realize;
    dc->vmsd = &vmstate_ub_bus_controller_dev;
}

static const TypeInfo ub_bus_controller_dev_type_info = {
    .name = TYPE_BUS_CONTROLLER_DEV,
    .parent = TYPE_UB_DEVICE,
    .instance_size = sizeof(BusControllerDev),
    .class_size = sizeof(BusControllerDevClass),
    .class_init = ub_bus_controller_dev_class_init,
};

static void ub_bus_controller_register_types(void)
{
    type_register_static(&ub_bus_controller_type_info);
    type_register_static(&ub_bus_controller_dev_type_info);
}
type_init(ub_bus_controller_register_types)
