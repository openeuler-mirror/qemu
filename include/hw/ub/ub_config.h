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

#ifndef UB_CONFIG_H
#define UB_CONFIG_H

#include "hw/ub/hisi/ubc.h"
#include "hw/ub/ub_common.h"
#include "hw/ub/ub.h"
#include "qemu/units.h"

enum UbCfgEmulatedSlice {
    CFG0_BASIC = 0,
    /* CFG0_CAP START */
    CAP1_RSV,
    CAP2_SHP,
    CAP3_ERR_RECORD,
    CAP4_ERR_INFO,
    CAP5_EMQ,
    /* CFG0_CAP END */
    CFG1_BASIC,
    /* CFG1_CAP START */
    CAP1_DECODER,
    CAP2_JETTY,
    CAP3_INT_TYPE1,
    CAP4_INT_TYPE2,
    CAP5_RSV,
    CAP6_UB_MEM,
    /* CFG1_CAP END */
    UB_CFG_GENERAL_SLICES_NUM,

    /* dont add new here */
    CFG0_PORT_BASIC,
    CFG0_ROUTE_TABLE,
    UB_CFG_EMULATED_SLICES_NUM,
    /* dont add new here */
};

/* In UB spec, route table slice is 1GB, in virtualization,
 * route table is not used. To redece mem overhead, the route
 * table also emulated with 1k slice, so add 1 extra */
#define UB_CFG_SLICE_NUMS (UB_CFG_GENERAL_SLICES_NUM + UB_DEV_MAX_NUM_OF_PORT + 1)
#define UB_CFG_START_OFFSET_GRANU 4
#define UB_CFG_SLICE_SIZE (1 * KiB)

typedef struct UbCfgAddrMapEntry {
    uint64_t start_addr;
    uint64_t mapped_offset;
} UbCfgAddrMapEntry;

int ub_cfg_addr_map_table_init(void);

enum UbCfgSubMsgCode {
    UB_CFG0_READ = 0,
    UB_CFG0_WRITE = 1,
    UB_CFG1_READ = 2,
    UB_CFG1_WRITE = 3,
    UB_CFG_MAX_SUB_MSG_CODE,
};

typedef struct CfgMsgPldReq {
    /* DW0 */
    uint32_t rsvd0 : 4;
    uint32_t byte_enable : 4;
    uint32_t rsvd1 : 8;
    uint32_t entity_idx : 16;

    /* DW1 */
    uint32_t req_addr;

    /* DW2 */
    uint32_t rsvd2;
    /* DW3 */
    uint32_t write_data;
} CfgMsgPldReq;

typedef struct CfgMsgPldRsp {
    /* DW0 */
    uint32_t read_data;
    /* DW1 */
    uint32_t rsvd1;
    /* DW2 */
    uint32_t rsvd2;
    /* DW3 */
    uint32_t rsvd3;
} CfgMsgPldRsp;

typedef struct CfgMsgPld {
    union {
        CfgMsgPldReq req;
        CfgMsgPldRsp rsp;
    };
} CfgMsgPld;
#define CFG_MSG_PLD_SIZE 16
#define MSG_CFG_PKT_SIZE (MSG_PKT_HEADER_SIZE + CFG_MSG_PLD_SIZE) /* header 32bytes, pld 16bytes */

void handle_msg_cfg(void *opaque, HiMsgSqe *sqe, void *payload);
enum UbCfgBlockType {
    UB_CFG0_BASIC_BLOCK_TYPE  = 0,
    UB_CFG_ROUTING_BLOCK_TYPE = 1,
    UB_CFG_CAP_BLOCK_TYPE     = 2,
    UB_CFG_PORT_BLOCK_TYPE    = 3,
    UB_CFG_VD_BLOCK_TYPE      = 4,
    UB_CFG1_BASIC_BLOCK_TYPE  = 5,
    UB_CFG_BLOCK_NUMS
};

typedef struct CfgMsgPkt {
    MsgPktHeader header;
    CfgMsgPld pld;
} CfgMsgPkt;

typedef struct __attribute__ ((__packed__)) ConfigNetAddrInfo {
    uint32_t primary_cna : 24; /* 0x1A */
    uint32_t rsv : 8;
    uint32_t rsv1;     /* 0x1B */
    uint32_t rsv2;     /* 0x1C */
    uint32_t rsv3;     /* 0x1D */
    uint32_t rsv4;     /* 0x1E */
} ConfigNetAddrInfo;

typedef struct SliceHeader {
    uint32_t slice_version : 4;
    uint32_t slice_used_size : 28;
} SliceHeader;

typedef struct __attribute__ ((__packed__)) Cfg0SupportFeature {
    union {
        uint32_t rsv[4];
        struct {
            uint8_t entity_available : 1;
            uint8_t mtu_supported : 3;
            uint8_t route_table_supported : 1;
            uint8_t upi_supported : 1;
            uint8_t rsv1 : 1;
            uint8_t switch_supported : 1;
            uint8_t rsv2 : 1;
            uint8_t cc_supported : 1;
        } bits;
    };
} Cfg0SupportFeature;

typedef struct __attribute__ ((__packed__)) UbEid {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t dw2;
    uint32_t dw3;
} UbEid;

#define CAP_BITMAP_LEN 32
#define RSV_LEN 4
typedef struct __attribute__ ((__packed__)) UbCfg0Basic {
    /* dw0 */
    SliceHeader header; // RO
    /* dw1 */
    uint16_t total_num_of_port; // RO
    uint16_t total_num_of_ue; // RO
    /* dw2~dw9 */
    uint8_t cap_bitmap[CAP_BITMAP_LEN]; // RO
    /* dw10~dw14 */
    Cfg0SupportFeature support_feature; // RO
    /* dw14~dw17 */
    UbGuid guid; // RO
    /* dw18~dw21 */
    UbEid eid; // RW
    /* dw22~dw25 */
    UbEid fm_eid;
    /* dw26~dw30 */
    ConfigNetAddrInfo net_addr_info; // RW
    /* dw31~dw44 */
    uint32_t upi : 15; // RW
    uint32_t rsv1 : 17;
    uint32_t module_id : 16; // HwInit
    uint32_t vendor_id : 16;
    uint32_t dev_rst : 1; // RW
    uint32_t rsv3 : 31;
    uint32_t rsv4;
    uint32_t mtu_cfg : 3; // RW
    uint32_t rsv5 : 29;
    uint32_t cc_en : 1; // RW
    uint32_t rsv6 : 31;
    uint32_t th_en : 1; // RW
    uint32_t rsv7 : 31;
    uint32_t fm_cna : 24; // RW
    uint32_t rsv8 : 8;
    uint64_t ueid_low;  // RW
    uint64_t ueid_high; // RW
    uint32_t ucna : 24; // RW
    uint32_t rsv9 : 8;
    uint32_t rsv10;
} UbCfg0Basic;

typedef struct __attribute__ ((__packed__)) UbSlotInfo {
    /* dw2 */
    uint8_t pps : 1;
    uint8_t wlps : 1;
    uint8_t plps : 1;
    uint8_t pdss : 1;
    uint8_t pwcs : 1;
    uint32_t rsv1 : 27;
    /* dw3 */
    uint16_t start_port_idx;
    uint16_t end_port_idx;
    /* dw4~dw10 */
    uint8_t pp_ctrl : 1;
    uint32_t rsv2 : 31;
    uint8_t wl_ctrl : 2;
    uint32_t rsv3 : 30;
    uint8_t pl_ctrl : 2;
    uint32_t rsv4 : 30;
    uint8_t ms_ctrl : 1;
    uint32_t rsv5 : 31;
    uint8_t pd_ctrl : 1;
    uint32_t rsv6 : 31;
    uint8_t pds_ctrl : 1;
    uint32_t rsv7 : 31;
    uint8_t pw_ctrl : 1;
    uint32_t rsv8 : 31;
    /* dw11~dw13 */
    uint8_t pp_st : 1;
    uint32_t rsv9 : 31;
    uint8_t pd_st : 1;
    uint32_t rsv10 : 31;
    uint8_t pdsc_st : 1;
    uint32_t rsv11 : 31;
    /* dw14~dw15 */
    uint32_t rsv[2];
} UbSlotInfo;

typedef struct __attribute__ ((__packed__)) UbCfg0ShpCap {
    /* dw0 */
    SliceHeader header; // RO
    /* dw1 */
    uint16_t slot_num; // RO
    uint16_t rsv1;
    /* dw2 ~ */
    UbSlotInfo slot_info[0]; // RO
} UbCfg0ShpCap;

typedef struct __attribute__ ((__packed__)) ErrorMsgQueCtrl {
    uint64_t correctable_err_report_enable : 1;
    uint64_t uncorrectable_nonfatal_err_report_enable : 1;
    uint64_t uncorrectable_fatal_err_report_enable : 1;
    uint64_t rsv_1 : 61;
} ErrorMsgQueCtrl;

typedef struct __attribute__ ((__packed__)) UbCfg0EmqCap {
    /* dw0 */
    uint64_t segment_header;
    ErrorMsgQueCtrl error_msg_que_ctrlr;
} UbCfg0EmqCap;

typedef struct __attribute__ ((__packed__)) Cfg1SupportFeature {
    union {
        uint32_t rsv[4];
        struct {
            uint8_t rsv1 : 2;
            uint8_t mgs : 1;
            uint8_t rsv2 : 2;
            uint8_t ubbas : 1;
            uint8_t ers0s : 1;
            uint8_t ers1s : 1;
            uint8_t ers2s : 1;
            uint8_t rsv3 : 1;
            uint8_t matt_juris : 1;
        } bits;
    };
} Cfg1SupportFeature;

typedef struct __attribute__ ((__packed__)) UbCfg1DecoderCap {
    /* dw0 */
    SliceHeader header;
#define DECODER_CAP_EVENT_SIZE  5
#define DECODER_CAP_CMD_SIZE    5
#define DECODER_CAP_MMIO_SIZE   7
    /* dw1 */
    struct {
        uint16_t rsv1 : 4;
        uint16_t event_size_sup : 4;
        uint16_t rsv2 : 4;
        uint16_t cmd_size_sup : 4;
        uint16_t mmio_size_sup : 3;
        uint16_t rsv3 : 13;
    } decoder;
    /* dw2 */
    struct {
        uint32_t decoder_en : 1;
        uint32_t rsv : 31;
    } decoder_ctrl;
    /* dw3-4 */
    uint64_t dec_matt_ba;
    /* dw5-6 */
    uint64_t dec_mmio_ba;
    /* dw7 */
    uint32_t dev_usi_idx;
    /* dw 8-0xf */
#define DECODER_CAP_RESERVED1_BYTES 8
    uint32_t rsv1[DECODER_CAP_RESERVED1_BYTES];
    /* dw 0x10 */
    struct {
        uint32_t cmdq_en : 1;
        uint32_t rsv1 : 7;
        uint32_t cmdq_size_use : 4;
        uint32_t rsv2 : 20;
    } decoder_cmdq_cfg;
    /* dw 0x11 */
    struct {
        uint32_t cmdq_wr_idx : 11;
        uint32_t rsv1 : 5;
        uint32_t cmdq_err_resp : 1;
        uint32_t rsv2 : 15;
    } decoder_cmdq_prod;
    /* dw 0x12 */
    struct {
        uint32_t cmdq_rd_idx : 11;
        uint32_t rsv1 : 5;
        uint32_t cmdq_err : 1;
        uint32_t cmdq_err_res : 3;
        uint32_t rsv2 : 12;
    } decoder_cmdq_cons;
    /* dw 0x13-0x14 */
    struct {
        uint64_t rsv1 : 6;
        uint64_t cmdq_ba : 42;
        uint64_t rsv2 : 16;
    } decoder_cmdq_ba;
    /* dw 0x15-0x1f */
#define DECODER_CAP_RESERVED2_BYTES 11
    uint32_t rsv2[DECODER_CAP_RESERVED2_BYTES];
    /* dw 0x20 */
    struct {
        uint32_t evtq_en : 1;
        uint32_t rsv1 : 7;
        uint32_t evtq_size_use : 4;
        uint32_t rsv2 : 20;
    } decoder_evtq_cfg;
    /* dw 0x21 */
    struct {
        uint32_t evtq_wr_idx : 11;
        uint32_t rsv : 20;
        uint32_t evtq_ovrl_err : 1;
    } decoder_evtq_prod;
    /* dw 0x22 */
    struct {
        uint32_t evtq_rd_idx : 11;
        uint32_t rsv : 20;
        uint32_t evtq_ovrl_err_resp : 1;
    } decoder_evtq_cons;
    /* dw 0x23 */
    struct {
        uint64_t rsv1 : 6;
        uint64_t evtq_ba : 42;
        uint64_t rsv2 : 16;
    } decoder_evtq_ba;
} UbCfg1DecoderCap;

typedef struct __attribute__ ((__packed__)) UbCfg1IntType1Cap {
    /* dw0 */
    SliceHeader header;
    /* dw1 */
    uint32_t interrupt_enable : 1;
    uint32_t rsv1 : 31;
    /* dw2 */
    uint32_t support_interrupt_num : 3;
    uint32_t rsv2 : 29;
    /* dw3 */
    uint32_t interrupt_enable_num : 3;
    uint32_t rsv3 : 29;
    /* dw4 */
    uint32_t interrupt_data;
    /* dw5-dw6 */
    uint64_t interrupt_address;
    /* dw7 */
    uint32_t interrupt_id;
    /* dw8 */
    uint32_t interrupt_mask;
    /* dw9 */
    uint32_t interrupt_pending;
} UbCfg1IntType1Cap;

typedef struct __attribute__ ((__packed__)) UbCfg1IntType2Cap {
    /* dw0 */
    SliceHeader header;
    /* dw1 */
    uint16_t vec_table_num;
    uint16_t add_table_num;
    /* dw2 ~ dw8 */
    uint64_t vec_table_start_addr;
    uint64_t add_table_start_addr;
    uint64_t pend_table_start_addr;
    uint32_t interrupt_id;
    uint32_t interrupt_mask : 1;
    uint32_t rsv1 : 31;
    uint32_t interrupt_enable : 1;
    uint32_t rsv2 : 31;
} UbCfg1IntType2Cap;

typedef struct __attribute__ ((__packed__)) UbCfg1Basic {
    /* dw0 */
    SliceHeader header; // RO
    /* dw1~dw8 */
    uint8_t cap_bitmap[CAP_BITMAP_LEN]; // RO
    /* dw9~dw12 */
    Cfg1SupportFeature support_feature; // RO
    /* dw13~dw42 */
    uint32_t ers_space_size[UB_NUM_REGIONS];
    uint64_t ers_start_addr[UB_NUM_REGIONS];
    uint64_t ers_ubba[UB_NUM_REGIONS];
    uint32_t elr : 1;
    uint32_t rsv1 : 31;
    uint32_t elr_done : 1;
    uint32_t rsv2 : 31;
    uint32_t rsv3;
    uint32_t rsv4;
    uint32_t rsv5;
    uint32_t sys_pgs : 1;
    uint32_t rsv6 : 31;
    uint64_t eid_upi_tab;
    uint32_t eid_upi_ten;
    uint64_t rsv7;
    uint64_t rsv8;
    uint32_t class_code : 16;
    uint32_t rsv9 : 16;
    uint32_t rsv10;
    uint32_t rsv11;
    uint32_t rsv12;
    uint32_t dev_token_id : 20;
    uint32_t rsv13 : 12;
    uint32_t bus_access_en : 1;
    uint32_t rsv14 : 31;
    uint32_t dev_rs_access_en : 1;
    uint32_t rsv15 : 31;
} UbCfg1Basic;

typedef struct __attribute__ ((__packed__)) ConfigPortInfo {
    uint16_t port_idx : 16;
    uint8_t port_type : 1;
    uint8_t enum_boundary : 1;
    uint16_t rsv : 14;
} ConfigPortInfo;

typedef struct __attribute__ ((__packed__)) ConfigNeighborPortInfo {
    uint16_t neighbor_port_idx : 16;
    uint16_t rsv : 16;
    UbGuid neighbot_port_guid;
} ConfigNeighborPortInfo;

#define PORT_CAP_BITMAP_LEN 32
typedef struct __attribute__ ((__packed__)) ConfigPortBasic {
    SliceHeader header;
    uint8_t port_cap_bitmap[PORT_CAP_BITMAP_LEN];
    ConfigPortInfo port_info;
    ConfigNeighborPortInfo neighbor_port_info;
    uint32_t port_cna : 24;
    uint32_t rsv1 : 8;
    uint8_t port_reset : 1;
    uint32_t rsv2 : 31;
} ConfigPortBasic;

typedef struct __attribute__ ((__packed__)) UbRouteTable {
    SliceHeader header;
    uint32_t entry_num : 16;
    uint32_t ers : 1;
    uint32_t rsv1 : 15;
    uint32_t er_en : 1;
    uint32_t rsv2 : 31;
    uint32_t entry[0];
} UbRouteTable;

#define SUPPORTED       1
#define NOT_SUPPORTED   0

#define UBFM            1
#define UB_DRIVE        0

/* slice header default value, unit (4 bytes) */
#define UB_SLICE_VERSION                0x0
#define UB_CFG0_BASIC_SLICE_USED_SIZE   0x2C
#define UB_CFG1_BASIC_SLICE_USED_SIZE   0x30
#define UB_PORT_BASIC_SLICE_USED_SIZE   0x11

/* ub dev cap */
#define BITS_PER_CAP_BIT_MAP            128
#define CFG0_RSV_INDEX                  1
#define CFG0_CAP2_SHP_INDEX             2
#define CFG1_DECODER_CAP_INDEX          1
#define CFG1_JETTY_CAP_INDEX            2
#define CFG1_INT_CAP_INDEX              3

/* ub dev config space CFG0 addr offset, unit (bytes) */
#define UB_SLICE_SZ                     (0x00000100 * DWORD_SIZE)
#define UB_CFG0_BASIC_START             0x00000000
#define UB_CFG0_BASIC_CAP_BITMAP        (UB_CFG0_BASIC_START + 0x02 * DWORD_SIZE)
#define UB_CFG0_BASIC_GUID_START        (UB_CFG0_BASIC_START + 0x0E * DWORD_SIZE)
#define UB_CFG0_BASIC_NA_INFO_START     (UB_CFG0_BASIC_START + 0x1A * DWORD_SIZE)
#define UB_CFG0_DEV_UEID_OFFSET         (UB_CFG0_BASIC_START + 0x27 * DWORD_SIZE)
#define UB_CFG0_CAP1_RSV_START          (UB_CFG0_BASIC_START + UB_SLICE_SZ)
#define UB_CFG0_CAP2_SHP_START          (UB_CFG0_CAP1_RSV_START + UB_SLICE_SZ)
#define UB_CFG0_CAP3_ERR_RECORD_START   (UB_CFG0_CAP2_SHP_START + UB_SLICE_SZ)
#define UB_CFG0_CAP4_ERR_INFO_START     (UB_CFG0_CAP3_ERR_RECORD_START + UB_SLICE_SZ)
#define UB_CFG0_EMQ_CAP_START           (UB_CFG0_CAP4_ERR_INFO_START + UB_SLICE_SZ)
/* ub dev config space CFG1 addr offset, unit (bytes) */
#define UB_CFG1_BASIC_START             (0x00010000 * DWORD_SIZE)
#define UB_CFG1_CAP1_DECODER            (UB_CFG1_BASIC_START + UB_SLICE_SZ)
#define UB_CFG1_CAP2_JETTY              (UB_CFG1_CAP1_DECODER + UB_SLICE_SZ)
#define UB_CFG1_CAP3_INT_TYPE1          (UB_CFG1_CAP2_JETTY + UB_SLICE_SZ)
#define UB_CFG1_CAP4_INT_TYPE2          (UB_CFG1_CAP3_INT_TYPE1 + UB_SLICE_SZ)
#define UB_CFG1_CAP5_RSV                (UB_CFG1_CAP4_INT_TYPE2 + UB_SLICE_SZ)
#define UB_CFG1_CAP6_UB_MEM             (UB_CFG1_CAP5_RSV + UB_SLICE_SZ)
/* ub dev config space PORT addr offset, unit (bytes) */
#define UB_PORT_SLICE_START             (0x00020000 * DWORD_SIZE)
#define UB_PORT_SZ                      (0x00010000 * DWORD_SIZE)
/* ub dev config space ROUT TABLE addr offset, unit (bytes) */
#define UB_ROUTE_TABLE_START            (0xF0000000ULL * DWORD_SIZE)
#define UB_ROUTE_TABLE_SIZE             (0x10000000 * DWORD_SIZE)
/* ub dev config space CFG1 system page granule size define */
#define UB_CFG1_BASIC_SYSTEM_GRANULE_SIZE_4K   (4 * 1024)
#define UB_CFG1_BASIC_SYSTEM_GRANULE_SIZE_64K  (64 * 1024)
/* ub dev config space CFG1 dev_toke id offset 0xB4 */
#define UB_CFG1_DEV_TOKEN_ID_OFFSET     (UB_CFG1_BASIC_START + 0x2D * DWORD_SIZE)
#define UB_TOKEN_ID_MASK 0xfffff
/* ub dev config space CFG1 dev_rs_access_en offset 0xBC */
#define UB_CFG1_DEV_RS_ACCESS_EN_OFFSET (UB_CFG1_BASIC_START + 0x2F * DWORD_SIZE)
#define UB_DEV_RS_ACCESS_EN_MASK 0x1
/* ub dev config space CFG1 bus_access_en offset 0xB8 */
#define UB_CFG1_BUS_ACCESS_EN_OFFSET (UB_CFG1_BASIC_START + 0x2E * DWORD_SIZE)
#define UB_BUS_ACCESS_EN_MASK 0x1
/* ub dev config space INT TYPE2 CAP addr offset, unit (bytes) */
#define UB_CFG1_CAP4_INT_TYPE2_NUMOF_INT_VEC_OFFSET     (UB_CFG1_CAP4_INT_TYPE2 + 1 * DWORD_SIZE)
#define UB_CFG1_CAP4_INT_TYPE2_NUMOF_INT_ADDR_OFFSET    (UB_CFG1_CAP4_INT_TYPE2 + 1 * DWORD_SIZE + WORD_SIZE)
#define UB_CFG1_CAP4_INT_TYPE2_INT_VEC_TAB_OFFSET       (UB_CFG1_CAP4_INT_TYPE2 + 2 * DWORD_SIZE)
#define UB_CFG1_CAP4_INT_TYPE2_INT_ADDR_TAB_OFFSET      (UB_CFG1_CAP4_INT_TYPE2 + 4 * DWORD_SIZE)
#define UB_CFG1_CAP4_INT_TYPE2_INT_PENDING_TAB_OFFSET   (UB_CFG1_CAP4_INT_TYPE2 + 6 * DWORD_SIZE)
#define UB_CFG1_CAP4_INT_TYPE2_INT_ID_OFFSET            (UB_CFG1_CAP4_INT_TYPE2 + 8 * DWORD_SIZE)
#define UB_CFG1_CAP4_INT_TYPE2_INT_MASK_OFFSET          (UB_CFG1_CAP4_INT_TYPE2 + 9 * DWORD_SIZE)
/* ub dev usi vec&addr&pend table entrys size uint (bytes) */
#define USI_VEC_TABLE_ENTRY_SIZE          0x8
#define USI_ADDR_TABLE_ENTRY_SIZE         0x20
#define USI_PEND_TABLE_ENTRY_SIZE         0x4
#define USI_PEND_TABLE_ENTRY_BIT_NUM      32
/* ub dev usi addr table valid bit offset */
#define USI_ADDR_TABLE_VALID_BIT_OFFSET   10
#define USI_ADDR_TABLE_VALID_BIT_MASK     0x10
/* usi config space */
#define UB_CFG1_CAP4_INT_TYPE2_MASK_OFFSET       (UB_CFG1_CAP4_INT_TYPE2 + 0x24)
#define UB_CFG1_CAP4_INT_TYPE2_MASKBIT           0x1
#define UB_CFG1_CAP4_INT_TYPE2_ENABLE_OFFSET     (UB_CFG1_CAP4_INT_TYPE2 + 0x28)
#define UB_CFG1_CAP4_INT_TYPE2_ENABLEBIT         0x1
/* usi vec table source */
#define USI_VEC_TABLE_MASK_OFFSET         0x6
#define USI_VEC_TABLE_MASKBIT             0x1
#define USI_VEC_TABLE_ADDR_INDEX_OFFSET   0x4

uint32_t ub_emulated_config_size(void);
uint64_t ub_cfg_offset_to_emulated_offset(uint64_t offset, bool check_success);

#endif