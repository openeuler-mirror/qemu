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

#ifndef UB_BUS_H
#define UB_BUS_H

#include "hw/ub/ub.h"

enum EnumTlvType { /* M: Mandatory , O: optional */
    TLV_SLICE_INFO = 0, /* M */
    TLV_PORT_NUM = 1, /* M */
    TLV_PORT_INFO = 2, /* M */
    TLV_RSV0 = 3, /* O */
    TLV_CAP_INFO = 4, /* M */
    TLV_RSV1 = 5, /* O */
    TLV_RSV2 = 6, /* O */
};

struct UBBusClass {
    /* < private > */
    BusClass parent_class;
    /* < public > */
};

typedef QLIST_HEAD(, UBDevice) UBDeviceList;
struct UBBus {
    BusState qbus;
    UBDeviceList devices;
    MemoryRegion *address_space_mem;
    const UBIOMMUOps *iommu_ops;
    void *iommu_opaque;
};

#define TYPE_UB_BUS "UB_BUS"
OBJECT_DECLARE_TYPE(UBBus, UBBusClass, UB_BUS)

UBBus *ub_register_root_bus(DeviceState *parent, const char *name,
                            MemoryRegion *io_mmio);
void ub_unregister_root_bus(UBBus *bus);
UBDevice *ub_find_device_by_eid(UBBus *bus, uint32_t eid);
static inline UBBus *ub_get_bus(const UBDevice *dev)
{
    return UB_BUS(qdev_get_parent_bus(DEVICE(dev)));
}
#endif
