#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "fpu/softfloat.h"

static inline uint32_t extractFloat16Frac(float16 a)
{
    return float16_val(a) & 0x3ff;
}

/*----------------------------------------------------------------------------
| Returns the exponent bits of the half-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

static inline int extractFloat16Exp(float16 a)
{
    return (float16_val(a) >> 10) & 0x1f;
}

/*----------------------------------------------------------------------------
| Returns the sign bit of the single-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

static inline uint8_t extractFloat16Sign(float16 a)
{
    return float16_val(a) >> 15;
}

#define FP_STATUS (env->fp_status)

#define CONVERT_BIT(X, SRC, DST) \
    (SRC > DST ? (X) / (SRC / DST) & (DST) : ((X)&SRC) * (DST / SRC))

static uint64_t soft_to_errcode_exc(CPUSW64State *env)
{
    uint8_t exc = get_float_exception_flags(&FP_STATUS);

    if (unlikely(exc)) {
        set_float_exception_flags(0, &FP_STATUS);
    }
    return exc;
}

static inline uint64_t float32_to_s_int(uint32_t fi)
{
    uint32_t frac = fi & 0x7fffff;
    uint32_t sign = (fi >> 31) & 1;
    uint32_t exp_msb = (fi >> 30) & 1;
    uint32_t exp_low = (fi >> 23) & 0x7f;
    uint32_t exp;

    exp = (exp_msb << 10) | exp_low;
    if (exp_msb) {
        if (exp_low == 0x7f) {
            exp = 0x7ff;
        }
    } else {
        if (exp_low != 0x00) {
            exp |= 0x380;
        }
    }

    return (((uint64_t)sign << 63) | ((uint64_t)exp << 52) |
            ((uint64_t)frac << 29));
}

static inline uint64_t float32_to_s(float32 fa)
{
    CPU_FloatU a;
    a.f = fa;
    return float32_to_s_int(a.l);
}
static inline uint32_t s_to_float32_int(uint64_t a)
{
    return ((a >> 32) & 0xc0000000) | ((a >> 29) & 0x3fffffff);
}

static inline float32 s_to_float32(uint64_t a)
{
    CPU_FloatU r;
    r.l = s_to_float32_int(a);
    return r.f;
}

uint32_t helper_s_to_memory(uint64_t a)
{
    return s_to_float32(a);
}

uint64_t helper_memory_to_s(uint32_t a)
{
    return float32_to_s(a);
}

uint64_t helper_fcvtls(CPUSW64State *env, uint64_t a)
{
    float32 fr = int64_to_float32(a, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
    return float32_to_s(fr);
}

uint64_t helper_fcvtld(CPUSW64State *env, uint64_t a)
{
    float64 fr = int64_to_float64(a, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
    return (uint64_t)fr;
}

static uint64_t do_fcvtdl(CPUSW64State *env, uint64_t a, uint64_t roundmode)
{
    uint64_t frac, ret = 0;
    uint32_t exp, sign, exc = 0;
    int shift;

    sign = (a >> 63);
    exp = (uint32_t)(a >> 52) & 0x7ff;
    frac = a & 0xfffffffffffffull;

    if (exp == 0) {
        if (unlikely(frac != 0) && !env->fp_status.flush_inputs_to_zero) {
            goto do_underflow;
        }
    } else if (exp == 0x7ff) {
        exc = float_flag_invalid;
    } else {
        /* Restore implicit bit.  */
        frac |= 0x10000000000000ull;

        shift = exp - 1023 - 52;
        if (shift >= 0) {
            /* In this case the number is so large that we must shift
               the fraction left.  There is no rounding to do.  */
            if (shift < 64) {
                ret = frac << shift;
            }
            /* Check for overflow.  Note the special case of -0x1p63.  */
            if (shift >= 11 && a != 0xC3E0000000000000ull) {
                exc = float_flag_inexact;
            }
        } else {
            uint64_t round;

            /* In this case the number is smaller than the fraction as
               represented by the 52 bit number.  Here we must think
               about rounding the result.  Handle this by shifting the
               fractional part of the number into the high bits of ROUND.
               This will let us efficiently handle round-to-nearest.  */
            shift = -shift;
            if (shift < 63) {
                ret = frac >> shift;
                round = frac << (64 - shift);
            } else {
            /* The exponent is so small we shift out everything.
                   Leave a sticky bit for proper rounding below.  */
            do_underflow:
                round = 1;
            }

            if (round) {
                exc = float_flag_inexact;
                switch (roundmode) {
                    case float_round_nearest_even:
                        if (round == (1ull << 63)) {
                            /* Fraction is exactly 0.5; round to even.  */
                            ret += (ret & 1);
                        } else if (round > (1ull << 63)) {
                            ret += 1;
                        }
                        break;
                    case float_round_to_zero:
                        break;
                    case float_round_up:
                        ret += 1 - sign;
                        break;
                    case float_round_down:
                        ret += sign;
                        break;
                }
            }
        }
        if (sign) {
            ret = -ret;
        }
    }
    env->error_code = exc;

    return ret;
}

/* TODO: */
uint64_t helper_fris(CPUSW64State *env, uint64_t a, uint64_t roundmode)
{
    uint64_t ir;
    float32 fr;

    if (roundmode == 5)
        roundmode = env->fpcr_round_mode;
    ir = do_fcvtdl(env, a, roundmode);
    fr = int64_to_float32(ir, &FP_STATUS);
    return float32_to_s(fr);
}

/* TODO: */
uint64_t helper_frid(CPUSW64State *env, uint64_t a, uint64_t roundmode)
{
    if (roundmode == 5)
        roundmode = env->fpcr_round_mode;
    return int64_to_float64(do_fcvtdl(env, a, roundmode), &FP_STATUS);
}

uint64_t helper_fcvtdl(CPUSW64State *env, uint64_t a, uint64_t roundmode)
{
    return do_fcvtdl(env, a, roundmode);
}

uint64_t helper_fcvtdl_dyn(CPUSW64State *env, uint64_t a)
{
    uint64_t roundmode = (uint64_t)(env->fpcr_round_mode);
    return do_fcvtdl(env, a, roundmode);
}

uint64_t helper_fcvtsd(CPUSW64State *env, uint64_t a)
{
    float32 fa;
    float64 fr;

    fa = s_to_float32(a);
    fr = float32_to_float64(fa, &FP_STATUS);

    return fr;
}

uint64_t helper_fcvtds(CPUSW64State *env, uint64_t a)
{
    float32 fa;

    fa = float64_to_float32((float64)a, &FP_STATUS);

    return float32_to_s(fa);
}

uint64_t helper_fcvtwl(CPUSW64State *env, uint64_t a)
{
    int32_t ret;
    ret = (a >> 29) & 0x3fffffff;
    ret |= ((a >> 62) & 0x3) << 30;
    return (uint64_t)(int64_t)ret; //int32_t  to int64_t as Sign-Extend
}

uint64_t helper_fcvtlw(CPUSW64State *env, uint64_t a)
{
    uint64_t ret;
    ret = (a & 0x3fffffff) << 29;
    ret |= ((a >> 30) & 0x3) << 62;
    return ret;
}

uint64_t helper_fadds(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float32 fa, fb, fr;

    fa = s_to_float32(a);
    fb = s_to_float32(b);
#if 1
    fr = float32_add(fa, fb, &FP_STATUS);

    env->error_code = soft_to_errcode_exc(env);
#else
    *(float*)&fr = *(float*)&fb + *(float*)&fa;
#endif
    return float32_to_s(fr);
}

/* Input handing without software completion.  Trap for all
   non-finite numbers.  */
uint64_t helper_faddd(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb, fr;

    fa = (float64)a;
    fb = (float64)b;
#if 1
    fr = float64_add(fa, fb, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(double*)&fr = *(double*)&fb + *(double*)&fa;
#endif
    return (uint64_t)fr;
}

uint64_t helper_fsubs(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float32 fa, fb, fr;

    fa = s_to_float32(a);
    fb = s_to_float32(b);
#if 1
    fr = float32_sub(fa, fb, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(float*)&fr = *(float*)&fa - *(float*)&fb;
#endif
    return float32_to_s(fr);
}

uint64_t helper_fsubd(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb, fr;

    fa = (float64)a;
    fb = (float64)b;
#if 1
    fr = float64_sub(fa, fb, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(double*)&fr = *(double*)&fa - *(double*)&fb;
#endif
    return (uint64_t)fr;
}

uint64_t helper_fmuls(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float32 fa, fb, fr;

    fa = s_to_float32(a);
    fb = s_to_float32(b);
#if 1
    fr = float32_mul(fa, fb, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(float*)&fr = *(float*)&fa * *(float*)&fb;
#endif
    return float32_to_s(fr);
}

uint64_t helper_fmuld(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb, fr;

    fa = (float64)a;
    fb = (float64)b;
#if 1
    fr = float64_mul(fa, fb, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(double*)&fr = *(double*)&fa * *(double*)&fb;
#endif
    return (uint64_t)fr;
}

uint64_t helper_fdivs(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float32 fa, fb, fr;

    fa = s_to_float32(a);
    fb = s_to_float32(b);
#if 1
    fr = float32_div(fa, fb, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(float*)&fr = *(float*)&fa / *(float*)&fb;
#endif
    return float32_to_s(fr);
}

uint64_t helper_fdivd(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb, fr;

    fa = (float64)a;
    fb = (float64)b;
#if 1
    fr = float64_div(fa, fb, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(double*)&fr = *(double*)&fa / *(double*)&fb;
#endif

    return (uint64_t)fr;
}

uint64_t helper_frecs(CPUSW64State *env, uint64_t a)
{
    float32 fa, fb, fr;

    fa = s_to_float32(a);
    fb = int64_to_float32(1, &FP_STATUS);
#if 1
    fr = float32_div(fb, fa, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(float*)&fr = *(float*)&fb / *(float*)&fa;
#endif
    return float32_to_s(fr);
}

uint64_t helper_frecd(CPUSW64State *env, uint64_t a)
{
    float64 fa, fb, fr;

    fa = (float64)a;
    fb = int64_to_float64(1, &FP_STATUS);
#if 1
    fr = float64_div(fb, fa, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
    *(double*)&fr = *(double*)&fb / *(double*)&fa;
#endif

    return (uint64_t)fr;
}

uint64_t helper_fsqrts(CPUSW64State *env, uint64_t b)
{
    float32 fb, fr;
#if 1
    fb = s_to_float32(b);
    fr = float32_sqrt(fb, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
#include <math.h>
    *(double*)&fr = sqrt(*(double*)&b);
#endif

    return float32_to_s(fr);
}

uint64_t helper_fsqrt(CPUSW64State *env, uint64_t b)
{
    float64 fr;

#if 1
    fr = float64_sqrt(b, &FP_STATUS);
    env->error_code = soft_to_errcode_exc(env);
#else
#include <math.h>
    *(double*)&fr = sqrt(*(double*)&b);
#endif

    return (uint64_t)fr;
}


uint64_t helper_fmas(CPUSW64State *env, uint64_t a, uint64_t b, uint64_t c)
{
    float32 fa, fb, fc, fr;
    fa = s_to_float32(a);
    fb = s_to_float32(b);
    fc = s_to_float32(c);

    fr = float32_muladd(fa, fb, fc, 0, &FP_STATUS);

    return float32_to_s(fr);
}

uint64_t helper_fmad(CPUSW64State *env, uint64_t a, uint64_t b, uint64_t c)
{
    float64 fr;

    fr = float64_muladd(a, b, c, 0, &FP_STATUS);

    return fr;
}


uint64_t helper_fmss(CPUSW64State *env, uint64_t a, uint64_t b, uint64_t c)
{
    float32 fa, fb, fc, fr;
    fa = s_to_float32(a);
    fb = s_to_float32(b);
    fc = s_to_float32(c);

    fr = float32_muladd(fa, fb, fc, float_muladd_negate_c, &FP_STATUS);

    return float32_to_s(fr);
}

uint64_t helper_fmsd(CPUSW64State *env, uint64_t a, uint64_t b, uint64_t c)
{
    float64 fr;

    fr = float64_muladd(a, b, c, float_muladd_negate_c, &FP_STATUS);

    return fr;
}


uint64_t helper_fnmas(CPUSW64State *env, uint64_t a, uint64_t b, uint64_t c)
{
    float32 fa, fb, fc, fr;
    fa = s_to_float32(a);
    fb = s_to_float32(b);
    fc = s_to_float32(c);
    int flag = float_muladd_negate_product;

    fr = float32_muladd(fa, fb, fc, flag, &FP_STATUS);

    return float32_to_s(fr);
}

uint64_t helper_fnmad(CPUSW64State *env, uint64_t a, uint64_t b, uint64_t c)
{
    float64 fr;
    int flag = float_muladd_negate_product;

    fr = float64_muladd(a, b, c, flag, &FP_STATUS);

    return fr;
}

uint64_t helper_fnmss(CPUSW64State *env, uint64_t a, uint64_t b, uint64_t c)
{
    float32 fa, fb, fc, fr;
    fa = s_to_float32(a);
    fb = s_to_float32(b);
    fc = s_to_float32(c);
    int flag = float_muladd_negate_product | float_muladd_negate_c;

    fr = float32_muladd(fa, fb, fc, flag, &FP_STATUS);

    return float32_to_s(fr);
}

uint64_t helper_fnmsd(CPUSW64State *env, uint64_t a, uint64_t b, uint64_t c)
{
    float64 fr;
    int flag = float_muladd_negate_product | float_muladd_negate_c;

    fr = float64_muladd(a, b, c, flag, &FP_STATUS);

    return fr;
}
uint64_t helper_load_fpcr(CPUSW64State *env)
{
    return cpu_sw64_load_fpcr(env);
}

static void update_fpcr_status_mask(CPUSW64State *env)
{
    uint64_t t = 0;

    /* Don't mask the inv excp:
     * EXC_CTL1 = 1
     * EXC_CTL1 = 0, input denormal, DNZ=0
     * EXC_CTL1 = 0, no input denormal or DNZ=1, INVD = 0
     */
    if ((env->fpcr & FPCR_MASK(EXC_CTL) & 0x2)) {
        if (env->fpcr & FPCR_MASK(EXC_CTL) & 0x1) {
            t |= (EXC_M_INE | EXC_M_UNF | EXC_M_IOV);
        } else {
            t |= EXC_M_INE;
        }
    } else {
        /* INV and DNO mask */
        if (env->fpcr & FPCR_MASK(DNZ)) t |= EXC_M_DNO;
        if (env->fpcr & FPCR_MASK(INVD)) t |= EXC_M_INV;
        if (env->fpcr & FPCR_MASK(OVFD)) t |= EXC_M_OVF;
        if (env->fpcr & FPCR_MASK(UNFD)) {
            t |= EXC_M_UNF;
        }
        if (env->fpcr & FPCR_MASK(DZED)) t |= EXC_M_DZE;
        if (env->fpcr & FPCR_MASK(INED)) t |= EXC_M_INE;
    }

    env->fpcr_exc_enable = t;
}

void helper_store_fpcr(CPUSW64State *env, uint64_t val)
{
    uint64_t fpcr = val;
    uint8_t ret;

    switch ((fpcr & FPCR_MASK(DYN)) >> FPCR_DYN_S) {
    case 0x0:
        ret = float_round_to_zero;
        break;
    case 0x1:
        ret = float_round_down;
        break;
    case 0x2:
        ret = float_round_nearest_even;
        break;
    case 0x3:
        ret = float_round_up;
        break;
    default:
        ret = float_round_nearest_even;
        break;
    }

    env->fpcr_round_mode = ret;

    env->fp_status.float_rounding_mode = ret;

    env->fpcr_flush_to_zero =
        (fpcr & FPCR_MASK(UNFD)) && (fpcr & FPCR_MASK(UNDZ));
    env->fp_status.flush_to_zero = env->fpcr_flush_to_zero;

    /* FIXME: Now the DNZ flag does not work int C3A. */
    //set_flush_inputs_to_zero((val & FPCR_MASK(DNZ)) != 0? 1 : 0, &FP_STATUS);

    val &= ~0x3UL;
    val |= env->fpcr & 0x3UL;
    env->fpcr = val;
    update_fpcr_status_mask(env);
}

void helper_setfpcrx(CPUSW64State *env, uint64_t val)
{
    if (env->fpcr & FPCR_MASK(EXC_CTL_WEN)) {
        env->fpcr &= ~3UL;
        env->fpcr |= val & 0x3;
        update_fpcr_status_mask(env);
    }
}
#ifndef CONFIG_USER_ONLY
static uint32_t soft_to_exc_type(uint64_t exc)
{
    uint32_t ret = 0;

    if (unlikely(exc)) {
        ret |= CONVERT_BIT(exc, float_flag_invalid, EXC_M_INV);
        ret |= CONVERT_BIT(exc, float_flag_divbyzero, EXC_M_DZE);
        ret |= CONVERT_BIT(exc, float_flag_overflow, EXC_M_OVF);
        ret |= CONVERT_BIT(exc, float_flag_underflow, EXC_M_UNF);
        ret |= CONVERT_BIT(exc, float_flag_inexact, EXC_M_INE);
    }

    return ret;
}
static void fp_exc_raise1(CPUSW64State *env, uintptr_t retaddr, uint64_t exc,
                          uint32_t regno)
{
    if (!likely(exc))
	return;
    arith_excp(env, retaddr, exc, 1ull << regno);
}

void helper_fp_exc_raise(CPUSW64State *env, uint32_t regno)
{
    uint64_t exc = env->error_code;
    uint32_t exc_type = soft_to_exc_type(exc);

    if (exc_type) {
        exc_type &= ~(env->fpcr_exc_enable);
        if (exc_type) fp_exc_raise1(env, GETPC(), exc_type | EXC_M_SWC, regno);
    }
}
#endif

void helper_ieee_input(CPUSW64State *env, uint64_t val)
{
#ifndef CONFIG_USER_ONLY
    uint32_t exp = (uint32_t)(val >> 52) & 0x7ff;
    uint64_t frac = val & 0xfffffffffffffull;

    if (exp == 0x7ff) {
        /* Infinity or NaN.  */
        uint32_t exc_type = EXC_M_INV;

        if (exc_type) {
            exc_type &= ~(env->fpcr_exc_enable);
            if (exc_type)
		    fp_exc_raise1(env, GETPC(), exc_type | EXC_M_SWC, 32);
        }
    }
#endif
}

void helper_ieee_input_s(CPUSW64State *env, uint64_t val)
{
    if (unlikely(2 * val - 1 < 0x1fffffffffffffull) &&
        !env->fp_status.flush_inputs_to_zero) {
    }
}

static inline float64 t_to_float64(uint64_t a)
{
    /* Memory format is the same as float64 */
    CPU_DoubleU r;
    r.ll = a;
    return r.d;
}

uint64_t helper_fcmpun(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb;
    uint64_t ret = 0;

    fa = t_to_float64(a);
    fb = t_to_float64(b);

    if (float64_unordered_quiet(fa, fb, &FP_STATUS)) {
        ret = 0x4000000000000000ULL;
    }
    env->error_code = soft_to_errcode_exc(env);

    return ret;
}

uint64_t helper_fcmpeq(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb;
    uint64_t ret = 0;

    fa = t_to_float64(a);
    fb = t_to_float64(b);

    if (float64_eq_quiet(fa, fb, &FP_STATUS)) {
        ret = 0x4000000000000000ULL;
    }
    env->error_code = soft_to_errcode_exc(env);

    return ret;
}

uint64_t helper_fcmple(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb;
    uint64_t ret = 0;

    fa = t_to_float64(a);
    fb = t_to_float64(b);

    if (float64_le_quiet(fa, fb, &FP_STATUS)) {
        ret = 0x4000000000000000ULL;
    }
    env->error_code = soft_to_errcode_exc(env);

    return ret;
}

uint64_t helper_fcmplt(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb;
    uint64_t ret = 0;

    fa = t_to_float64(a);
    fb = t_to_float64(b);

    if (float64_lt_quiet(fa, fb, &FP_STATUS)) {
        ret = 0x4000000000000000ULL;
    }
    env->error_code = soft_to_errcode_exc(env);

    return ret;
}

uint64_t helper_fcmpge(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb;
    uint64_t ret = 0;

    fa = t_to_float64(a);
    fb = t_to_float64(b);

    if (float64_le_quiet(fb, fa, &FP_STATUS)) {
        ret = 0x4000000000000000ULL;
    }
    env->error_code = soft_to_errcode_exc(env);

    return ret;
}

uint64_t helper_fcmpgt(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb;
    uint64_t ret = 0;

    fa = t_to_float64(a);
    fb = t_to_float64(b);

    if (float64_lt_quiet(fb, fa, &FP_STATUS)) {
        ret = 0x4000000000000000ULL;
    }
    env->error_code = soft_to_errcode_exc(env);

    return ret;
}

uint64_t helper_fcmpge_s(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb;
    uint64_t ret = 0;

    /* Make sure va and vb is s float. */
    fa = float32_to_float64(s_to_float32(a), &FP_STATUS);
    fb = float32_to_float64(s_to_float32(b), &FP_STATUS);

    if (float64_le_quiet(fb, fa, &FP_STATUS)) {
        ret = 0x4000000000000000ULL;
    }
    env->error_code = soft_to_errcode_exc(env);

    return ret;
}

uint64_t helper_fcmple_s(CPUSW64State *env, uint64_t a, uint64_t b)
{
    float64 fa, fb;
    uint64_t ret = 0;

    /* Make sure va and vb is s float. */
    fa = float32_to_float64(s_to_float32(a), &FP_STATUS);
    fb = float32_to_float64(s_to_float32(b), &FP_STATUS);

    if (float64_le_quiet(fa, fb, &FP_STATUS)) {
        ret = 0x4000000000000000ULL;
    }
    env->error_code = soft_to_errcode_exc(env);

    return ret;
}

void helper_vfcvtsh(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t vc,
                    uint64_t rd)
{
    uint64_t temp = 0;
    int i;
    for (i = 0; i < 4; i++) {
        temp |= (uint64_t)float32_to_float16(s_to_float32(env->fr[ra + i * 32]),
                                             1, &FP_STATUS)
                << (i * 16);
    }
    for (i = 0; i < 4; i++) {
        if (i == (vc & 0x3)) {
            env->fr[rd + i * 32] = temp;
        } else {
            env->fr[rd + i * 32] = env->fr[rb + i * 32];
        }
    }
}

void helper_vfcvths(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t vc,
                    uint64_t rd)
{
    uint64_t temp;
    int i;

    temp = env->fr[ra + 32 * (vc & 0x3)];
    for (i = 0; i < 4; i++) {
        env->fr[rd + i * 32] = float32_to_s(
            float16_to_float32((temp >> (i * 16)) & 0xffffUL, 1, &FP_STATUS));
    }
}
