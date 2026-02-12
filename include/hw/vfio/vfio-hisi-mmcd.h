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

#ifndef HW_VFIO_HISI_MMCD_H
#define HW_VFIO_HISI_MMCD_H

#include "hw/vfio/vfio-platform.h"
#include "qom/object.h"

#define TYPE_VFIO_HISI_MMCD "vfio-hisi-mmcd"

/**
 * This device exposes:
 * -3 MMIO regions: ub regs
 */
struct VFIOHisiDevDevice {
    VFIOPlatformDevice vdev;
};

typedef struct VFIOHisiDevDevice VFIOHisiDevDevice;

struct VFIOHisiDevDeviceClass {
    /*< private >*/
    VFIOPlatformDeviceClass parent_class;
    /*< public >*/
    DeviceRealize parent_realize;
};

typedef struct VFIOHisiDevDeviceClass VFIOHisiDevDeviceClass;

DECLARE_OBJ_CHECKERS(VFIOHisiDevDevice, VFIOHisiDevDeviceClass,
                     VFIO_HISI_MMCD_DEVICE, TYPE_VFIO_HISI_MMCD)

#endif
