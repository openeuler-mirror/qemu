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
#include "hw/ub/ub_acpi.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "migration/vmstate.h"
#include "exec/address-spaces.h"
#include "monitor/monitor.h"
#include "trace.h"

QLIST_HEAD(, BusControllerState) ub_bus_controllers;

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

static void ubbus_reset(BusState *qbus)
{
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
    cfg0_basic_wmask->dev_rst = ~0;
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
    cfg0_emq_cap_wmask->error_msg_que_ctrlr.interrupt_generation_enable = ~0;

    /* cfg1 basic */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic_wmask = (UbCfg1Basic *)(ub_dev->wmask + emulated_offset);
    memset(cfg1_basic_wmask, 0, sizeof(UbCfg1Basic));
    cfg1_basic_wmask->elr = ~0;
    cfg1_basic_wmask->mig_ctrl = ~0;
    cfg1_basic_wmask->sys_pgs = ~0;
    cfg1_basic_wmask->eid_upi_tab = ~0UL;
    cfg1_basic_wmask->ctp_tb_bypass = ~0;
    cfg1_basic_wmask->crystal_dma_en = ~0;
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

    /* port basic */
    // set after port_info is initialized

    /* port cap */
    //  not support yet

    /* route table */
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_ROUTE_TABLE_START, true);
    route_table_wmask = (UbRouteTable *)(ub_dev->wmask + emulated_offset);
    memset(route_table_wmask, 0xff, UB_CFG_SLICE_SIZE);
    route_table_wmask->entry_num = 0;
    route_table_wmask->ers = 0;

    /* route table entry */
    // not support yet
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

void ub_default_read_config(UBDevice *dev, uint64_t offset,
                            uint32_t *val, uint32_t dw_mask)
{
    uint32_t read_data;
    uint64_t emulated_offset = ub_cfg_offset_to_emulated_offset(offset, false);

    if (emulated_offset == UINT64_MAX) {
        *val = 0;
        qemu_log("ub default read config out of emulated range, offset "
                 "is 0x%lx\n", offset);
        return;
    }

    memcpy(&read_data, dev->config + emulated_offset, DWORD_SIZE);
    *val = read_data & dw_mask;
}

static uint64_t ub_er_address(UBDevice *dev, uint8_t ers, uint64_t size)
{
    uint64_t new_addr, last_addr;
    UbCfg1Basic *cfg1_basic;
    uint64_t emulated_offset;

    if (ers > UB_NUM_REGIONS) {
        qemu_log("invalid ers %u\n", ers);
        return UB_ER_UNMAPPED;
    }

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(dev->config + emulated_offset);
    if (!cfg1_basic->dev_rs_access_en) {
        return UB_ER_UNMAPPED;
    }

    new_addr = cfg1_basic->ers_ubba[ers];
    new_addr &= ~(size -1);
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
        if (new_addr == UB_ER_UNMAPPED) {
            continue;
        }

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

void ub_default_write_config(UBDevice *dev, uint64_t offset,
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
        return;
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

    if (uc->realize) {
        uc->realize(ub_dev, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            do_ub_unregister_device(ub_dev);
            return;
        }
    }
}

static void ub_qdev_unrealize(DeviceState *dev)
{
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

AddressSpace *ub_device_iommu_address_space(UBDevice *dev)
{
    UBBus *bus = ub_get_bus(dev);

    if (bus->iommu_ops && bus->iommu_ops->get_address_space) {
        return bus->iommu_ops->get_address_space(bus, bus->iommu_opaque, dev->eid);
    }
    return &address_space_memory;
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
    port_basic_wmask->port_reset = ~0;
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

    neighbor_info_str = strtok(dev->port.neighbors_cmd, "+");
    while (neighbor_info_str != NULL) {
        int ret = sscanf(neighbor_info_str, "%u:%[^:]:%u",
                         &local_port_idx, neighbor_id, &neighbor_port_idx);
        neighbor_info_str = strtok(NULL, "+");
        if (ret < 3) {
            qemu_log("port info format is incorrect %s\n", neighbor_info_str);
            error_setg(errp, "port info format is incorrect %s\n", neighbor_info_str);
            g_free(dev->port.neighbors_cmd);
            dev->port.neighbors_cmd = NULL;
            return -1;
        }
        if (local_port_idx >= dev->port.port_num) {
            qemu_log("%s local port info is illegal, port idx:%u port num %u\n",
                     dev->qdev.id, local_port_idx, dev->port.port_num);
            error_setg(errp, "%s local port info is illegal, port idx:%u port num %u\n",
                       dev->qdev.id, local_port_idx, dev->port.port_num);
            g_free(dev->port.neighbors_cmd);
            dev->port.neighbors_cmd = NULL;
            return -1;
        }

        neighbor_dev = ub_find_device_by_id(neighbor_id);
        if (neighbor_dev == NULL) {
            qemu_log("%s:%u neighbor_dev not exist %s\n",
                     dev->qdev.id, local_port_idx, neighbor_id);
            error_setg(errp, "%s:%u neighbor_dev not exist %s\n",
                       dev->qdev.id, local_port_idx, neighbor_id);
            g_free(dev->port.neighbors_cmd);
            dev->port.neighbors_cmd = NULL;
            return -1;
        }
        if (neighbor_dev == dev) {
            qemu_log("%s can not connect to itself\n", dev->qdev.id);
            error_setg(errp, "%s can not connect to itself\n", dev->qdev.id);
            g_free(dev->port.neighbors_cmd);
            dev->port.neighbors_cmd = NULL;
            return -1;
        }
        if (neighbor_port_idx >= neighbor_dev->port.port_num) {
            qemu_log("%s neighbor port info is illegal, port idx:%u port num %u\n",
                     dev->qdev.id, neighbor_port_idx, neighbor_dev->port.port_num);
            error_setg(errp, "%s neighbor port info is illegal, port idx:%u port num %u\n",
                       dev->qdev.id, neighbor_port_idx, neighbor_dev->port.port_num);
            g_free(dev->port.neighbors_cmd);
            dev->port.neighbors_cmd = NULL;
            return -1;
        }
        /* ub device can only connect with ub controller or ub switch */
        if ((dev->dev_type & UB_TYPE_DEVICE) &&
            !(neighbor_dev->dev_type & (UB_TYPE_SWITCH | UB_TYPE_ISWITCH | UB_TYPE_IBUS_CONTROLLER))) {
            qemu_log("%s can not connect with %s, ub device can only connect with "
                     "ub controller or ub switch\n", dev->qdev.id, neighbor_dev->qdev.id);
            error_setg(errp,"%s can not connect with %s ub device can only connect with "
                       "ub controller or ub switch\n", dev->qdev.id, neighbor_dev->qdev.id);
            g_free(dev->port.neighbors_cmd);
            dev->port.neighbors_cmd = NULL;
            return -1;
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

                g_free(dev->port.neighbors_cmd);
                dev->port.neighbors_cmd = NULL;
                return -1;
            }
        }
        dev->port.neighbors[local_port_idx].local_port_idx = local_port_idx;
        dev->port.neighbors[local_port_idx].neighbor_port_idx = neighbor_port_idx;
        dev->port.neighbors[local_port_idx].neighbor_dev = neighbor_dev;
        dev->port.port_info_exist = true;
        /* set remote neighbor_dev */
        if (ub_dev_set_neighbor_dev_neighbor_info(local_port_idx, neighbor_port_idx, dev,
                                                  neighbor_dev, errp) < 0) {
            g_free(dev->port.neighbors_cmd);
            dev->port.neighbors_cmd = NULL;
            return -1;
        }
        ub_config_set_port_basic(&dev->port.neighbors[local_port_idx], dev);
    }
    g_free(dev->port.neighbors_cmd);
    dev->port.neighbors_cmd = NULL;
    return 0;
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

uint32_t ub_interrupt_id(UBDevice *udev)
{
    uint64_t offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2, true);
    UbCfg1IntType2Cap *cfg1_int_cap = (UbCfg1IntType2Cap *)(udev->config + offset);

    return cfg1_int_cap->interrupt_id;
}

/*
 * now all ub device add, finally setup for all ub device.
 * 1. check ub device bus instance type
 * 2. init the port info
 * */
int ub_dev_finally_setup(VirtMachineState *vms, Error **errp)
{
    /*
     * Initialize the port information of all UB devices according
     * to the input information after all UB devices are constructed.
     */
    if (ub_dev_init_port_info_by_cmd(errp) < 0) {
        return -1;
    }

    ub_set_ubinfo_in_ubc_table(vms);

    return 0;
}