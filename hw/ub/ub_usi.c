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

static void usi_init_vector_notifiers(UBDevice *udev,
                                      USIVectorUseNotifier use_notifier,
                                      USIVectorReleaseNotifier release_notifier,
                                      USIVectorPollNotifier poll_notifier)
{
    udev->usi_vector_use_notifier = use_notifier;
    udev->usi_vector_release_notifier = release_notifier;
    udev->usi_vector_poll_notifier = poll_notifier;
}

static int usi_set_notifier_for_vector(UBDevice *udev, uint16_t vector)
{
    USIMessage msg;

    if (usi_is_masked(udev, vector)) {
        return 0;
    }

    msg = usi_get_message(udev, vector);
    return udev->usi_vector_use_notifier(udev, vector, msg);
}

static void usi_unset_notifier_for_vector(UBDevice *udev, uint16_t vector)
{
    if (usi_is_masked(udev, vector)) {
        return;
    }
    udev->usi_vector_release_notifier(udev, vector);
}

void usi_unset_vector_notifiers(UBDevice *udev)
{
    int vector;

    for (vector = 0; vector < udev->usi_entries_nr; vector++) {
        usi_unset_notifier_for_vector(udev, vector);
    }

    udev->usi_vector_use_notifier = NULL;
    udev->usi_vector_release_notifier = NULL;
    udev->usi_vector_poll_notifier = NULL;
}

int usi_set_vector_notifiers(UBDevice *udev,
                             USIVectorUseNotifier use_notifier,
                             USIVectorReleaseNotifier release_notifier,
                             USIVectorPollNotifier poll_notifier)
{
    int vector, ret;

    usi_init_vector_notifiers(udev, use_notifier, release_notifier, poll_notifier);
    for (vector = 0; vector < udev->usi_entries_nr; vector++) {
        ret = usi_set_notifier_for_vector(udev, vector);
        if (ret < 0) {
            goto undo;
        }
    }

    qemu_log("usi set notifier for vector success.\n");
    return 0;

undo:
    qemu_log("usi set notifier for vector failed.\n");
    while (--vector >= 0) {
        usi_unset_notifier_for_vector(udev, vector);
    }
    udev->usi_vector_use_notifier = NULL;
    udev->usi_vector_release_notifier = NULL;
    return ret;
}

int usi_enabled(UBDevice *udev)
{
    uint64_t emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2_ENABLE_OFFSET, true);
    uint32_t *mask = (uint32_t *)(udev->config + emulated_offset);

    return (*mask) & UB_CFG1_CAP4_INT_TYPE2_ENABLEBIT;
}

USIMessage usi_get_message(UBDevice *udev, uint16_t vector)
{
    USIMessage msg;
    uint16_t addr_index;
    uint8_t *vec_table_entry = NULL;
    uint8_t *addr_table_entry = NULL;
    uint8_t *valid_byte = NULL;
    uint8_t valid_bit;

    vec_table_entry = udev->usi_vec_table + vector * USI_VEC_TABLE_ENTRY_SIZE;
    msg.data = ub_get_long(vec_table_entry);
    addr_index = ub_get_word(vec_table_entry + USI_VEC_TABLE_ADDR_INDEX_OFFSET);
    if (addr_index >= udev->usi_addr_table_nr) {
        qemu_log("address index exceed, the index is %u, total table num is %u\n",
                 addr_index, udev->usi_addr_table_nr);
        addr_index = udev->usi_addr_table_nr - 1;
    }

    addr_table_entry = udev->usi_addr_table + addr_index * USI_ADDR_TABLE_ENTRY_SIZE;
    /* check addr table entry is valid */
    msg.address = ub_get_quad(addr_table_entry);

    valid_byte = addr_table_entry + USI_ADDR_TABLE_VALID_BIT_OFFSET;
    valid_bit = ub_get_byte(valid_byte);
    valid_bit = valid_bit & USI_ADDR_TABLE_VALID_BIT_MASK;
    if (valid_bit == 0) {
        qemu_log("invalid interrupt address table, the index is %u\n", addr_index);
    }

    return msg;
}

static uint8_t usi_pending_mask(uint16_t vector)
{
    return 1 << (vector % 8);
}

static uint8_t *usi_pending_byte(UBDevice *udev, uint16_t vector)
{
    return udev->usi_pend_table + vector / 8;
}

int usi_is_pending(UBDevice *udev, uint16_t vector)
{
    return *usi_pending_byte(udev, vector) & usi_pending_mask(vector);
}

void usi_set_pending(UBDevice *udev, uint16_t vector)
{
    *usi_pending_byte(udev, vector) |= usi_pending_mask(vector);
}

void usi_clr_pending(UBDevice *udev, uint16_t vector)
{
    *usi_pending_byte(udev, vector) &= ~usi_pending_mask(vector);
}

static void usi_fire_vector_notifier(UBDevice *udev, uint16_t vector, bool is_masked)
{
    USIMessage msg;

    if (!udev->usi_vector_use_notifier) {
        qemu_log("usi_vector_use_notifier not init, do nothing.\n");
        return;
    }

    if (is_masked) {
        qemu_log("udev(%s %s) vector(%u) masked.\n",
                 udev->name, udev->qdev.id, vector);
        udev->usi_vector_release_notifier(udev, vector);
        return;
    }

    msg = usi_get_message(udev, vector);
    udev->usi_vector_use_notifier(udev, vector, msg);
}

static void usi_handle_mask_update(UBDevice *udev, uint16_t vector, bool was_masked)
{
    bool is_masked = usi_is_masked(udev, vector);

    if (is_masked == was_masked) {
        qemu_log("vector(%u) is_masked and was_masked equal, val is %d, "
                 "update do nothing.\n", vector, is_masked);
        return;
    }

    if (usi_ue_is_masked(udev)) {
        qemu_log("function entity is masked, vector(%u) mask update do nothing.\n", vector);
        return;
    }

    usi_fire_vector_notifier(udev, vector, is_masked);

    if (!is_masked && usi_is_pending(udev, vector)) {
        usi_clr_pending(udev, vector);
        usi_notify(udev, vector);
    }
}

static uint64_t usi_vec_table_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    UBDevice *udev = opaque;
    uint64_t val = UINT64_MAX;

    switch (size) {
        case BYTE_SIZE:
            val = ub_get_byte(udev->usi_vec_table + addr);
            break;
        case WORD_SIZE:
            val = ub_get_word(udev->usi_vec_table + addr);
            break;
        case DWORD_SIZE:
            val = ub_get_long(udev->usi_vec_table + addr);
            break;
        default:
            qemu_log("uxpect usi vec table read size %u.\n", size);
            break;
    }

    qemu_log("vec table read: addr(0x%lx), size(%u) value(0x%lx).\n", addr, size, val);
    return val;
}

static void usi_vec_table_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    UBDevice *udev = opaque;
    uint16_t vector = addr / USI_VEC_TABLE_ENTRY_SIZE;
    bool was_masked;

    was_masked = usi_is_masked(udev, vector);
    switch (size) {
        case BYTE_SIZE:
            ub_set_byte(udev->usi_vec_table + addr, val);
            break;
        case WORD_SIZE:
            ub_set_word(udev->usi_vec_table + addr, val);
            break;
        case DWORD_SIZE:
            ub_set_long(udev->usi_vec_table + addr, val);
            break;
        default:
            qemu_log("uxpect usi vec table write size %u.\n", size);
            break;
    }

    qemu_log("vec table update: addr(0x%lx), size(%u), val(0x%lx).\n", addr, size, val);
    usi_handle_mask_update(udev, vector, was_masked);
}

static const MemoryRegionOps usi_vec_table_mmio_ops = {
    .read = usi_vec_table_mmio_read,
    .write = usi_vec_table_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static uint64_t usi_addr_table_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    UBDevice *udev = opaque;
    uint64_t val = UINT64_MAX;

    switch (size) {
        case BYTE_SIZE:
            val = ub_get_byte(udev->usi_addr_table + addr);
            break;
        case WORD_SIZE:
            val = ub_get_word(udev->usi_addr_table + addr);
            break;
        case DWORD_SIZE:
            val = ub_get_long(udev->usi_addr_table + addr);
            break;
        default:
            qemu_log("uxpect usi addr table read size %u.\n", size);
            break;
    }

    qemu_log("addr table read: addr(0x%lx), size(%u) value(0x%lx).\n", addr, size, val);
    return val;
}

static void usi_addr_table_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    UBDevice *udev = opaque;

    switch (size) {
        case BYTE_SIZE:
            ub_set_byte(udev->usi_addr_table + addr, val);
            break;
        case WORD_SIZE:
            ub_set_word(udev->usi_addr_table + addr, val);
            break;
        case DWORD_SIZE:
            ub_set_long(udev->usi_addr_table + addr, val);
            break;
        default:
            qemu_log("uxpect usi addr table write size %u.\n", size);
            break;
    }

    qemu_log("usi addr table update: addr(0x%lx), size(%u), val(0x%lx).\n",
             addr, size, val);
}

static const MemoryRegionOps usi_addr_table_mmio_ops = {
    .read = usi_addr_table_mmio_read,
    .write = usi_addr_table_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    }
};

static uint64_t usi_pend_table_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    UBDevice *udev = opaque;
    uint64_t val = UINT64_MAX;

    switch (size) {
        case BYTE_SIZE:
            val = ub_get_byte(udev->usi_pend_table + addr);
            break;
        case WORD_SIZE:
            val = ub_get_word(udev->usi_pend_table + addr);
            break;
        case DWORD_SIZE:
            val = ub_get_long(udev->usi_pend_table + addr);
            break;
        default:
            qemu_log("expect usi pend addr table read size %u.\n", size);
            break;
    }

    qemu_log("pend table read: addr(0x%lx), size(%u) value(0x%lx).\n", addr, size, val);

    return val;
}

static void usi_pend_table_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    /* do nothing now */
}

static const MemoryRegionOps usi_pend_table_mmio_ops = {
    .read = usi_pend_table_mmio_read,
    .write = usi_pend_table_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    }
};

bool usi_is_masked(UBDevice *udev, uint16_t vector)
{
    uint32_t offset = (uint32_t)vector * USI_VEC_TABLE_ENTRY_SIZE + USI_VEC_TABLE_MASK_OFFSET;

    return udev->usi_vec_table[offset] & USI_VEC_TABLE_MASKBIT;
}

static void usi_mask_all(UBDevice *udev, uint16_t entries)
{
    uint16_t vector;
    uint32_t offset;
    bool was_masked;

    for (vector = 0; vector < entries; vector++) {
        offset = (uint32_t)vector * USI_VEC_TABLE_ENTRY_SIZE + USI_VEC_TABLE_MASK_OFFSET;
        was_masked = usi_is_masked(udev, vector);
        udev->usi_vec_table[offset] |= USI_VEC_TABLE_MASKBIT;
        usi_handle_mask_update(udev, vector, was_masked);
    }
}

static void usi_set_disable(UBDevice *udev)
{
    uint64_t emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2_ENABLE_OFFSET, true);
    uint32_t *val = (uint32_t *)(udev->config + emulated_offset);
    memset(val, 0, sizeof(uint32_t));

    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2_MASK_OFFSET, true);
    val = (uint32_t *)(udev->config + emulated_offset);
    memset(val, 0, sizeof(uint32_t));
    (*val) |= UB_CFG1_CAP4_INT_TYPE2_MASKBIT;
    qemu_log("ub device(%s %s) disable usi\n", udev->name, udev->qdev.id);
}

static void usi_clear_all_vectors(UBDevice *dev)
{
    int vector;

    for (vector = 0; vector < dev->usi_entries_nr; ++vector) {
        usi_clr_pending(dev, vector);
    }
}

void usi_reset(UBDevice *udev)
{
    uint32_t pend_table_size = DIV_ROUND_UP(udev->usi_entries_nr, USI_PEND_TABLE_ENTRY_BIT_NUM) *
                                            USI_PEND_TABLE_ENTRY_SIZE;
    usi_clear_all_vectors(udev);
    memset(udev->usi_vec_table, 0, udev->usi_entries_nr * USI_VEC_TABLE_ENTRY_SIZE);
    memset(udev->usi_addr_table, 0, udev->usi_addr_table_nr * USI_ADDR_TABLE_ENTRY_SIZE);
    memset(udev->usi_pend_table, 0, pend_table_size);

    usi_mask_all(udev, udev->usi_entries_nr);
    usi_set_disable(udev);
}

void usi_init(UBDevice *udev, uint16_t vec_table_num, uint16_t addr_table_num,
              uint64_t vec_table_start_addr, uint64_t addr_table_start_addr,
              uint64_t pend_table_start_addr, MemoryRegion *fer0_mr)
{
    uint32_t vec_table_size, addr_table_size, pend_table_size;

    vec_table_size = (uint32_t)vec_table_num * USI_VEC_TABLE_ENTRY_SIZE;
    addr_table_size = (uint32_t)addr_table_num * USI_ADDR_TABLE_ENTRY_SIZE;
    pend_table_size = DIV_ROUND_UP(vec_table_num, USI_PEND_TABLE_ENTRY_BIT_NUM) *
                                   USI_PEND_TABLE_ENTRY_SIZE;

    udev->usi_entries_nr = vec_table_num;
    udev->usi_addr_table_nr = addr_table_num;
    udev->usi_vec_table = g_malloc0(vec_table_size);
    udev->usi_addr_table = g_malloc0(addr_table_size);
    udev->usi_pend_table = g_malloc0(pend_table_size);

    usi_mask_all(udev, vec_table_num);
    usi_set_disable(udev);

    memory_region_init_io(&udev->usi_vec_table_mmio, OBJECT(udev), &usi_vec_table_mmio_ops,
                          udev, "usi-vec-table", vec_table_size);
    memory_region_add_subregion(fer0_mr, vec_table_start_addr, &udev->usi_vec_table_mmio);
    memory_region_init_io(&udev->usi_addr_table_mmio, OBJECT(udev), &usi_addr_table_mmio_ops,
                          udev, "usi-addr-table", addr_table_size);
    memory_region_add_subregion(fer0_mr, addr_table_start_addr, &udev->usi_addr_table_mmio);
    memory_region_init_io(&udev->usi_pend_table_mmio, OBJECT(udev), &usi_pend_table_mmio_ops,
                          udev, "usi-pend-table", pend_table_size);
    memory_region_add_subregion(fer0_mr, pend_table_start_addr, &udev->usi_pend_table_mmio);
}

void usi_uninit(UBDevice *udev, MemoryRegion *fer0_mr)
{
    memory_region_del_subregion(fer0_mr, &udev->usi_vec_table_mmio);
    g_free(udev->usi_vec_table);

    memory_region_del_subregion(fer0_mr, &udev->usi_addr_table_mmio);
    g_free(udev->usi_addr_table);

    memory_region_del_subregion(fer0_mr, &udev->usi_pend_table_mmio);
    g_free(udev->usi_pend_table);
}

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

void usi_notify(UBDevice *udev, uint16_t vector)
{
    USIMessage msg;

    /* check vector is valid later */

    if (usi_is_masked(udev, vector) || usi_ue_is_masked(udev)) {
        usi_set_pending(udev, vector);
        return;
    }

    msg = usi_get_message(udev, vector);
    usi_send_message(&msg, ub_interrupt_id(udev), udev);
}

int usi_ue_is_masked(UBDevice *udev)
{
    uint64_t emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_CAP4_INT_TYPE2_MASK_OFFSET, true);
    uint32_t *mask = (uint32_t *)(udev->config + emulated_offset);

    return (*mask) & UB_CFG1_CAP4_INT_TYPE2_MASKBIT;
}

static void usi_ue_each_vector_update(UBDevice *udev, uint16_t vector, bool ue_is_masked)
{
    USIMessage msg;

    if (usi_is_masked(udev, vector)) {
        qemu_log("vector(%u) is masked, do nothing.\n", vector);
        return;
    }

    if (!udev->usi_vector_use_notifier) {
        qemu_log("usi_vector_use_notifier not init, do nothing.\n");
        return;
    }

    if (ue_is_masked) {
        udev->usi_vector_release_notifier(udev, vector);
        return;
    }

    msg = usi_get_message(udev, vector);
    udev->usi_vector_use_notifier(udev, vector, msg);

    if (usi_is_pending(udev, vector)) {
        qemu_log("start udev(%s) vector(%u) pending interrupt notify.\n", udev->name, vector);
        usi_clr_pending(udev, vector);
        usi_notify(udev, vector);
    }
}

void usi_handle_ue_mask_update(UBDevice *udev, bool was_masked)
{
    bool is_masked = usi_ue_is_masked(udev);
    uint16_t vector;

    if (is_masked == was_masked) {
        qemu_log("ue is_masked and was_masked equal, val is %d, "
                 "update do nothing.\n", is_masked);
        return;
    }

    for (vector = 0; vector < udev->usi_entries_nr; vector++) {
        usi_ue_each_vector_update(udev, vector, is_masked);
    }
}