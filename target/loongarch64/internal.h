/*
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

#ifndef LOONGARCH_INTERNAL_H
#define LOONGARCH_INTERNAL_H

#include "cpu-csr.h"

/*
 * MMU types, the first four entries have the same layout as the
 * CP0C0_MT field.
 */
enum loongarch_mmu_types {
    MMU_TYPE_NONE,
    MMU_TYPE_LS3A5K, /* LISA CSR */
};

struct loongarch_def_t {
    const char *name;
    int32_t CSR_PRid;
    int32_t FCSR0;
    int32_t FCSR0_rw_bitmask;
    int32_t PABITS;
    CPU_LOONGARCH_CSR
    uint64_t insn_flags;
    enum loongarch_mmu_types mmu_type;
    int cpu_cfg[64];
};

/* loongarch 3a5000 TLB entry */
struct ls3a5k_tlb_t {
    target_ulong VPN;
    uint64_t PageMask; /* CSR_TLBIDX[29:24] */
    uint32_t PageSize;
    uint16_t ASID;
    unsigned int G:1;    /* CSR_TLBLO[6] */

    unsigned int C0:3;   /* CSR_TLBLO[5:4] */
    unsigned int C1:3;

    unsigned int V0:1;   /* CSR_TLBLO[0] */
    unsigned int V1:1;

    unsigned int WE0:1;  /* CSR_TLBLO[1] */
    unsigned int WE1:1;

    unsigned int XI0:1;  /* CSR_TLBLO[62] */
    unsigned int XI1:1;

    unsigned int RI0:1;  /* CSR_TLBLO[61] */
    unsigned int RI1:1;

    unsigned int EHINV:1;/* CSR_TLBIDX[31] */

    unsigned int PLV0:2; /* CSR_TLBLO[3:2] */
    unsigned int PLV1:2;

    unsigned int RPLV0:1;
    unsigned int RPLV1:1; /* CSR_TLBLO[63] */

    uint64_t PPN0; /* CSR_TLBLO[47:12] */
    uint64_t PPN1; /* CSR_TLBLO[47:12] */
};
typedef struct ls3a5k_tlb_t ls3a5k_tlb_t;

struct CPULOONGARCHTLBContext {
    uint32_t nb_tlb;
    uint32_t tlb_in_use;
    int (*map_address)(struct CPULOONGARCHState *env, hwaddr *physical,
                       int *prot, target_ulong address, int rw,
                       int access_type);
    void (*helper_tlbwr)(struct CPULOONGARCHState *env);
    void (*helper_tlbfill)(struct CPULOONGARCHState *env);
    void (*helper_tlbsrch)(struct CPULOONGARCHState *env);
    void (*helper_tlbrd)(struct CPULOONGARCHState *env);
    void (*helper_tlbclr)(struct CPULOONGARCHState *env);
    void (*helper_tlbflush)(struct CPULOONGARCHState *env);
    void (*helper_invtlb)(struct CPULOONGARCHState *env, target_ulong addr,
                          target_ulong info, int op);
    union
    {
        struct {
            uint64_t ftlb_mask;
            uint32_t ftlb_size;          /* at most : 8 * 256 = 2048 */
            uint32_t vtlb_size;          /* at most : 64 */
            ls3a5k_tlb_t tlb[2048 + 64]; /* at most : 2048 FTLB + 64 VTLB */
        } ls3a5k;
    } mmu;
};

enum {
    TLBRET_PE = -7,
    TLBRET_XI = -6,
    TLBRET_RI = -5,
    TLBRET_DIRTY = -4,
    TLBRET_INVALID = -3,
    TLBRET_NOMATCH = -2,
    TLBRET_BADADDR = -1,
    TLBRET_MATCH = 0
};

extern unsigned int ieee_rm[];

static inline void restore_rounding_mode(CPULOONGARCHState *env)
{
    set_float_rounding_mode(ieee_rm[(env->active_fpu.fcsr0 >> FCSR0_RM) & 0x3],
                            &env->active_fpu.fp_status);
}

static inline void restore_flush_mode(CPULOONGARCHState *env)
{
    set_flush_to_zero(0, &env->active_fpu.fp_status);
}

static inline void restore_fp_status(CPULOONGARCHState *env)
{
    restore_rounding_mode(env);
    restore_flush_mode(env);
}

static inline void compute_hflags(CPULOONGARCHState *env)
{
    env->hflags &= ~(LARCH_HFLAG_64 | LARCH_HFLAG_FPU | LARCH_HFLAG_KSU |
                     LARCH_HFLAG_AWRAP | LARCH_HFLAG_LSX | LARCH_HFLAG_LASX);

    env->hflags |= (env->CSR_CRMD & CSR_CRMD_PLV);
    env->hflags |= LARCH_HFLAG_64;

    if (env->CSR_EUEN & CSR_EUEN_FPEN) {
        env->hflags |= LARCH_HFLAG_FPU;
    }
    if (env->CSR_EUEN & CSR_EUEN_LSXEN) {
        env->hflags |= LARCH_HFLAG_LSX;
    }
    if (env->CSR_EUEN & CSR_EUEN_LASXEN) {
        env->hflags |= LARCH_HFLAG_LASX;
    }
    if (env->CSR_EUEN & CSR_EUEN_LBTEN) {
        env->hflags |= LARCH_HFLAG_LBT;
    }
}

/* Check if there is pending and not masked out interrupt */
static inline bool cpu_loongarch_hw_interrupts_pending(CPULOONGARCHState *env)
{
    int32_t pending;
    int32_t status;
    bool r;

    pending = env->CSR_ESTAT & CSR_ESTAT_IPMASK;
    status = env->CSR_ECFG & CSR_ECFG_IPMASK;

    /*
     * Configured with compatibility or VInt (Vectored Interrupts)
     * treats the pending lines as individual interrupt lines, the status
     * lines are individual masks.
     */
    r = (pending & status) != 0;

    return r;
}

/* stabletimer.c */
uint32_t cpu_loongarch_get_random_ls3a5k_tlb(uint32_t low, uint32_t high);
uint64_t cpu_loongarch_get_stable_counter(CPULOONGARCHState *env);
uint64_t cpu_loongarch_get_stable_timer_ticks(CPULOONGARCHState *env);
void cpu_loongarch_store_stable_timer_config(CPULOONGARCHState *env,
                                             uint64_t value);
int loongarch_cpu_write_elf64_note(WriteCoreDumpFunction f, CPUState *cpu,
                                   int cpuid, void *opaque);

void loongarch_cpu_dump_state(CPUState *cpu, FILE *f, int flags);

/* TODO QOM'ify CPU reset and remove */
void cpu_state_reset(CPULOONGARCHState *s);
void cpu_loongarch_realize_env(CPULOONGARCHState *env);

uint64_t read_fcc(CPULOONGARCHState *env);
void write_fcc(CPULOONGARCHState *env, uint64_t val);

int loongarch_cpu_gdb_read_register(CPUState *cs, GByteArray *mem_buf, int n);
int loongarch_cpu_gdb_write_register(CPUState *cpu, uint8_t *buf, int reg);

#ifdef CONFIG_TCG
#include "fpu_helper.h"
#endif

#ifndef CONFIG_USER_ONLY
extern const struct VMStateDescription vmstate_loongarch_cpu;
hwaddr loongarch_cpu_get_phys_page_debug(CPUState *cpu, vaddr addr);
#endif

#endif
