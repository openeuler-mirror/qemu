/*
 * LOONGARCH emulation helpers for qemu.
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
#include "sysemu/kvm.h"
#include "qemu/crc32c.h"
#include <zlib.h>
#include "hw/irq.h"
#include "hw/core/cpu.h"
#include "instmap.h"

/* Exceptions processing helpers */

void helper_raise_exception_err(CPULOONGARCHState *env, uint32_t exception,
                                int error_code)
{
    do_raise_exception_err(env, exception, error_code, 0);
}

void helper_raise_exception(CPULOONGARCHState *env, uint32_t exception)
{
    do_raise_exception(env, exception, GETPC());
}

void helper_raise_exception_debug(CPULOONGARCHState *env)
{
    do_raise_exception(env, EXCP_DEBUG, 0);
}

static void raise_exception(CPULOONGARCHState *env, uint32_t exception)
{
    do_raise_exception(env, exception, 0);
}

#if defined(CONFIG_USER_ONLY)
#define HELPER_LD(name, insn, type)                                           \
    static inline type do_##name(CPULOONGARCHState *env, target_ulong addr,   \
                                 int mem_idx, uintptr_t retaddr)              \
    {                                                                         \
        return (type)cpu_##insn##_data_ra(env, addr, retaddr);                \
    }
#else

#define HF_SMAP_SHIFT 23 /* CR4.SMAP */
#define HF_SMAP_MASK (1 << HF_SMAP_SHIFT)
#define MMU_KNOSMAP_IDX 2
#define HF_CPL_SHIFT 0
#define HF_CPL_MASK (3 << HF_CPL_SHIFT)
#define AC_MASK 0x00040000
#define MMU_KSMAP_IDX 0
static inline int cpu_mmu_index_kernel(CPULOONGARCHState *env)
{
    return !(env->hflags & HF_SMAP_MASK)
               ? MMU_KNOSMAP_IDX
               : ((env->hflags & HF_CPL_MASK) < 3 && (env->hflags & AC_MASK))
                     ? MMU_KNOSMAP_IDX
                     : MMU_KSMAP_IDX;
}

#define cpu_ldl_kernel_ra(e, p, r)                                            \
    cpu_ldl_mmuidx_ra(e, p, cpu_mmu_index_kernel(e), r)

#define HELPER_LD(name, insn, type)                                           \
    static inline type do_##name(CPULOONGARCHState *env, target_ulong addr,   \
                                 int mem_idx, uintptr_t retaddr)              \
    {                                                                         \
    }
#endif

#if defined(CONFIG_USER_ONLY)
#define HELPER_ST(name, insn, type)                                           \
    static inline void do_##name(CPULOONGARCHState *env, target_ulong addr,   \
                                 type val, int mem_idx, uintptr_t retaddr)    \
    {                                                                         \
    }
#else
#define HELPER_ST(name, insn, type)                                           \
    static inline void do_##name(CPULOONGARCHState *env, target_ulong addr,   \
                                 type val, int mem_idx, uintptr_t retaddr)    \
    {                                                                         \
    }
#endif

static inline target_ulong bitswap(target_ulong v)
{
    v = ((v >> 1) & (target_ulong)0x5555555555555555ULL) |
        ((v & (target_ulong)0x5555555555555555ULL) << 1);
    v = ((v >> 2) & (target_ulong)0x3333333333333333ULL) |
        ((v & (target_ulong)0x3333333333333333ULL) << 2);
    v = ((v >> 4) & (target_ulong)0x0F0F0F0F0F0F0F0FULL) |
        ((v & (target_ulong)0x0F0F0F0F0F0F0F0FULL) << 4);
    return v;
}

target_ulong helper_dbitswap(target_ulong rt)
{
    return bitswap(rt);
}

target_ulong helper_bitswap(target_ulong rt)
{
    return (int32_t)bitswap(rt);
}

/* these crc32 functions are based on target/arm/helper-a64.c */
target_ulong helper_crc32(target_ulong val, target_ulong m, uint32_t sz)
{
    uint8_t buf[8];
    target_ulong mask = ((sz * 8) == 64) ? -1ULL : ((1ULL << (sz * 8)) - 1);

    m &= mask;
    stq_le_p(buf, m);
    return (int32_t)(crc32(val ^ 0xffffffff, buf, sz) ^ 0xffffffff);
}

target_ulong helper_crc32c(target_ulong val, target_ulong m, uint32_t sz)
{
    uint8_t buf[8];
    target_ulong mask = ((sz * 8) == 64) ? -1ULL : ((1ULL << (sz * 8)) - 1);
    m &= mask;
    stq_le_p(buf, m);
    return (int32_t)(crc32c(val, buf, sz) ^ 0xffffffff);
}

#ifndef CONFIG_USER_ONLY

#define HELPER_LD_ATOMIC(name, insn, almask)                                  \
    target_ulong helper_##name(CPULOONGARCHState *env, target_ulong arg,      \
                               int mem_idx)                                   \
    {                                                                         \
    }
#endif

#ifndef CONFIG_USER_ONLY
void helper_drdtime(CPULOONGARCHState *env, target_ulong rd, target_ulong rs)
{
    env->active_tc.gpr[rd] = cpu_loongarch_get_stable_counter(env);
    env->active_tc.gpr[rs] = env->CSR_TMID;
}
#endif

#ifndef CONFIG_USER_ONLY
static void debug_pre_ertn(CPULOONGARCHState *env)
{
    if (qemu_loglevel_mask(CPU_LOG_EXEC)) {
        qemu_log("ERTN: PC " TARGET_FMT_lx " ERA " TARGET_FMT_lx,
                 env->active_tc.PC, env->CSR_ERA);
        qemu_log("\n");
    }
}

static void debug_post_ertn(CPULOONGARCHState *env)
{
    if (qemu_loglevel_mask(CPU_LOG_EXEC)) {
        qemu_log("ERTN: PC " TARGET_FMT_lx " ERA " TARGET_FMT_lx,
                 env->active_tc.PC, env->CSR_ERA);
    }
}

static void set_pc(CPULOONGARCHState *env, target_ulong error_pc)
{
    env->active_tc.PC = error_pc & ~(target_ulong)1;
}

static inline void exception_return(CPULOONGARCHState *env)
{
    debug_pre_ertn(env);

    if (cpu_refill_state(env)) {
        env->CSR_CRMD &= (~0x7);
        env->CSR_CRMD |= (env->CSR_TLBRPRMD & 0x7);
        /* Clear Refill flag and set pc */
        env->CSR_TLBRERA &= (~0x1);
        set_pc(env, env->CSR_TLBRERA);
        if (qemu_loglevel_mask(CPU_LOG_INT)) {
            qemu_log("%s: TLBRERA 0x%lx\n", __func__, env->CSR_TLBRERA);
        }
    } else {
        env->CSR_CRMD &= (~0x7);
        env->CSR_CRMD |= (env->CSR_PRMD & 0x7);
        /* Clear Refill flag and set pc*/
        set_pc(env, env->CSR_ERA);
        if (qemu_loglevel_mask(CPU_LOG_INT)) {
            qemu_log("%s: ERA 0x%lx\n", __func__, env->CSR_ERA);
        }
    }

    compute_hflags(env);
    debug_post_ertn(env);
}

void helper_ertn(CPULOONGARCHState *env)
{
    exception_return(env);
    env->lladdr = 1;
}

#endif /* !CONFIG_USER_ONLY */

void helper_idle(CPULOONGARCHState *env)
{
    CPUState *cs = CPU(loongarch_env_get_cpu(env));

    cs->halted = 1;
    cpu_reset_interrupt(cs, CPU_INTERRUPT_WAKE);
    /*
     * Last instruction in the block, PC was updated before
     * - no need to recover PC and icount
     */
    raise_exception(env, EXCP_HLT);
}

#if !defined(CONFIG_USER_ONLY)

void loongarch_cpu_do_unaligned_access(CPUState *cs, vaddr addr,
                                       MMUAccessType access_type, int mmu_idx,
                                       uintptr_t retaddr)
{
    while (1) {
    }
}

#endif /* !CONFIG_USER_ONLY */

void helper_store_scr(CPULOONGARCHState *env, uint32_t n, target_ulong val)
{
    env->scr[n & 0x3] = val;
}

target_ulong helper_load_scr(CPULOONGARCHState *env, uint32_t n)
{
    return env->scr[n & 0x3];
}

/* loongarch assert op */
void helper_asrtle_d(CPULOONGARCHState *env, target_ulong rs, target_ulong rt)
{
    if (rs > rt) {
        do_raise_exception(env, EXCP_AdEL, GETPC());
    }
}

void helper_asrtgt_d(CPULOONGARCHState *env, target_ulong rs, target_ulong rt)
{
    if (rs <= rt) {
        do_raise_exception(env, EXCP_AdEL, GETPC());
    }
}

target_ulong helper_cto_w(CPULOONGARCHState *env, target_ulong a0)
{
    uint32_t v = (uint32_t)a0;
    int temp = 0;

    while ((v & 0x1) == 1) {
        temp++;
        v = v >> 1;
    }

    return (target_ulong)temp;
}

target_ulong helper_ctz_w(CPULOONGARCHState *env, target_ulong a0)
{
    uint32_t v = (uint32_t)a0;

    if (v == 0) {
        return 32;
    }

    int temp = 0;
    while ((v & 0x1) == 0) {
        temp++;
        v = v >> 1;
    }

    return (target_ulong)temp;
}

target_ulong helper_cto_d(CPULOONGARCHState *env, target_ulong a0)
{
    uint64_t v = a0;
    int temp = 0;

    while ((v & 0x1) == 1) {
        temp++;
        v = v >> 1;
    }

    return (target_ulong)temp;
}

target_ulong helper_ctz_d(CPULOONGARCHState *env, target_ulong a0)
{
    uint64_t v = a0;

    if (v == 0) {
        return 64;
    }

    int temp = 0;
    while ((v & 0x1) == 0) {
        temp++;
        v = v >> 1;
    }

    return (target_ulong)temp;
}

target_ulong helper_bitrev_w(CPULOONGARCHState *env, target_ulong a0)
{
    int32_t v = (int32_t)a0;
    const int SIZE = 32;
    uint8_t bytes[SIZE];

    int i;
    for (i = 0; i < SIZE; i++) {
        bytes[i] = v & 0x1;
        v = v >> 1;
    }
    /* v == 0 */
    for (i = 0; i < SIZE; i++) {
        v = v | ((uint32_t)bytes[i] << (SIZE - 1 - i));
    }

    return (target_ulong)(int32_t)v;
}

target_ulong helper_bitrev_d(CPULOONGARCHState *env, target_ulong a0)
{
    uint64_t v = a0;
    const int SIZE = 64;
    uint8_t bytes[SIZE];

    int i;
    for (i = 0; i < SIZE; i++) {
        bytes[i] = v & 0x1;
        v = v >> 1;
    }
    /* v == 0 */
    for (i = 0; i < SIZE; i++) {
        v = v | ((uint64_t)bytes[i] << (SIZE - 1 - i));
    }

    return (target_ulong)v;
}

void helper_memtrace_addr(CPULOONGARCHState *env, target_ulong address,
                          uint32_t op)
{
    qemu_log("[cpu %d asid 0x%lx pc 0x%lx] addr 0x%lx op",
             CPU(loongarch_env_get_cpu(env))->cpu_index, env->CSR_ASID,
             env->active_tc.PC, address);
    switch (op) {
    case OPC_LARCH_LDPTR_D:
        qemu_log("OPC_LARCH_LDPTR_D");
        break;
    case OPC_LARCH_LD_D:
        qemu_log("OPC_LARCH_LD_D");
        break;
    case OPC_LARCH_LDPTR_W:
        qemu_log("OPC_LARCH_LDPTR_W");
        break;
    case OPC_LARCH_LD_W:
        qemu_log("OPC_LARCH_LD_W");
        break;
    case OPC_LARCH_LD_H:
        qemu_log("OPC_LARCH_LD_H");
        break;
    case OPC_LARCH_LD_B:
        qemu_log("OPC_LARCH_LD_B");
        break;
    case OPC_LARCH_LD_WU:
        qemu_log("OPC_LARCH_LD_WU");
        break;
    case OPC_LARCH_LD_HU:
        qemu_log("OPC_LARCH_LD_HU");
        break;
    case OPC_LARCH_LD_BU:
        qemu_log("OPC_LARCH_LD_BU");
        break;
    case OPC_LARCH_STPTR_D:
        qemu_log("OPC_LARCH_STPTR_D");
        break;
    case OPC_LARCH_ST_D:
        qemu_log("OPC_LARCH_ST_D");
        break;
    case OPC_LARCH_STPTR_W:
        qemu_log("OPC_LARCH_STPTR_W");
        break;
    case OPC_LARCH_ST_W:
        qemu_log("OPC_LARCH_ST_W");
        break;
    case OPC_LARCH_ST_H:
        qemu_log("OPC_LARCH_ST_H");
        break;
    case OPC_LARCH_ST_B:
        qemu_log("OPC_LARCH_ST_B");
        break;
    case OPC_LARCH_FLD_S:
        qemu_log("OPC_LARCH_FLD_S");
        break;
    case OPC_LARCH_FLD_D:
        qemu_log("OPC_LARCH_FLD_D");
        break;
    case OPC_LARCH_FST_S:
        qemu_log("OPC_LARCH_FST_S");
        break;
    case OPC_LARCH_FST_D:
        qemu_log("OPC_LARCH_FST_D");
        break;
    case OPC_LARCH_FLDX_S:
        qemu_log("OPC_LARCH_FLDX_S");
        break;
    case OPC_LARCH_FLDGT_S:
        qemu_log("OPC_LARCH_FLDGT_S");
        break;
    case OPC_LARCH_FLDLE_S:
        qemu_log("OPC_LARCH_FLDLE_S");
        break;
    case OPC_LARCH_FSTX_S:
        qemu_log("OPC_LARCH_FSTX_S");
        break;
    case OPC_LARCH_FSTGT_S:
        qemu_log("OPC_LARCH_FSTGT_S");
        break;
    case OPC_LARCH_FSTLE_S:
        qemu_log("OPC_LARCH_FSTLE_S");
        break;
    case OPC_LARCH_FLDX_D:
        qemu_log("OPC_LARCH_FLDX_D");
        break;
    case OPC_LARCH_FLDGT_D:
        qemu_log("OPC_LARCH_FLDGT_D");
        break;
    case OPC_LARCH_FLDLE_D:
        qemu_log("OPC_LARCH_FLDLE_D");
        break;
    case OPC_LARCH_FSTX_D:
        qemu_log("OPC_LARCH_FSTX_D");
        break;
    case OPC_LARCH_FSTGT_D:
        qemu_log("OPC_LARCH_FSTGT_D");
        break;
    case OPC_LARCH_FSTLE_D:
        qemu_log("OPC_LARCH_FSTLE_D");
        break;
    case OPC_LARCH_LL_W:
        qemu_log("OPC_LARCH_LL_W");
        break;
    case OPC_LARCH_LL_D:
        qemu_log("OPC_LARCH_LL_D");
        break;
    default:
        qemu_log("0x%x", op);
    }
}

void helper_memtrace_val(CPULOONGARCHState *env, target_ulong val)
{
    qemu_log("val 0x%lx\n", val);
}
