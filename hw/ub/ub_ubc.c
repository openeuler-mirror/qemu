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
#include "hw/ub/ub_ubc.h"
#include "migration/vmstate.h"

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
}

static const MemoryRegionOps ub_msgq_reg_ops = {
    .read = ub_msgq_reg_read,
    .write = ub_msgq_reg_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps ub_fm_msgq_reg_ops = {

};

static void ub_reg_alloc(DeviceState *dev)
{
    BusControllerState *s = BUS_CONTROLLER(dev);

    s->msgq_reg = g_malloc0(s->msgq_reg_size);
    s->fm_msgq_reg = g_malloc0(s->fm_msgq_reg_size);
    qemu_log("alloc ub reg mem size: msgq_reg %u, "
             "fm_msgq_reg %u\n",
             s->msgq_reg_size, s->fm_msgq_reg_size);
}

static void ub_reg_free(DeviceState *dev)
{
    BusControllerState *s = BUS_CONTROLLER(dev);

    g_free(s->msgq_reg);
    g_free(s->fm_msgq_reg);
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

    g_free(name);
}

static void ub_bus_controller_unrealize(DeviceState *dev)
{
    BusControllerState *s = BUS_CONTROLLER(dev);
    SysBusDevice *sysdev = SYS_BUS_DEVICE(dev);
    g_free(sysdev->parent_obj.id);
    QLIST_REMOVE(s, node);
    ub_reg_free(dev);
}

static bool ub_bus_controller_needed(void *opaque)
{
    BusControllerState *s = opaque;
    return s->mig_enabled;
}

static Property ub_bus_controller_properties[] = {
    DEFINE_PROP_UINT32("ub-bus-controller-msgq-reg-size", BusControllerState,
                       msgq_reg_size, 0),
    DEFINE_PROP_UINT32("ub-bus-controller-fm-msgq-reg-size", BusControllerState,
                       fm_msgq_reg_size, 0),
    DEFINE_PROP_BOOL("ub-bus-controller-migration-enabled", BusControllerState,
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

static void ub_bus_controller_register_types(void)
{
    type_register_static(&ub_bus_controller_type_info);
}
type_init(ub_bus_controller_register_types)
