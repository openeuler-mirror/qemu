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

#include <pthread.h>
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/ub/hisi/ubc.h"
#include "hw/ub/ub_bus.h"

#define TYPE_BUS_CONTROLLER_DEV "ubc"
OBJECT_DECLARE_TYPE(BusControllerDev, BusControllerDevClass, BUS_CONTROLLER_DEV)

typedef struct BusControllerDev {
    UBDevice parent;
    UbGuid bus_instance_guid;
    int bus_instance_lock_fd;
} BusControllerDev;

struct BusControllerDevClass {
    UBDeviceClass parent_class;
};

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
    HiMsgqInfo msgq;
    BusControllerDev *ubc_dev;
    UBBus *bus;
    pthread_spinlock_t rq_cq_lock; /* protect RQ/CQ from concurrent access */
    QLIST_ENTRY(BusControllerState) node;
};

struct BusControllerClass {
    SysBusDeviceClass parent_class;
};

#define UBC_ERS0_SPACE_SIZE 0x2
#define UBC_ERS1_SPACE_SIZE 0x10001
#define UBC_ERS2_SPACE_SIZE 0x20
#define UBC_ERS0_SPACE_ADDR 0x2c00000000
#define UBC_ERS1_SPACE_ADDR 0x2d00000000
#define UBC_ERS2_SPACE_ADDR 0x2e00000000
#define UBC_EID_UPI_TEN_DEFAULT_VAL 1024
#define UBC_CLASS_CODE 0x0

void ub_save_ubc_list(BusControllerState *s);
BusControllerState *container_of_ubbus(UBBus *bus);
#endif
