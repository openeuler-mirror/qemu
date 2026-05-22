/*
 * Copyright (c) 2023-2023 HUAWEI TECHNOLOGIES CO.,LTD.
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
#ifndef HW_VFIO_VFIO_UB_H
#define HW_VFIO_VFIO_UB_H

#include "exec/memory.h"
#include "hw/ub/ub.h"
#include "hw/vfio/vfio-common.h"
#include "qemu/event_notifier.h"
#include "qemu/queue.h"
#include "qemu/timer.h"
#include "qom/object.h"
#include "sysemu/kvm.h"

#define UB_ANY_ID (~0)

#define TYPE_VFIO_UB "vfio-ub"
OBJECT_DECLARE_SIMPLE_TYPE(VFIOUBDevice, VFIO_UB)
#define VFIO_UB_SAFE(UBDevice) \
 ((UBDevice)->host_dev ? VFIO_UB(UBDevice) : NULL)

typedef struct VFIOERS {
    VFIORegion region;
    MemoryRegion *mr;
    size_t size;
    QLIST_HEAD(, VFIOQuirk) quirks;
} VFIOERS;

typedef struct VFIOUSIVector {
    EventNotifier interrupt;
    EventNotifier kvm_interrupt;
    struct VFIOUBDevice *vdev;
    int virq;
    bool use;
} VFIOUSIVector;

typedef struct VFIOUSIInfo {
    uint16_t vec_table_num;
    uint16_t addr_table_num;
    uint64_t vec_table_start_addr;
    uint64_t addr_table_start_addr;
    uint64_t pend_table_start_addr;
} VFIOUSIInfo;

struct VFIOUBDevice {
    UBDevice udev;
    VFIODevice vbasedev;
    unsigned int config_size;
    uint8_t *emulated_config_bits; /* QEMU emulated bits, little-endian */
    off_t config_offset; /* Offset of config space region within device fd */
    VFIOERS ers[UB_NUM_REGIONS];
    VFIOUSIInfo *usi;
    VFIOUSIVector *usi_vectors;
    int nr_vectors; /* Number of usi vectors currently in use */

    /* reinit irq */
    EventNotifier reinit_irq;
    bool reinit_irq_enabled;

    UBHostDeviceAddress host;
};
#endif /* HW_VFIO_VFIO_UB_H */