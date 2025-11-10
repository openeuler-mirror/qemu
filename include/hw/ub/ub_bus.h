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
};

#define TYPE_UB_BUS "UB_BUS"
OBJECT_DECLARE_TYPE(UBBus, UBBusClass, UB_BUS)

#endif
