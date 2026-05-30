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

#ifndef HW_UB_MEM_H
#define HW_UB_MEM_H

/*
 *
 *                  +---------------------------------------------------------+
 *                  |                       CPU Die                           |
 *                  |  +-------------+-------------+---------------+          |
 *                  |  |    A        |      B      |      C        | AddrSpace|
 *                  |__+-/-----------+-----------\-+--------------\+ _________|
 *                      /                         \                \
 *   +-----------------/---------------------------\----------------\-------------+
 *   |  +-------------/ +-----------+ +-----------+ \------------+ +-\----------+ |
 *   |  | MAR0(Master)| |MAR0(Slave)| |MAR1(Slave)| |MAR1(Master)| |MAR2(Master)| |
 *   |  +-------------+ +-----------+ +-----------+ +------------+ +------------+ |
 *   |  +---------------------------+ +--------------------------+ +------------+ |
 *   |  |            MAR0           | |           MAR1           | |    MAR2    | |
 *   |  | +-----------+ +---------+ | | +---------+ +----------+ | | +--------+ | |
 *   |  | |    NL0    | |    NL1  | | | |   NL2   | |   NL3    | | | |   NL4  | | |
 *   |  | +-----------+ +---------+ | | +---------+ +----------+ | | +--------+ | |
 *   |  | +----+ +----+ +---+ +---+ | | +---+ +---+ +---+ +----+ | | +--------+ | |
 *   |  | |    | |    | |   | |   | | | |   | |   | |   | |    | | | |        | | |
 *   |  | +----+ +----+ +---+ +---+ | | +---+ +---+ +---+ +----+ | | +--------+ | |
 *   +----------------------------------------------------------------------------+
*/
#define MAR_NUM_ONE_UDIE     5
#define DECODER0_MARID       0
#define DECODER_SLAVE_MARID0 1
#define DECODER_SLAVE_MARID1 2
#define DECODER1_MARID       3
#define DECODER2_MARID       4
#define MB_SIZE_OFFSET       20
#define UB_MEM_MAR0_SPACE_SIZE (512 * GiB)
#define UB_MEM_MAR1_SPACE_SIZE (0)
#define UB_MEM_MAR2_SPACE_SIZE (0)
#define UB_MEM_MAR3_SPACE_SIZE (512 * GiB)
#define UB_MEM_MAR4_SPACE_SIZE (1024 * GiB)
#define UB_MEM_SPACE_SIZE (UB_MEM_MAR0_SPACE_SIZE + \
                           UB_MEM_MAR1_SPACE_SIZE + \
                           UB_MEM_MAR2_SPACE_SIZE + \
                           UB_MEM_MAR3_SPACE_SIZE + \
                           UB_MEM_MAR4_SPACE_SIZE)
#define UB_MEM_REG_SHIFT 16
#define UMMU_EXT_REG_SIZE 0x100
#define UB_MEM_VALID_VALUE 0
#define UB_MEM_VALID_MASK GENMASK_ULL(2, 0)
#define UB_MEM_REG_BASE 0x800000
#define UMMU_MEM_START_ADDR 0x0
#define START_PTE_ADDR_MASK GENMASK(26, 0)
#define START_ATE_ADDR_MASK GENMASK(22, 0)
#define UMMU_MEM_LEN_GRANU 0x4
#define MEM_GRANU_MASK GENMASK(19, 17)
#define MEM_GRANU_SHIFT 17
#define MEM_LEN_MASK GENMASK(16, 0)
#define UMMU_MEM_BTE 0x8
#define MEM_BTE_MASK GENMASK(16, 0)
#define UMMU_MEM_INDEX 0xC
#define MEM_INDEX_RSV_MASK GENMASK(31, 20)
#define MEM_WR_MASK (1UL << 19)
#define MEM_TYPE_MASK (1UL << 18)
#define MEM_VLD_MASK (1UL << 17)
#define MEM_PTE_INDEX_MASK GENMASK(9, 0)
#define MEM_ATE_INDEX_MASK GENMASK(16, 0)
#define UMMU_MEM_DTLB_INVLD 0x10
#define MEM_DTLB_INVLD_MASK (1UL)
typedef struct UbMemMmuInfo {
    /* valid bits
     * bit0: protection_table_bits
     * bit1: translation_table_bits
     * bit2: ummu_reg_addr_bits
     *  other reserved
     */
    uint64_t valid_bits;
    uint32_t protection_table_bits;
    uint32_t translation_table_bits;
    uint64_t ext_reg_base;
    uint64_t ext_reg_size;
    uint8_t reserved[48];
} UbMemMmuInfo;

typedef struct UbMemDecoderInfo {
    uint64_t decode_addr;
    uint32_t cc_base_addr;
    uint32_t cc_base_size;
    uint32_t nc_base_addr;
    uint32_t nc_base_size;
} UbMemDecoderInfo;

typedef struct UbcVendorInfo {
    uint32_t ub_mem_ver;
    uint8_t max_addr_bits;
    uint8_t reserved1[3];
    UbMemDecoderInfo mem_info[MAR_NUM_ONE_UDIE];
    uint64_t cmd_queue_base;  /* IO Decoder CMD   queue  */
    uint64_t event_queue_base;  /* IO Decoder Event queue  */
    uint64_t vendor_feature_sets;  /* bit0: management plane deployment 0(enable) 1(disable)
                                    * bit1~31: reserved
                                    * bit32~55: UB feature capability from sysfs
                                    * bit56~63: reserved */
    uint8_t reserved2[104];
} UbcVendorInfo;

/* hisi memory */
typedef struct UbMemBlockHw {
    /* DW0 */
    uint32_t valid : 1;
    uint32_t mem_base : 9;
    uint32_t mem_limit : 9;
    uint32_t one_path : 1;
    uint32_t wr_delay_comp : 1;
    uint32_t reduce_delay_comp : 1;
    uint32_t cmo_delay_comp : 1;
    uint32_t so : 1;
    uint32_t lb_0 : 8;
    /* DW1 */
    uint32_t token_id0 : 20;
    uint32_t dcna0_l : 12;
    /* DW2 */
    uint32_t dcna0_h : 4;
    uint32_t uba_base0_l : 28;
    /* DW3 */
    uint32_t uba_base0_h : 15;
    uint32_t pa_0 : 1;
    uint32_t rsv : 16;
} UbMemBlockHw;

#define MAX_BLOCKS 4
typedef struct UbMemPageEntry {
    UbMemBlockHw blocks[MAX_BLOCKS];
} UbMemPageEntry;

typedef enum UbMemEntryGranule {
    GRANULE_1GB    = 0,
    GRANULE_2GB    = 1,
    GRANULE_4GB    = 2,
    GRANULE_8GB    = 3,
    GRANULE_16GB   = 4,
    GRANULE_32GB   = 5,
    GRANULE_64GB   = 6,
    GRANULE_128GB  = 7,
    GRANULE_256GB  = 8,
    GRANULE_512GB  = 9,
    GRANULE_1TB    = 10,
} UbMemEntryGranule;
#endif
