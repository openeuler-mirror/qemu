/*
 * HISI MMCD VFIO device
 *
 * Copyright (c) 2026 Huawei Technologies.
 *
 * Authors: chenxiaoyu46@huawei.com
 *
 * This work is licensed under the terms of the GNU GPL, version 2. See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/vfio/vfio-hisi-mmcd.h"
#include "migration/vmstate.h"
#include "qemu/module.h"

static void hisi_mmcd_realize(DeviceState *dev, Error **errp)
{
    VFIOPlatformDevice *vdev = VFIO_PLATFORM_DEVICE(dev);
    VFIOHisiDevDeviceClass *k = VFIO_HISI_MMCD_DEVICE_GET_CLASS(dev);

    vdev->compat = g_strdup("hisi,mmcd");
    vdev->num_compat = 1;

    k->parent_realize(dev, errp);
}

static const VMStateDescription vfio_platform_hisi_mmcd_vmstate = {
        .name = "vfio-hisi-mmcd",
        .unmigratable = 1,
};

static void vfio_hisi_mmcd_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VFIOHisiDevDeviceClass *vcxc =
            VFIO_HISI_MMCD_DEVICE_CLASS(klass);
    device_class_set_parent_realize(dc, hisi_mmcd_realize,
                                    &vcxc->parent_realize);
    dc->desc = "VFIO HISI MMCD";
    dc->vmsd = &vfio_platform_hisi_mmcd_vmstate;
    /* Supported by TYPE_VIRT_MACHINE */
    dc->user_creatable = true;
}

static const TypeInfo vfio_hisi_mmcd_info = {
        .name = TYPE_VFIO_HISI_MMCD,
        .parent = TYPE_VFIO_PLATFORM,
        .instance_size = sizeof(VFIOHisiDevDevice),
        .class_init = vfio_hisi_mmcd_class_init,
        .class_size = sizeof(VFIOHisiDevDeviceClass),
};

static void register_hisi_mmcd_type(void)
{
    type_register_static(&vfio_hisi_mmcd_info);
}

type_init(register_hisi_mmcd_type)
