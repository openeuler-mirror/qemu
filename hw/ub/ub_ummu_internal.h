/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
#ifndef UB_UMMU_INTERNAL_H
#define UB_UMMU_INTERNAL_H
#include "hw/registerfields.h"
#include "hw/ub/ub_usi.h"
#include "sysemu/dma.h"
#include "sysemu/iommufd.h"
#include <linux/iommufd.h>

/* ummu spec register define */
REG32(IIDR, 0x0000)
    FIELD(IIDR, PROD_REVISION,         0, 4)
    FIELD(IIDR, PROD_VARIANT,          4, 4)
    FIELD(IIDR, PROD_ID,               8, 12)

REG32(AIDR, 0x0004)
    FIELD(AIDR, ARCH_MINOR_REV,        0, 4)
    FIELD(AIDR, ARCH_MAJOR_REV,        4, 4)

REG32(CAP0, 0x0010)
    FIELD(CAP0, DSTEID_SIZE,           0, 8)
    FIELD(CAP0, TOKENID_SIZE,          8, 5)
    FIELD(CAP0, ATTR_PERMS_OVR,       13, 1)
    FIELD(CAP0, ATTR_TYPES_OVR,       14, 1)
    FIELD(CAP0, S2_ATTR_TYPE,         15, 1)
    FIELD(CAP0, TCT_LEVEL,            16, 1)
    FIELD(CAP0, TECT_MODE,            17, 2)
    FIELD(CAP0, TECT_LEVEL,           19, 1)

REG32(CAP1, 0x0014)
    FIELD(CAP1, EVENTQ_SIZE,          0,  5)
    FIELD(CAP1, EVENTQ_NUMB,          5,  4)
    FIELD(CAP1, EVENTQ_SUPPORT,       9,  1)
    FIELD(CAP1, MCMDQ_SIZE,           10, 4)
    FIELD(CAP1, MCMDQ_NUMB,           14, 4)
    FIELD(CAP1, MCMDQ_SUPPORT,        18, 1)
    FIELD(CAP1, EVENT_GEN,            19, 1)
    FIELD(CAP1, STALL_MAX,            20, 12)

REG32(CAP2, 0x0018)
    FIELD(CAP2, VMID_TLBI,            0,  1)
    FIELD(CAP2, TLB_BOARDCAST,        1,  1)
    FIELD(CAP2, RANGE_TLBI,           2,  1)
    FIELD(CAP2, OA_SIZE,              3,  3)
    FIELD(CAP2, GRAN4K_T,             6,  1)
    FIELD(CAP2, GRAN16K_T,            7,  1)
    FIELD(CAP2, GRAN64K_T,            8,  1)
    FIELD(CAP2, VA_EXTEND,            9,  2)
    FIELD(CAP2, S2_TRANS,             11, 1)
    FIELD(CAP2, S1_TRANS,             12, 1)
    FIELD(CAP2, SMALL_TRANS,          13, 1)
    FIELD(CAP2, TRANS_FORM,           14, 2)

REG32(CAP3, 0x001C)
    FIELD(CAP3, HIER_ATTR_DISABLE,    0,  1)
    FIELD(CAP3, S2_EXEC_NEVER_CTRL,   1,  1)
    FIELD(CAP3, BBM_LEVEL,            2,  2)
    FIELD(CAP3, COHERENT_ACCESS,      4,  1)
    FIELD(CAP3, TTENDIAN_MODE,        5,  2)
    FIELD(CAP3, MTM_SUPPORT,          7,  1)
    FIELD(CAP3, HTTU_SUPPORT,         8,  2)
    FIELD(CAP3, HYP_S1CONTEXT,        10, 1)
    FIELD(CAP3, USI_SUPPORT,          11, 1)
    FIELD(CAP3, STALL_MODEL,          12, 2)
    FIELD(CAP3, TERM_MODEL,           14, 1)
    FIELD(CAP3, SATI_MAX,             15, 6)

REG32(CAP4, 0x0020)
    FIELD(CAP4, UCMDQ_UCPLQ_NUMB,     0,  8)
    FIELD(CAP4, UCMDQ_SIZE,           8,  4)
    FIELD(CAP4, UCPLQ_SIZE,           12, 4)
    FIELD(CAP4, UIEQ_SIZE,            16, 4)
    FIELD(CAP4, UIEQ_NUMB,            20, 4)
    FIELD(CAP4, UIEQ_SUPPORT,         24, 1)
    FIELD(CAP4, PPLB_SUPPORT,         25, 1)

REG32(CAP5, 0x0024)
    FIELD(CAP5, MAPT_SUPPORT,         0,  1)
    FIELD(CAP5, MAPT_MODE,            1,  2)
    FIELD(CAP5, GRAN2M_P,             3,  1)
    FIELD(CAP5, GRAN4K_P,             4,  1)
    FIELD(CAP5, TOKENVAL_CHK,         5,  1)
    FIELD(CAP5, TOKENVAL_CHK_MODE,    6,  2)
    FIELD(CAP5, RANGE_PLBI,           8,  1)
    FIELD(CAP5, PLB_BORDCAST,         9,  1)

REG32(CAP6, 0x0028)
    FIELD(CAP6, MTM_ID_MAX,           0,  16)
    FIELD(CAP6, MTM_GP_MAX,           16, 8)

#define UMMU_CTRL0_WMASK              GENMASK(5, 0)
REG32(CTRL0, 0x0030)
    FIELD(CTRL0, UMMU_EN,             0,  1)
    FIELD(CTRL0, EVENTQ_EN,           1,  1)
    FIELD(CTRL0, VMID_WILDCARD_T,     2,  3)
    FIELD(CTRL0, MAPT_EN,             5,  1)

REG32(CTRL0_ACK, 0x0034)
    FIELD(CTRL0_ACK, UMMU_EN,         0, 1)
    FIELD(CTRL0_ACK, EVENTQ_EN,       1, 1)
    FIELD(CTRL0_ACK, VMID_WILDCARD_T, 2, 3)
    FIELD(CTRL0_ACK, MAPT_EN,         5, 1)

#define UMMU_CTRL1_WMASK              GENMASK(15, 0)
REG32(CTRL1, 0x0038)
    FIELD(CTRL1, QUEUE_IC_T,          0,  2)
    FIELD(CTRL1, QUEUE_OC_T,          2,  2)
    FIELD(CTRL1, QUEUE_SH_T,          4,  2)
    FIELD(CTRL1, TABLE_IC_T,          6,  2)
    FIELD(CTRL1, TABLE_OC_T,          8,  2)
    FIELD(CTRL1, TABLE_SH_T,          10, 2)
    FIELD(CTRL1, E2H,                 12, 1)
    FIELD(CTRL1, BAD_DSTEID_RECORD,   13, 1)
    FIELD(CTRL1, PRIVATE_TLB,         14, 1)
    FIELD(CTRL1, TECT_MODE_SEL,       15, 1)

#define UMMU_CTRL2_WMASK              GENMASK(6, 0)
REG32(CTRL2, 0x003C)
    FIELD(CTRL2, PRIVATE_PLB,         6,  1)
    FIELD(CTRL2, UIE_QUEUE_SH_P,      4,  2)
    FIELD(CTRL2, UIE_QUEUE_OC_P,      2,  2)
    FIELD(CTRL2, UIE_QUEUE_IC_P,      0,  2)

#define UMMU_CTRL3_WMASK              (GENMASK(23, 0) | GENMASK(31, 31))
REG32(CTRL3, 0x0040)
    FIELD(CTRL3, UPDATE_FLG,          31, 1)
    FIELD(CTRL3, UOTR_MTM_GP,         16, 8)
    FIELD(CTRL3, UOTR_MTM_ID,         0, 16)

#define UMMU_TECT_BASE_WMASK          (GENMASK_ULL(51, 6) | GENMASK_ULL(63, 63))
REG32(TECT_BASE0, 0x0070)
    FIELD(TECT_BASE0, TECT_BASE_ADDR0, 6, 26)

REG32(TECT_BASE1, 0x0074)
    FIELD(TECT_BASE1, TECT_BASE_ADDR1, 0, 19)
    FIELD(TECT_BASE1, TECT_RA_CFG,     31, 1)

#define UMMU_TECT_BASE_CFG_WMASK       GENMASK_ULL(12, 0)
REG32(TECT_BASE_CFG, 0x0078)
    FIELD(TECT_BASE_CFG, TECT_LOG2SIZE, 0,  6)
    FIELD(TECT_BASE_CFG, TECT_SPLIT,    6,  5)
    FIELD(TECT_BASE_CFG, TECT_FMT,      11, 2)

#define UMMU_MCMDQ_BASE_WMASK          (GENMASK_ULL(51, 0) | GENMASK_ULL(63, 63))
#define UMMU_MCMDQ_PI_WMASK            (GENMASK(19, 0) | GENMASK(23, 23) | GENMASK(31, 31))
#define UMMU_MCMDQ_CI_WMASK            (GENMASK(19, 0) | GENMASK(26, 23) | GENMASK(31, 31))
#define A_MCMD_QUE_BASE 0x0100
#define A_MCMD_QUE_LASTEST_CI 0x10FC

#define UMMU_EVENTQ_BASE_WMASK         (GENMASK_ULL(4, 0) | GENMASK_ULL(51, 6) | GENMASK_ULL(63, 63))
REG32(EVENT_QUE_BASE0, 0x1100)
    FIELD(EVENT_QUE_BASE0, EVENT_QUE_LOG2SIZE, 0, 5)
    FIELD(EVENT_QUE_BASE0, EVENT_QUE_ADDR0,    6, 26)

REG32(EVENT_QUE_BASE1, 0x1104)
    FIELD(EVENT_QUE_BASE1, EVENT_QUE_ADDR1,  0, 20)
    FIELD(EVENT_QUE_BASE1, EVENT_QUE_WA_CFG, 31, 1)

#define UMMU_EVENTQ_PI_WMASK           (GENMASK(19, 0) | GENMASK(31, 31))
REG32(EVENT_QUE_PI, 0x1108)
    FIELD(EVENT_QUE_PI, EVENT_QUE_WR_IDX,  0, 19)
    FIELD(EVENT_QUE_PI, EVENT_QUE_WR_WRAP, 19, 1)
    FIELD(EVENT_QUE_PI, EVENT_QUE_OVFLG,   31, 1)

#define UMMU_EVENTQ_CI_WMASK          (GENMASK(19, 0) | GENMASK(31, 31))
REG32(EVENT_QUE_CI, 0x110C)
    FIELD(EVENT_QUE_CI, EVENT_QUE_RD_IDX,     0, 19)
    FIELD(EVENT_QUE_CI, EVENT_QUE_RD_WRAP,    19, 1)
    FIELD(EVENT_QUE_CI, EVENT_QUE_OVFLG_RESP, 31, 1)

#define UMMU_EVENTQ_USI_ADDR_WMASK    GENMASK_ULL(51, 2)
REG32(EVENT_QUE_USI_ADDR0, 0x1110)
    FIELD(EVENT_QUE_USI_ADDR0, USI_ADDR0, 2, 30)

REG32(EVENT_QUE_USI_ADDR1, 0x1114)
    FIELD(EVENT_QUE_USI_ADDR1, USI_ADDR1, 0, 20)

#define UMMU_EVENT_QUE_USI_DATA_WMASK GENMASK(31, 0)
REG32(EVENT_QUE_USI_DATA, 0x1118)
    FIELD(EVENT_QUE_USI_DATA, USI_DATA,   0, 32)

#define UMMU_EVENTQ_USI_ATTR_WMASK    GENMASK(5, 0)
REG32(EVENT_QUE_USI_ATTR, 0x111C)
    FIELD(EVENT_QUE_USI_ATTR, USI_MEM_ATTR_CFG, 0, 4)
    FIELD(EVENT_QUE_USI_ATTR, USI_SH_CFG,       4, 2)

REG32(GLB_INT_EN,   0x1130)
    FIELD(GLB_INT_EN, GLB_ERR_INT_EN,   0, 1)
    FIELD(GLB_INT_EN, EVENT_QUE_INT_EN, 1, 1)

REG32(GLB_ERR, 0x1134)
    FIELD(GLB_ERR, MCMD_QUE_ERR,          0, 1)
    FIELD(GLB_ERR, EVENT_QUE_ABT_ERR,     1, 1)
    FIELD(GLB_ERR, USI_MCMD_QUE_ABT_ERR,  2, 1)
    FIELD(GLB_ERR, USI_EVENT_QUE_ABT_ERR, 3, 1)
    FIELD(GLB_ERR, USI_UIEQ_QUE_ABT_ERR,  4, 1)
    FIELD(GLB_ERR, USI_GLB_ERR_ABT_ERR,   7, 1)

#define UMMU_GLB_ERR_RESP_WMASK       GENMASK(4, 0) | GENMASK(7, 7)
REG32(GLB_ERR_RESP, 0x1138)
    FIELD(GLB_ERR_RESP, MCMDQ_QUE_ERR,         0, 1)
    FIELD(GLB_ERR_RESP, EVENT_QUE_ABT_ERR,     1, 1)
    FIELD(GLB_ERR_RESP, USI_MCMDQ_QUE_ABT_ERR, 2, 1)
    FIELD(GLB_ERR_RESP, USI_EVENT_QUE_ABT_ERR, 3, 1)
    FIELD(GLB_ERR_RESP, USI_UIEQ_QUE_ABT_ERR,  4, 1)
    FIELD(GLB_ERR_RESP, USI_GLB_ERR_ABT_ERR,   7, 1)

#define UMMU_GLB_ERR_INT_USI_ADDR_WMASK GENMASK_ULL(51, 2)
REG32(GLB_ERR_INT_USI_ADDR0, 0x1140)
    FIELD(GLB_ERR_INT_USI_ADDR0, USI_ADDR0, 2, 29)

REG32(GLB_ERR_INT_USI_ADDR1, 0x1144)
    FIELD(GLB_ERR_INT_USI_ADDR1, USI_ADDR1, 0, 19)

#define UMMU_GLB_ERR_INT_USI_DATA_WMASK GENMASK(31, 0)
REG32(GLB_ERR_INT_USI_DATA, 0x1148)
    FIELD(GLB_ERR_INT_USI_DATA, USI_DATA, 0, 32)

#define UMMU_GLB_ERR_INT_USI_ATTR_WMASK GENMASK(5, 0)
REG32(GLB_ERR_INT_USI_ATTR, 0x114C)
    FIELD(GLB_ERR_INT_USI_ATTR, USI_MEM_ATTR_CFG, 0, 4)
    FIELD(GLB_ERR_INT_USI_ATTR, USI_SH_CFG,       4, 2)

#define MAPT_CMDQ_CTXT_BADDR_WMASK (((GENMASK_ULL(31, 31) | GENMASK_ULL(19, 0)) << 32) | \
                                    (GENMASK_ULL(4, 0) | GENMASK_ULL(31, 6)))
REG32(MAPT_CMDQ_CTXT_BADDR0, 0x1160)
    FIELD(MAPT_CMDQ_CTXT_BADDR0, MAPT_CMDQ_CTXT_LOG2SIZE, 0, 5)
    FIELD(MAPT_CMDQ_CTXT_BADDR0, MAPT_CMDQ_CTXT_ADDR0,    6, 26)

REG32(MAPT_CMDQ_CTXT_BADDR1, 0x1164)
    FIELD(MAPT_CMDQ_CTXT_BADDR1, MAPT_CMDQ_CTXT_ADDR1,    0, 20)
    FIELD(MAPT_CMDQ_CTXT_BADDR1, MAPT_CMDQ_CTXT_RA_CFG,   31, 1)

REG32(MAPT_CMDQ_CTXT_ATTR, 0x1168)
    FIELD(MAPT_CMDQ_CTXT_ATTR, MAPT_CMDQ_MEM_ATTR_CFG,     0, 4)
    FIELD(MAPT_CMDQ_CTXT_ATTR, MAPT_CMDQ_SH_CFG,           4, 2)

#define RELEASE_UM_QUEUE_WMASK 0x1
REG32(RELEASE_UM_QUEUE, 0x1178)
    FIELD(RELEASE_UM_QUEUE, MAPT_RLSE_UM_CMDQ, 0, 1)

#define RELEASE_UM_QUEUE_ID_WMASK GENMASK(30, 0)
REG32(RELEASE_UM_QUEUE_ID, 0x117C)
    FIELD(RELEASE_UM_QUEUE_ID, MAPT_RLSE_UM_CMDQ_ID, 0, 31)

#define A_UCMDQ_PI_START_REG 0x20000
/* MAPT Commd queue control page 4k:  0x2000C + 2^16 * 0x1000
 * MAPT Commd queue control page 64k: 0x2000C + 2^12 * 0x10000 */
#define A_UCPLQ_CI_END_REG   0x1002000C

/* ummu user register define */
REG32(UMMU_INT_MASK, 0x3404)
    FIELD(UMMU_INT_MASK, UIEQ_USI_MASK, 0, 1)
    FIELD(UMMU_INT_MASK, UBIF_USI_MASK, 1, 1)

REG32(DSTEID_KV_TABLE_BASE0, 0x3800)
    FIELD(DSTEID_KV_TABLE_BASE0, DSTEID_TV_TABLE_BASE_ADDR0, 5, 27)

REG32(DSTEID_KV_TABLE_BASE1, 0x3804)
    FIELD(DSTEID_KV_TABLE_BASE1, DSTEID_TV_TABLE_BASE_ADDR1, 0, 20)

REG32(DSTEID_KV_TABLE_BASE_CFG, 0x3808)
    FIELD(DSTEID_KV_TABLE_BASE_CFG, DSTEID_KV_TABLE_MEMATTR,  0,  4)
    FIELD(DSTEID_KV_TABLE_BASE_CFG, DSTEID_KV_TABLE_SH,       4,  2)
    FIELD(DSTEID_KV_TABLE_BASE_CFG, DSTEID_KV_TABLE_BANK_NUM, 8,  8)
    FIELD(DSTEID_KV_TABLE_BASE_CFG, DSTEID_KV_TABLE_DEPTH,    16, 16)

REG32(UMMU_DSTEID_KV_TABLE_HASH_CFG0, 0x380C)
    FIELD(UMMU_DSTEID_KV_TABLE_HASH_CFG0, DSTEID_KV_TABLE_HASH_SEL,   0, 4)
    FIELD(UMMU_DSTEID_KV_TABLE_HASH_CFG0, DSTEID_KV_TABLE_HASH_WIDTH, 4, 4)

REG32(UMMU_DSTEID_KV_TABLE_HASH_CFG1, 0x3810)
    FIELD(UMMU_DSTEID_KV_TABLE_HASH_CFG1, DSTEID_KV_TABLE_HASH_CRC32_SEED, 0, 32)

REG32(UMMU_DSTEID_CAM_TABLE_BASE0, 0x3820)
    FIELD(UMMU_DSTEID_CAM_TABLE_BASE0, DSTEID_CAM_TABLE_BASE_ADDR0, 5, 27)

REG32(UMMU_DSTEID_CAM_TABLE_BASE1, 0x3824)
    FIELD(UMMU_DSTEID_CAM_TABLE_BASE1, DSTEID_CAM_TABLE_BASE_ADDR1, 0, 20)

REG32(UMMU_DSTEID_CAM_TABLE_BASE_CFG, 0x3828)
    FIELD(UMMU_DSTEID_CAM_TABLE_BASE_CFG, DSTEID_CAM_TABLE_MEMATTR, 0, 4)
    FIELD(UMMU_DSTEID_CAM_TABLE_BASE_CFG, DSTEID_CAM_TABLE_SH, 4, 2)
    FIELD(UMMU_DSTEID_CAM_TABLE_BASE_CFG, DSTEID_CAM_TABLE_DEPTH, 16, 32)

#define MAPT_CMDQ_CTRLR_PAGE_SIZE_4K  1
#define MAPT_CMDQ_CTRLR_PAGE_SIZE_64K 0
#define UMCMD_PAGE_SEL_WMASK 0x1
REG32(UMCMD_PAGE_SEL, 0x3834)
    FIELD(UMCMD_PAGE_SEL, PAGE_MODEL_SEL_EN,      0, 1)


/* ummu user logic register define */
REG32(UMMU_USER_CONFIG0, 0x4C00)

REG32(UMMU_USER_CONFIG1, 0x4C04)

REG32(UMMU_USER_CONFIG2, 0x4C08)
    FIELD(UMMU_USER_CONFIG2, INV_TLB_ALL_NS,     0, 1)
    FIELD(UMMU_USER_CONFIG2, TBU_L2_MEM_INIT_EN, 1, 1)
    FIELD(UMMU_USER_CONFIG2, TBU_L2_MEM_INITING, 2, 1)
    FIELD(UMMU_USER_CONFIG2, MCMDQ_MEM_INIT_EN,  3, 1)
    FIELD(UMMU_USER_CONFIG2, MCMDQ_MEM_INITING,  4, 1)

REG32(UMMU_USER_CONFIG3, 0x4C0C)

REG32(UMMU_USER_CONFIG4, 0x4C10)

REG32(UMMU_USER_CONFIG5, 0x4C14)

REG32(UMMU_USER_CONFIG6, 0x4C18)

REG32(UMMU_USER_CONFIG7, 0x4C1C)

REG32(UMMU_USER_CONFIG8, 0x4C20)

REG32(UMMU_USER_CONFIG9, 0x4C24)

REG32(UMMU_USER_CONFIG10, 0x4C28)

REG32(UMMU_USER_CONFIG11, 0x4C2C)

REG32(UMMU_MEM_USI_ADDR0, 0x4D90)
    FIELD(UMMU_MEM_USI_ADDR0, UBIF_MEM_USI_ADDR0, 2, 30)

REG32(UMMU_MEM_USI_ADDR1, 0x4D94)
    FIELD(UMMU_MEM_USI_ADDR1, UBIF_MEM_USI_ADDR1, 0, 20)

REG32(UMMU_MEM_USI_DATA, 0x4D98)
    FIELD(UMMU_MEM_USI_DATA, UBIF_MEM_USI_DATA, 0, 32)

REG32(UMMU_MEM_USI_ATTR, 0x4D9C)
    FIELD(UMMU_MEM_USI_ATTR, UBIF_MEM_USI_MEM_ATTR_CFG, 0, 4)
    FIELD(UMMU_MEM_USI_ATTR, UBIF_MEM_USI_SH_CFG,       4, 2)

#define TYPE_UMMU_IOMMU_MEMORY_REGION "ummu-iommu-memory-region"

#define CMD_TYPE(x)                        extract32((x)->word[0], 0, 8)
#define CMD_SYNC_CM(x)                     extract32((x)->word[0], 12, 2)
#define CMD_SYNC_CM_NONE                   0x0
#define CMD_SYNC_CM_USI                    0x1
#define CMD_SYNC_CM_SEV                    0x2
#define CMD_SYNC_USI_SH(x)                 extract32((x)->word[0], 14, 2)
#define CMD_SYNC_USI_ATTR(x)               extract32((x)->word[0], 16, 4)
#define CMD_SYNC_USI_DATA(x)               extract32((x)->word[1], 0, 32)
#define CMD_SYNC_USI_ADDR(x)               ((*(uint64_t *)&(x)->word[2]) & GENMASK_ULL(51, 2))
#define CMD_CREATE_KVTBL_DEST_EID(x)       extract32((x)->word[4], 0, 32)
#define CMD_CREATE_KVTBL_BASE_ADDR(x)      ((*(uint64_t *)&(x)->word[2]) & GENMASK_ULL(51, 6))
#define CMD_CREATE_KVTBL_TECTE_TAG(x)      extract32((x)->word[0], 16, 16)
#define CMD_DELETE_KVTBL_DEST_EID(x)       extract32((x)->word[4], 0, 32)
#define CMD_DELETE_KVTBL_TECTE_TAG(x)      extract32((x)->word[0], 16, 16)
#define CMD_TECTE_TAG(x)                   extract32((x)->word[4], 0, 16)
#define CMD_TECTE_RANGE(x)                 extract32((x)->word[1], 20, 5)
/* according to UB SPEC, if range val is 31, invalid all tecte */
#define CMD_TECTE_RANGE_INVILID_ALL(x)     ((x) == 31)
#define CMD_NULL_SUBOP_CHECK_PA_CONTINUITY 1
#ifdef CONFIG_UBMEM_VMMU
#define CMD_NULL_SUBOP_CHECK_UBMEM_VMMU_SUPPORT 2
#endif
#define CMD_NULL_SUBOP(x)                  extract32((x)->word[0], 8, 8)
#define CMD_NULL_CHECK_PA_CONTI_SIZE(x)    (1 << extract32((x)->word[0], 24, 6))
#define CMD_NULL_CHECK_PA_CONTI_ADDR(x)    ((*(uint64_t *)&(x)->word[2]) & GENMASK_ULL(47, 12))
#define UMMU_RUN_IN_VM_FLAG                0x10
#define PA_CONTINUITY                      0x00
#define PA_NOT_CONTINUITY                  0x01
#ifdef CONFIG_UBMEM_VMMU
#define UBMEM_UMMU_NOT_SUPPORT             0x00
#endif

#define MCMDQ_BASE_ADDR_MASK               ~0xf0UL
#define MCMDQ_IDX_MASK                     0xf0
#define MCMDQ_PROD_WMASK                   0x808fffff
#define MCMDQ_CONS_WMASK                   0x878fffff
#define MCMDQ_PROD_BASE_ADDR               0x108
#define MCMDQ_CONS_BASE_ADDR               0x10C
#define MCMD_QUE_LOG2SIZE(x)               extract32(x, 0, 5)
#define MCMD_QUE_BASE_ADDR(que)            ((que)->base & GENMASK_ULL(51, 5))
#define MCMD_QUE_RD_IDX(que)               (extract32((que)->cons, 0, 19) & ((1 << (que)->log2size) - 1))
#define MCMD_QUE_WD_IDX(que)               (extract32((que)->prod, 0, 19) & ((1 << (que)->log2size) - 1))
#define MCMD_QUE_RD_WRAP(que)              extract32((que)->cons, (que)->log2size, 1)
#define MCMD_QUE_WD_WRAP(que)              extract32((que)->prod, (que)->log2size, 1)
#define MCMD_QUE_EN_BIT(que)               extract32((que)->prod, 31, 1)
#define MCMD_QUE_EN_RESP_BIT               31

#define EVENT_QUE_LOG2SIZE(x)              extract32(x, 0, 5)
#define EVENT_QUE_BASE_ADDR(que)           ((que)->base & GENMASK_ULL(51, 6))
#define EVENT_QUE_RD_IDX(que)              (extract32((que)->cons, 0, 19) & ((1 << (que)->log2size) - 1))
#define EVENT_QUE_WR_IDX(que)              (extract32((que)->prod, 0, 19) & ((1 << (que)->log2size) - 1))
#define EVENT_QUE_RD_WRAP(que)             extract32((que)->cons, (que)->log2size, 1)
#define EVENT_QUE_WR_WRAP(que)             extract32((que)->prod, (que)->log2size, 1)

#define TECT_BASE_ADDR(x)               ((x) & GENMASK_ULL(51, 6))
#define TECT_L2TECTE_PTR(x)             ((*(uint64_t *)&(x)->word[0]) & GENMASK_ULL(51, 6))
#define TECT_DESC_V(x)                  extract32((x)->word[0], 0, 1)
#define TECTE_TCT_PTR(x)                ((*(uint64_t *)&(x)->word[2]) & GENMASK_ULL(51, 6))
#define TECTE_TCT_NUM(x)                extract32((x)->word[2], 0, 5)
#define TECTE_TCT_FMT(x)                extract32((x)->word[3], 20, 2)
#define TECTE_VALID(x)                  extract32((x)->word[0],  0, 1)
#define TECTE_ST_MODE(x)                extract32((x)->word[0],  1, 3)
#define TECTE_ST_MODE_ABORT             0
#define TECTE_ST_MODE_BYPASS            4
#define TECTE_ST_MODE_S1                5
#define TECTE_ST_MODE_S2                6
#define TECTE_ST_MODE_NESTED            7

#define TCT_FMT_LINEAR 0
#define TCT_FMT_LVL2_4K 1
#define TCT_FMT_LVL2_64K 2
#define TCT_SPLIT_64K 10
#define TCT_L2_ENTRIES                  (1UL << TCT_SPLIT_64K)
#define TCT_L1TCTE_V(x)                 extract32((x)->word[0], 0, 1)
#define TCT_L2TCTE_PTR(x)               ((*(uint64_t *)&(x)->word[0]) & GENMASK_ULL(51, 12))
#define TCTE_TTBA(x)                    ((*(uint64_t *)&(x)->word[4]) & GENMASK_ULL(51, 4))
#define TCTE_TCT_V(x)                   extract32((x)->word[0], 0, 1)
#define TCTE_SZ(x)                      extract32((x)->word[2], 0, 6)
#define TCTE_TGS(x)                     extract32((x)->word[2], 6, 2)
/* according ub spec Chapter 9, tct max num is 2 ^ tct_num */
#define TCTE_MAX_NUM(x)                 (1 << (x))

#define MAPT_CMDQ_CTXT_BASE_BYTES       64
#define MAPT_CMDQ_CTXT_BASE_ADDR(x)     ((x) & GENMASK_ULL(51, 6))
#define UCMDQ_UCPLQ_CI_PI_MASK          0xFULL
#define UCMDQ_PI                        0x00
#define UCMDQ_CI                        0x04
#define UCPLQ_PI                        0x08
#define UCPLQ_CI                        0x0C
#define MAPT_4K_CMDQ_CTXT_QID(offset)   ((((offset) & (~0xFULL)) - A_UCMDQ_PI_START_REG) / 0x1000)
#define MAPT_64K_CMDQ_CTXT_QID(offset)  ((((offset) & (~0xFULL)) - A_UCMDQ_PI_START_REG) / 0x10000)
#define MAPT_UCMDQ_LOG2SIZE(base)       extract32((base)->word[0],   2,  4)
#define MAPT_UCMDQ_PI(base)             (extract32((base)->word[10],  0, 16) & \
                                            ((1 << MAPT_UCMDQ_LOG2SIZE(base)) - 1))
#define MAPT_UCMDQ_PI_WRAP(base)        extract32((base)->word[10], MAPT_UCMDQ_LOG2SIZE(base), 1)
#define MAPT_UCMDQ_CI(base)             (extract32((base)->word[10], 16, 16) & \
                                            ((1 << MAPT_UCMDQ_LOG2SIZE(base)) - 1))
#define MAPT_UCMDQ_CI_WRAP(base)        extract32((base)->word[10], 16 + MAPT_UCMDQ_LOG2SIZE(base), 1)
#define MAPT_UCMDQ_BASE_ADDR(base)      ((*(uint64_t *)&(base)->word[0]) & GENMASK_ULL(51, 12))

#define MAPT_UCMD_TYPE_PSYNC            0x01
#define MAPT_UCMD_TYPE_PLBI_USR_ALL     0x10
#define MAPT_UCMD_TYPE_PLBI_USR_VA      0x11
#define MAPT_UCMD_TYPE(cmd)             ((cmd)->word[0] & GENMASK(7, 0))

#define MAPT_UCPLQ_LOG2SIZE(base)       extract32((base)->word[2],   2,  4)
#define MAPT_UCPLQ_PI(base)             (extract32((base)->word[11],  0, 16) & \
                                            ((1 << MAPT_UCPLQ_LOG2SIZE(base)) - 1))
#define MAPT_UCPLQ_PI_WRAP(base)        extract32((base)->word[11], MAPT_UCPLQ_LOG2SIZE(base), 1)
#define MAPT_UCPLQ_CI(base)             (extract32((base)->word[11], 16, 16) & \
                                            ((1 << MAPT_UCPLQ_LOG2SIZE(base)) - 1))
#define MAPT_UCPLQ_CI_WRAP(base)        extract32((base)->word[11], 16 + MAPT_UCPLQ_LOG2SIZE(base), 1)
#define MAPT_UCPLQ_BASE_ADDR(base)      ((*(uint64_t *)&(base)->word[2]) & GENMASK_ULL(51, 12))
#define MAPT_UCPL_STATUS_INVALID        0x0
#define MAPT_UCPL_STATUS_PSYNC_SUCCESS  0x1
#define MAPT_UCPL_STATUS_TYPE_ERROR     0x2
#define MAPT_UCPL_STATUS_PROCESS_ERROR  0x3

typedef struct UMMUMcmdqCmd {
    uint32_t word[8];
} UMMUMcmdqCmd;

typedef struct UMMUEvent {
    uint32_t word[16];
} UMMUEvent;

typedef enum UmmuMcmdqCmdType {
    CMD_SYNC              = 0x1,
    CMD_STALL_RESUME      = 0x02,
    CMD_PREFET_CFG        = 0x04,
    CMD_CFGI_TECT         = 0x08,
    CMD_CFGI_TECT_RANGE   = 0x09,
    CMD_CFGI_TCT          = 0x0A,
    CMD_CFGI_TCT_ALL      = 0x0B,
    CMD_CFGI_VMS_PIDM     = 0x0C,
    CMD_PLBI_OS_EID       = 0x14,
    CMD_PLBI_OS_EIDTID    = 0x15,
    CMD_PLBI_OS_VA        = 0x16,
    CMD_TLBI_OS_ALL       = 0x10,
    CMD_TLBI_OS_TID       = 0x11,
    CMD_TLBI_OS_VA        = 0x12,
    CMD_TLBI_OS_VAA       = 0x13,
    CMD_TLBI_HYP_ALL      = 0x18,
    CMD_TLBI_HYP_TID      = 0x19,
    CMD_TLBI_HYP_VA       = 0x1A,
    CMD_TLBI_HYP_VAA      = 0x1B,
    CMD_TLBI_S1S2_VMALL   = 0x28,
    CMD_TLBI_S2_IPA       = 0x2a,
    CMD_TLBI_NS_OS_ALL    = 0x2C,
    CMD_RESUME            = 0x44,
    CMD_CREATE_KVTBL      = 0x60,
    CMD_DELETE_KVTBL      = 0x61,
    CMD_NULL              = 0x62,
    CMD_TLBI_OS_ALL_U     = 0x90,
    CMD_TLBI_OS_ASID_U    = 0x91,
    CMD_TLBI_OS_VA_U      = 0x92,
    CMD_TLBI_OS_VAA_U     = 0x93,
    CMD_TLBI_HYP_ASID_U   = 0x99,
    CMD_TLBI_HYP_VA_U     = 0x9A,
    CMD_TLBI_S1S2_VMALL_U = 0xA8,
    CMD_TLBI_S2_IPA_U     = 0xAa,
    MCMDQ_CMD_MAX,
} UmmuMcmdqCmdType;

typedef struct UMMUS2Hwpt {
    IOMMUFDBackend *iommufd;
    uint32_t hwpt_id;
    uint32_t ioas_id;
} UMMUS2Hwpt;

typedef struct UMMUViommu {
    UMMUState *ummu;
    IOMMUFDBackend *iommufd;
    IOMMUFDViommu *core;
    UMMUS2Hwpt *s2_hwpt;
    QLIST_HEAD(, UMMUDevice) device_list;
    QLIST_ENTRY(UMMUViommu) next;
} UMMUViommu;

typedef struct UMMUS1Hwpt {
    void *ummu;
    IOMMUFDBackend *iommufd;
    UMMUViommu *viommu;
    uint32_t hwpt_id;
    QLIST_HEAD(, UMMUDevice) device_list;
    QLIST_ENTRY(UMMUViommu) next;
} UMMUS1Hwpt;

typedef struct UMMUVdev {
    UMMUViommu *vummu;
    IOMMUFDVdev *core;
    uint32_t sid;
} UMMUVdev;

typedef struct UMMUDevice {
    UMMUState *ummu;
    IOMMUMemoryRegion iommu;
    AddressSpace as;
    AddressSpace as_sysmem;
    HostIOMMUDeviceIOMMUFD *idev;
    UMMUViommu *viommu;
    UMMUS1Hwpt *s1_hwpt;
    UBDevice *udev;
    UMMUVdev *vdev;
    QLIST_ENTRY(UMMUDevice) next;
    struct iommu_hw_info_ummu info;
} UMMUDevice;

typedef struct UMMUTransCfg {
    dma_addr_t tct_ptr;
    uint64_t tct_num;
    uint64_t tct_fmt;
    dma_addr_t tct_ttba;
    uint32_t tct_sz;
    uint32_t tct_tgs;
    uint32_t tecte_tag;
    uint32_t tid;
} UMMUTransCfg;

typedef enum UMMUEventType {
    EVT_NONE = 0,
    /* unsupport translation type */
    EVT_UT,
    /* dstEid overflow */
    EVT_BAD_DSTEID,
    /* abort when visit tect, or addr overflow */
    EVT_TECT_FETCH,
    /* TECT not valid, (V=0) */
    EVT_BAD_TECT,
    /* tect ent lack tokenid */
    EVT_RESERVE_0 = 5,
    /* reserved, no content */
    EVT_BAD_TOKENID,
    /* 1. TECT.TCT_MAXNUM = 0, tokenid disable,
     * 2. TECT.ST_MODE[0] = 0, stage 1 translation close.
     * 3. tokenid > TECT.TCT_MAXNUM
     * 4. lvl1 tct invalid in two-level tct
     */
    EVT_TCT_FETCH,
    /* invalid tct */
    EVT_BAD_TCT,
    /* error when Address Table walk */
    EVT_A_PTW_EABT,
    /* translation input bigger than max valid value,
     * or no valid translation table descriptor
     */
    EVT_A_TRANSLATION = 10,
    /* address translation out put bigger than max valid value */
    EVT_A_ADDR_SIZE,
    /* Access flag fault because of AF=0 */
    EVT_ACCESS,
    /* address translation permission error */
    EVT_A_PERMISSION,
    /* TLB or PLB conflicted in translation */
    EVT_TBU_CONFLICT,
    /* config cache conflicted in translation */
    EVT_CFG_CONFLICT = 15,
    /* error occured when getting VMS */
    EVT_VMS_FETCH,
    /* error when Permission Table walk */
    EVT_P_PTW_EABT,
    /* abnormal software configuration in PTW */
    EVT_P_CFG_ERROR,
    /* permission exception in PTW process */
    EVT_P_PERMISSION,
    /* E-Bit verification failed */
    EVT_RESERVE_1 = 20,
    /* reserved, no content */
    EVT_EBIT_DENY,
    /* the UMMU hardware reports the execution result
     * of the CMD_CREAT_DSTEID_TECT_RELATION command
     * to the software.
     */
    EVT_CREATE_DSTEID_TECT_RELATION_RESULT = 60,
    /* the UMMU hardware reports the execution result
     * of the CMD_DELETE_DSTEID_TECT_RELATION command
     * to the software.
     */
    EVT_DELETE_DSTEID_TECT_RELATION_RESULT,
    EVT_MAX
} UMMUEventType;

typedef struct UMMUEventInfo {
    UMMUEventType type;
    uint32_t tecte_tag;
    uint32_t tid;
    union {
        struct {
            bool stall;
        } f_translation;
    } u;
    /* TODO */
} UMMUEventInfo;

typedef enum {
    UMMU_PTW_ERR_NONE,
    UMMU_PTW_ERR_TRANSLATION,
    UMMU_PTW_ERR_PERMISSION
} UMMUPTWEventType;

typedef struct UMMUPTWEventInfo {
    UMMUPTWEventType type;
} UMMUPTWEventInfo;

#define EVT_SET_TYPE(x, v)        ((x)->word[0] = deposit32((x)->word[0], 0, 8, v))
#define EVT_SET_TECTE_TAG(x, v)   ((x)->word[8] = deposit32((x)->word[8], 0, 16, v))
#define EVT_SET_TID(x, v)         ((x)->word[1] = deposit32((x)->word[1], 0, 20, v))

/* TECTE Level 1 Description */
typedef struct TECTEDesc {
    uint32_t word[2];
} TECTEDesc;

/* TCTE Level1 Description */
typedef struct TCTEDesc {
    uint32_t word[2];
} TCTEDesc;

/* Target Entity Config Table Entry (TECTE) */
typedef struct TECTE {
    uint32_t word[16];
} TECTE;

/* Target Contex Table Entry (TCTE) */
typedef struct TCTE {
    uint32_t word[16];
} TCTE;

typedef struct MAPTCmdqBase {
    uint32_t word[16];
} MAPTCmdqBase;

typedef struct MAPTCmd {
    uint32_t word[4];
} MAPTCmd;

typedef struct MAPTCmdCpl {
    uint32_t cpl_status : 4;
    uint32_t rsv :        12;
    uint32_t cmdq_ci :    16;
} MAPTCmdCpl;

typedef struct UMMUTecteRange {
    bool invalid_all;
    uint32_t start;
    uint32_t end;
} UMMUTecteRange;

static inline void update_reg32_by_wmask(uint32_t *old, uint32_t new, uint32_t wmask)
{
    *old = (*old & ~wmask) | (new & wmask);
}

static inline void update_reg64_by_wmask(uint64_t *old, uint64_t new, uint64_t wmask)
{
    *old = (*old & ~wmask) | (new & wmask);
}

static inline bool ummu_mcmdq_enabled(UMMUMcmdQueue *mcmdq)
{
    return MCMD_QUE_EN_BIT(&mcmdq->queue);
}

static inline void ummu_mcmdq_enable_resp(UMMUMcmdQueue *mcmdq)
{
    mcmdq->queue.cons |= GENMASK(MCMD_QUE_EN_RESP_BIT, MCMD_QUE_EN_RESP_BIT);
}

static inline void ummu_mcmdq_disable_resp(UMMUMcmdQueue *mcmdq)
{
    mcmdq->queue.cons &= ~(GENMASK(MCMD_QUE_EN_RESP_BIT, MCMD_QUE_EN_RESP_BIT));
}

static inline bool ummu_mcmdq_empty(UMMUMcmdQueue *mcmdq)
{
    UMMUQueue *q = &mcmdq->queue;

    return MCMD_QUE_WD_IDX(q) == MCMD_QUE_RD_IDX(q) &&
           MCMD_QUE_WD_WRAP(q) == MCMD_QUE_RD_WRAP(q);
}

static inline void ummu_mcmdq_cons_incr(UMMUMcmdQueue *mcmdq)
{
    mcmdq->queue.cons =
        deposit32(mcmdq->queue.cons, 0, mcmdq->queue.log2size + 1, mcmdq->queue.cons + 1);
}

static inline void ummu_set_event_que_int_en(UMMUState *u, uint64_t data)
{
    u->eventq.event_que_int_en = FIELD_EX32(data, GLB_INT_EN, EVENT_QUE_INT_EN);
}

static inline void ummu_set_glb_err_int_en(UMMUState *u, uint64_t data)
{
    u->glb_err.glb_err_int_en = FIELD_EX32(data, GLB_INT_EN, GLB_ERR_INT_EN);
}

static inline bool ummu_event_que_int_en(UMMUState *u)
{
    return u->eventq.event_que_int_en;
}

static inline bool ummu_glb_err_int_en(UMMUState *u)
{
    return u->glb_err.glb_err_int_en;
}

static inline USIMessage ummu_get_eventq_usi_message(UMMUState *u)
{
    USIMessage msg;

    msg.address = u->eventq.usi_addr;
    msg.data = u->eventq.usi_data;

    return msg;
}

static inline USIMessage ummu_get_gerror_usi_message(UMMUState *u)
{
    USIMessage msg;

    msg.address = u->glb_err.usi_addr;
    msg.data = u->glb_err.usi_data;

    return msg;
}

#define UMMU_TECT_MODE_SPARSE_TABLE 0x1
static inline uint32_t ummu_tect_mode_sparse_table(UMMUState *u)
{
    return FIELD_EX32(u->ctrl[1], CTRL1, TECT_MODE_SEL) & UMMU_TECT_MODE_SPARSE_TABLE;
}

#define UMMU_FEAT_2_LVL_TECT 0x1
static inline uint32_t ummu_tect_fmt_2level(UMMUState *u)
{
    return FIELD_EX32(u->tect_base_cfg, TECT_BASE_CFG, TECT_FMT) & UMMU_FEAT_2_LVL_TECT;
}

static inline uint32_t ummu_tect_split(UMMUState *u)
{
    return FIELD_EX32(u->tect_base_cfg, TECT_BASE_CFG, TECT_SPLIT);
}

static inline int tgs2granule(int bits)
{
    switch (bits) {
    case 0:
        /* Translation Granule Size: 2 ^ 12 = 4K */
        return 12;
    case 1:
        /* Translation Granule Size: 2 ^ 16 = 64K */
        return 16;
    case 2:
        /* Translation Granule Size: 2 ^ 14 = 16K */
        return 14;
    default:
        return 0;
    }
}

static inline bool ummu_eventq_enabled(UMMUState *u)
{
    return !!FIELD_EX32(u->ctrl[0], CTRL0, EVENTQ_EN);
}

static inline bool ummu_eventq_full(UMMUEventQueue *eventq)
{
    UMMUQueue *q = &eventq->queue;

    return EVENT_QUE_WR_IDX(q) == EVENT_QUE_RD_IDX(q) &&
           EVENT_QUE_WR_WRAP(q) != EVENT_QUE_RD_WRAP(q);
}

static inline bool ummu_eventq_empty(UMMUEventQueue *eventq)
{
    UMMUQueue *q = &eventq->queue;

    return EVENT_QUE_WR_IDX(q) == EVENT_QUE_RD_IDX(q) &&
           EVENT_QUE_WR_WRAP(q) == EVENT_QUE_RD_WRAP(q);
}

static inline void ummu_eventq_prod_incr(UMMUEventQueue *eventq)
{
    UMMUQueue *q = &eventq->queue;

    /* qlog2size + 1: add 1 which is consider for queue wrap bit.
     * when cons == prod, the queue may full or empty, according warp bit
     * to detemin full or emtpy. if cons.wrap == prod.wrap, the queue empty,
     * if cons.wrap != prod.wrap, the queue full.
     * */
    q->prod = deposit32(q->prod, 0, q->log2size + 1, q->prod + 1);
}

/*
 * MAPT Cmd Queue Base Struct
 *    ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
 *    │31│30│29│28│27│26│25│24│23│22│21│20│19│18│17│16│15│14│13│12│11│10│ 9│ 8│ 7│ 6│ 5│ 4│ 3│ 2│ 1│ 0│
 *  0 │             UCMD QUEUE BASE ADDRESS[31:12]                │                                   │
 *  1 │                                   │            UCMD QUEUE BASE ADDRESS[51:32]                 │
 *  2 │             UCPL QUEUE BASE ADDRESS[31:12]                │                                   │
 *  3 │                                   │            UCPL QUEUE BASE ADDRESS[51:32]                 │
 *  4 │                                               │            TECTE_TAG                          │
 *  5 │                                                                                               │
 *  6 │                                                                                               │
 *  7 │                                                                                               │
 *  8 │                                   │                      TokenID                              │
 *  9 │                                                                                               │
 * 10 │            UCMQ_QUEUE CI                      │          UCMQ_QUEUE PI                        │
 * 11 │            UCPL_QUEUE CI                      │          UCPL_QUEUE PI                        │
 * 12 │                                                                                               │
 * 13 │                                                                                               │
 * 14 │                                                                                               │
 * 15 │                                                                                               │
 *    └───────────────────────────────────────────────────────────────────────────────────────────────┘
 */
static inline void ummu_mapt_cmdq_base_update_ucmdq_pi(MAPTCmdqBase *base, uint16_t data)
{
    base->word[10] = deposit32(base->word[10], 0, 16, data);
}

static inline void ummu_mapt_cmdq_base_update_ucmdq_ci(MAPTCmdqBase *base, uint16_t data)
{
    base->word[10] = deposit32(base->word[10], 16, 16, data);
}

static inline void ummu_mapt_cmdq_base_update_ucplq_pi(MAPTCmdqBase *base, uint16_t data)
{
    base->word[11] = deposit32(base->word[11], 0, 16, data);
}

static inline void ummu_mapt_cmdq_base_update_ucplq_ci(MAPTCmdqBase *base, uint16_t data)
{
    base->word[11] = deposit32(base->word[11], 16, 16, data);
}

static inline uint16_t ummu_mapt_cmdq_base_get_ucmdq_pi(MAPTCmdqBase *base)
{
    return extract32(base->word[10], 0, 16);
}

static inline uint16_t ummu_mapt_cmdq_base_get_ucmdq_ci(MAPTCmdqBase *base)
{
    return extract32(base->word[10], 16, 16);
}

static inline uint16_t ummu_mapt_cmdq_base_get_ucplq_pi(MAPTCmdqBase *base)
{
    return extract32(base->word[11], 0, 16);
}

static inline uint16_t ummu_mapt_cmdq_base_get_ucplq_ci(MAPTCmdqBase *base)
{
    return extract32(base->word[11], 16, 16);
}

static inline uint16_t ummu_mapt_cmdq_base_get_tecte_tag(MAPTCmdqBase *base)
{
    return extract32(base->word[4], 0, 16);
}

static inline uint32_t ummu_mapt_cmdq_base_get_token_id(MAPTCmdqBase *base)
{
    return extract32(base->word[8], 0, 20);
}

static inline bool ummu_mapt_ucmdq_empty(MAPTCmdqBase *base)
{
    return MAPT_UCMDQ_PI(base) == MAPT_UCMDQ_CI(base) &&
           MAPT_UCMDQ_PI_WRAP(base) == MAPT_UCMDQ_CI_WRAP(base);
}

static inline void ummu_mapt_ucmdq_cons_incr(MAPTCmdqBase *base)
{
    base->word[10] = deposit32(base->word[10], 16,
                               MAPT_UCMDQ_LOG2SIZE(base) + 1,
                               ummu_mapt_cmdq_base_get_ucmdq_ci(base) + 1);
}

static inline bool ummu_mapt_ucplq_full(MAPTCmdqBase *base)
{
    return MAPT_UCPLQ_PI(base) == MAPT_UCPLQ_CI(base) &&
           MAPT_UCPLQ_PI_WRAP(base) != MAPT_UCPLQ_CI_WRAP(base);
}

static inline void ummu_mapt_ucqlq_prod_incr(MAPTCmdqBase *base)
{
    base->word[11] = deposit32(base->word[11], 0,
                               MAPT_UCPLQ_LOG2SIZE(base) + 1,
                               ummu_mapt_cmdq_base_get_ucplq_pi(base) + 1);
}

static inline void ummu_mapt_ucplq_set_cpl(MAPTCmdCpl *cpl, uint16_t status, uint16_t ci)
{
    cpl->cpl_status = status;
    cpl->cmdq_ci = ci;
}

static inline uint32_t ummu_mapt_cmdq_get_qid(UMMUState *u, uint64_t offset)
{
    if (u->ucmdq_page_sel == MAPT_CMDQ_CTRLR_PAGE_SIZE_4K) {
        return MAPT_4K_CMDQ_CTXT_QID(offset);
    } else {
        return MAPT_64K_CMDQ_CTXT_QID(offset);
    }
}

static inline void ummu_mcmdq_construct_plbi_os_eidtid(UMMUMcmdqCmd *mcmd_cmd, uint32_t tid, uint16_t tag)
{
    mcmd_cmd->word[0] = deposit32(mcmd_cmd->word[0], 0, 8, CMD_PLBI_OS_EIDTID);
    mcmd_cmd->word[0] = deposit32(mcmd_cmd->word[0], 12, 20, tid);
    mcmd_cmd->word[4] = deposit32(mcmd_cmd->word[4], 0, 16, tag);
}

static inline void ummu_plib_usr_va_to_pibi_os_va(MAPTCmd *mapt_cmd, UMMUMcmdqCmd *mcmd_cmd,
                                                  uint32_t tid, uint16_t tag)
{
    mcmd_cmd->word[0] = deposit32(mcmd_cmd->word[0], 0, 8, CMD_PLBI_OS_VA);
    mcmd_cmd->word[0] = deposit32(mcmd_cmd->word[0], 12, 20, tid);
    mcmd_cmd->word[1] = deposit32(mcmd_cmd->word[1], 0, 6, extract32(mapt_cmd->word[1], 0, 6));
    mcmd_cmd->word[2] = mapt_cmd->word[2] & 0xFFFFF000;
    mcmd_cmd->word[3] = mapt_cmd->word[3];
    mcmd_cmd->word[4] = deposit32(mcmd_cmd->word[4], 0, 16, tag);
}

void ummu_dev_uninstall_nested_tecte(UMMUDevice *ummu_dev);
int ummu_dev_install_nested_tecte(UMMUDevice *sdev, uint32_t data_type,
                                  uint32_t data_len, void *data);
#endif
