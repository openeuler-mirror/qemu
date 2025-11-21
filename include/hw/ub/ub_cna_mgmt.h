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

#ifndef UB_CNA_MGMT_H
#define UB_CNA_MGMT_H
#include "hw/ub/hisi/ubc.h"
#include "hw/qdev-core.h"
#include "hw/ub/ub_common.h"
#include "hw/ub/ub_enum.h"

typedef struct EnumCnaQueryReq {
    /* DW0~DW5 */
    struct EnumPldScanPduCommon common;
    /* DW6 */
    uint32_t port_idx : 16;
    uint32_t rsv : 16;
} EnumCnaQueryReq;
#define ENUM_NA_QRY_REQ_SIZE 28

typedef struct EnumCnaQueryRsp {
    /* DW0~DW5 */
    struct EnumPldScanPduCommon common;
    /* DW6 */
    uint32_t cna : 24;
    uint32_t rsvd : 8;
} EnumCnaQueryRsp;

/* opcode for CNA config and query operation */
enum UbEnumCnaMgmtOpcode {
    UB_ENUM_CNA_MGMT_RSV0 = 0,
    UB_ENUM_CNA_MGMT_RSV1,
    UB_ENUM_CNA_MGMT_DEVICE,
    UB_ENUM_CNA_MGMT_PORT
};

typedef struct EnumCnaCfgReq {
    /* DW0~DW5 */
    struct EnumPldScanPduCommon common;
    /* DW6 */
    uint16_t rsvd;
    uint16_t port_idx;
    /* DW7 */
    uint32_t cna : 24;
    uint8_t rsvd1;
} EnumCnaCfgReq;
#define ENUM_NA_CFG_REQ_SIZE 32

typedef struct EnumNaCfgRsp {
    /* DW0~DW5 */
    struct EnumPldScanPduCommon common;
} EnumNaCfgRsp;


void handle_enum_cna_config_request(BusControllerState *s,
                                    HiMsgSqe *sqe, void *buf);
void handle_enum_cna_query_request(BusControllerState *s,
                                   HiMsgSqe *sqe, void *buf);
#endif