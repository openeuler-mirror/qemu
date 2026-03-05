/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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

#ifndef UB_MSG_H
#define UB_MSG_H

enum ub_pool_sub_msg_code {
    UB_DEV_REG,
    UB_DEV_RLS,
    UB_BI_CREATE,
    UB_BI_DESTROY,
    UB_CFG_CPL_NOTIFY,
};

enum UbExchSubMsgCode {
    UB_OBTAIN_ENTITY_INFO = 2,
    UB_LINK_NEIGHBOR_QUERY = 7,
    UB_EXCH_MAX_SUB_MSG_CODE
};

struct UeMap {
    uint16_t start_entity_idx;
    uint16_t end_entity_idx;
};

struct EntityInfoMsgPldRsp {
    uint32_t reserved;
    uint16_t entity_nums;
    uint16_t mue_nums;
    struct UeMap map[];
};
#define ENTITY_INFO_BASE_PLD_SIZE 8

struct EntityInfoMsgPld {
    /* request payload is NULL */
    struct EntityInfoMsgPldRsp rsp;
};

typedef struct EntityInfoMsgPkt {
    struct MsgPktHeader header;
    struct EntityInfoMsgPld pld;
} EntityInfoMsgPkt;

#endif