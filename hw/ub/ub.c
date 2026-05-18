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
#include "qemu/module.h"
#include "qemu/cutils.h"
#include "qemu/range.h"
#include "qemu/bitmap.h"
#include "hw/arm/virt.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/ub/ub_common.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_config.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_ummu.h"
#include "hw/ub/ub_usi.h"
#include "hw/ub/ub_acpi.h"
#include "hw/vfio/ub.h"
#include "ub_ummu_internal.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "migration/vmstate.h"
#include "qapi/qapi-commands-ub.h"
#include "qapi/error.h"
#include "qapi/util.h"
#include "qapi/qmp/qstring.h"
#include "exec/address-spaces.h"
#include "hw/ub/ubus_instance.h"
#include "monitor/monitor.h"
#include "trace.h"

QLIST_HEAD(, BusControllerState) ub_bus_controllers;
static void ub_update_mappings(UBDevice *dev);

static void ubbus_dev_print(Monitor *mon, DeviceState *dev, int indent)
{
    UBDevice *udev = (UBDevice *)dev;
    uint64_t offset0 = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    uint64_t offset1 = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    UbCfg0Basic *cfg0 = (UbCfg0Basic *)(udev->config + offset0);
    UbCfg1Basic *cfg1 = (UbCfg1Basic *)(udev->config + offset1);
    UBIORegion *r;
    int i;

    monitor_printf(mon, "%*sGUID:vendor 0x%x Class 0x%x Type 0x%x "
                   "DevId 0x%x Ver 0x%x SN 0x%lx\n",
                   indent, "", cfg0->guid.vendor, cfg1->class_code,
                   cfg0->guid.type, cfg0->guid.device_id, cfg0->guid.version,
                   (unsigned long)cfg0->guid.seq_num);
    for (i = 0; i < UB_NUM_REGIONS; i++) {
        r = &udev->io_regions[i];
        if (!r->size) {
            continue;
        }
        monitor_printf(mon, "%*sers %d: mem at 0x%"PRIx64
                       " [0x%"PRIx64"]\n",
                       indent, "", i,
                       r->addr, r->addr + r->size - 1);
    }
}

static char *ubbus_get_dev_path(DeviceState *dev)
{
    UBDevice *udev = (UBDevice *)dev;
    uint64_t offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    UbCfg0Basic *cfg0 = (UbCfg0Basic *)(udev->config + offset);
    char *path = g_malloc(UB_DEV_GUID_STRING_LENGTH + 1);

    ub_device_get_str_from_guid(&cfg0->guid, path, UB_DEV_GUID_STRING_LENGTH + 1);

    return path;
}

static char *ubbus_get_fw_dev_path(DeviceState *dev)
{
    return NULL;
}

static const VMStateDescription vmstate_ubbus = {
    .name = TYPE_UB_BUS,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static void ub_bus_realize(BusState *qbus, Error **errp)
{
    UBBus *bus = UB_BUS(qbus);

    vmstate_register(NULL, VMSTATE_INSTANCE_ID_ANY, &vmstate_ubbus, bus);
}

void ub_save_ubc_list(BusControllerState *s)
{
    QLIST_INSERT_HEAD(&ub_bus_controllers, s, node);
}

static void ub_bus_unrealize(BusState *qbus)
{
    UBBus *bus = UB_BUS(qbus);

    vmstate_unregister(NULL, &vmstate_ubbus, bus);
}

static void ub_dev_clear_cfg0(UBDevice *dev)
{
    UbCfg0Basic *cfg0_basic;
    uint64_t offset;

    /* emulated feilds in cfg0 basic */
    offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    cfg0_basic = (UbCfg0Basic *)(dev->config + offset);
    memset(&cfg0_basic->eid, 0, sizeof(cfg0_basic->eid));
    memset(&cfg0_basic->fm_eid, 0, sizeof(cfg0_basic->fm_eid));
    memset(&cfg0_basic->net_addr_info, 0,
           sizeof(cfg0_basic->net_addr_info));
    cfg0_basic->upi = 0;
    cfg0_basic->mtu_cfg = 0;
    cfg0_basic->dev_rst = 0;
    cfg0_basic->th_en = 0;
    cfg0_basic->cc_en = 0;
    cfg0_basic->ueid_low = 0;
    cfg0_basic->ueid_high = 0;
    cfg0_basic->ucna = 0;
    cfg0_basic->fm_cna = 0;
}

static void ub_dev_clear_cfg1(UBDevice *dev)
{
    UbCfg1Basic *cfg1_basic;
    uint64_t offset;

    offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(dev->config + offset);
    cfg1_basic->elr = 0;
    cfg1_basic->elr_done = 0;
    cfg1_basic->sys_pgs = 0;
    cfg1_basic->eid_upi_tab = 0;
    cfg1_basic->dev_token_id = 0;
    cfg1_basic->bus_access_en = 0;
    cfg1_basic->dev_rs_access_en = 0;
}

static void ub_reset_regions(UBDevice *dev)
{
    UbCfg1Basic *cfg1_basic;
    uint64_t offset;
    int i;

    offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(dev->config + offset);

    for (i = 0; i < UB_NUM_REGIONS; i++) {
        cfg1_basic->ers_ubba[i] = UB_ER_UNMAPPED;
    }
    qemu_log("ub device(%s %s) clear ubba\n",
             dev->name, dev->qdev.id);
}

static void ub_do_device_reset(UBDevice *dev)
{
    /* ubba of idev is allocated by virtualization not by driver */
    if (dev->dev_type != UB_TYPE_IDEVICE) {
        ub_reset_regions(dev);
        ub_update_mappings(dev);
    }
    ub_dev_clear_cfg0(dev);
    ub_dev_clear_cfg1(dev);
    usi_reset(dev);
    dev->rst_cnt++;
}

static void ubbus_reset(BusState *qbus)
{
    UBBus *bus = DO_UPCAST(UBBus, qbus, qbus);
    UBDevice *dev;

    QLIST_FOREACH(dev, &bus->devices, node) {
        if (dev->dev_type != UB_TYPE_DEVICE && dev->dev_type != UB_TYPE_IDEVICE) {
            continue;
        }
        qemu_log("ub device(%s %s) ub_do_device_reset\n",
                 dev->name, dev->qdev.id);
        ub_do_device_reset(dev);
    }
}

UBBus *ub_register_root_bus(DeviceState *parent, const char *name,
                            MemoryRegion *io_mmio)
{
    UBBus *bus;

    bus = UB_BUS(qbus_new(TYPE_UB_BUS, parent, name));
    bus->address_space_mem = io_mmio;

    return bus;
}

void ub_unregister_root_bus(UBBus *bus)
{
    qbus_unrealize(BUS(bus));
}

static void ub_bus_class_init(ObjectClass *klass, void *data)
{
    BusClass *k = BUS_CLASS(klass);

    k->print_dev = ubbus_dev_print;
    k->get_dev_path = ubbus_get_dev_path;
    k->get_fw_dev_path = ubbus_get_fw_dev_path;
    k->realize = ub_bus_realize;
    k->unrealize = ub_bus_unrealize;
    k->reset = ubbus_reset;
}

static const TypeInfo ub_bus_info = {
    .name = TYPE_UB_BUS,
    .parent = TYPE_BUS,
    .instance_size = sizeof(UBBus),
    .class_size = sizeof(UBBusClass),
    .class_init = ub_bus_class_init,
};

static void ub_config_alloc(UBDevice *ub_dev)
{
    size_t config_size = ub_emulated_config_size();
    ub_dev->config = g_malloc0(config_size);
    ub_dev->wmask = g_malloc0(config_size);
    ub_dev->w1cmask = g_malloc0(config_size);
}

static void ub_port_info_alloc(UBDevice *ub_dev)
{
    ub_dev->port.neighbors = g_malloc0(sizeof(NeighborInfo) *
                                   ub_dev->port.port_num);
    ub_dev->port.port_info_exist = false;
}

static void ub_config_free(UBDevice *ub_dev)
{
    g_free(ub_dev->config);
    g_free(ub_dev->wmask);
    g_free(ub_dev->w1cmask);
}

static void ub_port_info_free(UBDevice *ub_dev)
{
    if (ub_dev->port.neighbors_cmd) {
        g_free(ub_dev->port.neighbors_cmd);
    }
    if (ub_dev->port.neighbors) {
        g_free(ub_dev->port.neighbors);
    }
}

static void ub_config_set_guid(UBDevice *ub_dev)
{
    uint64_t offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_GUID_START, true);
    uint8_t *ub_config_guid_ptr = ub_dev->config + offset;
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};

    ub_device_get_str_from_guid(&ub_dev->guid, guid_str,
                                UB_DEV_GUID_STRING_LENGTH + 1);
    memcpy(ub_config_guid_ptr, &ub_dev->guid, sizeof(UbGuid));
}

static void ub_init_wmask(UBDevice *ub_dev)
{
    UbCfg0Basic *cfg0_basic_wmask;
    UbCfg0EmqCap *cfg0_emq_cap_wmask;
    UbCfg1Basic *cfg1_basic_wmask;
    UbCfg1IntType1Cap *cfg1_int_type1_wmask;
    UbCfg1IntType2Cap *cfg1_int_type2_wmask;
    UbRouteTable *route_table_wmask;
    uint64_t emulated_offset;

    /* cfg0 basic */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    cfg0_basic_wmask = (UbCfg0Basic *)(ub_dev->wmask + emulated_offset);
    memset(cfg0_basic_wmask, 0, sizeof(UbCfg0Basic));
    memset(&cfg0_basic_wmask->eid, 0xff, sizeof(UbEid));
    memset(&cfg0_basic_wmask->fm_eid, 0xff, sizeof(UbEid));
    cfg0_basic_wmask->net_addr_info.primary_cna = 0xffffff;
    cfg0_basic_wmask->upi = ~0;
    cfg0_basic_wmask->mtu_cfg = ~0;
    cfg0_basic_wmask->cc_en = ~0;
    cfg0_basic_wmask->th_en = ~0;
    cfg0_basic_wmask->fm_cna = ~0;
    cfg0_basic_wmask->ueid_low = ~0UL;
    cfg0_basic_wmask->ueid_high = ~0UL;
    cfg0_basic_wmask->ucna = ~0;

    /* cfg0 emq cap */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_EMQ_CAP_START, true);
    cfg0_emq_cap_wmask = (UbCfg0EmqCap *)(ub_dev->wmask + emulated_offset);
    memset(cfg0_emq_cap_wmask, 0, sizeof(UbCfg0EmqCap));
    cfg0_emq_cap_wmask->error_msg_que_ctrlr.correctable_err_report_enable = ~0;
    cfg0_emq_cap_wmask->error_msg_que_ctrlr.uncorrectable_nonfatal_err_report_enable = ~0;
    cfg0_emq_cap_wmask->error_msg_que_ctrlr.uncorrectable_fatal_err_report_enable = ~0;

    /* cfg1 basic */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic_wmask = (UbCfg1Basic *)(ub_dev->wmask + emulated_offset);
    memset(cfg1_basic_wmask, 0, sizeof(UbCfg1Basic));
    cfg1_basic_wmask->elr = ~0;
    cfg1_basic_wmask->sys_pgs = ~0;
    cfg1_basic_wmask->eid_upi_tab = ~0UL;
    cfg1_basic_wmask->dev_token_id = ~0;
    cfg1_basic_wmask->bus_access_en = ~0;
    cfg1_basic_wmask->dev_rs_access_en = ~0;

    /* cfg1 int type1 cap */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP3_INT_TYPE1, true);
    cfg1_int_type1_wmask = (UbCfg1IntType1Cap *)(ub_dev->wmask + emulated_offset);
    memset(cfg1_int_type1_wmask, 0, sizeof(UbCfg1IntType1Cap));
    cfg1_int_type1_wmask->interrupt_enable = ~0;
    cfg1_int_type1_wmask->interrupt_enable_num = ~0;
    cfg1_int_type1_wmask->interrupt_data = ~0U;
    cfg1_int_type1_wmask->interrupt_address = ~0UL;
    cfg1_int_type1_wmask->interrupt_id = ~0U;
    cfg1_int_type1_wmask->interrupt_mask = ~0U;

    /* cfg1 int type2 cap */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2, true);
    cfg1_int_type2_wmask = (UbCfg1IntType2Cap *)(ub_dev->wmask + emulated_offset);
    memset(cfg1_int_type2_wmask, 0, sizeof(UbCfg1IntType2Cap));
    cfg1_int_type2_wmask->interrupt_id = ~0U;
    cfg1_int_type2_wmask->interrupt_mask = ~0;
    cfg1_int_type2_wmask->interrupt_enable = ~0;

    /* route table */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_ROUTE_TABLE_START, true);
    route_table_wmask = (UbRouteTable *)(ub_dev->wmask + emulated_offset);
    memset(route_table_wmask, 0xff, UB_CFG_SLICE_SIZE);
    route_table_wmask->entry_num = 0;
    route_table_wmask->ers = 0;
}

static void ub_init_w1cmask(UBDevice *ub_dev)
{
    UbCfg0Basic *cfg0_basic_w1cmask;
    UbCfg1Basic *cfg1_basic_w1cmask;
    uint64_t emulated_offset;

    /* cfg0 basic */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    cfg0_basic_w1cmask = (UbCfg0Basic *)(ub_dev->w1cmask + emulated_offset);
    memset(cfg0_basic_w1cmask, 0, sizeof(UbCfg0Basic));
    cfg0_basic_w1cmask->dev_rst = ~0;

    /* cfg1 basic */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic_w1cmask = (UbCfg1Basic *)(ub_dev->w1cmask + emulated_offset);
    memset(cfg1_basic_w1cmask, 0, sizeof(UbCfg1Basic));
    cfg1_basic_w1cmask->elr = ~0;

    /* port cap */
    // not support yet
}

static void ub_config_space_init(UBDevice *ub_dev)
{
    ub_config_set_guid(ub_dev);
    ub_init_wmask(ub_dev);
    ub_init_w1cmask(ub_dev);
}

int ub_default_read_config(UBDevice *dev, uint64_t offset,
                           uint32_t *val, uint32_t dw_mask)
{
    uint32_t read_data;
    uint64_t emulated_offset = ub_cfg_offset_to_emulated_offset(offset, false);

    if (emulated_offset == UINT64_MAX) {
        *val = 0;
        qemu_log("ub default read config out of emulated range, offset "
                 "is 0x%lx\n", offset);
        return EFAULT;
    }

    memcpy(&read_data, dev->config + emulated_offset, DWORD_SIZE);
    *val = read_data & dw_mask;
    return 0;
}

static uint64_t ub_er_address(UBDevice *dev, uint8_t ers, uint64_t size)
{
    uint64_t new_addr, last_addr;
    UbCfg1Basic *cfg1_basic;
    uint64_t emulated_offset;

    if (ers >= UB_NUM_REGIONS) {
        qemu_log("invalid ers %u\n", ers);
        return UB_ER_UNMAPPED;
    }

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(dev->config + emulated_offset);
    if (!cfg1_basic->dev_rs_access_en) {
        return UB_ER_UNMAPPED;
    }

    new_addr = cfg1_basic->ers_ubba[ers];
    last_addr = new_addr + size - 1;
    /* NOTE: we do not support wrapping */
    if (last_addr <= new_addr || last_addr == UB_ER_UNMAPPED) {
        return UB_ER_UNMAPPED;
    }

    return new_addr;
}

static void ub_update_mappings(UBDevice *dev)
{
    UBIORegion *region;
    uint64_t new_addr;
    uint8_t i;

    for (i = 0; i < UB_NUM_REGIONS; i++) {
        region = &dev->io_regions[i];

        /* this region isn't registered */
        if (!region->size) {
            continue;
        }

        new_addr = ub_er_address(dev, i, region->size);
        trace_ub_update_mappings(i, region->size, region->addr, new_addr);

        /* This ers isn't changed */
        if (new_addr == region->addr) {
            continue;
        }

        if (region->addr != UB_ER_UNMAPPED) {
            memory_region_del_subregion(region->address_space, region->memory);
        }

        region->addr = new_addr;
        if (region->addr != UB_ER_UNMAPPED) {
            trace_ub_update_mappings_add(region->addr);
            memory_region_add_subregion_overlap(region->address_space,
                                                region->addr, region->memory, 1);
        }
    }
}

int ub_default_write_config(UBDevice *dev, uint64_t offset,
                            uint32_t *val, uint32_t dw_mask)
{
    uint32_t write_data = *val;
    uint32_t dw_wmask, dw_w1cmask;
    uint64_t emulated_offset;
    uint32_t *dst_data = NULL;

    emulated_offset = ub_cfg_offset_to_emulated_offset(offset, false);
    if (emulated_offset == UINT64_MAX) {
        qemu_log("ub default write config out of emulated range, offset "
                 "is 0x%lx\n", offset);
        return EFAULT;
    }

    dst_data = (uint32_t *)(dev->config + emulated_offset);
    dw_wmask = *(uint32_t *)(dev->wmask + emulated_offset) & dw_mask;
    dw_w1cmask = *(uint32_t *)(dev->w1cmask + emulated_offset) & dw_mask;
    *dst_data = (*dst_data & ~dw_wmask) | (write_data & dw_wmask);
    *dst_data &= ~(write_data & dw_w1cmask);

    if (ranges_overlap(offset, DWORD_SIZE,
        UB_CFG1_BASIC_START + offsetof(UbCfg1Basic, ers_ubba),
        UB_NUM_REGIONS * sizeof(uint64_t)) && write_data != UINT32_MAX) {
        ub_update_mappings(dev);
    }

    /* for idev update mapping */
    if (ranges_overlap(offset, DWORD_SIZE, UB_CFG1_DEV_RS_ACCESS_EN_OFFSET, DWORD_SIZE)) {
        ub_update_mappings(dev);
    }
    return 0;
}

static UBDevice *do_ub_register_device(UBDevice *ub_dev, const char *name, Error **errp)
{
    UBBus *bus = ub_get_bus(ub_dev);
    UBDeviceClass *uc = UB_DEVICE_GET_CLASS(ub_dev);
    UBConfigReadFunc *config_read = uc->config_read;
    UBConfigWriteFunc *config_write = uc->config_write;

    if (ub_dev->eid < UB_SUPPORT_MIN_EID || ub_dev->eid > UB_SUPPORT_MAX_EID) {
        qemu_log("expect eid val is [0x%x, 0x%x], but current eid val is 0x%x\n",
                 UB_SUPPORT_MIN_EID, UB_SUPPORT_MAX_EID, ub_dev->eid);
        error_setg(errp, "expect eid val is [0x%x, 0x%x], but current eid val is 0x%x\n",
                   UB_SUPPORT_MIN_EID, UB_SUPPORT_MAX_EID, ub_dev->eid);
        return NULL;
    }
    if (ub_find_device_by_guid(&ub_dev->guid)) {
        qemu_log("%s guid already exists.\n", ub_dev->qdev.id);
        error_setg(errp, "%s guid already exists.\n", ub_dev->qdev.id);
        return NULL;
    }
    if (ub_find_device_by_eid(bus, ub_dev->eid)) {
        qemu_log("%s eid already exists.\n", ub_dev->qdev.id);
        error_setg(errp, "%s eid already exists.\n", ub_dev->qdev.id);
        return NULL;
    }
    pstrcpy(ub_dev->name, sizeof(ub_dev->name), name);
    QLIST_INSERT_HEAD(&bus->devices, ub_dev, node);

    /* allocate memory for ub device config space */
    ub_config_alloc(ub_dev);
    ub_config_space_init(ub_dev);
    /* allocate memory for ub device port info */
    ub_port_info_alloc(ub_dev);

    if (!config_read)
        config_read = ub_default_read_config;
    if (!config_write)
        config_write = ub_default_write_config;
    ub_dev->config_read = config_read;
    ub_dev->config_write = config_write;

    return ub_dev;
}

static void do_ub_unregister_device(UBDevice *ub_dev)
{
    ub_config_free(ub_dev);
    ub_port_info_free(ub_dev);
}

static uint32_t ub_get_host_bus_instance_eid(UbGuid *guid)
{
    uint32_t bus_instance_eid;
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    int bus_instance_type;

    ub_device_get_str_from_guid(guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);
    bus_instance_eid = sysfs_get_bus_instance_eid_by_guid(guid);
    if (bus_instance_eid == UINT32_MAX) {
        qemu_log("sysfs failed to get bus instance eid by guid %s\n", guid_str);
        return UINT32_MAX;
    }

    bus_instance_type = sysfs_get_bus_instance_type_by_eid(bus_instance_eid);
    if (!UBUS_INSTANCE_IS_DYNAMIC(bus_instance_type)) {
        qemu_log("bus instance(guid: %s) not dynamic bus instance.\n", guid_str);
        return UINT32_MAX;
    }

    return bus_instance_eid;
}

/* current this just for vfio ub dev host bus instance verify */
static int ub_dev_bus_instance_verify(UBDevice *dev, Error **errp)
{
    BusControllerState *ubc = QLIST_FIRST(&ub_bus_controllers);
    BusControllerDev *ub_bus_controller_dev = NULL;
    UBDevice *ubc_dev = NULL;
    uint32_t bus_instance_eid;
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};

    if (!ubc) {
        qemu_log("failed to get ub bus controller, bus instance verify later.\n");
        return 0;
    }

    ub_bus_controller_dev = ubc->ubc_dev;

    if (!ub_bus_controller_dev) {
        qemu_log("ub controller dev not realized, bus instance verify later.\n");
        return 0;
    }

    ubc_dev = &ub_bus_controller_dev->parent;

    if (ubc_dev->bus_instance_eid == UINT32_MAX) {
        bus_instance_eid = ub_get_host_bus_instance_eid(&ub_bus_controller_dev->bus_instance_guid);
        if (bus_instance_eid == UINT32_MAX) {
            error_setg(errp, "failed to get bus instance eid.\n");
            return -1;
        }
        ubc_dev->bus_instance_eid = bus_instance_eid;
    }

    if (ubc_dev->bus_instance_eid != dev->bus_instance_eid) {
        ub_device_get_str_from_guid(&dev->guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);
        error_setg(errp, "ub dev(guid: %s) bus instance eid verify failed: expect 0x%x, actual 0x%x\n",
                   guid_str, ubc_dev->bus_instance_eid, dev->bus_instance_eid);
        return -1;
    }

    return 0;
}

static void ub_qdev_realize(DeviceState *qdev, Error **errp)
{
    UBDevice *ub_dev = (UBDevice *)qdev;
    UBDeviceClass *uc = UB_DEVICE_GET_CLASS(ub_dev);
    Error *local_err = NULL;

    ub_dev->dev_type = UB_TYPE_UNINIT;
    ub_dev->bus_instance_eid = UINT32_MAX;
    ub_dev->rst_cnt = 0;
    ub_dev->host_dev = false;
    ub_dev = do_ub_register_device(ub_dev,
                                   object_get_typename(OBJECT(qdev)), errp);
    if (ub_dev == NULL) {
        return;
    }

    ub_dev->bus_instance_verify = ub_dev_bus_instance_verify;
    if (uc->realize) {
        uc->realize(ub_dev, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            do_ub_unregister_device(ub_dev);
            return;
        }
    }
}

static void ub_unregister_io_regions(UBDevice *ub_dev)
{
    UBIORegion *r;
    int i;

    for (i = 0; i < UB_NUM_REGIONS; i++) {
        r = &ub_dev->io_regions[i];
        if (!r->size || r->addr == UB_ER_UNMAPPED)
            continue;
        memory_region_del_subregion(r->address_space, r->memory);
    }
}

static void ub_qdev_unrealize(DeviceState *dev)
{
    UBDevice *ub_dev = UB_DEVICE(dev);

    ub_unregister_io_regions(ub_dev);
    do_ub_unregister_device(ub_dev);
}

#define DECLARE_PORT_INFO(n) \
    DEFINE_PROP_UB_DEV_NEIGHBOR_INFO("port"#n, UBDevice, port),
static Property ub_props[] = {
    DEFINE_PROP_UINT32("eid", UBDevice, eid, 0),
    DEFINE_PROP_UB_DEV_GUID("guid", UBDevice, guid),
    DEFINE_PROP_UB_DEV_PORT_NUM("portnum", UBDevice, port),
    /* max port num UB_DEV_MAX_NUM_OF_PORT(256)
     * port id start with 0, so here set 255
     */
    LOOP(DECLARE_PORT_INFO, 255)
    DEFINE_PROP_END_OF_LIST()
};

UBDevice *ub_find_device_by_eid(UBBus *bus, uint32_t eid)
{
    UBDevice *dev;

    QLIST_FOREACH(dev, &bus->devices, node) {
        if (dev->eid == eid) {
            return dev;
        }
    }

    return NULL;
}

static void ub_device_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    k->realize = ub_qdev_realize;
    k->unrealize = ub_qdev_unrealize;
    k->bus_type = TYPE_UB_BUS;
    device_class_set_props(k, ub_props);
}

static const TypeInfo ub_device_type_info = {
    .name = TYPE_UB_DEVICE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(UBDevice),
    .abstract = true,
    .class_size = sizeof(UBDeviceClass),
    .class_init = ub_device_class_init,
};

static void ub_register_types(void)
{
    type_register_static(&ub_bus_info);
    type_register_static(&ub_device_type_info);
}

type_init(ub_register_types)

/* guid format:
 * vendor:device id:version:type:rsv:sequence number
 * 16     16        4       4    24  64 (bits)
 */
void ub_device_get_str_from_guid(UbGuid *guid, char *guid_str, uint32_t str_len)
{
    uint32_t len = UB_DEV_GUID_STRING_LENGTH + 1;
    int ret;

    if (str_len < UB_DEV_GUID_STRING_LENGTH + 1) {
        qemu_log("expect str_len(%u) < guid_str_len(%u), guid "
                 "to str will be truncate.\n", str_len, UB_DEV_GUID_STRING_LENGTH + 1);
        len = str_len;
    }
    ret = snprintf(guid_str, len, "%04x-%04x-%01x-%01x-%06x-%016lx",
                   guid->vendor, guid->device_id,
                   guid->version, guid->type, guid->rsv,
                   (guid->seq_num & 0xFFFFFFFFFFFFFFFF));
    if (ret < 0) {
        qemu_log("get str from ub device guid fail.\n");
    }
}

#define UB_GUID_ELEMENT_NUM 6
bool ub_device_get_guid_from_str(UbGuid *guid, char *guid_str)
{
    unsigned long seq_num;
    unsigned int device_id;
    unsigned int version;
    unsigned int type;
    unsigned int vendor;
    unsigned int rsv;
    int ret;

    if (strlen(guid_str) != UB_DEV_GUID_STRING_LENGTH) {
        qemu_log("expect guid len is %d, but current guid len is %ld\n",
                 UB_DEV_GUID_STRING_LENGTH, strlen(guid_str));
        return false;
    }

    ret = sscanf(guid_str, "%04x-%04x-%01x-%01x-%06x-%016lx",
                 &vendor, &device_id, &version, &type, &rsv, &seq_num);
    if (ret != UB_GUID_ELEMENT_NUM) {
        qemu_log("guid format is incorrect, example: " GUID_STR_EXAMPLE "\n");
        return false;
    }
    guid->vendor = vendor & 0xFFFF;
    guid->type = type & 0x0F;
    guid->version = version & 0x0F;
    guid->device_id = device_id & 0xFFFF;
    guid->rsv = rsv & 0xFFFFFF;
    guid->seq_num = seq_num & 0xFFFFFFFFFFFFFFFF;
    return true;
}

static UBDeviceInfo *qmp_query_ub_device(UBDevice *dev)
{
    UBDeviceInfo *info;
    /* will be freed by qmp framework */
    char *guid_str = g_malloc0(UB_DEV_GUID_STRING_LENGTH + 1);

    info = g_new0(UBDeviceInfo, 1);
    info->bi = dev->bus_instance_eid;
    info->eid = dev->eid;
    info->type = dev->dev_type;
    info->name = g_strdup(dev->name);
    info->id = g_strdup(dev->qdev.id);
    info->cna = dev->cna;
    info->feidx = dev->ue_idx;
    ub_device_get_str_from_guid(&dev->guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);
    info->guid = guid_str;
    info->ports = dev->port.port_num;
    info->usis = dev->usi_entries_nr;
    return info;
}

static UBDeviceInfoList *qmp_query_ub_devices(UBBus *bus)
{
    UBDeviceInfoList *head = NULL, **tail = &head;
    UBDevice *dev;

    QLIST_FOREACH(dev, &bus->devices, node) {
        QAPI_LIST_APPEND(tail, qmp_query_ub_device(dev));
    }

    return head;
}

static UBInfo *qmp_query_ub_bus(UBBus *bus)
{
    UBInfo *info = NULL;
    info = g_malloc0(sizeof(*info));
    info->devices = qmp_query_ub_devices(bus);

    return info;
}

UBInfoList *qmp_query_ub(Error **errp)
{
    UBInfoList *head = NULL, **tail = &head;
    BusControllerState *ubc = NULL;

    QLIST_FOREACH(ubc, &ub_bus_controllers, node) {
        QAPI_LIST_APPEND(tail,
                         qmp_query_ub_bus(ubc->bus));
    }

    return head;
}

/* container_of cannot be used here because 'bus' is a pointer member. */
BusControllerState *container_of_ubbus(UBBus *bus)
{
    BusControllerState *ubc = NULL;

    QLIST_FOREACH(ubc, &ub_bus_controllers, node) {
        if (bus == ubc->bus) {
            return ubc;
        }
    }

    return NULL;
}

UBDevice *ub_find_device_by_id(const char *id)
{
    BusControllerState *ubc = NULL;
    UBDevice *dev = NULL;

    QLIST_FOREACH(ubc, &ub_bus_controllers, node) {
        if (!ubc->bus->qbus.num_children) {
            continue;
        }
        QLIST_FOREACH(dev, &ubc->bus->devices, node) {
            if (dev && !strcmp(id, dev->qdev.id)) {
                return dev;
            }
        }
    }
    return NULL;
}

UBDevice *ub_find_device_by_guid(UbGuid *guid)
{
    BusControllerState *ubc = NULL;
    UBDevice *dev = NULL;

    QLIST_FOREACH(ubc, &ub_bus_controllers, node) {
        if (!ubc->bus->qbus.num_children) {
            continue;
        }
        QLIST_FOREACH(dev, &ubc->bus->devices, node) {
            if (dev && !memcmp(guid, &dev->guid, sizeof(UbGuid))) {
                return dev;
            }
        }
    }
    return NULL;
}

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
static void ub_config_set_port_basic(NeighborInfo *info, UBDevice *dev)
{
    uint32_t port_idx = info->local_port_idx;
    uint64_t emulated_offset;
    ConfigPortBasic *port_basic = NULL;
    ConfigPortBasic *port_basic_wmask = NULL;
    ConfigPortBasic *port_basic_w1cmask = NULL;

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_PORT_SLICE_START + port_idx * UB_PORT_SZ, true);
    port_basic = (ConfigPortBasic *)(dev->config + emulated_offset);
    port_basic_wmask = (ConfigPortBasic *)(dev->wmask + emulated_offset);
    port_basic_w1cmask = (ConfigPortBasic *)(dev->w1cmask + emulated_offset);
    memset(port_basic, 0, sizeof(ConfigPortBasic));
    memset(port_basic_wmask, 0, sizeof(ConfigPortBasic));
    memset(port_basic_w1cmask, 0, sizeof(ConfigPortBasic));
    /* slice header */
    port_basic->header.slice_version = UB_SLICE_VERSION;
    port_basic->header.slice_used_size = UB_PORT_BASIC_SLICE_USED_SIZE;
    /* port info */
    port_basic->port_info.port_type = 0; // physical port
    port_basic->port_info.port_idx = port_idx & UINT16_MASK;
    /* neighbor port info */
    port_basic->neighbor_port_info.neighbor_port_idx = info->neighbor_port_idx & UINT16_MASK;
    port_basic->neighbor_port_info.neighbot_port_guid = info->neighbor_dev->guid;
    port_basic->port_reset = 0;

    /* set wmask */
    port_basic_wmask->port_cna = ~0;
    port_basic_w1cmask->port_reset = ~0;
}
// #pragma GCC pop_options

static int ub_dev_set_neighbor_dev_neighbor_info(uint32_t local_port_idx,
                                                 uint32_t neighbor_port_idx, UBDevice *local_dev,
                                                 UBDevice *neighbor_dev, Error **errp)
{
    UbPortInfo *neighbor_port = &neighbor_dev->port;

    if (neighbor_port->port_num <= neighbor_port_idx) {
        qemu_log("invalid neighbor port idx %u %u\n",
                 neighbor_port->port_num, neighbor_port_idx);

        error_setg(errp, "invalid neighbor port idx %u %u\n",
                   neighbor_port->port_num, neighbor_port_idx);
        return -1;
    }

    if (neighbor_port->neighbors[neighbor_port_idx].neighbor_dev) {
        if (neighbor_port->neighbors[neighbor_port_idx].neighbor_dev != local_dev ||
            neighbor_port->neighbors[neighbor_port_idx].local_port_idx != neighbor_port_idx ||
            neighbor_port->neighbors[neighbor_port_idx].neighbor_port_idx != local_port_idx) {
            qemu_log("The neighbor information of the two devices does not match "
                     "each other. \nPlease check your command line parameter port info:\n"
                     "%s set (%s:%u = %s:%u) BUT neighbor %s already set (%s:%u = %s:%u)\n",
                     local_dev->qdev.id, local_dev->qdev.id, local_port_idx,
                     neighbor_dev->qdev.id, neighbor_port_idx,
                     neighbor_dev->qdev.id, neighbor_dev->qdev.id, neighbor_port_idx,
                     neighbor_port->neighbors[neighbor_port_idx].neighbor_dev->qdev.id,
                     neighbor_port->neighbors[neighbor_port_idx].neighbor_port_idx);

            error_setg(errp, "The neighbor information of the two devices does not match "
                       "each other. \nPlease check your command line parameter port info:\n"
                       "%s set (%s:%u = %s:%u) BUT neighbor %s already set (%s:%u = %s:%u)\n",
                       local_dev->qdev.id, local_dev->qdev.id, local_port_idx,
                       neighbor_dev->qdev.id, neighbor_port_idx,
                       neighbor_dev->qdev.id, neighbor_dev->qdev.id, neighbor_port_idx,
                       neighbor_port->neighbors[neighbor_port_idx].neighbor_dev->qdev.id,
                       neighbor_port->neighbors[neighbor_port_idx].neighbor_port_idx);
            return -1;
        }
    }
    neighbor_port->neighbors[neighbor_port_idx].local_port_idx = neighbor_port_idx;
    neighbor_port->neighbors[neighbor_port_idx].neighbor_port_idx = local_port_idx;
    neighbor_port->neighbors[neighbor_port_idx].neighbor_dev = local_dev;
    neighbor_port->port_info_exist = true;
    ub_config_set_port_basic(&neighbor_port->neighbors[neighbor_port_idx], neighbor_dev);
    return 0;
}

static int ub_dev_set_neighbor_info(UBDevice *dev, Error **errp)
{
    char *neighbor_info_str;
    char neighbor_id[UB_DEV_ID_LEN] = {0};
    uint32_t local_port_idx;
    uint32_t neighbor_port_idx;
    UBDevice *neighbor_dev;
    int ret = -1;

    neighbor_info_str = strtok(dev->port.neighbors_cmd, "+");
    if (strlen(neighbor_info_str) >= UB_DEV_ID_LEN) {
        qemu_log("unexpect neighbor_info_str(%s) is too long, please check\n", neighbor_info_str);
        goto free;
    }

    while (neighbor_info_str != NULL) {
        int num = sscanf(neighbor_info_str, "%u:%[^:]:%u",
                         &local_port_idx, neighbor_id, &neighbor_port_idx);
        neighbor_info_str = strtok(NULL, "+");
        if (num < 3) {
            qemu_log("port info format is incorrect %s\n", neighbor_info_str);
            error_setg(errp, "port info format is incorrect %s\n", neighbor_info_str);
            goto free;
        }
        if (local_port_idx >= dev->port.port_num) {
            qemu_log("%s local port info is illegal, port idx:%u port num %u\n",
                     dev->qdev.id, local_port_idx, dev->port.port_num);
            error_setg(errp, "%s local port info is illegal, port idx:%u port num %u\n",
                       dev->qdev.id, local_port_idx, dev->port.port_num);
            goto free;
        }

        neighbor_dev = ub_find_device_by_id(neighbor_id);
        if (neighbor_dev == NULL) {
            qemu_log("%s:%u neighbor_dev not exist %s\n",
                     dev->qdev.id, local_port_idx, neighbor_id);
            error_setg(errp, "%s:%u neighbor_dev not exist %s\n",
                       dev->qdev.id, local_port_idx, neighbor_id);
            goto free;
        }
        if (neighbor_dev == dev) {
            qemu_log("%s can not connect to itself\n", dev->qdev.id);
            error_setg(errp, "%s can not connect to itself\n", dev->qdev.id);
            goto free;
        }
        if (neighbor_port_idx >= neighbor_dev->port.port_num) {
            qemu_log("%s neighbor port info is illegal, port idx:%u port num %u\n",
                     dev->qdev.id, neighbor_port_idx, neighbor_dev->port.port_num);
            error_setg(errp, "%s neighbor port info is illegal, port idx:%u port num %u\n",
                       dev->qdev.id, neighbor_port_idx, neighbor_dev->port.port_num);
            goto free;
        }
        /* ub device can only connect with ub controller or ub switch */
        if ((dev->dev_type & UB_TYPE_DEVICE) &&
            !(neighbor_dev->dev_type & (UB_TYPE_SWITCH | UB_TYPE_ISWITCH | UB_TYPE_IBUS_CONTROLLER))) {
            qemu_log("%s can not connect with %s, ub device can only connect with "
                     "ub controller or ub switch\n", dev->qdev.id, neighbor_dev->qdev.id);
            error_setg(errp,"%s can not connect with %s ub device can only connect with "
                       "ub controller or ub switch\n", dev->qdev.id, neighbor_dev->qdev.id);
            goto free;
        }
        /* Check whether the neighbor information of the two ends matches. */
        if (dev->port.neighbors[local_port_idx].neighbor_dev) {
            if (dev->port.neighbors[local_port_idx].neighbor_dev != neighbor_dev ||
                dev->port.neighbors[local_port_idx].local_port_idx != local_port_idx ||
                dev->port.neighbors[local_port_idx].neighbor_port_idx != neighbor_port_idx) {
                qemu_log("The neighbor information of the two devices does not match "
                         "each other. \nPlease check your command line parameter port info:\n"
                         "%s set (%s:%u = %s:%u) BUT %s set (%s:%u = %s:%u)\n",
                         dev->qdev.id, dev->qdev.id, local_port_idx,
                         neighbor_dev->qdev.id, neighbor_port_idx,
                         dev->port.neighbors[local_port_idx].neighbor_dev->qdev.id,
                         dev->port.neighbors[local_port_idx].neighbor_dev->qdev.id,
                         dev->port.neighbors[local_port_idx].neighbor_port_idx,
                         dev->qdev.id,
                         dev->port.neighbors[local_port_idx].local_port_idx);

                error_setg(errp, "The neighbor information of the two devices does not match "
                           "each other. \nPlease check your command line parameter port info:\n"
                           "%s set (%s:%u = %s:%u) BUT %s set (%s:%u = %s:%u)\n",
                           dev->qdev.id, dev->qdev.id, local_port_idx,
                           neighbor_dev->qdev.id, neighbor_port_idx,
                           dev->port.neighbors[local_port_idx].neighbor_dev->qdev.id,
                           dev->port.neighbors[local_port_idx].neighbor_dev->qdev.id,
                           dev->port.neighbors[local_port_idx].neighbor_port_idx,
                           dev->qdev.id,
                           dev->port.neighbors[local_port_idx].local_port_idx);

                goto free;
            }
        }
        dev->port.neighbors[local_port_idx].local_port_idx = local_port_idx;
        dev->port.neighbors[local_port_idx].neighbor_port_idx = neighbor_port_idx;
        dev->port.neighbors[local_port_idx].neighbor_dev = neighbor_dev;
        dev->port.port_info_exist = true;
        /* set remote neighbor_dev */
        if (ub_dev_set_neighbor_dev_neighbor_info(local_port_idx, neighbor_port_idx, dev,
                                                  neighbor_dev, errp) < 0) {
            goto free;
        }
        ub_config_set_port_basic(&dev->port.neighbors[local_port_idx], dev);
    }
    ret = 0;

free:
    g_free(dev->port.neighbors_cmd);
    dev->port.neighbors_cmd = NULL;
    return ret;
}

static int ub_dev_init_port_info_by_cmd(Error **errp)
{
    BusControllerState *ubc = NULL;
    UBDevice *dev = NULL;

    QLIST_FOREACH(ubc, &ub_bus_controllers, node) {
        if (!ubc->bus->qbus.num_children) {
            continue;
        }

        QLIST_FOREACH(dev, &ubc->bus->devices, node) {
            if (dev && dev->port.neighbors_cmd) {
                if (ub_dev_set_neighbor_info(dev, errp) < 0) {
                    return -1;
                }
                qemu_log("finish set_neighbor_info, eid:%u\n", dev->eid);
            }
        }
    }
    /* Check whether any device port info does not exist */
    QLIST_FOREACH(ubc, &ub_bus_controllers, node) {
        if (!ubc->bus->qbus.num_children) {
            continue;
        }
        QLIST_FOREACH(dev, &ubc->bus->devices, node) {
            if (dev->dev_type != UB_TYPE_DEVICE && dev->dev_type != UB_TYPE_IDEVICE) {
                continue;
            }

            if (dev && !dev->port.port_info_exist) {
                qemu_log("%s port info does not exist.\n", dev->qdev.id);
                error_setg(errp, "%s port info does not exist.\n", dev->qdev.id);
                return -1;
            }
        }
    }
    return 0;
}

bool ub_guid_initialized(UbGuid *guid)
{
    if (!guid->vendor && !guid->type && !guid->version &&
        !guid->device_id && !guid->rsv && !guid->seq_num) {
        return false;
    } else {
        return true;
    }
}

AddressSpace *ub_device_iommu_address_space(UBDevice *dev)
{
    UBBus *bus = ub_get_bus(dev);

    if (bus->iommu_ops && bus->iommu_ops->get_address_space) {
        return bus->iommu_ops->get_address_space(bus, bus->iommu_opaque, dev->eid);
    }
    return &address_space_memory;
}

int ub_device_set_iommu_device(UBDevice *dev, HostIOMMUDevice *hoid, Error **errp)
{
    UBBus *bus = ub_get_bus(dev);

    if (bus->iommu_ops && bus->iommu_ops->set_iommu_device) {
        return !bus->iommu_ops->set_iommu_device(bus, bus->iommu_opaque, dev->eid, hoid, errp);
    }

    return 0;
}

void ub_device_unset_iommu_device(UBDevice *dev)
{
    UBBus *bus = ub_get_bus(dev);

    if (bus->iommu_ops && bus->iommu_ops->unset_iommu_device) {
        bus->iommu_ops->unset_iommu_device(bus, bus->iommu_opaque, dev->eid);
    }
}

bool ub_device_check_ummu_is_nested(UBDevice *dev)
{
    UBBus *bus = ub_get_bus(dev);

    if (bus->iommu_ops && bus->iommu_ops->ummu_is_nested) {
        return bus->iommu_ops->ummu_is_nested(bus->iommu_opaque);
    }

    return false;
}

void ub_register_ers(UBDevice *dev, uint8_t region_num, MemoryRegion *memory)
{
    UBIORegion *r;
    UbCfg1Basic *cfg1_basic_wmask;
    uint64_t size = memory_region_size(memory);
    uint64_t emulated_offset;

    if (region_num >= UB_NUM_REGIONS) {
        qemu_log("invalid region_num %u\n", region_num);
        return;
    }

    r = &dev->io_regions[region_num];
    r->addr = UINT64_MAX;
    r->size = size;
    r->memory = memory;
    r->address_space = ub_get_bus(dev)->address_space_mem;
    /* Mark that the ers is RW */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic_wmask = (UbCfg1Basic *)(dev->wmask + emulated_offset);
    memset(&cfg1_basic_wmask->ers_ubba[region_num], 0xff, sizeof(uint64_t));
}

uint32_t ub_interrupt_id(UBDevice *udev)
{
    uint64_t offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2, true);
    UbCfg1IntType2Cap *cfg1_int_cap = (UbCfg1IntType2Cap *)(udev->config + offset);

    return cfg1_int_cap->interrupt_id;
}

static int ub_bus_instance_verify(Error **errp)
{
    BusControllerState *ubc = QLIST_FIRST(&ub_bus_controllers);
    UBDevice *dev = NULL;

    QLIST_FOREACH(dev, &ubc->bus->devices, node) {
        if (dev->dev_type == UB_TYPE_IBUS_CONTROLLER ||
            dev->bus_instance_eid == UINT32_MAX) {
            continue;
        }

        if (ub_dev_bus_instance_verify(dev, errp)) {
            return -1;
        }
    }
    return 0;
}

/*
 * now all ub device add, finally setup for all ub device.
 * 1. check ub device bus instance type
 * 2. init the port info
 * */
int ub_dev_finally_setup(Error **errp)
{
    if (ub_bus_instance_verify(errp)) {
        return -1;
    }

    /*
     * Initialize the port information of all UB devices according
     * to the input information after all UB devices are constructed.
     */
    if (ub_dev_init_port_info_by_cmd(errp) < 0) {
        return -1;
    }

    ub_set_ubinfo_in_ubc_table();

    return 0;
}

void ub_setup_iommu(UBBus *bus, const UBIOMMUOps *ops, void *opaque)
{
    /*
     * If called, ub_setup_iommu() should provide a minimum set of
     * useful callbacks for the bus.
     */
    bus->iommu_ops = ops;
    bus->iommu_opaque = opaque;
}

uint32_t ub_dev_get_token_id(UBDevice *udev)
{
    uint64_t offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_DEV_TOKEN_ID_OFFSET, true);
    return *(uint32_t *)(udev->config + offset);
}

uint32_t ub_dev_get_ueid(UBDevice *udev)
{
    uint64_t offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_DEV_UEID_OFFSET, true);
    return *(uint32_t *)(udev->config + offset);
}

enum UbDeviceType ub_dev_get_type(UBDevice *udev)
{
    uint64_t offset;
    UbCfg1Basic *cfg1;
    int baseCode;

    if (udev == NULL) {
        return UB_TYPE_UNINIT;
    }

    offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1 = (UbCfg1Basic *)(udev->config + offset);
    baseCode = cfg1->class_code & UB_GUID_BASE_CODE_MASK;

    switch (udev->guid.type) {
    case UB_GUID_TYPE_BUS_INSTANCE:
        return UB_TYPE_BUS_INSTANCE;
    case UB_GUID_TYPE_BUS_CONTROLLER:
        if (baseCode == UB_GUID_BASE_INSTANCE) {
            return UB_TYPE_UNINIT;
        } else {
            return UB_TYPE_DEVICE;
        }
    case UB_GUID_TYPE_IBUS_CONTROLLER:
        if (baseCode == UB_GUID_BASE_INSTANCE) {
            return UB_TYPE_IBUS_CONTROLLER;
        } else {
            return UB_TYPE_IDEVICE;
        }
    case UB_GUID_TYPE_SWITCH:
        return UB_TYPE_SWITCH;
    case UB_GUID_TYPE_ISWITCH:
        return UB_TYPE_ISWITCH;
    default:
        return UB_TYPE_UNINIT;
    }
}

int ub_dev_dump_config(const char *id, uint64_t offset, uint64_t len,
                       char *buff, int buff_size)
{
    UBDevice *dev = ub_find_device_by_id(id);
    uint64_t emulated_offset;
    uint64_t origin_len = len;

    if (!dev) {
        qemu_log("UB device not found, id %s\n", id);
        return -1;
    }

    emulated_offset = ub_cfg_offset_to_emulated_offset(offset, false);
    if (emulated_offset == UINT64_MAX) {
        qemu_log("ub dev dump config out of emulated cfg range, "
                 "offset is 0x%lx\n", offset);
        return -1;
    }

    if (emulated_offset + len > ub_emulated_config_size()) {
        len = ub_emulated_config_size() - emulated_offset;
        qemu_log("ub dev dump config len out of eulated cfg range, "
                 "adjust len from 0x%lx to 0x%lx\n", origin_len, len);
    }

    return ub_hexdump(dev->config, emulated_offset, len, buff, buff_size);
}

void ub_dev_dump_ers(const char *id, uint8_t idx, uint64_t offset, uint64_t len,
                      char *buff, int buff_size)
{
    UBDevice *udev = ub_find_device_by_id(id);
    VFIOUBDevice *vdev = NULL;
    VFIOERS *ers = NULL;
    VFIORegion *region = NULL;
    uint64_t emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    UbCfg1Basic *cfg1 = NULL;
    int i;
    int l = 0;
    uint64_t len_printed = 0;
    uint64_t len_remain = 0;

    if (!udev) {
        qemu_log("do not have ub device %s\n", id);
        return;
    }

    cfg1 = (UbCfg1Basic *)(udev->config + emulated_offset);
    l += snprintf(buff + l, buff_size - l, "io_region[%u] size 0x%lx addr 0x%lx\n",
                  idx, udev->io_regions[idx].size, udev->io_regions[idx].addr);

    vdev = VFIO_UB_SAFE(udev);
    if (!vdev) {
        l += snprintf(buff + l, buff_size - l, "only support vfio-ub dev\n");
        return;
    }
    ers = &vdev->ers[idx];
    region = &ers->region;
    l += snprintf(buff + l, buff_size - l, "ers[%u] size 0x%zx gpa 0x%lx\n",
                  idx, ers->size, cfg1->ers_ubba[idx]);
    l += snprintf(buff + l, buff_size - l, "ers[%u] region->nr_mmaps %u\n",
                  idx, region->nr_mmaps);
    for (i = 0; i < region->nr_mmaps; i++) {
        l += snprintf(buff + l, buff_size - l,
                      "mmaps[%d]:\n"
                      " +-- mmap %p size 0x%lx offset 0x%lx\n"
                      " +-- memRegion:\n"
                      "       +--name %s addr 0x%lx align 0x%lx\n"
                      "       +--bool: ram %u readonly %u\n",
                      i, region->mmaps[i].mmap, region->mmaps[i].size,
                      region->mmaps[i].offset, region->mmaps[i].mem.name,
                      region->mmaps[i].mem.addr, region->mmaps[i].mem.align,
                      region->mmaps[i].mem.ram, region->mmaps[i].mem.readonly);
        if (region->mmaps[i].offset) {
            if (offset < region->mmaps[i].offset) {
                l += snprintf(buff + l, buff_size - l,
                              "warn:The query area falls within the simulation area,\n"
                              "querying the simulation area is not supported at present.\n"
                              "please adjust the offset to the queryable area.\n");
                return;
            } else {
                offset -= region->mmaps[i].offset;
            }
        }
        len_remain = len - len_printed;
        if (len_remain > region->mmaps[i].size) {
            len_remain = region->mmaps[i].size;
        }
        ub_hexdump(region->mmaps[i].mmap, offset, len_remain,
                   buff + l, buff_size - l);
        len_printed += region->mmaps[i].size;
    }
}

static void ub_dev_get_usi_info(Monitor *mon, UBDevice *udev)
{
    int i;
    /* usi enable info */
    monitor_printf(mon, "│%-24s│%-25u%-20u│\n",
                  "USI: enable masked", usi_enabled(udev), usi_ue_is_masked(udev));
    for (i = 0; i < udev->usi_entries_nr; i++) {
        USIMessage msg = usi_get_message(udev, i);
        monitor_printf(mon, "│%-4s%d%-19s│0x%-20lx%7u%8u%8u│\n",
                       "vect", i, ":addr data pend msk",
                       msg.address, msg.data,
                       usi_is_pending(udev, i), usi_is_masked(udev, i));
    }
    /* USI table info */
    monitor_printf(mon, "│%-24s│0x%-23lx%-20u│\n", "USI: vector_table nr",
                  udev->usi_vec_table_mmio.addr, udev->usi_entries_nr);
    monitor_printf(mon, "│%-24s│0x%-23lx%-20u│\n", "USI: addr_table nr",
                  udev->usi_addr_table_mmio.addr, udev->usi_addr_table_nr);
    monitor_printf(mon, "│%-24s│0x%-43lx│\n", "USI: pend_table",
                  udev->usi_pend_table_mmio.addr);

    /* USI notify info */
    monitor_printf(mon, "│%-24s│%-45p│\n", "USI: UseNotify",
                  udev->usi_vector_use_notifier);
    monitor_printf(mon, "│%-24s│%-45p│\n", "USI: ReleaseNotify",
                  udev->usi_vector_release_notifier);
    monitor_printf(mon, "│%-24s│%-45p│\n", "USI: PollNotify",
                  udev->usi_vector_poll_notifier);
    return;
}

static void ub_dev_get_cfg0_info(Monitor *mon, UBDevice *udev)
{
    uint64_t offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_START, true);
    UbCfg0Basic *cfg0 = (UbCfg0Basic *)(udev->config + offset);
    ConfigNetAddrInfo *cna;
    char cap_bitmap[CAP_BITMAP_LEN + 1] = {0};

    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:total ports", cfg0->total_num_of_port);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:total UEs", cfg0->total_num_of_ue);
    if (bitmap_scnprintf(cap_bitmap, sizeof(cap_bitmap),
        (unsigned long *)cfg0->cap_bitmap, sizeof(cfg0->cap_bitmap)) <= 0) {
        snprintf(cap_bitmap, sizeof(cap_bitmap), "failed to get bitmap");
    }
    monitor_printf(mon, "│%-24s│0x%-43s│\n", "cfg0:cap_bitmap", cap_bitmap);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:feat.entity",
                  cfg0->support_feature.bits.entity_available);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:feat.mtu",
                  cfg0->support_feature.bits.mtu_supported);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:feat.route_table",
                  cfg0->support_feature.bits.route_table_supported);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:feat.upi",
                  cfg0->support_feature.bits.upi_supported);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:feat.switch",
                  cfg0->support_feature.bits.switch_supported);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:feat.cc",
                  cfg0->support_feature.bits.cc_supported);
    monitor_printf(mon, "│%-24s│0x%-8x0x%-8x0x%-8x0x%-13x│\n",
                  "cfg0:eid", cfg0->eid.dw0, cfg0->eid.dw1,
                  cfg0->eid.dw2, cfg0->eid.dw3);
    monitor_printf(mon, "│%-24s│0x%-8x0x%-8x0x%-8x0x%-13x│\n",
                  "cfg0:fm_eid", cfg0->fm_eid.dw0, cfg0->fm_eid.dw1,
                  cfg0->fm_eid.dw2, cfg0->fm_eid.dw3);
    offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_NA_INFO_START, true);
    cna = (ConfigNetAddrInfo *)(udev->config + offset);
    monitor_printf(mon, "│%-24s│0x%-23x%-20u│\n", "cfg0:net_addr.cna",
                  cna->primary_cna, cna->primary_cna);
    monitor_printf(mon, "│%-24s│0x%-23x%-20u│\n", "cfg0:upi", cfg0->upi, cfg0->upi);
    monitor_printf(mon, "│%-24s│%-45u│\n", "config0:module_id", cfg0->module_id);
    monitor_printf(mon, "│%-24s│%-45u│\n", "config0:vendor_id", cfg0->vendor_id);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:dev_rst", cfg0->dev_rst);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:mtu_cfg", cfg0->mtu_cfg);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:cc_en", cfg0->cc_en);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg0:th_en", cfg0->th_en);
    monitor_printf(mon, "│%-24s│0x%-23x%-20u│\n", "cfg0:fm_cna",
                  cfg0->fm_cna, cfg0->fm_cna);
    monitor_printf(mon, "│%-24s│0x%-23lx%-20lu│\n", "cfg0:ueid_low",
                  cfg0->ueid_low, cfg0->ueid_low);
    monitor_printf(mon, "│%-24s│0x%-23lx%-20lu│\n", "cfg0:ueid_high",
                  cfg0->ueid_high, cfg0->ueid_high);
    return;
}

static void ub_dev_get_cfg1_int_type2_capinfo(Monitor *mon, UBDevice *udev)
{
    uint64_t emu_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2, true);
    UbCfg1IntType2Cap *cap = (UbCfg1IntType2Cap *)(udev->config + emu_offset);

    monitor_printf(mon, "│%-24s│vec_table_start_addr 0x%-22lx│\n", "cfg1:int type2 CAP",
                  cap->vec_table_start_addr);
    monitor_printf(mon, "│%-24s│add_table_start_addr 0x%-22lx│\n", "cfg1:int type2 CAP",
                  cap->add_table_start_addr);
    monitor_printf(mon, "│%-24s│pend_table_start_addr 0x%-21lx│\n", "cfg1:int type2 CAP",
                  cap->pend_table_start_addr);
    monitor_printf(mon, "│%-24s│int_id 0x%-6xint_mask 0x%-3xint_enable 0x%-3x│\n",
                  "cfg1:int type2 CAP", cap->interrupt_id, cap->interrupt_mask, cap->interrupt_enable);
    return;
}

static void ub_dev_get_cfg1_info(Monitor *mon, UBDevice *udev)
{
    uint64_t offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    UbCfg1Basic *cfg1 = (UbCfg1Basic *)(udev->config + offset);
    char cap_bitmap[CAP_BITMAP_LEN + 1] = {0};
    int i;

    if (bitmap_scnprintf(cap_bitmap, sizeof(cap_bitmap),
        (unsigned long *)cfg1->cap_bitmap, sizeof(cfg1->cap_bitmap)) <= 0) {
        snprintf(cap_bitmap, sizeof(cap_bitmap), "failed to get bitmap");
    }
    monitor_printf(mon, "│%-24s│0x%-43s│\n", "cfg1:cap_bitmap", cap_bitmap);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg1:feat.mgs",
                  cfg1->support_feature.bits.mgs);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg1:feat.ubbas",
                  cfg1->support_feature.bits.ubbas);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg1:feat.ers0s",
                  cfg1->support_feature.bits.ers0s);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg1:feat.ers1s",
                  cfg1->support_feature.bits.ers1s);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg1:feat.ers2s",
                  cfg1->support_feature.bits.ers2s);
    monitor_printf(mon, "│%-24s│%-45u│\n", "config1:feat.matt_juris",
                  cfg1->support_feature.bits.matt_juris);
    for (i = 0; i < UB_NUM_REGIONS; i++) {
        monitor_printf(mon, "│%-9s%-2usz sa ba(hex)│%-11x%-17lx%-17lx│\n",
                      "cfg1:ERS", i, cfg1->ers_space_size[i],
                      cfg1->ers_start_addr[i], cfg1->ers_ubba[i]);
    }
    monitor_printf(mon, "│%-24s│%-20u%-25u│\n", "cfg1:elr elr_done",
                  cfg1->elr, cfg1->elr_done);
    monitor_printf(mon, "│%-24s│0x%-21lx0x%-20x│\n", "cfg1:eid_upi tab ten",
                  cfg1->eid_upi_tab, cfg1->eid_upi_ten);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg1:bus_access_en",
                  cfg1->bus_access_en);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cfg1:dev_rs_access_en",
                  cfg1->dev_rs_access_en);
    monitor_printf(mon, "│%-24s│0x%-23x%-20u│\n", "cfg1:dev_token_id",
                  cfg1->dev_token_id, cfg1->dev_token_id);

    ub_dev_get_cfg1_int_type2_capinfo(mon, udev);

    return;
}
static void ub_dev_get_bus_info(Monitor *mon, UBDevice *udev)
{
    BusControllerState *ubcs = container_of_ubbus(UB_BUS(udev->qdev.parent_bus));
    UBDevice *tmp;

    monitor_printf(mon, "│%-24s│%-45s│\n",
                  "parent_bus name", udev->qdev.parent_bus->name);
    monitor_printf(mon, "│%-24s│%-45d│\n",
                  "parent_bus max_index", udev->qdev.parent_bus->max_index);
    monitor_printf(mon, "│%-24s│%-45u│\n",
                  "parent_bus realized", udev->qdev.parent_bus->realized);
    monitor_printf(mon, "│%-24s│%-45u│\n",
                  "parent_bus full", udev->qdev.parent_bus->full);
    monitor_printf(mon, "│%-24s│%-45u│\n", "parent_bus num_children",
                  udev->qdev.parent_bus->num_children);
    QLIST_FOREACH(tmp, &ubcs->bus->devices, node) {
        monitor_printf(mon, "│%-24s│name %-10s id %-16seid%7u│\n",
                      "      device", tmp->name, tmp->qdev.id, tmp->eid);
    }

    monitor_printf(mon, "│%-24s│%-45s│\n",
                  "parent_bus p id", udev->qdev.parent_bus->parent->id);
    monitor_printf(mon, "│%-24s│%-45s│\n", "parent_bus p canon_path",
                  udev->qdev.parent_bus->parent->canonical_path);
    return;
}

static void ub_dev_get_ubc_info(Monitor *mon, UBDevice *udev)
{
    BusControllerState *ubcs = container_of_ubbus(UB_BUS(udev->qdev.parent_bus));
    VirtMachineState *vms = VIRT_MACHINE(qdev_get_machine());

    monitor_printf(mon, "│%-24s│%-45u│\n", "cluster_mode", vms->ub_cluster_mode);
    monitor_printf(mon, "│%-24s│%-45u│\n", "fm_deployment", vms->fm_deployment);
    monitor_printf(mon, "│%-24s│%-45u│\n", "mmio_size", ubcs->mmio_size);
    monitor_printf(mon, "│%-24s│%-45u│\n", "mig_enabled", ubcs->mig_enabled);
    monitor_printf(mon, "│%-24s│%-45u│\n", "msgq_reg_size", ubcs->msgq_reg_size);
    monitor_printf(mon, "│%-24s│0x%-43lx│\n", "msgq_reg", (uint64_t)ubcs->msgq_reg);
    monitor_printf(mon, "│%-24s│%-45s│\n", "MR msgq_reg_mem name", ubcs->msgq_reg_mem.name);
    monitor_printf(mon, "│%-24s│%-45s│\n", "MR io_mmio name", ubcs->io_mmio.name);
    return;
}

static void ub_dev_get_ummu_info(Monitor *mon, UBDevice *udev)
{
    unsigned int bus_num;
    UMMUState *ummu = NULL;
    UMMUDevice *ummu_dev = NULL;
    UMMUKVTblEntry *entry = NULL;
    UMMUTransCfg *cfg = NULL;
    int i;

    if (1 == sscanf(udev->qdev.parent_bus->name, "ubus.%u", &bus_num)) {
        ummu = ummu_find_by_bus_num(bus_num);
    }

    if (!ummu) {
        return;
    }
    monitor_printf(mon, "│%-24s│%-45s│\n", "ummu id ", ummu->dev.parent_obj.id);
    monitor_printf(mon, "│%-24s│0x%-43lx│\n", " ummu_reg_size ", ummu->ummu_reg_size);
    for (i = 0; i < ARRAY_SIZE(ummu->mcmdqs); i++) {
        monitor_printf(mon, "│%-21s%-3u│gpa 0x%-39lx│\n",
                      " que_info cmdq base", i, ummu->mcmdqs[i].queue.base);
    }
    monitor_printf(mon, "│%-22s%2u│gpa 0x%-39lx│\n",
                  " que_info eventq base", i, ummu->eventq.queue.base);
    monitor_printf(mon, "│%-24s│%-8x %-8x %-8x %-8x %-8x │\n",
                  "ummu CAP[0-4]", ummu->cap[0], ummu->cap[1],
                  ummu->cap[2], ummu->cap[3], ummu->cap[4]);
    monitor_printf(mon, "│%-24s│%-8x %-18x %-17x│\n",
                  "ummu CAP[5-6] ctrl0_ack",
                  ummu->cap[5], ummu->cap[6], ummu->ctrl0_ack);
    monitor_printf(mon, "│%-24s│%-8x %-8x %-8x %-18x│\n",
                  "ummu CTRL[0-3]", ummu->ctrl[0], ummu->ctrl[1],
                  ummu->ctrl[2], ummu->ctrl[3]);

    monitor_printf(mon, "│%-24s│0x%-20lx 0x%-20lx│\n",
                  "tect_base_addr", ummu->tect_base,
                  (uint64_t)TECT_BASE_ADDR(ummu->tect_base));
    monitor_printf(mon, "│%-24s│0x%-23x%-20u│\n", "tect_base_cfg tag_num",
                  ummu->tect_base_cfg, ummu->tecte_tag_num);
    for (i = 0; i < ummu->tecte_tag_num; i++) {
        monitor_printf(mon, "│%-16s%2u%-6s│0x%-43x│\n",
                      " tecte_tag_cahe[", i, "]", ummu->tecte_tag_cache[i]);
    }
    monitor_printf(mon, "│%-24s│%-22d%-23d│\n", "usi_virq[EVETQ,GERROR]",
                  ummu->usi_virq[UMMU_USI_VECTOR_EVETQ],
                  ummu->usi_virq[UMMU_USI_VECTOR_GERROR]);
    QLIST_FOREACH(entry, &ummu->kvtbl, list) {
        monitor_printf(mon, "│%-24s│eid 0x%-17xtag %-18u│\n",
                      "kvtbl: dst_eid tecte_tag", entry->dst_eid, entry->tecte_tag);
    }
    monitor_printf(mon, "│%-24s│fd %-8downed %-4uusers %-6uref %-8u│\n",
                  "UMMUViommu->iommufd", ummu->viommu->iommufd->fd,
                  ummu->viommu->iommufd ? ummu->viommu->iommufd->owned : 0,
                  ummu->viommu->iommufd ? ummu->viommu->iommufd->users : 0,
                  ummu->viommu->iommufd ? ummu->viommu->iommufd->parent.ref : 0);
    if (ummu->viommu->core) {
        monitor_printf(mon, "│%-24s│s2_hwpt_id %-12uviommu_id %-12u│\n",
                      " ->core", ummu->viommu->core->s2_hwpt_id,
                      ummu->viommu->core->viommu_id);
    } else {
        monitor_printf(mon, "│%-24s│%-45s│\n",
                      " ->core", "IOMMUFDViommu is NULL, viommu not attach yet");
    }
    if (ummu->viommu->s2_hwpt) {
        monitor_printf(mon, "│%-24s│iommufd %-7uhwpt_id %-7uioas_id %-7u│\n",
                      " ->s2_hwpt", ummu->viommu->s2_hwpt->iommufd->fd,
                      ummu->viommu->s2_hwpt->hwpt_id,
                      ummu->viommu->s2_hwpt->ioas_id);
    } else {
        monitor_printf(mon, "│%-24s│%-45s│\n",
                      " ->s2_hwpt", "s2_hwpt is NULL, not attach viommu yet");
    }
    /* UMMUViommu: UMMUDevice device_list info */
    QLIST_FOREACH(ummu_dev, &ummu->viommu->device_list, next) {
        monitor_printf(mon, "│%-12s%-12s│as:name %-37s│\n",
                      " ->dev_list", ummu_dev->udev->qdev.id, ummu_dev->as.name);
        monitor_printf(mon, "│%-12s%-12s│idev: devid %-5uioas_id %-6uiommufd %-6u│\n",
                      " ->dev_list", ummu_dev->udev->qdev.id, ummu_dev->idev->devid,
                      ummu_dev->idev->ioas_id, ummu_dev->idev->iommufd->fd);
        if (ummu_dev->s1_hwpt) {
            monitor_printf(mon, "│%-12s%-12s│s1_hwpt: hwpt_id %-10uiommufd %-10u│\n",
                          " ->dev_list", ummu_dev->udev->qdev.id,
                          ummu_dev->s1_hwpt->hwpt_id,
                          ummu_dev->s1_hwpt->iommufd->fd);
        } else {
            monitor_printf(mon, "│%-12s%-12s│%-45s│\n",
                          " ->dev_list", ummu_dev->udev->qdev.id,
                          "s1_hwpt is NULL, tecte not install yet");
        }
        if (ummu_dev->vdev) {
            monitor_printf(mon, "│%-12s%-12s│vdev: sid %-7uVdevId %-7uVirtId %-7lu│\n",
                          " ->dev_list", ummu_dev->udev->qdev.id, ummu_dev->vdev->sid,
                          ummu_dev->vdev->core->vdev_id, ummu_dev->vdev->core->virt_id);
        } else {
            monitor_printf(mon, "│%-12s%-12s│%-45s│\n",
                          " ->dev_list", ummu_dev->udev->qdev.id,
                          "UMMUVdev is NULL, tecte not install yet");
        }

        cfg = g_hash_table_lookup(ummu->configs, ummu_dev);
        if (cfg) {
            monitor_printf(mon, "│+TransCfg %-14s│tct_ptr 0x%-16lxtct_num %-5lufmt %-2lu│\n",
                          ummu_dev->udev->qdev.id, cfg->tct_ptr, cfg->tct_num, cfg->tct_fmt);
            monitor_printf(mon, "│+TransCfg %-14s│tct_ttba 0x%-16lxtct_sz %-11u│\n",
                          ummu_dev->udev->qdev.id, cfg->tct_ttba, cfg->tct_sz);
            monitor_printf(mon, "│+TransCfg %-14s│tct_tgs 0x%-16xtecte_tag %-9u│\n",
                          ummu_dev->udev->qdev.id, cfg->tct_tgs, cfg->tecte_tag);
        }
    }
    return;
}

static void ub_dev_get_vfio_info(Monitor *mon, UBDevice *udev)
{
    VFIOUBDevice *vdev = VFIO_UB_SAFE(udev);
    int i;
    char guid[UB_DEV_GUID_STRING_LENGTH + 1] = {0};

    if (!vdev) {
        return;
    }
    monitor_printf(mon, "│%-24s│%-27sfd %-3ddevid 0x%-4x│\n",
                  "VFIOUBDev sysfsdev", vdev->vbasedev.sysfsdev,
                  vdev->vbasedev.fd, vdev->vbasedev.devid);
    ub_device_get_str_from_guid(&vdev->host.guid, guid,
                                UB_DEV_GUID_STRING_LENGTH + 1);
    monitor_printf(mon, "│%-24s│%-45s│\n", "VFIOUBDev host", guid);

    for (i = 0; i < UB_NUM_REGIONS; i++) {
        monitor_printf(mon, "│vfioers %-16d│hva %-18pofs 0x%-17lx│\n",
                      i, vdev->ers[i].region.mmaps ?
                      vdev->ers[i].region.mmaps[0].mmap : NULL,
                      vdev->ers[i].region.fd_offset);
    }
    if (!vdev->usi || !vdev->usi_vectors) {
        return;
    }
    for (i = 0; i < vdev->usi->vec_table_num; i++) {
        monitor_printf(mon, "│usi_vectors[%-2d] use=%-4u│virq %-6d "
                      "kvm_int %-1u %-5d interrupt %-1u %-5d│\n",
                      i, vdev->usi_vectors[i].use, vdev->usi_vectors[i].virq,
                      vdev->usi_vectors[i].kvm_interrupt.initialized,
                      vdev->usi_vectors[i].kvm_interrupt.rfd,
                      vdev->usi_vectors[i].interrupt.initialized,
                      vdev->usi_vectors[i].interrupt.rfd);
    }
    return;
}

int ub_dev_get_detail(Monitor *mon, const char *id)
{
    UBDevice *dev = ub_find_device_by_id(id);
    char guid[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    /* Column 1 width 24, column 2 width 45 */
    g_autofree char *line_c1 = line_generator(24);
    g_autofree char *line_c2 = line_generator(45);
    int i;

    if (!dev) {
        qemu_log("UB device not found, id %s\n", id);
        return -1;
    }
    if (!line_c1 || !line_c2) {
        qemu_log("failed to alloc mem %p %p\n",
                 line_c1, line_c2);
        return -1;
    }
    ub_device_get_str_from_guid(&dev->guid, guid,
                                UB_DEV_GUID_STRING_LENGTH + 1);
    monitor_printf(mon, "┌%s┬%s┐\n", line_c1, line_c2);
    monitor_printf(mon, "│%-24s│%-45s│\n", "id", dev->qdev.id);
    monitor_printf(mon, "│%-24s│%-11u%-34s│\n", "dev_type",
                  dev->dev_type, ub_dev_get_type_str(dev->dev_type));
    monitor_printf(mon, "│%-24s│%-45p│\n", "config", dev->config);
    monitor_printf(mon, "│%-24s│0x%-21lx0x%-20lx│\n", "config_size",
                  ub_config_size(), ub_config_size());
    monitor_printf(mon, "│%-24s│%-45s│\n", "name", dev->name);
    monitor_printf(mon, "│%-24s│%-45u│\n", "eid", dev->eid);
    ub_dev_get_cfg0_info(mon, dev);
    ub_dev_get_cfg1_info(mon, dev);
    monitor_printf(mon, "│%-24s│%-45u│\n", "cna", dev->cna);
    monitor_printf(mon, "│%-24s│%-45u│\n", "ue_idx", dev->ue_idx);
    monitor_printf(mon, "│%-24s│%-45s│\n", "guid", guid);
    monitor_printf(mon, "│%-24s│%-45u│\n", "port_num", dev->port.port_num);
    for (i = 0; i < dev->port.port_num; i++) {
        if ((dev->port.neighbors + i)->neighbor_dev) {
            monitor_printf(mon, "│neighbor_info lport %-4u│%-10s rport %-28u│\n",
                          (dev->port.neighbors + i)->local_port_idx,
                          (dev->port.neighbors + i)->neighbor_dev->qdev.id,
                          (dev->port.neighbors + i)->neighbor_port_idx);
        }
    }
    for (i = 0; i < UB_NUM_REGIONS; i++) {
        monitor_printf(mon, "│io_regions %-13d│gpa 0x%-18lx size 0x%-13lx│\n",
                      i, dev->io_regions[i].addr, dev->io_regions[i].size);
    }
    if (dev->dev_type == UB_TYPE_IDEVICE || dev->dev_type == UB_TYPE_DEVICE) {
        ub_dev_get_vfio_info(mon, dev);
    }
    monitor_printf(mon, "│%-24s│%-45s│\n", "canonical_path", dev->qdev.canonical_path);
    monitor_printf(mon, "│%-24s│%-45u│\n", "realized", dev->qdev.realized);
    monitor_printf(mon, "│%-24s│%-45u│\n", "pending_del_evt", dev->qdev.pending_deleted_event);
    monitor_printf(mon, "│%-24s│%-45lu│\n", "pending_del_expr_ms", dev->qdev.pending_deleted_expires_ms);
    monitor_printf(mon, "│%-24s│%-45d│\n", "hotplugged", dev->qdev.hotplugged);
    monitor_printf(mon, "│%-24s│%-45d│\n", "allow_unplug_dur_mig", dev->qdev.allow_unplug_during_migration);
    monitor_printf(mon, "│%-24s│count %-4uhold_pending %-4uexit_progress %-4u│\n",
                  "ResettableState", dev->qdev.reset.count,
                  dev->qdev.reset.hold_phase_pending,
                  dev->qdev.reset.exit_phase_in_progress);
    monitor_printf(mon, "│%-24s│%-45u│\n", "reset_count", dev->rst_cnt);
    ub_dev_get_bus_info(mon, dev);
    ub_dev_get_usi_info(mon, dev);
    /* ubc info */
    if (UB_TYPE_IBUS_CONTROLLER == dev->dev_type) {
        ub_dev_get_ummu_info(mon, dev);
        ub_dev_get_ubc_info(mon, dev);
    }

    monitor_printf(mon, "└%s┴%s┘", line_c1, line_c2);
    return 0;
}
