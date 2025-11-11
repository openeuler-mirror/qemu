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

#ifndef UB_ENUM_H
#define UB_ENUM_H

#include "hw/ub/hisi/ubc.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_common.h"
#include "qemu/log.h"

enum UbEnumSubMsgCode {
    UB_ENUM_QUERY_REQ       = 0x0,
    UB_ENUM_QUERY_RSP       = 0x1,
    UB_ENUM_CNA_CONFIG_REQ  = 0x2,
    UB_ENUM_CNA_CONFIG_RSP  = 0x3
};

enum EnumCMD {
    ENUM_CMD_TOPO_QUERY = 0,
    ENUM_CMD_CNA_CFG,
    ENUM_CMD_CNA_QUERY
};

enum UbEnumTopoQueryOpcode {
    UB_ENUM_TOPO_QUERY_RSP       = 0x0,
    UB_ENUM_TOPO_QUERY_REQ       = 0x1
};


/*
 * enum pkt : EnumPktHeader + EnumPldScanHeader + reqX
 * reqX : EnumTopoQueryReq or EnumCnaCfgReq or EnumNaQueryReq
 */
typedef struct EnumPktHeader {
    /* DW0 */
    struct UbLinkHeader ulh;
    /* DW1-DW2 */
    struct ClanNetworkHeader cnth;
    /* DW3 */
    uint16_t rsv;
#define UB_CP_UPI 0x7FFF /* id = ~0, permission = 0 */
    uint16_t upi;

    /* DW4~ */
    char payload[0];
} EnumPktHeader;
#define ENUM_PKT_HEADER_SIZE 16

typedef struct EnumPldScanHeader {
    /* DW0 */
    union {
        struct {
            uint32_t step : 8;
            uint32_t hops : 8;
            uint32_t hop_type : 4;
            uint32_t r : 1;
            uint32_t rsv : 11;
        } bits;
        uint32_t dw0;
    };
    /* DW1~ */
    uint8_t path[]; /* include forward & return, 4byte align */
} EnumPldScanHeader;
#define ENUM_PLD_SCAN_HEADER_BASE_SIZE 4 /* exclusive path */

typedef struct EnumPldScanPduCommon {
    /* DW0 */
    union {
        struct {
            union {
                uint8_t status;
                uint8_t slice_id;
            };
            uint8_t opcode;
            uint8_t cmd;
#define UB_ENUM_MNG_VERSION 0x1
            uint8_t version;
        } bits;
        uint32_t dw0;
    };
    /* DW1 */
    uint32_t msn : 16;
    uint32_t pdu_len : 8;
    uint32_t msgq_id : 8;
    /* DW2~DW5 */
    UbGuid guid;
} EnumPldScanPduCommon;
#define ENUM_PLD_SCAN_PDU_COMMON_SIZE 24

typedef struct EnumTopoQueryReq {
    /* DW0~DW5 */
    struct EnumPldScanPduCommon common;
} EnumTopoQueryReq;
#define ENUM_TOPO_QUERY_REQ_SIZE 24

/* enum query respons message */
typedef struct EnumTlvPortInfo {
    /* DW0 */
    union {
        struct {
            uint32_t rsvd : 8;
            uint32_t s : 1;
            uint32_t b : 1;
            uint32_t w : 1;
            uint32_t t : 1;
            uint32_t rsvd1 : 4;
            uint32_t len : 8;
            uint32_t type : 8;
        } bits0;
        uint32_t dw0;
    };
    /* DW1 */
    uint16_t remote_port_idx;
    uint16_t local_port_idx;

    /* DW2 */
    uint16_t cur_rate;
    uint16_t max_rate;

    /* DW3~DW6 */
    UbGuid remote_guid;
} EnumTlvPortInfo;
#define ENUM_TOPO_QUERY_RSP_PORT_SIZE 28
#define UB_PORT_STATUS_UP             1
#define UB_PORT_STATUS_DOWN           0

typedef struct EnumTlvPortNum {
    uint32_t total_num_ports : 16;
    uint32_t len : 8;
    uint32_t type : 8;
    uint32_t rsvd;
} EnumTlvPortNum;
#define ENUM_TLV_PORT_NUM_SZ 8

typedef struct EnumTlvSliceInfo {
    uint32_t rsvd : 8;
    uint32_t total_slice : 8;
    uint32_t len : 8;
    uint32_t type : 8;
} EnumTlvSliceInfo;
#define ENUM_TLV_SLICE_INFO_SZ 4

typedef struct EnumTlvCapInfo {
    uint32_t da : 1;
    uint32_t rsvd0 : 7;
    uint32_t mtu : 3;
    uint32_t rsvd1 : 1;
    uint32_t sup_mtu : 3;
    uint32_t rsvd2 : 1;
    uint32_t len : 8;
    uint32_t type : 8;
    uint32_t class_code : 16;
    uint32_t rsvd3 : 16;
} EnumTlvCapInfo;
#define ENUM_TLV_CAP_INFO_SZ 8

typedef struct EnumTopoQueryRspPdu {
    /* DW0~DW5 */
    struct EnumPldScanPduCommon common;
    struct EnumTlvPortInfo port_info[0]; // TODO: size depends on num of port
} EnumTopoQueryRspPdu;
#define ENUM_TOPO_QUERY_RSP_BASE_SIZE 40 /* exclusive port_info */
#define ENUM_TOPO_QUERY_RSP_SIZE sizeof(EnumTopoQueryRsp)
#define ENUM_TOPO_QUERY_MAX_RSP_SIZE HI_MSG_RQE_SIZE
#define ENUM_TOPO_QUERY_RSP_PDU_MAX_LEN 256

typedef struct EnumTopoQueryRsp {
    EnumPktHeader pkt_hdr;
    EnumPldScanHeader scan_hdr;
    EnumTopoQueryRspPdu scan_pdu;
} EnumTopoQueryRsp;

void handle_msg_enum(void *opaque, HiMsgSqe *sqe, void *payload);
static inline size_t calc_forward_path_size(struct EnumPldScanHeader *header)
{
#define FOUR_BITS_PER_DWORD 8
#define ALIGN(a, b) (((a) + ((b) - 1)) & ~((b) - 1))
    uint8_t hop_bits[] = { 0x00000004, 0x00000008, 0x00000010 };

    if (header->bits.hop_type >= ARRAY_SIZE(hop_bits)) {
        qemu_log("enum hop_type error, hop_type is %u, hop_bits size is %zu\n",
                 header->bits.hop_type, ARRAY_SIZE(hop_bits));
        return 0;
    }

    /* Path size is hops * hop_bits[], then align it to 4byte */
    return ALIGN(hop_bits[header->bits.hop_type] * header->bits.hops /
             hop_bits[0], FOUR_BITS_PER_DWORD) / 0x00000002;
}

static inline size_t calc_enum_pld_header_size(EnumPldScanHeader *header, bool req)
{
    size_t bytes = calc_forward_path_size(header);

    if (req && header->bits.r)
        bytes <<= 1;

    bytes += ENUM_PLD_SCAN_HEADER_BASE_SIZE;

    return bytes;
}
#endif