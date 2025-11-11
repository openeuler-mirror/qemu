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

#ifndef UB_UBC_H
#define UB_UBC_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/ub/hisi/ubc.h"
#include "hw/ub/ub_bus.h"

#define TYPE_BUS_CONTROLLER "ub-bus-controller"
OBJECT_DECLARE_TYPE(BusControllerState, BusControllerClass, BUS_CONTROLLER)

typedef struct BusControllerState BusControllerState;
struct BusControllerState {
    SysBusDevice busdev;

    MemoryRegion msgq_reg_mem; /* ubc msgq */
    uint32_t msgq_reg_size;
    uint8_t *msgq_reg;
    MemoryRegion fm_msgq_reg_mem; /* fm msgq */
    uint32_t fm_msgq_reg_size;
    uint8_t *fm_msgq_reg;
    MemoryRegion io_mmio; /* ub mmio hpa memory region */
    uint32_t mmio_size;
    bool mig_enabled;
    UBBus *bus;
    QLIST_ENTRY(BusControllerState) node;
};

struct BusControllerClass {
    SysBusDeviceClass parent_class;
};

void ub_save_ubc_list(BusControllerState *s);
#endif
