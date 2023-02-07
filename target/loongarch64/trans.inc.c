/*
 * LOONGARCH emulation for QEMU - main translation routines Extension
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

static bool trans_syscall(DisasContext *ctx, arg_syscall *a)
{
    generate_exception_end(ctx, EXCP_SYSCALL);
    return true;
}

static bool trans_break(DisasContext *ctx, arg_break *a)
{
    generate_exception_end(ctx, EXCP_BREAK);
    return true;
}

static bool trans_dbcl(DisasContext *ctx, arg_dbcl *a)
{
    /*
     * dbcl instruction is not support in tcg
     */
    generate_exception_end(ctx, EXCP_RI);
    return true;
}

static bool trans_addi_w(DisasContext *ctx, arg_addi_w *a)
{
    gen_arith_imm(ctx, OPC_LARCH_ADDI_W, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_addi_d(DisasContext *ctx, arg_addi_d *a)
{
    gen_arith_imm(ctx, OPC_LARCH_ADDI_D, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_slli_d(DisasContext *ctx, arg_slli_d *a)
{
    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    tcg_gen_shli_tl(cpu_gpr[a->rd], t0, a->ui6);

    tcg_temp_free(t0);
    return true;
}

static bool trans_andi(DisasContext *ctx, arg_andi *a)
{
    gen_logic_imm(ctx, OPC_LARCH_ANDI, a->rd, a->rj, a->ui12);
    return true;
}

static bool trans_srli_d(DisasContext *ctx, arg_srli_d *a)
{
    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    tcg_gen_shri_tl(cpu_gpr[a->rd], t0, a->ui6);

    tcg_temp_free(t0);
    return true;
}

static bool trans_slli_w(DisasContext *ctx, arg_slli_w *a)
{
    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    tcg_gen_shli_tl(t0, t0, a->ui5);
    tcg_gen_ext32s_tl(cpu_gpr[a->rd], t0);

    tcg_temp_free(t0);
    return true;
}

static bool trans_addu16i_d(DisasContext *ctx, arg_addu16i_d *a)
{
    if (a->rj != 0) {
        tcg_gen_addi_tl(cpu_gpr[a->rd], cpu_gpr[a->rj], a->si16 << 16);
    } else {
        tcg_gen_movi_tl(cpu_gpr[a->rd], a->si16 << 16);
    }
    return true;
}

static bool trans_lu12i_w(DisasContext *ctx, arg_lu12i_w *a)
{
    tcg_gen_movi_tl(cpu_gpr[a->rd], a->si20 << 12);
    return true;
}

static bool trans_lu32i_d(DisasContext *ctx, arg_lu32i_d *a)
{
    TCGv_i64 t0, t1;
    t0 = tcg_temp_new_i64();
    t1 = tcg_temp_new_i64();

    tcg_gen_movi_tl(t0, a->si20);
    tcg_gen_concat_tl_i64(t1, cpu_gpr[a->rd], t0);
    gen_store_gpr(t1, a->rd);

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_pcaddi(DisasContext *ctx, arg_pcaddi *a)
{
    target_ulong pc = ctx->base.pc_next;
    target_ulong addr = pc + (a->si20 << 2);
    tcg_gen_movi_tl(cpu_gpr[a->rd], addr);
    return true;
}

static bool trans_pcalau12i(DisasContext *ctx, arg_pcalau12i *a)
{
    target_ulong pc = ctx->base.pc_next;
    target_ulong addr = (pc + (a->si20 << 12)) & ~0xfff;
    tcg_gen_movi_tl(cpu_gpr[a->rd], addr);
    return true;
}

static bool trans_pcaddu12i(DisasContext *ctx, arg_pcaddu12i *a)
{
    target_ulong pc = ctx->base.pc_next;
    target_ulong addr = pc + (a->si20 << 12);
    tcg_gen_movi_tl(cpu_gpr[a->rd], addr);
    return true;
}

static bool trans_pcaddu18i(DisasContext *ctx, arg_pcaddu18i *a)
{
    target_ulong pc = ctx->base.pc_next;
    target_ulong addr = pc + ((target_ulong)(a->si20) << 18);
    tcg_gen_movi_tl(cpu_gpr[a->rd], addr);
    return true;
}

static bool trans_slti(DisasContext *ctx, arg_slti *a)
{
    gen_slt_imm(ctx, OPC_LARCH_SLTI, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_sltui(DisasContext *ctx, arg_sltui *a)
{
    gen_slt_imm(ctx, OPC_LARCH_SLTIU, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_lu52i_d(DisasContext *ctx, arg_lu52i_d *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();

    gen_load_gpr(t1, a->rj);

    tcg_gen_movi_tl(t0, a->si12);
    tcg_gen_shli_tl(t0, t0, 52);
    tcg_gen_andi_tl(t1, t1, 0xfffffffffffffU);
    tcg_gen_or_tl(cpu_gpr[a->rd], t0, t1);

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_ori(DisasContext *ctx, arg_ori *a)
{
    gen_logic_imm(ctx, OPC_LARCH_ORI, a->rd, a->rj, a->ui12);
    return true;
}

static bool trans_xori(DisasContext *ctx, arg_xori *a)
{
    gen_logic_imm(ctx, OPC_LARCH_XORI, a->rd, a->rj, a->ui12);
    return true;
}

static bool trans_bstrins_d(DisasContext *ctx, arg_bstrins_d *a)
{
    int lsb = a->lsbd;
    int msb = a->msbd;
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();

    if (lsb > msb) {
        return false;
    }

    gen_load_gpr(t1, a->rj);
    gen_load_gpr(t0, a->rd);
    tcg_gen_deposit_tl(t0, t0, t1, lsb, msb - lsb + 1);
    gen_store_gpr(t0, a->rd);

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_bstrpick_d(DisasContext *ctx, arg_bstrpick_d *a)
{
    int lsb = a->lsbd;
    int msb = a->msbd;
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();

    if (lsb > msb) {
        return false;
    }

    gen_load_gpr(t1, a->rj);
    gen_load_gpr(t0, a->rd);
    tcg_gen_extract_tl(t0, t1, lsb, msb - lsb + 1);
    gen_store_gpr(t0, a->rd);

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_bstrins_w(DisasContext *ctx, arg_bstrins_w *a)
{
    gen_bitops(ctx, OPC_LARCH_TRINS_W, a->rd, a->rj, a->lsbw, a->msbw);
    return true;
}

static bool trans_bstrpick_w(DisasContext *ctx, arg_bstrpick_w *a)
{
    if (a->lsbw > a->msbw) {
        return false;
    }
    gen_bitops(ctx, OPC_LARCH_TRPICK_W, a->rd, a->rj, a->lsbw,
               a->msbw - a->lsbw);
    return true;
}

static bool trans_ldptr_w(DisasContext *ctx, arg_ldptr_w *a)
{
    gen_ld(ctx, OPC_LARCH_LDPTR_W, a->rd, a->rj, a->si14 << 2);
    return true;
}

static bool trans_stptr_w(DisasContext *ctx, arg_stptr_w *a)
{
    gen_st(ctx, OPC_LARCH_STPTR_W, a->rd, a->rj, a->si14 << 2);
    return true;
}

static bool trans_ldptr_d(DisasContext *ctx, arg_ldptr_d *a)
{
    gen_ld(ctx, OPC_LARCH_LDPTR_D, a->rd, a->rj, a->si14 << 2);
    return true;
}

static bool trans_stptr_d(DisasContext *ctx, arg_stptr_d *a)
{
    gen_st(ctx, OPC_LARCH_STPTR_D, a->rd, a->rj, a->si14 << 2);
    return true;
}

static bool trans_ld_b(DisasContext *ctx, arg_ld_b *a)
{
    gen_ld(ctx, OPC_LARCH_LD_B, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_ld_h(DisasContext *ctx, arg_ld_h *a)
{
    gen_ld(ctx, OPC_LARCH_LD_H, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_ld_w(DisasContext *ctx, arg_ld_w *a)
{
    gen_ld(ctx, OPC_LARCH_LD_W, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_ld_d(DisasContext *ctx, arg_ld_d *a)
{
    gen_ld(ctx, OPC_LARCH_LD_D, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_st_b(DisasContext *ctx, arg_st_b *a)
{
    gen_st(ctx, OPC_LARCH_ST_B, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_st_h(DisasContext *ctx, arg_st_h *a)
{
    gen_st(ctx, OPC_LARCH_ST_H, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_st_w(DisasContext *ctx, arg_st_w *a)
{
    gen_st(ctx, OPC_LARCH_ST_W, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_st_d(DisasContext *ctx, arg_st_d *a)
{
    gen_st(ctx, OPC_LARCH_ST_D, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_ld_bu(DisasContext *ctx, arg_ld_bu *a)
{
    gen_ld(ctx, OPC_LARCH_LD_BU, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_ld_hu(DisasContext *ctx, arg_ld_hu *a)
{
    gen_ld(ctx, OPC_LARCH_LD_HU, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_ld_wu(DisasContext *ctx, arg_ld_wu *a)
{
    gen_ld(ctx, OPC_LARCH_LD_WU, a->rd, a->rj, a->si12);
    return true;
}

static bool trans_preld(DisasContext *ctx, arg_preld *a)
{
    /* Treat as NOP. */
    return true;
}

static bool trans_ll_w(DisasContext *ctx, arg_ll_w *a)
{
    gen_ld(ctx, OPC_LARCH_LL_W, a->rd, a->rj, a->si14 << 2);
    return true;
}

static bool trans_sc_w(DisasContext *ctx, arg_sc_w *a)
{
    gen_st_cond(ctx, a->rd, a->rj, a->si14 << 2, MO_TESL, false);
    return true;
}

static bool trans_ll_d(DisasContext *ctx, arg_ll_d *a)
{
    gen_ld(ctx, OPC_LARCH_LL_D, a->rd, a->rj, a->si14 << 2);
    return true;
}

static bool trans_sc_d(DisasContext *ctx, arg_sc_d *a)
{
    gen_st_cond(ctx, a->rd, a->rj, a->si14 << 2, MO_TEQ, false);
    return true;
}

static bool trans_fld_s(DisasContext *ctx, arg_fld_s *a)
{
    gen_fp_ldst(ctx, OPC_LARCH_FLD_S, a->fd, a->rj, a->si12);
    return true;
}

static bool trans_fst_s(DisasContext *ctx, arg_fst_s *a)
{
    gen_fp_ldst(ctx, OPC_LARCH_FST_S, a->fd, a->rj, a->si12);
    return true;
}

static bool trans_fld_d(DisasContext *ctx, arg_fld_d *a)
{
    gen_fp_ldst(ctx, OPC_LARCH_FLD_D, a->fd, a->rj, a->si12);
    return true;
}

static bool trans_fst_d(DisasContext *ctx, arg_fst_d *a)
{
    gen_fp_ldst(ctx, OPC_LARCH_FST_D, a->fd, a->rj, a->si12);
    return true;
}

static bool trans_ldx_b(DisasContext *ctx, arg_ldx_b *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    tcg_gen_qemu_ld_tl(t1, t0, mem_idx, MO_SB);
    gen_store_gpr(t1, a->rd);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_ldx_h(DisasContext *ctx, arg_ldx_h *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    tcg_gen_qemu_ld_tl(t1, t0, mem_idx, MO_TESW | ctx->default_tcg_memop_mask);
    gen_store_gpr(t1, a->rd);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_ldx_w(DisasContext *ctx, arg_ldx_w *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    tcg_gen_qemu_ld_tl(t1, t0, mem_idx, MO_TESL | ctx->default_tcg_memop_mask);
    gen_store_gpr(t1, a->rd);
    tcg_temp_free(t0);
    return true;
}

static bool trans_ldx_d(DisasContext *ctx, arg_ldx_d *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    tcg_gen_qemu_ld_tl(t1, t0, mem_idx, MO_TEQ | ctx->default_tcg_memop_mask);
    gen_store_gpr(t1, a->rd);
    tcg_temp_free(t1);
    return true;
}

static bool trans_stx_b(DisasContext *ctx, arg_stx_b *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    gen_load_gpr(t1, a->rd);
    tcg_gen_qemu_st_tl(t1, t0, mem_idx, MO_8);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_stx_h(DisasContext *ctx, arg_stx_h *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    gen_load_gpr(t1, a->rd);
    tcg_gen_qemu_st_tl(t1, t0, mem_idx, MO_TEUW | ctx->default_tcg_memop_mask);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_stx_w(DisasContext *ctx, arg_stx_w *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    gen_load_gpr(t1, a->rd);
    tcg_gen_qemu_st_tl(t1, t0, mem_idx, MO_TEUL | ctx->default_tcg_memop_mask);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_stx_d(DisasContext *ctx, arg_stx_d *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    gen_load_gpr(t1, a->rd);
    tcg_gen_qemu_st_tl(t1, t0, mem_idx, MO_TEQ | ctx->default_tcg_memop_mask);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_ldx_bu(DisasContext *ctx, arg_ldx_bu *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    tcg_gen_qemu_ld_tl(t1, t0, mem_idx, MO_UB);
    gen_store_gpr(t1, a->rd);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_ldx_hu(DisasContext *ctx, arg_ldx_hu *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    tcg_gen_qemu_ld_tl(t1, t0, mem_idx, MO_TEUW | ctx->default_tcg_memop_mask);
    gen_store_gpr(t1, a->rd);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_ldx_wu(DisasContext *ctx, arg_ldx_wu *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    int mem_idx = ctx->mem_idx;

    gen_op_addr_add(ctx, t0, cpu_gpr[a->rj], cpu_gpr[a->rk]);
    tcg_gen_qemu_ld_tl(t1, t0, mem_idx, MO_TEUL | ctx->default_tcg_memop_mask);
    gen_store_gpr(t1, a->rd);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_fldx_s(DisasContext *ctx, arg_fldx_s *a)
{
    gen_flt3_ldst(ctx, OPC_LARCH_FLDX_S, a->fd, 0, a->rj, a->rk);
    return true;
}

static bool trans_fldx_d(DisasContext *ctx, arg_fldx_d *a)
{
    gen_flt3_ldst(ctx, OPC_LARCH_FLDX_D, a->fd, 0, a->rj, a->rk);
    return true;
}

static bool trans_fstx_s(DisasContext *ctx, arg_fstx_s *a)
{
    gen_flt3_ldst(ctx, OPC_LARCH_FSTX_S, 0, a->fd, a->rj, a->rk);
    return true;
}

static bool trans_fstx_d(DisasContext *ctx, arg_fstx_d *a)
{
    gen_flt3_ldst(ctx, OPC_LARCH_FSTX_D, 0, a->fd, a->rj, a->rk);
    return true;
}

#define TRANS_AM_W(name, op)                                                  \
    static bool trans_##name(DisasContext *ctx, arg_##name *a)                \
    {                                                                         \
        if ((a->rd != 0) && ((a->rj == a->rd) || (a->rk == a->rd))) {         \
            printf("%s: warning, register equal\n", __func__);                \
            return false;                                                     \
        }                                                                     \
        int mem_idx = ctx->mem_idx;                                           \
        TCGv addr = tcg_temp_new();                                           \
        TCGv val = tcg_temp_new();                                            \
        TCGv ret = tcg_temp_new();                                            \
                                                                              \
        gen_load_gpr(addr, a->rj);                                            \
        gen_load_gpr(val, a->rk);                                             \
        tcg_gen_atomic_##op##_tl(ret, addr, val, mem_idx,                     \
                                 MO_TESL | ctx->default_tcg_memop_mask);      \
        gen_store_gpr(ret, a->rd);                                            \
                                                                              \
        tcg_temp_free(addr);                                                  \
        tcg_temp_free(val);                                                   \
        tcg_temp_free(ret);                                                   \
        return true;                                                          \
    }
#define TRANS_AM_D(name, op)                                                  \
    static bool trans_##name(DisasContext *ctx, arg_##name *a)                \
    {                                                                         \
        if ((a->rd != 0) && ((a->rj == a->rd) || (a->rk == a->rd))) {         \
            printf("%s: warning, register equal\n", __func__);                \
            return false;                                                     \
        }                                                                     \
        int mem_idx = ctx->mem_idx;                                           \
        TCGv addr = tcg_temp_new();                                           \
        TCGv val = tcg_temp_new();                                            \
        TCGv ret = tcg_temp_new();                                            \
                                                                              \
        gen_load_gpr(addr, a->rj);                                            \
        gen_load_gpr(val, a->rk);                                             \
        tcg_gen_atomic_##op##_tl(ret, addr, val, mem_idx,                     \
                                 MO_TEQ | ctx->default_tcg_memop_mask);       \
        gen_store_gpr(ret, a->rd);                                            \
                                                                              \
        tcg_temp_free(addr);                                                  \
        tcg_temp_free(val);                                                   \
        tcg_temp_free(ret);                                                   \
        return true;                                                          \
    }
#define TRANS_AM(name, op)                                                    \
    TRANS_AM_W(name##_w, op)                                                  \
    TRANS_AM_D(name##_d, op)
TRANS_AM(amswap, xchg)           /* trans_amswap_w, trans_amswap_d */
TRANS_AM(amadd, fetch_add)       /* trans_amadd_w, trans_amadd_d   */
TRANS_AM(amand, fetch_and)       /* trans_amand_w, trans_amand_d   */
TRANS_AM(amor, fetch_or)         /* trans_amor_w, trans_amor_d     */
TRANS_AM(amxor, fetch_xor)       /* trans_amxor_w, trans_amxor_d   */
TRANS_AM(ammax, fetch_smax)      /* trans_ammax_w, trans_ammax_d   */
TRANS_AM(ammin, fetch_smin)      /* trans_ammin_w, trans_ammin_d   */
TRANS_AM_W(ammax_wu, fetch_umax) /* trans_ammax_wu */
TRANS_AM_D(ammax_du, fetch_umax) /* trans_ammax_du */
TRANS_AM_W(ammin_wu, fetch_umin) /* trans_ammin_wu */
TRANS_AM_D(ammin_du, fetch_umin) /* trans_ammin_du */
#undef TRANS_AM
#undef TRANS_AM_W
#undef TRANS_AM_D

#define TRANS_AM_DB_W(name, op)                                               \
    static bool trans_##name(DisasContext *ctx, arg_##name *a)                \
    {                                                                         \
        if ((a->rd != 0) && ((a->rj == a->rd) || (a->rk == a->rd))) {         \
            printf("%s: warning, register equal\n", __func__);                \
            return false;                                                     \
        }                                                                     \
        int mem_idx = ctx->mem_idx;                                           \
        TCGv addr = tcg_temp_new();                                           \
        TCGv val = tcg_temp_new();                                            \
        TCGv ret = tcg_temp_new();                                            \
                                                                              \
        gen_sync(0x10);                                                       \
        gen_load_gpr(addr, a->rj);                                            \
        gen_load_gpr(val, a->rk);                                             \
        tcg_gen_atomic_##op##_tl(ret, addr, val, mem_idx,                     \
                                 MO_TESL | ctx->default_tcg_memop_mask);      \
        gen_store_gpr(ret, a->rd);                                            \
                                                                              \
        tcg_temp_free(addr);                                                  \
        tcg_temp_free(val);                                                   \
        tcg_temp_free(ret);                                                   \
        return true;                                                          \
    }
#define TRANS_AM_DB_D(name, op)                                               \
    static bool trans_##name(DisasContext *ctx, arg_##name *a)                \
    {                                                                         \
        if ((a->rd != 0) && ((a->rj == a->rd) || (a->rk == a->rd))) {         \
            printf("%s: warning, register equal\n", __func__);                \
            return false;                                                     \
        }                                                                     \
        int mem_idx = ctx->mem_idx;                                           \
        TCGv addr = tcg_temp_new();                                           \
        TCGv val = tcg_temp_new();                                            \
        TCGv ret = tcg_temp_new();                                            \
                                                                              \
        gen_sync(0x10);                                                       \
        gen_load_gpr(addr, a->rj);                                            \
        gen_load_gpr(val, a->rk);                                             \
        tcg_gen_atomic_##op##_tl(ret, addr, val, mem_idx,                     \
                                 MO_TEQ | ctx->default_tcg_memop_mask);       \
        gen_store_gpr(ret, a->rd);                                            \
                                                                              \
        tcg_temp_free(addr);                                                  \
        tcg_temp_free(val);                                                   \
        tcg_temp_free(ret);                                                   \
        return true;                                                          \
    }
#define TRANS_AM_DB(name, op)                                                 \
    TRANS_AM_DB_W(name##_db_w, op)                                            \
    TRANS_AM_DB_D(name##_db_d, op)
TRANS_AM_DB(amswap, xchg)      /* trans_amswap_db_w, trans_amswap_db_d */
TRANS_AM_DB(amadd, fetch_add)  /* trans_amadd_db_w, trans_amadd_db_d   */
TRANS_AM_DB(amand, fetch_and)  /* trans_amand_db_w, trans_amand_db_d   */
TRANS_AM_DB(amor, fetch_or)    /* trans_amor_db_w, trans_amor_db_d     */
TRANS_AM_DB(amxor, fetch_xor)  /* trans_amxor_db_w, trans_amxor_db_d   */
TRANS_AM_DB(ammax, fetch_smax) /* trans_ammax_db_w, trans_ammax_db_d   */
TRANS_AM_DB(ammin, fetch_smin) /* trans_ammin_db_w, trans_ammin_db_d   */
TRANS_AM_DB_W(ammax_db_wu, fetch_umax) /* trans_ammax_db_wu */
TRANS_AM_DB_D(ammax_db_du, fetch_umax) /* trans_ammax_db_du */
TRANS_AM_DB_W(ammin_db_wu, fetch_umin) /* trans_ammin_db_wu */
TRANS_AM_DB_D(ammin_db_du, fetch_umin) /* trans_ammin_db_du */
#undef TRANS_AM_DB
#undef TRANS_AM_DB_W
#undef TRANS_AM_DB_D

static bool trans_dbar(DisasContext *ctx, arg_dbar *a)
{
    gen_sync(a->whint);
    return true;
}

static bool trans_ibar(DisasContext *ctx, arg_ibar *a)
{
    /*
     * FENCE_I is a no-op in QEMU,
     * however we need to end the translation block
     */
    ctx->base.is_jmp = DISAS_STOP;
    return true;
}

#define ASRTGT                                                                \
    do {                                                                      \
        TCGv t1 = tcg_temp_new();                                             \
        TCGv t2 = tcg_temp_new();                                             \
        gen_load_gpr(t1, a->rj);                                              \
        gen_load_gpr(t2, a->rk);                                              \
        gen_helper_asrtgt_d(cpu_env, t1, t2);                                 \
        tcg_temp_free(t1);                                                    \
        tcg_temp_free(t2);                                                    \
    } while (0)

#define ASRTLE                                                                \
    do {                                                                      \
        TCGv t1 = tcg_temp_new();                                             \
        TCGv t2 = tcg_temp_new();                                             \
        gen_load_gpr(t1, a->rj);                                              \
        gen_load_gpr(t2, a->rk);                                              \
        gen_helper_asrtle_d(cpu_env, t1, t2);                                 \
        tcg_temp_free(t1);                                                    \
        tcg_temp_free(t2);                                                    \
    } while (0)

static bool trans_fldgt_s(DisasContext *ctx, arg_fldgt_s *a)
{
    ASRTGT;
    gen_flt3_ldst(ctx, OPC_LARCH_FLDGT_S, a->fd, 0, a->rj, a->rk);
    return true;
}

static bool trans_fldgt_d(DisasContext *ctx, arg_fldgt_d *a)
{
    ASRTGT;
    gen_flt3_ldst(ctx, OPC_LARCH_FLDGT_D, a->fd, 0, a->rj, a->rk);
    return true;
}

static bool trans_fldle_s(DisasContext *ctx, arg_fldle_s *a)
{
    ASRTLE;
    gen_flt3_ldst(ctx, OPC_LARCH_FLDLE_S, a->fd, 0, a->rj, a->rk);
    return true;
}

static bool trans_fldle_d(DisasContext *ctx, arg_fldle_d *a)
{
    ASRTLE;
    gen_flt3_ldst(ctx, OPC_LARCH_FLDLE_D, a->fd, 0, a->rj, a->rk);
    return true;
}

static bool trans_fstgt_s(DisasContext *ctx, arg_fstgt_s *a)
{
    ASRTGT;
    gen_flt3_ldst(ctx, OPC_LARCH_FSTGT_S, 0, a->fd, a->rj, a->rk);
    return true;
}

static bool trans_fstgt_d(DisasContext *ctx, arg_fstgt_d *a)
{
    ASRTGT;
    gen_flt3_ldst(ctx, OPC_LARCH_FSTGT_D, 0, a->fd, a->rj, a->rk);
    return true;
}

static bool trans_fstle_s(DisasContext *ctx, arg_fstle_s *a)
{
    ASRTLE;
    gen_flt3_ldst(ctx, OPC_LARCH_FSTLE_S, 0, a->fd, a->rj, a->rk);
    return true;
}

static bool trans_fstle_d(DisasContext *ctx, arg_fstle_d *a)
{
    ASRTLE;
    gen_flt3_ldst(ctx, OPC_LARCH_FSTLE_D, 0, a->fd, a->rj, a->rk);
    return true;
}

#define DECL_ARG(name)                                                        \
    arg_##name arg = {                                                        \
        .rd = a->rd,                                                          \
        .rj = a->rj,                                                          \
        .rk = a->rk,                                                          \
    };

static bool trans_ldgt_b(DisasContext *ctx, arg_ldgt_b *a)
{
    ASRTGT;
    DECL_ARG(ldx_b)
    trans_ldx_b(ctx, &arg);
    return true;
}

static bool trans_ldgt_h(DisasContext *ctx, arg_ldgt_h *a)
{
    ASRTGT;
    DECL_ARG(ldx_h)
    trans_ldx_h(ctx, &arg);
    return true;
}

static bool trans_ldgt_w(DisasContext *ctx, arg_ldgt_w *a)
{
    ASRTGT;
    DECL_ARG(ldx_w)
    trans_ldx_w(ctx, &arg);
    return true;
}

static bool trans_ldgt_d(DisasContext *ctx, arg_ldgt_d *a)
{
    ASRTGT;
    DECL_ARG(ldx_d)
    trans_ldx_d(ctx, &arg);
    return true;
}

static bool trans_ldle_b(DisasContext *ctx, arg_ldle_b *a)
{
    ASRTLE;
    DECL_ARG(ldx_b)
    trans_ldx_b(ctx, &arg);
    return true;
}

static bool trans_ldle_h(DisasContext *ctx, arg_ldle_h *a)
{
    ASRTLE;
    DECL_ARG(ldx_h)
    trans_ldx_h(ctx, &arg);
    return true;
}

static bool trans_ldle_w(DisasContext *ctx, arg_ldle_w *a)
{
    ASRTLE;
    DECL_ARG(ldx_w)
    trans_ldx_w(ctx, &arg);
    return true;
}

static bool trans_ldle_d(DisasContext *ctx, arg_ldle_d *a)
{
    ASRTLE;
    DECL_ARG(ldx_d)
    trans_ldx_d(ctx, &arg);
    return true;
}

static bool trans_stgt_b(DisasContext *ctx, arg_stgt_b *a)
{
    ASRTGT;
    DECL_ARG(stx_b)
    trans_stx_b(ctx, &arg);
    return true;
}

static bool trans_stgt_h(DisasContext *ctx, arg_stgt_h *a)
{
    ASRTGT;
    DECL_ARG(stx_h)
    trans_stx_h(ctx, &arg);
    return true;
}

static bool trans_stgt_w(DisasContext *ctx, arg_stgt_w *a)
{
    ASRTGT;
    DECL_ARG(stx_w)
    trans_stx_w(ctx, &arg);
    return true;
}

static bool trans_stgt_d(DisasContext *ctx, arg_stgt_d *a)
{
    ASRTGT;
    DECL_ARG(stx_d)
    trans_stx_d(ctx, &arg);
    return true;
}

static bool trans_stle_b(DisasContext *ctx, arg_stle_b *a)
{
    ASRTLE;
    DECL_ARG(stx_b)
    trans_stx_b(ctx, &arg);
    return true;
}

static bool trans_stle_h(DisasContext *ctx, arg_stle_h *a)
{
    ASRTLE;
    DECL_ARG(stx_h)
    trans_stx_h(ctx, &arg);
    return true;
}

static bool trans_stle_w(DisasContext *ctx, arg_stle_w *a)
{
    ASRTLE;
    DECL_ARG(stx_w)
    trans_stx_w(ctx, &arg);
    return true;
}

static bool trans_stle_d(DisasContext *ctx, arg_stle_d *a)
{
    ASRTLE;
    DECL_ARG(stx_d)
    trans_stx_d(ctx, &arg);
    return true;
}

#undef ASRTGT
#undef ASRTLE
#undef DECL_ARG

static bool trans_beqz(DisasContext *ctx, arg_beqz *a)
{
    gen_compute_branch(ctx, OPC_LARCH_BEQZ, 4, a->rj, 0, a->offs21 << 2);
    return true;
}

static bool trans_bnez(DisasContext *ctx, arg_bnez *a)
{
    gen_compute_branch(ctx, OPC_LARCH_BNEZ, 4, a->rj, 0, a->offs21 << 2);
    return true;
}

static bool trans_bceqz(DisasContext *ctx, arg_bceqz *a)
{
    TCGv_i32 cj = tcg_const_i32(a->cj);
    TCGv v0 = tcg_temp_new();
    TCGv v1 = tcg_const_i64(0);

    gen_helper_movcf2reg(v0, cpu_env, cj);
    tcg_gen_setcond_tl(TCG_COND_EQ, bcond, v0, v1);
    ctx->hflags |= LARCH_HFLAG_BC;
    ctx->btarget = ctx->base.pc_next + (a->offs21 << 2);

    tcg_temp_free_i32(cj);
    tcg_temp_free(v0);
    tcg_temp_free(v1);
    return true;
}

static bool trans_bcnez(DisasContext *ctx, arg_bcnez *a)
{
    TCGv_i32 cj = tcg_const_i32(a->cj);
    TCGv v0 = tcg_temp_new();
    TCGv v1 = tcg_const_i64(0);

    gen_helper_movcf2reg(v0, cpu_env, cj);
    tcg_gen_setcond_tl(TCG_COND_NE, bcond, v0, v1);
    ctx->hflags |= LARCH_HFLAG_BC;
    ctx->btarget = ctx->base.pc_next + (a->offs21 << 2);

    tcg_temp_free_i32(cj);
    tcg_temp_free(v0);
    tcg_temp_free(v1);
    return true;
}

static bool trans_b(DisasContext *ctx, arg_b *a)
{
    gen_compute_branch(ctx, OPC_LARCH_B, 4, 0, 0, a->offs << 2);
    return true;
}

static bool trans_bl(DisasContext *ctx, arg_bl *a)
{
    ctx->btarget = ctx->base.pc_next + (a->offs << 2);
    tcg_gen_movi_tl(cpu_gpr[1], ctx->base.pc_next + 4);
    ctx->hflags |= LARCH_HFLAG_B;
    gen_branch(ctx, 4);
    return true;
}

static bool trans_blt(DisasContext *ctx, arg_blt *a)
{
    gen_compute_branch(ctx, OPC_LARCH_BLT, 4, a->rj, a->rd, a->offs16 << 2);
    return true;
}

static bool trans_bge(DisasContext *ctx, arg_bge *a)
{
    gen_compute_branch(ctx, OPC_LARCH_BGE, 4, a->rj, a->rd, a->offs16 << 2);
    return true;
}

static bool trans_bltu(DisasContext *ctx, arg_bltu *a)
{
    gen_compute_branch(ctx, OPC_LARCH_BLTU, 4, a->rj, a->rd, a->offs16 << 2);
    return true;
}

static bool trans_bgeu(DisasContext *ctx, arg_bgeu *a)
{
    gen_compute_branch(ctx, OPC_LARCH_BGEU, 4, a->rj, a->rd, a->offs16 << 2);
    return true;
}

static bool trans_beq(DisasContext *ctx, arg_beq *a)
{
    gen_compute_branch(ctx, OPC_LARCH_BEQ, 4, a->rj, a->rd, a->offs16 << 2);
    return true;
}

static bool trans_bne(DisasContext *ctx, arg_bne *a)
{
    gen_compute_branch(ctx, OPC_LARCH_BNE, 4, a->rj, a->rd, a->offs16 << 2);
    return true;
}

static bool trans_jirl(DisasContext *ctx, arg_jirl *a)
{
    gen_base_offset_addr(ctx, btarget, a->rj, a->offs16 << 2);
    if (a->rd != 0) {
        tcg_gen_movi_tl(cpu_gpr[a->rd], ctx->base.pc_next + 4);
    }
    ctx->hflags |= LARCH_HFLAG_BR;
    gen_branch(ctx, 4);

    return true;
}

#define TRANS_F4FR(name, fmt, op, bits)                                       \
    static bool trans_##name##_##fmt(DisasContext *ctx,                       \
                                     arg_##name##_##fmt *a)                   \
    {                                                                         \
        check_cp1_enabled(ctx);                                               \
        TCGv_i##bits fp0 = tcg_temp_new_i##bits();                            \
        TCGv_i##bits fp1 = tcg_temp_new_i##bits();                            \
        TCGv_i##bits fp2 = tcg_temp_new_i##bits();                            \
        TCGv_i##bits fp3 = tcg_temp_new_i##bits();                            \
        check_cp1_enabled(ctx);                                               \
        gen_load_fpr##bits(ctx, fp0, a->fj);                                  \
        gen_load_fpr##bits(ctx, fp1, a->fk);                                  \
        gen_load_fpr##bits(ctx, fp2, a->fa);                                  \
        gen_helper_float_##op##_##fmt(fp3, cpu_env, fp0, fp1, fp2);           \
        gen_store_fpr##bits(ctx, fp3, a->fd);                                 \
        tcg_temp_free_i##bits(fp3);                                           \
        tcg_temp_free_i##bits(fp2);                                           \
        tcg_temp_free_i##bits(fp1);                                           \
        tcg_temp_free_i##bits(fp0);                                           \
        return true;                                                          \
    }

TRANS_F4FR(fmadd, s, maddf, 32)   /* trans_fmadd_s */
TRANS_F4FR(fmadd, d, maddf, 64)   /* trans_fmadd_d */
TRANS_F4FR(fmsub, s, msubf, 32)   /* trans_fmsub_s */
TRANS_F4FR(fmsub, d, msubf, 64)   /* trans_fmsub_d */
TRANS_F4FR(fnmadd, s, nmaddf, 32) /* trans_fnmadd_s */
TRANS_F4FR(fnmadd, d, nmaddf, 64) /* trans_fnmadd_d */
TRANS_F4FR(fnmsub, s, nmsubf, 32) /* trans_fnmsub_s */
TRANS_F4FR(fnmsub, d, nmsubf, 64) /* trans_fnmsub_d */
#undef TRANS_F4FR

static bool trans_fadd_s(DisasContext *ctx, arg_fadd_s *a)
{
    gen_farith(ctx, OPC_LARCH_FADD_S, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fadd_d(DisasContext *ctx, arg_fadd_d *a)
{
    gen_farith(ctx, OPC_LARCH_FADD_D, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fsub_s(DisasContext *ctx, arg_fsub_s *a)
{
    gen_farith(ctx, OPC_LARCH_FSUB_S, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fsub_d(DisasContext *ctx, arg_fsub_d *a)
{
    gen_farith(ctx, OPC_LARCH_FSUB_D, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmul_s(DisasContext *ctx, arg_fmul_s *a)
{
    gen_farith(ctx, OPC_LARCH_FMUL_S, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmul_d(DisasContext *ctx, arg_fmul_d *a)
{
    gen_farith(ctx, OPC_LARCH_FMUL_D, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fdiv_s(DisasContext *ctx, arg_fdiv_s *a)
{
    gen_farith(ctx, OPC_LARCH_FDIV_S, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fdiv_d(DisasContext *ctx, arg_fdiv_d *a)
{
    gen_farith(ctx, OPC_LARCH_FDIV_D, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmax_s(DisasContext *ctx, arg_fmax_s *a)
{
    gen_farith(ctx, OPC_LARCH_FMAX_S, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmax_d(DisasContext *ctx, arg_fmax_d *a)
{
    gen_farith(ctx, OPC_LARCH_FMAX_D, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmin_s(DisasContext *ctx, arg_fmin_s *a)
{
    gen_farith(ctx, OPC_LARCH_FMIN_S, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmin_d(DisasContext *ctx, arg_fmin_d *a)
{
    gen_farith(ctx, OPC_LARCH_FMIN_D, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmaxa_s(DisasContext *ctx, arg_fmaxa_s *a)
{
    gen_farith(ctx, OPC_LARCH_FMAXA_S, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmaxa_d(DisasContext *ctx, arg_fmaxa_d *a)
{
    gen_farith(ctx, OPC_LARCH_FMAXA_D, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmina_s(DisasContext *ctx, arg_fmina_s *a)
{
    gen_farith(ctx, OPC_LARCH_FMINA_S, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmina_d(DisasContext *ctx, arg_fmina_d *a)
{
    gen_farith(ctx, OPC_LARCH_FMINA_D, a->fk, a->fj, a->fd, 0);
    return true;
}

static bool trans_fscaleb_s(DisasContext *ctx, arg_fscaleb_s *a)
{
    TCGv_i32 fp0 = tcg_temp_new_i32();
    TCGv_i32 fp1 = tcg_temp_new_i32();

    check_cp1_enabled(ctx);
    gen_load_fpr32(ctx, fp0, a->fj);
    gen_load_fpr32(ctx, fp1, a->fk);
    gen_helper_float_exp2_s(fp0, cpu_env, fp0, fp1);
    tcg_temp_free_i32(fp1);
    gen_store_fpr32(ctx, fp0, a->fd);
    tcg_temp_free_i32(fp0);
    return true;
}

static bool trans_fscaleb_d(DisasContext *ctx, arg_fscaleb_d *a)
{
    TCGv_i64 fp0 = tcg_temp_new_i64();
    TCGv_i64 fp1 = tcg_temp_new_i64();

    check_cp1_enabled(ctx);
    gen_load_fpr64(ctx, fp0, a->fj);
    gen_load_fpr64(ctx, fp1, a->fk);
    gen_helper_float_exp2_d(fp0, cpu_env, fp0, fp1);
    tcg_temp_free_i64(fp1);
    gen_store_fpr64(ctx, fp0, a->fd);
    tcg_temp_free_i64(fp0);
    return true;
}

static bool trans_fcopysign_s(DisasContext *ctx, arg_fcopysign_s *a)
{
    TCGv_i32 fp0 = tcg_temp_new_i32();
    TCGv_i32 fp1 = tcg_temp_new_i32();
    TCGv_i32 fp2 = tcg_temp_new_i32();

    check_cp1_enabled(ctx);
    gen_load_fpr32(ctx, fp0, a->fj);
    gen_load_fpr32(ctx, fp1, a->fk);
    tcg_gen_deposit_i32(fp2, fp1, fp0, 0, 31);
    gen_store_fpr32(ctx, fp2, a->fd);

    tcg_temp_free_i32(fp2);
    tcg_temp_free_i32(fp1);
    tcg_temp_free_i32(fp0);
    return true;
}

static bool trans_fcopysign_d(DisasContext *ctx, arg_fcopysign_d *a)
{
    TCGv_i64 fp0 = tcg_temp_new_i64();
    TCGv_i64 fp1 = tcg_temp_new_i64();
    TCGv_i64 fp2 = tcg_temp_new_i64();

    check_cp1_enabled(ctx);
    gen_load_fpr64(ctx, fp0, a->fj);
    gen_load_fpr64(ctx, fp1, a->fk);
    tcg_gen_deposit_i64(fp2, fp1, fp0, 0, 63);
    gen_store_fpr64(ctx, fp2, a->fd);

    tcg_temp_free_i64(fp2);
    tcg_temp_free_i64(fp1);
    tcg_temp_free_i64(fp0);
    return true;
}

static bool trans_fabs_s(DisasContext *ctx, arg_fabs_s *a)
{
    gen_farith(ctx, OPC_LARCH_FABS_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fabs_d(DisasContext *ctx, arg_fabs_d *a)
{
    gen_farith(ctx, OPC_LARCH_FABS_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fneg_s(DisasContext *ctx, arg_fneg_s *a)
{
    gen_farith(ctx, OPC_LARCH_FNEG_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fneg_d(DisasContext *ctx, arg_fneg_d *a)
{
    gen_farith(ctx, OPC_LARCH_FNEG_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_flogb_s(DisasContext *ctx, arg_flogb_s *a)
{
    TCGv_i32 fp0 = tcg_temp_new_i32();
    TCGv_i32 fp1 = tcg_temp_new_i32();

    check_cp1_enabled(ctx);
    gen_load_fpr32(ctx, fp0, a->fj);
    gen_helper_float_logb_s(fp1, cpu_env, fp0);
    gen_store_fpr32(ctx, fp1, a->fd);

    tcg_temp_free_i32(fp0);
    tcg_temp_free_i32(fp1);
    return true;
}

static bool trans_flogb_d(DisasContext *ctx, arg_flogb_d *a)
{
    TCGv_i64 fp0 = tcg_temp_new_i64();
    TCGv_i64 fp1 = tcg_temp_new_i64();

    check_cp1_enabled(ctx);
    gen_load_fpr64(ctx, fp0, a->fj);
    gen_helper_float_logb_d(fp1, cpu_env, fp0);
    gen_store_fpr64(ctx, fp1, a->fd);

    tcg_temp_free_i64(fp0);
    tcg_temp_free_i64(fp1);
    return true;
}

static bool trans_fclass_s(DisasContext *ctx, arg_fclass_s *a)
{
    gen_farith(ctx, OPC_LARCH_FCLASS_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fclass_d(DisasContext *ctx, arg_fclass_d *a)
{
    gen_farith(ctx, OPC_LARCH_FCLASS_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fsqrt_s(DisasContext *ctx, arg_fsqrt_s *a)
{
    gen_farith(ctx, OPC_LARCH_FSQRT_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fsqrt_d(DisasContext *ctx, arg_fsqrt_d *a)
{
    gen_farith(ctx, OPC_LARCH_FSQRT_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_frecip_s(DisasContext *ctx, arg_frecip_s *a)
{
    gen_farith(ctx, OPC_LARCH_FRECIP_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_frecip_d(DisasContext *ctx, arg_frecip_d *a)
{
    gen_farith(ctx, OPC_LARCH_FRECIP_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_frsqrt_s(DisasContext *ctx, arg_frsqrt_s *a)
{
    gen_farith(ctx, OPC_LARCH_FRSQRT_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_frsqrt_d(DisasContext *ctx, arg_frsqrt_d *a)
{
    gen_farith(ctx, OPC_LARCH_FRSQRT_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmov_s(DisasContext *ctx, arg_fmov_s *a)
{
    gen_farith(ctx, OPC_LARCH_FMOV_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fmov_d(DisasContext *ctx, arg_fmov_d *a)
{
    gen_farith(ctx, OPC_LARCH_FMOV_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_movgr2fr_w(DisasContext *ctx, arg_movgr2fr_w *a)
{
    gen_cp1(ctx, OPC_LARCH_GR2FR_W, a->rj, a->fd);
    return true;
}

static bool trans_movgr2fr_d(DisasContext *ctx, arg_movgr2fr_d *a)
{
    gen_cp1(ctx, OPC_LARCH_GR2FR_D, a->rj, a->fd);
    return true;
}

static bool trans_movgr2frh_w(DisasContext *ctx, arg_movgr2frh_w *a)
{
    gen_cp1(ctx, OPC_LARCH_GR2FRH_W, a->rj, a->fd);
    return true;
}

static bool trans_movfr2gr_s(DisasContext *ctx, arg_movfr2gr_s *a)
{
    gen_cp1(ctx, OPC_LARCH_FR2GR_S, a->rd, a->fj);
    return true;
}

static bool trans_movfr2gr_d(DisasContext *ctx, arg_movfr2gr_d *a)
{
    gen_cp1(ctx, OPC_LARCH_FR2GR_D, a->rd, a->fj);
    return true;
}

static bool trans_movfrh2gr_s(DisasContext *ctx, arg_movfrh2gr_s *a)
{
    gen_cp1(ctx, OPC_LARCH_FRH2GR_S, a->rd, a->fj);
    return true;
}

static bool trans_movgr2fcsr(DisasContext *ctx, arg_movgr2fcsr *a)
{
    TCGv t0 = tcg_temp_new();

    check_cp1_enabled(ctx);
    gen_load_gpr(t0, a->rj);
    save_cpu_state(ctx, 0);
    {
        TCGv_i32 fs_tmp = tcg_const_i32(a->fcsrd);
        gen_helper_0e2i(movgr2fcsr, t0, fs_tmp, a->rj);
        tcg_temp_free_i32(fs_tmp);
    }
    /* Stop translation as we may have changed hflags */
    ctx->base.is_jmp = DISAS_STOP;

    tcg_temp_free(t0);
    return true;
}

static bool trans_movfcsr2gr(DisasContext *ctx, arg_movfcsr2gr *a)
{
    TCGv t0 = tcg_temp_new();
    gen_helper_1e0i(movfcsr2gr, t0, a->fcsrs);
    gen_store_gpr(t0, a->rd);
    tcg_temp_free(t0);
    return true;
}

static bool trans_movfr2cf(DisasContext *ctx, arg_movfr2cf *a)
{
    TCGv_i64 fp0 = tcg_temp_new_i64();
    TCGv_i32 cd = tcg_const_i32(a->cd);

    check_cp1_enabled(ctx);
    gen_load_fpr64(ctx, fp0, a->fj);
    gen_helper_movreg2cf(cpu_env, cd, fp0);

    tcg_temp_free_i64(fp0);
    tcg_temp_free_i32(cd);
    return true;
}

static bool trans_movcf2fr(DisasContext *ctx, arg_movcf2fr *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv_i32 cj = tcg_const_i32(a->cj);

    check_cp1_enabled(ctx);
    gen_helper_movcf2reg(t0, cpu_env, cj);
    gen_store_fpr64(ctx, t0, a->fd);

    tcg_temp_free(t0);
    return true;
}

static bool trans_movgr2cf(DisasContext *ctx, arg_movgr2cf *a)
{
    TCGv t0 = tcg_temp_new();
    TCGv_i32 cd = tcg_const_i32(a->cd);

    check_cp1_enabled(ctx);
    gen_load_gpr(t0, a->rj);
    gen_helper_movreg2cf(cpu_env, cd, t0);

    tcg_temp_free(t0);
    tcg_temp_free_i32(cd);
    return true;
}

static bool trans_movcf2gr(DisasContext *ctx, arg_movcf2gr *a)
{
    TCGv_i32 cj = tcg_const_i32(a->cj);

    check_cp1_enabled(ctx);
    gen_helper_movcf2reg(cpu_gpr[a->rd], cpu_env, cj);

    tcg_temp_free_i32(cj);
    return true;
}

static bool trans_fcvt_s_d(DisasContext *ctx, arg_fcvt_s_d *a)
{
    gen_farith(ctx, OPC_LARCH_FCVT_S_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_fcvt_d_s(DisasContext *ctx, arg_fcvt_d_s *a)
{
    gen_farith(ctx, OPC_LARCH_FCVT_D_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrm_w_s(DisasContext *ctx, arg_ftintrm_l_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRM_W_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrm_w_d(DisasContext *ctx, arg_ftintrm_l_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRM_W_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrm_l_s(DisasContext *ctx, arg_ftintrm_l_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRM_L_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrm_l_d(DisasContext *ctx, arg_ftintrm_l_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRM_L_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrp_w_s(DisasContext *ctx, arg_ftintrp_w_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRP_W_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrp_w_d(DisasContext *ctx, arg_ftintrp_w_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRP_W_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrp_l_s(DisasContext *ctx, arg_ftintrp_l_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRP_L_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrp_l_d(DisasContext *ctx, arg_ftintrp_l_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRP_L_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrz_w_s(DisasContext *ctx, arg_ftintrz_w_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRZ_W_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrz_w_d(DisasContext *ctx, arg_ftintrz_w_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRZ_W_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrz_l_s(DisasContext *ctx, arg_ftintrz_l_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRZ_L_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrz_l_d(DisasContext *ctx, arg_ftintrz_l_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRZ_L_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrne_w_s(DisasContext *ctx, arg_ftintrne_w_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRNE_W_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrne_w_d(DisasContext *ctx, arg_ftintrne_w_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRNE_W_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrne_l_s(DisasContext *ctx, arg_ftintrne_l_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRNE_L_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftintrne_l_d(DisasContext *ctx, arg_ftintrne_l_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINTRNE_L_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftint_w_s(DisasContext *ctx, arg_ftint_w_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINT_W_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftint_w_d(DisasContext *ctx, arg_ftint_w_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINT_W_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftint_l_s(DisasContext *ctx, arg_ftint_l_s *a)
{
    gen_farith(ctx, OPC_LARCH_FTINT_L_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ftint_l_d(DisasContext *ctx, arg_ftint_l_d *a)
{
    gen_farith(ctx, OPC_LARCH_FTINT_L_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ffint_s_w(DisasContext *ctx, arg_ffint_s_w *a)
{
    gen_farith(ctx, OPC_LARCH_FFINT_S_W, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ffint_s_l(DisasContext *ctx, arg_ffint_s_l *a)
{
    gen_farith(ctx, OPC_LARCH_FFINT_S_L, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ffint_d_w(DisasContext *ctx, arg_ffint_d_w *a)
{
    gen_farith(ctx, OPC_LARCH_FFINT_D_W, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_ffint_d_l(DisasContext *ctx, arg_ffint_d_l *a)
{
    gen_farith(ctx, OPC_LARCH_FFINT_D_L, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_frint_s(DisasContext *ctx, arg_frint_s *a)
{
    gen_farith(ctx, OPC_LARCH_FRINT_S, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_frint_d(DisasContext *ctx, arg_frint_d *a)
{
    gen_farith(ctx, OPC_LARCH_FRINT_D, 0, a->fj, a->fd, 0);
    return true;
}

static bool trans_alsl_w(DisasContext *ctx, arg_alsl_w *a)
{
    gen_lsa(ctx, OPC_LARCH_ALSL_W, a->rd, a->rj, a->rk, a->sa2);
    return true;
}

static bool trans_alsl_wu(DisasContext *ctx, arg_alsl_wu *a)
{
    TCGv t0, t1;
    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);
    tcg_gen_shli_tl(t0, t0, a->sa2 + 1);
    tcg_gen_add_tl(t0, t0, t1);
    tcg_gen_ext32u_tl(cpu_gpr[a->rd], t0);
    tcg_temp_free(t0);
    tcg_temp_free(t1);

    return true;
}

static bool trans_alsl_d(DisasContext *ctx, arg_alsl_d *a)
{
    check_larch_64(ctx);
    gen_lsa(ctx, OPC_LARCH_ALSL_D, a->rd, a->rj, a->rk, a->sa2);
    return true;
}

static bool trans_bytepick_w(DisasContext *ctx, arg_bytepick_w *a)
{
    gen_align(ctx, 32, a->rd, a->rj, a->rk, a->sa2);
    return true;
}

static bool trans_bytepick_d(DisasContext *ctx, arg_bytepick_d *a)
{
    check_larch_64(ctx);
    gen_align(ctx, 64, a->rd, a->rj, a->rk, a->sa3);
    return true;
}

static bool trans_add_w(DisasContext *ctx, arg_add_w *a)
{
    gen_arith(ctx, OPC_LARCH_ADD_W, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_sub_w(DisasContext *ctx, arg_sub_w *a)
{
    gen_arith(ctx, OPC_LARCH_SUB_W, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_add_d(DisasContext *ctx, arg_add_d *a)
{
    gen_arith(ctx, OPC_LARCH_ADD_D, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_sub_d(DisasContext *ctx, arg_sub_d *a)
{
    check_larch_64(ctx);
    gen_arith(ctx, OPC_LARCH_SUB_D, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_slt(DisasContext *ctx, arg_slt *a)
{
    gen_slt(ctx, OPC_LARCH_SLT, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_sltu(DisasContext *ctx, arg_sltu *a)
{
    gen_slt(ctx, OPC_LARCH_SLTU, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_maskeqz(DisasContext *ctx, arg_maskeqz *a)
{
    gen_cond_move(ctx, OPC_LARCH_MASKEQZ, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_masknez(DisasContext *ctx, arg_masknez *a)
{
    gen_cond_move(ctx, OPC_LARCH_MASKNEZ, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_nor(DisasContext *ctx, arg_nor *a)
{
    gen_logic(ctx, OPC_LARCH_NOR, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_and(DisasContext *ctx, arg_and *a)
{
    gen_logic(ctx, OPC_LARCH_AND, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_or(DisasContext *ctx, arg_or *a)
{
    gen_logic(ctx, OPC_LARCH_OR, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_xor(DisasContext *ctx, arg_xor *a)
{
    gen_logic(ctx, OPC_LARCH_XOR, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_orn(DisasContext *ctx, arg_orn *a)
{
    TCGv t0 = tcg_temp_new();
    gen_load_gpr(t0, a->rk);
    tcg_gen_not_tl(t0, t0);
    tcg_gen_or_tl(cpu_gpr[a->rd], cpu_gpr[a->rj], t0);
    tcg_temp_free(t0);
    return true;
}

static bool trans_andn(DisasContext *ctx, arg_andn *a)
{
    TCGv t0, t1;
    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    gen_load_gpr(t0, a->rk);
    gen_load_gpr(t1, a->rj);
    tcg_gen_not_tl(t0, t0);
    tcg_gen_and_tl(cpu_gpr[a->rd], t1, t0);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

static bool trans_sll_w(DisasContext *ctx, arg_sll_w *a)
{
    gen_shift(ctx, OPC_LARCH_SLL_W, a->rd, a->rk, a->rj);
    return true;
}

static bool trans_srl_w(DisasContext *ctx, arg_srl_w *a)
{
    gen_shift(ctx, OPC_LARCH_SRL_W, a->rd, a->rk, a->rj);
    return true;
}

static bool trans_sra_w(DisasContext *ctx, arg_sra_w *a)
{
    gen_shift(ctx, OPC_LARCH_SRA_W, a->rd, a->rk, a->rj);
    return true;
}

static bool trans_sll_d(DisasContext *ctx, arg_sll_d *a)
{
    check_larch_64(ctx);
    gen_shift(ctx, OPC_LARCH_SLL_D, a->rd, a->rk, a->rj);
    return true;
}

static bool trans_srl_d(DisasContext *ctx, arg_srl_d *a)
{
    check_larch_64(ctx);
    gen_shift(ctx, OPC_LARCH_SRL_D, a->rd, a->rk, a->rj);
    return true;
}

static bool trans_sra_d(DisasContext *ctx, arg_sra_d *a)
{
    check_larch_64(ctx);
    gen_shift(ctx, OPC_LARCH_SRA_D, a->rd, a->rk, a->rj);
    return true;
}

static bool trans_rotr_w(DisasContext *ctx, arg_rotr_w *a)
{
    gen_shift(ctx, OPC_LARCH_ROTR_W, a->rd, a->rk, a->rj);
    return true;
}

static bool trans_rotr_d(DisasContext *ctx, arg_rotr_d *a)
{
    check_larch_64(ctx);
    gen_shift(ctx, OPC_LARCH_ROTR_D, a->rd, a->rk, a->rj);
    return true;
}

static bool trans_crc_w_b_w(DisasContext *ctx, arg_crc_w_b_w *a)
{
    gen_crc32(ctx, a->rd, a->rj, a->rk, 1, 0);
    return true;
}

static bool trans_crc_w_h_w(DisasContext *ctx, arg_crc_w_h_w *a)
{
    gen_crc32(ctx, a->rd, a->rj, a->rk, 2, 0);
    return true;
}

static bool trans_crc_w_w_w(DisasContext *ctx, arg_crc_w_w_w *a)
{
    gen_crc32(ctx, a->rd, a->rj, a->rk, 4, 0);
    return true;
}

static bool trans_crc_w_d_w(DisasContext *ctx, arg_crc_w_d_w *a)
{
    gen_crc32(ctx, a->rd, a->rj, a->rk, 8, 0);
    return true;
}

static bool trans_crcc_w_b_w(DisasContext *ctx, arg_crcc_w_b_w *a)
{
    gen_crc32(ctx, a->rd, a->rj, a->rk, 1, 1);
    return true;
}

static bool trans_crcc_w_h_w(DisasContext *ctx, arg_crcc_w_h_w *a)
{
    gen_crc32(ctx, a->rd, a->rj, a->rk, 2, 1);
    return true;
}

static bool trans_crcc_w_w_w(DisasContext *ctx, arg_crcc_w_w_w *a)
{
    gen_crc32(ctx, a->rd, a->rj, a->rk, 4, 1);
    return true;
}

static bool trans_crcc_w_d_w(DisasContext *ctx, arg_crcc_w_d_w *a)
{
    gen_crc32(ctx, a->rd, a->rj, a->rk, 8, 1);
    return true;
}

static bool trans_mul_w(DisasContext *ctx, arg_mul_w *a)
{
    gen_r6_muldiv(ctx, OPC_LARCH_MUL_W, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mulh_w(DisasContext *ctx, arg_mulh_w *a)
{
    gen_r6_muldiv(ctx, OPC_LARCH_MULH_W, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mulh_wu(DisasContext *ctx, arg_mulh_wu *a)
{
    gen_r6_muldiv(ctx, OPC_LARCH_MULH_WU, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mul_d(DisasContext *ctx, arg_mul_d *a)
{
    check_larch_64(ctx);
    gen_r6_muldiv(ctx, OPC_LARCH_MUL_D, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mulh_d(DisasContext *ctx, arg_mulh_d *a)
{
    check_larch_64(ctx);
    gen_r6_muldiv(ctx, OPC_LARCH_MULH_D, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mulh_du(DisasContext *ctx, arg_mulh_du *a)
{
    check_larch_64(ctx);
    gen_r6_muldiv(ctx, OPC_LARCH_MULH_DU, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mulw_d_w(DisasContext *ctx, arg_mulw_d_w *a)
{
    TCGv_i64 t0 = tcg_temp_new_i64();
    TCGv_i64 t1 = tcg_temp_new_i64();
    TCGv_i64 t2 = tcg_temp_new_i64();
    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);
    tcg_gen_ext32s_i64(t0, t0);
    tcg_gen_ext32s_i64(t1, t1);
    tcg_gen_mul_i64(t2, t0, t1);
    gen_store_gpr(t2, a->rd);
    tcg_temp_free_i64(t0);
    tcg_temp_free_i64(t1);
    tcg_temp_free_i64(t2);
    return true;
}

static bool trans_mulw_d_wu(DisasContext *ctx, arg_mulw_d_wu *a)
{
    TCGv_i64 t0 = tcg_temp_new_i64();
    TCGv_i64 t1 = tcg_temp_new_i64();
    TCGv_i64 t2 = tcg_temp_new_i64();
    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);
    tcg_gen_ext32u_i64(t0, t0);
    tcg_gen_ext32u_i64(t1, t1);
    tcg_gen_mul_i64(t2, t0, t1);
    gen_store_gpr(t2, a->rd);
    tcg_temp_free_i64(t0);
    tcg_temp_free_i64(t1);
    tcg_temp_free_i64(t2);
    return true;
}

static bool trans_div_w(DisasContext *ctx, arg_div_w *a)
{
    gen_r6_muldiv(ctx, OPC_LARCH_DIV_W, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mod_w(DisasContext *ctx, arg_mod_w *a)
{
    gen_r6_muldiv(ctx, OPC_LARCH_MOD_W, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_div_wu(DisasContext *ctx, arg_div_wu *a)
{
    gen_r6_muldiv(ctx, OPC_LARCH_DIV_WU, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mod_wu(DisasContext *ctx, arg_mod_wu *a)
{
    gen_r6_muldiv(ctx, OPC_LARCH_MOD_WU, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_div_d(DisasContext *ctx, arg_div_d *a)
{
    check_larch_64(ctx);
    gen_r6_muldiv(ctx, OPC_LARCH_DIV_D, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mod_d(DisasContext *ctx, arg_mod_d *a)
{
    check_larch_64(ctx);
    gen_r6_muldiv(ctx, OPC_LARCH_MOD_D, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_div_du(DisasContext *ctx, arg_div_du *a)
{
    check_larch_64(ctx);
    gen_r6_muldiv(ctx, OPC_LARCH_DIV_DU, a->rd, a->rj, a->rk);
    return true;
}

static bool trans_mod_du(DisasContext *ctx, arg_mod_du *a)
{
    check_larch_64(ctx);
    gen_r6_muldiv(ctx, OPC_LARCH_MOD_DU, a->rd, a->rj, a->rk);
    return true;
}

/* do not update CP0.BadVaddr */
static bool trans_asrtle_d(DisasContext *ctx, arg_asrtle_d *a)
{
    TCGv t1 = tcg_temp_new();
    TCGv t2 = tcg_temp_new();
    gen_load_gpr(t1, a->rj);
    gen_load_gpr(t2, a->rk);
    gen_helper_asrtle_d(cpu_env, t1, t2);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    return true;
}

/* do not update CP0.BadVaddr */
static bool trans_asrtgt_d(DisasContext *ctx, arg_asrtgt_d *a)
{
    TCGv t1 = tcg_temp_new();
    TCGv t2 = tcg_temp_new();
    gen_load_gpr(t1, a->rj);
    gen_load_gpr(t2, a->rk);
    gen_helper_asrtgt_d(cpu_env, t1, t2);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    return true;
}

#ifdef CONFIG_USER_ONLY
static bool trans_gr2scr(DisasContext *ctx, arg_gr2scr *a)
{
    return false;
}

static bool trans_scr2gr(DisasContext *ctx, arg_scr2gr *a)
{
    return false;
}
#else
static bool trans_gr2scr(DisasContext *ctx, arg_gr2scr *a)
{
    TCGv_i32 sd = tcg_const_i32(a->sd);
    TCGv val = tcg_temp_new();
    check_lbt_enabled(ctx);
    gen_load_gpr(val, a->rj);
    gen_helper_store_scr(cpu_env, sd, val);
    tcg_temp_free_i32(sd);
    tcg_temp_free(val);
    return true;
}

static bool trans_scr2gr(DisasContext *ctx, arg_scr2gr *a)
{
    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    TCGv_i32 tsj = tcg_const_i32(a->sj);
    check_lbt_enabled(ctx);
    gen_helper_load_scr(cpu_gpr[a->rd], cpu_env, tsj);
    tcg_temp_free_i32(tsj);
    return true;
}
#endif

static bool trans_clo_w(DisasContext *ctx, arg_clo_w *a)
{
    gen_cl(ctx, OPC_LARCH_CLO_W, a->rd, a->rj);
    return true;
}

static bool trans_clz_w(DisasContext *ctx, arg_clz_w *a)
{
    gen_cl(ctx, OPC_LARCH_CLZ_W, a->rd, a->rj);
    return true;
}

static bool trans_cto_w(DisasContext *ctx, arg_cto_w *a)
{
    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    gen_helper_cto_w(cpu_gpr[a->rd], cpu_env, t0);

    tcg_temp_free(t0);
    return true;
}

static bool trans_ctz_w(DisasContext *ctx, arg_ctz_w *a)
{
    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    gen_helper_ctz_w(cpu_gpr[a->rd], cpu_env, t0);

    tcg_temp_free(t0);
    return true;
}

static bool trans_clo_d(DisasContext *ctx, arg_clo_d *a)
{
    check_larch_64(ctx);
    gen_cl(ctx, OPC_LARCH_CLO_D, a->rd, a->rj);
    return true;
}

static bool trans_clz_d(DisasContext *ctx, arg_clz_d *a)
{
    check_larch_64(ctx);
    gen_cl(ctx, OPC_LARCH_CLZ_D, a->rd, a->rj);
    return true;
}

static bool trans_cto_d(DisasContext *ctx, arg_cto_d *a)
{
    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    gen_helper_cto_d(cpu_gpr[a->rd], cpu_env, t0);

    tcg_temp_free(t0);
    return true;
}

static bool trans_ctz_d(DisasContext *ctx, arg_ctz_d *a)
{
    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    gen_helper_ctz_d(cpu_gpr[a->rd], cpu_env, t0);

    tcg_temp_free(t0);
    return true;
}

static bool trans_revb_2h(DisasContext *ctx, arg_revb_2h *a)
{
    gen_bshfl(ctx, OPC_LARCH_REVB_2H, a->rj, a->rd);
    return true;
}

static bool trans_revb_4h(DisasContext *ctx, arg_revb_4h *a)
{
    check_larch_64(ctx);
    gen_bshfl(ctx, OPC_LARCH_REVB_4H, a->rj, a->rd);
    return true;
}

static bool trans_revb_2w(DisasContext *ctx, arg_revb_2w *a)
{
    handle_rev32(ctx, a->rj, a->rd);
    return true;
}

static bool trans_revb_d(DisasContext *ctx, arg_revb_d *a)
{
    handle_rev64(ctx, a->rj, a->rd);
    return true;
}

static bool trans_revh_2w(DisasContext *ctx, arg_revh_2w *a)
{
    handle_rev16(ctx, a->rj, a->rd);
    return true;
}

static bool trans_revh_d(DisasContext *ctx, arg_revh_d *a)
{
    check_larch_64(ctx);
    gen_bshfl(ctx, OPC_LARCH_REVH_D, a->rj, a->rd);
    return true;
}

static bool trans_bitrev_4b(DisasContext *ctx, arg_bitrev_4b *a)
{
    gen_bitswap(ctx, OPC_LARCH_BREV_4B, a->rd, a->rj);
    return true;
}

static bool trans_bitrev_8b(DisasContext *ctx, arg_bitrev_8b *a)
{
    check_larch_64(ctx);
    gen_bitswap(ctx, OPC_LARCH_BREV_8B, a->rd, a->rj);
    return true;
}

static bool trans_bitrev_w(DisasContext *ctx, arg_bitrev_w *a)
{
    TCGv t0 = tcg_temp_new();
    gen_load_gpr(t0, a->rj);
    gen_helper_bitrev_w(cpu_gpr[a->rd], cpu_env, t0);
    tcg_temp_free(t0);
    return true;
}

static bool trans_bitrev_d(DisasContext *ctx, arg_bitrev_d *a)
{
    TCGv t0 = tcg_temp_new();
    gen_load_gpr(t0, a->rj);
    gen_helper_bitrev_d(cpu_gpr[a->rd], cpu_env, t0);
    tcg_temp_free(t0);
    return true;
}

static bool trans_ext_w_h(DisasContext *ctx, arg_ext_w_h *a)
{
    gen_bshfl(ctx, OPC_LARCH_EXT_WH, a->rj, a->rd);
    return true;
}

static bool trans_ext_w_b(DisasContext *ctx, arg_ext_w_b *a)
{
    gen_bshfl(ctx, OPC_LARCH_EXT_WB, a->rj, a->rd);
    return true;
}

static bool trans_srli_w(DisasContext *ctx, arg_srli_w *a)
{
    gen_shift_imm(ctx, OPC_LARCH_SRLI_W, a->rd, a->rj, a->ui5);
    return true;
}

static bool trans_srai_w(DisasContext *ctx, arg_srai_w *a)
{
    gen_shift_imm(ctx, OPC_LARCH_SRAI_W, a->rd, a->rj, a->ui5);
    return true;
}

static bool trans_srai_d(DisasContext *ctx, arg_srai_d *a)
{
    TCGv t0;
    check_larch_64(ctx);
    t0 = tcg_temp_new();
    gen_load_gpr(t0, a->rj);
    tcg_gen_sari_tl(cpu_gpr[a->rd], t0, a->ui6);
    tcg_temp_free(t0);
    return true;
}

static bool trans_rotri_w(DisasContext *ctx, arg_rotri_w *a)
{
    gen_shift_imm(ctx, OPC_LARCH_ROTRI_W, a->rd, a->rj, a->ui5);
    return true;
}

static bool trans_rotri_d(DisasContext *ctx, arg_rotri_d *a)
{
    TCGv t0;
    check_larch_64(ctx);
    t0 = tcg_temp_new();
    gen_load_gpr(t0, a->rj);
    tcg_gen_rotri_tl(cpu_gpr[a->rd], t0, a->ui6);
    tcg_temp_free(t0);
    return true;
}

static bool trans_fcmp_cond_s(DisasContext *ctx, arg_fcmp_cond_s *a)
{
    check_cp1_enabled(ctx);
    gen_fcmp_s(ctx, a->fcond, a->fk, a->fj, a->cd);
    return true;
}

static bool trans_fcmp_cond_d(DisasContext *ctx, arg_fcmp_cond_d *a)
{
    check_cp1_enabled(ctx);
    gen_fcmp_d(ctx, a->fcond, a->fk, a->fj, a->cd);
    return true;
}

static bool trans_fsel(DisasContext *ctx, arg_fsel *a)
{
    TCGv_i64 fj = tcg_temp_new_i64();
    TCGv_i64 fk = tcg_temp_new_i64();
    TCGv_i64 fd = tcg_temp_new_i64();
    TCGv_i32 ca = tcg_const_i32(a->ca);
    check_cp1_enabled(ctx);
    gen_load_fpr64(ctx, fj, a->fj);
    gen_load_fpr64(ctx, fk, a->fk);
    gen_helper_fsel(fd, cpu_env, fj, fk, ca);
    gen_store_fpr64(ctx, fd, a->fd);
    tcg_temp_free_i64(fj);
    tcg_temp_free_i64(fk);
    tcg_temp_free_i64(fd);
    tcg_temp_free_i32(ca);
    return true;
}

#include "cpu-csr.h"

#ifdef CONFIG_USER_ONLY

static bool trans_csrxchg(DisasContext *ctx, arg_csrxchg *a)
{
    return false;
}

#else

#define GEN_CSRRQ_CASE(name)                                                  \
    do {                                                                      \
    case LOONGARCH_CSR_##name:                                                \
        gen_csr_rdq(ctx, cpu_gpr[rd], LOONGARCH_CSR_##name);                  \
    } while (0)

static bool trans_csrrd(DisasContext *ctx, unsigned rd, unsigned csr)
{
    switch (csr) {
        GEN_CSRRQ_CASE(CRMD);
        break;
        GEN_CSRRQ_CASE(PRMD);
        break;
        GEN_CSRRQ_CASE(EUEN);
        break;
        GEN_CSRRQ_CASE(MISC);
        break;
        GEN_CSRRQ_CASE(ECFG);
        break;
        GEN_CSRRQ_CASE(ESTAT);
        break;
        GEN_CSRRQ_CASE(ERA);
        break;
        GEN_CSRRQ_CASE(BADV);
        break;
        GEN_CSRRQ_CASE(BADI);
        break;
        GEN_CSRRQ_CASE(EEPN);
        break;
        GEN_CSRRQ_CASE(TLBIDX);
        break;
        GEN_CSRRQ_CASE(TLBEHI);
        break;
        GEN_CSRRQ_CASE(TLBELO0);
        break;
        GEN_CSRRQ_CASE(TLBELO1);
        break;
        GEN_CSRRQ_CASE(TLBWIRED);
        break;
        GEN_CSRRQ_CASE(GTLBC);
        break;
        GEN_CSRRQ_CASE(TRGP);
        break;
        GEN_CSRRQ_CASE(ASID);
        break;
        GEN_CSRRQ_CASE(PGDL);
        break;
        GEN_CSRRQ_CASE(PGDH);
        break;
    case LOONGARCH_CSR_PGD:
        gen_helper_read_pgd(cpu_gpr[rd], cpu_env);
        break;
        GEN_CSRRQ_CASE(PWCTL0);
        break;
        GEN_CSRRQ_CASE(PWCTL1);
        break;
        GEN_CSRRQ_CASE(STLBPGSIZE);
        break;
        GEN_CSRRQ_CASE(RVACFG);
        break;
        GEN_CSRRQ_CASE(CPUID);
        break;
        GEN_CSRRQ_CASE(PRCFG1);
        break;
        GEN_CSRRQ_CASE(PRCFG2);
        break;
        GEN_CSRRQ_CASE(PRCFG3);
        break;
        GEN_CSRRQ_CASE(KS0);
        break;
        GEN_CSRRQ_CASE(KS1);
        break;
        GEN_CSRRQ_CASE(KS2);
        break;
        GEN_CSRRQ_CASE(KS3);
        break;
        GEN_CSRRQ_CASE(KS4);
        break;
        GEN_CSRRQ_CASE(KS5);
        break;
        GEN_CSRRQ_CASE(KS6);
        break;
        GEN_CSRRQ_CASE(KS7);
        break;
        GEN_CSRRQ_CASE(KS8);
        break;
        GEN_CSRRQ_CASE(TMID);
        break;
        GEN_CSRRQ_CASE(TCFG);
        break;
        GEN_CSRRQ_CASE(TVAL);
        break;
        GEN_CSRRQ_CASE(CNTC);
        break;
        GEN_CSRRQ_CASE(TINTCLR);
        break;
        GEN_CSRRQ_CASE(GSTAT);
        break;
        GEN_CSRRQ_CASE(GCFG);
        break;
        GEN_CSRRQ_CASE(GINTC);
        break;
        GEN_CSRRQ_CASE(GCNTC);
        break;
        GEN_CSRRQ_CASE(LLBCTL);
        break;
        GEN_CSRRQ_CASE(IMPCTL1);
        break;
        GEN_CSRRQ_CASE(IMPCTL2);
        break;
        GEN_CSRRQ_CASE(GNMI);
        break;
        GEN_CSRRQ_CASE(TLBRENT);
        break;
        GEN_CSRRQ_CASE(TLBRBADV);
        break;
        GEN_CSRRQ_CASE(TLBRERA);
        break;
        GEN_CSRRQ_CASE(TLBRSAVE);
        break;
        GEN_CSRRQ_CASE(TLBRELO0);
        break;
        GEN_CSRRQ_CASE(TLBRELO1);
        break;
        GEN_CSRRQ_CASE(TLBREHI);
        break;
        GEN_CSRRQ_CASE(TLBRPRMD);
        break;
        GEN_CSRRQ_CASE(ERRCTL);
        break;
        GEN_CSRRQ_CASE(ERRINFO);
        break;
        GEN_CSRRQ_CASE(ERRINFO1);
        break;
        GEN_CSRRQ_CASE(ERRENT);
        break;
        GEN_CSRRQ_CASE(ERRERA);
        break;
        GEN_CSRRQ_CASE(ERRSAVE);
        break;
        GEN_CSRRQ_CASE(CTAG);
        break;
        GEN_CSRRQ_CASE(DMWIN0);
        break;
        GEN_CSRRQ_CASE(DMWIN1);
        break;
        GEN_CSRRQ_CASE(DMWIN2);
        break;
        GEN_CSRRQ_CASE(DMWIN3);
        break;
        GEN_CSRRQ_CASE(PERFCTRL0);
        break;
        GEN_CSRRQ_CASE(PERFCNTR0);
        break;
        GEN_CSRRQ_CASE(PERFCTRL1);
        break;
        GEN_CSRRQ_CASE(PERFCNTR1);
        break;
        GEN_CSRRQ_CASE(PERFCTRL2);
        break;
        GEN_CSRRQ_CASE(PERFCNTR2);
        break;
        GEN_CSRRQ_CASE(PERFCTRL3);
        break;
        GEN_CSRRQ_CASE(PERFCNTR3);
        break;
        /* debug */
        GEN_CSRRQ_CASE(MWPC);
        break;
        GEN_CSRRQ_CASE(MWPS);
        break;
        GEN_CSRRQ_CASE(DB0ADDR);
        break;
        GEN_CSRRQ_CASE(DB0MASK);
        break;
        GEN_CSRRQ_CASE(DB0CTL);
        break;
        GEN_CSRRQ_CASE(DB0ASID);
        break;
        GEN_CSRRQ_CASE(DB1ADDR);
        break;
        GEN_CSRRQ_CASE(DB1MASK);
        break;
        GEN_CSRRQ_CASE(DB1CTL);
        break;
        GEN_CSRRQ_CASE(DB1ASID);
        break;
        GEN_CSRRQ_CASE(DB2ADDR);
        break;
        GEN_CSRRQ_CASE(DB2MASK);
        break;
        GEN_CSRRQ_CASE(DB2CTL);
        break;
        GEN_CSRRQ_CASE(DB2ASID);
        break;
        GEN_CSRRQ_CASE(DB3ADDR);
        break;
        GEN_CSRRQ_CASE(DB3MASK);
        break;
        GEN_CSRRQ_CASE(DB3CTL);
        break;
        GEN_CSRRQ_CASE(DB3ASID);
        break;
        GEN_CSRRQ_CASE(FWPC);
        break;
        GEN_CSRRQ_CASE(FWPS);
        break;
        GEN_CSRRQ_CASE(IB0ADDR);
        break;
        GEN_CSRRQ_CASE(IB0MASK);
        break;
        GEN_CSRRQ_CASE(IB0CTL);
        break;
        GEN_CSRRQ_CASE(IB0ASID);
        break;
        GEN_CSRRQ_CASE(IB1ADDR);
        break;
        GEN_CSRRQ_CASE(IB1MASK);
        break;
        GEN_CSRRQ_CASE(IB1CTL);
        break;
        GEN_CSRRQ_CASE(IB1ASID);
        break;
        GEN_CSRRQ_CASE(IB2ADDR);
        break;
        GEN_CSRRQ_CASE(IB2MASK);
        break;
        GEN_CSRRQ_CASE(IB2CTL);
        break;
        GEN_CSRRQ_CASE(IB2ASID);
        break;
        GEN_CSRRQ_CASE(IB3ADDR);
        break;
        GEN_CSRRQ_CASE(IB3MASK);
        break;
        GEN_CSRRQ_CASE(IB3CTL);
        break;
        GEN_CSRRQ_CASE(IB3ASID);
        break;
        GEN_CSRRQ_CASE(IB4ADDR);
        break;
        GEN_CSRRQ_CASE(IB4MASK);
        break;
        GEN_CSRRQ_CASE(IB4CTL);
        break;
        GEN_CSRRQ_CASE(IB4ASID);
        break;
        GEN_CSRRQ_CASE(IB5ADDR);
        break;
        GEN_CSRRQ_CASE(IB5MASK);
        break;
        GEN_CSRRQ_CASE(IB5CTL);
        break;
        GEN_CSRRQ_CASE(IB5ASID);
        break;
        GEN_CSRRQ_CASE(IB6ADDR);
        break;
        GEN_CSRRQ_CASE(IB6MASK);
        break;
        GEN_CSRRQ_CASE(IB6CTL);
        break;
        GEN_CSRRQ_CASE(IB6ASID);
        break;
        GEN_CSRRQ_CASE(IB7ADDR);
        break;
        GEN_CSRRQ_CASE(IB7MASK);
        break;
        GEN_CSRRQ_CASE(IB7CTL);
        break;
        GEN_CSRRQ_CASE(IB7ASID);
        break;
        GEN_CSRRQ_CASE(DEBUG);
        break;
        GEN_CSRRQ_CASE(DERA);
        break;
        GEN_CSRRQ_CASE(DESAVE);
        break;
    default:
        return false;
    }

#undef GEN_CSRRQ_CASE

    return true;
}

#define GEN_CSRWQ_CASE(name)                                                  \
    do {                                                                      \
    case LOONGARCH_CSR_##name:                                                \
        gen_csr_wrq(ctx, cpu_gpr[rd], LOONGARCH_CSR_##name);                  \
    } while (0)

static bool trans_csrwr(DisasContext *ctx, unsigned rd, unsigned csr)
{

    switch (csr) {
    case LOONGARCH_CSR_CRMD:
        save_cpu_state(ctx, 1);
        gen_csr_wrq(ctx, cpu_gpr[rd], LOONGARCH_CSR_CRMD);
        gen_save_pc(ctx->base.pc_next + 4);
        ctx->base.is_jmp = DISAS_EXIT;
        break;
        GEN_CSRWQ_CASE(PRMD);
        break;
    case LOONGARCH_CSR_EUEN:
        gen_csr_wrq(ctx, cpu_gpr[rd], LOONGARCH_CSR_EUEN);
        /* Stop translation */
        gen_save_pc(ctx->base.pc_next + 4);
        ctx->base.is_jmp = DISAS_EXIT;
        break;
        GEN_CSRWQ_CASE(MISC);
        break;
        GEN_CSRWQ_CASE(ECFG);
        break;
        GEN_CSRWQ_CASE(ESTAT);
        break;
        GEN_CSRWQ_CASE(ERA);
        break;
        GEN_CSRWQ_CASE(BADV);
        break;
        GEN_CSRWQ_CASE(BADI);
        break;
        GEN_CSRWQ_CASE(EEPN);
        break;
        GEN_CSRWQ_CASE(TLBIDX);
        break;
        GEN_CSRWQ_CASE(TLBEHI);
        break;
        GEN_CSRWQ_CASE(TLBELO0);
        break;
        GEN_CSRWQ_CASE(TLBELO1);
        break;
        GEN_CSRWQ_CASE(TLBWIRED);
        break;
        GEN_CSRWQ_CASE(GTLBC);
        break;
        GEN_CSRWQ_CASE(TRGP);
        break;
        GEN_CSRWQ_CASE(ASID);
        break;
        GEN_CSRWQ_CASE(PGDL);
        break;
        GEN_CSRWQ_CASE(PGDH);
        break;
        GEN_CSRWQ_CASE(PGD);
        break;
        GEN_CSRWQ_CASE(PWCTL0);
        break;
        GEN_CSRWQ_CASE(PWCTL1);
        break;
        GEN_CSRWQ_CASE(STLBPGSIZE);
        break;
        GEN_CSRWQ_CASE(RVACFG);
        break;
        GEN_CSRWQ_CASE(CPUID);
        break;
        GEN_CSRWQ_CASE(PRCFG1);
        break;
        GEN_CSRWQ_CASE(PRCFG2);
        break;
        GEN_CSRWQ_CASE(PRCFG3);
        break;
        GEN_CSRWQ_CASE(KS0);
        break;
        GEN_CSRWQ_CASE(KS1);
        break;
        GEN_CSRWQ_CASE(KS2);
        break;
        GEN_CSRWQ_CASE(KS3);
        break;
        GEN_CSRWQ_CASE(KS4);
        break;
        GEN_CSRWQ_CASE(KS5);
        break;
        GEN_CSRWQ_CASE(KS6);
        break;
        GEN_CSRWQ_CASE(KS7);
        break;
        GEN_CSRWQ_CASE(KS8);
        break;
        GEN_CSRWQ_CASE(TMID);
        break;
        GEN_CSRWQ_CASE(TCFG);
        break;
        GEN_CSRWQ_CASE(TVAL);
        break;
        GEN_CSRWQ_CASE(CNTC);
        break;
        GEN_CSRWQ_CASE(TINTCLR);
        break;
        GEN_CSRWQ_CASE(GSTAT);
        break;
        GEN_CSRWQ_CASE(GCFG);
        break;
        GEN_CSRWQ_CASE(GINTC);
        break;
        GEN_CSRWQ_CASE(GCNTC);
        break;
        GEN_CSRWQ_CASE(LLBCTL);
        break;
        GEN_CSRWQ_CASE(IMPCTL1);
        break;
        GEN_CSRWQ_CASE(IMPCTL2);
        break;
        GEN_CSRWQ_CASE(GNMI);
        break;
        GEN_CSRWQ_CASE(TLBRENT);
        break;
        GEN_CSRWQ_CASE(TLBRBADV);
        break;
        GEN_CSRWQ_CASE(TLBRERA);
        break;
        GEN_CSRWQ_CASE(TLBRSAVE);
        break;
        GEN_CSRWQ_CASE(TLBRELO0);
        break;
        GEN_CSRWQ_CASE(TLBRELO1);
        break;
        GEN_CSRWQ_CASE(TLBREHI);
        break;
        GEN_CSRWQ_CASE(TLBRPRMD);
        break;
        GEN_CSRWQ_CASE(ERRCTL);
        break;
        GEN_CSRWQ_CASE(ERRINFO);
        break;
        GEN_CSRWQ_CASE(ERRINFO1);
        break;
        GEN_CSRWQ_CASE(ERRENT);
        break;
        GEN_CSRWQ_CASE(ERRERA);
        break;
        GEN_CSRWQ_CASE(ERRSAVE);
        break;
        GEN_CSRWQ_CASE(CTAG);
        break;
        GEN_CSRWQ_CASE(DMWIN0);
        break;
        GEN_CSRWQ_CASE(DMWIN1);
        break;
        GEN_CSRWQ_CASE(DMWIN2);
        break;
        GEN_CSRWQ_CASE(DMWIN3);
        break;
        GEN_CSRWQ_CASE(PERFCTRL0);
        break;
        GEN_CSRWQ_CASE(PERFCNTR0);
        break;
        GEN_CSRWQ_CASE(PERFCTRL1);
        break;
        GEN_CSRWQ_CASE(PERFCNTR1);
        break;
        GEN_CSRWQ_CASE(PERFCTRL2);
        break;
        GEN_CSRWQ_CASE(PERFCNTR2);
        break;
        GEN_CSRWQ_CASE(PERFCTRL3);
        break;
        GEN_CSRWQ_CASE(PERFCNTR3);
        break;
        /* debug */
        GEN_CSRWQ_CASE(MWPC);
        break;
        GEN_CSRWQ_CASE(MWPS);
        break;
        GEN_CSRWQ_CASE(DB0ADDR);
        break;
        GEN_CSRWQ_CASE(DB0MASK);
        break;
        GEN_CSRWQ_CASE(DB0CTL);
        break;
        GEN_CSRWQ_CASE(DB0ASID);
        break;
        GEN_CSRWQ_CASE(DB1ADDR);
        break;
        GEN_CSRWQ_CASE(DB1MASK);
        break;
        GEN_CSRWQ_CASE(DB1CTL);
        break;
        GEN_CSRWQ_CASE(DB1ASID);
        break;
        GEN_CSRWQ_CASE(DB2ADDR);
        break;
        GEN_CSRWQ_CASE(DB2MASK);
        break;
        GEN_CSRWQ_CASE(DB2CTL);
        break;
        GEN_CSRWQ_CASE(DB2ASID);
        break;
        GEN_CSRWQ_CASE(DB3ADDR);
        break;
        GEN_CSRWQ_CASE(DB3MASK);
        break;
        GEN_CSRWQ_CASE(DB3CTL);
        break;
        GEN_CSRWQ_CASE(DB3ASID);
        break;
        GEN_CSRWQ_CASE(FWPC);
        break;
        GEN_CSRWQ_CASE(FWPS);
        break;
        GEN_CSRWQ_CASE(IB0ADDR);
        break;
        GEN_CSRWQ_CASE(IB0MASK);
        break;
        GEN_CSRWQ_CASE(IB0CTL);
        break;
        GEN_CSRWQ_CASE(IB0ASID);
        break;
        GEN_CSRWQ_CASE(IB1ADDR);
        break;
        GEN_CSRWQ_CASE(IB1MASK);
        break;
        GEN_CSRWQ_CASE(IB1CTL);
        break;
        GEN_CSRWQ_CASE(IB1ASID);
        break;
        GEN_CSRWQ_CASE(IB2ADDR);
        break;
        GEN_CSRWQ_CASE(IB2MASK);
        break;
        GEN_CSRWQ_CASE(IB2CTL);
        break;
        GEN_CSRWQ_CASE(IB2ASID);
        break;
        GEN_CSRWQ_CASE(IB3ADDR);
        break;
        GEN_CSRWQ_CASE(IB3MASK);
        break;
        GEN_CSRWQ_CASE(IB3CTL);
        break;
        GEN_CSRWQ_CASE(IB3ASID);
        break;
        GEN_CSRWQ_CASE(IB4ADDR);
        break;
        GEN_CSRWQ_CASE(IB4MASK);
        break;
        GEN_CSRWQ_CASE(IB4CTL);
        break;
        GEN_CSRWQ_CASE(IB4ASID);
        break;
        GEN_CSRWQ_CASE(IB5ADDR);
        break;
        GEN_CSRWQ_CASE(IB5MASK);
        break;
        GEN_CSRWQ_CASE(IB5CTL);
        break;
        GEN_CSRWQ_CASE(IB5ASID);
        break;
        GEN_CSRWQ_CASE(IB6ADDR);
        break;
        GEN_CSRWQ_CASE(IB6MASK);
        break;
        GEN_CSRWQ_CASE(IB6CTL);
        break;
        GEN_CSRWQ_CASE(IB6ASID);
        break;
        GEN_CSRWQ_CASE(IB7ADDR);
        break;
        GEN_CSRWQ_CASE(IB7MASK);
        break;
        GEN_CSRWQ_CASE(IB7CTL);
        break;
        GEN_CSRWQ_CASE(IB7ASID);
        break;
        GEN_CSRWQ_CASE(DEBUG);
        break;
        GEN_CSRWQ_CASE(DERA);
        break;
        GEN_CSRWQ_CASE(DESAVE);
        break;
    default:
        return false;
    }

#undef GEN_CSRWQ_CASE

    return true;
}

#define GEN_CSRXQ_CASE(name)                                                  \
    do {                                                                      \
    case LOONGARCH_CSR_##name:                                                \
        if (rd == 0) {                                                        \
            gen_csr_xchgq(ctx, zero, cpu_gpr[rj], LOONGARCH_CSR_##name);      \
        } else {                                                              \
            gen_csr_xchgq(ctx, cpu_gpr[rd], cpu_gpr[rj],                      \
                          LOONGARCH_CSR_##name);                              \
        }                                                                     \
    } while (0)

static bool trans_csrxchg(DisasContext *ctx, arg_csrxchg *a)
{
    unsigned rd, rj, csr;
    TCGv zero = tcg_const_tl(0);
    rd = a->rd;
    rj = a->rj;
    csr = a->csr;

    if (rj == 0) {
        return trans_csrrd(ctx, rd, csr);
    } else if (rj == 1) {
        return trans_csrwr(ctx, rd, csr);
    }

    switch (csr) {
    case LOONGARCH_CSR_CRMD:
        save_cpu_state(ctx, 1);
        if (rd == 0) {
            gen_csr_xchgq(ctx, zero, cpu_gpr[rj], LOONGARCH_CSR_CRMD);
        } else {
            gen_csr_xchgq(ctx, cpu_gpr[rd], cpu_gpr[rj], LOONGARCH_CSR_CRMD);
        }
        gen_save_pc(ctx->base.pc_next + 4);
        ctx->base.is_jmp = DISAS_EXIT;
        break;

        GEN_CSRXQ_CASE(PRMD);
        break;
    case LOONGARCH_CSR_EUEN:
        if (rd == 0) {
            gen_csr_xchgq(ctx, zero, cpu_gpr[rj], LOONGARCH_CSR_EUEN);
        } else {
            gen_csr_xchgq(ctx, cpu_gpr[rd], cpu_gpr[rj], LOONGARCH_CSR_EUEN);
        }
        /* Stop translation */
        gen_save_pc(ctx->base.pc_next + 4);
        ctx->base.is_jmp = DISAS_EXIT;
        break;
        GEN_CSRXQ_CASE(MISC);
        break;
        GEN_CSRXQ_CASE(ECFG);
        break;
        GEN_CSRXQ_CASE(ESTAT);
        break;
        GEN_CSRXQ_CASE(ERA);
        break;
        GEN_CSRXQ_CASE(BADV);
        break;
        GEN_CSRXQ_CASE(BADI);
        break;
        GEN_CSRXQ_CASE(EEPN);
        break;
        GEN_CSRXQ_CASE(TLBIDX);
        break;
        GEN_CSRXQ_CASE(TLBEHI);
        break;
        GEN_CSRXQ_CASE(TLBELO0);
        break;
        GEN_CSRXQ_CASE(TLBELO1);
        break;
        GEN_CSRXQ_CASE(TLBWIRED);
        break;
        GEN_CSRXQ_CASE(GTLBC);
        break;
        GEN_CSRXQ_CASE(TRGP);
        break;
        GEN_CSRXQ_CASE(ASID);
        break;
        GEN_CSRXQ_CASE(PGDL);
        break;
        GEN_CSRXQ_CASE(PGDH);
        break;
        GEN_CSRXQ_CASE(PGD);
        break;
        GEN_CSRXQ_CASE(PWCTL0);
        break;
        GEN_CSRXQ_CASE(PWCTL1);
        break;
        GEN_CSRXQ_CASE(STLBPGSIZE);
        break;
        GEN_CSRXQ_CASE(RVACFG);
        break;
        GEN_CSRXQ_CASE(CPUID);
        break;
        GEN_CSRXQ_CASE(PRCFG1);
        break;
        GEN_CSRXQ_CASE(PRCFG2);
        break;
        GEN_CSRXQ_CASE(PRCFG3);
        break;
        GEN_CSRXQ_CASE(KS0);
        break;
        GEN_CSRXQ_CASE(KS1);
        break;
        GEN_CSRXQ_CASE(KS2);
        break;
        GEN_CSRXQ_CASE(KS3);
        break;
        GEN_CSRXQ_CASE(KS4);
        break;
        GEN_CSRXQ_CASE(KS5);
        break;
        GEN_CSRXQ_CASE(KS6);
        break;
        GEN_CSRXQ_CASE(KS7);
        break;
        GEN_CSRXQ_CASE(KS8);
        break;
        GEN_CSRXQ_CASE(TMID);
        break;
        GEN_CSRXQ_CASE(TCFG);
        break;
        GEN_CSRXQ_CASE(TVAL);
        break;
        GEN_CSRXQ_CASE(CNTC);
        break;
        GEN_CSRXQ_CASE(TINTCLR);
        break;
        GEN_CSRXQ_CASE(GSTAT);
        break;
        GEN_CSRXQ_CASE(GCFG);
        break;
        GEN_CSRXQ_CASE(GINTC);
        break;
        GEN_CSRXQ_CASE(GCNTC);
        break;
        GEN_CSRXQ_CASE(LLBCTL);
        break;
        GEN_CSRXQ_CASE(IMPCTL1);
        break;
        GEN_CSRXQ_CASE(IMPCTL2);
        break;
        GEN_CSRXQ_CASE(GNMI);
        break;
        GEN_CSRXQ_CASE(TLBRENT);
        break;
        GEN_CSRXQ_CASE(TLBRBADV);
        break;
        GEN_CSRXQ_CASE(TLBRERA);
        break;
        GEN_CSRXQ_CASE(TLBRSAVE);
        break;
        GEN_CSRXQ_CASE(TLBRELO0);
        break;
        GEN_CSRXQ_CASE(TLBRELO1);
        break;
        GEN_CSRXQ_CASE(TLBREHI);
        break;
        GEN_CSRXQ_CASE(TLBRPRMD);
        break;
        GEN_CSRXQ_CASE(ERRCTL);
        break;
        GEN_CSRXQ_CASE(ERRINFO);
        break;
        GEN_CSRXQ_CASE(ERRINFO1);
        break;
        GEN_CSRXQ_CASE(ERRENT);
        break;
        GEN_CSRXQ_CASE(ERRERA);
        break;
        GEN_CSRXQ_CASE(ERRSAVE);
        break;
        GEN_CSRXQ_CASE(CTAG);
        break;
        GEN_CSRXQ_CASE(DMWIN0);
        break;
        GEN_CSRXQ_CASE(DMWIN1);
        break;
        GEN_CSRXQ_CASE(DMWIN2);
        break;
        GEN_CSRXQ_CASE(DMWIN3);
        break;
        GEN_CSRXQ_CASE(PERFCTRL0);
        break;
        GEN_CSRXQ_CASE(PERFCNTR0);
        break;
        GEN_CSRXQ_CASE(PERFCTRL1);
        break;
        GEN_CSRXQ_CASE(PERFCNTR1);
        break;
        GEN_CSRXQ_CASE(PERFCTRL2);
        break;
        GEN_CSRXQ_CASE(PERFCNTR2);
        break;
        GEN_CSRXQ_CASE(PERFCTRL3);
        break;
        GEN_CSRXQ_CASE(PERFCNTR3);
        break;
        /* debug */
        GEN_CSRXQ_CASE(MWPC);
        break;
        GEN_CSRXQ_CASE(MWPS);
        break;
        GEN_CSRXQ_CASE(DB0ADDR);
        break;
        GEN_CSRXQ_CASE(DB0MASK);
        break;
        GEN_CSRXQ_CASE(DB0CTL);
        break;
        GEN_CSRXQ_CASE(DB0ASID);
        break;
        GEN_CSRXQ_CASE(DB1ADDR);
        break;
        GEN_CSRXQ_CASE(DB1MASK);
        break;
        GEN_CSRXQ_CASE(DB1CTL);
        break;
        GEN_CSRXQ_CASE(DB1ASID);
        break;
        GEN_CSRXQ_CASE(DB2ADDR);
        break;
        GEN_CSRXQ_CASE(DB2MASK);
        break;
        GEN_CSRXQ_CASE(DB2CTL);
        break;
        GEN_CSRXQ_CASE(DB2ASID);
        break;
        GEN_CSRXQ_CASE(DB3ADDR);
        break;
        GEN_CSRXQ_CASE(DB3MASK);
        break;
        GEN_CSRXQ_CASE(DB3CTL);
        break;
        GEN_CSRXQ_CASE(DB3ASID);
        break;
        GEN_CSRXQ_CASE(FWPC);
        break;
        GEN_CSRXQ_CASE(FWPS);
        break;
        GEN_CSRXQ_CASE(IB0ADDR);
        break;
        GEN_CSRXQ_CASE(IB0MASK);
        break;
        GEN_CSRXQ_CASE(IB0CTL);
        break;
        GEN_CSRXQ_CASE(IB0ASID);
        break;
        GEN_CSRXQ_CASE(IB1ADDR);
        break;
        GEN_CSRXQ_CASE(IB1MASK);
        break;
        GEN_CSRXQ_CASE(IB1CTL);
        break;
        GEN_CSRXQ_CASE(IB1ASID);
        break;
        GEN_CSRXQ_CASE(IB2ADDR);
        break;
        GEN_CSRXQ_CASE(IB2MASK);
        break;
        GEN_CSRXQ_CASE(IB2CTL);
        break;
        GEN_CSRXQ_CASE(IB2ASID);
        break;
        GEN_CSRXQ_CASE(IB3ADDR);
        break;
        GEN_CSRXQ_CASE(IB3MASK);
        break;
        GEN_CSRXQ_CASE(IB3CTL);
        break;
        GEN_CSRXQ_CASE(IB3ASID);
        break;
        GEN_CSRXQ_CASE(IB4ADDR);
        break;
        GEN_CSRXQ_CASE(IB4MASK);
        break;
        GEN_CSRXQ_CASE(IB4CTL);
        break;
        GEN_CSRXQ_CASE(IB4ASID);
        break;
        GEN_CSRXQ_CASE(IB5ADDR);
        break;
        GEN_CSRXQ_CASE(IB5MASK);
        break;
        GEN_CSRXQ_CASE(IB5CTL);
        break;
        GEN_CSRXQ_CASE(IB5ASID);
        break;
        GEN_CSRXQ_CASE(IB6ADDR);
        break;
        GEN_CSRXQ_CASE(IB6MASK);
        break;
        GEN_CSRXQ_CASE(IB6CTL);
        break;
        GEN_CSRXQ_CASE(IB6ASID);
        break;
        GEN_CSRXQ_CASE(IB7ADDR);
        break;
        GEN_CSRXQ_CASE(IB7MASK);
        break;
        GEN_CSRXQ_CASE(IB7CTL);
        break;
        GEN_CSRXQ_CASE(IB7ASID);
        break;
        GEN_CSRXQ_CASE(DEBUG);
        break;
        GEN_CSRXQ_CASE(DERA);
        break;
        GEN_CSRXQ_CASE(DESAVE);
        break;
    default:
        return false;
    }

#undef GEN_CSRXQ_CASE
    tcg_temp_free(zero);
    return true;
}

#endif

static bool trans_cacop(DisasContext *ctx, arg_cacop *a)
{
    /* Treat as NOP. */
    return true;
}

#ifdef CONFIG_USER_ONLY

static bool trans_ldpte(DisasContext *ctx, arg_ldpte *a)
{
    return false;
}

static bool trans_lddir(DisasContext *ctx, arg_lddir *a)
{
    return false;
}

static bool trans_iocsrrd_b(DisasContext *ctx, arg_iocsrrd_b *a)
{
    return false;
}

static bool trans_iocsrrd_h(DisasContext *ctx, arg_iocsrrd_h *a)
{
    return false;
}

static bool trans_iocsrrd_w(DisasContext *ctx, arg_iocsrrd_w *a)
{
    return false;
}

static bool trans_iocsrrd_d(DisasContext *ctx, arg_iocsrrd_d *a)
{
    return false;
}

static bool trans_iocsrwr_b(DisasContext *ctx, arg_iocsrwr_b *a)
{
    return false;
}

static bool trans_iocsrwr_h(DisasContext *ctx, arg_iocsrwr_h *a)
{
    return false;
}

static bool trans_iocsrwr_w(DisasContext *ctx, arg_iocsrwr_w *a)
{
    return false;
}

static bool trans_iocsrwr_d(DisasContext *ctx, arg_iocsrwr_d *a)
{
    return false;
}
#else

static bool trans_ldpte(DisasContext *ctx, arg_ldpte *a)
{
    TCGv t0, t1;
    TCGv_i32 t2;
    t0 = tcg_const_tl(a->rj);
    t1 = tcg_const_tl(a->seq);
    t2 = tcg_const_i32(ctx->mem_idx);
    gen_helper_ldpte(cpu_env, t0, t1, t2);

    return true;
}

static bool trans_lddir(DisasContext *ctx, arg_lddir *a)
{
    TCGv t0, t1, t2;
    TCGv_i32 t3;
    t0 = tcg_const_tl(a->rj);
    t1 = tcg_const_tl(a->rd);
    t2 = tcg_const_tl(a->level);
    t3 = tcg_const_i32(ctx->mem_idx);
    gen_helper_lddir(cpu_env, t0, t1, t2, t3);

    return true;
}

static bool trans_iocsrrd_b(DisasContext *ctx, arg_iocsrrd_b *a)
{
    return false;
}

static bool trans_iocsrrd_h(DisasContext *ctx, arg_iocsrrd_h *a)
{
    return false;
}

static bool trans_iocsrrd_w(DisasContext *ctx, arg_iocsrrd_w *a)
{
    TCGv_i32 iocsr_op = tcg_const_i32(OPC_LARCH_LD_W);
    TCGv t0, t1;
    t0 = tcg_const_tl(a->rj);
    t1 = tcg_const_tl(a->rd);
    gen_helper_iocsr(cpu_env, t0, t1, iocsr_op);
    return true;
}

static bool trans_iocsrrd_d(DisasContext *ctx, arg_iocsrrd_d *a)
{
    TCGv_i32 iocsr_op = tcg_const_i32(OPC_LARCH_LD_D);
    TCGv t0, t1;
    t0 = tcg_const_tl(a->rj);
    t1 = tcg_const_tl(a->rd);
    gen_helper_iocsr(cpu_env, t0, t1, iocsr_op);
    return true;
}

static bool trans_iocsrwr_b(DisasContext *ctx, arg_iocsrwr_b *a)
{
    TCGv_i32 iocsr_op = tcg_const_i32(OPC_LARCH_ST_B);
    TCGv t0, t1;
    t0 = tcg_const_tl(a->rj);
    t1 = tcg_const_tl(a->rd);
    gen_helper_iocsr(cpu_env, t0, t1, iocsr_op);
    return true;
}

static bool trans_iocsrwr_h(DisasContext *ctx, arg_iocsrwr_h *a)
{
    return false;
}

static bool trans_iocsrwr_w(DisasContext *ctx, arg_iocsrwr_w *a)
{
    TCGv_i32 iocsr_op = tcg_const_i32(OPC_LARCH_ST_W);
    TCGv t0, t1;
    t0 = tcg_const_tl(a->rj);
    t1 = tcg_const_tl(a->rd);
    gen_helper_iocsr(cpu_env, t0, t1, iocsr_op);
    return true;
}

static bool trans_iocsrwr_d(DisasContext *ctx, arg_iocsrwr_d *a)
{
    TCGv_i32 iocsr_op = tcg_const_i32(OPC_LARCH_ST_D);
    TCGv t0, t1;
    t0 = tcg_const_tl(a->rj);
    t1 = tcg_const_tl(a->rd);
    gen_helper_iocsr(cpu_env, t0, t1, iocsr_op);
    return true;
}
#endif /* !CONFIG_USER_ONLY */

#ifdef CONFIG_USER_ONLY

#define GEN_FALSE_TRANS(name)                                                 \
    static bool trans_##name(DisasContext *ctx, arg_##name *a)                \
    {                                                                         \
        return false;                                                         \
    }

GEN_FALSE_TRANS(tlbclr)
GEN_FALSE_TRANS(invtlb)
GEN_FALSE_TRANS(tlbflush)
GEN_FALSE_TRANS(tlbsrch)
GEN_FALSE_TRANS(tlbrd)
GEN_FALSE_TRANS(tlbwr)
GEN_FALSE_TRANS(tlbfill)
GEN_FALSE_TRANS(ertn)

#else

static bool trans_tlbclr(DisasContext *ctx, arg_tlbclr *a)
{
    gen_helper_tlbclr(cpu_env);
    return true;
}

static bool trans_tlbflush(DisasContext *ctx, arg_tlbflush *a)
{
    gen_helper_tlbflush(cpu_env);
    return true;
}

static bool trans_invtlb(DisasContext *ctx, arg_invtlb *a)
{
    TCGv addr = tcg_temp_new();
    TCGv info = tcg_temp_new();
    TCGv op = tcg_const_tl(a->invop);

    gen_load_gpr(addr, a->addr);
    gen_load_gpr(info, a->info);
    gen_helper_invtlb(cpu_env, addr, info, op);

    tcg_temp_free(addr);
    tcg_temp_free(info);
    tcg_temp_free(op);
    return true;
}

static bool trans_tlbsrch(DisasContext *ctx, arg_tlbsrch *a)
{
    gen_helper_tlbsrch(cpu_env);
    return true;
}

static bool trans_tlbrd(DisasContext *ctx, arg_tlbrd *a)
{
    gen_helper_tlbrd(cpu_env);
    return true;
}

static bool trans_tlbwr(DisasContext *ctx, arg_tlbwr *a)
{
    gen_helper_tlbwr(cpu_env);
    return true;
}

static bool trans_tlbfill(DisasContext *ctx, arg_tlbfill *a)
{
    gen_helper_tlbfill(cpu_env);
    return true;
}

static bool trans_ertn(DisasContext *ctx, arg_ertn *a)
{
    gen_helper_ertn(cpu_env);
    ctx->base.is_jmp = DISAS_EXIT;
    return true;
}

#endif /* CONFIG_USER_ONLY */

static bool trans_idle(DisasContext *ctx, arg_idle *a)
{
    ctx->base.pc_next += 4;
    save_cpu_state(ctx, 1);
    ctx->base.pc_next -= 4;
    gen_helper_idle(cpu_env);
    ctx->base.is_jmp = DISAS_NORETURN;
    return true;
}

#ifdef CONFIG_USER_ONLY

static bool trans_rdtime_d(DisasContext *ctx, arg_rdtime_d *a)
{
    /* Nop */
    return true;
}

#else

static bool trans_rdtime_d(DisasContext *ctx, arg_rdtime_d *a)
{
    TCGv t0, t1;
    t0 = tcg_const_tl(a->rd);
    t1 = tcg_const_tl(a->rj);
    gen_helper_drdtime(cpu_env, t0, t1);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    return true;
}

#endif

static bool trans_cpucfg(DisasContext *ctx, arg_cpucfg *a)
{
    TCGv t0 = tcg_temp_new();
    gen_load_gpr(t0, a->rj);
    gen_helper_cpucfg(cpu_gpr[a->rd], cpu_env, t0);
    tcg_temp_free(t0);
    return true;
}
