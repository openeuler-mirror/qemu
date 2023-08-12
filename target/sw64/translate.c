#include "translate.h"
#include "tcg/tcg.h"
#define DEVELOP_SW64 1
#ifdef DEVELOP_SW64

#define ILLEGAL(x)                                              \
    do {                                                        \
        printf("Illegal SW64 0x%x at line %d!\n", x, __LINE__); \
        exit(-1);                                               \
    } while (0)
#endif

TCGv cpu_pc;
TCGv cpu_std_ir[31];
TCGv cpu_fr[128];
TCGv cpu_lock_addr;
TCGv cpu_lock_flag;
TCGv cpu_lock_success;
#ifdef SW64_FIXLOCK
TCGv cpu_lock_value;
#endif

#ifndef CONFIG_USER_ONLY
TCGv cpu_hm_ir[31];
#endif

#include "exec/gen-icount.h"

void sw64_translate_init(void)
{
#define DEF_VAR(V) \
    { &cpu_##V, #V, offsetof(CPUSW64State, V) }

    typedef struct {
        TCGv* var;
        const char* name;
        int ofs;
    } GlobalVar;

    static const GlobalVar vars[] = {
        DEF_VAR(pc),         DEF_VAR(lock_addr),
        DEF_VAR(lock_flag),  DEF_VAR(lock_success),
#ifdef SW64_FIXLOCK
        DEF_VAR(lock_value),
#endif
    };
    cpu_pc = tcg_global_mem_new_i64(cpu_env,
                                    offsetof(CPUSW64State, pc), "PC");

#undef DEF_VAR

    /* Use the symbolic register names that match the disassembler.  */
    static const char ireg_names[31][4] = {
        "v0", "t0", "t1",  "t2",  "t3", "t4",  "t5", "t6", "t7", "s0", "s1",
        "s2", "s3", "s4",  "s5",  "fp", "a0",  "a1", "a2", "a3", "a4", "a5",
        "t8", "t9", "t10", "t11", "ra", "t12", "at", "gp", "sp"};

    static const char freg_names[128][4] = {
        "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",  "f8",  "f9",
        "f10", "f11", "f12", "f13", "f14", "f15", "f16", "f17", "f18", "f19",
        "f20", "f21", "f22", "f23", "f24", "f25", "f26", "f27", "f28", "f29",
        "f30", "f31", "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
        "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15", "f16", "f17",
        "f18", "f19", "f20", "f21", "f22", "f23", "f24", "f25", "f26", "f27",
        "f28", "f29", "f30", "f31", "f0",  "f1",  "f2",  "f3",  "f4",  "f5",
        "f6",  "f7",  "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
        "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23", "f24", "f25",
        "f26", "f27", "f28", "f29", "f30", "f31", "f0",  "f1",  "f2",  "f3",
        "f4",  "f5",  "f6",  "f7",  "f8",  "f9",  "f10", "f11", "f12", "f13",
        "f14", "f15", "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
        "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"};

#ifndef CONFIG_USER_ONLY
    static const char shadow_names[10][8] = {
        "hm_p1", "hm_p2",  "hm_p4",  "hm_p5",  "hm_p6",
        "hm_p7", "hm_p20", "hm_p21", "hm_p22", "hm_p23"};
    static const int shadow_index[10] = {1, 2, 4, 5, 6, 7, 20, 21, 22, 23};
#endif

    int i;

    for (i = 0; i < 31; i++) {
        cpu_std_ir[i] = tcg_global_mem_new_i64(
            cpu_env, offsetof(CPUSW64State, ir[i]), ireg_names[i]);
    }

    for (i = 0; i < 128; i++) {
        cpu_fr[i] = tcg_global_mem_new_i64(
            cpu_env, offsetof(CPUSW64State, fr[i]), freg_names[i]);
    }
    for (i = 0; i < ARRAY_SIZE(vars); ++i) {
        const GlobalVar* v = &vars[i];
        *v->var = tcg_global_mem_new_i64(cpu_env, v->ofs, v->name);
    }
#ifndef CONFIG_USER_ONLY
    memcpy(cpu_hm_ir, cpu_std_ir, sizeof(cpu_hm_ir));
    for (i = 0; i < 10; i++) {
        int r = shadow_index[i];
        cpu_hm_ir[r] = tcg_global_mem_new_i64(
            cpu_env, offsetof(CPUSW64State, sr[i]), shadow_names[i]);
    }
#endif
}

static bool in_superpage(DisasContext* ctx, int64_t addr)
{
    return false;
}

bool use_exit_tb(DisasContext* ctx)
{
    return ((tb_cflags(ctx->base.tb) & CF_LAST_IO) ||
            ctx->base.singlestep_enabled || singlestep);
}

bool use_goto_tb(DisasContext* ctx, uint64_t dest)
{
    /* Suppress goto_tb in the case of single-steping and IO.  */
    if (unlikely(use_exit_tb(ctx))) {
        return false;
    }
    /* If the destination is in the superpage, the page perms can't change.  */
    if (in_superpage(ctx, dest)) {
        return true;
    }
/* Check for the dest on the same page as the start of the TB.  */
#ifndef CONFIG_USER_ONLY
    return ((ctx->base.tb->pc ^ dest) & TARGET_PAGE_MASK) == 0;
#else
    return true;
#endif
}

void gen_fold_mzero(TCGCond cond, TCGv dest, TCGv src)
{
    uint64_t mzero = 1ull << 63;

    switch (cond) {
    case TCG_COND_LE:
    case TCG_COND_GT:
        /* For <= or >, the -0.0 value directly compares the way we want. */
        tcg_gen_mov_i64(dest, src);
        break;

    case TCG_COND_EQ:
    case TCG_COND_NE:
        /* For == or !=, we can simply mask off the sign bit and compare. */
        tcg_gen_andi_i64(dest, src, mzero - 1);
        break;

    case TCG_COND_GE:
    case TCG_COND_LT:
        /* For >= or <, map -0.0 to +0.0 via comparison and mask.  */
        tcg_gen_setcondi_i64(TCG_COND_NE, dest, src, mzero);
        tcg_gen_neg_i64(dest, dest);
        tcg_gen_and_i64(dest, dest, src);
        break;

    default:
        abort();
    }
}

static TCGv load_zero(DisasContext *ctx)
{
    if (!ctx->zero) {
        ctx->zero = tcg_const_i64(0);
    }
    return ctx->zero;
}

static void free_context_temps(DisasContext *ctx)
{
    if (ctx->zero) {
        tcg_temp_free(ctx->zero);
        ctx->zero = NULL;
    }
}

static TCGv load_gir(DisasContext *ctx, unsigned reg)
{
    if (likely(reg < 31)) {
        return ctx->ir[reg];
    } else {
        return load_zero(ctx);
    }
}

static void gen_excp_1(int exception, int error_code)
{
    TCGv_i32 tmp1, tmp2;

    tmp1 = tcg_const_i32(exception);
    tmp2 = tcg_const_i32(error_code);
    gen_helper_excp(cpu_env, tmp1, tmp2);
    tcg_temp_free_i32(tmp2);
    tcg_temp_free_i32(tmp1);
}

static DisasJumpType gen_excp(DisasContext* ctx, int exception,
                              int error_code)
{
    tcg_gen_movi_i64(cpu_pc, ctx->base.pc_next);
    gen_excp_1(exception, error_code);
    return DISAS_NORETURN;
}

static int i_count = 1;

static inline DisasJumpType gen_invalid(DisasContext *ctx)
{
    if (i_count == 0) {
        i_count++;
        return DISAS_NEXT;
    }
    fprintf(stderr, "here %lx\n", ctx->base.pc_next);
    return gen_excp(ctx, EXCP_OPCDEC, 0);
}

static uint64_t zapnot_mask(uint8_t byte_mask)
{
    uint64_t mask = 0;
    int i;

    for (i = 0; i < 8; ++i) {
        if ((byte_mask >> i) & 1) {
            mask |= 0xffull << (i * 8);
        }
    }
    return mask;
}

static void gen_ins_l(DisasContext* ctx, TCGv vc, TCGv va, TCGv vb,
                      uint8_t byte_mask)
{
    TCGv tmp = tcg_temp_new();
    TCGv shift = tcg_temp_new();

    tcg_gen_andi_i64(tmp, va, zapnot_mask(byte_mask));

    tcg_gen_andi_i64(shift, vb, 7);
    tcg_gen_shli_i64(shift, shift, 3);
    tcg_gen_shl_i64(vc, tmp, shift);

    tcg_temp_free(shift);
    tcg_temp_free(tmp);
}

static void gen_ins_h(DisasContext* ctx, TCGv vc, TCGv va, TCGv vb,
                      uint8_t byte_mask)
{
    TCGv tmp = tcg_temp_new();
    TCGv shift = tcg_temp_new();

    tcg_gen_andi_i64(tmp, va, zapnot_mask(byte_mask));

    tcg_gen_shli_i64(shift, vb, 3);
    tcg_gen_not_i64(shift, shift);
    tcg_gen_andi_i64(shift, shift, 0x3f);

    tcg_gen_shr_i64(vc, tmp, shift);
    tcg_gen_shri_i64(vc, vc, 1);
    tcg_temp_free(shift);
    tcg_temp_free(tmp);
}

static void gen_ext_l(DisasContext* ctx, TCGv vc, TCGv va, TCGv vb,
                      uint8_t byte_mask)
{
    TCGv tmp = tcg_temp_new();
    TCGv shift = tcg_temp_new();

    tcg_gen_andi_i64(shift, vb, 7);
    tcg_gen_shli_i64(shift, shift, 3);
    tcg_gen_shr_i64(tmp, va, shift);

    tcg_gen_andi_i64(vc, tmp, zapnot_mask(byte_mask));

    tcg_temp_free(shift);
    tcg_temp_free(tmp);
}

static void gen_ext_h(DisasContext* ctx, TCGv vc, TCGv va, TCGv vb,
                      uint8_t byte_mask)
{
    TCGv tmp = tcg_temp_new();
    TCGv shift = tcg_temp_new();

    tcg_gen_andi_i64(shift, vb, 7);
    tcg_gen_shli_i64(shift, shift, 3);
    tcg_gen_movi_i64(tmp, 64);
    tcg_gen_sub_i64(shift, tmp, shift);
    tcg_gen_shl_i64(tmp, va, shift);

    tcg_gen_andi_i64(vc, tmp, zapnot_mask(byte_mask));

    tcg_temp_free(shift);
    tcg_temp_free(tmp);
}

static void gen_mask_l(DisasContext* ctx, TCGv vc, TCGv va, TCGv vb,
                       uint8_t byte_mask)
{
    TCGv shift = tcg_temp_new();
    TCGv mask = tcg_temp_new();

    tcg_gen_andi_i64(shift, vb, 7);
    tcg_gen_shli_i64(shift, shift, 3);
    tcg_gen_movi_i64(mask, zapnot_mask(byte_mask));
    tcg_gen_shl_i64(mask, mask, shift);

    tcg_gen_andc_i64(vc, va, mask);

    tcg_temp_free(mask);
    tcg_temp_free(shift);
}

static void gen_mask_h(DisasContext *ctx, TCGv vc, TCGv va, TCGv vb,
                       uint8_t byte_mask)
{
    TCGv shift = tcg_temp_new();
    TCGv mask = tcg_temp_new();

    /* The instruction description is as above, where the byte_mask
       is shifted left, and then we extract bits <15:8>.  This can be
       emulated with a right-shift on the expanded byte mask.  This
       requires extra care because for an input <2:0> == 0 we need a
       shift of 64 bits in order to generate a zero.  This is done by
       splitting the shift into two parts, the variable shift - 1
       followed by a constant 1 shift.  The code we expand below is
       equivalent to ~(B * 8) & 63.  */

    tcg_gen_shli_i64(shift, vb, 3);
    tcg_gen_not_i64(shift, shift);
    tcg_gen_andi_i64(shift, shift, 0x3f);
    tcg_gen_movi_i64(mask, zapnot_mask(byte_mask));
    tcg_gen_shr_i64(mask, mask, shift);
    tcg_gen_shri_i64(mask, mask, 1);

    tcg_gen_andc_i64(vc, va, mask);

    tcg_temp_free(mask);
    tcg_temp_free(shift);
}

static inline void gen_load_mem(
    DisasContext *ctx, void (*tcg_gen_qemu_load)(TCGv t0, TCGv t1, int flags),
    int ra, int rb, int32_t disp16, bool fp, bool clear)
{
    TCGv tmp, addr, va;

    /* LDQ_U with ra $31 is UNOP.  Other various loads are forms of
       prefetches, which we can treat as nops.  No worries about
       missed exceptions here.  */
    if (unlikely(ra == 31)) {
        return;
    }

    tmp = tcg_temp_new();
    addr = load_gir(ctx, rb);

    if (disp16) {
        tcg_gen_addi_i64(tmp, addr, (int64_t)disp16);
        addr = tmp;
    } else {
        tcg_gen_mov_i64(tmp, addr);
        addr = tmp;
    }
    if (clear) {
        tcg_gen_andi_i64(tmp, addr, ~0x7UL);
        addr = tmp;
    }

    va = (fp ? cpu_fr[ra] : load_gir(ctx, ra));
    tcg_gen_qemu_load(va, addr, ctx->mem_idx);

    tcg_temp_free(tmp);
}

static inline void gen_store_mem(
    DisasContext *ctx, void (*tcg_gen_qemu_store)(TCGv t0, TCGv t1, int flags),
    int ra, int rb, int32_t disp16, bool fp, bool clear)
{
    TCGv tmp, addr, va;

    tmp = tcg_temp_new();
    addr = load_gir(ctx, rb);
    if (disp16) {
        tcg_gen_addi_i64(tmp, addr, disp16);
        addr = tmp;
    } else {
        tcg_gen_mov_i64(tmp, addr);
        addr = tmp;
    }
    if (clear) {
        tcg_gen_andi_i64(tmp, addr, ~0x7);
        addr = tmp;
    }
    va = (fp ? cpu_fr[ra] : load_gir(ctx, ra));

    tcg_gen_qemu_store(va, addr, ctx->mem_idx);
    gen_helper_trace_mem(cpu_env, addr, va);
    tcg_temp_free(tmp);
}

static void cal_with_iregs_2(DisasContext *ctx, TCGv vc, TCGv va, TCGv vb,
                             int32_t disp13, uint16_t fn)
{
    TCGv tmp;

    switch (fn & 0xff) {
    case 0x00:
        /* ADDW */
        tcg_gen_add_i64(vc, va, vb);
        tcg_gen_ext32s_i64(vc, vc);
        break;
    case 0x01:
        /* SUBW */
        tcg_gen_sub_i64(vc, va, vb);
        tcg_gen_ext32s_i64(vc, vc);
        break;
    case 0x02:
        /* S4ADDW */
        tmp = tcg_temp_new();
        tcg_gen_shli_i64(tmp, va, 2);
        tcg_gen_add_i64(tmp, tmp, vb);
        tcg_gen_ext32s_i64(vc, tmp);
        tcg_temp_free(tmp);
        break;
    case 0x03:
        /* S4SUBW */
        tmp = tcg_temp_new();
        tcg_gen_shli_i64(tmp, va, 2);
        tcg_gen_sub_i64(tmp, tmp, vb);
        tcg_gen_ext32s_i64(vc, tmp);
        tcg_temp_free(tmp);
        break;
    case 0x04:
        /* S8ADDW */
        tmp = tcg_temp_new();
        tcg_gen_shli_i64(tmp, va, 3);
        tcg_gen_add_i64(tmp, tmp, vb);
        tcg_gen_ext32s_i64(vc, tmp);
        tcg_temp_free(tmp);
        break;
    case 0x05:
        /* S8SUBW */
        tmp = tcg_temp_new();
        tcg_gen_shli_i64(tmp, va, 3);
        tcg_gen_sub_i64(tmp, tmp, vb);
        tcg_gen_ext32s_i64(vc, tmp);
        tcg_temp_free(tmp);
        break;

    case 0x08:
        /* ADDL */
        tcg_gen_add_i64(vc, va, vb);
        break;
    case 0x09:
        /* SUBL */
        tcg_gen_sub_i64(vc, va, vb);
        break;
    case 0x0a:
        /* S4ADDL */
        tmp = tcg_temp_new();
        tcg_gen_shli_i64(tmp, va, 2);
        tcg_gen_add_i64(vc, tmp, vb);
        tcg_temp_free(tmp);
        break;
    case 0x0b:
        /* S4SUBL */
        tmp = tcg_temp_new();
        tcg_gen_shli_i64(tmp, va, 2);
        tcg_gen_sub_i64(vc, tmp, vb);
        tcg_temp_free(tmp);
        break;
    case 0x0c:
        /* S8ADDL */
        tmp = tcg_temp_new();
        tcg_gen_shli_i64(tmp, va, 3);
        tcg_gen_add_i64(vc, tmp, vb);
        tcg_temp_free(tmp);
        break;
    case 0x0d:
        /* S8SUBL */
        tmp = tcg_temp_new();
        tcg_gen_shli_i64(tmp, va, 3);
        tcg_gen_sub_i64(vc, tmp, vb);
        tcg_temp_free(tmp);
        break;
    case 0x10:
        /* MULW */
        tcg_gen_mul_i64(vc, va, vb);
        tcg_gen_ext32s_i64(vc, vc);
        break;
    case 0x18:
        /* MULL */
        tcg_gen_mul_i64(vc, va, vb);
        break;
    case 0x19:
        /* MULH */
        tmp = tcg_temp_new();
        tcg_gen_mulu2_i64(tmp, vc, va, vb);
        tcg_temp_free(tmp);
        break;
    case 0x28:
        /* CMPEQ */
        tcg_gen_setcond_i64(TCG_COND_EQ, vc, va, vb);
        break;
    case 0x29:
        /* CMPLT */
        tcg_gen_setcond_i64(TCG_COND_LT, vc, va, vb);
        break;
    case 0x2a:
        /* CMPLE */
        tcg_gen_setcond_i64(TCG_COND_LE, vc, va, vb);
        break;
    case 0x2b:
        /* CMPULT */
        tcg_gen_setcond_i64(TCG_COND_LTU, vc, va, vb);
        break;
    case 0x2c:
        /* CMPULE */
        tcg_gen_setcond_i64(TCG_COND_LEU, vc, va, vb);
        break;
    case 0x38:
        /* AND */
        tcg_gen_and_i64(vc, va, vb);
        break;
    case 0x39:
        /* BIC */
        tcg_gen_andc_i64(vc, va, vb);
        break;
    case 0x3a:
        /* BIS */
        tcg_gen_or_i64(vc, va, vb);
        break;
    case 0x3b:
        /* ORNOT */
        tcg_gen_orc_i64(vc, va, vb);
        break;
    case 0x3c:
        /* XOR */
        tcg_gen_xor_i64(vc, va, vb);
        break;
    case 0x3d:
        /* EQV */
        tcg_gen_eqv_i64(vc, va, vb);
        break;
    case 0x40:
        /* INSLB */
        gen_ins_l(ctx, vc, va, vb, 0x1);
        break;
    case 0x41:
        /* INSLH */
        gen_ins_l(ctx, vc, va, vb, 0x3);
        break;
    case 0x42:
        /* INSLW */
        gen_ins_l(ctx, vc, va, vb, 0xf);
        break;
    case 0x43:
        /* INSLL */
        gen_ins_l(ctx, vc, va, vb, 0xff);
        break;
    case 0x44:
        /* INSHB */
        gen_ins_h(ctx, vc, va, vb, 0x1);
        break;
    case 0x45:
        /* INSHH */
        gen_ins_h(ctx, vc, va, vb, 0x3);
        break;
    case 0x46:
        /* INSHW */
        gen_ins_h(ctx, vc, va, vb, 0xf);
        break;
    case 0x47:
        /* INSHL */
        gen_ins_h(ctx, vc, va, vb, 0xff);
        break;
    case 0x48:
        /* SLL/SLLL */
        tmp = tcg_temp_new();
        tcg_gen_andi_i64(tmp, vb, 0x3f);
        tcg_gen_shl_i64(vc, va, tmp);
        tcg_temp_free(tmp);
        break;
    case 0x49:
        /* SRL/SRLL */
        tmp = tcg_temp_new();
        tcg_gen_andi_i64(tmp, vb, 0x3f);
        tcg_gen_shr_i64(vc, va, tmp);
        tcg_temp_free(tmp);
        break;
    case 0x4a:
        /* SRA/SRAL */
        tmp = tcg_temp_new();
        tcg_gen_andi_i64(tmp, vb, 0x3f);
        tcg_gen_sar_i64(vc, va, tmp);
        tcg_temp_free(tmp);
        break;
    case 0x50:
        /* EXTLB */
        gen_ext_l(ctx, vc, va, vb, 0x1);
        break;
    case 0x51:
        /* EXTLH */
        gen_ext_l(ctx, vc, va, vb, 0x3);
        break;
    case 0x52:
        /* EXTLW */
        gen_ext_l(ctx, vc, va, vb, 0xf);
        break;
    case 0x53:
        /* EXTLL */
        gen_ext_l(ctx, vc, va, vb, 0xff);
        break;
    case 0x54:
        /* EXTHB */
        gen_ext_h(ctx, vc, va, vb, 0x1);
        break;
    case 0x55:
        /* EXTHH */
        gen_ext_h(ctx, vc, va, vb, 0x3);
        break;
    case 0x56:
        /* EXTHW */
        gen_ext_h(ctx, vc, va, vb, 0xf);
        break;
    case 0x57:
        /* EXTHL */
        gen_ext_h(ctx, vc, va, vb, 0xff);
        break;
    case 0x58:
        /* CTPOP */
        tcg_gen_ctpop_i64(vc, vb);
        break;
    case 0x59:
        /* CTLZ */
        tcg_gen_clzi_i64(vc, vb, 64);
        break;
    case 0x5a:
        /* CTTZ */
        tcg_gen_ctzi_i64(vc, vb, 64);
        break;
    case 0x60:
        /* MASKLB */
        gen_mask_l(ctx, vc, va, vb, 0x1);
        break;
    case 0x61:
        /* MASKLH */
        gen_mask_l(ctx, vc, va, vb, 0x3);
        break;
    case 0x62:
        /* MASKLW */
        gen_mask_l(ctx, vc, va, vb, 0xf);
        break;
    case 0x63:
        /* MASKLL */
        gen_mask_l(ctx, vc, va, vb, 0xff);
        break;
    case 0x64:
        /* MASKHB */
        gen_mask_h(ctx, vc, va, vb, 0x1);
        break;
    case 0x65:
        /* MASKHH */
        gen_mask_h(ctx, vc, va, vb, 0x3);
        break;
    case 0x66:
        /* MASKHW */
        gen_mask_h(ctx, vc, va, vb, 0xf);
        break;
    case 0x67:
        /* MASKHL */
        gen_mask_h(ctx, vc, va, vb, 0xff);
        break;
    case 0x68:
        /* ZAP */
        gen_helper_zap(vc, va, vb);
        break;
    case 0x69:
        /* ZAPNOT */
        gen_helper_zapnot(vc, va, vb);
        break;
    case 0x6a:
        /* SEXTB */
        tcg_gen_ext8s_i64(vc, vb);
        break;
    case 0x6b:
        /* SEXTH */
        tcg_gen_ext16s_i64(vc, vb);
        break;
    case 0x6c:
        /* CMPGEB*/
        gen_helper_cmpgeb(vc, va, vb);
        break;
    default:
        ILLEGAL(fn);
    }
}

static void cal_with_imm_2(DisasContext *ctx, TCGv vc, TCGv va, int64_t disp,
                           uint8_t fn)
{
    TCGv_i64 t0 = tcg_const_i64(disp);
    cal_with_iregs_2(ctx, vc, va, t0, 0, fn);
    tcg_temp_free_i64(t0);
}

static void cal_with_iregs_3(DisasContext *ctx, TCGv vd, TCGv va, TCGv vb,
                             TCGv vc, uint8_t fn)
{
    TCGv_i64 t0 = tcg_const_i64(0);
    TCGv_i64 tmp;
    switch (fn) {
    case 0x0:
        /* SELEQ */
        tcg_gen_movcond_i64(TCG_COND_EQ, vd, va, t0, vb, vc);
        break;
    case 0x1:
        /* SELGE */
        tcg_gen_movcond_i64(TCG_COND_GE, vd, va, t0, vb, vc);
        break;
    case 0x2:
        /* SELGT */
        tcg_gen_movcond_i64(TCG_COND_GT, vd, va, t0, vb, vc);
        break;
    case 0x3:
        /* SELLE */
        tcg_gen_movcond_i64(TCG_COND_LE, vd, va, t0, vb, vc);
        break;
    case 0x4:
        /* SELLT */
        tcg_gen_movcond_i64(TCG_COND_LT, vd, va, t0, vb, vc);
        break;
    case 0x5:
        /* SELNE */
        tcg_gen_movcond_i64(TCG_COND_NE, vd, va, t0, vb, vc);
        break;
    case 0x6:
        /* SELLBC */
        tmp = tcg_temp_new_i64();
        tcg_gen_andi_i64(tmp, va, 1);
        tcg_gen_movcond_i64(TCG_COND_EQ, vd, tmp, t0, vb, vc);
        tcg_temp_free_i64(tmp);
        break;
    case 0x7:
        /* SELLBS */
        tmp = tcg_temp_new_i64();
        tcg_gen_andi_i64(tmp, va, 1);
        tcg_gen_movcond_i64(TCG_COND_NE, vd, tmp, t0, vb, vc);
        tcg_temp_free_i64(tmp);
        break;
    default:
        ILLEGAL(fn);
        break;
    }
    tcg_temp_free_i64(t0);
}

static void cal_with_imm_3(DisasContext *ctx, TCGv vd, TCGv va, int32_t disp,
                           TCGv vc, uint8_t fn)
{
    TCGv_i64 vb = tcg_const_i64(disp);
    cal_with_iregs_3(ctx, vd, va, vb, vc, fn);
    tcg_temp_free_i64(vb);
}

static DisasJumpType gen_bdirect(DisasContext *ctx, int ra, int32_t disp)
{
    uint64_t dest = ctx->base.pc_next + ((int64_t)disp << 2);
    if (ra != 31) {
        tcg_gen_movi_i64(load_gir(ctx, ra), ctx->base.pc_next & (~0x3UL));
    }
    if (disp == 0) {
        return 0;
    } else if (use_goto_tb(ctx, dest)) {
        tcg_gen_goto_tb(0);
        tcg_gen_movi_i64(cpu_pc, dest);
        tcg_gen_exit_tb(ctx->base.tb, 0);
        return DISAS_NORETURN;
    } else {
        tcg_gen_movi_i64(cpu_pc, dest);
        return DISAS_PC_UPDATED;
    }
}

static DisasJumpType gen_bcond_internal(DisasContext *ctx, TCGCond cond,
                                        TCGv cmp, int disp)
{
    uint64_t dest = ctx->base.pc_next + (disp << 2);
    TCGLabel* lab_true = gen_new_label();

    if (use_goto_tb(ctx, dest)) {
        tcg_gen_brcondi_i64(cond, cmp, 0, lab_true);

        tcg_gen_goto_tb(0);
        tcg_gen_movi_i64(cpu_pc, ctx->base.pc_next);
        tcg_gen_exit_tb(ctx->base.tb, 0);

        gen_set_label(lab_true);
        tcg_gen_goto_tb(1);
        tcg_gen_movi_i64(cpu_pc, dest);
        tcg_gen_exit_tb(ctx->base.tb, 1);

        return DISAS_NORETURN;
    } else {
        TCGv_i64 t = tcg_const_i64(0);
        TCGv_i64 d = tcg_const_i64(dest);
        TCGv_i64 p = tcg_const_i64(ctx->base.pc_next);

        tcg_gen_movcond_i64(cond, cpu_pc, cmp, t, d, p);

        tcg_temp_free_i64(t);
        tcg_temp_free_i64(d);
        tcg_temp_free_i64(p);
        return DISAS_PC_UPDATED;
    }
}

static DisasJumpType gen_bcond(DisasContext *ctx, TCGCond cond, uint32_t ra,
                               int32_t disp, uint64_t mask)
{
    TCGv tmp = tcg_temp_new();
    DisasJumpType ret;

    tcg_gen_andi_i64(tmp, load_gir(ctx, ra), mask);
    ret = gen_bcond_internal(ctx, cond, tmp, disp);
    tcg_temp_free(tmp);
    return ret;
}

static DisasJumpType gen_fbcond(DisasContext *ctx, TCGCond cond, int ra,
                                int32_t disp)
{
    TCGv cmp_tmp = tcg_temp_new();
    DisasJumpType ret;

    gen_fold_mzero(cond, cmp_tmp, cpu_fr[ra]);
    ret = gen_bcond_internal(ctx, cond, cmp_tmp, disp);
    tcg_temp_free(cmp_tmp);
    return ret;
}

#ifndef CONFIG_USER_ONLY
static void gen_qemu_pri_ldw(TCGv t0, TCGv t1, int memidx)
{
    gen_helper_pri_ldw(t0, cpu_env, t1);
}

static void gen_qemu_pri_stw(TCGv t0, TCGv t1, int memidx)
{
    gen_helper_pri_stw(cpu_env, t0, t1);
}

static void gen_qemu_pri_ldl(TCGv t0, TCGv t1, int memidx)
{
    gen_helper_pri_ldl(t0, cpu_env, t1);
}

static void gen_qemu_pri_stl(TCGv t0, TCGv t1, int memidx)
{
    gen_helper_pri_stl(cpu_env, t0, t1);
}
#endif

static inline void gen_load_mem_simd(
    DisasContext *ctx, void (*tcg_gen_qemu_load)(int t0, TCGv t1, int flags),
    int ra, int rb, int32_t disp16, uint64_t mask)
{
    TCGv tmp, addr;

    /* LDQ_U with ra $31 is UNOP.  Other various loads are forms of
       prefetches, which we can treat as nops.  No worries about
       missed exceptions here.  */
    if (unlikely(ra == 31))
        return;

    tmp = tcg_temp_new();
    addr = load_gir(ctx, rb);

    if (disp16) {
        tcg_gen_addi_i64(tmp, addr, (int64_t)disp16);
        addr = tmp;
    } else {
        tcg_gen_mov_i64(tmp, addr);
        addr = tmp;
    }

    if (mask) {
        tcg_gen_andi_i64(addr, addr, mask);
    }

    tcg_gen_qemu_load(ra, addr, ctx->mem_idx);
    // FIXME: for debug

    tcg_temp_free(tmp);
}

static inline void gen_store_mem_simd(
    DisasContext *ctx, void (*tcg_gen_qemu_store)(int t0, TCGv t1, int flags),
    int ra, int rb, int32_t disp16, uint64_t mask)
{
    TCGv tmp, addr;

    tmp = tcg_temp_new();
    addr = load_gir(ctx, rb);
    if (disp16) {
        tcg_gen_addi_i64(tmp, addr, (int64_t)disp16);
        addr = tmp;
    } else {
        tcg_gen_mov_i64(tmp, addr);
        addr = tmp;
    }
    if (mask) {
        tcg_gen_andi_i64(addr, addr, mask);
    }
    // FIXME: for debug
    tcg_gen_qemu_store(ra, addr, ctx->mem_idx);

    tcg_temp_free(tmp);
}

static void gen_qemu_ldwe(int t0, TCGv t1, int memidx)
{
    TCGv tmp = tcg_temp_new();

    tcg_gen_qemu_ld_i64(tmp, t1, memidx, MO_ALIGN_4 | MO_LEUL);
    tcg_gen_shli_i64(cpu_fr[t0], tmp, 32);
    tcg_gen_or_i64(cpu_fr[t0], cpu_fr[t0], tmp);
    tcg_gen_mov_i64(cpu_fr[t0 + 32], cpu_fr[t0]);
    tcg_gen_mov_i64(cpu_fr[t0 + 64], cpu_fr[t0]);
    tcg_gen_mov_i64(cpu_fr[t0 + 96], cpu_fr[t0]);

    tcg_temp_free(tmp);
}

static void gen_qemu_vlds(int t0, TCGv t1, int memidx)
{
    int i;
    TCGv_i32 tmp32 = tcg_temp_new_i32();

    tcg_gen_qemu_ld_i32(tmp32, t1, memidx, MO_ALIGN_4 | MO_LEUL);
    gen_helper_memory_to_s(cpu_fr[t0], tmp32);
    tcg_gen_addi_i64(t1, t1, 4);

    for (i = 1; i < 4; i++) {
        tcg_gen_qemu_ld_i32(tmp32, t1, memidx, MO_LEUL);
        gen_helper_memory_to_s(cpu_fr[t0 + i * 32], tmp32);
        tcg_gen_addi_i64(t1, t1, 4);
    }

    tcg_temp_free_i32(tmp32);
}

static void gen_qemu_ldse(int t0, TCGv t1, int memidx)
{
    TCGv_i32 tmp32 = tcg_temp_new_i32();
    TCGv tmp64 = tcg_temp_new();

    tcg_gen_qemu_ld_i32(tmp32, t1, memidx, MO_ALIGN_4 | MO_LEUL);
    gen_helper_memory_to_s(cpu_fr[t0], tmp32);
    tcg_gen_mov_i64(cpu_fr[t0 + 32], cpu_fr[t0]);
    tcg_gen_mov_i64(cpu_fr[t0 + 64], cpu_fr[t0]);
    tcg_gen_mov_i64(cpu_fr[t0 + 96], cpu_fr[t0]);

    tcg_temp_free(tmp64);
    tcg_temp_free_i32(tmp32);
}

static void gen_qemu_ldde(int t0, TCGv t1, int memidx)
{
    tcg_gen_qemu_ld_i64(cpu_fr[t0], t1, memidx, MO_ALIGN_4 | MO_TEQ);
    tcg_gen_mov_i64(cpu_fr[t0 + 32], cpu_fr[t0]);
    tcg_gen_mov_i64(cpu_fr[t0 + 64], cpu_fr[t0]);
    tcg_gen_mov_i64(cpu_fr[t0 + 96], cpu_fr[t0]);
}

static void gen_qemu_vldd(int t0, TCGv t1, int memidx)
{
    tcg_gen_qemu_ld_i64(cpu_fr[t0], t1, memidx, MO_ALIGN_4 | MO_TEQ);
    tcg_gen_addi_i64(t1, t1, 8);
    tcg_gen_qemu_ld_i64(cpu_fr[t0 + 32], t1, memidx, MO_TEQ);
    tcg_gen_addi_i64(t1, t1, 8);
    tcg_gen_qemu_ld_i64(cpu_fr[t0 + 64], t1, memidx, MO_TEQ);
    tcg_gen_addi_i64(t1, t1, 8);
    tcg_gen_qemu_ld_i64(cpu_fr[t0 + 96], t1, memidx, MO_TEQ);
}

static void gen_qemu_vsts(int t0, TCGv t1, int memidx)
{
    int i;
    TCGv_i32 tmp = tcg_temp_new_i32();

    gen_helper_s_to_memory(tmp, cpu_fr[t0]);
    tcg_gen_qemu_st_i32(tmp, t1, memidx, MO_ALIGN_4 | MO_LEUL);
    tcg_gen_addi_i64(t1, t1, 4);
    for (i = 1; i < 4; i++) {
        gen_helper_s_to_memory(tmp, cpu_fr[t0 + 32 * i]);
        tcg_gen_qemu_st_i32(tmp, t1, memidx, MO_LEUL);
        tcg_gen_addi_i64(t1, t1, 4);
    }
    tcg_temp_free_i32(tmp);
}

static void gen_qemu_vstd(int t0, TCGv t1, int memidx)
{
    tcg_gen_qemu_st_i64(cpu_fr[t0], t1, memidx, MO_ALIGN_4 | MO_TEQ);
    tcg_gen_addi_i64(t1, t1, 8);
    tcg_gen_qemu_st_i64(cpu_fr[t0 + 32], t1, memidx, MO_TEQ);
    tcg_gen_addi_i64(t1, t1, 8);
    tcg_gen_qemu_st_i64(cpu_fr[t0 + 64], t1, memidx, MO_TEQ);
    tcg_gen_addi_i64(t1, t1, 8);
    tcg_gen_qemu_st_i64(cpu_fr[t0 + 96], t1, memidx, MO_TEQ);
}

static inline void gen_qemu_fsts(TCGv t0, TCGv t1, int flags)
{
    TCGv_i32 tmp = tcg_temp_new_i32();
    gen_helper_s_to_memory(tmp, t0);
    tcg_gen_qemu_st_i32(tmp, t1, flags, MO_LEUL);
    tcg_temp_free_i32(tmp);
}

static inline void gen_qemu_flds(TCGv t0, TCGv t1, int flags)
{
    TCGv_i32 tmp = tcg_temp_new_i32();
    tcg_gen_qemu_ld_i32(tmp, t1, flags, MO_LEUL);
    gen_helper_memory_to_s(t0, tmp);
    tcg_temp_free_i32(tmp);
}

static TCGv gen_ieee_input(DisasContext *ctx, int reg, int is_cmp)
{
    TCGv val;

    if (unlikely(reg == 31)) {
        val = load_zero(ctx);
    } else {
        val = cpu_fr[reg];
#ifndef CONFIG_USER_ONLY
        /* In system mode, raise exceptions for denormals like real
           hardware.  In user mode, proceed as if the OS completion
           handler is handling the denormal as per spec.  */
        gen_helper_ieee_input(cpu_env, val);
#endif
    }
    return val;
}

static void gen_fp_exc_raise(int rc)
{
#ifndef CONFIG_USER_ONLY
    TCGv_i32 reg = tcg_const_i32(rc + 32);
    gen_helper_fp_exc_raise(cpu_env, reg);
    tcg_temp_free_i32(reg);
#endif
}

static void gen_ieee_arith2(DisasContext *ctx,
                            void (*helper)(TCGv, TCGv_ptr, TCGv), int ra,
                            int rc)
{
    TCGv va, vc;

    va = gen_ieee_input(ctx, ra, 0);
    vc = cpu_fr[rc];
    helper(vc, cpu_env, va);

    gen_fp_exc_raise(rc);
}

static void gen_ieee_arith3(DisasContext *ctx,
                            void (*helper)(TCGv, TCGv_ptr, TCGv, TCGv), int ra,
                            int rb, int rc)
{
    TCGv va, vb, vc;

    va = gen_ieee_input(ctx, ra, 0);
    vb = gen_ieee_input(ctx, rb, 0);
    vc = cpu_fr[rc];
    helper(vc, cpu_env, va, vb);

    gen_fp_exc_raise(rc);
}

#define IEEE_ARITH2(name)                                                     \
    static inline void glue(gen_, name)(DisasContext * ctx, int ra, int rc) { \
        gen_ieee_arith2(ctx, gen_helper_##name, ra, rc);                      \
    }

#define IEEE_ARITH3(name)                                                   \
    static inline void glue(gen_, name)(DisasContext * ctx, int ra, int rb, \
                                        int rc) {                           \
        gen_ieee_arith3(ctx, gen_helper_##name, ra, rb, rc);                \
    }
IEEE_ARITH3(fadds)
IEEE_ARITH3(faddd)
IEEE_ARITH3(fsubs)
IEEE_ARITH3(fsubd)
IEEE_ARITH3(fmuls)
IEEE_ARITH3(fmuld)
IEEE_ARITH3(fdivs)
IEEE_ARITH3(fdivd)
IEEE_ARITH2(frecs)
IEEE_ARITH2(frecd)

static void gen_ieee_compare(DisasContext *ctx,
                             void (*helper)(TCGv, TCGv_ptr, TCGv, TCGv), int ra,
                             int rb, int rc)
{
    TCGv va, vb, vc;

    va = gen_ieee_input(ctx, ra, 1);
    vb = gen_ieee_input(ctx, rb, 1);
    vc = cpu_fr[rc];
    helper(vc, cpu_env, va, vb);

    gen_fp_exc_raise(rc);
}

#define IEEE_CMP2(name)                                                     \
    static inline void glue(gen_, name)(DisasContext *ctx, int ra, int rb, \
                                        int rc) {                           \
        gen_ieee_compare(ctx, gen_helper_##name, ra, rb, rc);               \
    }

IEEE_CMP2(fcmpun)
IEEE_CMP2(fcmpeq)
IEEE_CMP2(fcmplt)
IEEE_CMP2(fcmple)

static void gen_fcvtdl(int rb, int rc, uint64_t round_mode)
{
    TCGv tmp64;
    tmp64 = tcg_temp_new_i64();
    tcg_gen_movi_i64(tmp64, round_mode);
    gen_helper_fcvtdl(cpu_fr[rc], cpu_env, cpu_fr[rb], tmp64);
    tcg_temp_free(tmp64);
    gen_fp_exc_raise(rc);
}

static void cal_with_fregs_2(DisasContext *ctx, uint8_t rc, uint8_t ra,
                             uint8_t rb, uint8_t fn)
{
    TCGv tmp64;
    TCGv_i32 tmp32;
    switch (fn) {
    case 0x00:
        /* FADDS */
        gen_fadds(ctx, ra, rb, rc);
        break;
    case 0x01:
        /* FADDD */
        gen_faddd(ctx, ra, rb, rc);
        break;
    case 0x02:
        /* FSUBS */
        gen_fsubs(ctx, ra, rb, rc);
        break;
    case 0x03:
        /* FSUBD */
        gen_fsubd(ctx, ra, rb, rc);
        break;
    case 0x4:
        /* FMULS */
        gen_fmuls(ctx, ra, rb, rc);
        break;
    case 0x05:
        /* FMULD */
        gen_fmuld(ctx, ra, rb, rc);
        break;
    case 0x06:
        /* FDIVS */
        gen_fdivs(ctx, ra, rb, rc);
        break;
    case 0x07:
        /* FDIVD */
        gen_fdivd(ctx, ra, rb, rc);
        break;
    case 0x08:
        /* FSQRTS */
        gen_helper_fsqrts(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        break;
    case 0x09:
        /* FSQRTD */
        gen_helper_fsqrt(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        break;
    case 0x10:
        /* FCMPEQ */
        gen_fcmpeq(ctx, ra, rb, rc);
        break;
    case 0x11:
        /* FCMPLE */
        gen_fcmple(ctx, ra, rb, rc);
        break;
    case 0x12:
        /* FCMPLT */
        gen_fcmplt(ctx, ra, rb, rc);
        break;
    case 0x13:
        /* FCMPUN */
        gen_fcmpun(ctx, ra, rb, rc);
        break;
    case 0x20:
        /* FCVTSD */
        gen_helper_fcvtsd(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        break;
    case 0x21:
        /* FCVTDS */
        gen_helper_fcvtds(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        break;
    case 0x22:
        /* FCVTDL_G */
        gen_fcvtdl(rb, rc, 0);
        break;
    case 0x23:
        /* FCVTDL_P */
        gen_fcvtdl(rb, rc, 2);
        break;
    case 0x24:
        /* FCVTDL_Z */
        gen_fcvtdl(rb, rc, 3);
        break;
    case 0x25:
        /* FCVTDL_N */
        gen_fcvtdl(rb, rc, 1);
        break;
    case 0x27:
        /* FCVTDL */
        gen_helper_fcvtdl_dyn(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        break;
    case 0x28:
        /* FCVTWL */
        gen_helper_fcvtwl(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        tcg_gen_ext32s_i64(cpu_fr[rc], cpu_fr[rc]);
        break;
    case 0x29:
        /* FCVTLW */
        gen_helper_fcvtlw(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        break;
    case 0x2d:
        /* FCVTLS */
        gen_helper_fcvtls(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        break;
    case 0x2f:
        /* FCVTLD */
        gen_helper_fcvtld(cpu_fr[rc], cpu_env, cpu_fr[rb]);
        break;
    case 0x30:
        /* FCPYS */
        tmp64 = tcg_temp_new();
        tcg_gen_shri_i64(tmp64, cpu_fr[ra], 63);
        tcg_gen_shli_i64(tmp64, tmp64, 63);
        tcg_gen_andi_i64(cpu_fr[rc], cpu_fr[rb], 0x7fffffffffffffffUL);
        tcg_gen_or_i64(cpu_fr[rc], tmp64, cpu_fr[rc]);
        tcg_temp_free(tmp64);
        break;
    case 0x31:
        /* FCPYSE */
        tmp64 = tcg_temp_new();
        tcg_gen_shri_i64(tmp64, cpu_fr[ra], 52);
        tcg_gen_shli_i64(tmp64, tmp64, 52);
        tcg_gen_andi_i64(cpu_fr[rc], cpu_fr[rb], 0x000fffffffffffffUL);
        tcg_gen_or_i64(cpu_fr[rc], tmp64, cpu_fr[rc]);
        tcg_temp_free(tmp64);
        break;
    case 0x32:
        /* FCPYSN */
        tmp64 = tcg_temp_new();
        tcg_gen_shri_i64(tmp64, cpu_fr[ra], 63);
        tcg_gen_not_i64(tmp64, tmp64);
        tcg_gen_shli_i64(tmp64, tmp64, 63);
        tcg_gen_andi_i64(cpu_fr[rc], cpu_fr[rb], 0x7fffffffffffffffUL);
        tcg_gen_or_i64(cpu_fr[rc], tmp64, cpu_fr[rc]);
        tcg_temp_free(tmp64);
        break;
    case 0x40:
        /* IFMOVS */
        tmp64 = tcg_temp_new();
        tmp32 = tcg_temp_new_i32();
        tcg_gen_movi_i64(tmp64, ra);
        tcg_gen_extrl_i64_i32(tmp32, load_gir(ctx, ra));
        gen_helper_memory_to_s(tmp64, tmp32);
        tcg_gen_mov_i64(cpu_fr[rc], tmp64);
        tcg_gen_movi_i64(tmp64, rc);
        tcg_temp_free(tmp64);
        tcg_temp_free_i32(tmp32);
        break;
    case 0x41:
        /* IFMOVD */
        tcg_gen_mov_i64(cpu_fr[rc], load_gir(ctx, ra));
        break;
    case 0x50:
        /* RFPCR */
        gen_helper_load_fpcr(cpu_fr[ra], cpu_env);
        break;
    case 0x51:
        /* WFPCR */
        gen_helper_store_fpcr(cpu_env, cpu_fr[ra]);
        break;
    case 0x54:
        /* SETFPEC0 */
        tmp64 = tcg_const_i64(0);
        gen_helper_setfpcrx(cpu_env, tmp64);
        tcg_temp_free(tmp64);
        break;
    case 0x55:
        /* SETFPEC1 */
        tmp64 = tcg_const_i64(1);
        gen_helper_setfpcrx(cpu_env, tmp64);
        tcg_temp_free(tmp64);
        break;
    case 0x56:
        /* SETFPEC2 */
        tmp64 = tcg_const_i64(2);
        gen_helper_setfpcrx(cpu_env, tmp64);
        tcg_temp_free(tmp64);
        break;
    case 0x57:
        /* SETFPEC3 */
        tmp64 = tcg_const_i64(3);
        gen_helper_setfpcrx(cpu_env, tmp64);
        tcg_temp_free(tmp64);
        break;
    default:
        fprintf(stderr, "Illegal insn func[%x]\n", fn);
        gen_invalid(ctx);
        break;
    }
}

static void cal_with_fregs_4(DisasContext *ctx, uint8_t rd, uint8_t ra,
                             uint8_t rb, uint8_t rc, uint8_t fn)
{
    TCGv zero = tcg_const_i64(0);
    TCGv va, vb, vc, vd, tmp64;

    va = cpu_fr[ra];
    vb = cpu_fr[rb];
    vc = cpu_fr[rc];
    vd = cpu_fr[rd];

    switch (fn) {
    case 0x00:
        /* FMAS */
        gen_helper_fmas(vd, cpu_env, va, vb, vc);
        break;
    case 0x01:
        /* FMAD */
        gen_helper_fmad(vd, cpu_env, va, vb, vc);
        break;
    case 0x02:
        /* FMSS */
        gen_helper_fmss(vd, cpu_env, va, vb, vc);
        break;
    case 0x03:
        /* FMSD */
        gen_helper_fmsd(vd, cpu_env, va, vb, vc);
        break;
    case 0x04:
        /* FNMAS */
        gen_helper_fnmas(vd, cpu_env, va, vb, vc);
        break;
    case 0x05:
        /* FNMAD */
        gen_helper_fnmad(vd, cpu_env, va, vb, vc);
        break;
    case 0x06:
        /* FNMSS */
        gen_helper_fnmss(vd, cpu_env, va, vb, vc);
        break;
    case 0x07:
        /* FNMSD */
        gen_helper_fnmsd(vd, cpu_env, va, vb, vc);
        break;
    case 0x10:
        /* FSELEQ */
        // Maybe wrong translation.
        tmp64 = tcg_temp_new();
        gen_helper_fcmpeq(tmp64, cpu_env, va, zero);
        tcg_gen_movcond_i64(TCG_COND_EQ, vd, tmp64, zero, vc, vb);
        tcg_temp_free(tmp64);
        break;
    case 0x11:
        /* FSELNE */
        tmp64 = tcg_temp_new();
        gen_helper_fcmpeq(tmp64, cpu_env, va, zero);
        tcg_gen_movcond_i64(TCG_COND_EQ, vd, tmp64, zero, vb, vc);
        tcg_temp_free(tmp64);
        break;
    case 0x12:
        /* FSELLT */
        tmp64 = tcg_temp_new();
        gen_helper_fcmplt(tmp64, cpu_env, va, zero);
        tcg_gen_movcond_i64(TCG_COND_EQ, vd, tmp64, zero, vc, vb);
        tcg_temp_free(tmp64);
        break;
    case 0x13:
        /* FSELLE */
        tmp64 = tcg_temp_new();
        gen_helper_fcmple(tmp64, cpu_env, va, zero);
        tcg_gen_movcond_i64(TCG_COND_EQ, vd, tmp64, zero, vc, vb);
        tcg_temp_free(tmp64);
        break;
    case 0x14:
        /* FSELGT */
        tmp64 = tcg_temp_new();
        gen_helper_fcmpgt(tmp64, cpu_env, va, zero);
        tcg_gen_movcond_i64(TCG_COND_NE, vd, tmp64, zero, vb, vc);
        tcg_temp_free(tmp64);
        break;
    case 0x15:
        /* FSELGE */
        tmp64 = tcg_temp_new();
        gen_helper_fcmpge(tmp64, cpu_env, va, zero);
        tcg_gen_movcond_i64(TCG_COND_NE, vd, tmp64, zero, vb, vc);
        tcg_temp_free(tmp64);
        break;
    default:
        fprintf(stderr, "Illegal insn func[%x]\n", fn);
        gen_invalid(ctx);
        break;
    }
    tcg_temp_free(zero);
}
static inline void gen_qemu_lldw(TCGv t0, TCGv t1, int flags)
{
    tcg_gen_qemu_ld_i64(t0, t1, flags, MO_LESL);
    tcg_gen_mov_i64(cpu_lock_addr, t1);
#ifdef SW64_FIXLOCK
    tcg_gen_ext32u_i64(cpu_lock_value, t0);
#endif
}

static inline void gen_qemu_lldl(TCGv t0, TCGv t1, int flags)
{
    tcg_gen_qemu_ld_i64(t0, t1, flags, MO_LEQ);
    tcg_gen_mov_i64(cpu_lock_addr, t1);
#ifdef SW64_FIXLOCK
    tcg_gen_mov_i64(cpu_lock_value, t0);
#endif
}

static DisasJumpType gen_store_conditional(DisasContext *ctx, int ra, int rb,
                                           int32_t disp16, int mem_idx,
                                           MemOp op)
{
    TCGLabel *lab_fail, *lab_done;
    TCGv addr;

    addr = tcg_temp_new_i64();
    tcg_gen_addi_i64(addr, load_gir(ctx, rb), disp16);
    free_context_temps(ctx);

    lab_fail = gen_new_label();
    lab_done = gen_new_label();
    tcg_gen_brcond_i64(TCG_COND_NE, addr, cpu_lock_addr, lab_fail);
    tcg_temp_free_i64(addr);
    tcg_gen_brcondi_i64(TCG_COND_NE, cpu_lock_flag, 0x1, lab_fail);
#ifdef SW64_FIXLOCK
    TCGv val = tcg_temp_new_i64();
    tcg_gen_atomic_cmpxchg_i64(val, cpu_lock_addr, cpu_lock_value,
                               load_gir(ctx, ra), mem_idx, op);
    tcg_gen_setcond_i64(TCG_COND_EQ, cpu_lock_success, val, cpu_lock_value);
    tcg_temp_free_i64(val);
#else
    tcg_gen_qemu_st_i64(load_gir(ctx, ra), addr, mem_idx, op);
#endif

    tcg_gen_br(lab_done);

    gen_set_label(lab_fail);
    tcg_gen_movi_i64(cpu_lock_success, 0);
    gen_set_label(lab_done);

    tcg_gen_movi_i64(cpu_lock_flag, 0);
    tcg_gen_movi_i64(cpu_lock_addr, -1);
    return DISAS_NEXT;
}

static DisasJumpType gen_sys_call(DisasContext *ctx, int syscode)
{
    if (syscode >= 0x80 && syscode <= 0xbf) {
        switch (syscode) {
        case 0x86:
            /* IMB */
            /* No-op inside QEMU */
            break;
#ifdef CONFIG_USER_ONLY
        case 0x9E:
            /* RDUNIQUE */
            tcg_gen_ld_i64(ctx->ir[IDX_V0], cpu_env,
                           offsetof(CPUSW64State, unique));
            break;
        case 0x9F:
            /* WRUNIQUE */
            tcg_gen_st_i64(ctx->ir[IDX_A0], cpu_env,
                           offsetof(CPUSW64State, unique));
            break;
#endif
        default:
            goto do_sys_call;
        }
        return DISAS_NEXT;
    }
do_sys_call:
#ifdef CONFIG_USER_ONLY
    return gen_excp(ctx, EXCP_CALL_SYS, syscode);
#else
    tcg_gen_movi_i64(cpu_hm_ir[23], ctx->base.pc_next);
    return gen_excp(ctx, EXCP_CALL_SYS, syscode);
#endif
}

static void read_csr(int idx, TCGv va)
{
    TCGv_i64 tmp = tcg_const_i64(idx);
    gen_helper_read_csr(va, cpu_env, tmp);
    tcg_temp_free_i64(tmp);
}

static void write_csr(int idx, TCGv va, CPUSW64State *env)
{
    TCGv_i64 tmp = tcg_const_i64(idx);
    gen_helper_write_csr(cpu_env, tmp, va);
    tcg_temp_free_i64(tmp);
}

static inline void ldx_set(DisasContext *ctx, int ra, int rb, int32_t disp12,
                           bool bype)
{
    TCGv tmp, addr, va, t1;

    /* LDQ_U with ra $31 is UNOP.  Other various loads are forms of
       prefetches, which we can treat as nops.  No worries about
       missed exceptions here.  */
    if (unlikely(ra == 31)) {
        return;
    }

    tmp = tcg_temp_new();
    t1 = tcg_const_i64(1);
    addr = load_gir(ctx, rb);

    tcg_gen_addi_i64(tmp, addr, disp12);
    addr = tmp;

    va = load_gir(ctx, ra);
    if (bype == 0) {
        tcg_gen_atomic_xchg_i64(va, addr, t1, ctx->mem_idx, MO_TESL);
    } else {
        tcg_gen_atomic_xchg_i64(va, addr, t1, ctx->mem_idx, MO_TEQ);
    }

    tcg_temp_free(tmp);
    tcg_temp_free(t1);
}

static inline void ldx_xxx(DisasContext *ctx, int ra, int rb, int32_t disp12,
                           bool bype, int64_t val)
{
    TCGv tmp, addr, va, t;

    /* LDQ_U with ra $31 is UNOP.  Other various loads are forms of
       prefetches, which we can treat as nops.  No worries about
       missed exceptions here.  */
    if (unlikely(ra == 31)) {
        return;
    }

    tmp = tcg_temp_new();
    t = tcg_const_i64(val);
    addr = load_gir(ctx, rb);

    tcg_gen_addi_i64(tmp, addr, disp12);
    addr = tmp;

    va = load_gir(ctx, ra);
    if (bype == 0) {
        tcg_gen_atomic_fetch_add_i64(va, addr, t, ctx->mem_idx, MO_TESL);
    } else {
        tcg_gen_atomic_fetch_add_i64(va, addr, t, ctx->mem_idx, MO_TEQ);
    }

    tcg_temp_free(tmp);
    tcg_temp_free(t);
}

static void tcg_gen_srlow_i64(int ra, int rc, int rb)
{
    TCGv va, vb, vc;
    TCGv shift;

    va = tcg_const_i64(ra);
    vc = tcg_const_i64(rc);
    shift = tcg_temp_new();
    vb = cpu_fr[rb];
    tcg_gen_shri_i64(shift, vb, 29);
    tcg_gen_andi_i64(shift, shift, 0xff);

    gen_helper_srlow(cpu_env, va, vc, shift);

    tcg_temp_free(vc);
    tcg_temp_free(va);
    tcg_temp_free(shift);
}

static void tcg_gen_srlowi_i64(int ra, int rc, int disp8)
{
    TCGv va, vc;
    TCGv shift;

    va = tcg_const_i64(ra);
    vc = tcg_const_i64(rc);
    shift = tcg_temp_new();
    tcg_gen_movi_i64(shift, disp8);
    tcg_gen_andi_i64(shift, shift, 0xff);

    gen_helper_srlow(cpu_env, va, vc, shift);

    tcg_temp_free(vc);
    tcg_temp_free(va);
    tcg_temp_free(shift);
}

static void tcg_gen_sllow_i64(int ra, int rc, int rb)
{
    TCGv va, vb, vc;
    TCGv shift;

    va = tcg_const_i64(ra);
    vc = tcg_const_i64(rc);
    shift = tcg_temp_new();
    vb = cpu_fr[rb];
    tcg_gen_shri_i64(shift, vb, 29);
    tcg_gen_andi_i64(shift, shift, 0xff);

    gen_helper_sllow(cpu_env, va, vc, shift);

    tcg_temp_free(vc);
    tcg_temp_free(va);
    tcg_temp_free(shift);
}

static void tcg_gen_sllowi_i64(int ra, int rc, int disp8)
{
    TCGv va, vc;
    TCGv shift;

    va = tcg_const_i64(ra);
    vc = tcg_const_i64(rc);
    shift = tcg_temp_new();
    tcg_gen_movi_i64(shift, disp8);
    tcg_gen_andi_i64(shift, shift, 0xff);

    gen_helper_sllow(cpu_env, va, vc, shift);

    tcg_temp_free(vc);
    tcg_temp_free(va);
    tcg_temp_free(shift);
}

static void gen_qemu_vstw_uh(int t0, TCGv t1, int memidx)
{
    TCGv byte4_len;
    TCGv addr_start, addr_end;
    TCGv tmp[8];
    TCGv ti;
    int i;

    tmp[0] = tcg_temp_new();
    tmp[1] = tcg_temp_new();
    tmp[2] = tcg_temp_new();
    tmp[3] = tcg_temp_new();
    tmp[4] = tcg_temp_new();
    tmp[5] = tcg_temp_new();
    tmp[6] = tcg_temp_new();
    tmp[7] = tcg_temp_new();
    ti = tcg_temp_new();
    addr_start = tcg_temp_new();
    addr_end = tcg_temp_new();
    byte4_len = tcg_temp_new();

    tcg_gen_shri_i64(byte4_len, t1, 2);
    tcg_gen_andi_i64(byte4_len, byte4_len, 0x7UL);
    tcg_gen_andi_i64(t1, t1, ~0x3UL); /* t1 = addr + byte4_len * 4 */
    tcg_gen_andi_i64(addr_start, t1, ~0x1fUL);
    tcg_gen_mov_i64(addr_end, t1);
    for (i = 7; i >= 0; i--) {
        tcg_gen_movcond_i64(TCG_COND_GEU, t1, t1, addr_start, t1, addr_start);
        tcg_gen_qemu_ld_i64(tmp[i], t1, memidx, MO_TEUL);
        tcg_gen_subi_i64(t1, t1, 4);
        tcg_gen_movi_i64(ti, i);
        if (i % 2)
            tcg_gen_shli_i64(tmp[i], tmp[i], 32);
    }
    tcg_gen_subfi_i64(byte4_len, 8, byte4_len);

    for (i = 0; i < 8; i++) {
        tcg_gen_movi_i64(ti, i);
        tcg_gen_movcond_i64(TCG_COND_GEU, tmp[i], ti, byte4_len, cpu_fr[t0 + (i / 2)*32], tmp[i]);
        if (i % 2)
            tcg_gen_shri_i64(tmp[i], tmp[i], 32);
        else
            tcg_gen_andi_i64(tmp[i], tmp[i], 0xffffffffUL);
    }

    tcg_gen_subi_i64(addr_end, addr_end, 32);
    for (i = 0; i < 8; i++) {
        tcg_gen_movcond_i64(TCG_COND_GEU, t1, addr_end, addr_start, addr_end, addr_start);
        tcg_gen_qemu_st_i64(tmp[i], t1, memidx, MO_TEUL);
        tcg_gen_addi_i64(addr_end, addr_end, 4);
    }

    tcg_temp_free(ti);
    tcg_temp_free(addr_start);
    tcg_temp_free(addr_end);
    tcg_temp_free(byte4_len);
    tcg_temp_free(tmp[0]);
    tcg_temp_free(tmp[1]);
    tcg_temp_free(tmp[2]);
    tcg_temp_free(tmp[3]);
    tcg_temp_free(tmp[4]);
    tcg_temp_free(tmp[5]);
    tcg_temp_free(tmp[6]);
    tcg_temp_free(tmp[7]);
}

static void gen_qemu_vstw_ul(int t0, TCGv t1, int memidx)
{
    TCGv byte4_len;
    TCGv addr_start, addr_end;
    TCGv tmp[8];
    TCGv ti;
    int i;

    tmp[0] = tcg_temp_new();
    tmp[1] = tcg_temp_new();
    tmp[2] = tcg_temp_new();
    tmp[3] = tcg_temp_new();
    tmp[4] = tcg_temp_new();
    tmp[5] = tcg_temp_new();
    tmp[6] = tcg_temp_new();
    tmp[7] = tcg_temp_new();
    ti = tcg_temp_new();
    addr_start = tcg_temp_new();
    addr_end = tcg_temp_new();
    byte4_len = tcg_temp_new();

    tcg_gen_shri_i64(byte4_len, t1, 2);
    tcg_gen_andi_i64(byte4_len, byte4_len, 0x7UL);
    tcg_gen_andi_i64(t1, t1, ~0x3UL); /* t1 = addr + byte4_len * 4 */
    tcg_gen_mov_i64(addr_start, t1); /* t1 = addr + byte4_len * 4 */
    tcg_gen_addi_i64(addr_end, addr_start, 24);
    for (i = 0; i < 8; i++) {
        tcg_gen_movcond_i64(TCG_COND_LEU, t1, t1, addr_end, t1, addr_end);
        tcg_gen_qemu_ld_i64(tmp[i], t1, memidx, MO_TEUL);
        tcg_gen_addi_i64(t1, t1, 4);
        if (i % 2)
            tcg_gen_shli_i64(tmp[i], tmp[i], 32);
    }
    tcg_gen_subfi_i64(byte4_len, 8, byte4_len);

    for (i = 0; i < 8; i++) {
        tcg_gen_movi_i64(ti, i);
        tcg_gen_movcond_i64(TCG_COND_LTU, tmp[i], ti, byte4_len, cpu_fr[t0 + (i/2)*32], tmp[i]);
        if (i % 2)
            tcg_gen_shri_i64(tmp[i], tmp[i], 32);
        else
            tcg_gen_andi_i64(tmp[i], tmp[i], 0xffffffffUL);
    }

    tcg_gen_addi_i64(addr_start, addr_start, 32);
    for (i = 7; i >= 0; i--) {
        tcg_gen_subi_i64(addr_start, addr_start, 4);
        tcg_gen_movcond_i64(TCG_COND_LEU, t1, addr_start, addr_end, addr_start, addr_end);
        tcg_gen_qemu_st_i64(tmp[i], t1, memidx, MO_TEUL);
    }

    tcg_temp_free(ti);
    tcg_temp_free(addr_start);
    tcg_temp_free(addr_end);
    tcg_temp_free(byte4_len);
    tcg_temp_free(tmp[0]);
    tcg_temp_free(tmp[1]);
    tcg_temp_free(tmp[2]);
    tcg_temp_free(tmp[3]);
    tcg_temp_free(tmp[4]);
    tcg_temp_free(tmp[5]);
    tcg_temp_free(tmp[6]);
    tcg_temp_free(tmp[7]);
}

static void gen_qemu_vsts_uh(int t0, TCGv t1, int memidx)
{
    TCGv byte4_len;
    TCGv addr_start, addr_end;
    TCGv tmp[4];
    TCGv ftmp;
    TCGv ti;
    int i;

    tmp[0] = tcg_temp_new();
    tmp[1] = tcg_temp_new();
    tmp[2] = tcg_temp_new();
    tmp[3] = tcg_temp_new();
    ti = tcg_temp_new();
    ftmp = tcg_temp_new();
    addr_start = tcg_temp_new();
    addr_end = tcg_temp_new();
    byte4_len = tcg_temp_new();

    tcg_gen_shri_i64(byte4_len, t1, 2);
    tcg_gen_andi_i64(byte4_len, byte4_len, 0x3UL);
    tcg_gen_andi_i64(t1, t1, ~0x3UL); /* t1 = addr + byte4_len * 4 */
    tcg_gen_andi_i64(addr_start, t1, ~0xfUL);
    tcg_gen_mov_i64(addr_end, t1);
    for (i = 3; i >= 0; i--) {
        tcg_gen_movcond_i64(TCG_COND_GEU, t1, t1, addr_start, t1, addr_start);
        tcg_gen_qemu_ld_i64(tmp[i], t1, memidx, MO_TEUL);
        tcg_gen_subi_i64(t1, t1, 4);
    }
    tcg_gen_subfi_i64(byte4_len, 4, byte4_len);

    for (i = 0; i < 4; i++) {
        tcg_gen_shri_i64(ti, cpu_fr[t0 + i * 32], 62);
        tcg_gen_shli_i64(ti, ti, 30);
        tcg_gen_shri_i64(ftmp, cpu_fr[t0 + i * 32], 29);
        tcg_gen_andi_i64(ftmp, ftmp, 0x3fffffffUL);
        tcg_gen_or_i64(ftmp, ftmp, ti);
        tcg_gen_movi_i64(ti, i);
        tcg_gen_movcond_i64(TCG_COND_GEU, tmp[i], ti, byte4_len, ftmp, tmp[i]);
    }

    tcg_gen_subi_i64(addr_end, addr_end, 16);
    for (i = 0; i < 4; i++) {
        tcg_gen_movcond_i64(TCG_COND_GEU, t1, addr_end, addr_start, addr_end, addr_start);
        tcg_gen_qemu_st_i64(tmp[i], t1, memidx, MO_TEUL);
        tcg_gen_addi_i64(addr_end, addr_end, 4);
    }

    tcg_temp_free(ti);
    tcg_temp_free(ftmp);
    tcg_temp_free(addr_start);
    tcg_temp_free(addr_end);
    tcg_temp_free(byte4_len);
    tcg_temp_free(tmp[0]);
    tcg_temp_free(tmp[1]);
    tcg_temp_free(tmp[2]);
    tcg_temp_free(tmp[3]);
}

static void gen_qemu_vsts_ul(int t0, TCGv t1, int memidx)
{
    TCGv byte4_len;
    TCGv addr_start, addr_end;
    TCGv tmp[4];
    TCGv ftmp;
    TCGv ti;
    int i;

    tmp[0] = tcg_temp_new();
    tmp[1] = tcg_temp_new();
    tmp[2] = tcg_temp_new();
    tmp[3] = tcg_temp_new();
    ftmp = tcg_temp_new();
    ti = tcg_temp_new();
    addr_start = tcg_temp_new();
    addr_end = tcg_temp_new();
    byte4_len = tcg_temp_new();

    tcg_gen_shri_i64(byte4_len, t1, 2);
    tcg_gen_andi_i64(byte4_len, byte4_len, 0x3UL);
    tcg_gen_andi_i64(t1, t1, ~0x3UL); /* t1 = addr + byte4_len * 4 */
    tcg_gen_mov_i64(addr_start, t1); /* t1 = addr + byte4_len * 4 */
    tcg_gen_addi_i64(addr_end, addr_start, 12);
    for (i = 0; i < 4; i++) {
        tcg_gen_movcond_i64(TCG_COND_LEU, t1, t1, addr_end, t1, addr_end);
        tcg_gen_qemu_ld_i64(tmp[i], t1, memidx, MO_TEUL);
        tcg_gen_addi_i64(t1, t1, 4);
    }
    tcg_gen_subfi_i64(byte4_len, 4, byte4_len);

    for (i = 0; i < 4; i++) {
        tcg_gen_shri_i64(ti, cpu_fr[t0 + i * 32], 62);
        tcg_gen_shli_i64(ti, ti, 30);
        tcg_gen_shri_i64(ftmp, cpu_fr[t0 + i * 32], 29);
        tcg_gen_andi_i64(ftmp, ftmp, 0x3fffffffUL);
        tcg_gen_or_i64(ftmp, ftmp, ti);
        tcg_gen_movi_i64(ti, i);
        tcg_gen_movcond_i64(TCG_COND_LTU, tmp[i], ti, byte4_len, ftmp, tmp[i]);
    }

    tcg_gen_addi_i64(addr_start, addr_start, 16);
    for (i = 3; i >= 0; i--) {
        tcg_gen_subi_i64(addr_start, addr_start, 4);
        tcg_gen_movcond_i64(TCG_COND_LEU, t1, addr_start, addr_end, addr_start, addr_end);
        tcg_gen_qemu_st_i64(tmp[i], t1, memidx, MO_TEUL);
    }

    tcg_temp_free(ti);
    tcg_temp_free(addr_start);
    tcg_temp_free(addr_end);
    tcg_temp_free(byte4_len);
    tcg_temp_free(ftmp);
    tcg_temp_free(tmp[0]);
    tcg_temp_free(tmp[1]);
    tcg_temp_free(tmp[2]);
    tcg_temp_free(tmp[3]);
}

static void gen_qemu_vstd_uh(int t0, TCGv t1, int memidx)
{
    TCGv byte8_len;
    TCGv addr_start, addr_end;
    TCGv tmp[4];
    TCGv ti;
    int i;

    tmp[0] = tcg_temp_new();
    tmp[1] = tcg_temp_new();
    tmp[2] = tcg_temp_new();
    tmp[3] = tcg_temp_new();
    ti = tcg_temp_new();
    addr_start = tcg_temp_new();
    addr_end = tcg_temp_new();
    byte8_len = tcg_temp_new();

    tcg_gen_shri_i64(byte8_len, t1, 3);
    tcg_gen_andi_i64(byte8_len, byte8_len, 0x3UL);
    tcg_gen_andi_i64(t1, t1, ~0x7UL); /* t1 = addr + byte4_len * 4 */
    tcg_gen_andi_i64(addr_start, t1, ~0x1fUL);
    tcg_gen_mov_i64(addr_end, t1);
    for (i = 3; i >= 0; i--) {
        tcg_gen_movcond_i64(TCG_COND_GEU, t1, t1, addr_start, t1, addr_start);
        tcg_gen_qemu_ld_i64(tmp[i], t1, memidx, MO_TEQ);
        tcg_gen_subi_i64(t1, t1, 8);
    }
    tcg_gen_subfi_i64(byte8_len, 4, byte8_len);

    for (i = 0; i < 4; i++) {
        tcg_gen_movi_i64(ti, i);
        tcg_gen_movcond_i64(TCG_COND_GEU, tmp[i], ti, byte8_len, cpu_fr[t0 + i*32], tmp[i]);
    }

    tcg_gen_subi_i64(addr_end, addr_end, 32);
    for (i = 0; i < 4; i++) {
        tcg_gen_movcond_i64(TCG_COND_GEU, t1, addr_end, addr_start, addr_end, addr_start);
        tcg_gen_qemu_st_i64(tmp[i], t1, memidx, MO_TEQ);
        tcg_gen_addi_i64(addr_end, addr_end, 8);
    }

    tcg_temp_free(ti);
    tcg_temp_free(addr_start);
    tcg_temp_free(addr_end);
    tcg_temp_free(byte8_len);
    tcg_temp_free(tmp[0]);
    tcg_temp_free(tmp[1]);
    tcg_temp_free(tmp[2]);
    tcg_temp_free(tmp[3]);
}

static void gen_qemu_vstd_ul(int t0, TCGv t1, int memidx)
{
    TCGv byte8_len;
    TCGv addr_start, addr_end;
    TCGv tmp[4];
    TCGv ti;
    int i;

    tmp[0] = tcg_temp_new();
    tmp[1] = tcg_temp_new();
    tmp[2] = tcg_temp_new();
    tmp[3] = tcg_temp_new();
    ti = tcg_temp_new();
    addr_start = tcg_temp_new();
    addr_end = tcg_temp_new();
    byte8_len = tcg_temp_new();

    tcg_gen_shri_i64(byte8_len, t1, 3);
    tcg_gen_andi_i64(byte8_len, byte8_len, 0x3UL);
    tcg_gen_andi_i64(t1, t1, ~0x7UL); /* t1 = addr + byte4_len * 4 */
    tcg_gen_mov_i64(addr_start, t1); /* t1 = addr + byte4_len * 4 */
    tcg_gen_addi_i64(addr_end, addr_start, 24);
    for (i = 0; i < 4; i++) {
        tcg_gen_movcond_i64(TCG_COND_LEU, t1, t1, addr_end, t1, addr_end);
        tcg_gen_qemu_ld_i64(tmp[i], t1, memidx, MO_TEQ);
        tcg_gen_addi_i64(t1, t1, 8);
    }
    tcg_gen_subfi_i64(byte8_len, 4, byte8_len);

    for (i = 0; i < 4; i++) {
        tcg_gen_movi_i64(ti, i);
        tcg_gen_movcond_i64(TCG_COND_LTU, tmp[i], ti, byte8_len, cpu_fr[t0 + i*32], tmp[i]);
    }

    tcg_gen_addi_i64(addr_start, addr_start, 32);
    for (i = 3; i >= 0; i--) {
        tcg_gen_subi_i64(addr_start, addr_start, 8);
        tcg_gen_movcond_i64(TCG_COND_LEU, t1, addr_start, addr_end, addr_start, addr_end);
        tcg_gen_qemu_st_i64(tmp[i], t1, memidx, MO_TEQ);
    }

    tcg_temp_free(ti);
    tcg_temp_free(addr_start);
    tcg_temp_free(addr_end);
    tcg_temp_free(byte8_len);
    tcg_temp_free(tmp[0]);
    tcg_temp_free(tmp[1]);
    tcg_temp_free(tmp[2]);
    tcg_temp_free(tmp[3]);
}

static void tcg_gen_vcpys_i64(int ra, int rb, int rc)
{
    int i;
    TCGv tmp64 = tcg_temp_new();
    for (i = 0; i < 128; i += 32) {
        tcg_gen_shri_i64(tmp64, cpu_fr[ra + i], 63);
        tcg_gen_shli_i64(tmp64, tmp64, 63);
        tcg_gen_andi_i64(cpu_fr[rc + i], cpu_fr[rb + i], 0x7fffffffffffffffUL);
        tcg_gen_or_i64(cpu_fr[rc + i], tmp64, cpu_fr[rc + i]);
    }
    tcg_temp_free(tmp64);
}

static void tcg_gen_vcpyse_i64(int ra, int rb, int rc)
{
    int i;

    TCGv tmp64 = tcg_temp_new();

    for (i = 0; i < 128; i += 32) {
        tcg_gen_shri_i64(tmp64, cpu_fr[ra + i], 52);
        tcg_gen_shli_i64(tmp64, tmp64, 52);
        tcg_gen_andi_i64(cpu_fr[rc + i], cpu_fr[rb + i], 0x000fffffffffffffUL);
        tcg_gen_or_i64(cpu_fr[rc + i], tmp64, cpu_fr[rc + i]);
    }
    tcg_temp_free(tmp64);
}

static void tcg_gen_vcpysn_i64(int ra, int rb, int rc)
{
    int i;
    TCGv tmp64 = tcg_temp_new();
    for (i = 0; i < 128; i += 32) {
        tcg_gen_shri_i64(tmp64, cpu_fr[ra + i], 63);
        tcg_gen_not_i64(tmp64, tmp64);
        tcg_gen_shli_i64(tmp64, tmp64, 63);
        tcg_gen_andi_i64(cpu_fr[rc + i], cpu_fr[rb + i], 0x7fffffffffffffffUL);
        tcg_gen_or_i64(cpu_fr[rc + i], tmp64, cpu_fr[rc + i]);
    }
    tcg_temp_free(tmp64);
}

static void tcg_gen_vlogzz_i64(DisasContext *ctx, int opc, int ra, int rb,
                               int rc, int rd, int fn6)
{
    TCGv zz;
    TCGv args, vd;
    zz = tcg_const_i64(((opc & 0x3) << 6) | fn6);
    args = tcg_const_i64((ra << 16) | (rb << 8) | rc);
    vd = tcg_const_i64(rd);

    gen_helper_vlogzz(cpu_env, args, vd, zz);

    tcg_temp_free(vd);
    tcg_temp_free(args);
    tcg_temp_free(zz);
}

static void gen_qemu_vcmpxxw_i64(TCGCond cond, int ra, int rb, int rc)
{
    TCGv va, vb, vc, tmp64;
    int i;

    va = tcg_temp_new();
    vb = tcg_temp_new();
    vc = tcg_temp_new();
    tmp64 = tcg_temp_new();

    for (i = 0; i < 128; i += 32) {
        if ((cond >> 1) & 1) {
            tcg_gen_ext32s_i64(va, cpu_fr[ra + i]);
            tcg_gen_ext32s_i64(vb, cpu_fr[rb + i]);
        } else {
            tcg_gen_ext32u_i64(va, cpu_fr[ra + i]);
            tcg_gen_ext32u_i64(vb, cpu_fr[rb + i]);
        }
        tcg_gen_setcond_i64(cond, vc, va, vb);
        tcg_gen_mov_i64(tmp64, vc);

        tcg_gen_shri_i64(va, cpu_fr[ra + i], 32);
        tcg_gen_shri_i64(vb, cpu_fr[rb + i], 32);
        if ((cond >> 1) & 1) {
            tcg_gen_ext32s_i64(va, va);
            tcg_gen_ext32s_i64(vb, vb);
        } else {
            tcg_gen_ext32u_i64(va, va);
            tcg_gen_ext32u_i64(vb, vb);
        }
        tcg_gen_setcond_i64(cond, vc, va, vb);
        tcg_gen_shli_i64(vc, vc, 32);
        tcg_gen_or_i64(cpu_fr[rc + i], tmp64, vc);
    }
    tcg_temp_free(va);
    tcg_temp_free(vb);
    tcg_temp_free(vc);
    tcg_temp_free(tmp64);
}

static void gen_qemu_vcmpxxwi_i64(TCGCond cond, int ra, int rb, int rc)
{
    TCGv va, vb, vc, tmp64;
    int i;

    va = tcg_temp_new();
    vb = tcg_const_i64(rb);
    vc = tcg_temp_new();
    tmp64 = tcg_temp_new();

    for (i = 0; i < 128; i += 32) {
        if ((cond >> 1) & 1) {
            tcg_gen_ext32s_i64(va, cpu_fr[ra + i]);
        } else {
            tcg_gen_ext32u_i64(va, cpu_fr[ra + i]);
        }
        tcg_gen_setcond_i64(cond, vc, va, vb);
        tcg_gen_mov_i64(tmp64, vc);

        tcg_gen_shri_i64(va, cpu_fr[ra + i], 32);
        if ((cond >> 1) & 1) {
            tcg_gen_ext32s_i64(va, va);
        } else {
            tcg_gen_ext32u_i64(va, va);
        }
        tcg_gen_setcond_i64(cond, vc, va, vb);
        tcg_gen_shli_i64(vc, vc, 32);
        tcg_gen_or_i64(cpu_fr[rc + i], tmp64, vc);
    }
    tcg_temp_free(va);
    tcg_temp_free(vb);
    tcg_temp_free(vc);
    tcg_temp_free(tmp64);
}

static void gen_qemu_vselxxw(TCGCond cond, int ra, int rb, int rc, int rd,
                             int mask)
{
    int i;

    TCGv t0 = tcg_const_i64(0);
    TCGv tmpa = tcg_temp_new();
    TCGv tmpb = tcg_temp_new();
    TCGv tmpc = tcg_temp_new();
    TCGv tmpd = tcg_temp_new();

    for (i = 0; i < 128; i += 32) {
        tcg_gen_ext32s_i64(tmpa, cpu_fr[ra + i]);
        tcg_gen_ext32u_i64(tmpb, cpu_fr[rb + i]);
        tcg_gen_ext32u_i64(tmpc, cpu_fr[rc + i]);
        if (mask) tcg_gen_andi_i64(tmpa, tmpa, mask);
        tcg_gen_movcond_i64(cond, tmpd, tmpa, t0, tmpb, tmpc);

        tcg_gen_andi_i64(tmpa, cpu_fr[ra + i], 0xffffffff00000000UL);
        tcg_gen_andi_i64(tmpb, cpu_fr[rb + i], 0xffffffff00000000UL);
        tcg_gen_andi_i64(tmpc, cpu_fr[rc + i], 0xffffffff00000000UL);
        if (mask) tcg_gen_andi_i64(tmpa, tmpa, (uint64_t)mask << 32);
        tcg_gen_movcond_i64(cond, cpu_fr[rd + i], tmpa, t0, tmpb, tmpc);

        tcg_gen_or_i64(cpu_fr[rd + i], cpu_fr[rd + i], tmpd);
    }

    tcg_temp_free(t0);
    tcg_temp_free(tmpa);
    tcg_temp_free(tmpb);
    tcg_temp_free(tmpc);
    tcg_temp_free(tmpd);
}

static void gen_qemu_vselxxwi(TCGCond cond, int ra, int rb, int disp8, int rd,
                              int mask)
{
    int i;

    TCGv t0 = tcg_const_i64(0);
    TCGv tmpa = tcg_temp_new();
    TCGv tmpb = tcg_temp_new();
    TCGv tmpc_0 = tcg_temp_new();
    TCGv tmpc_1 = tcg_temp_new();
    TCGv tmpd = tcg_temp_new();

    tcg_gen_movi_i64(tmpc_0, (uint64_t)(((uint64_t)disp8)));
    tcg_gen_movi_i64(tmpc_1, (uint64_t)(((uint64_t)disp8 << 32)));
    for (i = 0; i < 128; i += 32) {
        tcg_gen_ext32s_i64(tmpa, cpu_fr[ra + i]);
        tcg_gen_ext32u_i64(tmpb, cpu_fr[rb + i]);
        if (mask) tcg_gen_andi_i64(tmpa, tmpa, mask);
        tcg_gen_movcond_i64(cond, tmpd, tmpa, t0, tmpb, tmpc_0);

        tcg_gen_andi_i64(tmpa, cpu_fr[ra + i], 0xffffffff00000000UL);
        tcg_gen_andi_i64(tmpb, cpu_fr[rb + i], 0xffffffff00000000UL);
        if (mask) tcg_gen_andi_i64(tmpa, tmpa, (uint64_t)mask << 32);
        tcg_gen_movcond_i64(cond, cpu_fr[rd + i], tmpa, t0, tmpb, tmpc_1);

        tcg_gen_or_i64(cpu_fr[rd + i], cpu_fr[rd + i], tmpd);
    }

    tcg_temp_free(t0);
    tcg_temp_free(tmpa);
    tcg_temp_free(tmpb);
    tcg_temp_free(tmpc_0);
    tcg_temp_free(tmpc_1);
    tcg_temp_free(tmpd);
}

DisasJumpType translate_one(DisasContextBase *dcbase, uint32_t insn,
                                   CPUState *cpu)
{
    int32_t disp5, disp8, disp12, disp13, disp16, disp21, disp26 __attribute__((unused));
    uint8_t opc, ra, rb, rc, rd;
    uint16_t fn3, fn4, fn6, fn8, fn11;
    int32_t i;
    TCGv va, vb, vc, vd;
    TCGv_i32 tmp32;
    TCGv_i64 tmp64, tmp64_0, tmp64_1, shift;
    TCGv_i32 tmpa, tmpb, tmpc;
    DisasJumpType ret;
    DisasContext* ctx = container_of(dcbase, DisasContext, base);

    opc = extract32(insn, 26, 6);
    ra = extract32(insn, 21, 5);
    rb = extract32(insn, 16, 5);
    rc = extract32(insn, 0, 5);
    rd = extract32(insn, 5, 5);

    fn3 = extract32(insn, 10, 3);
    fn6 = extract32(insn, 10, 6);
    fn4 = extract32(insn, 12, 4);
    fn8 = extract32(insn, 5, 8);
    fn11 = extract32(insn, 5, 11);

    disp5 = extract32(insn, 5, 5);
    disp8 = extract32(insn, 13, 8);
    disp12 = sextract32(insn, 0, 12);
    disp13 = sextract32(insn, 13, 13);
    disp16 = sextract32(insn, 0, 16);
    disp21 = sextract32(insn, 0, 21);
    disp26 = sextract32(insn, 0, 26);

    ret = DISAS_NEXT;
    insn_profile(ctx, insn);

    switch (opc) {
    case 0x00:
        /* SYS_CALL */
        ret = gen_sys_call(ctx, insn & 0x1ffffff);
        break;
    case 0x01:
    /* CALL */
    case 0x02:
    /* RET */
    case 0x03:
        /* JMP */
        vb = load_gir(ctx, rb);
        tcg_gen_addi_i64(cpu_pc, vb, ctx->base.pc_next & 0x3);
        if (ra != 31) {
            tcg_gen_movi_i64(load_gir(ctx, ra), ctx->base.pc_next & (~3UL));
        }
        ret = DISAS_PC_UPDATED;
        break;
    case 0x04:
    /* BR */
    case 0x05:
        /* BSR */
        ret = gen_bdirect(ctx, ra, disp21);
        break;
    case 0x06:
        switch (disp16) {
        case 0x0000:
            /* MEMB */
            tcg_gen_mb(TCG_MO_ALL | TCG_BAR_SC);
            break;
        case 0x0001:
            /* IMEMB */
            /* No achievement in Qemu*/
            break;
        case 0x0020:
            /* RTC */
            if (disp16 && unlikely(ra == 31)) break;
            va = load_gir(ctx, ra);
            gen_helper_rtc(va);
            break;
        case 0x0040:
            /* RCID */
            if (disp16 && unlikely(ra == 31)) break;
            va = load_gir(ctx, ra);
            read_csr(0xc9, va);
            break;
        case 0x0080:
            /* HALT */
#ifndef CONFIG_USER_ONLY
        {
            tmp32 = tcg_const_i32(1);
            tcg_gen_st_i32(
                tmp32, cpu_env,
                -offsetof(SW64CPU, env) + offsetof(CPUState, halted));
            tcg_temp_free_i32(tmp32);
        }
            ret = gen_excp(ctx, EXCP_HALTED, 0);
#endif
            break;
        case 0x1000:
            /* RD_F */
            if (disp16 && unlikely(ra == 31)) break;
            va = load_gir(ctx, ra);
            tcg_gen_mov_i64(va, cpu_lock_success);
            break;
        case 0x1020:
            /* WR_F */
            if (disp16 && unlikely(ra == 31)) break;
            va = load_gir(ctx, ra);
            tcg_gen_andi_i64(cpu_lock_flag, va, 0x1);
            break;
        case 0x1040:
            /* RTID */
            if (unlikely(ra == 31)) break;
            va = load_gir(ctx, ra);
            read_csr(0xc7, va);
            break;
        default:
            if ((disp16 & 0xFF00) == 0xFE00) {
                /* PRI_RCSR */
                if (disp16 && unlikely(ra == 31)) break;
                va = load_gir(ctx, ra);
                read_csr(disp16 & 0xff, va);
                break;
            }
            if ((disp16 & 0xFF00) == 0xFF00) {
                /* PRI_WCSR */
                va = load_gir(ctx, ra);
                write_csr(disp16 & 0xff, va, ctx->env);
                break;
            }
            goto do_invalid;
            }
            break;
        case 0x07:
            /* PRI_RET */
            va = load_gir(ctx, ra);
            tcg_gen_mov_i64(cpu_pc, va);
            gen_helper_cpustate_update(cpu_env, va);
            ret = DISAS_PC_UPDATED_NOCHAIN;
            break;
        case 0x08:
            switch (fn4) {
            case 0x0:
                /* LLDW */
                gen_load_mem(ctx, &gen_qemu_lldw, ra, rb, disp12, 0, 0);
                break;
            case 0x1:
                /* LLDL */
                gen_load_mem(ctx, &gen_qemu_lldl, ra, rb, disp12, 0, 0);
                break;
            case 0x2:
                /* LDW_INC */
                ldx_xxx(ctx, ra, rb, disp12, 0, 1);
                break;
            case 0x3:
                /* LDL_INC */
                ldx_xxx(ctx, ra, rb, disp12, 1, 1);
                break;
            case 0x4:
                /* LDW_DEC */
                ldx_xxx(ctx, ra, rb, disp12, 0, -1);
                break;
            case 0x5:
                /* LDL_DEC */
                ldx_xxx(ctx, ra, rb, disp12, 1, -1);
                break;
            case 0x6:
                /* LDW_SET */
                ldx_set(ctx, ra, rb, disp12, 0);
                break;
            case 0x7:
                /* LDL_SET */
                ldx_set(ctx, ra, rb, disp12, 1);
                break;
            case 0x8:
                /* LSTW */
                ret = gen_store_conditional(ctx, ra, rb, disp12,
                                            ctx->mem_idx, MO_LEUL);
                break;
            case 0x9:
                /* LSTL */
                ret = gen_store_conditional(ctx, ra, rb, disp12,
                                            ctx->mem_idx, MO_LEQ);
                break;
            case 0xa:
                /* LDW_NC */
                gen_load_mem(ctx, &tcg_gen_qemu_ld32s, ra, rb, disp12, 0,
                             0);
                break;
            case 0xb:
                /* LDL_NC */
                gen_load_mem(ctx, &tcg_gen_qemu_ld64, ra, rb, disp12, 0, 0);
                break;
            case 0xc:
                /* LDD_NC */
                gen_load_mem(ctx, &tcg_gen_qemu_ld64, ra, rb, disp12, 1, 0);
                break;
            case 0xd:
                /* STW_NC */
                gen_store_mem(ctx, &tcg_gen_qemu_st32, ra, rb, disp12, 0,
                              0);
                break;
            case 0xe:
                /* STL_NC */
                gen_store_mem(ctx, &tcg_gen_qemu_st64, ra, rb, disp12, 0,
                              0);
                break;
            case 0xf:
                /* STD_NC */
                gen_store_mem(ctx, &tcg_gen_qemu_st64, ra, rb, disp12, 1,
                              0);
                break;
            default:
                goto do_invalid;
            }
            break;
        case 0x9:
            /* LDWE */
            gen_load_mem_simd(ctx, &gen_qemu_ldwe, ra, rb, disp16, 0);
            break;
        case 0x0a:
            /* LDSE */
            gen_load_mem_simd(ctx, &gen_qemu_ldse, ra, rb, disp16, 0);
            break;
        case 0x0b:
            /* LDDE */
            gen_load_mem_simd(ctx, &gen_qemu_ldde, ra, rb, disp16, 0);
            break;
        case 0x0c:
            /* VLDS */
            gen_load_mem_simd(ctx, &gen_qemu_vlds, ra, rb, disp16, 0);
            break;
        case 0x0d:
            /* VLDD */
            if (unlikely(ra == 31)) break;
            gen_load_mem_simd(ctx, &gen_qemu_vldd, ra, rb, disp16, 0);
            break;
        case 0x0e:
            /* VSTS */
            gen_store_mem_simd(ctx, &gen_qemu_vsts, ra, rb, disp16, 0);
            break;
        case 0x0f:
            /* VSTD */
            gen_store_mem_simd(ctx, &gen_qemu_vstd, ra, rb, disp16, 0);
            break;
        case 0x10:
            if (unlikely(rc == 31)) break;
            if (fn11 == 0x70) {
                /* FIMOVS */
                va = cpu_fr[ra];
                vc = load_gir(ctx, rc);
                tmp32 = tcg_temp_new_i32();
                gen_helper_s_to_memory(tmp32, va);
                tcg_gen_ext_i32_i64(vc, tmp32);
                tcg_temp_free_i32(tmp32);
            } else if (fn11 == 0x78) {
                /* FIMOVD */
                va = cpu_fr[ra];
                vc = load_gir(ctx, rc);
                tcg_gen_mov_i64(vc, va);
            } else {
                va = load_gir(ctx, ra);
                vb = load_gir(ctx, rb);
                vc = load_gir(ctx, rc);
                cal_with_iregs_2(ctx, vc, va, vb, disp13, fn11);
            }
            break;
        case 0x11:
            if (unlikely(rc == 31)) break;
            va = load_gir(ctx, ra);
            vb = load_gir(ctx, rb);
            vc = load_gir(ctx, rc);
            vd = load_gir(ctx, rd);
            cal_with_iregs_3(ctx, vc, va, vb, vd, fn3);
            break;
        case 0x12:
            if (unlikely(rc == 31)) break;
            va = load_gir(ctx, ra);
            vc = load_gir(ctx, rc);
            cal_with_imm_2(ctx, vc, va, disp8, fn8);
            break;
        case 0x13:
            if (rc == 31) /* Special deal */
                break;
            va = load_gir(ctx, ra);
            vc = load_gir(ctx, rc);
            vd = load_gir(ctx, rd);
            cal_with_imm_3(ctx, vc, va, disp8, vd, fn3);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            /* VLOGZZ */
            tcg_gen_vlogzz_i64(ctx, opc, ra, rb, rd, rc, fn6);
            break;
        case 0x18:
            if (unlikely(rc == 31)) break;
            cal_with_fregs_2(ctx, rc, ra, rb, fn8);
            break;
        case 0x19:
            if (unlikely(rc == 31)) break;
            cal_with_fregs_4(ctx, rc, ra, rb, rd, fn6);
            break;
        case 0x1A:
            /* SIMD */
            if (unlikely(rc == 31)) break;
            switch (fn8) {
            case 0x00:
                /* VADDW */
                tmp64 = tcg_temp_new();
                va = tcg_temp_new();
                vb = tcg_temp_new();
                vc = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_andi_i64(va, cpu_fr[ra + i], 0xffffffffUL);
                    tcg_gen_andi_i64(vb, cpu_fr[rb + i], 0xffffffffUL);
                    tcg_gen_add_i64(tmp64, va, vb);
                    tcg_gen_ext32u_i64(tmp64, tmp64);
                    tcg_gen_andi_i64(va, cpu_fr[ra + i],
                                     0xffffffff00000000UL);
                    tcg_gen_andi_i64(vb, cpu_fr[rb + i],
                                     0xffffffff00000000UL);
                    tcg_gen_add_i64(vc, va, vb);
                    tcg_gen_or_i64(tmp64, tmp64, vc);
                    tcg_gen_mov_i64(cpu_fr[rc + i], tmp64);
                }
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                tcg_temp_free(tmp64);
                break;
            case 0x20:
                /* VADDW */
                tmp64 = tcg_temp_new();
                va = tcg_temp_new();
                vc = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_andi_i64(va, cpu_fr[ra + i], 0xffffffffUL);
                    tcg_gen_addi_i64(tmp64, va, disp8);
                    tcg_gen_ext32u_i64(tmp64, tmp64);
                    tcg_gen_andi_i64(va, cpu_fr[ra + i],
                                     0xffffffff00000000UL);
                    tcg_gen_addi_i64(vc, va, ((uint64_t)disp8 << 32));
                    tcg_gen_or_i64(tmp64, tmp64, vc);
                    tcg_gen_mov_i64(cpu_fr[rc + i], tmp64);
                }
                tcg_temp_free(va);
                tcg_temp_free(vc);
                tcg_temp_free(tmp64);
                break;
            case 0x01:
                /* VSUBW */
                tmp64 = tcg_temp_new();
                va = tcg_temp_new();
                vb = tcg_temp_new();
                vc = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_andi_i64(va, cpu_fr[ra + i], 0xffffffffUL);
                    tcg_gen_andi_i64(vb, cpu_fr[rb + i], 0xffffffffUL);
                    tcg_gen_sub_i64(tmp64, va, vb);
                    tcg_gen_ext32u_i64(tmp64, tmp64);
                    tcg_gen_andi_i64(va, cpu_fr[ra + i],
                                     0xffffffff00000000UL);
                    tcg_gen_andi_i64(vb, cpu_fr[rb + i],
                                     0xffffffff00000000UL);
                    tcg_gen_sub_i64(vc, va, vb);
                    tcg_gen_or_i64(tmp64, tmp64, vc);
                    tcg_gen_mov_i64(cpu_fr[rc + i], tmp64);
                }
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                tcg_temp_free(tmp64);
                break;
            case 0x21:
                /* VSUBW */
                tmp64 = tcg_temp_new();
                va = tcg_temp_new();
                vc = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_andi_i64(va, cpu_fr[ra + i], 0xffffffffUL);
                    tcg_gen_subi_i64(tmp64, va, disp8);
                    tcg_gen_ext32u_i64(tmp64, tmp64);
                    tcg_gen_andi_i64(va, cpu_fr[ra + i],
                                     0xffffffff00000000UL);
                    tcg_gen_subi_i64(vc, va, ((uint64_t)disp8 << 32));
                    tcg_gen_or_i64(tmp64, tmp64, vc);
                    tcg_gen_mov_i64(cpu_fr[rc + i], tmp64);
                }
                tcg_temp_free(va);
                tcg_temp_free(vc);
                tcg_temp_free(tmp64);
                break;
            case 0x02:
                /* VCMPGEW */
                tmp64 = tcg_const_i64(0);
                va = tcg_temp_new();
                vb = tcg_temp_new();
                vc = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_ext32s_i64(va, cpu_fr[ra + i]);
                    tcg_gen_ext32s_i64(vb, cpu_fr[rb + i]);
                    tcg_gen_setcond_i64(TCG_COND_GE, vc, va, vb);
                    tcg_gen_or_i64(tmp64, tmp64, vc);
                    tcg_gen_shri_i64(va, cpu_fr[ra + i], 32);
                    tcg_gen_shri_i64(vb, cpu_fr[rb + i], 32);
                    tcg_gen_ext32s_i64(va, va);
                    tcg_gen_ext32s_i64(vb, vb);
                    tcg_gen_setcond_i64(TCG_COND_GE, vc, va, vb);
                    tcg_gen_or_i64(tmp64, tmp64, vc);
                }
                tcg_gen_shli_i64(cpu_fr[rc], tmp64, 29);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                tcg_temp_free(tmp64);
                break;
            case 0x22:
                /* VCMPGEW */
                tmp64 = tcg_const_i64(0);
                va = tcg_temp_new();
                vb = tcg_const_i64(disp8);
                vc = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_ext32s_i64(va, cpu_fr[ra + i]);
                    tcg_gen_setcond_i64(TCG_COND_GE, vc, va, vb);
                    tcg_gen_or_i64(tmp64, tmp64, vc);
                    tcg_gen_shri_i64(va, cpu_fr[ra + i], 32);
                    tcg_gen_ext32s_i64(va, va);
                    tcg_gen_setcond_i64(TCG_COND_GE, vc, va, vb);
                    tcg_gen_or_i64(tmp64, tmp64, vc);
                }
                tcg_gen_shli_i64(cpu_fr[rc], tmp64, 29);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                tcg_temp_free(tmp64);
                break;
            case 0x03:
                /* VCMPEQW */
                gen_qemu_vcmpxxw_i64(TCG_COND_EQ, ra, rb, rc);
                break;
            case 0x23:
                /* VCMPEQW */
                gen_qemu_vcmpxxwi_i64(TCG_COND_EQ, ra, disp8, rc);
                break;
            case 0x04:
                /* VCMPLEW */
                gen_qemu_vcmpxxw_i64(TCG_COND_LE, ra, rb, rc);
                break;
            case 0x24:
                /* VCMPLEW */
                gen_qemu_vcmpxxwi_i64(TCG_COND_LE, ra, disp8, rc);
                break;
            case 0x05:
                /* VCMPLTW */
                gen_qemu_vcmpxxw_i64(TCG_COND_LT, ra, rb, rc);
                break;
            case 0x25:
                /* VCMPLTW */
                gen_qemu_vcmpxxwi_i64(TCG_COND_LT, ra, disp8, rc);
                break;
            case 0x06:
                /* VCMPULEW */
                gen_qemu_vcmpxxw_i64(TCG_COND_LEU, ra, rb, rc);
                break;
            case 0x26:
                /* VCMPULEW */
                gen_qemu_vcmpxxwi_i64(TCG_COND_LEU, ra, disp8, rc);
                break;
            case 0x07:
                /* VCMPULTW */
                gen_qemu_vcmpxxw_i64(TCG_COND_LTU, ra, rb, rc);
                break;
            case 0x27:
                /* VCMPULTW */
                gen_qemu_vcmpxxwi_i64(TCG_COND_LTU, ra, disp8, rc);
                break;
            case 0x08:
                /* VSLLW */
                tmp64 = tcg_temp_new();
                shift = tcg_temp_new();
                vc = tcg_temp_new();
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_shri_i64(shift, cpu_fr[rb], 29);
                    tcg_gen_andi_i64(shift, shift, 0x1fUL);

                    tcg_gen_shl_i64(vc, cpu_fr[ra + i], shift);
                    tcg_gen_ext32u_i64(tmp64, vc);

                    tcg_gen_andi_i64(vc, cpu_fr[ra + i],
                                     0xffffffff00000000UL);
                    tcg_gen_shl_i64(vc, vc, shift);
                    tcg_gen_or_i64(cpu_fr[rc + i], tmp64, vc);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(shift);
                tcg_temp_free(vc);
                break;
            case 0x28:
                /* VSLLW */
                tmp64 = tcg_temp_new();
                shift = tcg_temp_new();
                vc = tcg_temp_new();
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_movi_i64(shift, disp8 & 0x1fUL);

                    tcg_gen_shl_i64(vc, cpu_fr[ra + i], shift);
                    tcg_gen_ext32u_i64(tmp64, vc);

                    tcg_gen_andi_i64(vc, cpu_fr[ra + i],
                                     0xffffffff00000000UL);
                    tcg_gen_shl_i64(vc, vc, shift);
                    tcg_gen_or_i64(cpu_fr[rc + i], tmp64, vc);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(shift);
                tcg_temp_free(vc);
                break;
            case 0x09:
                /* VSRLW */
                tmp64 = tcg_temp_new();
                shift = tcg_temp_new();
                vc = tcg_temp_new();
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_shri_i64(shift, cpu_fr[rb], 29);
                    tcg_gen_andi_i64(shift, shift, 0x1fUL);

                    tcg_gen_ext32u_i64(vc, cpu_fr[ra + i]);
                    tcg_gen_shr_i64(tmp64, vc, shift);

                    tcg_gen_shr_i64(vc, cpu_fr[ra + i], shift);
                    tcg_gen_andi_i64(vc, vc, 0xffffffff00000000UL);
                    tcg_gen_or_i64(cpu_fr[rc + i], tmp64, vc);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(shift);
                tcg_temp_free(vc);
                break;
            case 0x29:
                /* VSRLW */
                tmp64 = tcg_temp_new();
                shift = tcg_temp_new();
                vc = tcg_temp_new();
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_movi_i64(shift, disp8 & 0x1fUL);

                    tcg_gen_ext32u_i64(vc, cpu_fr[ra + i]);
                    tcg_gen_shr_i64(tmp64, vc, shift);

                    tcg_gen_shr_i64(vc, cpu_fr[ra + i], shift);
                    tcg_gen_andi_i64(vc, vc, 0xffffffff00000000UL);
                    tcg_gen_or_i64(cpu_fr[rc + i], tmp64, vc);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(shift);
                tcg_temp_free(vc);
                break;
            case 0x0A:
                /* VSRAW */
                tmp64 = tcg_temp_new();
                shift = tcg_temp_new();
                vc = tcg_temp_new();
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_shri_i64(shift, cpu_fr[rb], 29);
                    tcg_gen_andi_i64(shift, shift, 0x1fUL);

                    tcg_gen_ext32s_i64(vc, cpu_fr[ra + i]);
                    tcg_gen_sar_i64(tmp64, vc, shift);

                    tcg_gen_sar_i64(vc, cpu_fr[ra + i], shift);
                    tcg_gen_andi_i64(vc, vc, 0xffffffff00000000UL);
                    tcg_gen_or_i64(cpu_fr[rc + i], tmp64, vc);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(shift);
                tcg_temp_free(vc);
                break;
            case 0x2A:
                /* VSRAWI */
                tmp64 = tcg_temp_new();
                shift = tcg_temp_new();
                vc = tcg_temp_new();
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_movi_i64(shift, disp8 & 0x1fUL);

                    tcg_gen_ext32s_i64(vc, cpu_fr[ra + i]);
                    tcg_gen_sar_i64(tmp64, vc, shift);

                    tcg_gen_sar_i64(vc, cpu_fr[ra + i], shift);
                    tcg_gen_andi_i64(vc, vc, 0xffffffff00000000UL);
                    tcg_gen_or_i64(cpu_fr[rc + i], tmp64, vc);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(shift);
                tcg_temp_free(vc);
                break;
            case 0x0B:
                /* VROLW */
                tmpa = tcg_temp_new_i32();
                tmpb = tcg_temp_new_i32();
                tmpc = tcg_temp_new_i32();
                tmp64 = tcg_temp_new();
                shift = tcg_temp_new();
                vc = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_shri_i64(shift, cpu_fr[rb], 29);
                    tcg_gen_andi_i64(shift, shift, 0x1fUL);

                    tcg_gen_extrl_i64_i32(tmpa, cpu_fr[ra + i]);
                    tcg_gen_extrl_i64_i32(tmpb, shift);

                    tcg_gen_rotl_i32(tmpc, tmpa, tmpb);
                    tcg_gen_extu_i32_i64(tmp64, tmpc);

                    tcg_gen_extrh_i64_i32(tmpa, cpu_fr[ra + i]);
                    tcg_gen_rotl_i32(tmpc, tmpa, tmpb);
                    tcg_gen_extu_i32_i64(vc, tmpc);
                    tcg_gen_shli_i64(vc, vc, 32);

                    tcg_gen_or_i64(cpu_fr[rc + i], vc, tmp64);
                }
                tcg_temp_free_i32(tmpa);
                tcg_temp_free_i32(tmpb);
                tcg_temp_free_i32(tmpc);
                tcg_temp_free(tmp64);
                tcg_temp_free(shift);
                tcg_temp_free(vc);
                break;
            case 0x2B:
                /* VROLW */
                tmpa = tcg_temp_new_i32();
                tmpb = tcg_temp_new_i32();
                tmpc = tcg_temp_new_i32();
                tmp64 = tcg_temp_new();
                shift = tcg_temp_new();
                vc = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_movi_i64(shift, disp8 & 0x1fUL);

                    tcg_gen_extrl_i64_i32(tmpa, cpu_fr[ra + i]);
                    tcg_gen_extrl_i64_i32(tmpb, shift);

                    tcg_gen_rotl_i32(tmpc, tmpa, tmpb);
                    tcg_gen_extu_i32_i64(tmp64, tmpc);

                    tcg_gen_extrh_i64_i32(tmpa, cpu_fr[ra + i]);
                    tcg_gen_rotl_i32(tmpc, tmpa, tmpb);
                    tcg_gen_extu_i32_i64(vc, tmpc);
                    tcg_gen_shli_i64(vc, vc, 32);

                    tcg_gen_or_i64(cpu_fr[rc + i], vc, tmp64);
                }
                tcg_temp_free_i32(tmpa);
                tcg_temp_free_i32(tmpb);
                tcg_temp_free_i32(tmpc);
                tcg_temp_free(tmp64);
                tcg_temp_free(shift);
                tcg_temp_free(vc);
                break;
            case 0x0C:
                /* SLLOW */
                tcg_gen_sllow_i64(ra, rc, rb);
                break;
            case 0x2C:
                /* SLLOW */
                tcg_gen_sllowi_i64(ra, rc, disp8);
                break;
            case 0x0D:
                /* SRLOW */
                tcg_gen_srlow_i64(ra, rc, rb);
                break;
            case 0x2D:
                /* SRLOW */
                tcg_gen_srlowi_i64(ra, rc, disp8);
                break;
            case 0x0E:
                /* VADDL */
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_add_i64(cpu_fr[rc + i], cpu_fr[ra + i],
                                    cpu_fr[rb + i]);
                }
                break;
            case 0x2E:
                /* VADDL */
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_addi_i64(cpu_fr[rc + i], cpu_fr[ra + i], disp8);
                }
                break;
            case 0x0F:
                /* VSUBL */
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_sub_i64(cpu_fr[rc + i], cpu_fr[ra + i],
                                    cpu_fr[rb + i]);
                }
                break;
            case 0x2F:
                /* VSUBL */
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_subi_i64(cpu_fr[rc + i], cpu_fr[ra + i], disp8);
                }
                break;
            case 0x18:
                /* CTPOPOW */
                tmp64 = tcg_const_i64(0);
                tmp64_0 = tcg_temp_new();

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_ctpop_i64(tmp64_0, cpu_fr[ra + i]);
                    tcg_gen_add_i64(tmp64, tmp64, tmp64_0);
                }
                tcg_gen_shli_i64(cpu_fr[rc], tmp64, 29);
                tcg_temp_free(tmp64);
                tcg_temp_free(tmp64_0);
                break;
            case 0x19:
                /* CTLZOW */
                va = tcg_const_i64(ra);
                gen_helper_ctlzow(cpu_fr[rc], cpu_env, va);
                tcg_temp_free(va);
                break;
            case 0x40:
                /* VUCADDW */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(rb);
                vc = tcg_const_i64(rc);
                gen_helper_vucaddw(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x60:
                /* VUCADDW */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(disp8);
                vc = tcg_const_i64(rc);
                gen_helper_vucaddwi(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x41:
                /* VUCSUBW */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(rb);
                vc = tcg_const_i64(rc);
                gen_helper_vucsubw(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x61:
                /* VUCSUBW */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(disp8);
                vc = tcg_const_i64(rc);
                gen_helper_vucsubwi(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x42:
                /* VUCADDH */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(rb);
                vc = tcg_const_i64(rc);
                gen_helper_vucaddh(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x62:
                /* VUCADDH */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(disp8);
                vc = tcg_const_i64(rc);
                gen_helper_vucaddhi(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x43:
                /* VUCSUBH */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(rb);
                vc = tcg_const_i64(rc);
                gen_helper_vucsubh(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x63:
                /* VUCSUBH */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(disp8);
                vc = tcg_const_i64(rc);
                gen_helper_vucsubhi(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x44:
                /* VUCADDB */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(rb);
                vc = tcg_const_i64(rc);
                gen_helper_vucaddb(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x64:
                /* VUCADDB */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(disp8);
                vc = tcg_const_i64(rc);
                gen_helper_vucaddbi(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x45:
                /* VUCSUBB */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(rb);
                vc = tcg_const_i64(rc);
                gen_helper_vucsubb(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x65:
                /* VUCSUBB */
                va = tcg_const_i64(ra);
                vb = tcg_const_i64(disp8);
                vc = tcg_const_i64(rc);
                gen_helper_vucsubbi(cpu_env, va, vb, vc);
                tcg_temp_free(va);
                tcg_temp_free(vb);
                tcg_temp_free(vc);
                break;
            case 0x80:
                /* VADDS */
                for (i = 0; i < 128; i += 32)
                    gen_fadds(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x81:
                /* VADDD */
                for (i = 0; i < 128; i += 32)
                    gen_faddd(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x82:
                /* VSUBS */
                for (i = 0; i < 128; i += 32)
                    gen_fsubs(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x83:
                /* VSUBD */
                for (i = 0; i < 128; i += 32)
                    gen_fsubd(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x84:
                /* VMULS */
                for (i = 0; i < 128; i += 32)
                    gen_fmuls(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x85:
                /* VMULD */
                for (i = 0; i < 128; i += 32)
                    gen_fmuld(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x86:
                /* VDIVS */
                for (i = 0; i < 128; i += 32)
                    gen_fdivs(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x87:
                /* VDIVD */
                for (i = 0; i < 128; i += 32)
                    gen_fdivd(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x88:
                /* VSQRTS */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fsqrts(cpu_fr[rc + i], cpu_env,
                                      cpu_fr[rb + i]);
                break;
            case 0x89:
                /* VSQRTD */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fsqrt(cpu_fr[rc + i], cpu_env,
                                     cpu_fr[rb + i]);
                break;
            case 0x8C:
                /* VFCMPEQ */
                for (i = 0; i < 128; i += 32)
                    gen_fcmpeq(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x8D:
                /* VFCMPLE */
                for (i = 0; i < 128; i += 32)
                    gen_fcmple(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x8E:
                /* VFCMPLT */
                for (i = 0; i < 128; i += 32)
                    gen_fcmplt(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x8F:
                /* VFCMPUN */
                for (i = 0; i < 128; i += 32)
                    gen_fcmpun(ctx, ra + i, rb + i, rc + i);
                break;
            case 0x90:
                /* VCPYS */
                tcg_gen_vcpys_i64(ra, rb, rc);
                break;
            case 0x91:
                /* VCPYSE */
                tcg_gen_vcpyse_i64(ra, rb, rc);
                break;
            case 0x92:
                /* VCPYSN */
                tcg_gen_vcpysn_i64(ra, rb, rc);
                break;
            case 0x93:
                /* VSUMS */
                gen_fadds(ctx, ra, ra + 32, rc);
                gen_fadds(ctx, rc, ra + 64, rc);
                gen_fadds(ctx, rc, ra + 96, rc);
                break;
            case 0x94:
                /* VSUMD */
                gen_faddd(ctx, ra, ra + 32, rc);
                gen_faddd(ctx, rc, ra + 64, rc);
                gen_faddd(ctx, rc, ra + 96, rc);
                break;
            default:
                printf("ILLEGAL BELOW OPC[%x] func[%08x]\n", opc, fn8);
                ret = gen_invalid(ctx);
                break;
            }
            break;
        case 0x1B:
            /* SIMD */
            if (unlikely(rc == 31)) break;
            switch (fn6) {
            case 0x00:
                /* VMAS */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fmas(cpu_fr[rc + i], cpu_env, cpu_fr[ra + i],
                                    cpu_fr[rb + i], cpu_fr[rd + i]);
                break;
            case 0x01:
                /* VMAD */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fmad(cpu_fr[rc + i], cpu_env, cpu_fr[ra + i],
                                    cpu_fr[rb + i], cpu_fr[rd + i]);
                break;
            case 0x02:
                /* VMSS */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fmss(cpu_fr[rc + i], cpu_env, cpu_fr[ra + i],
                                    cpu_fr[rb + i], cpu_fr[rd + i]);
                break;
            case 0x03:
                /* VMSD */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fmsd(cpu_fr[rc + i], cpu_env, cpu_fr[ra + i],
                                    cpu_fr[rb + i], cpu_fr[rd + i]);
                break;
            case 0x04:
                /* VNMAS */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fnmas(cpu_fr[rc + i], cpu_env,
                                     cpu_fr[ra + i], cpu_fr[rb + i],
                                     cpu_fr[rd + i]);
                break;
            case 0x05:
                /* VNMAD */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fnmad(cpu_fr[rc + i], cpu_env,
                                     cpu_fr[ra + i], cpu_fr[rb + i],
                                     cpu_fr[rd + i]);
                break;
            case 0x06:
                /* VNMSS */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fnmss(cpu_fr[rc + i], cpu_env,
                                     cpu_fr[ra + i], cpu_fr[rb + i],
                                     cpu_fr[rd + i]);
                break;
            case 0x07:
                /* VNMSD */
                for (i = 0; i < 128; i += 32)
                    gen_helper_fnmsd(cpu_fr[rc + i], cpu_env,
                                     cpu_fr[ra + i], cpu_fr[rb + i],
                                     cpu_fr[rd + i]);
                break;
            case 0x10:
                /* VFSELEQ */
                tmp64 = tcg_temp_new();
                tmp64_0 = tcg_const_i64(0);
                for (i = 0; i < 128; i += 32) {
                    gen_helper_fcmpeq(tmp64, cpu_env, cpu_fr[ra + i],
                                      tmp64_0);
                    tcg_gen_movcond_i64(TCG_COND_EQ, cpu_fr[rc + i], tmp64,
                                        tmp64_0, cpu_fr[rd + i],
                                        cpu_fr[rb + i]);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(tmp64_0);
                break;
            case 0x12:
                /* VFSELLT */
                tmp64 = tcg_temp_new();
                tmp64_0 = tcg_const_i64(0);
                tmp64_1 = tcg_temp_new();
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_andi_i64(tmp64, cpu_fr[ra + i],
                            0x7fffffffffffffffUL);
                    tcg_gen_setcond_i64(TCG_COND_NE, tmp64, tmp64,
                            tmp64_0);
                    tcg_gen_shri_i64(tmp64_1, cpu_fr[ra +i], 63);
                    tcg_gen_and_i64(tmp64, tmp64_1, tmp64);
                    tcg_gen_movcond_i64(TCG_COND_EQ, cpu_fr[rc + i], tmp64,
                                        tmp64_0, cpu_fr[rd + i],
                                        cpu_fr[rb + i]);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(tmp64_0);
                tcg_temp_free(tmp64_1);
                break;
            case 0x13:
                /* VFSELLE */
                tmp64 = tcg_temp_new();
                tmp64_0 = tcg_const_i64(0);
                tmp64_1 = tcg_temp_new();
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_andi_i64(tmp64, cpu_fr[ra + i],
                            0x7fffffffffffffffUL);
                    tcg_gen_setcond_i64(TCG_COND_EQ, tmp64, tmp64,
                            tmp64_0);
                    tcg_gen_shri_i64(tmp64_1, cpu_fr[ra + i], 63);
                    tcg_gen_or_i64(tmp64, tmp64_1, tmp64);
                    tcg_gen_movcond_i64(TCG_COND_EQ, cpu_fr[rc + i], tmp64,
                                        tmp64_0, cpu_fr[rd + i],
                                        cpu_fr[rb + i]);
                }
                tcg_temp_free(tmp64);
                tcg_temp_free(tmp64_0);
                tcg_temp_free(tmp64_1);
                break;
            case 0x18:
                /* VSELEQW */
                gen_qemu_vselxxw(TCG_COND_EQ, ra, rb, rd, rc, 0);
                break;
            case 0x38:
                /* VSELEQW */
                gen_qemu_vselxxwi(TCG_COND_EQ, ra, rb, disp5, rc, 0);
                break;
            case 0x19:
                /* VSELLBCW */
                gen_qemu_vselxxw(TCG_COND_EQ, ra, rb, rd, rc, 1);
                break;
            case 0x39:
                /* VSELLBCW */
                gen_qemu_vselxxwi(TCG_COND_EQ, ra, rb, disp5, rc, 1);
                break;
            case 0x1A:
                /* VSELLTW */
                gen_qemu_vselxxw(TCG_COND_LT, ra, rb, rd, rc, 0);
                break;
            case 0x3A:
                /* VSELLTW */
                gen_qemu_vselxxwi(TCG_COND_LT, ra, rb, disp5, rc, 0);
                break;
            case 0x1B:
                /* VSELLEW */
                gen_qemu_vselxxw(TCG_COND_LE, ra, rb, rd, rc, 0);
                break;
            case 0x3B:
                /* VSELLEW */
                gen_qemu_vselxxwi(TCG_COND_LE, ra, rb, disp5, rc, 0);
                break;
            case 0x20:
                /* VINSW */
                if (disp5 > 7) break;
                tmp64 = tcg_temp_new();
                tmp32 = tcg_temp_new_i32();
                gen_helper_s_to_memory(tmp32, cpu_fr[ra]);
                tcg_gen_extu_i32_i64(tmp64, tmp32);
                tcg_gen_shli_i64(tmp64, tmp64, (disp5 % 2) * 32);
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_mov_i64(cpu_fr[rc + i], cpu_fr[rb + i]);
                }
                if (disp5 % 2) {
                    tcg_gen_andi_i64(cpu_fr[rc + (disp5 / 2) * 32],
                                     cpu_fr[rc + (disp5 / 2) * 32],
                                     0xffffffffUL);
                } else {
                    tcg_gen_andi_i64(cpu_fr[rc + (disp5 / 2) * 32],
                                     cpu_fr[rc + (disp5 / 2) * 32],
                                     0xffffffff00000000UL);
                }
                tcg_gen_or_i64(cpu_fr[rc + (disp5 / 2) * 32],
                               cpu_fr[rc + (disp5 / 2) * 32], tmp64);
                tcg_temp_free(tmp64);
                tcg_temp_free_i32(tmp32);
                break;
            case 0x21:
                /* VINSF */
                if (disp5 > 3) break;
                tmp64 = tcg_temp_new();
                tcg_gen_mov_i64(tmp64, cpu_fr[ra]);

                for (i = 0; i < 128; i += 32) {
                    tcg_gen_mov_i64(cpu_fr[rc + i], cpu_fr[rb + i]);
                }
                tcg_gen_mov_i64(cpu_fr[rc + disp5 * 32], tmp64);
                tcg_temp_free(tmp64);
                break;
            case 0x22:
                /* VEXTW */
                if (disp5 > 7) break;
                tmp64 = tcg_temp_new();
                tmp32 = tcg_temp_new_i32();
                tcg_gen_shri_i64(tmp64, cpu_fr[ra + (disp5 / 2) * 32],
                                 (disp5 % 2) * 32);
                tcg_gen_extrl_i64_i32(tmp32, tmp64);
                gen_helper_memory_to_s(tmp64, tmp32);
                tcg_gen_mov_i64(cpu_fr[rc], tmp64);
                tcg_temp_free(tmp64);
                tcg_temp_free_i32(tmp32);
                break;
            case 0x23:
                /* VEXTF */
                if (disp5 > 3) break;
                tcg_gen_mov_i64(cpu_fr[rc], cpu_fr[ra + disp5 * 32]);
                break;
            case 0x24:
                /* VCPYW */
                tmp64 = tcg_temp_new();
                tmp64_0 = tcg_temp_new();
                /*  FIXME: for debug
                tcg_gen_movi_i64(tmp64, ra);
                gen_helper_v_print(cpu_env, tmp64);
		*/
                tcg_gen_shri_i64(tmp64, cpu_fr[ra], 29);
                tcg_gen_andi_i64(tmp64_0, tmp64, 0x3fffffffUL);
                tcg_gen_shri_i64(tmp64, cpu_fr[ra], 62);
                tcg_gen_shli_i64(tmp64, tmp64, 30);
                tcg_gen_or_i64(tmp64_0, tmp64, tmp64_0);
                tcg_gen_mov_i64(tmp64, tmp64_0);
                tcg_gen_shli_i64(tmp64, tmp64, 32);
                tcg_gen_or_i64(tmp64_0, tmp64_0, tmp64);
                tcg_gen_mov_i64(cpu_fr[rc], tmp64_0);
                tcg_gen_mov_i64(cpu_fr[rc + 32], cpu_fr[rc]);
                tcg_gen_mov_i64(cpu_fr[rc + 64], cpu_fr[rc]);
                tcg_gen_mov_i64(cpu_fr[rc + 96], cpu_fr[rc]);
                /*  FIXME: for debug
                tcg_gen_movi_i64(tmp64, rb);
		gen_helper_v_print(cpu_env, tmp64);
		tcg_gen_movi_i64(tmp64, rc);
                gen_helper_v_print(cpu_env, tmp64);
		*/
                tcg_temp_free(tmp64);
                tcg_temp_free(tmp64_0);
                break;
            case 0x25:
                /* VCPYF */
                for (i = 0; i < 128; i += 32) {
                    tcg_gen_mov_i64(cpu_fr[rc + i], cpu_fr[ra]);
                }
                break;
            case 0x26:
                /* VCONW */
                tmp64 = tcg_const_i64(ra << 8 | rb);
                tmp64_0 = tcg_temp_new();
                vd = tcg_const_i64(rc);
                tcg_gen_shri_i64(tmp64_0, cpu_fr[rd], 2);
                tcg_gen_andi_i64(tmp64_0, tmp64_0, 0x7ul);
                gen_helper_vconw(cpu_env, tmp64, vd, tmp64_0);
                tcg_temp_free(tmp64_0);
                tcg_temp_free(tmp64);
                tcg_temp_free(vd);
                break;
            case 0x27:
                /* VSHFW */
                tmp64 = tcg_const_i64(ra << 8 | rb);
                vd = tcg_const_i64(rc);
                gen_helper_vshfw(cpu_env, tmp64, vd, cpu_fr[rd]);
                tcg_temp_free(tmp64);
                tcg_temp_free(vd);
                break;
            case 0x28:
                /* VCONS */
                tmp64 = tcg_const_i64(ra << 8 | rb);
                tmp64_0 = tcg_temp_new();
                vd = tcg_const_i64(rc);
                tcg_gen_shri_i64(tmp64_0, cpu_fr[rd], 2);
                tcg_gen_andi_i64(tmp64_0, tmp64_0, 0x3ul);
                gen_helper_vcond(cpu_env, tmp64, vd, tmp64_0);
                tcg_temp_free(tmp64_0);
                tcg_temp_free(tmp64);
                tcg_temp_free(vd);
                break;
            case 0x29:
                /* FIXME: VCOND maybe it's wrong in the instruction book
                 * that there are no temp. */
                tmp64 = tcg_const_i64(ra << 8 | rb);
                tmp64_0 = tcg_temp_new();
                vd = tcg_const_i64(rc);
                tcg_gen_shri_i64(tmp64_0, cpu_fr[rd], 3);
                tcg_gen_andi_i64(tmp64_0, tmp64_0, 0x3ul);
                gen_helper_vcond(cpu_env, tmp64, vd, tmp64_0);
                tcg_temp_free(tmp64_0);
                tcg_temp_free(tmp64);
                tcg_temp_free(vd);
                break;
            default:
                printf("ILLEGAL BELOW OPC[%x] func[%08x]\n", opc, fn6);
                ret = gen_invalid(ctx);
                break;
            }
            break;
        case 0x1C:
            switch (fn4) {
            case 0x0:
                /* VLDW_U */
	        if (unlikely(ra == 31)) break;
                gen_load_mem_simd(ctx, &gen_qemu_vldd, ra, rb, disp12,
                                  ~0x1fUL);
                break;
            case 0x1:
                /* VSTW_U */
                gen_store_mem_simd(ctx, &gen_qemu_vstd, ra, rb, disp12,
                                   ~0x1fUL);
                break;
            case 0x2:
                /* VLDS_U */
	        if (unlikely(ra == 31)) break;
                gen_load_mem_simd(ctx, &gen_qemu_vlds, ra, rb, disp12,
                                  ~0xfUL);
                break;
            case 0x3:
                /* VSTS_U */
                gen_store_mem_simd(ctx, &gen_qemu_vsts, ra, rb, disp12,
                                   ~0xfUL);
                break;
            case 0x4:
                /* VLDD_U */
	        if (unlikely(ra == 31)) break;
                gen_load_mem_simd(ctx, &gen_qemu_vldd, ra, rb, disp12,
                                  ~0x1fUL);
                break;
            case 0x5:
                /* VSTD_U */
                gen_store_mem_simd(ctx, &gen_qemu_vstd, ra, rb, disp12,
                                   ~0x1fUL);
                break;
            case 0x8:
                /* VSTW_UL */
                gen_store_mem_simd(ctx, &gen_qemu_vstw_ul, ra, rb, disp12,
                                  0);
                break;
            case 0x9:
                /* VSTW_UH */
                gen_store_mem_simd(ctx, &gen_qemu_vstw_uh, ra, rb, disp12,
                                  0);
                break;
            case 0xa:
                /* VSTS_UL */
                gen_store_mem_simd(ctx, &gen_qemu_vsts_ul, ra, rb, disp12,
                                  0);
                break;
            case 0xb:
                /* VSTS_UH */
                gen_store_mem_simd(ctx, &gen_qemu_vsts_uh, ra, rb, disp12,
                                  0);
                break;
            case 0xc:
                /* VSTD_UL */
                gen_store_mem_simd(ctx, &gen_qemu_vstd_ul, ra, rb, disp12,
                                  0);
                break;
            case 0xd:
                /* VSTD_UH */
                gen_store_mem_simd(ctx, &gen_qemu_vstd_uh, ra, rb, disp12,
                                  0);
                break;
            case 0xe:
                /* VLDD_NC */
                gen_load_mem_simd(ctx, &gen_qemu_vldd, ra, rb, disp12, 0);
                break;
            case 0xf:
                /* VSTD_NC */
                gen_store_mem_simd(ctx, &gen_qemu_vstd, ra, rb, disp12, 0);
                break;
            default:
                printf("ILLEGAL BELOW OPC[%x] func[%08x]\n", opc, fn4);
                ret = gen_invalid(ctx);
                break;
            }
            break;
        case 0x20:
            /* LDBU */
            gen_load_mem(ctx, &tcg_gen_qemu_ld8u, ra, rb, disp16, 0, 0);
            break;
        case 0x21:
            /* LDHU */
            gen_load_mem(ctx, &tcg_gen_qemu_ld16u, ra, rb, disp16, 0, 0);
            break;
        case 0x22:
            /* LDW */
            gen_load_mem(ctx, &tcg_gen_qemu_ld32s, ra, rb, disp16, 0, 0);
            break;
        case 0x23:
            /* LDL */
            gen_load_mem(ctx, &tcg_gen_qemu_ld64, ra, rb, disp16, 0, 0);
            break;
        case 0x24:
            /* LDL_U */
            gen_load_mem(ctx, &tcg_gen_qemu_ld64, ra, rb, disp16, 0, 1);
            break;
        case 0x25:
            /* PRI_LD */
#ifndef CONFIG_USER_ONLY
            if ((insn >> 12) & 1) {
                gen_load_mem(ctx, &gen_qemu_pri_ldl, ra, rb, disp12, 0, 1);
            } else {
                gen_load_mem(ctx, &gen_qemu_pri_ldw, ra, rb, disp12, 0, 1);
            }
#endif
            break;
        case 0x26:
            /* FLDS */
            gen_load_mem(ctx, &gen_qemu_flds, ra, rb, disp16, 1, 0);
            break;
        case 0x27:
            /* FLDD */
            gen_load_mem(ctx, &tcg_gen_qemu_ld64, ra, rb, disp16, 1, 0);
            break;
        case 0x28:
            /* STB */
            gen_store_mem(ctx, &tcg_gen_qemu_st8, ra, rb, disp16, 0, 0);
            break;
        case 0x29:
            /* STH */
            gen_store_mem(ctx, &tcg_gen_qemu_st16, ra, rb, disp16, 0, 0);
            break;
        case 0x2a:
            /* STW */
            gen_store_mem(ctx, &tcg_gen_qemu_st32, ra, rb, disp16, 0, 0);
            break;
        case 0x2b:
            /* STL */
            gen_store_mem(ctx, &tcg_gen_qemu_st64, ra, rb, disp16, 0, 0);
            break;
        case 0x2c:
            /* STL_U */
            gen_store_mem(ctx, &tcg_gen_qemu_st64, ra, rb, disp16, 0, 1);
            break;
        case 0x2d:
            /* PRI_ST */
#ifndef CONFIG_USER_ONLY
            if ((insn >> 12) & 1) {
                gen_store_mem(ctx, &gen_qemu_pri_stl, ra, rb, disp12, 0, 1);
            } else {
                gen_store_mem(ctx, &gen_qemu_pri_stw, ra, rb, disp12, 0, 1);
            }
#endif
            break;
        case 0x2e:
            /* FSTS */
            gen_store_mem(ctx, &gen_qemu_fsts, ra, rb, disp16, 1, 0);
            break;
        case 0x2f:
            /* FSTD */
            gen_store_mem(ctx, &tcg_gen_qemu_st64, ra, rb, disp16, 1, 0);
            break;
        case 0x30:
            /* BEQ */
            ret = gen_bcond(ctx, TCG_COND_EQ, ra, disp21, (uint64_t)-1);
            break;
        case 0x31:
            /* BNE */
            ret = gen_bcond(ctx, TCG_COND_NE, ra, disp21, (uint64_t)-1);
            break;
        case 0x32:
            /* BLT */
            ret = gen_bcond(ctx, TCG_COND_LT, ra, disp21, (uint64_t)-1);
            break;
        case 0x33:
            /* BLE */
            ret = gen_bcond(ctx, TCG_COND_LE, ra, disp21, (uint64_t)-1);
            break;
        case 0x34:
            /* BGT */
            ret = gen_bcond(ctx, TCG_COND_GT, ra, disp21, (uint64_t)-1);
            break;
        case 0x35:
            /* BGE */
            ret = gen_bcond(ctx, TCG_COND_GE, ra, disp21, (uint64_t)-1);
            break;
        case 0x36:
            /* BLBC */
            ret = gen_bcond(ctx, TCG_COND_EQ, ra, disp21, 1);
            break;
        case 0x37:
            /* BLBS */
            ret = gen_bcond(ctx, TCG_COND_NE, ra, disp21, 1);
            break;
        case 0x38:
            /* FBEQ */
            ret = gen_fbcond(ctx, TCG_COND_EQ, ra, disp21);
            break;
        case 0x39:
            /* FBNE */
            ret = gen_fbcond(ctx, TCG_COND_NE, ra, disp21);
            break;
        case 0x3a:
            /* FBLT */
            ret = gen_fbcond(ctx, TCG_COND_LT, ra, disp21);
            break;
        case 0x3b:
            /* FBLE */
            ret = gen_fbcond(ctx, TCG_COND_LE, ra, disp21);
            break;
        case 0x3c:
            /* FBGT */
            ret = gen_fbcond(ctx, TCG_COND_GT, ra, disp21);
            break;
        case 0x3d:
            /* FBGE */
            ret = gen_fbcond(ctx, TCG_COND_GE, ra, disp21);
            break;
        case 0x3f:
            /* LDIH */
            disp16 = ((uint32_t)disp16) << 16;
	    if (ra == 31) break;
            va = load_gir(ctx, ra);
            if (rb == 31) {
                tcg_gen_movi_i64(va, disp16);
            } else {
                tcg_gen_addi_i64(va, load_gir(ctx, rb), (int64_t)disp16);
            }
            break;
        case 0x3e:
            /* LDI */
            if (ra == 31) break;
            va = load_gir(ctx, ra);
            if (rb == 31) {
                tcg_gen_movi_i64(va, disp16);
            } else {
                tcg_gen_addi_i64(va, load_gir(ctx, rb), (int64_t)disp16);
            }
            break;
        do_invalid:
        default:
            printf("ILLEGAL BELOW OPC[%x] insn[%08x]\n", opc, insn);
            ret = gen_invalid(ctx);
    }
    return ret;
}
static void sw64_tr_init_disas_context(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext* ctx = container_of(dcbase, DisasContext, base);
    CPUSW64State* env = cpu->env_ptr; /*init by instance_initfn*/

    ctx->tbflags = ctx->base.tb->flags;
    ctx->mem_idx = cpu_mmu_index(env, false);
#ifdef CONFIG_USER_ONLY
    ctx->ir = cpu_std_ir;
#else
    ctx->ir = (ctx->tbflags & ENV_FLAG_HM_MODE ? cpu_hm_ir : cpu_std_ir);
#endif
    ctx->zero = NULL;
}

static void sw64_tr_tb_start(DisasContextBase *db, CPUState *cpu)
{
}

static void sw64_tr_insn_start(DisasContextBase *dcbase, CPUState *cpu)
{
    tcg_gen_insn_start(dcbase->pc_next);
}

static void sw64_tr_translate_insn(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    CPUSW64State *env = cpu->env_ptr;
    uint32_t insn;

    insn = cpu_ldl_code(env, ctx->base.pc_next & (~3UL));
    ctx->env = env;
    ctx->base.pc_next += 4;
    ctx->base.is_jmp = ctx->translate_one(dcbase, insn, cpu);

    free_context_temps(ctx);
    translator_loop_temp_check(&ctx->base);
}

/* FIXME:Linhainan */
static void sw64_tr_tb_stop(DisasContextBase* dcbase, CPUState* cpu) {
    DisasContext* ctx = container_of(dcbase, DisasContext, base);

    switch (ctx->base.is_jmp) {
        case DISAS_NORETURN:
            break;
        case DISAS_TOO_MANY:
            if (use_goto_tb(ctx, ctx->base.pc_next)) {
                tcg_gen_goto_tb(0);
                tcg_gen_movi_i64(cpu_pc, ctx->base.pc_next);
                tcg_gen_exit_tb(ctx->base.tb, 0);
            }
        /* FALLTHRU */
        case DISAS_PC_STALE:
            tcg_gen_movi_i64(cpu_pc, ctx->base.pc_next);
        /* FALLTHRU */
        case DISAS_PC_UPDATED:
            if (!use_exit_tb(ctx)) {
                tcg_gen_lookup_and_goto_ptr();
                break;
            }
        /* FALLTHRU */
        case DISAS_PC_UPDATED_NOCHAIN:
            if (ctx->base.singlestep_enabled) {
                /* FIXME: for gdb*/
                cpu_loop_exit(cpu);
            } else {
                tcg_gen_exit_tb(NULL, 0);
            }
            break;
        default:
            g_assert_not_reached();
    }
}

static void sw64_tr_disas_log(const DisasContextBase* dcbase, CPUState* cpu) {
    SW64CPU* sc = SW64_CPU(cpu);
    qemu_log("IN(%d): %s\n", sc->cid,
             lookup_symbol(dcbase->pc_first));
    log_target_disas(cpu, dcbase->pc_first & (~0x3UL), dcbase->tb->size);
}

static void init_transops(CPUState *cpu, DisasContext *dc)
{
    dc->translate_one = translate_one;
}

void restore_state_to_opc(CPUSW64State* env, TranslationBlock* tb,
                          target_ulong* data) {
    env->pc = data[0];
}

static const TranslatorOps sw64_trans_ops = {
    .init_disas_context = sw64_tr_init_disas_context,
    .tb_start = sw64_tr_tb_start,
    .insn_start = sw64_tr_insn_start,
    .translate_insn = sw64_tr_translate_insn,
    .tb_stop = sw64_tr_tb_stop,
    .disas_log = sw64_tr_disas_log,
};

void gen_intermediate_code(CPUState* cpu, TranslationBlock* tb, int max_insns)
{
    DisasContext dc;
    init_transops(cpu, &dc);
    translator_loop(&sw64_trans_ops, &dc.base, cpu, tb, max_insns);
}
