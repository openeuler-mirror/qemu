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
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "migration/vmstate.h"


QLIST_HEAD(, BusControllerState) ub_bus_controllers;

static void ubbus_dev_print(Monitor *mon, DeviceState *dev, int indent)
{
}

static char *ubbus_get_dev_path(DeviceState *dev)
{
    return NULL;
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

static UBDevice *do_ub_register_device(UBDevice *ub_dev, const char *name, Error **errp)
{
    return NULL;
}

static void do_ub_unregister_device(UBDevice *ub_dev)
{
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