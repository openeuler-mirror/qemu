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
#include "qemu/log.h"
#include "hw/qdev-properties.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_config.h"
#include "hw/ub/ub_pool.h"
#include "hw/ub/ub_sec.h"
#include "hw/ub/ub_enum.h"
#include "hw/ub/hisi/ubc.h"
#include "trace.h"
#include "sysemu/dma.h"

static void (*msgq_pool_handlers[])(BusControllerState *s, HiMsgSqe *sqe,
                                    MsgPktHeader *header) = {
    [UB_DEV_REG]         = NULL, /* only send from CFM */
    [UB_DEV_RLS]         = NULL, /* only send from CFM */
    [UB_BI_CREATE]       = NULL,
    [UB_BI_DESTROY]      = NULL,
    [UB_CFG_CPL_NOTIFY]  = NULL, /* only send from CFM */
};

static void handle_msg_pool(void *opaque, HiMsgSqe *sqe, void *payload)
{
    BusControllerState *s = opaque;
    MsgPktHeader *header = (MsgPktHeader *)payload;
    MsgExtendedHeader *msgetah = &header->msgetah;

    if (msgetah->msg_code != UB_MSG_CODE_POOL ||
        msgetah->sub_msg_code >= ARRAY_SIZE(msgq_pool_handlers)) {
        qemu_log("invalid msg code %u or sub msg code %u, array size %lu\n",
                 msgetah->msg_code, msgetah->sub_msg_code, ARRAY_SIZE(msgq_pool_handlers));
        return;
    }

    if (msgq_pool_handlers[msgetah->sub_msg_code]) {
        msgq_pool_handlers[msgetah->sub_msg_code](s, sqe, header);
    } else {
        qemu_log("dont support sub msg code %d.\n", msgetah->sub_msg_code);
    }
}

static void (*msgq_handlers[])(void *opaque, HiMsgSqe *sqe, void *payload) = {
    [UB_MSG_CODE_RAS]  = NULL,
    [UB_MSG_CODE_LINK] = NULL,
    [UB_MSG_CODE_CFG]  = handle_msg_cfg,
    [UB_MSG_CODE_VDM]  = NULL,
    [UB_MSG_CODE_EXCH] = NULL,
    [UB_MSG_CODE_SEC]  = handle_msg_sec,
    [UB_MSG_CODE_POOL]  = handle_msg_pool,
};

static void handle_task_type_msg(BusControllerState *s, HiMsgSqe *sqe)
{
    MsgPktHeader *payload = NULL;
    uint8_t msg_code = sqe->msg_code;
    uint32_t p_addr = sqe->p_addr;
    uint32_t plen;

    if (msg_code >= (ARRAY_SIZE(msgq_handlers))) {
        qemu_log("invalid msg code %u, array size %lu\n",
                 msg_code, ARRAY_SIZE(msgq_handlers));
        return;
    }

    if (p_addr + HI_MSG_SQE_PLD_SIZE > s->msgq.sq_sz) {
        qemu_log("invalid p_addr %u, total size %ld\n",
                 p_addr, s->msgq.sq_sz);
        return;
    }

    payload = g_malloc0(sizeof(MsgPktHeader));
    if (dma_memory_read(&address_space_memory, s->msgq.sq_base_addr_gpa + p_addr,
                        payload, sizeof(MsgPktHeader), MEMTXATTRS_UNSPECIFIED)) {
        qemu_log("Fail to read sq_base_addr_gpa entry\n");
        g_free(payload);
        return;
    }
    plen = payload->msgetah.plen;
    g_free(payload);
    payload = g_malloc0(sizeof(MsgPktHeader) + plen);
    if (dma_memory_read(&address_space_memory, s->msgq.sq_base_addr_gpa + p_addr,
                        payload, sizeof(MsgPktHeader) + plen, MEMTXATTRS_UNSPECIFIED)) {
        qemu_log("Fail to read sq_base_addr_gpa entry\n");
        g_free(payload);
        return;
    }

    if (msgq_handlers[msg_code]) {
        msgq_handlers[msg_code](s, sqe, payload);
    } else {
        qemu_log("current cannot support process msg code: %u.\n", msg_code);
    }
    g_free(payload);
}

static void handle_task_type_enum(BusControllerState *s, HiMsgSqe *sqe)
{
    EnumPktHeader *payload = NULL;
    EnumPldScanHeader *scan_header = NULL;
    uint32_t p_addr = sqe->p_addr;
    uint32_t header_size;

    if (p_addr + HI_MSG_SQE_PLD_SIZE > s->msgq.sq_sz) {
        qemu_log("invalid p_addr %u, total size %ld\n",
                 p_addr, s->msgq.sq_sz);
        return;
    }

    scan_header = g_malloc0(sizeof(EnumPldScanHeader));
    if (dma_memory_read(&address_space_memory,
                        s->msgq.sq_base_addr_gpa + p_addr + ENUM_PKT_HEADER_SIZE,
                        scan_header, sizeof(EnumPldScanHeader), MEMTXATTRS_UNSPECIFIED)) {
        qemu_log("Fail to read sq_base_addr_gpa entry\n");
        g_free(scan_header);
        return;
    }
    header_size = ENUM_PKT_HEADER_SIZE + calc_enum_pld_header_size(scan_header, true) + ENUM_TOPO_QUERY_REQ_SIZE;
    g_free(scan_header);
    payload = g_malloc0(header_size);
    if (dma_memory_read(&address_space_memory, s->msgq.sq_base_addr_gpa + p_addr,
                        payload, header_size, MEMTXATTRS_UNSPECIFIED)) {
        qemu_log("Fail to read sq_base_addr_gpa entry\n");
        g_free(payload);
        return;
    }

    handle_msg_enum(s, sqe, payload);
    g_free(payload);
}

static void handle_eu_table_cfg_cmd(BusControllerState *s, HiMsgSqe *sqe, void *payload)
{
    HiEuCfgReq *req = (HiEuCfgReq *)payload;
    HiEuCfgRsp rsp;
    HiMsgCqe cqe;

    /* qemu do nothing for hisi_private msg, just mask the msg return success */
    trace_handle_eu_table_cfg_cmd(req->eu_msg_code, req->cfg_entry_num,
                                  req->tbl_cfg_mode, req->tbl_cfg_status,
                                  req->entry_start_id, req->eid, req->upi);

    memset(&rsp, 0, sizeof(rsp));
    rsp.tbl_cfg_status = EU_CFG_SUCCESS;

    memset(&cqe, 0, sizeof(cqe));
    cqe.opcode = EU_TABLE_CFG_CMD;
    cqe.task_type = HISI_PRIVATE;
    cqe.msn = sqe->msn;
    cqe.p_len = sizeof(rsp);
    cqe.status = CQE_SUCCESS;
    cqe.rq_pi = fill_rq(s, &rsp, sizeof(rsp));
    (void)fill_cq(s, &cqe);
}

static void (*hisi_private_handlers[])(BusControllerState *s, HiMsgSqe *sqe, void *payload) = {
    [CC_CTX_CFG_CMD] = NULL,
    [QUERY_UB_MEM_ROUTE_CMD] = NULL,
    [EU_TABLE_CFG_CMD] = handle_eu_table_cfg_cmd,
    [CC_CTX_QUERY_CMD] = NULL,
};

static void handle_task_type_hisi_private(BusControllerState *s, HiMsgSqe *sqe)
{
    HiEuCfgReq *payload = NULL;
    uint8_t opcode = sqe->opcode;
    uint32_t p_addr = sqe->p_addr;

    if (opcode >= ARRAY_SIZE(hisi_private_handlers)) {
        qemu_log("invalid msg code %u, array size %lu\n",
                 opcode, ARRAY_SIZE(hisi_private_handlers));
        return;
    }

    if (p_addr + HI_MSG_SQE_PLD_SIZE > s->msgq.sq_sz) {
        qemu_log("invalid p_addr %u, total size %ld\n",
                 p_addr, s->msgq.sq_sz);
        return;
    }

    payload = g_malloc0(sizeof(HiEuCfgReq));
    if (dma_memory_read(&address_space_memory, s->msgq.sq_base_addr_gpa + p_addr,
                        payload, sizeof(HiEuCfgReq), MEMTXATTRS_UNSPECIFIED)) {
        qemu_log("Fail to read sq_base_addr_gpa entry\n");
        g_free(payload);
        return;
    }

    if (hisi_private_handlers[opcode]) {
        hisi_private_handlers[opcode](s, sqe, payload);
    } else {
        qemu_log("current cannot support process hisi private opcode: %u.\n", opcode);
    }
    g_free(payload);
}

void msgq_process_task(void *opaque, uint64_t val)
{
    BusControllerState *s = opaque;
    HiMsgSqe *sqe = NULL;
    uint16_t i;
    uint16_t cnt;
    uint32_t ci = ub_get_long(s->msgq_reg + SQ_CI);
    uint32_t pi = ub_get_long(s->msgq_reg + SQ_PI);
    uint32_t depth = ub_get_long(s->msgq_reg + SQ_DEPTH);

    if (!s->msgq.sq_base_addr_gpa) {
        /* not ready */
        return;
    }

    if (depth > HI_MSGQ_MAX_DEPTH || depth < HI_MSGQ_MIN_DEPTH || ci >= depth || pi >= depth) {
        qemu_log("Invalid arguments: ci=%u pi=%u depth=%u\n", ci, pi, depth);
        return;
    }

    sqe = g_malloc0(sizeof(HiMsgSqe));
    cnt = (pi + depth - ci) % depth;
    for (i = 0; i < cnt; i++) {
        if (dma_memory_read(&address_space_memory, s->msgq.sq_base_addr_gpa + ci,
                            sqe, sizeof(HiMsgSqe), MEMTXATTRS_UNSPECIFIED)) {
            qemu_log("Fail to read sq_base_addr_gpa entry\n");
            g_free(sqe);
            return;
        }
        if (sqe->msg_code >= (ARRAY_SIZE(msgq_handlers))) {
            qemu_log("invalid msg code %u, array size %lu\n",
                     sqe->msg_code, ARRAY_SIZE(msgq_handlers));
            g_free(sqe);
            return;
        }

        switch (sqe->task_type) {
            case PROTOCOL_MSG:
                handle_task_type_msg(s, sqe);
                break;
            case PROTOCOL_ENUM:
                handle_task_type_enum(s, sqe);
                break;
            case HISI_PRIVATE:
                handle_task_type_hisi_private(s, sqe);
                break;
            default:
                qemu_log("current can not process task type: %u\n", sqe->task_type);
                break;
        }
        ci = (ci + 1) % depth;
    }
    ub_set_long(s->msgq_reg + SQ_CI, ci);
    g_free(sqe);
}

void msgq_sq_init(void *opaque)
{
    BusControllerState *s = opaque;
    uint32_t addr_l = ub_get_long(s->msgq_reg + SQ_ADDR_L);
    uint32_t addr_h = ub_get_long(s->msgq_reg + SQ_ADDR_H);
    uint32_t depth = ub_get_long(s->msgq_reg + SQ_DEPTH);
    uint64_t size = (uint64_t)depth * (HI_MSG_SQE_SIZE + HI_MSG_SQE_PLD_SIZE);

    s->msgq.sq_base_addr_gpa = addr_l | ((uint64_t)addr_h << 32);
    s->msgq.sq_base_addr_hva = (uint64_t)cpu_physical_memory_map(s->msgq.sq_base_addr_gpa, &size, true);
    if (size != depth * (HI_MSG_SQE_SIZE + HI_MSG_SQE_PLD_SIZE)) {
        qemu_log("sq size %lu != %lu, depth=%u\n", size,
                 depth * (HI_MSG_SQE_SIZE + HI_MSG_SQE_PLD_SIZE), depth);
        return;
    }
    s->msgq.sq_sz = size;
    trace_msgq_sq_init(s->msgq.sq_base_addr_gpa, s->msgq.sq_base_addr_hva, depth);
}

void msgq_cq_init(void *opaque)
{
    BusControllerState *s = opaque;
    uint32_t addr_l = ub_get_long(s->msgq_reg + CQ_ADDR_L);
    uint32_t addr_h = ub_get_long(s->msgq_reg + CQ_ADDR_H);
    uint32_t depth = ub_get_long(s->msgq_reg + CQ_DEPTH);
    uint64_t size = (uint64_t)depth * HI_MSG_CQE_SIZE;

    s->msgq.cq_base_addr_gpa = addr_l | ((uint64_t)addr_h << 32);
    s->msgq.cq_base_addr_hva = (uint64_t)cpu_physical_memory_map(s->msgq.cq_base_addr_gpa, &size, true);
    if (size != depth * HI_MSG_CQE_SIZE) {
        qemu_log("cq size %lu != %lu, depth=%u\n", size,
                 depth * HI_MSG_CQE_SIZE, depth);
        return;
    }
    s->msgq.cq_sz = size;
    trace_msgq_cq_init(s->msgq.cq_base_addr_gpa, s->msgq.cq_base_addr_hva, depth);
}

void msgq_rq_init(void *opaque)
{
    BusControllerState *s = opaque;
    uint32_t addr_l = ub_get_long(s->msgq_reg + RQ_ADDR_L);
    uint32_t addr_h = ub_get_long(s->msgq_reg + RQ_ADDR_H);
    uint32_t depth = ub_get_long(s->msgq_reg + RQ_DEPTH);
    uint64_t size = (uint64_t)depth * HI_MSG_RQE_SIZE;

    s->msgq.rq_base_addr_gpa = addr_l | ((uint64_t)addr_h << 32);
    s->msgq.rq_base_addr_hva = (uint64_t)cpu_physical_memory_map(s->msgq.rq_base_addr_gpa, &size, true);
    if (size != depth * HI_MSG_RQE_SIZE) {
        qemu_log("rq size %lu != %u, depth=%u\n", size,
                 depth * HI_MSG_RQE_SIZE, depth);
        return;
    }
    s->msgq.rq_sz = size;
    trace_msgq_rq_init(s->msgq.rq_base_addr_gpa, s->msgq.rq_base_addr_hva, depth);
}

void msgq_handle_rst(void *opaque)
{
    BusControllerState *s = opaque;
    uint32_t old = ub_get_long(s->msgq_reg + SQ_CI);

    qemu_log("BusControllerState receive reset event, "
             "clear SQ_CI(%u -> 0).\n", old);
    ub_set_long(s->msgq_reg + SQ_CI, 0);
    ub_set_long(s->msgq_reg + SQ_ADDR_L, 0);
    ub_set_long(s->msgq_reg + SQ_ADDR_H, 0);
    ub_set_long(s->msgq_reg + SQ_DEPTH, 0);
    ub_set_long(s->msgq_reg + CQ_PI, 0);
    ub_set_long(s->msgq_reg + CQ_ADDR_L, 0);
    ub_set_long(s->msgq_reg + CQ_ADDR_H, 0);
    ub_set_long(s->msgq_reg + CQ_DEPTH, 0);
    ub_set_long(s->msgq_reg + RQ_PI, 0);
    ub_set_long(s->msgq_reg + RQ_ADDR_L, 0);
    ub_set_long(s->msgq_reg + RQ_ADDR_H, 0);
    ub_set_long(s->msgq_reg + RQ_DEPTH, 0);

    if (s->msgq.rq_sz && s->msgq.rq_base_addr_hva) {
        cpu_physical_memory_unmap((void *)s->msgq.rq_base_addr_hva,
                                  s->msgq.rq_sz, true, s->msgq.rq_sz);
    }
    if (s->msgq.sq_sz && s->msgq.sq_base_addr_hva) {
        cpu_physical_memory_unmap((void *)s->msgq.sq_base_addr_hva,
                                  s->msgq.sq_sz, true, s->msgq.sq_sz);
    }
    if (s->msgq.cq_sz && s->msgq.cq_base_addr_hva) {
        cpu_physical_memory_unmap((void *)s->msgq.cq_base_addr_hva,
                                  s->msgq.cq_sz, true, s->msgq.cq_sz);
    }
    memset(&s->msgq, 0, sizeof(s->msgq));
}