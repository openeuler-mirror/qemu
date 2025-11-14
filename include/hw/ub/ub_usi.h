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
#ifndef UB_USI_H
#define UB_USI_H
#include "qemu/typedefs.h"
#include "hw/ub/ub.h"

struct USIMessage {
    uint64_t address;
    uint32_t data;
};

void usi_init(UBDevice *udev, uint16_t vec_table_num, uint16_t addr_table_num,
              uint64_t vec_table_start_addr, uint64_t addr_table_start_addr,
              uint64_t pend_table_start_addr, MemoryRegion *fer0_mr);
void usi_uninit(UBDevice *udev, MemoryRegion *fer0_mr);
bool usi_is_masked(UBDevice *udev, uint16_t vector);
USIMessage usi_get_message(UBDevice *udev, uint16_t vector);
int usi_enabled(UBDevice *udev);
int usi_set_vector_notifiers(UBDevice *udev,
                             USIVectorUseNotifier use_notifier,
                             USIVectorReleaseNotifier release_notifier,
                             USIVectorPollNotifier poll_notifier);
void usi_unset_vector_notifiers(UBDevice *udev);
void usi_notify(UBDevice *udev, uint16_t vector);
int usi_is_pending(UBDevice *udev, uint16_t vector);
void usi_set_pending(UBDevice *udev, uint16_t vector);
void usi_clr_pending(UBDevice *udev, uint16_t vector);
int usi_ue_is_masked(UBDevice *udev);
void usi_handle_ue_mask_update(UBDevice *udev, bool was_masked);
void usi_send_message(USIMessage *msg, uint32_t interrupt_id, UBDevice *udev);
void usi_reset(UBDevice *dev);

#endif
