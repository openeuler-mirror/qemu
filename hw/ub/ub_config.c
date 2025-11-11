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
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/arm/virt.h"
#include "hw/qdev-properties.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_config.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "qapi/error.h"

UbCfgAddrMapEntry *g_ub_cfg_addr_map_table = NULL;
uint32_t g_emulated_ub_cfg_size;

uint64_t ub_cfg_slice_start_offset[UB_CFG_EMULATED_SLICES_NUM] = {
    [CFG0_BASIC]        = 0x0,
    [CAP1_RSV]          = 0x100,
    [CAP2_SHP]          = 0x200,
    [CAP3_ERR_RECORD]   = 0x300,
    [CAP4_ERR_INFO]     = 0x400,
    [CAP5_EMQ]          = 0x500,
    [CFG1_BASIC]        = 0x10000,
    [CAP1_DECODER]      = 0x10100,
    [CAP2_JETTY]        = 0x10200,
    [CAP3_INT_TYPE1]    = 0x10300,
    [CAP4_INT_TYPE2]    = 0x10400,
    [CAP5_RSV]          = 0x10500,
    [CAP6_UB_MEM]       = 0x10600,
    [CFG0_PORT_BASIC]   = 0x20000,
    [CFG0_ROUTE_TABLE]  = 0xF0000000,
};

static void ub_cfg_display_addr_map_table(void)
{
    int i;

    for (i = 0; i < UB_CFG_SLICE_NUMS; i++) {
        qemu_log("map_table[%d]---start_addr: 0x%lx, mapped_offset: 0x%lx\n", i,
                 g_ub_cfg_addr_map_table[i].start_addr, g_ub_cfg_addr_map_table[i].mapped_offset);
    }
}

int ub_cfg_addr_map_table_init(void)
{
    int i, idx;

    /* used in all qemu lifecycle, be freed when qemu exit */
    g_ub_cfg_addr_map_table = malloc(UB_CFG_SLICE_NUMS * sizeof(UbCfgAddrMapEntry));
    if (!g_ub_cfg_addr_map_table) {
        qemu_log("failed to malloc for g_ub_cfg_addr_map_table\n");
        return -1;
    }

    /* fill general slice map table */
    for (i = 0; i < UB_CFG_GENERAL_SLICES_NUM; i++) {
        g_ub_cfg_addr_map_table[i].start_addr = ub_cfg_slice_start_offset[i];
        g_ub_cfg_addr_map_table[i].start_addr *= UB_CFG_START_OFFSET_GRANU;
        g_ub_cfg_addr_map_table[i].mapped_offset = i * UB_CFG_SLICE_SIZE;
    }

    /* fill port info slice map table */
    for (i = 0; i < UB_DEV_MAX_NUM_OF_PORT; i++) {
        idx = UB_CFG_GENERAL_SLICES_NUM + i;
        g_ub_cfg_addr_map_table[idx].start_addr = ub_cfg_slice_start_offset[CFG0_PORT_BASIC];
        g_ub_cfg_addr_map_table[idx].start_addr *= UB_CFG_START_OFFSET_GRANU;
        g_ub_cfg_addr_map_table[idx].start_addr += i * UB_PORT_SZ;
        g_ub_cfg_addr_map_table[idx].mapped_offset = idx * UB_CFG_SLICE_SIZE;
    }

    /* fill route table slice map table */
    idx = UB_CFG_GENERAL_SLICES_NUM + UB_DEV_MAX_NUM_OF_PORT;
    g_ub_cfg_addr_map_table[idx].start_addr = ub_cfg_slice_start_offset[CFG0_ROUTE_TABLE];
    g_ub_cfg_addr_map_table[idx].start_addr *= UB_CFG_START_OFFSET_GRANU;
    g_ub_cfg_addr_map_table[idx].mapped_offset = idx * UB_CFG_SLICE_SIZE;

    g_emulated_ub_cfg_size = UB_CFG_SLICE_NUMS * UB_CFG_SLICE_SIZE;
    qemu_log("each ub-dev emulated ub cfg size is 0x%x bytes\n", g_emulated_ub_cfg_size);

    return 0;
}

uint32_t ub_emulated_config_size(void)
{
    return g_emulated_ub_cfg_size;
}

uint64_t ub_cfg_offset_to_emulated_offset(uint64_t offset, bool check_success)
{
    uint64_t emulate_offset = UINT64_MAX;
    int i;
    uint64_t diff;

    for (i = 0; i < UB_CFG_SLICE_NUMS; i++) {
        if (offset < g_ub_cfg_addr_map_table[i].start_addr) {
            break;
        }

        diff = offset - g_ub_cfg_addr_map_table[i].start_addr;
        if (diff >= UB_CFG_SLICE_SIZE) {
            continue;
        }

        emulate_offset = g_ub_cfg_addr_map_table[i].mapped_offset + diff;
        break;
    }

    if (check_success) {
        if (emulate_offset == UINT64_MAX) {
            ub_cfg_display_addr_map_table();
            qemu_log("failed to convert offset 0x%lx to emulated offset\n", offset);
        }
        assert(emulate_offset != UINT64_MAX);
    }

    return emulate_offset;
}

void handle_msg_cfg(void *opaque, HiMsgSqe *sqe, void *payload)
{
}