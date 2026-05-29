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
#include "hw/qdev-properties.h"
#include "hw/ub/hisi/ubc.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_config.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_enum.h"
#include "hw/ub/ub_cna_mgmt.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "trace.h"
#include "sysemu/dma.h"
#include "hw/ub/ub_cna_mgmt.h"

static void enum_get_port_info_from_config_space(UBDevice *dev, uint16_t port_idx,
                                                 EnumTlvPortInfo *port_info)
{
    uint64_t offset;
    uint64_t emulated_offset;
    ConfigPortBasic *port_basic = NULL;

    if (port_idx >= dev->port.port_num) {
        qemu_log("unexpect port_idx(%u) > udev(%s) port_num(%u)\n",
                 port_idx, dev->qdev.id, dev->port.port_num);
        return;
    }

    offset = UB_PORT_SLICE_START + port_idx * UB_PORT_SZ;
    emulated_offset = ub_cfg_offset_to_emulated_offset(offset, true);
    port_basic = (ConfigPortBasic *)(dev->config + emulated_offset);
    memset(port_info, 0, sizeof(EnumTlvPortInfo));
    port_info->bits0.len = sizeof(EnumTlvPortInfo);
    port_info->bits0.type = TLV_PORT_INFO;
    port_info->bits0.w = 1;
    if (!memcmp(&port_basic->neighbor_port_info.neighbot_port_guid,
                &port_info->remote_guid, sizeof(UbGuid))) {
        port_info->bits0.s = UB_PORT_STATUS_DOWN;
    } else {
        /* dw0 */
        port_info->bits0.s = UB_PORT_STATUS_UP;
        port_info->bits0.b = port_basic->port_info.enum_boundary;
        port_info->bits0.t = port_basic->port_info.port_type;
        /* dw1 */
        port_info->remote_port_idx = port_basic->neighbor_port_info.neighbor_port_idx;
        port_info->local_port_idx = port_basic->port_info.port_idx;
        /* dw3~dw6 */
        port_info->remote_guid = port_basic->neighbor_port_info.neighbot_port_guid;
    }
}

static void enum_query_set_rsp_port_info(EnumTopoQueryRspPdu *rsp_pdu, uint16_t num_ports,
                                         uint16_t start_port_idx, UBDevice *dev)
{
    EnumTlvPortInfo port_info;
    uint32_t port_idx = start_port_idx;
    for (uint32_t idx = 0; idx < num_ports; ++idx) {
        uint8_t *dst_port_info_ptr = (uint8_t *)rsp_pdu->port_info + idx * sizeof(EnumTlvPortInfo);
        enum_get_port_info_from_config_space(dev, port_idx, &port_info);
        memcpy(dst_port_info_ptr, &port_info, sizeof(EnumTlvPortInfo));
        port_idx++;
    }
}

static uint32_t enum_query_get_slice0_resv_size(void)
{
    uint32_t size = 0;

    size += sizeof(EnumTlvPortNum);
    size += sizeof(EnumTlvSliceInfo);
    size += sizeof(EnumTlvCapInfo);

    return size;
}

static uint16_t enum_query_get_max_num_ports(void)
{
    return (ENUM_TOPO_QUERY_RSP_PDU_MAX_LEN - ENUM_PLD_SCAN_PDU_COMMON_SIZE -
            enum_query_get_slice0_resv_size()) / sizeof(EnumTlvPortInfo);
}

static size_t enum_query_get_rsp_size(EnumPldScanHeader *scan_header, uint16_t rsp_num_ports, uint8_t slice_id)
{
    size_t size;

    size = sizeof(EnumTopoQueryRsp) + calc_forward_path_size(scan_header) +
           rsp_num_ports * sizeof(EnumTlvPortInfo);
    if (slice_id == 0) {
        size += enum_query_get_slice0_resv_size();
    }

    return size;
}

static uint32_t enum_query_get_rsp_pdu_len(uint16_t rsp_num_ports, uint8_t slice_id)
{
    uint32_t size;

    size = ENUM_PLD_SCAN_PDU_COMMON_SIZE + rsp_num_ports * sizeof(EnumTlvPortInfo);
    if (slice_id == 0) {
        size += enum_query_get_slice0_resv_size();
    }

    return size / DWORD_SIZE;
}

static void enum_query_set_rsp_port_num(EnumTopoQueryRspPdu *rsp_pdu, uint16_t rsp_num_ports, UBDevice *dev)
{
    EnumTlvPortNum *tlv_port_num = NULL;

    tlv_port_num = (EnumTlvPortNum *)((uint8_t *)rsp_pdu + ENUM_PLD_SCAN_PDU_COMMON_SIZE +
                                       rsp_num_ports * sizeof(EnumTlvPortInfo));
    tlv_port_num->type = TLV_PORT_NUM;
    tlv_port_num->len = sizeof(EnumTlvPortNum);
    tlv_port_num->total_num_ports = dev->port.port_num;

    trace_enum_query_set_rsp_port_num(tlv_port_num->total_num_ports);
}

static void enum_query_set_rsp_slice_info(EnumTopoQueryRspPdu *rsp_pdu, uint16_t rsp_num_ports, uint8_t total_slice)
{
    EnumTlvSliceInfo *tlv_slice_info = NULL;

    tlv_slice_info = (EnumTlvSliceInfo *)((uint8_t *)rsp_pdu + ENUM_PLD_SCAN_PDU_COMMON_SIZE +
                                          rsp_num_ports * sizeof(EnumTlvPortInfo) + sizeof(EnumTlvPortNum));
    tlv_slice_info->type = TLV_SLICE_INFO;
    tlv_slice_info->len = sizeof(EnumTlvSliceInfo);
    tlv_slice_info->total_slice = total_slice;
}

static void enum_query_set_rsp_cap_info(EnumTopoQueryRspPdu *rsp_pdu, uint16_t rsp_num_ports, UBDevice *dev)
{
    EnumTlvCapInfo *tlv_cap_info = NULL;
    UbCfg1Basic *cfg1_basic;
    uint64_t emulated_offset;

    tlv_cap_info = (EnumTlvCapInfo *)((uint8_t *)rsp_pdu + ENUM_PLD_SCAN_PDU_COMMON_SIZE +
                                      rsp_num_ports * sizeof(EnumTlvPortInfo) + sizeof(EnumTlvPortNum) +
                                      sizeof(EnumTlvSliceInfo));
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG1_BASIC_START, true);
    cfg1_basic = (UbCfg1Basic *)(dev->config + emulated_offset);
    tlv_cap_info->type = TLV_CAP_INFO;
    tlv_cap_info->len = sizeof(EnumTlvCapInfo);
    tlv_cap_info->class_code = cfg1_basic->class_code;
    /* now cap add nothing */
}

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
static void handle_enum_query_request(BusControllerState *s, HiMsgSqe *sqe,
                                      void *buf)
{
    /* req message */
    void *payload;
    size_t header_sz;
    size_t total_sz;
    EnumPktHeader *header;
    struct ClanNetworkHeader *cnth;
    struct UbLinkHeader *ulh;
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    EnumPldScanHeader *scan_header;
    EnumTopoQueryReq *scan_pdu;
    EnumPldScanPduCommon *scan_pdu_com;
    UBDevice *dev;
    uint16_t port_idx_start, remain_num_ports, max_num_ports, rsp_num_ports;
    uint8_t slice_id, total_slice;
    /* rsp  message */
    size_t rsp_size;
    void *rsp_buf;
    EnumPktHeader *rsp_pkt_hdr;
    EnumPldScanHeader *rsp_scan_header;
    EnumTopoQueryRspPdu *rsp_pdu;
    HiMsgCqe cqe;

    assert(HI_MSG_SQE_PLD_SIZE > ENUM_PKT_HEADER_SIZE + sizeof(EnumPldScanHeader));
    scan_header = g_malloc0(sizeof(EnumPldScanHeader));
    if (dma_memory_read(&address_space_memory,
                        (unsigned long)(buf + ENUM_PKT_HEADER_SIZE),
                        scan_header, sizeof(EnumPldScanHeader), MEMTXATTRS_MEMORY)) {
        qemu_log("Failed to read sq_base_addr_gpa entry\n");
        g_free(scan_header);
        return;
    }

    total_sz = ENUM_PKT_HEADER_SIZE + calc_enum_pld_header_size(scan_header, true) + ENUM_TOPO_QUERY_REQ_SIZE;
    if (HI_MSG_SQE_PLD_SIZE < total_sz) {
        qemu_log("unexpect msgq sqe pld size(0x%x) < prepare read payload size(0x%lx)\n",
                 HI_MSG_SQE_PLD_SIZE, total_sz);
        g_free(scan_header);
        return;
    }

    g_free(scan_header);
    payload = g_malloc0(total_sz);
    if (dma_memory_read(&address_space_memory, (unsigned long)(buf),
                        payload, total_sz, MEMTXATTRS_MEMORY)) {
        qemu_log("Failed to read sq_base_addr_gpa entry\n");
        g_free(payload);
        return;
    }

    header = (EnumPktHeader *)payload;
    cnth = &header->cnth;
    ulh = &header->ulh;

    if (ulh->cfg != UB_CLAN_LINK_CFG || cnth->nth_nlp != NTH_NLP_WITHOUT_TPH ||
        header->upi != UB_CP_UPI) {
        qemu_log("invalid enum pkt header, please check the driver inside guestos:"
                 " cfg %u nth_nlp %u upi 0x%x\n", ulh->cfg, cnth->nth_nlp, header->upi);
        g_free(payload);
        return;
    }

    scan_header = (EnumPldScanHeader *)((uint8_t *)payload + ENUM_PKT_HEADER_SIZE);
    header_sz = ENUM_PKT_HEADER_SIZE + calc_enum_pld_header_size(scan_header, true);
    if (header_sz + ENUM_TOPO_QUERY_REQ_SIZE > total_sz) {
        qemu_log("calculate incorrect header size %lu, expect %lu\n",
                 header_sz, total_sz - ENUM_TOPO_QUERY_REQ_SIZE);
        g_free(payload);
        return;
    }

    scan_pdu = (EnumTopoQueryReq *)((uint8_t *)payload + header_sz);
    scan_pdu_com = (EnumPldScanPduCommon *)scan_pdu;
    ub_device_get_str_from_guid(&scan_pdu_com->guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);
    dev = ub_find_device_by_guid(&scan_pdu_com->guid);
    if (!dev) {
        qemu_log("can not find device by guid %s\n", guid_str);
        g_free(payload);
        return;
    }

    slice_id = scan_pdu->common.bits.slice_id;
    max_num_ports = enum_query_get_max_num_ports();
    port_idx_start = slice_id * max_num_ports;

    remain_num_ports = dev->port.port_num - port_idx_start;
    rsp_num_ports = remain_num_ports > max_num_ports ? max_num_ports : remain_num_ports;
    trace_handle_enum_query_request(scan_header->bits.hops, scan_pdu_com->bits.opcode,
                                    port_idx_start, rsp_num_ports, max_num_ports, guid_str);

    /* response includes forward path but not return path. */
    rsp_size = enum_query_get_rsp_size(scan_header, rsp_num_ports, slice_id);
    rsp_buf = g_malloc(rsp_size);
    memset(rsp_buf, 0, rsp_size);
    rsp_pkt_hdr = (EnumPktHeader *)rsp_buf;
    memcpy(rsp_pkt_hdr, header, sizeof(EnumPktHeader));
    rsp_scan_header = (EnumPldScanHeader *)(rsp_buf + ENUM_PKT_HEADER_SIZE);
    memcpy(rsp_scan_header, scan_header, sizeof(EnumPldScanHeader));
    rsp_scan_header->bits.r = 0;
    rsp_pdu = (EnumTopoQueryRspPdu *)(rsp_buf + ENUM_PKT_HEADER_SIZE +
              ENUM_PLD_SCAN_HEADER_BASE_SIZE + calc_forward_path_size(scan_header));
    memcpy(&rsp_pdu->common, scan_pdu_com, sizeof(EnumPldScanPduCommon));
    rsp_pdu->common.bits.opcode = UB_ENUM_TOPO_QUERY_RSP;
    rsp_pdu->common.bits.status = 0;

    /* set tlv port info */
    enum_query_set_rsp_port_info(rsp_pdu, rsp_num_ports, port_idx_start, dev);

    if (slice_id == 0) {
        /* set tlv port num info */
        enum_query_set_rsp_port_num(rsp_pdu, rsp_num_ports, dev);
        /* set tlv slice info */
        total_slice = (dev->port.port_num + max_num_ports - 1) / max_num_ports;
        enum_query_set_rsp_slice_info(rsp_pdu, rsp_num_ports, total_slice);
        /* set tlv cap info */
        enum_query_set_rsp_cap_info(rsp_pdu, rsp_num_ports, dev);
    }
    /* set pdu_len */
    rsp_pdu->common.pdu_len = enum_query_get_rsp_pdu_len(rsp_num_ports, slice_id);

    memset(&cqe, 0, sizeof(cqe));
    cqe.opcode = sqe->opcode;
    cqe.task_type = PROTOCOL_ENUM;
    cqe.msn = sqe->msn;
    cqe.p_len = rsp_size;
    cqe.status = CQE_SUCCESS;
    if (fill_rq_cq(s, rsp_buf, rsp_size, &cqe) != 0) {
        qemu_log("handle_enum_query_request: fill_rq_cq failed\n");
    }
    g_free(payload);
    g_free(rsp_buf);
}
// #pragma GCC pop_options

static void (*msgq_enum_handlers[])(BusControllerState *s, HiMsgSqe *sqe,
                                    void *payload) = {
    [ENUM_CMD_TOPO_QUERY]  = handle_enum_query_request,
    [ENUM_CMD_CNA_CFG] = handle_enum_cna_config_request,
    [ENUM_CMD_CNA_QUERY]  = handle_enum_cna_query_request,
};

void handle_msg_enum(void *opaque, HiMsgSqe *sqe, void *payload)
{
    BusControllerState *s = opaque;

    if (sqe->task_type != PROTOCOL_ENUM) {
        qemu_log("invalid enum task type, please check the driver inside guestos:"
                 " task_type %u\n", sqe->task_type);
        return;
    }

    if (sqe->opcode >= ARRAY_SIZE(msgq_enum_handlers)) {
        qemu_log("invalid msg code %u, array size %lu\n",
                 sqe->opcode, ARRAY_SIZE(msgq_enum_handlers));
        return;
    }

    if (msgq_enum_handlers[sqe->opcode]) {
        msgq_enum_handlers[sqe->opcode](s, sqe, payload);
    } else {
        qemu_log("cannot process PROTOCOL_ENUM opcode: %d\n", sqe->opcode);
    }
}