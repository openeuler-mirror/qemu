/*
 * LOONGARCH emulation for QEMU - main translation routines
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
#include "cpu.h"
#include "internal.h"
#include "disas/disas.h"
#include "exec/exec-all.h"
#include "tcg/tcg-op.h"
#include "exec/cpu_ldst.h"
#include "hw/loongarch/cpudevs.h"

#include "exec/helper-proto.h"
#include "exec/helper-gen.h"
#include "semihosting/semihost.h"

#include "trace-tcg.h"
#include "exec/translator.h"
#include "exec/log.h"

#include "instmap.h"

#define LARCH_DEBUG_DISAS 0

/* Values for the fmt field in FP instructions */
enum {
    /* 0 - 15 are reserved */
    FMT_S = 16, /* single fp */
    FMT_D = 17, /* double fp */
};

/* global register indices */
static TCGv cpu_gpr[32], cpu_PC;
static TCGv btarget, bcond;
static TCGv cpu_lladdr, cpu_llval;
static TCGv_i32 hflags;
static TCGv_i32 fpu_fcsr0;
static TCGv_i64 fpu_f64[32];

#include "exec/gen-icount.h"

#define gen_helper_0e0i(name, arg)                                            \
    do {                                                                      \
        TCGv_i32 helper_tmp = tcg_const_i32(arg);                             \
        gen_helper_##name(cpu_env, helper_tmp);                               \
        tcg_temp_free_i32(helper_tmp);                                        \
    } while (0)

#define gen_helper_0e1i(name, arg1, arg2)                                     \
    do {                                                                      \
        TCGv_i32 helper_tmp = tcg_const_i32(arg2);                            \
        gen_helper_##name(cpu_env, arg1, helper_tmp);                         \
        tcg_temp_free_i32(helper_tmp);                                        \
    } while (0)

#define gen_helper_1e0i(name, ret, arg1)                                      \
    do {                                                                      \
        TCGv_i32 helper_tmp = tcg_const_i32(arg1);                            \
        gen_helper_##name(ret, cpu_env, helper_tmp);                          \
        tcg_temp_free_i32(helper_tmp);                                        \
    } while (0)

#define gen_helper_1e1i(name, ret, arg1, arg2)                                \
    do {                                                                      \
        TCGv_i32 helper_tmp = tcg_const_i32(arg2);                            \
        gen_helper_##name(ret, cpu_env, arg1, helper_tmp);                    \
        tcg_temp_free_i32(helper_tmp);                                        \
    } while (0)

#define gen_helper_0e2i(name, arg1, arg2, arg3)                               \
    do {                                                                      \
        TCGv_i32 helper_tmp = tcg_const_i32(arg3);                            \
        gen_helper_##name(cpu_env, arg1, arg2, helper_tmp);                   \
        tcg_temp_free_i32(helper_tmp);                                        \
    } while (0)

#define gen_helper_1e2i(name, ret, arg1, arg2, arg3)                          \
    do {                                                                      \
        TCGv_i32 helper_tmp = tcg_const_i32(arg3);                            \
        gen_helper_##name(ret, cpu_env, arg1, arg2, helper_tmp);              \
        tcg_temp_free_i32(helper_tmp);                                        \
    } while (0)

#define gen_helper_0e3i(name, arg1, arg2, arg3, arg4)                         \
    do {                                                                      \
        TCGv_i32 helper_tmp = tcg_const_i32(arg4);                            \
        gen_helper_##name(cpu_env, arg1, arg2, arg3, helper_tmp);             \
        tcg_temp_free_i32(helper_tmp);                                        \
    } while (0)

typedef struct DisasContext {
    DisasContextBase base;
    target_ulong saved_pc;
    target_ulong page_start;
    uint32_t opcode;
    uint64_t insn_flags;
    /* Routine used to access memory */
    int mem_idx;
    MemOp default_tcg_memop_mask;
    uint32_t hflags, saved_hflags;
    target_ulong btarget;
} DisasContext;

#define DISAS_STOP DISAS_TARGET_0
#define DISAS_EXIT DISAS_TARGET_1

#define LOG_DISAS(...)                                                        \
    do {                                                                      \
        if (LARCH_DEBUG_DISAS) {                                              \
            qemu_log_mask(CPU_LOG_TB_IN_ASM, ##__VA_ARGS__);                  \
        }                                                                     \
    } while (0)

#define LARCH_INVAL(op)                                                       \
    do {                                                                      \
        if (LARCH_DEBUG_DISAS) {                                              \
            qemu_log_mask(CPU_LOG_TB_IN_ASM,                                  \
                          TARGET_FMT_lx ": %08x Invalid %s %03x %03x %03x\n", \
                          ctx->base.pc_next, ctx->opcode, op,                 \
                          ctx->opcode >> 26, ctx->opcode & 0x3F,              \
                          ((ctx->opcode >> 16) & 0x1F));                      \
        }                                                                     \
    } while (0)

/* General purpose registers moves. */
static inline void gen_load_gpr(TCGv t, int reg)
{
    if (reg == 0) {
        tcg_gen_movi_tl(t, 0);
    } else {
        tcg_gen_mov_tl(t, cpu_gpr[reg]);
    }
}

static inline void gen_store_gpr(TCGv t, int reg)
{
    if (reg != 0) {
        tcg_gen_mov_tl(cpu_gpr[reg], t);
    }
}

/* Moves to/from shadow registers. */
/* Tests */
static inline void gen_save_pc(target_ulong pc)
{
    tcg_gen_movi_tl(cpu_PC, pc);
}

static inline void save_cpu_state(DisasContext *ctx, int do_save_pc)
{
    LOG_DISAS("hflags %08x saved %08x\n", ctx->hflags, ctx->saved_hflags);
    if (do_save_pc && ctx->base.pc_next != ctx->saved_pc) {
        gen_save_pc(ctx->base.pc_next);
        ctx->saved_pc = ctx->base.pc_next;
    }
    if (ctx->hflags != ctx->saved_hflags) {
        tcg_gen_movi_i32(hflags, ctx->hflags);
        ctx->saved_hflags = ctx->hflags;
        switch (ctx->hflags & LARCH_HFLAG_BMASK) {
        case LARCH_HFLAG_BR:
            break;
        case LARCH_HFLAG_BC:
        case LARCH_HFLAG_B:
            tcg_gen_movi_tl(btarget, ctx->btarget);
            break;
        }
    }
}

static inline void restore_cpu_state(CPULOONGARCHState *env, DisasContext *ctx)
{
    ctx->saved_hflags = ctx->hflags;
    switch (ctx->hflags & LARCH_HFLAG_BMASK) {
    case LARCH_HFLAG_BR:
        break;
    case LARCH_HFLAG_BC:
    case LARCH_HFLAG_B:
        ctx->btarget = env->btarget;
        break;
    }
}

static inline void generate_exception_err(DisasContext *ctx, int excp, int err)
{
    TCGv_i32 texcp = tcg_const_i32(excp);
    TCGv_i32 terr = tcg_const_i32(err);
    save_cpu_state(ctx, 1);
    gen_helper_raise_exception_err(cpu_env, texcp, terr);
    tcg_temp_free_i32(terr);
    tcg_temp_free_i32(texcp);
    ctx->base.is_jmp = DISAS_NORETURN;
}

static inline void generate_exception_end(DisasContext *ctx, int excp)
{
    generate_exception_err(ctx, excp, 0);
}

/* Floating point register moves. */
static void gen_load_fpr32(DisasContext *ctx, TCGv_i32 t, int reg)
{
    tcg_gen_extrl_i64_i32(t, fpu_f64[reg]);
}

static void gen_store_fpr32(DisasContext *ctx, TCGv_i32 t, int reg)
{
    TCGv_i64 t64;
    t64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(t64, t);
    tcg_gen_deposit_i64(fpu_f64[reg], fpu_f64[reg], t64, 0, 32);
    tcg_temp_free_i64(t64);
}

static void gen_load_fpr32h(DisasContext *ctx, TCGv_i32 t, int reg)
{
    tcg_gen_extrh_i64_i32(t, fpu_f64[reg]);
}

static void gen_store_fpr32h(DisasContext *ctx, TCGv_i32 t, int reg)
{
    TCGv_i64 t64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(t64, t);
    tcg_gen_deposit_i64(fpu_f64[reg], fpu_f64[reg], t64, 32, 32);
    tcg_temp_free_i64(t64);
}

static void gen_load_fpr64(DisasContext *ctx, TCGv_i64 t, int reg)
{
    tcg_gen_mov_i64(t, fpu_f64[reg]);
}

static void gen_store_fpr64(DisasContext *ctx, TCGv_i64 t, int reg)
{
    tcg_gen_mov_i64(fpu_f64[reg], t);
}

static inline int get_fp_bit(int cc)
{
    if (cc) {
        return 24 + cc;
    } else {
        return 23;
    }
}

/* Addresses computation */
static inline void gen_op_addr_add(DisasContext *ctx, TCGv ret, TCGv arg0,
                                   TCGv arg1)
{
    tcg_gen_add_tl(ret, arg0, arg1);

    if (ctx->hflags & LARCH_HFLAG_AWRAP) {
        tcg_gen_ext32s_i64(ret, ret);
    }
}

static inline void gen_op_addr_addi(DisasContext *ctx, TCGv ret, TCGv base,
                                    target_long ofs)
{
    tcg_gen_addi_tl(ret, base, ofs);

    if (ctx->hflags & LARCH_HFLAG_AWRAP) {
        tcg_gen_ext32s_i64(ret, ret);
    }
}

/* Sign-extract the low 32-bits to a target_long.  */
static inline void gen_move_low32(TCGv ret, TCGv_i64 arg)
{
    tcg_gen_ext32s_i64(ret, arg);
}

/* Sign-extract the high 32-bits to a target_long.  */
static inline void gen_move_high32(TCGv ret, TCGv_i64 arg)
{
    tcg_gen_sari_i64(ret, arg, 32);
}

static inline void check_cp1_enabled(DisasContext *ctx)
{
#ifndef CONFIG_USER_ONLY
    if (unlikely(!(ctx->hflags & LARCH_HFLAG_FPU))) {
        generate_exception_err(ctx, EXCP_FPDIS, 1);
    }
#endif
}

static inline void check_lsx_enabled(DisasContext *ctx)
{
#ifndef CONFIG_USER_ONLY
    if (unlikely(!(ctx->hflags & LARCH_HFLAG_LSX))) {
        generate_exception_err(ctx, EXCP_LSXDIS, 1);
    }
#endif
}

static inline void check_lasx_enabled(DisasContext *ctx)
{
#ifndef CONFIG_USER_ONLY
    if (unlikely(!(ctx->hflags & LARCH_HFLAG_LASX))) {
        generate_exception_err(ctx, EXCP_LASXDIS, 1);
    }
#endif
}

static inline void check_lbt_enabled(DisasContext *ctx)
{
#ifndef CONFIG_USER_ONLY
    if (unlikely(!(ctx->hflags & LARCH_HFLAG_LBT))) {
        generate_exception_err(ctx, EXCP_BTDIS, 1);
    }
#endif
}

/*
 * This code generates a "reserved instruction" exception if the
 * CPU does not support the instruction set corresponding to flags.
 */
static inline void check_insn(DisasContext *ctx, uint64_t flags)
{
    if (unlikely(!(ctx->insn_flags & flags))) {
        generate_exception_end(ctx, EXCP_RI);
    }
}

/*
 * This code generates a "reserved instruction" exception if the
 * CPU has corresponding flag set which indicates that the instruction
 * has been removed.
 */
static inline void check_insn_opc_removed(DisasContext *ctx, uint64_t flags)
{
    if (unlikely(ctx->insn_flags & flags)) {
        generate_exception_end(ctx, EXCP_RI);
    }
}

/*
 * The Linux kernel traps certain reserved instruction exceptions to
 * emulate the corresponding instructions. QEMU is the kernel in user
 * mode, so those traps are emulated by accepting the instructions.
 *
 * A reserved instruction exception is generated for flagged CPUs if
 * QEMU runs in system mode.
 */
static inline void check_insn_opc_user_only(DisasContext *ctx, uint64_t flags)
{
#ifndef CONFIG_USER_ONLY
    check_insn_opc_removed(ctx, flags);
#endif
}

/*
 * This code generates a "reserved instruction" exception if 64-bit
 * instructions are not enabled.
 */
static inline void check_larch_64(DisasContext *ctx)
{
    if (unlikely(!(ctx->hflags & LARCH_HFLAG_64))) {
        generate_exception_end(ctx, EXCP_RI);
    }
}

/*
 * Define small wrappers for gen_load_fpr* so that we have a uniform
 * calling interface for 32 and 64-bit FPRs.  No sense in changing
 * all callers for gen_load_fpr32 when we need the CTX parameter for
 * this one use.
 */
#define gen_ldcmp_fpr32(ctx, x, y) gen_load_fpr32(ctx, x, y)
#define gen_ldcmp_fpr64(ctx, x, y) gen_load_fpr64(ctx, x, y)
#define FCOP_CONDNS(fmt, ifmt, bits, STORE)                                   \
    static inline void gen_fcmp_##fmt(DisasContext *ctx, int n, int ft,       \
                                      int fs, int cd)                         \
    {                                                                         \
        TCGv_i##bits fp0 = tcg_temp_new_i##bits();                            \
        TCGv_i##bits fp1 = tcg_temp_new_i##bits();                            \
        TCGv_i32 fcc = tcg_const_i32(cd);                                     \
        check_cp1_enabled(ctx);                                               \
        gen_ldcmp_fpr##bits(ctx, fp0, fs);                                    \
        gen_ldcmp_fpr##bits(ctx, fp1, ft);                                    \
        switch (n) {                                                          \
        case 0:                                                               \
            gen_helper_cmp_##fmt##_af(fp0, cpu_env, fp0, fp1);                \
            break;                                                            \
        case 1:                                                               \
            gen_helper_cmp_##fmt##_saf(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 2:                                                               \
            gen_helper_cmp_##fmt##_lt(fp0, cpu_env, fp0, fp1);                \
            break;                                                            \
        case 3:                                                               \
            gen_helper_cmp_##fmt##_slt(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 4:                                                               \
            gen_helper_cmp_##fmt##_eq(fp0, cpu_env, fp0, fp1);                \
            break;                                                            \
        case 5:                                                               \
            gen_helper_cmp_##fmt##_seq(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 6:                                                               \
            gen_helper_cmp_##fmt##_le(fp0, cpu_env, fp0, fp1);                \
            break;                                                            \
        case 7:                                                               \
            gen_helper_cmp_##fmt##_sle(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 8:                                                               \
            gen_helper_cmp_##fmt##_un(fp0, cpu_env, fp0, fp1);                \
            break;                                                            \
        case 9:                                                               \
            gen_helper_cmp_##fmt##_sun(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 10:                                                              \
            gen_helper_cmp_##fmt##_ult(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 11:                                                              \
            gen_helper_cmp_##fmt##_sult(fp0, cpu_env, fp0, fp1);              \
            break;                                                            \
        case 12:                                                              \
            gen_helper_cmp_##fmt##_ueq(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 13:                                                              \
            gen_helper_cmp_##fmt##_sueq(fp0, cpu_env, fp0, fp1);              \
            break;                                                            \
        case 14:                                                              \
            gen_helper_cmp_##fmt##_ule(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 15:                                                              \
            gen_helper_cmp_##fmt##_sule(fp0, cpu_env, fp0, fp1);              \
            break;                                                            \
        case 16:                                                              \
            gen_helper_cmp_##fmt##_ne(fp0, cpu_env, fp0, fp1);                \
            break;                                                            \
        case 17:                                                              \
            gen_helper_cmp_##fmt##_sne(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 20:                                                              \
            gen_helper_cmp_##fmt##_or(fp0, cpu_env, fp0, fp1);                \
            break;                                                            \
        case 21:                                                              \
            gen_helper_cmp_##fmt##_sor(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 24:                                                              \
            gen_helper_cmp_##fmt##_une(fp0, cpu_env, fp0, fp1);               \
            break;                                                            \
        case 25:                                                              \
            gen_helper_cmp_##fmt##_sune(fp0, cpu_env, fp0, fp1);              \
            break;                                                            \
        default:                                                              \
            abort();                                                          \
        }                                                                     \
        STORE;                                                                \
        tcg_temp_free_i##bits(fp0);                                           \
        tcg_temp_free_i##bits(fp1);                                           \
        tcg_temp_free_i32(fcc);                                               \
    }

FCOP_CONDNS(d, FMT_D, 64, gen_helper_movreg2cf_i64(cpu_env, fcc, fp0))
FCOP_CONDNS(s, FMT_S, 32, gen_helper_movreg2cf_i32(cpu_env, fcc, fp0))
#undef FCOP_CONDNS
#undef gen_ldcmp_fpr32
#undef gen_ldcmp_fpr64

/* load/store instructions. */
#ifdef CONFIG_USER_ONLY
#define OP_LD_ATOMIC(insn, fname)                                             \
    static inline void op_ld_##insn(TCGv ret, TCGv arg1, int mem_idx,         \
                                    DisasContext *ctx)                        \
    {                                                                         \
        TCGv t0 = tcg_temp_new();                                             \
        tcg_gen_mov_tl(t0, arg1);                                             \
        tcg_gen_qemu_##fname(ret, arg1, ctx->mem_idx);                        \
        tcg_gen_st_tl(t0, cpu_env, offsetof(CPULOONGARCHState, lladdr));      \
        tcg_gen_st_tl(ret, cpu_env, offsetof(CPULOONGARCHState, llval));      \
        tcg_temp_free(t0);                                                    \
    }
#else
#define OP_LD_ATOMIC(insn, fname)                                             \
    static inline void op_ld_##insn(TCGv ret, TCGv arg1, int mem_idx,         \
                                    DisasContext *ctx)                        \
    {                                                                         \
        gen_helper_1e1i(insn, ret, arg1, mem_idx);                            \
    }
#endif

static void gen_base_offset_addr(DisasContext *ctx, TCGv addr, int base,
                                 int offset)
{
    if (base == 0) {
        tcg_gen_movi_tl(addr, offset);
    } else if (offset == 0) {
        gen_load_gpr(addr, base);
    } else {
        tcg_gen_movi_tl(addr, offset);
        gen_op_addr_add(ctx, addr, cpu_gpr[base], addr);
    }
}

/* Load */
static void gen_ld(DisasContext *ctx, uint32_t opc, int rt, int base,
                   int offset)
{
    TCGv t0;
    int mem_idx = ctx->mem_idx;

    t0 = tcg_temp_new();
    gen_base_offset_addr(ctx, t0, base, offset);

    switch (opc) {
    case OPC_LARCH_LD_WU:
        tcg_gen_qemu_ld_tl(t0, t0, mem_idx,
                           MO_TEUL | ctx->default_tcg_memop_mask);
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_LDPTR_D:
    case OPC_LARCH_LD_D:
        tcg_gen_qemu_ld_tl(t0, t0, mem_idx,
                           MO_TEQ | ctx->default_tcg_memop_mask);
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_LL_D:
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_LDPTR_W:
    case OPC_LARCH_LD_W:
        tcg_gen_qemu_ld_tl(t0, t0, mem_idx,
                           MO_TESL | ctx->default_tcg_memop_mask);
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_LD_H:
        tcg_gen_qemu_ld_tl(t0, t0, mem_idx,
                           MO_TESW | ctx->default_tcg_memop_mask);
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_LD_HU:
        tcg_gen_qemu_ld_tl(t0, t0, mem_idx,
                           MO_TEUW | ctx->default_tcg_memop_mask);
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_LD_B:
        tcg_gen_qemu_ld_tl(t0, t0, mem_idx, MO_SB);
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_LD_BU:
        tcg_gen_qemu_ld_tl(t0, t0, mem_idx, MO_UB);
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_LL_W:
        gen_store_gpr(t0, rt);
        break;
    }

    tcg_temp_free(t0);
}

/* Store */
static void gen_st(DisasContext *ctx, uint32_t opc, int rt, int base,
                   int offset)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_base_offset_addr(ctx, t0, base, offset);
    gen_load_gpr(t1, rt);

    switch (opc) {
    case OPC_LARCH_STPTR_D:
    case OPC_LARCH_ST_D:
        tcg_gen_qemu_st_tl(t1, t0, mem_idx,
                           MO_TEQ | ctx->default_tcg_memop_mask);
        break;
    case OPC_LARCH_STPTR_W:
    case OPC_LARCH_ST_W:
        tcg_gen_qemu_st_tl(t1, t0, mem_idx,
                           MO_TEUL | ctx->default_tcg_memop_mask);
        break;
    case OPC_LARCH_ST_H:
        tcg_gen_qemu_st_tl(t1, t0, mem_idx,
                           MO_TEUW | ctx->default_tcg_memop_mask);
        break;
    case OPC_LARCH_ST_B:
        tcg_gen_qemu_st_tl(t1, t0, mem_idx, MO_8);
        break;
    }
    tcg_temp_free(t0);
    tcg_temp_free(t1);
}

/* Store conditional */
static void gen_st_cond(DisasContext *ctx, int rt, int base, int offset,
                        MemOp tcg_mo, bool eva)
{
    TCGv addr, t0, val;
    TCGLabel *l1 = gen_new_label();
    TCGLabel *done = gen_new_label();

    t0 = tcg_temp_new();
    addr = tcg_temp_new();
    /* compare the address against that of the preceeding LL */
    gen_base_offset_addr(ctx, addr, base, offset);
    tcg_gen_brcond_tl(TCG_COND_EQ, addr, cpu_lladdr, l1);
    tcg_temp_free(addr);
    tcg_gen_movi_tl(t0, 0);
    gen_store_gpr(t0, rt);
    tcg_gen_br(done);

    gen_set_label(l1);
    /* generate cmpxchg */
    val = tcg_temp_new();
    gen_load_gpr(val, rt);
    tcg_gen_atomic_cmpxchg_tl(t0, cpu_lladdr, cpu_llval, val,
                              eva ? LARCH_HFLAG_UM : ctx->mem_idx, tcg_mo);
    tcg_gen_setcond_tl(TCG_COND_EQ, t0, t0, cpu_llval);
    gen_store_gpr(t0, rt);
    tcg_temp_free(val);

    gen_set_label(done);
    tcg_temp_free(t0);
}

/* Load and store */
static void gen_flt_ldst(DisasContext *ctx, uint32_t opc, int ft, TCGv t0)
{
    /*
     * Don't do NOP if destination is zero: we must perform the actual
     * memory access.
     */
    switch (opc) {
    case OPC_LARCH_FLD_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        tcg_gen_qemu_ld_i32(fp0, t0, ctx->mem_idx,
                            MO_TESL | ctx->default_tcg_memop_mask);
        gen_store_fpr32(ctx, fp0, ft);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FST_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        gen_load_fpr32(ctx, fp0, ft);
        tcg_gen_qemu_st_i32(fp0, t0, ctx->mem_idx,
                            MO_TEUL | ctx->default_tcg_memop_mask);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FLD_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        tcg_gen_qemu_ld_i64(fp0, t0, ctx->mem_idx,
                            MO_TEQ | ctx->default_tcg_memop_mask);
        gen_store_fpr64(ctx, fp0, ft);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FST_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        gen_load_fpr64(ctx, fp0, ft);
        tcg_gen_qemu_st_i64(fp0, t0, ctx->mem_idx,
                            MO_TEQ | ctx->default_tcg_memop_mask);
        tcg_temp_free_i64(fp0);
    } break;
    default:
        LARCH_INVAL("flt_ldst");
        generate_exception_end(ctx, EXCP_RI);
        break;
    }
}

static void gen_fp_ldst(DisasContext *ctx, uint32_t op, int rt, int rs,
                        int16_t imm)
{
    TCGv t0 = tcg_temp_new();

    check_cp1_enabled(ctx);
    gen_base_offset_addr(ctx, t0, rs, imm);
    gen_flt_ldst(ctx, op, rt, t0);
    tcg_temp_free(t0);
}

/* Arithmetic with immediate operand */
static void gen_arith_imm(DisasContext *ctx, uint32_t opc, int rt, int rs,
                          int imm)
{
    target_ulong uimm = (target_long)imm; /* Sign extend to 32/64 bits */

    if (rt == 0) {
        /*
         * If no destination, treat it as a NOP.
         * For addi, we must generate the overflow exception when needed.
         */
        return;
    }
    switch (opc) {
    case OPC_LARCH_ADDI_W:
        if (rs != 0) {
            tcg_gen_addi_tl(cpu_gpr[rt], cpu_gpr[rs], uimm);
            tcg_gen_ext32s_tl(cpu_gpr[rt], cpu_gpr[rt]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rt], uimm);
        }
        break;
    case OPC_LARCH_ADDI_D:
        if (rs != 0) {
            tcg_gen_addi_tl(cpu_gpr[rt], cpu_gpr[rs], uimm);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rt], uimm);
        }
        break;
    }
}

/* Logic with immediate operand */
static void gen_logic_imm(DisasContext *ctx, uint32_t opc, int rt, int rs,
                          int16_t imm)
{
    target_ulong uimm;

    if (rt == 0) {
        /* If no destination, treat it as a NOP. */
        return;
    }
    uimm = (uint16_t)imm;
    switch (opc) {
    case OPC_LARCH_ANDI:
        if (likely(rs != 0)) {
            tcg_gen_andi_tl(cpu_gpr[rt], cpu_gpr[rs], uimm);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rt], 0);
        }
        break;
    case OPC_LARCH_ORI:
        if (rs != 0) {
            tcg_gen_ori_tl(cpu_gpr[rt], cpu_gpr[rs], uimm);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rt], uimm);
        }
        break;
    case OPC_LARCH_XORI:
        if (likely(rs != 0)) {
            tcg_gen_xori_tl(cpu_gpr[rt], cpu_gpr[rs], uimm);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rt], uimm);
        }
        break;
    default:
        break;
    }
}

/* Set on less than with immediate operand */
static void gen_slt_imm(DisasContext *ctx, uint32_t opc, int rt, int rs,
                        int16_t imm)
{
    target_ulong uimm = (target_long)imm; /* Sign extend to 32/64 bits */
    TCGv t0;

    if (rt == 0) {
        /* If no destination, treat it as a NOP. */
        return;
    }
    t0 = tcg_temp_new();
    gen_load_gpr(t0, rs);
    switch (opc) {
    case OPC_LARCH_SLTI:
        tcg_gen_setcondi_tl(TCG_COND_LT, cpu_gpr[rt], t0, uimm);
        break;
    case OPC_LARCH_SLTIU:
        tcg_gen_setcondi_tl(TCG_COND_LTU, cpu_gpr[rt], t0, uimm);
        break;
    }
    tcg_temp_free(t0);
}

/* Shifts with immediate operand */
static void gen_shift_imm(DisasContext *ctx, uint32_t opc, int rt, int rs,
                          int16_t imm)
{
    target_ulong uimm = ((uint16_t)imm) & 0x1f;
    TCGv t0;

    if (rt == 0) {
        /* If no destination, treat it as a NOP. */
        return;
    }

    t0 = tcg_temp_new();
    gen_load_gpr(t0, rs);
    switch (opc) {
    case OPC_LARCH_SRAI_W:
        tcg_gen_sari_tl(cpu_gpr[rt], t0, uimm);
        break;
    case OPC_LARCH_SRLI_W:
        if (uimm != 0) {
            tcg_gen_ext32u_tl(t0, t0);
            tcg_gen_shri_tl(cpu_gpr[rt], t0, uimm);
        } else {
            tcg_gen_ext32s_tl(cpu_gpr[rt], t0);
        }
        break;
    case OPC_LARCH_ROTRI_W:
        if (uimm != 0) {
            TCGv_i32 t1 = tcg_temp_new_i32();

            tcg_gen_trunc_tl_i32(t1, t0);
            tcg_gen_rotri_i32(t1, t1, uimm);
            tcg_gen_ext_i32_tl(cpu_gpr[rt], t1);
            tcg_temp_free_i32(t1);
        } else {
            tcg_gen_ext32s_tl(cpu_gpr[rt], t0);
        }
        break;
    }
    tcg_temp_free(t0);
}

/* Arithmetic */
static void gen_arith(DisasContext *ctx, uint32_t opc, int rd, int rs, int rt)
{
    if (rd == 0) {
        /*
         * If no destination, treat it as a NOP.
         * For add & sub, we must generate the
         * overflow exception when needed.
         */
        return;
    }

    switch (opc) {
    case OPC_LARCH_ADD_W:
        if (rs != 0 && rt != 0) {
            tcg_gen_add_tl(cpu_gpr[rd], cpu_gpr[rs], cpu_gpr[rt]);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        } else if (rs == 0 && rt != 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rt]);
        } else if (rs != 0 && rt == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rs]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case OPC_LARCH_SUB_W:
        if (rs != 0 && rt != 0) {
            tcg_gen_sub_tl(cpu_gpr[rd], cpu_gpr[rs], cpu_gpr[rt]);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        } else if (rs == 0 && rt != 0) {
            tcg_gen_neg_tl(cpu_gpr[rd], cpu_gpr[rt]);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        } else if (rs != 0 && rt == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rs]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case OPC_LARCH_ADD_D:
        if (rs != 0 && rt != 0) {
            tcg_gen_add_tl(cpu_gpr[rd], cpu_gpr[rs], cpu_gpr[rt]);
        } else if (rs == 0 && rt != 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rt]);
        } else if (rs != 0 && rt == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rs]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case OPC_LARCH_SUB_D:
        if (rs != 0 && rt != 0) {
            tcg_gen_sub_tl(cpu_gpr[rd], cpu_gpr[rs], cpu_gpr[rt]);
        } else if (rs == 0 && rt != 0) {
            tcg_gen_neg_tl(cpu_gpr[rd], cpu_gpr[rt]);
        } else if (rs != 0 && rt == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rs]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    }
}

/* Conditional move */
static void gen_cond_move(DisasContext *ctx, uint32_t opc, int rd, int rs,
                          int rt)
{
    TCGv t0, t1, t2;

    if (rd == 0) {
        /* If no destination, treat it as a NOP. */
        return;
    }

    t0 = tcg_temp_new();
    gen_load_gpr(t0, rt);
    t1 = tcg_const_tl(0);
    t2 = tcg_temp_new();
    gen_load_gpr(t2, rs);
    switch (opc) {
    case OPC_LARCH_MASKEQZ:
        tcg_gen_movcond_tl(TCG_COND_NE, cpu_gpr[rd], t0, t1, t2, t1);
        break;
    case OPC_LARCH_MASKNEZ:
        tcg_gen_movcond_tl(TCG_COND_EQ, cpu_gpr[rd], t0, t1, t2, t1);
        break;
    }
    tcg_temp_free(t2);
    tcg_temp_free(t1);
    tcg_temp_free(t0);
}

/* Logic */
static void gen_logic(DisasContext *ctx, uint32_t opc, int rd, int rs, int rt)
{
    if (rd == 0) {
        /* If no destination, treat it as a NOP. */
        return;
    }

    switch (opc) {
    case OPC_LARCH_AND:
        if (likely(rs != 0 && rt != 0)) {
            tcg_gen_and_tl(cpu_gpr[rd], cpu_gpr[rs], cpu_gpr[rt]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case OPC_LARCH_NOR:
        if (rs != 0 && rt != 0) {
            tcg_gen_nor_tl(cpu_gpr[rd], cpu_gpr[rs], cpu_gpr[rt]);
        } else if (rs == 0 && rt != 0) {
            tcg_gen_not_tl(cpu_gpr[rd], cpu_gpr[rt]);
        } else if (rs != 0 && rt == 0) {
            tcg_gen_not_tl(cpu_gpr[rd], cpu_gpr[rs]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], ~((target_ulong)0));
        }
        break;
    case OPC_LARCH_OR:
        if (likely(rs != 0 && rt != 0)) {
            tcg_gen_or_tl(cpu_gpr[rd], cpu_gpr[rs], cpu_gpr[rt]);
        } else if (rs == 0 && rt != 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rt]);
        } else if (rs != 0 && rt == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rs]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case OPC_LARCH_XOR:
        if (likely(rs != 0 && rt != 0)) {
            tcg_gen_xor_tl(cpu_gpr[rd], cpu_gpr[rs], cpu_gpr[rt]);
        } else if (rs == 0 && rt != 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rt]);
        } else if (rs != 0 && rt == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rs]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    }
}

/* Set on lower than */
static void gen_slt(DisasContext *ctx, uint32_t opc, int rd, int rs, int rt)
{
    TCGv t0, t1;

    if (rd == 0) {
        /* If no destination, treat it as a NOP. */
        return;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    gen_load_gpr(t0, rs);
    gen_load_gpr(t1, rt);
    switch (opc) {
    case OPC_LARCH_SLT:
        tcg_gen_setcond_tl(TCG_COND_LT, cpu_gpr[rd], t0, t1);
        break;
    case OPC_LARCH_SLTU:
        tcg_gen_setcond_tl(TCG_COND_LTU, cpu_gpr[rd], t0, t1);
        break;
    }
    tcg_temp_free(t0);
    tcg_temp_free(t1);
}

/* Shifts */
static void gen_shift(DisasContext *ctx, uint32_t opc, int rd, int rs, int rt)
{
    TCGv t0, t1;

    if (rd == 0) {
        /*
         * If no destination, treat it as a NOP.
         * For add & sub, we must generate the
         * overflow exception when needed.
         */
        return;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    gen_load_gpr(t0, rs);
    gen_load_gpr(t1, rt);
    switch (opc) {
    case OPC_LARCH_SLL_W:
        tcg_gen_andi_tl(t0, t0, 0x1f);
        tcg_gen_shl_tl(t0, t1, t0);
        tcg_gen_ext32s_tl(cpu_gpr[rd], t0);
        break;
    case OPC_LARCH_SRA_W:
        tcg_gen_andi_tl(t0, t0, 0x1f);
        tcg_gen_sar_tl(cpu_gpr[rd], t1, t0);
        break;
    case OPC_LARCH_SRL_W:
        tcg_gen_ext32u_tl(t1, t1);
        tcg_gen_andi_tl(t0, t0, 0x1f);
        tcg_gen_shr_tl(t0, t1, t0);
        tcg_gen_ext32s_tl(cpu_gpr[rd], t0);
        break;
    case OPC_LARCH_ROTR_W: {
        TCGv_i32 t2 = tcg_temp_new_i32();
        TCGv_i32 t3 = tcg_temp_new_i32();

        tcg_gen_trunc_tl_i32(t2, t0);
        tcg_gen_trunc_tl_i32(t3, t1);
        tcg_gen_andi_i32(t2, t2, 0x1f);
        tcg_gen_rotr_i32(t2, t3, t2);
        tcg_gen_ext_i32_tl(cpu_gpr[rd], t2);
        tcg_temp_free_i32(t2);
        tcg_temp_free_i32(t3);
    } break;
    case OPC_LARCH_SLL_D:
        tcg_gen_andi_tl(t0, t0, 0x3f);
        tcg_gen_shl_tl(cpu_gpr[rd], t1, t0);
        break;
    case OPC_LARCH_SRA_D:
        tcg_gen_andi_tl(t0, t0, 0x3f);
        tcg_gen_sar_tl(cpu_gpr[rd], t1, t0);
        break;
    case OPC_LARCH_SRL_D:
        tcg_gen_andi_tl(t0, t0, 0x3f);
        tcg_gen_shr_tl(cpu_gpr[rd], t1, t0);
        break;
    case OPC_LARCH_ROTR_D:
        tcg_gen_andi_tl(t0, t0, 0x3f);
        tcg_gen_rotr_tl(cpu_gpr[rd], t1, t0);
        break;
    }
    tcg_temp_free(t0);
    tcg_temp_free(t1);
}

static inline void gen_r6_ld(target_long addr, int reg, int memidx,
                             MemOp memop)
{
    TCGv t0 = tcg_const_tl(addr);
    tcg_gen_qemu_ld_tl(t0, t0, memidx, memop);
    gen_store_gpr(t0, reg);
    tcg_temp_free(t0);
}

static void gen_r6_muldiv(DisasContext *ctx, int opc, int rd, int rs, int rt)
{
    TCGv t0, t1;

    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();

    gen_load_gpr(t0, rs);
    gen_load_gpr(t1, rt);

    switch (opc) {
    case OPC_LARCH_DIV_W: {
        TCGv t2 = tcg_temp_new();
        TCGv t3 = tcg_temp_new();
        tcg_gen_ext32s_tl(t0, t0);
        tcg_gen_ext32s_tl(t1, t1);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, INT_MIN);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1);
        tcg_gen_and_tl(t2, t2, t3);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
        tcg_gen_or_tl(t2, t2, t3);
        tcg_gen_movi_tl(t3, 0);
        tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
        tcg_gen_div_tl(cpu_gpr[rd], t0, t1);
        tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        tcg_temp_free(t3);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_MOD_W: {
        TCGv t2 = tcg_temp_new();
        TCGv t3 = tcg_temp_new();
        tcg_gen_ext32s_tl(t0, t0);
        tcg_gen_ext32s_tl(t1, t1);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, INT_MIN);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1);
        tcg_gen_and_tl(t2, t2, t3);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
        tcg_gen_or_tl(t2, t2, t3);
        tcg_gen_movi_tl(t3, 0);
        tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
        tcg_gen_rem_tl(cpu_gpr[rd], t0, t1);
        tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        tcg_temp_free(t3);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_DIV_WU: {
        TCGv t2 = tcg_const_tl(0);
        TCGv t3 = tcg_const_tl(1);
        tcg_gen_ext32u_tl(t0, t0);
        tcg_gen_ext32u_tl(t1, t1);
        tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
        tcg_gen_divu_tl(cpu_gpr[rd], t0, t1);
        tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        tcg_temp_free(t3);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_MOD_WU: {
        TCGv t2 = tcg_const_tl(0);
        TCGv t3 = tcg_const_tl(1);
        tcg_gen_ext32u_tl(t0, t0);
        tcg_gen_ext32u_tl(t1, t1);
        tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
        tcg_gen_remu_tl(cpu_gpr[rd], t0, t1);
        tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        tcg_temp_free(t3);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_MUL_W: {
        TCGv_i32 t2 = tcg_temp_new_i32();
        TCGv_i32 t3 = tcg_temp_new_i32();
        tcg_gen_trunc_tl_i32(t2, t0);
        tcg_gen_trunc_tl_i32(t3, t1);
        tcg_gen_mul_i32(t2, t2, t3);
        tcg_gen_ext_i32_tl(cpu_gpr[rd], t2);
        tcg_temp_free_i32(t2);
        tcg_temp_free_i32(t3);
    } break;
    case OPC_LARCH_MULH_W: {
        TCGv_i32 t2 = tcg_temp_new_i32();
        TCGv_i32 t3 = tcg_temp_new_i32();
        tcg_gen_trunc_tl_i32(t2, t0);
        tcg_gen_trunc_tl_i32(t3, t1);
        tcg_gen_muls2_i32(t2, t3, t2, t3);
        tcg_gen_ext_i32_tl(cpu_gpr[rd], t3);
        tcg_temp_free_i32(t2);
        tcg_temp_free_i32(t3);
    } break;
    case OPC_LARCH_MULH_WU: {
        TCGv_i32 t2 = tcg_temp_new_i32();
        TCGv_i32 t3 = tcg_temp_new_i32();
        tcg_gen_trunc_tl_i32(t2, t0);
        tcg_gen_trunc_tl_i32(t3, t1);
        tcg_gen_mulu2_i32(t2, t3, t2, t3);
        tcg_gen_ext_i32_tl(cpu_gpr[rd], t3);
        tcg_temp_free_i32(t2);
        tcg_temp_free_i32(t3);
    } break;
    case OPC_LARCH_DIV_D: {
        TCGv t2 = tcg_temp_new();
        TCGv t3 = tcg_temp_new();
        tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, -1LL << 63);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1LL);
        tcg_gen_and_tl(t2, t2, t3);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
        tcg_gen_or_tl(t2, t2, t3);
        tcg_gen_movi_tl(t3, 0);
        tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
        tcg_gen_div_tl(cpu_gpr[rd], t0, t1);
        tcg_temp_free(t3);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_MOD_D: {
        TCGv t2 = tcg_temp_new();
        TCGv t3 = tcg_temp_new();
        tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, -1LL << 63);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1LL);
        tcg_gen_and_tl(t2, t2, t3);
        tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
        tcg_gen_or_tl(t2, t2, t3);
        tcg_gen_movi_tl(t3, 0);
        tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
        tcg_gen_rem_tl(cpu_gpr[rd], t0, t1);
        tcg_temp_free(t3);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_DIV_DU: {
        TCGv t2 = tcg_const_tl(0);
        TCGv t3 = tcg_const_tl(1);
        tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
        tcg_gen_divu_i64(cpu_gpr[rd], t0, t1);
        tcg_temp_free(t3);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_MOD_DU: {
        TCGv t2 = tcg_const_tl(0);
        TCGv t3 = tcg_const_tl(1);
        tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
        tcg_gen_remu_i64(cpu_gpr[rd], t0, t1);
        tcg_temp_free(t3);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_MUL_D:
        tcg_gen_mul_i64(cpu_gpr[rd], t0, t1);
        break;
    case OPC_LARCH_MULH_D: {
        TCGv t2 = tcg_temp_new();
        tcg_gen_muls2_i64(t2, cpu_gpr[rd], t0, t1);
        tcg_temp_free(t2);
    } break;
    case OPC_LARCH_MULH_DU: {
        TCGv t2 = tcg_temp_new();
        tcg_gen_mulu2_i64(t2, cpu_gpr[rd], t0, t1);
        tcg_temp_free(t2);
    } break;
    default:
        LARCH_INVAL("r6 mul/div");
        generate_exception_end(ctx, EXCP_RI);
        goto out;
    }
out:
    tcg_temp_free(t0);
    tcg_temp_free(t1);
}

static void gen_cl(DisasContext *ctx, uint32_t opc, int rd, int rs)
{
    TCGv t0;

    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    t0 = cpu_gpr[rd];
    gen_load_gpr(t0, rs);

    switch (opc) {
    case OPC_LARCH_CLO_W:
    case OPC_LARCH_CLO_D:
        tcg_gen_not_tl(t0, t0);
        break;
    }

    switch (opc) {
    case OPC_LARCH_CLO_W:
    case OPC_LARCH_CLZ_W:
        tcg_gen_ext32u_tl(t0, t0);
        tcg_gen_clzi_tl(t0, t0, TARGET_LONG_BITS);
        tcg_gen_subi_tl(t0, t0, TARGET_LONG_BITS - 32);
        break;
    case OPC_LARCH_CLO_D:
    case OPC_LARCH_CLZ_D:
        tcg_gen_clzi_i64(t0, t0, 64);
        break;
    }
}

static inline bool use_goto_tb(DisasContext *ctx, target_ulong dest)
{
    if (unlikely(ctx->base.singlestep_enabled)) {
        return false;
    }

#ifndef CONFIG_USER_ONLY
    return (ctx->base.tb->pc & TARGET_PAGE_MASK) == (dest & TARGET_PAGE_MASK);
#else
    return true;
#endif
}

static inline void gen_goto_tb(DisasContext *ctx, int n, target_ulong dest)
{
    if (use_goto_tb(ctx, dest)) {
        tcg_gen_goto_tb(n);
        gen_save_pc(dest);
        tcg_gen_exit_tb(ctx->base.tb, n);
    } else {
        gen_save_pc(dest);
        if (ctx->base.singlestep_enabled) {
            save_cpu_state(ctx, 0);
            gen_helper_raise_exception_debug(cpu_env);
        }
        tcg_gen_lookup_and_goto_ptr();
    }
}

/* Branches */
static void gen_compute_branch(DisasContext *ctx, uint32_t opc, int insn_bytes,
                               int rs, int rt, int32_t offset)
{
    target_ulong btgt = -1;
    int bcond_compute = 0;
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();

    if (ctx->hflags & LARCH_HFLAG_BMASK) {
#ifdef LARCH_DEBUG_DISAS
        LOG_DISAS("Branch at PC 0x" TARGET_FMT_lx "\n", ctx->base.pc_next);
#endif
        generate_exception_end(ctx, EXCP_RI);
        goto out;
    }

    /* Load needed operands */
    switch (opc) {
    case OPC_LARCH_BLT:
    case OPC_LARCH_BGE:
    case OPC_LARCH_BLTU:
    case OPC_LARCH_BGEU:
        gen_load_gpr(t0, rs);
        gen_load_gpr(t1, rt);
        bcond_compute = 1;
        btgt = ctx->base.pc_next + offset;
        break;
    case OPC_LARCH_BEQZ:
    case OPC_LARCH_B:
    case OPC_LARCH_BEQ:
    case OPC_LARCH_BNEZ:
    case OPC_LARCH_BNE:
        /* Compare two registers */
        if (rs != rt) {
            gen_load_gpr(t0, rs);
            gen_load_gpr(t1, rt);
            bcond_compute = 1;
        }
        btgt = ctx->base.pc_next + offset;
        break;
    default:
        LARCH_INVAL("branch/jump");
        generate_exception_end(ctx, EXCP_RI);
        goto out;
    }
    if (bcond_compute == 0) {
        /* No condition to be computed */
        switch (opc) {
        case OPC_LARCH_BEQZ: /* rx == rx        */
        case OPC_LARCH_B:
        case OPC_LARCH_BEQ:
            /* Always take */
            ctx->hflags |= LARCH_HFLAG_B;
            break;
        case OPC_LARCH_BNEZ:
        case OPC_LARCH_BNE:
            /* Treat as NOP. */
            goto out;
        default:
            LARCH_INVAL("branch/jump");
            generate_exception_end(ctx, EXCP_RI);
            goto out;
        }
    } else {
        switch (opc) {
        case OPC_LARCH_BLT:
            tcg_gen_setcond_tl(TCG_COND_LT, bcond, t0, t1);
            goto not_likely;
        case OPC_LARCH_BGE:
            tcg_gen_setcond_tl(TCG_COND_GE, bcond, t0, t1);
            goto not_likely;
        case OPC_LARCH_BLTU:
            tcg_gen_setcond_tl(TCG_COND_LTU, bcond, t0, t1);
            goto not_likely;
        case OPC_LARCH_BGEU:
            tcg_gen_setcond_tl(TCG_COND_GEU, bcond, t0, t1);
            goto not_likely;
        case OPC_LARCH_BEQZ:
        case OPC_LARCH_B:
        case OPC_LARCH_BEQ:
            tcg_gen_setcond_tl(TCG_COND_EQ, bcond, t0, t1);
            goto not_likely;
        case OPC_LARCH_BNEZ:
        case OPC_LARCH_BNE:
            tcg_gen_setcond_tl(TCG_COND_NE, bcond, t0, t1);
            goto not_likely;
        not_likely:
            ctx->hflags |= LARCH_HFLAG_BC;
            break;
        default:
            LARCH_INVAL("conditional branch/jump");
            generate_exception_end(ctx, EXCP_RI);
            goto out;
        }
    }

    ctx->btarget = btgt;

out:
    tcg_temp_free(t0);
    tcg_temp_free(t1);
}

/* special3 bitfield operations */
static void gen_bitops(DisasContext *ctx, uint32_t opc, int rt, int rs,
                       int lsb, int msb)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();

    gen_load_gpr(t1, rs);
    switch (opc) {
    case OPC_LARCH_TRPICK_W:
        if (lsb + msb > 31) {
            goto fail;
        }
        if (msb != 31) {
            tcg_gen_extract_tl(t0, t1, lsb, msb + 1);
        } else {
            /*
             * The two checks together imply that lsb == 0,
             * so this is a simple sign-extension.
             */
            tcg_gen_ext32s_tl(t0, t1);
        }
        break;
    case OPC_LARCH_TRINS_W:
        if (lsb > msb) {
            goto fail;
        }
        gen_load_gpr(t0, rt);
        tcg_gen_deposit_tl(t0, t0, t1, lsb, msb - lsb + 1);
        tcg_gen_ext32s_tl(t0, t0);
        break;
    default:
    fail:
        LARCH_INVAL("bitops");
        generate_exception_end(ctx, EXCP_RI);
        tcg_temp_free(t0);
        tcg_temp_free(t1);
        return;
    }
    gen_store_gpr(t0, rt);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
}

static void gen_bshfl(DisasContext *ctx, uint32_t op2, int rt, int rd)
{
    TCGv t0;

    if (rd == 0) {
        /* If no destination, treat it as a NOP. */
        return;
    }

    t0 = tcg_temp_new();
    gen_load_gpr(t0, rt);
    switch (op2) {
    case OPC_LARCH_REVB_2H: {
        TCGv t1 = tcg_temp_new();
        TCGv t2 = tcg_const_tl(0x00FF00FF);

        tcg_gen_shri_tl(t1, t0, 8);
        tcg_gen_and_tl(t1, t1, t2);
        tcg_gen_and_tl(t0, t0, t2);
        tcg_gen_shli_tl(t0, t0, 8);
        tcg_gen_or_tl(t0, t0, t1);
        tcg_temp_free(t2);
        tcg_temp_free(t1);
        tcg_gen_ext32s_tl(cpu_gpr[rd], t0);
    } break;
    case OPC_LARCH_EXT_WB:
        tcg_gen_ext8s_tl(cpu_gpr[rd], t0);
        break;
    case OPC_LARCH_EXT_WH:
        tcg_gen_ext16s_tl(cpu_gpr[rd], t0);
        break;
    case OPC_LARCH_REVB_4H: {
        TCGv t1 = tcg_temp_new();
        TCGv t2 = tcg_const_tl(0x00FF00FF00FF00FFULL);

        tcg_gen_shri_tl(t1, t0, 8);
        tcg_gen_and_tl(t1, t1, t2);
        tcg_gen_and_tl(t0, t0, t2);
        tcg_gen_shli_tl(t0, t0, 8);
        tcg_gen_or_tl(cpu_gpr[rd], t0, t1);
        tcg_temp_free(t2);
        tcg_temp_free(t1);
    } break;
    case OPC_LARCH_REVH_D: {
        TCGv t1 = tcg_temp_new();
        TCGv t2 = tcg_const_tl(0x0000FFFF0000FFFFULL);

        tcg_gen_shri_tl(t1, t0, 16);
        tcg_gen_and_tl(t1, t1, t2);
        tcg_gen_and_tl(t0, t0, t2);
        tcg_gen_shli_tl(t0, t0, 16);
        tcg_gen_or_tl(t0, t0, t1);
        tcg_gen_shri_tl(t1, t0, 32);
        tcg_gen_shli_tl(t0, t0, 32);
        tcg_gen_or_tl(cpu_gpr[rd], t0, t1);
        tcg_temp_free(t2);
        tcg_temp_free(t1);
    } break;
    default:
        LARCH_INVAL("bsfhl");
        generate_exception_end(ctx, EXCP_RI);
        tcg_temp_free(t0);
        return;
    }
    tcg_temp_free(t0);
}

/* REV with sf==1, opcode==3 ("REV64") */
static void handle_rev64(DisasContext *ctx, unsigned int rn, unsigned int rd)
{
    tcg_gen_bswap64_i64(cpu_gpr[rd], cpu_gpr[rn]);
}

/*
 * REV with sf==0, opcode==2
 * REV32 (sf==1, opcode==2)
 */
static void handle_rev32(DisasContext *ctx, unsigned int rn, unsigned int rd)
{
    TCGv_i64 tcg_rd = tcg_temp_new_i64();
    gen_load_gpr(tcg_rd, rd);

    TCGv_i64 tcg_tmp = tcg_temp_new_i64();
    TCGv_i64 tcg_rn = tcg_temp_new_i64();
    gen_load_gpr(tcg_rn, rn);

    /* bswap32_i64 requires zero high word */
    tcg_gen_ext32u_i64(tcg_tmp, tcg_rn);
    tcg_gen_bswap32_i64(tcg_rd, tcg_tmp, TCG_BSWAP_OZ);
    tcg_gen_shri_i64(tcg_tmp, tcg_rn, 32);
    tcg_gen_bswap32_i64(tcg_tmp, tcg_tmp, TCG_BSWAP_OZ);
    tcg_gen_concat32_i64(cpu_gpr[rd], tcg_rd, tcg_tmp);

    tcg_temp_free_i64(tcg_tmp);
    tcg_temp_free_i64(tcg_rd);
    tcg_temp_free_i64(tcg_rn);
}

/* REV16 */
static void handle_rev16(DisasContext *ctx, unsigned int rn, unsigned int rd)
{
    TCGv_i64 tcg_rd = tcg_temp_new_i64();
    TCGv_i64 tcg_rn = tcg_temp_new_i64();
    gen_load_gpr(tcg_rd, rd);
    gen_load_gpr(tcg_rn, rn);
    TCGv_i64 tcg_tmp = tcg_temp_new_i64();
    TCGv_i64 mask = tcg_const_i64(0x0000ffff0000ffffull);

    tcg_gen_shri_i64(tcg_tmp, tcg_rn, 16);
    tcg_gen_and_i64(tcg_rd, tcg_rn, mask);
    tcg_gen_and_i64(tcg_tmp, tcg_tmp, mask);
    tcg_gen_shli_i64(tcg_rd, tcg_rd, 16);
    tcg_gen_or_i64(cpu_gpr[rd], tcg_rd, tcg_tmp);

    tcg_temp_free_i64(mask);
    tcg_temp_free_i64(tcg_tmp);
    tcg_temp_free_i64(tcg_rd);
    tcg_temp_free_i64(tcg_rn);
}

static void gen_lsa(DisasContext *ctx, int opc, int rd, int rs, int rt,
                    int imm2)
{
    TCGv t0;
    TCGv t1;
    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    gen_load_gpr(t0, rs);
    gen_load_gpr(t1, rt);
    tcg_gen_shli_tl(t0, t0, imm2 + 1);
    tcg_gen_add_tl(cpu_gpr[rd], t0, t1);
    if (opc == OPC_LARCH_ALSL_W) {
        tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
    }

    tcg_temp_free(t1);
    tcg_temp_free(t0);

    return;
}

static void gen_align_bits(DisasContext *ctx, int wordsz, int rd, int rs,
                           int rt, int bits)
{
    TCGv t0;
    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    t0 = tcg_temp_new();
    if (bits == 0 || bits == wordsz) {
        if (bits == 0) {
            gen_load_gpr(t0, rt);
        } else {
            gen_load_gpr(t0, rs);
        }
        switch (wordsz) {
        case 32:
            tcg_gen_ext32s_tl(cpu_gpr[rd], t0);
            break;
        case 64:
            tcg_gen_mov_tl(cpu_gpr[rd], t0);
            break;
        }
    } else {
        TCGv t1 = tcg_temp_new();
        gen_load_gpr(t0, rt);
        gen_load_gpr(t1, rs);
        switch (wordsz) {
        case 32: {
            TCGv_i64 t2 = tcg_temp_new_i64();
            tcg_gen_concat_tl_i64(t2, t1, t0);
            tcg_gen_shri_i64(t2, t2, 32 - bits);
            gen_move_low32(cpu_gpr[rd], t2);
            tcg_temp_free_i64(t2);
        } break;
        case 64:
            tcg_gen_shli_tl(t0, t0, bits);
            tcg_gen_shri_tl(t1, t1, 64 - bits);
            tcg_gen_or_tl(cpu_gpr[rd], t1, t0);
            break;
        }
        tcg_temp_free(t1);
    }

    tcg_temp_free(t0);
}

static void gen_align(DisasContext *ctx, int wordsz, int rd, int rs, int rt,
                      int bp)
{
    gen_align_bits(ctx, wordsz, rd, rs, rt, bp * 8);
}

static void gen_bitswap(DisasContext *ctx, int opc, int rd, int rt)
{
    TCGv t0;
    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    t0 = tcg_temp_new();
    gen_load_gpr(t0, rt);
    switch (opc) {
    case OPC_LARCH_BREV_4B:
        gen_helper_bitswap(cpu_gpr[rd], t0);
        break;
    case OPC_LARCH_BREV_8B:
        gen_helper_dbitswap(cpu_gpr[rd], t0);
        break;
    }
    tcg_temp_free(t0);
}

static void gen_cp1(DisasContext *ctx, uint32_t opc, int rt, int fs)
{
    TCGv t0 = tcg_temp_new();
    check_cp1_enabled(ctx);

    switch (opc) {
    case OPC_LARCH_FR2GR_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        tcg_gen_ext_i32_tl(t0, fp0);
        tcg_temp_free_i32(fp0);
    }
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_GR2FR_W:
        gen_load_gpr(t0, rt);
        {
            TCGv_i32 fp0 = tcg_temp_new_i32();

            tcg_gen_trunc_tl_i32(fp0, t0);
            gen_store_fpr32(ctx, fp0, fs);
            tcg_temp_free_i32(fp0);
        }
        break;
    case OPC_LARCH_FR2GR_D:
        gen_load_fpr64(ctx, t0, fs);
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_GR2FR_D:
        gen_load_gpr(t0, rt);
        gen_store_fpr64(ctx, t0, fs);
        break;
    case OPC_LARCH_FRH2GR_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32h(ctx, fp0, fs);
        tcg_gen_ext_i32_tl(t0, fp0);
        tcg_temp_free_i32(fp0);
    }
        gen_store_gpr(t0, rt);
        break;
    case OPC_LARCH_GR2FRH_W:
        gen_load_gpr(t0, rt);
        {
            TCGv_i32 fp0 = tcg_temp_new_i32();

            tcg_gen_trunc_tl_i32(fp0, t0);
            gen_store_fpr32h(ctx, fp0, fs);
            tcg_temp_free_i32(fp0);
        }
        break;
    default:
        LARCH_INVAL("cp1 move");
        generate_exception_end(ctx, EXCP_RI);
        goto out;
    }

out:
    tcg_temp_free(t0);
}

static inline void gen_movcf_ps(DisasContext *ctx, int fs, int fd, int cc,
                                int tf)
{
    int cond;
    TCGv_i32 t0 = tcg_temp_new_i32();
    TCGLabel *l1 = gen_new_label();
    TCGLabel *l2 = gen_new_label();

    if (tf) {
        cond = TCG_COND_EQ;
    } else {
        cond = TCG_COND_NE;
    }

    tcg_gen_andi_i32(t0, fpu_fcsr0, 1 << get_fp_bit(cc));
    tcg_gen_brcondi_i32(cond, t0, 0, l1);
    gen_load_fpr32(ctx, t0, fs);
    gen_store_fpr32(ctx, t0, fd);
    gen_set_label(l1);

    tcg_gen_andi_i32(t0, fpu_fcsr0, 1 << get_fp_bit(cc + 1));
    tcg_gen_brcondi_i32(cond, t0, 0, l2);
    gen_load_fpr32h(ctx, t0, fs);
    gen_store_fpr32h(ctx, t0, fd);
    tcg_temp_free_i32(t0);
    gen_set_label(l2);
}

static void gen_farith(DisasContext *ctx, uint32_t opc, int ft, int fs, int fd,
                       int cc)
{
    check_cp1_enabled(ctx);
    switch (opc) {
    case OPC_LARCH_FADD_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        TCGv_i32 fp1 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_load_fpr32(ctx, fp1, ft);
        gen_helper_float_add_s(fp0, cpu_env, fp0, fp1);
        tcg_temp_free_i32(fp1);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FSUB_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        TCGv_i32 fp1 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_load_fpr32(ctx, fp1, ft);
        gen_helper_float_sub_s(fp0, cpu_env, fp0, fp1);
        tcg_temp_free_i32(fp1);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FMUL_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        TCGv_i32 fp1 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_load_fpr32(ctx, fp1, ft);
        gen_helper_float_mul_s(fp0, cpu_env, fp0, fp1);
        tcg_temp_free_i32(fp1);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FDIV_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        TCGv_i32 fp1 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_load_fpr32(ctx, fp1, ft);
        gen_helper_float_div_s(fp0, cpu_env, fp0, fp1);
        tcg_temp_free_i32(fp1);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FSQRT_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_sqrt_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FABS_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_abs_s(fp0, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FMOV_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FNEG_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_chs_s(fp0, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FTINTRNE_L_S: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr32(ctx, fp32, fs);
        gen_helper_float_round_l_s(fp64, cpu_env, fp32);
        tcg_temp_free_i32(fp32);
        gen_store_fpr64(ctx, fp64, fd);
        tcg_temp_free_i64(fp64);
    } break;
    case OPC_LARCH_FTINTRZ_L_S: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr32(ctx, fp32, fs);
        gen_helper_float_trunc_l_s(fp64, cpu_env, fp32);
        tcg_temp_free_i32(fp32);
        gen_store_fpr64(ctx, fp64, fd);
        tcg_temp_free_i64(fp64);
    } break;
    case OPC_LARCH_FTINTRP_L_S: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr32(ctx, fp32, fs);
        gen_helper_float_ceil_l_s(fp64, cpu_env, fp32);
        tcg_temp_free_i32(fp32);
        gen_store_fpr64(ctx, fp64, fd);
        tcg_temp_free_i64(fp64);
    } break;
    case OPC_LARCH_FTINTRM_L_S: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr32(ctx, fp32, fs);
        gen_helper_float_floor_l_s(fp64, cpu_env, fp32);
        tcg_temp_free_i32(fp32);
        gen_store_fpr64(ctx, fp64, fd);
        tcg_temp_free_i64(fp64);
    } break;
    case OPC_LARCH_FTINTRNE_W_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_round_w_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FTINTRZ_W_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_trunc_w_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FTINTRP_W_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_ceil_w_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FTINTRM_W_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_floor_w_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FRECIP_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_recip_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FRSQRT_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_rsqrt_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FRINT_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_rint_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FCLASS_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_class_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FMIN_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        TCGv_i32 fp1 = tcg_temp_new_i32();
        TCGv_i32 fp2 = tcg_temp_new_i32();
        gen_load_fpr32(ctx, fp0, fs);
        gen_load_fpr32(ctx, fp1, ft);
        gen_helper_float_min_s(fp2, cpu_env, fp0, fp1);
        gen_store_fpr32(ctx, fp2, fd);
        tcg_temp_free_i32(fp2);
        tcg_temp_free_i32(fp1);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FMINA_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        TCGv_i32 fp1 = tcg_temp_new_i32();
        TCGv_i32 fp2 = tcg_temp_new_i32();
        gen_load_fpr32(ctx, fp0, fs);
        gen_load_fpr32(ctx, fp1, ft);
        gen_helper_float_mina_s(fp2, cpu_env, fp0, fp1);
        gen_store_fpr32(ctx, fp2, fd);
        tcg_temp_free_i32(fp2);
        tcg_temp_free_i32(fp1);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FMAX_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        TCGv_i32 fp1 = tcg_temp_new_i32();
        gen_load_fpr32(ctx, fp0, fs);
        gen_load_fpr32(ctx, fp1, ft);
        gen_helper_float_max_s(fp1, cpu_env, fp0, fp1);
        gen_store_fpr32(ctx, fp1, fd);
        tcg_temp_free_i32(fp1);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FMAXA_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        TCGv_i32 fp1 = tcg_temp_new_i32();
        gen_load_fpr32(ctx, fp0, fs);
        gen_load_fpr32(ctx, fp1, ft);
        gen_helper_float_maxa_s(fp1, cpu_env, fp0, fp1);
        gen_store_fpr32(ctx, fp1, fd);
        tcg_temp_free_i32(fp1);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FCVT_D_S: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr32(ctx, fp32, fs);
        gen_helper_float_cvtd_s(fp64, cpu_env, fp32);
        tcg_temp_free_i32(fp32);
        gen_store_fpr64(ctx, fp64, fd);
        tcg_temp_free_i64(fp64);
    } break;
    case OPC_LARCH_FTINT_W_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_cvt_w_s(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FTINT_L_S: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr32(ctx, fp32, fs);
        gen_helper_float_cvt_l_s(fp64, cpu_env, fp32);
        tcg_temp_free_i32(fp32);
        gen_store_fpr64(ctx, fp64, fd);
        tcg_temp_free_i64(fp64);
    } break;
    case OPC_LARCH_FADD_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        TCGv_i64 fp1 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_load_fpr64(ctx, fp1, ft);
        gen_helper_float_add_d(fp0, cpu_env, fp0, fp1);
        tcg_temp_free_i64(fp1);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FSUB_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        TCGv_i64 fp1 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_load_fpr64(ctx, fp1, ft);
        gen_helper_float_sub_d(fp0, cpu_env, fp0, fp1);
        tcg_temp_free_i64(fp1);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FMUL_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        TCGv_i64 fp1 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_load_fpr64(ctx, fp1, ft);
        gen_helper_float_mul_d(fp0, cpu_env, fp0, fp1);
        tcg_temp_free_i64(fp1);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FDIV_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        TCGv_i64 fp1 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_load_fpr64(ctx, fp1, ft);
        gen_helper_float_div_d(fp0, cpu_env, fp0, fp1);
        tcg_temp_free_i64(fp1);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FSQRT_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_sqrt_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FABS_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_abs_d(fp0, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FMOV_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FNEG_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_chs_d(fp0, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FTINTRNE_L_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_round_l_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FTINTRZ_L_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_trunc_l_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FTINTRP_L_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_ceil_l_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FTINTRM_L_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_floor_l_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FTINTRNE_W_D: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp64, fs);
        gen_helper_float_round_w_d(fp32, cpu_env, fp64);
        tcg_temp_free_i64(fp64);
        gen_store_fpr32(ctx, fp32, fd);
        tcg_temp_free_i32(fp32);
    } break;
    case OPC_LARCH_FTINTRZ_W_D: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp64, fs);
        gen_helper_float_trunc_w_d(fp32, cpu_env, fp64);
        tcg_temp_free_i64(fp64);
        gen_store_fpr32(ctx, fp32, fd);
        tcg_temp_free_i32(fp32);
    } break;
    case OPC_LARCH_FTINTRP_W_D: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp64, fs);
        gen_helper_float_ceil_w_d(fp32, cpu_env, fp64);
        tcg_temp_free_i64(fp64);
        gen_store_fpr32(ctx, fp32, fd);
        tcg_temp_free_i32(fp32);
    } break;
    case OPC_LARCH_FTINTRM_W_D: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp64, fs);
        gen_helper_float_floor_w_d(fp32, cpu_env, fp64);
        tcg_temp_free_i64(fp64);
        gen_store_fpr32(ctx, fp32, fd);
        tcg_temp_free_i32(fp32);
    } break;
    case OPC_LARCH_FRECIP_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_recip_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FRSQRT_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_rsqrt_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FRINT_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_rint_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FCLASS_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_class_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FMIN_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        TCGv_i64 fp1 = tcg_temp_new_i64();
        gen_load_fpr64(ctx, fp0, fs);
        gen_load_fpr64(ctx, fp1, ft);
        gen_helper_float_min_d(fp1, cpu_env, fp0, fp1);
        gen_store_fpr64(ctx, fp1, fd);
        tcg_temp_free_i64(fp1);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FMINA_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        TCGv_i64 fp1 = tcg_temp_new_i64();
        gen_load_fpr64(ctx, fp0, fs);
        gen_load_fpr64(ctx, fp1, ft);
        gen_helper_float_mina_d(fp1, cpu_env, fp0, fp1);
        gen_store_fpr64(ctx, fp1, fd);
        tcg_temp_free_i64(fp1);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FMAX_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        TCGv_i64 fp1 = tcg_temp_new_i64();
        gen_load_fpr64(ctx, fp0, fs);
        gen_load_fpr64(ctx, fp1, ft);
        gen_helper_float_max_d(fp1, cpu_env, fp0, fp1);
        gen_store_fpr64(ctx, fp1, fd);
        tcg_temp_free_i64(fp1);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FMAXA_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        TCGv_i64 fp1 = tcg_temp_new_i64();
        gen_load_fpr64(ctx, fp0, fs);
        gen_load_fpr64(ctx, fp1, ft);
        gen_helper_float_maxa_d(fp1, cpu_env, fp0, fp1);
        gen_store_fpr64(ctx, fp1, fd);
        tcg_temp_free_i64(fp1);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FCVT_S_D: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp64, fs);
        gen_helper_float_cvts_d(fp32, cpu_env, fp64);
        tcg_temp_free_i64(fp64);
        gen_store_fpr32(ctx, fp32, fd);
        tcg_temp_free_i32(fp32);
    } break;
    case OPC_LARCH_FTINT_W_D: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp64, fs);
        gen_helper_float_cvt_w_d(fp32, cpu_env, fp64);
        tcg_temp_free_i64(fp64);
        gen_store_fpr32(ctx, fp32, fd);
        tcg_temp_free_i32(fp32);
    } break;
    case OPC_LARCH_FTINT_L_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_cvt_l_d(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FFINT_S_W: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        gen_load_fpr32(ctx, fp0, fs);
        gen_helper_float_cvts_w(fp0, cpu_env, fp0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FFINT_D_W: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr32(ctx, fp32, fs);
        gen_helper_float_cvtd_w(fp64, cpu_env, fp32);
        tcg_temp_free_i32(fp32);
        gen_store_fpr64(ctx, fp64, fd);
        tcg_temp_free_i64(fp64);
    } break;
    case OPC_LARCH_FFINT_S_L: {
        TCGv_i32 fp32 = tcg_temp_new_i32();
        TCGv_i64 fp64 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp64, fs);
        gen_helper_float_cvts_l(fp32, cpu_env, fp64);
        tcg_temp_free_i64(fp64);
        gen_store_fpr32(ctx, fp32, fd);
        tcg_temp_free_i32(fp32);
    } break;
    case OPC_LARCH_FFINT_D_L: {
        TCGv_i64 fp0 = tcg_temp_new_i64();

        gen_load_fpr64(ctx, fp0, fs);
        gen_helper_float_cvtd_l(fp0, cpu_env, fp0);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    default:
        LARCH_INVAL("farith");
        generate_exception_end(ctx, EXCP_RI);
        return;
    }
}

/* Coprocessor 3 (FPU) */
static void gen_flt3_ldst(DisasContext *ctx, uint32_t opc, int fd, int fs,
                          int base, int index)
{
    TCGv t0 = tcg_temp_new();

    check_cp1_enabled(ctx);
    if (base == 0) {
        gen_load_gpr(t0, index);
    } else if (index == 0) {
        gen_load_gpr(t0, base);
    } else {
        gen_op_addr_add(ctx, t0, cpu_gpr[base], cpu_gpr[index]);
    }

    /*
     * Don't do NOP if destination is zero: we must perform the actual
     * memory access.
     */
    switch (opc) {
    case OPC_LARCH_FLDX_S:
    case OPC_LARCH_FLDGT_S:
    case OPC_LARCH_FLDLE_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();

        tcg_gen_qemu_ld_tl(t0, t0, ctx->mem_idx, MO_TESL);
        tcg_gen_trunc_tl_i32(fp0, t0);
        gen_store_fpr32(ctx, fp0, fd);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FLDX_D:
    case OPC_LARCH_FLDGT_D:
    case OPC_LARCH_FLDLE_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        tcg_gen_qemu_ld_i64(fp0, t0, ctx->mem_idx, MO_TEQ);
        gen_store_fpr64(ctx, fp0, fd);
        tcg_temp_free_i64(fp0);
    } break;
    case OPC_LARCH_FSTX_S:
    case OPC_LARCH_FSTGT_S:
    case OPC_LARCH_FSTLE_S: {
        TCGv_i32 fp0 = tcg_temp_new_i32();
        gen_load_fpr32(ctx, fp0, fs);
        tcg_gen_qemu_st_i32(fp0, t0, ctx->mem_idx, MO_TEUL);
        tcg_temp_free_i32(fp0);
    } break;
    case OPC_LARCH_FSTX_D:
    case OPC_LARCH_FSTGT_D:
    case OPC_LARCH_FSTLE_D: {
        TCGv_i64 fp0 = tcg_temp_new_i64();
        gen_load_fpr64(ctx, fp0, fs);
        tcg_gen_qemu_st_i64(fp0, t0, ctx->mem_idx, MO_TEQ);
        tcg_temp_free_i64(fp0);
    } break;
    }
    tcg_temp_free(t0);
}

static inline void clear_branch_hflags(DisasContext *ctx)
{
    ctx->hflags &= ~LARCH_HFLAG_BMASK;
    if (ctx->base.is_jmp == DISAS_NEXT) {
        save_cpu_state(ctx, 0);
    } else {
        /*
         * It is not safe to save ctx->hflags as hflags may be changed
         * in execution time.
         */
        tcg_gen_andi_i32(hflags, hflags, ~LARCH_HFLAG_BMASK);
    }
}

static void gen_branch(DisasContext *ctx, int insn_bytes)
{
    if (ctx->hflags & LARCH_HFLAG_BMASK) {
        int proc_hflags = ctx->hflags & LARCH_HFLAG_BMASK;
        /* Branches completion */
        clear_branch_hflags(ctx);
        ctx->base.is_jmp = DISAS_NORETURN;
        /* FIXME: Need to clear can_do_io.  */
        switch (proc_hflags & LARCH_HFLAG_BMASK) {
        case LARCH_HFLAG_B:
            /* unconditional branch */
            gen_goto_tb(ctx, 0, ctx->btarget);
            break;
        case LARCH_HFLAG_BC:
            /* Conditional branch */
            {
                TCGLabel *l1 = gen_new_label();

                tcg_gen_brcondi_tl(TCG_COND_NE, bcond, 0, l1);
                gen_goto_tb(ctx, 1, ctx->base.pc_next + insn_bytes);
                gen_set_label(l1);
                gen_goto_tb(ctx, 0, ctx->btarget);
            }
            break;
        case LARCH_HFLAG_BR:
            /* unconditional branch to register */
            tcg_gen_mov_tl(cpu_PC, btarget);
            if (ctx->base.singlestep_enabled) {
                save_cpu_state(ctx, 0);
                gen_helper_raise_exception_debug(cpu_env);
            }
            tcg_gen_lookup_and_goto_ptr();
            break;
        default:
            fprintf(stderr, "unknown branch 0x%x\n", proc_hflags);
            abort();
        }
    }
}

/* Signed immediate */
#define SIMM(op, start, width)                                                \
    ((int32_t)(((op >> start) & ((~0U) >> (32 - width))) << (32 - width)) >>  \
     (32 - width))
/* Zero-extended immediate */
#define ZIMM(op, start, width) ((op >> start) & ((~0U) >> (32 - width)))

static void gen_sync(int stype)
{
    TCGBar tcg_mo = TCG_BAR_SC;

    switch (stype) {
    case 0x4: /* SYNC_WMB */
        tcg_mo |= TCG_MO_ST_ST;
        break;
    case 0x10: /* SYNC_MB */
        tcg_mo |= TCG_MO_ALL;
        break;
    case 0x11: /* SYNC_ACQUIRE */
        tcg_mo |= TCG_MO_LD_LD | TCG_MO_LD_ST;
        break;
    case 0x12: /* SYNC_RELEASE */
        tcg_mo |= TCG_MO_ST_ST | TCG_MO_LD_ST;
        break;
    case 0x13: /* SYNC_RMB */
        tcg_mo |= TCG_MO_LD_LD;
        break;
    default:
        tcg_mo |= TCG_MO_ALL;
        break;
    }

    tcg_gen_mb(tcg_mo);
}

static void gen_crc32(DisasContext *ctx, int rd, int rs, int rt, int sz,
                      int crc32c)
{
    TCGv t0;
    TCGv t1;
    TCGv_i32 tsz = tcg_const_i32(1 << sz);
    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    t0 = tcg_temp_new();
    t1 = tcg_temp_new();

    gen_load_gpr(t0, rt);
    gen_load_gpr(t1, rs);

    if (crc32c) {
        gen_helper_crc32c(cpu_gpr[rd], t0, t1, tsz);
    } else {
        gen_helper_crc32(cpu_gpr[rd], t0, t1, tsz);
    }

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free_i32(tsz);
}

#include "cpu-csr.h"

#ifndef CONFIG_USER_ONLY

/*
 * 64-bit CSR read
 *
 * @arg :   GPR to store the value of CSR register
 * @csr :   CSR register number
 */
static void gen_csr_rdq(DisasContext *ctx, TCGv rd, int64_t a1)
{
    TCGv_i64 csr = tcg_const_i64(a1);
    gen_helper_csr_rdq(rd, cpu_env, csr);
}

/*
 * 64-bit CSR write
 *
 * @arg :   GPR that stores the new value of CSR register
 * @csr :   CSR register number
 */
static void gen_csr_wrq(DisasContext *ctx, TCGv val, int64_t a1)
{
    TCGv_i64 csr = tcg_const_i64(a1);
    gen_helper_csr_wrq(val, cpu_env, val, csr);
}

/*
 * 64-bit CSR exchange
 *
 * @arg :   GPR that stores the new value of CSR register
 * @csr :   CSR register number
 */
static void gen_csr_xchgq(DisasContext *ctx, TCGv val, TCGv mask, int64_t a1)
{
    TCGv_i64 csr = tcg_const_i64(a1);
    gen_helper_csr_xchgq(val, cpu_env, val, mask, csr);
}
#endif /* !CONFIG_USER_ONLY */

static void loongarch_tr_init_disas_context(DisasContextBase *dcbase,
                                            CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    CPULOONGARCHState *env = cs->env_ptr;

    ctx->page_start = ctx->base.pc_first & TARGET_PAGE_MASK;
    ctx->saved_pc = -1;
    ctx->insn_flags = env->insn_flags;
    ctx->btarget = 0;
    /* Restore state from the tb context.  */
    ctx->hflags =
        (uint32_t)ctx->base.tb->flags; /* FIXME: maybe use 64 bits? */
    restore_cpu_state(env, ctx);
#ifdef CONFIG_USER_ONLY
    ctx->mem_idx = LARCH_HFLAG_UM;
#else
    ctx->mem_idx = hflags_mmu_index(ctx->hflags);
#endif
    ctx->default_tcg_memop_mask = MO_ALIGN;

    LOG_DISAS("\ntb %p idx %d hflags %04x\n", ctx->base.tb, ctx->mem_idx,
              ctx->hflags);
}

static void loongarch_tr_tb_start(DisasContextBase *dcbase, CPUState *cs)
{
}

static void loongarch_tr_insn_start(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    tcg_gen_insn_start(ctx->base.pc_next, ctx->hflags & LARCH_HFLAG_BMASK,
                       ctx->btarget);
}

/* 128 and 256 lsx vector instructions are not supported yet */
static bool decode_vector_lsx(uint32_t opcode)
{
    uint32_t value = (opcode & 0xff000000);

    if ((opcode & 0xf0000000) == 0x70000000) {
        return true;
    } else if ((opcode & 0xfff00000) == 0x38400000) {
        return true;
    } else {
        switch (value) {
        case 0x09000000:
        case 0x0a000000:
        case 0x0e000000:
        case 0x0f000000:
        case 0x2c000000:
        case 0x30000000:
        case 0x31000000:
        case 0x32000000:
        case 0x33000000:
            return true;
        }
    }
    return false;
}

static bool decode_insn(DisasContext *ctx, uint32_t insn);
#include "decode-insn.c.inc"
#include "trans.inc.c"

static void loongarch_tr_translate_insn(DisasContextBase *dcbase, CPUState *cs)
{
    CPULOONGARCHState *env = cs->env_ptr;
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    int insn_bytes = 4;

    ctx->opcode = cpu_ldl_code(env, ctx->base.pc_next);

    if (!decode_insn(ctx, ctx->opcode)) {
        if (decode_vector_lsx(ctx->opcode)) {
            generate_exception_end(ctx, EXCP_RI);
        } else {
            fprintf(stderr, "Error: unkown opcode. 0x%lx: 0x%x\n",
                    ctx->base.pc_next, ctx->opcode);
            generate_exception_end(ctx, EXCP_RI);
        }
    }

    if (ctx->hflags & LARCH_HFLAG_BMASK) {
        gen_branch(ctx, insn_bytes);
    }
    ctx->base.pc_next += insn_bytes;
}

static void loongarch_tr_tb_stop(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    if (ctx->base.singlestep_enabled && ctx->base.is_jmp != DISAS_NORETURN) {
        save_cpu_state(ctx, ctx->base.is_jmp != DISAS_EXIT);
        gen_helper_raise_exception_debug(cpu_env);
    } else {
        switch (ctx->base.is_jmp) {
        case DISAS_STOP:
            gen_save_pc(ctx->base.pc_next);
            tcg_gen_lookup_and_goto_ptr();
            break;
        case DISAS_NEXT:
        case DISAS_TOO_MANY:
            save_cpu_state(ctx, 0);
            gen_goto_tb(ctx, 0, ctx->base.pc_next);
            break;
        case DISAS_EXIT:
            tcg_gen_exit_tb(NULL, 0);
            break;
        case DISAS_NORETURN:
            break;
        default:
            g_assert_not_reached();
        }
    }
}

static void loongarch_tr_disas_log(const DisasContextBase *dcbase,
                                   CPUState *cs)
{
    qemu_log("IN: %s\n", lookup_symbol(dcbase->pc_first));
    log_target_disas(cs, dcbase->pc_first, dcbase->tb->size);
}

static const TranslatorOps loongarch_tr_ops = {
    .init_disas_context = loongarch_tr_init_disas_context,
    .tb_start = loongarch_tr_tb_start,
    .insn_start = loongarch_tr_insn_start,
    .translate_insn = loongarch_tr_translate_insn,
    .tb_stop = loongarch_tr_tb_stop,
    .disas_log = loongarch_tr_disas_log,
};

void gen_intermediate_code(CPUState *cs, struct TranslationBlock *tb,
                           int max_insns)
{
    DisasContext ctx;

    translator_loop(&loongarch_tr_ops, &ctx.base, cs, tb, max_insns);
}

void loongarch_tcg_init(void)
{
    int i;

    for (i = 0; i < 32; i++)
        cpu_gpr[i] = tcg_global_mem_new(
            cpu_env, offsetof(CPULOONGARCHState, active_tc.gpr[i]),
            regnames[i]);

    for (i = 0; i < 32; i++) {
        int off = offsetof(CPULOONGARCHState, active_fpu.fpr[i].d);
        fpu_f64[i] = tcg_global_mem_new_i64(cpu_env, off, fregnames[i]);
    }

    cpu_PC = tcg_global_mem_new(
        cpu_env, offsetof(CPULOONGARCHState, active_tc.PC), "PC");
    bcond = tcg_global_mem_new(cpu_env, offsetof(CPULOONGARCHState, bcond),
                               "bcond");
    btarget = tcg_global_mem_new(cpu_env, offsetof(CPULOONGARCHState, btarget),
                                 "btarget");
    hflags = tcg_global_mem_new_i32(
        cpu_env, offsetof(CPULOONGARCHState, hflags), "hflags");
    fpu_fcsr0 = tcg_global_mem_new_i32(
        cpu_env, offsetof(CPULOONGARCHState, active_fpu.fcsr0), "fcsr0");
    cpu_lladdr = tcg_global_mem_new(
        cpu_env, offsetof(CPULOONGARCHState, lladdr), "lladdr");
    cpu_llval = tcg_global_mem_new(cpu_env, offsetof(CPULOONGARCHState, llval),
                                   "llval");
}

void restore_state_to_opc(CPULOONGARCHState *env, TranslationBlock *tb,
                          target_ulong *data)
{
    env->active_tc.PC = data[0];
    env->hflags &= ~LARCH_HFLAG_BMASK;
    env->hflags |= data[1];
    switch (env->hflags & LARCH_HFLAG_BMASK) {
    case LARCH_HFLAG_BR:
        break;
    case LARCH_HFLAG_BC:
    case LARCH_HFLAG_B:
        env->btarget = data[2];
        break;
    }
}
