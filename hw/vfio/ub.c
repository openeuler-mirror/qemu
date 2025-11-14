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

static Property vfio_ub_dev_properties[] = {
    DEFINE_PROP_UB_HOST_DEVADDR("host", VFIOUBDevice, host),
#ifdef CONFIG_IOMMUFD
    DEFINE_PROP_LINK("iommufd", VFIOUBDevice, vbasedev.iommufd,
                     TYPE_IOMMUFD_BACKEND, IOMMUFDBackend *),
#endif
    DEFINE_PROP_END_OF_LIST(),
};

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
}

static void vfio_realize(UBDevice *udev, Error **errp)
{
}

static void vfio_exitfn(UBDevice *udev)
{
}

static void vfio_ub_read_config(UBDevice *dev, uint64_t offset,
                                uint32_t *val, uint32_t dw_mask)
{
}

static void vfio_ub_write_config(UBDevice *dev, uint64_t offset,
                                 uint32_t *val, uint32_t dw_mask)
{
}

static void vfio_ub_dev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    UBDeviceClass *udc = UB_DEVICE_CLASS(klass);

    dc->reset = vfio_ub_reset;
    device_class_set_props(dc, vfio_ub_dev_properties);
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
}

static void vfio_instance_finalize(Object *obj)
{
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