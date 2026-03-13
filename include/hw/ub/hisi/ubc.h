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

#ifndef HISI_UBC_H
#define HISI_UBC_H

/*
 * Address space layout of the UB controller
 * References: LinQuickCV100 Programming User Guide
 *
 * +----------------------------+ BA_ADDR+0xFFFF_FFFF
 * |   UMMU REG (3G)            |
 * +----------------------------+ BA_ADDR+0x4000_0000
 * |    ...                     |
 * +----------------------------+
 * | UB MSGQ(32M) only 2MB used |
 * +----------------------------+ BA_ADDR+0x1000_0000
 * | Local Register (256M)      |
 * +----------------------------+ BA_ADDR+0x0000_0000
*/
#define BASE_REG_SIZE        0x100000000 /* 4GiB */
#define LOCAL_REG_SIZE       0x10000000 /* 256MiB */
#define UBC_MSGQ_REG_SIZE    0x100000   /* 1MiB */
#define UMMU_REG_SIZE        0xC0000000 /* 3GiB */
#define UMMU_REG_OFFSET      0x40000000
#define UBC_MSGQ_REG_OFFSET  LOCAL_REG_SIZE
#define LOCAL_REG_OFFSET 0
#define SINGLE_UMMU_REG_SIZE     0x5000 /* 20KiB */
#define SINGLE_UMMU_PMU_REG_SIZE 0x1000 /* 4KiB */
#define UBC_INTERRUPT_ID_START      0x1FFF
#define UBC_INTERRUPT_ID_CNT        0x1000
#define VENDER_ID_HUAWEI            0xCC08

/*
 *   Local Register layout
 *
 *   +-----------------------------+
 *   |        rsv  15M             |
 *   +-----------------------------+
 *   |        16th 1M CCUM         |
 *   +-----------------------------+ 0xf00_0000
 *   |        15th 16M UMMU        |
 *   +-----------------------------+ 0xe00_0000
 *   |        14th 16M NL4         |
 *   +-----------------------------+ 0xd00_0000
 *   |        13th 16M BA4         |
 *   +-----------------------------+ 0xc00_0000
 *   |        12th 16M NL3         |
 *   +-----------------------------+ 0xb00_0000
 *   |        11th 16M BA3         |
 *   +-----------------------------+ 0xa00_0000
 *   |        10th 16M NL2         |
 *   +-----------------------------+ 0x900_0000
 *   |         9th 16M BA2         |
 *   +-----------------------------+ 0x800_000
 *   |         8th 16M NL1         |
 *   +-----------------------------+ 0x700_0000
 *   |         7th 16M BA1         |
 *   +-----------------------------+ 0x600_0000
 *   |         6th 16M NL0         |
 *   +-----------------------------+ 0x500_0000
 *   |         5th 16M TA          |
 *   +-----------------------------+ 0x400_0000
 *   |         4th 16M BA0         |
 *   +-----------------------------+ 0x300_0000
 *   |         3th 16M TP          |
 *   +-----------------------------+ 0x200_0000
 *   |         2rd 16M MISC        |
 *   +-----------------------------+ 0x100_0000
 *   |         1st 16M             |
 *   +-----------------------------+ 0x000_0000
 */
#define FST_OFFSET  0x0000000
#define MISC_OFFSET 0x1000000
#define TP_OFFSET   0x2000000
#define BA0_OFFSET  0x3000000
#define TA_OFFSET   0x4000000
#define NL0_OFFSET  0x5000000
#define BA1_OFFSET  0x6000000
#define NL1_OFFSET  0x7000000
#define BA2_OFFSET  0x8000000
#define NL2_OFFSET  0x9000000
#define BA3_OFFSET  0xa000000
#define NL3_OFFSET  0xb000000
#define BA4_OFFSET  0xc000000
#define NL4_OFFSET  0xd000000
#define UMMU_OFFSET 0xe000000
#define CCUM_OFFSET 0xf000000
#define LOCAL_REG_TYPE_SHIFT 24
#define LOCAL_REG_TYPE_MASK GENMASK_ULL(27, 24)
#define LOCAL_REG_MEMBER_MASK GENMASK_ULL(23, 0)
#define LOCAL_REG_ADDR_2_TYPE(addr)  (((addr) & LOCAL_REG_TYPE_MASK) >> LOCAL_REG_TYPE_SHIFT)
#define BA_REG_MEMBER_MASK GENMASK_ULL(19, 16)
#define BA_REG_DATA_MASK GENMASK_ULL(15, 0)
#define TOP_REG_OFFSET    (0x00 * 64 * KiB)
#define RXDMA_OFFSET      (0x01 * 64 * KiB)
#define TXDMA_OFFSET      (0x02 * 64 * KiB)
#define MASTER_OFFSET     (0x03 * 64 * KiB)
#define LSAD_OFFSET       (0x04 * 64 * KiB)
#define SMAP_OFFSET       (0x0a * 64 * KiB)
#define P2P_OFFSET        (0x0c * 64 * KiB)
#define MAR_OFFSET        (0x0d * 64 * KiB)
#define MAR_DECODE_OFFSET (0x0e * 64 * KiB) /* mem decoder table */
#define UB_RAS_OFFSET     (0x12 * 64 * KiB)
#define CCUA_OFFSET       (0x14 * 64 * KiB)

/* references LinQuickCV100_UBOMMU_nManager */
#define TP_UBOMMU0_OFFSET 0x180000
#define TP_UBOMMU1_OFFSET 0x190000
#define TP_UBOMMU2_OFFSET 0x1c0000
#define TP_UBOMMU3_OFFSET 0x1d0000
#define TP_UBOMMU4_OFFSET 0x1e0000
#define TP_UBOMMU5_OFFSET 0x1f0000
#define TP_UBOMMU0_SELF_REG_OFFSET 0
#define TP_UBOMMU0_PMCG_OFFSET     0x3000
#define TP_UBOMMU0_PROTOCOL_OFFSET 0x4000
#define TP_UBOMMU0_CMDQ_OFFSET     0xc000
#define TP_UBOMMU0_EVENTQ_OFFSET   0xe000
#define SELF_ICG_CFG_OFFSET     0x0
#define UBOMMU_MEM_INIT_OFFSET  0x1804
#define DECODER_REG_BASE (TP_OFFSET + TP_UBOMMU0_OFFSET + \
                          TP_UBOMMU0_SELF_REG_OFFSET)         /* 0x2180000 */
#define CMDQ_BASE_ADDR (TP_OFFSET + TP_UBOMMU0_OFFSET + \
                        TP_UBOMMU0_CMDQ_OFFSET)               /* 0x218c000 */
#define EVTQ_BASE_ADDR (TP_OFFSET + TP_UBOMMU0_OFFSET + \
                        TP_UBOMMU0_EVENTQ_OFFSET)             /* 0x218e000 */

#define DECODER_SELF_ICG_CFG_OFFSET (DECODER_REG_BASE + \
                                      SELF_ICG_CFG_OFFSET) /* 0x2180000 */
#define DECODER_SELF_MEM_INIT_OFFSET (DECODER_REG_BASE + \
                                      UBOMMU_MEM_INIT_OFFSET) /* 0x2181804 */
#define DECODER_MEM_INIT_DONE_VAL 0x3F
#define DECODER_MEM_INIT_DONE_SHIFT 16

#define SQ_ADDR_L 0x0
#define SQ_ADDR_H 0x4
#define SQ_PI 0x8
#define SQ_CI 0xc
#define SQ_DEPTH 0x10
#define SQ_STATUS 0x14
#define RQ_ADDR_L 0x40
#define RQ_ADDR_H 0x44
#define RQ_PI 0x48
#define RQ_CI 0x4c
#define RQ_DEPTH 0x50
#define RQ_ENTRY_SIZE 0x54
#define RQ_STATUS 0x58
#define CQ_ADDR_L 0x70
#define CQ_ADDR_H 0x74
#define CQ_PI 0x78
#define CQ_CI 0x7c
#define CQ_DEPTH 0x80
#define CQ_STATUS 0x84
#define CQ_INT_MASK 0x88
#define CQ_INT_STATUS 0x8c
#define CQ_INT_RO 0x90
#define CQ_INT_SET 0x94
#define MSGQ_RST 0xB0

#define HI_MSG_SQE_PLD_SIZE 0x800 /* 2K */
#define HI_MSG_RQE_SIZE 0x800     /* 2K */
#define HI_MSGQ_DEPTH 16
#define HI_SQ_CFG_DEPTH HI_MSGQ_DEPTH
#define HI_RQ_CFG_DEPTH HI_MSGQ_DEPTH
#define HI_CQ_CFG_DEPTH HI_MSGQ_DEPTH
#define HI_FM_MSG_ID_MIN 128
#define HI_FM_MSG_ID_MAX 191
#define MAP_COMMAND_MASK 0xff
#define HI_FM_MSG_MAX (HI_SQ_CFG_DEPTH - 1)
#define HI_MSGQ_MAX_DEPTH 1024
#define HI_MSGQ_MIN_DEPTH 4

/*
 *  msgq sq memory layout
 *            +----------------------------+
 *            | sqe 1                      |
 *            |----------------------------| 12Byte
 *       +----|    payload addr(offset)    |
 *       |    +----------------------------+
 *       |    | sqe 2                      |
 *       |    |----------------------------| 12Byte
 *    +-------|    payload addr(offset)    |
 *    |  |    +----------------------------+
 *    |  |    |       .....                |
 *    |  |    |                            |
 *    |  |    +----------------------------+
 *    |  |    | sqe (depth)                |
 *    |  |    |----------------------------| 12Byte
 *  +---------|    payload addr(offset)    |
 *  | |  |    +----------------------------+
 *  | |  +--> |       payload 1            |  1K
 *  | |       +----------------------------+
 *  | +-----> |       payload 2            |  1K
 *  |         +----------------------------+
 *  |         |       .......              |  1K
 *  |         +----------------------------+
 *  +-------> |   payload (depth)          |  1K
 *            +----------------------------+
 *
 *                      SQE layout
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
 * +-----------------------------------------------+-----------------------+-----------+-----------+
 * |                  payload length               |       msg id          |submsg code| msg code  |
 * +-----------------------------------------------+-----+--+--+-----------+-----------+-----------+
 * |                        rsvd                         |e1|e2|    vl     |        rsvd           |  e1:icrc  e2:local
 * +-----------------------------------------------------+--+--+-----------+-----------------------+
 * |                                        payload addr                                           |
 * +-----------------------------------------------------------------------------------------------+
 */
typedef struct HiMsgSqe {
    /* DW0 */
    uint32_t task_type : 2;
    uint32_t rsvd0 : 2;
    uint32_t local : 1;
    uint32_t dev_type : 2;
    uint32_t icrc : 1;
    union {
        struct {
            uint8_t type : 1;
            uint8_t msg_code : 3;
            uint8_t sub_msg_code : 4;
        };
        uint8_t opcode;
    };
    uint32_t p_len : 12;
    uint32_t rsvd1 : 4;

    /* DW1 */
    uint32_t msn : 16;
    uint32_t rsvd3 : 16;

    /* DW2 */
    uint32_t p_addr;

    /* DW3 */
    uint32_t rsvd2;
} HiMsgSqe;
#define HI_MSG_SQE_SIZE sizeof(HiMsgSqe)

typedef struct HiMsgCqe {
    /* DW0 */
    uint32_t task_type : 2;
    uint32_t rsvd0 : 6;
    union {
        struct {
            uint8_t type : 1;
            uint8_t msg_code : 3;
            uint8_t sub_msg_code : 4;
        };
        uint8_t opcode;
    };
    uint32_t p_len : 12;
    uint32_t rsvd1 : 4;

    /* DW1 */
    uint32_t msn : 16;
    uint32_t rsvd5 : 16;

    /* DW2 */
    uint32_t rq_pi : 10;
    uint32_t rsvd2 : 6;
    uint32_t status : 8;
    uint32_t rsvd3 : 8;

    /* DW3 */
    uint32_t rsvd4;
} HiMsgCqe;
#define HI_MSG_CQE_SIZE sizeof(HiMsgCqe)

typedef struct HiMsgSqePld {
    char packet[HI_MSG_SQE_PLD_SIZE];
} HiMsgSqePld;

typedef struct HiMsgqInfo {
    uint64_t sq_base_addr_gpa;
    uint64_t sq_sz;
    uint32_t sq_depth;
    bool sq_inited;
    uint64_t cq_base_addr_gpa;
    uint64_t cq_sz;
    uint32_t cq_depth;
    bool cq_inited;
    uint64_t rq_base_addr_gpa;
    uint64_t rq_sz;
    uint32_t rq_depth;
    bool rq_inited;
} HiMsgqInfo;

typedef enum HiMsgqIdx {
    MSG_SQ = 0,
    MSG_RQ = 1,
    MSG_CQ = 2,
    MSGQ_NUM
} HiMsgqIdx_t;

enum HiCqeStatus {
    CQE_SUCCESS,
    CQE_FAIL
};

enum HiCqSwState {
    CQ_SW_INIT,
    CQ_SW_HANDLED
};

struct HiMsgQueue {
    HiMsgqIdx_t idx;

    union {
        struct HiMsgSqe *sqe;
        void *rqe;
        struct HiMsgCqe *cqe;
        void *entry;
    };

    uint16_t entry_size;
    uint8_t depth;
    uint8_t ci;
    uint8_t pi;

    pthread_spinlock_t lock;
};

#define UB_MSG_CODE_ENUM 0x8 /* hisi private */
enum HiEnumSubMsgCode {
    ENUM_QUERY_REQ = 0,
    ENUM_QUERY_RSP,
    CNA_CFG_REQ,
    CNA_CFG_RSP
};

enum UB_MSG_RSP_STATUS_CODE {
    UB_MSG_RSP_SUCCESS =        0x00,
    UB_MSG_RSP_CMD_EACCESS =    0x01,
    UB_MSG_RSP_CMD_ENODEV =     0x02,
    UB_MSG_RSP_CMD_CODE_ERR =   0x03,
    UB_MSG_RSP_CMD_LEN_ERR =    0x04,
    /* 0x05~0x1f reserved */
    UB_MSG_RSP_EXEC_ENOMEM =    0x20,
    UB_MSG_RSP_EXEC_EACCES =    0x21,
    UB_MSG_RSP_EXEC_EFAULT =    0x22,
    UB_MSG_RSP_EXEC_EBUSY =     0x23,
    UB_MSG_RSP_EXEC_EEXIST =    0x24,
    UB_MSG_RSP_EXEC_ENODEV =    0x25,
    UB_MSG_RSP_EXEC_EINVAL =    0x26,
    UB_MSG_RSP_EXEC_ENOEXEC =   0x27,
    /* 0x28~0xfe reserved */
    UB_MSG_RSP_UNKNOWN =        0xff,
};

enum HiTaskType {
    PROTOCOL_MSG = 0,
    PROTOCOL_ENUM = 1,
    HISI_PRIVATE = 2
};

typedef enum HiMsgqPrivateOpcode {
    CC_CTX_CFG_CMD = 0,
    QUERY_UB_MEM_ROUTE_CMD = 1,
    EU_TABLE_CFG_CMD = 2,
    CC_CTX_QUERY_CMD = 3
} HiMsgqPrivateOpcode;

typedef enum HiEuCfgStatus {
    EU_CFG_FAIL,
    EU_CFG_SUCCESS
} HiEuCfgStatus;

typedef struct HiEuCfgReq {
    uint32_t eu_msg_code : 4;
    uint32_t cfg_entry_num : 10;
    uint32_t tbl_cfg_mode : 1;
    uint32_t tbl_cfg_status : 1;
    uint32_t entry_start_id : 16;
    uint32_t eid : 20;
    uint32_t rsv0 : 12;
    uint32_t upi : 16;
    uint32_t rsv1 : 16;
} HiEuCfgReq;
#define HI_EU_CFG_REQ_SIZE 12

typedef struct HiEuCfgRsp {
    uint32_t eu_msg_code : 4;
    uint32_t cfg_entry_num : 10;
    uint32_t tbl_cfg_mode : 1;
    uint32_t tbl_cfg_status : 1;
    uint32_t entry_start_id : 16;
} HiEuCfgRsp;
#define HI_EU_CFG_RSP_SIZE 4

typedef struct HiEuCfgPld {
    union {
        HiEuCfgReq req;
        HiEuCfgRsp rsp;
    };
} HiEuCfgPld;

void msgq_sq_init(void *opaque);
void msgq_cq_init(void *opaque);
void msgq_rq_init(void *opaque);
void msgq_process_task(void *opaque, uint64_t val);
void msgq_handle_rst(void *opaque);
#endif
