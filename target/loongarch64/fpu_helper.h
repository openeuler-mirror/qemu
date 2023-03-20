/*
 * loongarch internal definitions and helpers
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

#ifndef LOONGARCH_FPU_H
#define LOONGARCH_FPU_H

#include "cpu-csr.h"

extern const struct loongarch_def_t loongarch_defs[];
extern const int loongarch_defs_number;

enum CPULSXDataFormat { DF_BYTE = 0, DF_HALF, DF_WORD, DF_DOUBLE, DF_QUAD };

void loongarch_cpu_do_interrupt(CPUState *cpu);
bool loongarch_cpu_exec_interrupt(CPUState *cpu, int int_req);
void loongarch_cpu_do_unaligned_access(CPUState *cpu, vaddr addr,
                                       MMUAccessType access_type, int mmu_idx,
                                       uintptr_t retaddr) QEMU_NORETURN;

#if !defined(CONFIG_USER_ONLY)

typedef struct r4k_tlb_t r4k_tlb_t;
struct r4k_tlb_t {
    target_ulong VPN;
    uint32_t PageMask;
    uint16_t ASID;
    unsigned int G:1;
    unsigned int C0:3;
    unsigned int C1:3;
    unsigned int V0:1;
    unsigned int V1:1;
    unsigned int D0:1;
    unsigned int D1:1;
    unsigned int XI0:1;
    unsigned int XI1:1;
    unsigned int RI0:1;
    unsigned int RI1:1;
    unsigned int EHINV:1;
    uint64_t PPN[2];
};

int no_mmu_map_address(CPULOONGARCHState *env, hwaddr *physical, int *prot,
                       target_ulong address, int rw, int access_type);
int fixed_mmu_map_address(CPULOONGARCHState *env, hwaddr *physical, int *prot,
                          target_ulong address, int rw, int access_type);
int r4k_map_address(CPULOONGARCHState *env, hwaddr *physical, int *prot,
                    target_ulong address, int rw, int access_type);

/* loongarch 3a5000 tlb helper function : lisa csr */
int ls3a5k_map_address(CPULOONGARCHState *env, hwaddr *physical, int *prot,
                       target_ulong address, int rw, int access_type);
void ls3a5k_helper_tlbwr(CPULOONGARCHState *env);
void ls3a5k_helper_tlbfill(CPULOONGARCHState *env);
void ls3a5k_helper_tlbsrch(CPULOONGARCHState *env);
void ls3a5k_helper_tlbrd(CPULOONGARCHState *env);
void ls3a5k_helper_tlbclr(CPULOONGARCHState *env);
void ls3a5k_helper_tlbflush(CPULOONGARCHState *env);
void ls3a5k_invalidate_tlb(CPULOONGARCHState *env, int idx);
void ls3a5k_helper_invtlb(CPULOONGARCHState *env, target_ulong addr,
                          target_ulong info, int op);
void ls3a5k_flush_vtlb(CPULOONGARCHState *env);
void ls3a5k_flush_ftlb(CPULOONGARCHState *env);
hwaddr cpu_loongarch_translate_address(CPULOONGARCHState *env,
                                       target_ulong address, int rw);
#endif

#define cpu_signal_handler cpu_loongarch_signal_handler

static inline bool cpu_loongarch_hw_interrupts_enabled(CPULOONGARCHState *env)
{
    bool ret = 0;

    ret = env->CSR_CRMD & (1 << CSR_CRMD_IE_SHIFT);

    return ret;
}

void loongarch_tcg_init(void);

/* helper.c */
bool loongarch_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                            MMUAccessType access_type, int mmu_idx, bool probe,
                            uintptr_t retaddr);

/* op_helper.c */
uint32_t float_class_s(uint32_t arg, float_status *fst);
uint64_t float_class_d(uint64_t arg, float_status *fst);

int ieee_ex_to_loongarch(int xcpt);
void update_pagemask(CPULOONGARCHState *env, target_ulong arg1,
                     int32_t *pagemask);

void cpu_loongarch_tlb_flush(CPULOONGARCHState *env);
void sync_c0_status(CPULOONGARCHState *env, CPULOONGARCHState *cpu, int tc);

void QEMU_NORETURN do_raise_exception_err(CPULOONGARCHState *env,
                                          uint32_t exception, int error_code,
                                          uintptr_t pc);
int loongarch_read_qxfer(CPUState *cs, const char *annex, uint8_t *read_buf,
                         unsigned long offset, unsigned long len);
int loongarch_write_qxfer(CPUState *cs, const char *annex,
                          const uint8_t *write_buf, unsigned long offset,
                          unsigned long len);

static inline void QEMU_NORETURN do_raise_exception(CPULOONGARCHState *env,
                                                    uint32_t exception,
                                                    uintptr_t pc)
{
    do_raise_exception_err(env, exception, 0, pc);
}
#endif
