/*
 * loongarch tlb emulation helpers for qemu.
 *
 * Copyright (c) 2023 Loongarch Technology
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "cpu.h"
#include "internal.h"
#include "qemu/host-utils.h"
#include "exec/helper-proto.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"

#ifndef CONFIG_USER_ONLY

#define HELPER_LD(name, insn, type)                                           \
    static inline type do_##name(CPULOONGARCHState *env, target_ulong addr,   \
                                 int mem_idx, uintptr_t retaddr)              \
    {                                                                         \
    }

void helper_lddir(CPULOONGARCHState *env, target_ulong base, target_ulong rt,
                  target_ulong level, uint32_t mem_idx)
{
}

void helper_ldpte(CPULOONGARCHState *env, target_ulong base, target_ulong odd,
                  uint32_t mem_idx)
{
}

target_ulong helper_read_pgd(CPULOONGARCHState *env)
{
    uint64_t badv;

    assert(env->CSR_TLBRERA & 0x1);

    if (env->CSR_TLBRERA & 0x1) {
        badv = env->CSR_TLBRBADV;
    } else {
        badv = env->CSR_BADV;
    }

    if ((badv >> 63) & 0x1) {
        return env->CSR_PGDH;
    } else {
        return env->CSR_PGDL;
    }
}

/* TLB management */
static uint64_t ls3a5k_pagesize_to_mask(int pagesize)
{
    /* 4KB - 1GB */
    if (pagesize < 12 && pagesize > 30) {
        printf("[ERROR] unsupported page size %d\n", pagesize);
        exit(-1);
    }

    return (1 << (pagesize + 1)) - 1;
}

static void ls3a5k_fill_tlb_entry(CPULOONGARCHState *env, ls3a5k_tlb_t *tlb,
                                  int is_ftlb)
{
    uint64_t page_mask; /* 0000...00001111...1111 */
    uint32_t page_size;
    uint64_t entryhi;
    uint64_t lo0, lo1;

    if (env->CSR_TLBRERA & 0x1) {
        page_size = env->CSR_TLBREHI & 0x3f;
        entryhi = env->CSR_TLBREHI;
        lo0 = env->CSR_TLBRELO0;
        lo1 = env->CSR_TLBRELO1;
    } else {
        page_size = (env->CSR_TLBIDX >> CSR_TLBIDX_PS_SHIFT) & 0x3f;
        entryhi = env->CSR_TLBEHI;
        lo0 = env->CSR_TLBELO0;
        lo1 = env->CSR_TLBELO1;
    }

    if (page_size == 0) {
        printf("Warning: page_size is 0\n");
    }

    /*
     *       15-12  11-8   7-4    3-0
     * 4KB: 0001   1111   1111   1111 // double 4KB  mask [12:0]
     * 16KB: 0111   1111   1111   1111 // double 16KB mask [14:0]
     */
    if (is_ftlb) {
        page_mask = env->tlb->mmu.ls3a5k.ftlb_mask;
    } else {
        page_mask = ls3a5k_pagesize_to_mask(page_size);
    }

    tlb->VPN = entryhi & 0xffffffffe000 & ~page_mask;

    tlb->ASID = env->CSR_ASID & 0x3ff; /* CSR_ASID[9:0] */
    tlb->EHINV = 0;
    tlb->G = (lo0 >> CSR_TLBLO0_GLOBAL_SHIFT) & /* CSR_TLBLO[6] */
             (lo1 >> CSR_TLBLO1_GLOBAL_SHIFT) & 1;

    tlb->PageMask = page_mask;
    tlb->PageSize = page_size;

    tlb->V0 = (lo0 >> CSR_TLBLO0_V_SHIFT) & 0x1;     /* [0] */
    tlb->WE0 = (lo0 >> CSR_TLBLO0_WE_SHIFT) & 0x1;   /* [1] */
    tlb->PLV0 = (lo0 >> CSR_TLBLO0_PLV_SHIFT) & 0x3; /* [3:2] */
    tlb->C0 = (lo0 >> CSR_TLBLO0_CCA_SHIFT) & 0x3;   /* [5:4] */
    tlb->PPN0 = (lo0 & 0xfffffffff000 & ~(page_mask >> 1));
    tlb->RI0 = (lo0 >> CSR_TLBLO0_RI_SHIFT) & 0x1;     /* [61] */
    tlb->XI0 = (lo0 >> CSR_TLBLO0_XI_SHIFT) & 0x1;     /* [62] */
    tlb->RPLV0 = (lo0 >> CSR_TLBLO0_RPLV_SHIFT) & 0x1; /* [63] */

    tlb->V1 = (lo1 >> CSR_TLBLO1_V_SHIFT) & 0x1;     /* [0] */
    tlb->WE1 = (lo1 >> CSR_TLBLO1_WE_SHIFT) & 0x1;   /* [1] */
    tlb->PLV1 = (lo1 >> CSR_TLBLO1_PLV_SHIFT) & 0x3; /* [3:2] */
    tlb->C1 = (lo1 >> CSR_TLBLO1_CCA_SHIFT) & 0x3;   /* [5:4] */
    tlb->PPN1 = (lo1 & 0xfffffffff000 & ~(page_mask >> 1));
    tlb->RI1 = (lo1 >> CSR_TLBLO1_RI_SHIFT) & 0x1;     /* [61] */
    tlb->XI1 = (lo1 >> CSR_TLBLO1_XI_SHIFT) & 0x1;     /* [62] */
    tlb->RPLV1 = (lo1 >> CSR_TLBLO1_RPLV_SHIFT) & 0x1; /* [63] */
}

static void ls3a5k_fill_tlb(CPULOONGARCHState *env, int idx, bool tlbwr)
{
    ls3a5k_tlb_t *tlb;

    tlb = &env->tlb->mmu.ls3a5k.tlb[idx];
    if (tlbwr) {
        if ((env->CSR_TLBIDX >> CSR_TLBIDX_EHINV_SHIFT) & 0x1) {
            tlb->EHINV = 1;
            return;
        }
    }

    if (idx < 2048) {
        ls3a5k_fill_tlb_entry(env, tlb, 1);
    } else {
        ls3a5k_fill_tlb_entry(env, tlb, 0);
    }
}

void ls3a5k_flush_vtlb(CPULOONGARCHState *env)
{
    uint32_t ftlb_size = env->tlb->mmu.ls3a5k.ftlb_size;
    uint32_t vtlb_size = env->tlb->mmu.ls3a5k.vtlb_size;
    int i;

    ls3a5k_tlb_t *tlb;

    for (i = ftlb_size; i < ftlb_size + vtlb_size; ++i) {
        tlb = &env->tlb->mmu.ls3a5k.tlb[i];
        tlb->EHINV = 1;
    }

    cpu_loongarch_tlb_flush(env);
}

void ls3a5k_flush_ftlb(CPULOONGARCHState *env)
{
    uint32_t ftlb_size = env->tlb->mmu.ls3a5k.ftlb_size;
    int i;

    ls3a5k_tlb_t *tlb;

    for (i = 0; i < ftlb_size; ++i) {
        tlb = &env->tlb->mmu.ls3a5k.tlb[i];
        tlb->EHINV = 1;
    }

    cpu_loongarch_tlb_flush(env);
}

void ls3a5k_helper_tlbclr(CPULOONGARCHState *env)
{
    int i;
    uint16_t asid;
    int vsize, fsize, index;
    int start = 0, end = -1;

    asid = env->CSR_ASID & 0x3ff;
    vsize = env->tlb->mmu.ls3a5k.vtlb_size;
    fsize = env->tlb->mmu.ls3a5k.ftlb_size;
    index = env->CSR_TLBIDX & CSR_TLBIDX_IDX;

    if (index < fsize) {
        /* FTLB. One line per operation */
        int set = index % 256;
        start = set * 8;
        end = start + 7;
    } else if (index < (fsize + vsize)) {
        /* VTLB. All entries */
        start = fsize;
        end = fsize + vsize - 1;
    } else {
        /* Ignore */
    }

    for (i = start; i <= end; i++) {
        ls3a5k_tlb_t *tlb;
        tlb = &env->tlb->mmu.ls3a5k.tlb[i];
        if (!tlb->G && tlb->ASID == asid) {
            tlb->EHINV = 1;
        }
    }

    cpu_loongarch_tlb_flush(env);
}

void ls3a5k_helper_tlbflush(CPULOONGARCHState *env)
{
    int i;
    int vsize, fsize, index;
    int start = 0, end = -1;

    vsize = env->tlb->mmu.ls3a5k.vtlb_size;
    fsize = env->tlb->mmu.ls3a5k.ftlb_size;
    index = env->CSR_TLBIDX & CSR_TLBIDX_IDX;

    if (index < fsize) {
        /* FTLB. One line per operation */
        int set = index % 256;
        start = set * 8;
        end = start + 7;
    } else if (index < (fsize + vsize)) {
        /* VTLB. All entries */
        start = fsize;
        end = fsize + vsize - 1;
    } else {
        /* Ignore */
    }

    for (i = start; i <= end; i++) {
        env->tlb->mmu.ls3a5k.tlb[i].EHINV = 1;
    }

    cpu_loongarch_tlb_flush(env);
}

void ls3a5k_helper_invtlb(CPULOONGARCHState *env, target_ulong addr,
                          target_ulong info, int op)
{
    uint32_t asid = info & 0x3ff;
    int i;

    switch (op) {
    case 0:
    case 1:
        for (i = 0; i < env->tlb->nb_tlb; i++) {
            env->tlb->mmu.ls3a5k.tlb[i].EHINV = 1;
        }
        break;
    case 4: {
        int i;
        for (i = 0; i < env->tlb->nb_tlb; i++) {
            struct ls3a5k_tlb_t *tlb = &env->tlb->mmu.ls3a5k.tlb[i];

            if (!tlb->G && tlb->ASID == asid) {
                tlb->EHINV = 1;
            }
        }
        break;
    }

    case 5: {
        int i;
        for (i = 0; i < env->tlb->nb_tlb; i++) {
            struct ls3a5k_tlb_t *tlb = &env->tlb->mmu.ls3a5k.tlb[i];
            uint64_t vpn = addr & 0xffffffffe000 & ~tlb->PageMask;

            if (!tlb->G && tlb->ASID == asid && vpn == tlb->VPN) {
                tlb->EHINV = 1;
            }
        }
        break;
    }
    case 6: {
        int i;
        for (i = 0; i < env->tlb->nb_tlb; i++) {
            struct ls3a5k_tlb_t *tlb = &env->tlb->mmu.ls3a5k.tlb[i];
            uint64_t vpn = addr & 0xffffffffe000 & ~tlb->PageMask;

            if ((tlb->G || tlb->ASID == asid) && vpn == tlb->VPN) {
                tlb->EHINV = 1;
            }
        }
        break;
    }
    default:
        helper_raise_exception(env, EXCP_RI);
    }

    cpu_loongarch_tlb_flush(env);
}

static void ls3a5k_invalidate_tlb_entry(CPULOONGARCHState *env,
                                        ls3a5k_tlb_t *tlb)
{
    LOONGARCHCPU *cpu = loongarch_env_get_cpu(env);
    CPUState *cs = CPU(cpu);
    target_ulong addr;
    target_ulong end;
    target_ulong mask;

    mask = tlb->PageMask; /* 000...000111...111 */

    if (tlb->V0) {
        addr = tlb->VPN & ~mask;  /* xxx...xxx[0]000..0000 */
        end = addr | (mask >> 1); /* xxx...xxx[0]111..1111 */
        while (addr < end) {
            tlb_flush_page(cs, addr);
            addr += TARGET_PAGE_SIZE;
        }
    }

    if (tlb->V1) {
        /* xxx...xxx[1]000..0000 */
        addr = (tlb->VPN & ~mask) | ((mask >> 1) + 1);
        end = addr | mask; /* xxx...xxx[1]111..1111 */
        while (addr - 1 < end) {
            tlb_flush_page(cs, addr);
            addr += TARGET_PAGE_SIZE;
        }
    }
}

void ls3a5k_invalidate_tlb(CPULOONGARCHState *env, int idx)
{
    ls3a5k_tlb_t *tlb;
    int asid = env->CSR_ASID & 0x3ff;
    tlb = &env->tlb->mmu.ls3a5k.tlb[idx];
    if (tlb->G == 0 && tlb->ASID != asid) {
        return;
    }
    ls3a5k_invalidate_tlb_entry(env, tlb);
}

void ls3a5k_helper_tlbwr(CPULOONGARCHState *env)
{
    int idx = env->CSR_TLBIDX & CSR_TLBIDX_IDX; /* [11:0] */

    /* Convert idx if in FTLB */
    if (idx < env->tlb->mmu.ls3a5k.ftlb_size) {
        /*
         *   0 3 6      0 1 2
         *   1 4 7  =>  3 4 5
         *   2 5 8      6 7 8
         */
        int set = idx % 256;
        int way = idx / 256;
        idx = set * 8 + way;
    }
    ls3a5k_invalidate_tlb(env, idx);
    ls3a5k_fill_tlb(env, idx, true);
}

void ls3a5k_helper_tlbfill(CPULOONGARCHState *env)
{
    uint64_t mask;
    uint64_t address;
    int idx;
    int set, ftlb_idx;

    uint64_t entryhi;
    uint32_t pagesize;

    if (env->CSR_TLBRERA & 0x1) {
        entryhi = env->CSR_TLBREHI & ~0x3f;
        pagesize = env->CSR_TLBREHI & 0x3f;
    } else {
        entryhi = env->CSR_TLBEHI;
        pagesize = (env->CSR_TLBIDX >> CSR_TLBIDX_PS_SHIFT) & 0x3f;
    }

    uint32_t ftlb_size = env->tlb->mmu.ls3a5k.ftlb_size;
    uint32_t vtlb_size = env->tlb->mmu.ls3a5k.vtlb_size;

    mask = ls3a5k_pagesize_to_mask(pagesize);

    if (mask == env->tlb->mmu.ls3a5k.ftlb_mask &&
        env->tlb->mmu.ls3a5k.ftlb_size > 0) {
        /* only write into FTLB */
        address = entryhi & 0xffffffffe000; /* [47:13] */

        /* choose one set ramdomly */
        set = cpu_loongarch_get_random_ls3a5k_tlb(0, 7);

        /* index in one set */
        ftlb_idx = (address >> 15) & 0xff; /* [0,255] */

        /* final idx */
        idx = ftlb_idx * 8 + set; /* max is 7 + 8 * 255 = 2047 */
    } else {
        /* only write into VTLB */
        int wired_nr = env->CSR_TLBWIRED & 0x3f;
        idx = cpu_loongarch_get_random_ls3a5k_tlb(ftlb_size + wired_nr,
                                                  ftlb_size + vtlb_size - 1);
    }

    ls3a5k_invalidate_tlb(env, idx);
    ls3a5k_fill_tlb(env, idx, false);
}

void ls3a5k_helper_tlbsrch(CPULOONGARCHState *env)
{
    uint64_t mask;
    uint64_t vpn;
    uint64_t tag;
    uint16_t asid;

    int ftlb_size = env->tlb->mmu.ls3a5k.ftlb_size;
    int vtlb_size = env->tlb->mmu.ls3a5k.vtlb_size;
    int i;
    int ftlb_idx; /* [0,255] 2^8 0xff */

    ls3a5k_tlb_t *tlb;

    asid = env->CSR_ASID & 0x3ff;

    /* search VTLB */
    for (i = ftlb_size; i < ftlb_size + vtlb_size; ++i) {
        tlb = &env->tlb->mmu.ls3a5k.tlb[i];
        mask = tlb->PageMask;

        vpn = env->CSR_TLBEHI & 0xffffffffe000 & ~mask;
        tag = tlb->VPN & ~mask;

        if ((tlb->G == 1 || tlb->ASID == asid) && vpn == tag &&
            tlb->EHINV != 1) {
            env->CSR_TLBIDX =
                (i & 0xfff) | ((tlb->PageSize & 0x3f) << CSR_TLBIDX_PS_SHIFT);
            goto _MATCH_OUT_;
        }
    }

    if (ftlb_size == 0) {
        goto _NO_MATCH_OUT_;
    }

    /* search FTLB */
    mask = env->tlb->mmu.ls3a5k.ftlb_mask;
    vpn = env->CSR_TLBEHI & 0xffffffffe000 & ~mask;

    ftlb_idx = (env->CSR_TLBEHI & 0xffffffffe000) >> 15; /* 16 KB */
    ftlb_idx = ftlb_idx & 0xff;                          /* [0,255] */

    for (i = 0; i < 8; ++i) {
        tlb = &env->tlb->mmu.ls3a5k.tlb[ftlb_idx * 8 + i];
        tag = tlb->VPN & ~mask;

        if ((tlb->G == 1 || tlb->ASID == asid) && vpn == tag &&
            tlb->EHINV != 1) {
            env->CSR_TLBIDX = ((i * 256 + ftlb_idx) & 0xfff) |
                              ((tlb->PageSize & 0x3f) << CSR_TLBIDX_PS_SHIFT);
            goto _MATCH_OUT_;
        }
    }

_NO_MATCH_OUT_:
    env->CSR_TLBIDX = 1 << CSR_TLBIDX_EHINV_SHIFT;
_MATCH_OUT_:
    return;
}

void ls3a5k_helper_tlbrd(CPULOONGARCHState *env)
{
    ls3a5k_tlb_t *tlb;
    int idx;
    uint16_t asid;

    idx = env->CSR_TLBIDX & CSR_TLBIDX_IDX;
    if (idx < env->tlb->mmu.ls3a5k.ftlb_size) {
        int set = idx % 256;
        int way = idx / 256;
        idx = set * 8 + way;
    }

    tlb = &env->tlb->mmu.ls3a5k.tlb[idx];

    asid = env->CSR_ASID & 0x3ff;

    if (asid != tlb->ASID) {
        cpu_loongarch_tlb_flush(env);
    }

    if (tlb->EHINV) {
        /* invalid TLB entry */
        env->CSR_TLBIDX = 1 << CSR_TLBIDX_EHINV_SHIFT;
        env->CSR_TLBEHI = 0;
        env->CSR_TLBELO0 = 0;
        env->CSR_TLBELO1 = 0;
    } else {
        /* valid TLB entry */
        env->CSR_TLBIDX = (env->CSR_TLBIDX & 0xfff) |
                          ((tlb->PageSize & 0x3f) << CSR_TLBIDX_PS_SHIFT);
        env->CSR_TLBEHI = tlb->VPN;
        env->CSR_TLBELO0 = (tlb->V0 << CSR_TLBLO0_V_SHIFT) |
                           (tlb->WE0 << CSR_TLBLO0_WE_SHIFT) |
                           (tlb->PLV0 << CSR_TLBLO0_PLV_SHIFT) |
                           (tlb->C0 << CSR_TLBLO0_CCA_SHIFT) |
                           (tlb->G << CSR_TLBLO0_GLOBAL_SHIFT) |
                           (tlb->PPN0 & 0xfffffffff000) |
                           ((uint64_t)tlb->RI0 << CSR_TLBLO0_RI_SHIFT) |
                           ((uint64_t)tlb->XI0 << CSR_TLBLO0_XI_SHIFT) |
                           ((uint64_t)tlb->RPLV0 << CSR_TLBLO0_RPLV_SHIFT);
        env->CSR_TLBELO1 = (tlb->V1 << CSR_TLBLO1_V_SHIFT) |
                           (tlb->WE1 << CSR_TLBLO1_WE_SHIFT) |
                           (tlb->PLV1 << CSR_TLBLO1_PLV_SHIFT) |
                           (tlb->C1 << CSR_TLBLO1_CCA_SHIFT) |
                           (tlb->G << CSR_TLBLO0_GLOBAL_SHIFT) |
                           (tlb->PPN1 & 0xfffffffff000) |
                           ((uint64_t)tlb->RI1 << CSR_TLBLO1_RI_SHIFT) |
                           ((uint64_t)tlb->XI1 << CSR_TLBLO1_XI_SHIFT) |
                           ((uint64_t)tlb->RPLV1 << CSR_TLBLO1_RPLV_SHIFT);
        env->CSR_ASID =
            (tlb->ASID << CSR_ASID_ASID_SHIFT) | (env->CSR_ASID & 0xff0000);
    }
}

void helper_tlbwr(CPULOONGARCHState *env)
{
    env->tlb->helper_tlbwr(env);
}

void helper_tlbfill(CPULOONGARCHState *env)
{
    env->tlb->helper_tlbfill(env);
}

void helper_tlbsrch(CPULOONGARCHState *env)
{
    env->tlb->helper_tlbsrch(env);
}

void helper_tlbrd(CPULOONGARCHState *env)
{
    env->tlb->helper_tlbrd(env);
}

void helper_tlbclr(CPULOONGARCHState *env)
{
    env->tlb->helper_tlbclr(env);
}

void helper_tlbflush(CPULOONGARCHState *env)
{
    env->tlb->helper_tlbflush(env);
}

void helper_invtlb(CPULOONGARCHState *env, target_ulong addr,
                   target_ulong info, target_ulong op)
{
    env->tlb->helper_invtlb(env, addr, info, op);
}

static void ls3a5k_mmu_init(CPULOONGARCHState *env, const loongarch_def_t *def)
{
    /* number of VTLB */
    env->tlb->nb_tlb = 64;
    env->tlb->mmu.ls3a5k.vtlb_size = 64;

    /* number of FTLB */
    env->tlb->nb_tlb += 2048;
    env->tlb->mmu.ls3a5k.ftlb_size = 2048;
    env->tlb->mmu.ls3a5k.ftlb_mask = (1 << 15) - 1; /* 16 KB */
    /*
     * page_size    |        ftlb_mask         | party field
     * ----------------------------------------------------------------
     *   4 KB = 12  | ( 1 << 13 ) - 1 = [12:0] |   [12]
     *  16 KB = 14  | ( 1 << 15 ) - 1 = [14:0] |   [14]
     *  64 KB = 16  | ( 1 << 17 ) - 1 = [16:0] |   [16]
     * 256 KB = 18  | ( 1 << 19 ) - 1 = [18:0] |   [18]
     *   1 MB = 20  | ( 1 << 21 ) - 1 = [20:0] |   [20]
     *   4 MB = 22  | ( 1 << 23 ) - 1 = [22:0] |   [22]
     *  16 MB = 24  | ( 1 << 25 ) - 1 = [24:0] |   [24]
     *  64 MB = 26  | ( 1 << 27 ) - 1 = [26:0] |   [26]
     * 256 MB = 28  | ( 1 << 29 ) - 1 = [28:0] |   [28]
     *   1 GB = 30  | ( 1 << 31 ) - 1 = [30:0] |   [30]
     * ----------------------------------------------------------------
     *  take party field index as @n. eg. For 16 KB, n = 14
     * ----------------------------------------------------------------
     *  tlb->VPN = TLBEHI & 0xffffffffe000[47:13] & ~mask = [47:n+1]
     *  tlb->PPN = TLBLO0 & 0xffffffffe000[47:13] & ~mask = [47:n+1]
     *  tlb->PPN = TLBLO1 & 0xffffffffe000[47:13] & ~mask = [47:n+1]
     * ----------------------------------------------------------------
     *  On mapping :
     *  >   vpn = address  & 0xffffffffe000[47:13] & ~mask = [47:n+1]
     *  >   tag = tlb->VPN & ~mask                         = [47:n+1]
     * ----------------------------------------------------------------
     * physical address = [47:n+1]  |  [n:0]
     * physical address = tlb->PPN0 | (address & mask)
     * physical address = tlb->PPN1 | (address & mask)
     */

    int i;
    for (i = 0; i < env->tlb->nb_tlb; i++) {
        env->tlb->mmu.ls3a5k.tlb[i].EHINV = 1;
    }

    /* TLB's helper functions */
    env->tlb->map_address = &ls3a5k_map_address;
    env->tlb->helper_tlbwr = ls3a5k_helper_tlbwr;
    env->tlb->helper_tlbfill = ls3a5k_helper_tlbfill;
    env->tlb->helper_tlbsrch = ls3a5k_helper_tlbsrch;
    env->tlb->helper_tlbrd = ls3a5k_helper_tlbrd;
    env->tlb->helper_tlbclr = ls3a5k_helper_tlbclr;
    env->tlb->helper_tlbflush = ls3a5k_helper_tlbflush;
    env->tlb->helper_invtlb = ls3a5k_helper_invtlb;
}

void mmu_init(CPULOONGARCHState *env, const loongarch_def_t *def)
{
    env->tlb = g_malloc0(sizeof(CPULOONGARCHTLBContext));

    switch (def->mmu_type) {
    case MMU_TYPE_LS3A5K:
        ls3a5k_mmu_init(env, def);
        break;
    default:
        cpu_abort(CPU(loongarch_env_get_cpu(env)), "MMU type not supported\n");
    }
}
#endif /* !CONFIG_USER_ONLY */
