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
#include "hw/ub/ub_sec.h"
#include "qemu/log.h"

static void ub_sec_msg_fill_cq_rq(BusControllerState *s, HiMsgSqe *sqe, MsgPktHeader *header,
                                  QueryTokenMsgPkt *rsp_pkt)
{
    HiMsgCqe cqe;

    memset(&cqe, 0, sizeof(cqe));
    cqe.type = MSG_RSP;
    cqe.msg_code = UB_MSG_CODE_SEC;
    cqe.sub_msg_code = header->msgetah.sub_msg_code;

    rsp_pkt->header.nth.scna = header->nth.dcna;
    rsp_pkt->header.nth.dcna = header->nth.scna;
    rsp_pkt->header.deid = EID_GEN(header->seid_h, header->seid_l);
    rsp_pkt->header.seid_h = EID_HIGH(header->deid);
    rsp_pkt->header.seid_l = EID_LOW(header->deid);

    cqe.msn = sqe->msn;
    cqe.p_len = MSG_SEC_QUERY_TOKEN_MSG_PKT_SIZE;
    cqe.status = CQE_SUCCESS;
    fill_rq_cq(s, rsp_pkt, sizeof(*rsp_pkt), &cqe);
}

static void ub_sec_token_get_req(BusControllerState *s, HiMsgSqe *sqe, MsgPktHeader *header)
{
    QueryTokenMsgPkt rsp_pkt;

    memset(&rsp_pkt, 0, sizeof(rsp_pkt));
    memcpy(&rsp_pkt.header, header, sizeof(rsp_pkt.header));
    rsp_pkt.pld.rsp.token_id = 0;
    rsp_pkt.pld.rsp.token_value = 0;
    rsp_pkt.header.msgetah.rsp_status = UB_MSG_RSP_SUCCESS;
    ub_sec_msg_fill_cq_rq(s, sqe, header, &rsp_pkt);
}

static void (*msgq_sec_handlers[])(BusControllerState *s, HiMsgSqe *sqe,
                                   MsgPktHeader *header) = {
    [UB_DEV_ATTESTATION]    = NULL,
    [UB_DEV_AUTH]           = NULL,
    [UB_DEV_TOKEN_GET]      = ub_sec_token_get_req,
    [UB_DEV_TOKEN_SET]      = NULL,
    [UB_DEV_KEY_EXCHANGE]   = NULL,
    [UB_DEV_TOKEN_GET_RSP]  = NULL,
    [UB_DEV_TOKEN_SET_RSP]  = NULL,
};

void handle_msg_sec(void *opaque, HiMsgSqe *sqe, void *payload)
{
    BusControllerState *s = opaque;
    MsgPktHeader *header = (MsgPktHeader *)payload;
    MsgExtendedHeader *msgetah = &header->msgetah;

    if (msgetah->msg_code != UB_MSG_CODE_SEC ||
        msgetah->sub_msg_code >= ARRAY_SIZE(msgq_sec_handlers)) {
        qemu_log("invalid msg code %u or sub msg code %u, array size %lu\n",
                 msgetah->msg_code, msgetah->sub_msg_code, ARRAY_SIZE(msgq_sec_handlers));
        return;
    }

    if (msgq_sec_handlers[msgetah->sub_msg_code]) {
        msgq_sec_handlers[msgetah->sub_msg_code](s, sqe, header);
    } else {
        qemu_log("dont support sec sub msg code %d.\n", msgetah->sub_msg_code);
    }
}
