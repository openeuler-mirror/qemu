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

#ifndef UB_SEC_H
#define UB_SEC_H
#include "hw/ub/hisi/ubc.h"
#include "hw/qdev-core.h"
#include "hw/ub/ub_common.h"
enum UbSecSubMsgCode {
    UB_DEV_ATTESTATION = 0,
    UB_DEV_AUTH = 1,
    UB_DEV_TOKEN_GET = 2,
    UB_DEV_TOKEN_SET = 3,
    UB_DEV_KEY_EXCHANGE = 4,
    UB_DEV_TOKEN_GET_RSP = 10,
    UB_DEV_TOKEN_SET_RSP = 11,
};

typedef struct QueryTokenMsgPldRsp {
    uint32_t token_check_support : 1;
    uint32_t encode_decode_support : 1;
    uint32_t reserved : 14;
    uint32_t token_id : 16;
    uint32_t token_value;
} QueryTokenMsgPldRsp;
#define QUERY_TOKEN_MSG_PLD_RSP_LEN sizeof(QueryTokenMsgPldRsp)

typedef struct QueryTokenMsgPld {
    union {
        /* request payload is NULL */
        struct QueryTokenMsgPldRsp rsp;
    };
} QueryTokenMsgPld;
#define QUERY_TOKEN_MSG_PLD_SIZE sizeof(QueryTokenMsgPld)

typedef struct QueryTokenMsgPkt {
    struct MsgPktHeader header;
    struct QueryTokenMsgPld pld;
} QueryTokenMsgPkt;

#define MSG_SEC_QUERY_TOKEN_MSG_PKT_SIZE (MSG_PKT_HEADER_SIZE + QUERY_TOKEN_MSG_PLD_SIZE)

void handle_msg_sec(void *opaque, HiMsgSqe *sqe, void *payload);

#endif