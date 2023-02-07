/*
 * loongarch float point emulation helpers for qemu.
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
#include "qemu/host-utils.h"
#include "exec/helper-proto.h"
#include "exec/exec-all.h"
#include "fpu/softfloat.h"

#define FP_TO_INT32_OVERFLOW 0x7fffffff
#define FP_TO_INT64_OVERFLOW 0x7fffffffffffffffULL

#define FLOAT_CLASS_SIGNALING_NAN 0x001
#define FLOAT_CLASS_QUIET_NAN 0x002
#define FLOAT_CLASS_NEGATIVE_INFINITY 0x004
#define FLOAT_CLASS_NEGATIVE_NORMAL 0x008
#define FLOAT_CLASS_NEGATIVE_SUBNORMAL 0x010
#define FLOAT_CLASS_NEGATIVE_ZERO 0x020
#define FLOAT_CLASS_POSITIVE_INFINITY 0x040
#define FLOAT_CLASS_POSITIVE_NORMAL 0x080
#define FLOAT_CLASS_POSITIVE_SUBNORMAL 0x100
#define FLOAT_CLASS_POSITIVE_ZERO 0x200

target_ulong helper_movfcsr2gr(CPULOONGARCHState *env, uint32_t reg)
{
    target_ulong r = 0;

    switch (reg) {
    case 0:
        r = (uint32_t)env->active_fpu.fcsr0;
        break;
    case 1:
        r = (env->active_fpu.fcsr0 & FCSR0_M1);
        break;
    case 2:
        r = (env->active_fpu.fcsr0 & FCSR0_M2);
        break;
    case 3:
        r = (env->active_fpu.fcsr0 & FCSR0_M3);
        break;
    case 16:
        r = (uint32_t)env->active_fpu.vcsr16;
        break;
    default:
        printf("%s: warning, fcsr '%d' not supported\n", __func__, reg);
        assert(0);
        break;
    }

    return r;
}

void helper_movgr2fcsr(CPULOONGARCHState *env, target_ulong arg1,
                       uint32_t fcsr, uint32_t rj)
{
    switch (fcsr) {
    case 0:
        env->active_fpu.fcsr0 = arg1;
        break;
    case 1:
        env->active_fpu.fcsr0 =
            (arg1 & FCSR0_M1) | (env->active_fpu.fcsr0 & ~FCSR0_M1);
        break;
    case 2:
        env->active_fpu.fcsr0 =
            (arg1 & FCSR0_M2) | (env->active_fpu.fcsr0 & ~FCSR0_M2);
        break;
    case 3:
        env->active_fpu.fcsr0 =
            (arg1 & FCSR0_M3) | (env->active_fpu.fcsr0 & ~FCSR0_M3);
        break;
    case 16:
        env->active_fpu.vcsr16 = arg1;
        break;
    default:
        printf("%s: warning, fcsr '%d' not supported\n", __func__, fcsr);
        assert(0);
        break;
    }
    restore_fp_status(env);
    set_float_exception_flags(0, &env->active_fpu.fp_status);
}

void helper_movreg2cf(CPULOONGARCHState *env, uint32_t cd, target_ulong src)
{
    env->active_fpu.cf[cd & 0x7] = src & 0x1;
}

void helper_movreg2cf_i32(CPULOONGARCHState *env, uint32_t cd, uint32_t src)
{
    env->active_fpu.cf[cd & 0x7] = src & 0x1;
}

void helper_movreg2cf_i64(CPULOONGARCHState *env, uint32_t cd, uint64_t src)
{
    env->active_fpu.cf[cd & 0x7] = src & 0x1;
}

target_ulong helper_movcf2reg(CPULOONGARCHState *env, uint32_t cj)
{
    return (target_ulong)env->active_fpu.cf[cj & 0x7];
}

int ieee_ex_to_loongarch(int xcpt)
{
    int ret = 0;
    if (xcpt) {
        if (xcpt & float_flag_invalid) {
            ret |= FP_INVALID;
        }
        if (xcpt & float_flag_overflow) {
            ret |= FP_OVERFLOW;
        }
        if (xcpt & float_flag_underflow) {
            ret |= FP_UNDERFLOW;
        }
        if (xcpt & float_flag_divbyzero) {
            ret |= FP_DIV0;
        }
        if (xcpt & float_flag_inexact) {
            ret |= FP_INEXACT;
        }
    }
    return ret;
}

static inline void update_fcsr0(CPULOONGARCHState *env, uintptr_t pc)
{
    int tmp = ieee_ex_to_loongarch(
        get_float_exception_flags(&env->active_fpu.fp_status));

    SET_FP_CAUSE(env->active_fpu.fcsr0, tmp);
    if (tmp) {
        set_float_exception_flags(0, &env->active_fpu.fp_status);

        if (GET_FP_ENABLE(env->active_fpu.fcsr0) & tmp) {
            do_raise_exception(env, EXCP_FPE, pc);
        } else {
            UPDATE_FP_FLAGS(env->active_fpu.fcsr0, tmp);
        }
    }
}

/* unary operations, modifying fp status  */
uint64_t helper_float_sqrt_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    fdt0 = float64_sqrt(fdt0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fdt0;
}

uint32_t helper_float_sqrt_s(CPULOONGARCHState *env, uint32_t fst0)
{
    fst0 = float32_sqrt(fst0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fst0;
}

uint64_t helper_float_cvtd_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint64_t fdt2;

    fdt2 = float32_to_float64(fst0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fdt2;
}

uint64_t helper_float_cvtd_w(CPULOONGARCHState *env, uint32_t wt0)
{
    uint64_t fdt2;

    fdt2 = int32_to_float64(wt0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fdt2;
}

uint64_t helper_float_cvtd_l(CPULOONGARCHState *env, uint64_t dt0)
{
    uint64_t fdt2;

    fdt2 = int64_to_float64(dt0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fdt2;
}

uint64_t helper_float_cvt_l_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint64_t dt2;

    dt2 = float64_to_int64(fdt0, &env->active_fpu.fp_status);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint64_t helper_float_cvt_l_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint64_t dt2;

    dt2 = float32_to_int64(fst0, &env->active_fpu.fp_status);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint32_t helper_float_cvts_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint32_t fst2;

    fst2 = float64_to_float32(fdt0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fst2;
}

uint32_t helper_float_cvts_w(CPULOONGARCHState *env, uint32_t wt0)
{
    uint32_t fst2;

    fst2 = int32_to_float32(wt0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fst2;
}

uint32_t helper_float_cvts_l(CPULOONGARCHState *env, uint64_t dt0)
{
    uint32_t fst2;

    fst2 = int64_to_float32(dt0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fst2;
}

uint32_t helper_float_cvt_w_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint32_t wt2;

    wt2 = float32_to_int32(fst0, &env->active_fpu.fp_status);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint32_t helper_float_cvt_w_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint32_t wt2;

    wt2 = float64_to_int32(fdt0, &env->active_fpu.fp_status);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint64_t helper_float_round_l_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint64_t dt2;

    set_float_rounding_mode(float_round_nearest_even,
                            &env->active_fpu.fp_status);
    dt2 = float64_to_int64(fdt0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint64_t helper_float_round_l_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint64_t dt2;

    set_float_rounding_mode(float_round_nearest_even,
                            &env->active_fpu.fp_status);
    dt2 = float32_to_int64(fst0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint32_t helper_float_round_w_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint32_t wt2;

    set_float_rounding_mode(float_round_nearest_even,
                            &env->active_fpu.fp_status);
    wt2 = float64_to_int32(fdt0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint32_t helper_float_round_w_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint32_t wt2;

    set_float_rounding_mode(float_round_nearest_even,
                            &env->active_fpu.fp_status);
    wt2 = float32_to_int32(fst0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint64_t helper_float_trunc_l_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint64_t dt2;

    dt2 = float64_to_int64_round_to_zero(fdt0, &env->active_fpu.fp_status);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint64_t helper_float_trunc_l_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint64_t dt2;

    dt2 = float32_to_int64_round_to_zero(fst0, &env->active_fpu.fp_status);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint32_t helper_float_trunc_w_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint32_t wt2;

    wt2 = float64_to_int32_round_to_zero(fdt0, &env->active_fpu.fp_status);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint32_t helper_float_trunc_w_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint32_t wt2;

    wt2 = float32_to_int32_round_to_zero(fst0, &env->active_fpu.fp_status);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint64_t helper_float_ceil_l_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint64_t dt2;

    set_float_rounding_mode(float_round_up, &env->active_fpu.fp_status);
    dt2 = float64_to_int64(fdt0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint64_t helper_float_ceil_l_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint64_t dt2;

    set_float_rounding_mode(float_round_up, &env->active_fpu.fp_status);
    dt2 = float32_to_int64(fst0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint32_t helper_float_ceil_w_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint32_t wt2;

    set_float_rounding_mode(float_round_up, &env->active_fpu.fp_status);
    wt2 = float64_to_int32(fdt0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint32_t helper_float_ceil_w_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint32_t wt2;

    set_float_rounding_mode(float_round_up, &env->active_fpu.fp_status);
    wt2 = float32_to_int32(fst0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint64_t helper_float_floor_l_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint64_t dt2;

    set_float_rounding_mode(float_round_down, &env->active_fpu.fp_status);
    dt2 = float64_to_int64(fdt0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint64_t helper_float_floor_l_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint64_t dt2;

    set_float_rounding_mode(float_round_down, &env->active_fpu.fp_status);
    dt2 = float32_to_int64(fst0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        dt2 = FP_TO_INT64_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return dt2;
}

uint32_t helper_float_floor_w_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint32_t wt2;

    set_float_rounding_mode(float_round_down, &env->active_fpu.fp_status);
    wt2 = float64_to_int32(fdt0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

uint32_t helper_float_floor_w_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint32_t wt2;

    set_float_rounding_mode(float_round_down, &env->active_fpu.fp_status);
    wt2 = float32_to_int32(fst0, &env->active_fpu.fp_status);
    restore_rounding_mode(env);
    if (get_float_exception_flags(&env->active_fpu.fp_status) &
        (float_flag_invalid | float_flag_overflow)) {
        wt2 = FP_TO_INT32_OVERFLOW;
    }
    update_fcsr0(env, GETPC());
    return wt2;
}

/* unary operations, not modifying fp status  */
#define FLOAT_UNOP(name)                                                      \
    uint64_t helper_float_##name##_d(uint64_t fdt0)                           \
    {                                                                         \
        return float64_##name(fdt0);                                          \
    }                                                                         \
    uint32_t helper_float_##name##_s(uint32_t fst0)                           \
    {                                                                         \
        return float32_##name(fst0);                                          \
    }

FLOAT_UNOP(abs)
FLOAT_UNOP(chs)
#undef FLOAT_UNOP

uint64_t helper_float_recip_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint64_t fdt2;

    fdt2 = float64_div(float64_one, fdt0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fdt2;
}

uint32_t helper_float_recip_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint32_t fst2;

    fst2 = float32_div(float32_one, fst0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fst2;
}

uint64_t helper_float_rsqrt_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint64_t fdt2;

    fdt2 = float64_sqrt(fdt0, &env->active_fpu.fp_status);
    fdt2 = float64_div(float64_one, fdt2, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fdt2;
}

uint32_t helper_float_rsqrt_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint32_t fst2;

    fst2 = float32_sqrt(fst0, &env->active_fpu.fp_status);
    fst2 = float32_div(float32_one, fst2, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fst2;
}

uint32_t helper_float_rint_s(CPULOONGARCHState *env, uint32_t fs)
{
    uint32_t fdret;

    fdret = float32_round_to_int(fs, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fdret;
}

uint64_t helper_float_rint_d(CPULOONGARCHState *env, uint64_t fs)
{
    uint64_t fdret;

    fdret = float64_round_to_int(fs, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return fdret;
}

#define FLOAT_CLASS(name, bits)                                               \
    uint##bits##_t float_##name(uint##bits##_t arg, float_status *status)     \
    {                                                                         \
        if (float##bits##_is_signaling_nan(arg, status)) {                    \
            return FLOAT_CLASS_SIGNALING_NAN;                                 \
        } else if (float##bits##_is_quiet_nan(arg, status)) {                 \
            return FLOAT_CLASS_QUIET_NAN;                                     \
        } else if (float##bits##_is_neg(arg)) {                               \
            if (float##bits##_is_infinity(arg)) {                             \
                return FLOAT_CLASS_NEGATIVE_INFINITY;                         \
            } else if (float##bits##_is_zero(arg)) {                          \
                return FLOAT_CLASS_NEGATIVE_ZERO;                             \
            } else if (float##bits##_is_zero_or_denormal(arg)) {              \
                return FLOAT_CLASS_NEGATIVE_SUBNORMAL;                        \
            } else {                                                          \
                return FLOAT_CLASS_NEGATIVE_NORMAL;                           \
            }                                                                 \
        } else {                                                              \
            if (float##bits##_is_infinity(arg)) {                             \
                return FLOAT_CLASS_POSITIVE_INFINITY;                         \
            } else if (float##bits##_is_zero(arg)) {                          \
                return FLOAT_CLASS_POSITIVE_ZERO;                             \
            } else if (float##bits##_is_zero_or_denormal(arg)) {              \
                return FLOAT_CLASS_POSITIVE_SUBNORMAL;                        \
            } else {                                                          \
                return FLOAT_CLASS_POSITIVE_NORMAL;                           \
            }                                                                 \
        }                                                                     \
    }                                                                         \
                                                                              \
    uint##bits##_t helper_float_##name(CPULOONGARCHState *env,                \
                                       uint##bits##_t arg)                    \
    {                                                                         \
        return float_##name(arg, &env->active_fpu.fp_status);                 \
    }

FLOAT_CLASS(class_s, 32)
FLOAT_CLASS(class_d, 64)
#undef FLOAT_CLASS

/* binary operations */
#define FLOAT_BINOP(name)                                                     \
    uint64_t helper_float_##name##_d(CPULOONGARCHState *env, uint64_t fdt0,   \
                                     uint64_t fdt1)                           \
    {                                                                         \
        uint64_t dt2;                                                         \
                                                                              \
        dt2 = float64_##name(fdt0, fdt1, &env->active_fpu.fp_status);         \
        update_fcsr0(env, GETPC());                                           \
        return dt2;                                                           \
    }                                                                         \
                                                                              \
    uint32_t helper_float_##name##_s(CPULOONGARCHState *env, uint32_t fst0,   \
                                     uint32_t fst1)                           \
    {                                                                         \
        uint32_t wt2;                                                         \
                                                                              \
        wt2 = float32_##name(fst0, fst1, &env->active_fpu.fp_status);         \
        update_fcsr0(env, GETPC());                                           \
        return wt2;                                                           \
    }

FLOAT_BINOP(add)
FLOAT_BINOP(sub)
FLOAT_BINOP(mul)
FLOAT_BINOP(div)
#undef FLOAT_BINOP

uint64_t helper_float_exp2_d(CPULOONGARCHState *env, uint64_t fdt0,
                             uint64_t fdt1)
{
    uint64_t dt2;
    int64_t n = (int64_t)fdt1;

    dt2 = float64_scalbn(fdt0, n > 0x1000 ? 0x1000 : n < -0x1000 ? -0x1000 : n,
                         &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return dt2;
}

uint32_t helper_float_exp2_s(CPULOONGARCHState *env, uint32_t fst0,
                             uint32_t fst1)
{
    uint32_t wt2;
    int32_t n = (int32_t)fst1;

    wt2 = float32_scalbn(fst0, n > 0x200 ? 0x200 : n < -0x200 ? -0x200 : n,
                         &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return wt2;
}

#define FLOAT_MINMAX(name, bits, minmaxfunc)                                  \
    uint##bits##_t helper_float_##name(CPULOONGARCHState *env,                \
                                       uint##bits##_t fs, uint##bits##_t ft)  \
    {                                                                         \
        uint##bits##_t fdret;                                                 \
                                                                              \
        fdret =                                                               \
            float##bits##_##minmaxfunc(fs, ft, &env->active_fpu.fp_status);   \
        update_fcsr0(env, GETPC());                                           \
        return fdret;                                                         \
    }

FLOAT_MINMAX(max_s, 32, maxnum)
FLOAT_MINMAX(max_d, 64, maxnum)
FLOAT_MINMAX(maxa_s, 32, maxnummag)
FLOAT_MINMAX(maxa_d, 64, maxnummag)

FLOAT_MINMAX(min_s, 32, minnum)
FLOAT_MINMAX(min_d, 64, minnum)
FLOAT_MINMAX(mina_s, 32, minnummag)
FLOAT_MINMAX(mina_d, 64, minnummag)
#undef FLOAT_MINMAX

#define FLOAT_FMADDSUB(name, bits, muladd_arg)                                \
    uint##bits##_t helper_float_##name(CPULOONGARCHState *env,                \
                                       uint##bits##_t fs, uint##bits##_t ft,  \
                                       uint##bits##_t fd)                     \
    {                                                                         \
        uint##bits##_t fdret;                                                 \
                                                                              \
        fdret = float##bits##_muladd(fs, ft, fd, muladd_arg,                  \
                                     &env->active_fpu.fp_status);             \
        update_fcsr0(env, GETPC());                                           \
        return fdret;                                                         \
    }

FLOAT_FMADDSUB(maddf_s, 32, 0)
FLOAT_FMADDSUB(maddf_d, 64, 0)
FLOAT_FMADDSUB(msubf_s, 32, float_muladd_negate_c)
FLOAT_FMADDSUB(msubf_d, 64, float_muladd_negate_c)
FLOAT_FMADDSUB(nmaddf_s, 32, float_muladd_negate_result)
FLOAT_FMADDSUB(nmaddf_d, 64, float_muladd_negate_result)
FLOAT_FMADDSUB(nmsubf_s, 32,
               float_muladd_negate_result | float_muladd_negate_c)
FLOAT_FMADDSUB(nmsubf_d, 64,
               float_muladd_negate_result | float_muladd_negate_c)
#undef FLOAT_FMADDSUB

/* compare operations */
#define FOP_CONDN_D(op, cond)                                                 \
    uint64_t helper_cmp_d_##op(CPULOONGARCHState *env, uint64_t fdt0,         \
                               uint64_t fdt1)                                 \
    {                                                                         \
        uint64_t c;                                                           \
        c = cond;                                                             \
        update_fcsr0(env, GETPC());                                           \
        if (c) {                                                              \
            return -1;                                                        \
        } else {                                                              \
            return 0;                                                         \
        }                                                                     \
    }

/*
 * NOTE: the comma operator will make "cond" to eval to false,
 * but float64_unordered_quiet() is still called.
 */
FOP_CONDN_D(af,
            (float64_unordered_quiet(fdt1, fdt0, &env->active_fpu.fp_status),
             0))
FOP_CONDN_D(un,
            (float64_unordered_quiet(fdt1, fdt0, &env->active_fpu.fp_status)))
FOP_CONDN_D(eq, (float64_eq_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(ueq,
            (float64_unordered_quiet(fdt1, fdt0, &env->active_fpu.fp_status) ||
             float64_eq_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(lt, (float64_lt_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(ult,
            (float64_unordered_quiet(fdt1, fdt0, &env->active_fpu.fp_status) ||
             float64_lt_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(le, (float64_le_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(ule,
            (float64_unordered_quiet(fdt1, fdt0, &env->active_fpu.fp_status) ||
             float64_le_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
/*
 * NOTE: the comma operator will make "cond" to eval to false,
 * but float64_unordered() is still called.
 */
FOP_CONDN_D(saf,
            (float64_unordered(fdt1, fdt0, &env->active_fpu.fp_status), 0))
FOP_CONDN_D(sun, (float64_unordered(fdt1, fdt0, &env->active_fpu.fp_status)))
FOP_CONDN_D(seq, (float64_eq(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(sueq, (float64_unordered(fdt1, fdt0, &env->active_fpu.fp_status) ||
                   float64_eq(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(slt, (float64_lt(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(sult, (float64_unordered(fdt1, fdt0, &env->active_fpu.fp_status) ||
                   float64_lt(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(sle, (float64_le(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(sule, (float64_unordered(fdt1, fdt0, &env->active_fpu.fp_status) ||
                   float64_le(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(or, (float64_le_quiet(fdt1, fdt0, &env->active_fpu.fp_status) ||
                 float64_le_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(une,
            (float64_unordered_quiet(fdt1, fdt0, &env->active_fpu.fp_status) ||
             float64_lt_quiet(fdt1, fdt0, &env->active_fpu.fp_status) ||
             float64_lt_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(ne, (float64_lt_quiet(fdt1, fdt0, &env->active_fpu.fp_status) ||
                 float64_lt_quiet(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(sor, (float64_le(fdt1, fdt0, &env->active_fpu.fp_status) ||
                  float64_le(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(sune, (float64_unordered(fdt1, fdt0, &env->active_fpu.fp_status) ||
                   float64_lt(fdt1, fdt0, &env->active_fpu.fp_status) ||
                   float64_lt(fdt0, fdt1, &env->active_fpu.fp_status)))
FOP_CONDN_D(sne, (float64_lt(fdt1, fdt0, &env->active_fpu.fp_status) ||
                  float64_lt(fdt0, fdt1, &env->active_fpu.fp_status)))

#define FOP_CONDN_S(op, cond)                                                 \
    uint32_t helper_cmp_s_##op(CPULOONGARCHState *env, uint32_t fst0,         \
                               uint32_t fst1)                                 \
    {                                                                         \
        uint64_t c;                                                           \
        c = cond;                                                             \
        update_fcsr0(env, GETPC());                                           \
        if (c) {                                                              \
            return -1;                                                        \
        } else {                                                              \
            return 0;                                                         \
        }                                                                     \
    }

/*
 * NOTE: the comma operator will make "cond" to eval to false,
 * but float32_unordered_quiet() is still called.
 */
FOP_CONDN_S(af,
            (float32_unordered_quiet(fst1, fst0, &env->active_fpu.fp_status),
             0))
FOP_CONDN_S(un,
            (float32_unordered_quiet(fst1, fst0, &env->active_fpu.fp_status)))
FOP_CONDN_S(eq, (float32_eq_quiet(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(ueq,
            (float32_unordered_quiet(fst1, fst0, &env->active_fpu.fp_status) ||
             float32_eq_quiet(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(lt, (float32_lt_quiet(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(ult,
            (float32_unordered_quiet(fst1, fst0, &env->active_fpu.fp_status) ||
             float32_lt_quiet(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(le, (float32_le_quiet(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(ule,
            (float32_unordered_quiet(fst1, fst0, &env->active_fpu.fp_status) ||
             float32_le_quiet(fst0, fst1, &env->active_fpu.fp_status)))
/*
 * NOTE: the comma operator will make "cond" to eval to false,
 * but float32_unordered() is still called.
 */
FOP_CONDN_S(saf,
            (float32_unordered(fst1, fst0, &env->active_fpu.fp_status), 0))
FOP_CONDN_S(sun, (float32_unordered(fst1, fst0, &env->active_fpu.fp_status)))
FOP_CONDN_S(seq, (float32_eq(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(sueq, (float32_unordered(fst1, fst0, &env->active_fpu.fp_status) ||
                   float32_eq(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(slt, (float32_lt(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(sult, (float32_unordered(fst1, fst0, &env->active_fpu.fp_status) ||
                   float32_lt(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(sle, (float32_le(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(sule, (float32_unordered(fst1, fst0, &env->active_fpu.fp_status) ||
                   float32_le(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(or, (float32_le_quiet(fst1, fst0, &env->active_fpu.fp_status) ||
                 float32_le_quiet(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(une,
            (float32_unordered_quiet(fst1, fst0, &env->active_fpu.fp_status) ||
             float32_lt_quiet(fst1, fst0, &env->active_fpu.fp_status) ||
             float32_lt_quiet(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(ne, (float32_lt_quiet(fst1, fst0, &env->active_fpu.fp_status) ||
                 float32_lt_quiet(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(sor, (float32_le(fst1, fst0, &env->active_fpu.fp_status) ||
                  float32_le(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(sune, (float32_unordered(fst1, fst0, &env->active_fpu.fp_status) ||
                   float32_lt(fst1, fst0, &env->active_fpu.fp_status) ||
                   float32_lt(fst0, fst1, &env->active_fpu.fp_status)))
FOP_CONDN_S(sne, (float32_lt(fst1, fst0, &env->active_fpu.fp_status) ||
                  float32_lt(fst0, fst1, &env->active_fpu.fp_status)))

uint32_t helper_float_logb_s(CPULOONGARCHState *env, uint32_t fst0)
{
    uint32_t wt2;

    wt2 = float32_log2(fst0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return wt2;
}

uint64_t helper_float_logb_d(CPULOONGARCHState *env, uint64_t fdt0)
{
    uint64_t dt2;

    dt2 = float64_log2(fdt0, &env->active_fpu.fp_status);
    update_fcsr0(env, GETPC());
    return dt2;
}

target_ulong helper_fsel(CPULOONGARCHState *env, target_ulong fj,
                         target_ulong fk, uint32_t ca)
{
    if (env->active_fpu.cf[ca & 0x7]) {
        return fk;
    } else {
        return fj;
    }
}
