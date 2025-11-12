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
#ifndef MMU_TRANSLATE_COMMON_H
#define MMU_TRANSLATE_COMMON_H

/* VMSAv8-64 Translation constants and functions */
#define VMSA_LEVELS                         4
#define VMSA_MAX_S2_CONCAT                  16

#define VMSA_STRIDE(gran)                   ((gran) - VMSA_LEVELS + 1)
#define VMSA_BIT_LVL(isz, strd, lvl)        ((isz) - (strd) * \
                                             (VMSA_LEVELS - (lvl)))
#define VMSA_IDXMSK(isz, strd, lvl)         ((1ULL << \
                                             VMSA_BIT_LVL(isz, strd, lvl)) - 1)

/* PTE Manipulation */
#define ARM_LPAE_PTE_TYPE_SHIFT         0
#define ARM_LPAE_PTE_TYPE_MASK          0x3

#define ARM_LPAE_PTE_TYPE_BLOCK         1
#define ARM_LPAE_PTE_TYPE_TABLE         3

#define ARM_LPAE_L3_PTE_TYPE_RESERVED   1
#define ARM_LPAE_L3_PTE_TYPE_PAGE       3

#define ARM_LPAE_PTE_VALID              (1 << 0)

#define PTE_ADDRESS(pte, shift) \
    (extract64(pte, shift, 47 - shift + 1) << shift)

#define is_invalid_pte(pte) (!(pte & ARM_LPAE_PTE_VALID))

#define is_reserved_pte(pte, level)                                      \
    ((level == 3) &&                                                     \
     ((pte & ARM_LPAE_PTE_TYPE_MASK) == ARM_LPAE_L3_PTE_TYPE_RESERVED))

#define is_block_pte(pte, level)                                         \
    ((level < 3) &&                                                      \
     ((pte & ARM_LPAE_PTE_TYPE_MASK) == ARM_LPAE_PTE_TYPE_BLOCK))

#define is_table_pte(pte, level)                                        \
    ((level < 3) &&                                                     \
     ((pte & ARM_LPAE_PTE_TYPE_MASK) == ARM_LPAE_PTE_TYPE_TABLE))

#define is_page_pte(pte, level)                                         \
    ((level == 3) &&                                                    \
     ((pte & ARM_LPAE_PTE_TYPE_MASK) == ARM_LPAE_L3_PTE_TYPE_PAGE))

/* access permissions */

#define PTE_AP(pte) \
    (extract64(pte, 6, 2))

#define PTE_APTABLE(pte) \
    (extract64(pte, 61, 2))

#define PTE_AF(pte) \
    (extract64(pte, 10, 1))
/*
 * TODO: At the moment all transactions are considered as privileged (EL1)
 * as IOMMU translation callback does not pass user/priv attributes.
 */
#define is_permission_fault(ap, perm) \
    (((perm) & IOMMU_WO) && ((ap) & 0x2))

#define is_permission_fault_s2(s2ap, perm) \
    (!(((s2ap) & (perm)) == (perm)))

#define PTE_AP_TO_PERM(ap) \
    (IOMMU_ACCESS_FLAG(true, !((ap) & 0x2)))

/* Level Indexing */

static inline int level_shift(int level, int granule_sz)
{
    return granule_sz + (3 - level) * (granule_sz - 3);
}

static inline uint64_t level_page_mask(int level, int granule_sz)
{
    return ~(MAKE_64BIT_MASK(0, level_shift(level, granule_sz)));
}

static inline
uint64_t iova_level_offset(uint64_t iova, int inputsize,
                           int level, int gsz)
{
    return ((iova & MAKE_64BIT_MASK(0, inputsize)) >> level_shift(level, gsz)) &
            MAKE_64BIT_MASK(0, gsz - 3);
}

/* VMSAv8-64 Translation Table Format Descriptor Decoding */

/**
 * get_page_pte_address - returns the L3 descriptor output address,
 * ie. the page frame
 * ARM ARM spec: Figure D4-17 VMSAv8-64 level 3 descriptor format
 */
static inline hwaddr get_page_pte_address(uint64_t pte, int granule_sz)
{
    return PTE_ADDRESS(pte, granule_sz);
}

/**
 * get_table_pte_address - return table descriptor output address,
 * ie. address of next level table
 * ARM ARM Figure D4-16 VMSAv8-64 level0, level1, and level 2 descriptor formats
 */
static inline hwaddr get_table_pte_address(uint64_t pte, int granule_sz)
{
    return PTE_ADDRESS(pte, granule_sz);
}

/**
 * get_block_pte_address - return block descriptor output address and block size
 * ARM ARM Figure D4-16 VMSAv8-64 level0, level1, and level 2 descriptor formats
 */
static inline hwaddr get_block_pte_address(uint64_t pte, int level,
                                           int granule_sz, uint64_t *bsz)
{
    int n = level_shift(level, granule_sz);

    *bsz = 1ULL << n;
    return PTE_ADDRESS(pte, n);
}

#endif