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
            assert(emulate_offset != UINT64_MAX);
        }
    }

    return emulate_offset;
}

static uint32_t ub_dev_get_cna(UBDevice *dev)
{
    uint64_t emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_NA_INFO_START, true);
    ConfigNetAddrInfo *net_addr_info = (ConfigNetAddrInfo *)(dev->config + emulated_offset);
    uint32_t dev_cna = net_addr_info->primary_cna;
    return dev_cna;
}

static UBDevice *ub_find_device_by_cna(UBBus *bus, uint32_t dcna)
{
    UBDevice *dev;

    QLIST_FOREACH(dev, &bus->devices, node) {
        if (ub_dev_get_cna(dev) == dcna) {
            return dev;
        }
    }

    return NULL;
}

static void ub_cfg_msg_fill_cq_rq(BusControllerState *s, HiMsgSqe *sqe, MsgPktHeader *header,
                                  CfgMsgPkt *rsp_pkt)
{
    HiMsgCqe cqe;

    memset(&cqe, 0, sizeof(cqe));
    cqe.type = MSG_RSP;
    cqe.msg_code = UB_MSG_CODE_CFG;
    cqe.sub_msg_code = header->msgetah.sub_msg_code;
    rsp_pkt->header.msgetah.type = MSG_RSP;
    rsp_pkt->header.msgetah.sub_msg_code = header->msgetah.sub_msg_code;

    rsp_pkt->header.nth.scna = header->nth.dcna;
    rsp_pkt->header.nth.dcna = header->nth.scna;
    rsp_pkt->header.deid = EID_GEN(header->seid_h, header->seid_l);
    rsp_pkt->header.seid_h = EID_HIGH(header->deid);
    rsp_pkt->header.seid_l = EID_LOW(header->deid);
    cqe.msn = sqe->msn;
    cqe.p_len = MSG_CFG_PKT_SIZE;
    cqe.status = CQE_SUCCESS;
    if (fill_rq_cq(s, rsp_pkt, sizeof(CfgMsgPkt), &cqe) != 0) {
        qemu_log("ub_cfg_msg_fill_cq_rq: fill_rq_cq failed\n");
    }
}

static uint32_t get_dw_mask(uint8_t byte_enable)
{
    uint32_t dw_mask = 0;
    uint32_t bt_mask = 0xff;

    byte_enable &= 0x0f; // for dword, only lower four bits are valid
    while (byte_enable) {
        if (byte_enable & 0x1) {
            dw_mask |= bt_mask;
        }
        bt_mask <<= BITS_PER_BYTE;
        byte_enable >>= 1;
    }
    return dw_mask;
}

static int err_to_msg_rsp(int err)
{
    if (err <= 0)
        return err;

    switch (err) {
    case ENOMEM:
        return UB_MSG_RSP_EXEC_ENOMEM;
    case EACCES:
        return UB_MSG_RSP_EXEC_EACCES;
    case EFAULT:
        return UB_MSG_RSP_EXEC_EFAULT;
    case EBUSY:
        return UB_MSG_RSP_EXEC_EBUSY;
    case ENODEV:
        return UB_MSG_RSP_EXEC_ENODEV;
    case EINVAL:
        return UB_MSG_RSP_EXEC_EINVAL;
    case ENOEXEC:
        return UB_MSG_RSP_EXEC_ENOEXEC;
    default:
        return UB_MSG_RSP_UNKNOWN;
    }
}

static void ub_cfg_rw(BusControllerState *s, HiMsgSqe *sqe,
                      MsgPktHeader *header)
{
    CfgMsgPldReq *payload = NULL;
    CfgMsgPkt rsp_pkt;
    uint32_t local = sqe->local;
    uint64_t cfg_offset = 0;
    uint32_t entity = 0;
    uint32_t dcna = header->nth.dcna;
    UBDevice *ub_dev = NULL;
    uint32_t dw_mask;
    uint64_t emulated_offset;
    int ret;

    if (header->msgetah.plen != CFG_MSG_PLD_SIZE) {
        qemu_log("invalid len %u, please check driver inside guestos\n",
                 header->msgetah.plen);
        return;
    }
    payload = (CfgMsgPldReq *)header->payload;
    cfg_offset = (uint64_t)payload->req_addr * DWORD_SIZE;
    entity = payload->entity_idx;
    memset(&rsp_pkt, 0, sizeof(CfgMsgPkt));
    memcpy(&rsp_pkt.header, header, sizeof(MsgPktHeader));

    /* vm support only FE0(entity_idx = 0) */
    if (entity) {
        qemu_log("vm support only FE0, entity idx: %u\n", entity);
        rsp_pkt.header.msgetah.rsp_status = UB_MSG_RSP_CMD_ENODEV;
        goto fill_rq_cq;
    }
    /*
     * TODO: Check whether dcna is the unique identifier of the device when the configuration space is read or written.
     * local = 1: ubc or idev, dcna = 0: default to ubc
     * local = 0: ub devices
     */
    if (local && !dcna) {
        ub_dev = UB_DEVICE(s->ubc_dev);
        if (!ub_dev) {
            qemu_log("ubc not config?\n");
            return;
        }
    } else {
        ub_dev = ub_find_device_by_cna(s->bus, dcna);
        if (!ub_dev) {
            qemu_log("device not found. dcna %u\n", dcna);
            return;
        }
    }

    if (cfg_offset >= ub_config_size()) {
        rsp_pkt.header.msgetah.rsp_status = UB_MSG_RSP_EXEC_EFAULT;
        goto fill_rq_cq;
    }
    dw_mask = get_dw_mask(payload->byte_enable);
    switch (header->msgetah.sub_msg_code) {
    case UB_CFG0_READ:
    case UB_CFG1_READ:
        if (ub_dev->config_read) {
            ret = ub_dev->config_read(ub_dev, cfg_offset, &rsp_pkt.pld.rsp.read_data, dw_mask);
            rsp_pkt.header.msgetah.rsp_status = err_to_msg_rsp(ret);
        } else {
            rsp_pkt.header.msgetah.rsp_status = UB_MSG_RSP_EXEC_ENOEXEC;
            qemu_log("dev: %s read config func NULL\n", ub_dev->qdev.id);
        }
        break;
    case UB_CFG0_WRITE:
    case UB_CFG1_WRITE:
        emulated_offset = ub_cfg_offset_to_emulated_offset(cfg_offset, false);
        if (emulated_offset != UINT64_MAX && !*((uint32_t *)(&ub_dev->wmask[emulated_offset]))) {
            rsp_pkt.header.msgetah.rsp_status = UB_MSG_RSP_EXEC_EACCES;
            qemu_log("register cannot be written.\n");
            goto fill_rq_cq;
        }
        if (ub_dev->config_write) {
            ret = ub_dev->config_write(ub_dev, cfg_offset, &payload->write_data, dw_mask);
            rsp_pkt.header.msgetah.rsp_status = err_to_msg_rsp(ret);
        } else {
            rsp_pkt.header.msgetah.rsp_status = UB_MSG_RSP_EXEC_ENOEXEC;
            qemu_log("dev: %s write config func NULL\n", ub_dev->qdev.id);
        }
        break;
    default:
        break;
    }
fill_rq_cq:
    ub_cfg_msg_fill_cq_rq(s, sqe, header, &rsp_pkt);
}

void handle_msg_cfg(void *opaque, HiMsgSqe *sqe, void *payload)
{
    BusControllerState *s = opaque;
    MsgPktHeader *header = (MsgPktHeader *)payload;
    MsgExtendedHeader *msgetah = &header->msgetah;

    if (msgetah->msg_code != UB_MSG_CODE_CFG ||
        msgetah->sub_msg_code >= UB_CFG_MAX_SUB_MSG_CODE) {
        qemu_log("invalid msg code %u or sub msg code %u, "
                 "please check the driver inside guestos\n",
                 msgetah->msg_code, msgetah->sub_msg_code);
        return;
    }

    ub_cfg_rw(s, sqe, header);
}