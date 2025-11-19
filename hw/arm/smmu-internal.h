/*
 * ARM SMMU support - Internal API
 *
 * Copyright (c) 2017 Red Hat, Inc.
 * Copyright (C) 2014-2016 Broadcom Corporation
 * Written by Prem Mallappa, Eric Auger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_ARM_SMMU_INTERNAL_H
#define HW_ARM_SMMU_INTERNAL_H

#define TBI0(tbi) ((tbi) & 0x1)
#define TBI1(tbi) ((tbi) & 0x2 >> 1)

/* FEAT_LPA2 and FEAT_TTST are not implemented. */
static inline int get_start_level(int sl0 , int granule_sz)
{
    /* ARM DDI0487I.a: Table D8-12. */
    if (granule_sz == 12) {
        return 2 - sl0;
    }
    /* ARM DDI0487I.a: Table D8-22 and Table D8-31. */
    return 3 - sl0;
}

/*
 * Index in a concatenated first level stage-2 page table.
 * ARM DDI0487I.a: D8.2.2 Concatenated translation tables.
 */
static inline int pgd_concat_idx(int start_level, int granule_sz,
                                 dma_addr_t ipa)
{
    uint64_t ret;
    /*
     * Get the number of bits handled by next levels, then any extra bits in
     * the address should index the concatenated tables. This relation can be
     * deduced from tables in ARM DDI0487I.a: D8.2.7-9
     */
    int shift =  level_shift(start_level - 1, granule_sz);

    ret = ipa >> shift;
    return ret;
}

#define SMMU_IOTLB_ASID(key) ((key).asid)
#define SMMU_IOTLB_VMID(key) ((key).vmid)

typedef struct SMMUIOTLBPageInvInfo {
    int asid;
    int vmid;
    uint64_t iova;
    uint64_t mask;
} SMMUIOTLBPageInvInfo;

typedef struct SMMUSIDRange {
    SMMUState *state;
    uint32_t start;
    uint32_t end;
} SMMUSIDRange;

#endif
