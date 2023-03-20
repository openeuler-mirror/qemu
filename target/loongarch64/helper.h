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

DEF_HELPER_3(raise_exception_err, noreturn, env, i32, int)
DEF_HELPER_2(raise_exception, noreturn, env, i32)
DEF_HELPER_1(raise_exception_debug, noreturn, env)

DEF_HELPER_FLAGS_1(bitswap, TCG_CALL_NO_RWG_SE, tl, tl)
DEF_HELPER_FLAGS_1(dbitswap, TCG_CALL_NO_RWG_SE, tl, tl)

DEF_HELPER_3(crc32, tl, tl, tl, i32)
DEF_HELPER_3(crc32c, tl, tl, tl, i32)

#ifndef CONFIG_USER_ONLY
/* LoongISA CSR register */
DEF_HELPER_2(csr_rdq, tl, env, i64)
DEF_HELPER_3(csr_wrq, tl, env, tl, i64)
DEF_HELPER_4(csr_xchgq, tl, env, tl, tl, i64)

#endif /* !CONFIG_USER_ONLY */

/* CP1 functions */
DEF_HELPER_2(movfcsr2gr, tl, env, i32)
DEF_HELPER_4(movgr2fcsr, void, env, tl, i32, i32)

DEF_HELPER_2(float_cvtd_s, i64, env, i32)
DEF_HELPER_2(float_cvtd_w, i64, env, i32)
DEF_HELPER_2(float_cvtd_l, i64, env, i64)
DEF_HELPER_2(float_cvts_d, i32, env, i64)
DEF_HELPER_2(float_cvts_w, i32, env, i32)
DEF_HELPER_2(float_cvts_l, i32, env, i64)

DEF_HELPER_FLAGS_2(float_class_s, TCG_CALL_NO_RWG_SE, i32, env, i32)
DEF_HELPER_FLAGS_2(float_class_d, TCG_CALL_NO_RWG_SE, i64, env, i64)

#define FOP_PROTO(op)                                                         \
    DEF_HELPER_4(float_##op##_s, i32, env, i32, i32, i32)                     \
    DEF_HELPER_4(float_##op##_d, i64, env, i64, i64, i64)
FOP_PROTO(maddf)
FOP_PROTO(msubf)
FOP_PROTO(nmaddf)
FOP_PROTO(nmsubf)
#undef FOP_PROTO

#define FOP_PROTO(op)                                                         \
    DEF_HELPER_3(float_##op##_s, i32, env, i32, i32)                          \
    DEF_HELPER_3(float_##op##_d, i64, env, i64, i64)
FOP_PROTO(max)
FOP_PROTO(maxa)
FOP_PROTO(min)
FOP_PROTO(mina)
#undef FOP_PROTO

#define FOP_PROTO(op)                                                         \
    DEF_HELPER_2(float_##op##_l_s, i64, env, i32)                             \
    DEF_HELPER_2(float_##op##_l_d, i64, env, i64)                             \
    DEF_HELPER_2(float_##op##_w_s, i32, env, i32)                             \
    DEF_HELPER_2(float_##op##_w_d, i32, env, i64)
FOP_PROTO(cvt)
FOP_PROTO(round)
FOP_PROTO(trunc)
FOP_PROTO(ceil)
FOP_PROTO(floor)
#undef FOP_PROTO

#define FOP_PROTO(op)                                                         \
    DEF_HELPER_2(float_##op##_s, i32, env, i32)                               \
    DEF_HELPER_2(float_##op##_d, i64, env, i64)
FOP_PROTO(sqrt)
FOP_PROTO(rsqrt)
FOP_PROTO(recip)
FOP_PROTO(rint)
#undef FOP_PROTO

#define FOP_PROTO(op)                                                         \
    DEF_HELPER_1(float_##op##_s, i32, i32)                                    \
    DEF_HELPER_1(float_##op##_d, i64, i64)
FOP_PROTO(abs)
FOP_PROTO(chs)
#undef FOP_PROTO

#define FOP_PROTO(op)                                                         \
    DEF_HELPER_3(float_##op##_s, i32, env, i32, i32)                          \
    DEF_HELPER_3(float_##op##_d, i64, env, i64, i64)
FOP_PROTO(add)
FOP_PROTO(sub)
FOP_PROTO(mul)
FOP_PROTO(div)
#undef FOP_PROTO

#define FOP_PROTO(op)                                                         \
    DEF_HELPER_3(cmp_d_##op, i64, env, i64, i64)                              \
    DEF_HELPER_3(cmp_s_##op, i32, env, i32, i32)
FOP_PROTO(af)
FOP_PROTO(un)
FOP_PROTO(eq)
FOP_PROTO(ueq)
FOP_PROTO(lt)
FOP_PROTO(ult)
FOP_PROTO(le)
FOP_PROTO(ule)
FOP_PROTO(saf)
FOP_PROTO(sun)
FOP_PROTO(seq)
FOP_PROTO(sueq)
FOP_PROTO(slt)
FOP_PROTO(sult)
FOP_PROTO(sle)
FOP_PROTO(sule)
FOP_PROTO(or)
FOP_PROTO(une)
FOP_PROTO(ne)
FOP_PROTO(sor)
FOP_PROTO(sune)
FOP_PROTO(sne)
#undef FOP_PROTO

/* Special functions */
#ifndef CONFIG_USER_ONLY
DEF_HELPER_1(tlbwr, void, env)
DEF_HELPER_1(tlbfill, void, env)
DEF_HELPER_1(tlbsrch, void, env)
DEF_HELPER_1(tlbrd, void, env)
DEF_HELPER_1(tlbclr, void, env)
DEF_HELPER_1(tlbflush, void, env)
DEF_HELPER_4(invtlb, void, env, tl, tl, tl)
DEF_HELPER_1(ertn, void, env)
DEF_HELPER_5(lddir, void, env, tl, tl, tl, i32)
DEF_HELPER_4(ldpte, void, env, tl, tl, i32)
DEF_HELPER_3(drdtime, void, env, tl, tl)
DEF_HELPER_1(read_pgd, tl, env)
#endif /* !CONFIG_USER_ONLY */
DEF_HELPER_2(cpucfg, tl, env, tl)
DEF_HELPER_1(idle, void, env)

DEF_HELPER_3(float_exp2_s, i32, env, i32, i32)
DEF_HELPER_3(float_exp2_d, i64, env, i64, i64)
DEF_HELPER_2(float_logb_s, i32, env, i32)
DEF_HELPER_2(float_logb_d, i64, env, i64)
DEF_HELPER_3(movreg2cf, void, env, i32, tl)
DEF_HELPER_2(movcf2reg, tl, env, i32)
DEF_HELPER_3(movreg2cf_i32, void, env, i32, i32)
DEF_HELPER_3(movreg2cf_i64, void, env, i32, i64)

DEF_HELPER_2(cto_w, tl, env, tl)
DEF_HELPER_2(ctz_w, tl, env, tl)
DEF_HELPER_2(cto_d, tl, env, tl)
DEF_HELPER_2(ctz_d, tl, env, tl)
DEF_HELPER_2(bitrev_w, tl, env, tl)
DEF_HELPER_2(bitrev_d, tl, env, tl)

DEF_HELPER_2(load_scr, i64, env, i32)
DEF_HELPER_3(store_scr, void, env, i32, i64)

DEF_HELPER_3(asrtle_d, void, env, tl, tl)
DEF_HELPER_3(asrtgt_d, void, env, tl, tl)

DEF_HELPER_4(fsel, i64, env, i64, i64, i32)

#ifndef CONFIG_USER_ONLY
DEF_HELPER_4(iocsr, void, env, tl, tl, i32)
#endif
DEF_HELPER_3(memtrace_addr, void, env, tl, i32)
DEF_HELPER_2(memtrace_val, void, env, tl)
