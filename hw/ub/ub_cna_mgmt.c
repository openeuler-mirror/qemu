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
#include "hw/ub/ub_cna_mgmt.h"
#include "hw/ub/ub_enum.h"
#include "hw/ub/ub_config.h"
#include "qemu/log.h"
#include "trace.h"
#include "sysemu/dma.h"
#include "hw/ub/ub_cna_mgmt.h"

static void enum_set_cna_config_space(uint8_t opcode, EnumCnaCfgReq *cna_cfg_req)
{
    UbGuid *guid = &cna_cfg_req->common.guid;
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    UBDevice *dev = ub_find_device_by_guid(guid);
    uint64_t emulated_offset;
    ConfigPortBasic *port_basic = NULL;
    ConfigNetAddrInfo *net_addr_info = NULL;

    ub_device_get_str_from_guid(guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);
    if (!dev) {
        qemu_log("cannot find ub-device by guid: %s\n", guid_str);
        return;
    }

    if (opcode == UB_ENUM_CNA_MGMT_PORT) {
        uint16_t port_idx = cna_cfg_req->port_idx;
        uint64_t offset;

        if (port_idx >= dev->port.port_num) {
            qemu_log("unexpect port_idx(%u) > udev(%s) port_num(%u)\n",
                     port_idx, dev->qdev.id, dev->port.port_num);
            return;
        }

        offset = UB_PORT_SLICE_START + port_idx * UB_PORT_SZ;
        emulated_offset = ub_cfg_offset_to_emulated_offset(offset, true);
        port_basic = (ConfigPortBasic *)(dev->config + emulated_offset);
        port_basic->port_cna = cna_cfg_req->cna;
        trace_enum_set_cna_config_space_port(guid_str, port_idx, cna_cfg_req->cna);
    } else if (opcode == UB_ENUM_CNA_MGMT_DEVICE) {
        emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_NA_INFO_START, true);
        net_addr_info = (ConfigNetAddrInfo *)(dev->config + emulated_offset);
        net_addr_info->primary_cna = cna_cfg_req->cna;
        trace_enum_set_cna_config_space_device(guid_str, cna_cfg_req->cna);
    } else {
        qemu_log("not support opcode: %u\n", opcode);
    }
}


void handle_enum_cna_config_request(BusControllerState *s,
                                    HiMsgSqe *sqe, void *buf)
{
    /* req message */
    void *payload;
    size_t header_sz;
    size_t total_sz;
    EnumPktHeader *header;
    EnumPldScanHeader *scan_header;
    EnumCnaCfgReq *cna_cfg_req;
    /* rsp message */
    size_t rsp_size;
    void *rsp_buf;
    EnumPktHeader *rsp_pkt_hdr;
    EnumPldScanHeader *rsp_scan_header;
    EnumNaCfgRsp *rsp_pdu;
    size_t forward_path_size;
    HiMsgCqe cqe;
    char guid[UB_DEV_GUID_STRING_LENGTH + 1] = {0};

    assert(HI_MSG_SQE_PLD_SIZE > ENUM_PKT_HEADER_SIZE + sizeof(EnumPldScanHeader));
    scan_header = g_malloc0(sizeof(EnumPldScanHeader));
    if (dma_memory_read(&address_space_memory,
                        (unsigned long)(buf + ENUM_PKT_HEADER_SIZE),
                        scan_header, sizeof(EnumPldScanHeader), MEMTXATTRS_MEMORY)) {
        qemu_log("Failed to read sq_base_addr_gpa entry\n");
        g_free(scan_header);
        return;
    }

    total_sz = ENUM_PKT_HEADER_SIZE + calc_enum_pld_header_size(scan_header, true) + ENUM_NA_CFG_REQ_SIZE;
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
    scan_header = (EnumPldScanHeader *)((uint8_t *)payload + ENUM_PKT_HEADER_SIZE);
    header_sz = ENUM_PKT_HEADER_SIZE +
                calc_enum_pld_header_size(scan_header, true);
    if (header_sz + ENUM_NA_CFG_REQ_SIZE > total_sz) {
        qemu_log("calculate incorrect header size %lu, expect %lu\n",
                 header_sz, total_sz - ENUM_NA_CFG_REQ_SIZE);
        g_free(payload);
        return;
    }

    cna_cfg_req = (EnumCnaCfgReq *)((uint8_t *)payload + header_sz);
    if (header->ulh.cfg != UB_CLAN_LINK_CFG ||
        header->cnth.nth_nlp != NTH_NLP_WITHOUT_TPH ||
        header->upi != UB_CP_UPI ||
        cna_cfg_req->common.bits.cmd != ENUM_CMD_CNA_CFG) {
        qemu_log("invalid cna cfg reguest, please check driver inside guestos, "
                 "ulh.cfg %u cnth.nth_nlp %u upi 0x%x cmd %u\n",
                 header->ulh.cfg, header->cnth.nth_nlp, header->upi,
                 cna_cfg_req->common.bits.cmd);
        g_free(payload);
        return;
    }

    ub_device_get_str_from_guid(&cna_cfg_req->common.guid, guid, UB_DEV_GUID_STRING_LENGTH + 1);
    trace_handle_enum_cna_config_request(guid, cna_cfg_req->port_idx,
                                         cna_cfg_req->common.bits.cmd,
                                         cna_cfg_req->common.bits.opcode);

    enum_set_cna_config_space(cna_cfg_req->common.bits.opcode, cna_cfg_req);

    /* response includes forward path but not return path. */
    forward_path_size = calc_forward_path_size(scan_header);
    rsp_size = sizeof(EnumPktHeader) + sizeof(EnumPldScanHeader) +
               forward_path_size + sizeof(EnumNaCfgRsp);
    rsp_buf = g_malloc0(rsp_size);
    memset(rsp_buf, 0, rsp_size);
    rsp_pkt_hdr = (EnumPktHeader *)rsp_buf;
    memcpy(rsp_pkt_hdr, header, sizeof(EnumPktHeader));
    rsp_scan_header = (EnumPldScanHeader *)(rsp_buf + ENUM_PKT_HEADER_SIZE);
    memcpy(rsp_scan_header, scan_header, sizeof(EnumPldScanHeader));
    rsp_scan_header->bits.r = 0;
    rsp_pdu = (EnumNaCfgRsp *)(rsp_buf + ENUM_PKT_HEADER_SIZE +
              ENUM_PLD_SCAN_HEADER_BASE_SIZE + forward_path_size);
    memcpy(&rsp_pdu->common, &cna_cfg_req->common, sizeof(EnumPldScanPduCommon));
    rsp_pdu->common.bits.opcode = UB_ENUM_CNA_MGMT_RSV0;
    rsp_pdu->common.bits.status = 0;

    /* set cqe val */
    memset(&cqe, 0, sizeof(cqe));
    cqe.opcode = sqe->opcode;
    cqe.task_type = PROTOCOL_ENUM;
    cqe.msn = sqe->msn;
    cqe.p_len = rsp_size;
    cqe.status = CQE_SUCCESS;
    fill_rq_cq(s, rsp_buf, rsp_size, &cqe);
    g_free(payload);
    g_free(rsp_buf);
}

void handle_enum_cna_query_request(BusControllerState *s,
                                   HiMsgSqe *sqe, void *buf)
{
    /* req message */
    void *payload;
    size_t header_sz;
    size_t total_sz;
    EnumPktHeader *header;
    EnumPldScanHeader *scan_header;
    EnumCnaQueryReq *cna_query_req;
    /* rsp message */
    size_t rsp_size;
    void *rsp_buf;
    EnumPktHeader *rsp_pkt_hdr;
    EnumPldScanHeader *rsp_scan_header;
    EnumCnaQueryRsp *cna_req_rsp;
    HiMsgCqe cqe;
    char guid[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    UBDevice *dev;
    ConfigNetAddrInfo *net_addr_info;
    uint64_t emulated_offset;
    size_t forward_path_size;

    assert(HI_MSG_SQE_PLD_SIZE > ENUM_PKT_HEADER_SIZE + sizeof(EnumPldScanHeader));
    scan_header = g_malloc0(sizeof(EnumPldScanHeader));
    if (dma_memory_read(&address_space_memory,
                        (unsigned long)(buf + ENUM_PKT_HEADER_SIZE),
                        scan_header, sizeof(EnumPldScanHeader), MEMTXATTRS_MEMORY)) {
        qemu_log("Failed to read sq_base_addr_gpa entry\n");
        g_free(scan_header);
        return;
    }

    total_sz = ENUM_PKT_HEADER_SIZE + calc_enum_pld_header_size(scan_header, true) + ENUM_NA_QRY_REQ_SIZE;
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
    scan_header = (EnumPldScanHeader *)((uint8_t *)payload + ENUM_PKT_HEADER_SIZE);
    header_sz = ENUM_PKT_HEADER_SIZE +
                calc_enum_pld_header_size(scan_header, true);
    if (header_sz + ENUM_NA_QRY_REQ_SIZE > total_sz) {
        qemu_log("calculate incorrect header size %lu, expect %lu\n",
                 header_sz, total_sz - ENUM_NA_QRY_REQ_SIZE);
        g_free(payload);
        return;
    }

    cna_query_req = (EnumCnaQueryReq *)((uint8_t *)payload + header_sz);
    if (header->ulh.cfg != UB_CLAN_LINK_CFG ||
        header->cnth.nth_nlp != NTH_NLP_WITHOUT_TPH ||
        header->upi != UB_CP_UPI ||
        cna_query_req->common.bits.cmd != ENUM_CMD_CNA_QUERY) {
        qemu_log("invalid cna cfg reguest, please check driver inside guestos, "
                 "ulh.cfg %u cnth.nth_nlp %u upi 0x%x cmd %u\n",
                 header->ulh.cfg, header->cnth.nth_nlp, header->upi,
                 cna_query_req->common.bits.cmd);
        g_free(payload);
        return;
    }

    ub_device_get_str_from_guid(&cna_query_req->common.guid, guid, UB_DEV_GUID_STRING_LENGTH + 1);
    dev = ub_find_device_by_guid(&cna_query_req->common.guid);
    if (!dev) {
        qemu_log("failed to find dev by guid %s\n", guid);
        g_free(payload);
        return;
    }

    trace_handle_enum_cna_query_request(guid, cna_query_req->port_idx,
                                        cna_query_req->common.bits.cmd,
                                        cna_query_req->common.bits.opcode);

    forward_path_size = calc_forward_path_size(scan_header);
    rsp_size = sizeof(EnumPktHeader) + sizeof(EnumPldScanHeader) +
               forward_path_size + sizeof(EnumCnaQueryRsp);
    rsp_buf = g_malloc0(rsp_size);
    memset(rsp_buf, 0, rsp_size);
    rsp_pkt_hdr = (EnumPktHeader *)rsp_buf;
    memcpy(rsp_pkt_hdr, header, sizeof(EnumPktHeader));
    rsp_scan_header = (EnumPldScanHeader *)(rsp_buf + ENUM_PKT_HEADER_SIZE);
    memcpy(rsp_scan_header, scan_header, sizeof(EnumPldScanHeader));
    rsp_scan_header->bits.r = 0;
    cna_req_rsp = (EnumCnaQueryRsp *)(rsp_buf + ENUM_PKT_HEADER_SIZE +
              ENUM_PLD_SCAN_HEADER_BASE_SIZE + forward_path_size);
    memcpy(&cna_req_rsp->common, &cna_query_req->common, sizeof(cna_query_req->common));
    emulated_offset = ub_cfg_offset_to_emulated_offset(UB_CFG0_BASIC_NA_INFO_START, true);
    net_addr_info = (ConfigNetAddrInfo *)(dev->config + emulated_offset);
    cna_req_rsp->cna = net_addr_info->primary_cna;

    trace_handle_enum_cna_query_request_rsp(guid, cna_req_rsp->cna);

    cna_req_rsp->common.bits.opcode = UB_ENUM_CNA_MGMT_RSV0;
    cna_req_rsp->common.bits.status = 0;

    /* set cqe val */
    memset(&cqe, 0, sizeof(cqe));
    cqe.opcode = sqe->opcode;
    cqe.task_type = PROTOCOL_ENUM;
    cqe.msn = sqe->msn;
    cqe.p_len = rsp_size;
    cqe.status = CQE_SUCCESS;
    fill_rq_cq(s, rsp_buf, rsp_size, &cqe);
    g_free(payload);
    g_free(rsp_buf);
}
