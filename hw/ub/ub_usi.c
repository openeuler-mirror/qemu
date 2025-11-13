/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "hw/ub/ub_usi.h"
#include "hw/ub/ub_config.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"

void usi_send_message(USIMessage *msg, uint32_t interrupt_id, UBDevice *udev)
{
    MemTxAttrs attrs = {};
    attrs.requester_id = interrupt_id;
    if (udev) {
        AddressSpace *as = ub_device_iommu_address_space(udev);
        address_space_stl_le(as, msg->address, msg->data,
                             attrs, NULL);
    } else {
        address_space_stl_le(&address_space_memory, msg->address, msg->data,
                             attrs, NULL);
    }
    qemu_log("usi notify success: interrupt_id %u eventid %u gicv3_its 0x%lx\n",
             interrupt_id, msg->data, msg->address);
}
