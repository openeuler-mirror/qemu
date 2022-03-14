#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"

#undef DEBUG_SIMD

static inline uint8_t *get_element_b(CPUSW64State *env, uint64_t ra,
    int index)
{
    return (uint8_t*)&env->fr[ra + (index / 8) * 32] + (index % 8);
}

static inline uint16_t *get_element_h(CPUSW64State *env, uint64_t ra,
    int index)
{
    return (uint16_t*)&env->fr[ra + (index / 4) * 32] + (index % 4);
}

static inline uint32_t *get_element_w(CPUSW64State *env, uint64_t ra,
    int index)
{
    return (uint32_t*)&env->fr[ra + (index / 2) * 32] + (index % 2);
}

static inline uint64_t *get_element_l(CPUSW64State *env, uint64_t ra,
    int index)
{
    return &env->fr[ra + index * 32];
}

void helper_srlow(CPUSW64State *env, uint64_t ra, uint64_t rc, uint64_t shift)
{
    int i;
    int adden;
    int dest, src;
    adden = shift >> 6;
    shift &= 0x3f;
#ifdef DEBUG_SIMD
    printf("right shift = %ld adden = %d\n", shift, adden);
    printf("in_fr[%ld]:", ra);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[ra + 32 * i]);
    }
    printf("\n");
#endif

    for (i = 0; (i + adden) < 4; i++) {
        dest = i * 32 + rc;
        src = (i + adden) * 32 + ra;
        env->fr[dest] = env->fr[src] >> shift;
        if (((i + adden) < 3) && (shift != 0))
            env->fr[dest] |= (env->fr[src + 32] << (64 - shift));
    }

    for (; i < 4; i++) {
        env->fr[rc + i * 32] = 0;
    }
#ifdef DEBUG_SIMD
    printf("out_fr[%ld]:", rc);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[rc + 32 * i]);
    }
    printf("\n");
#endif
}

void helper_sllow(CPUSW64State *env, uint64_t ra, uint64_t rc, uint64_t shift)
{
    int i;
    int adden;
    int dest, src;
    adden = shift >> 6;
    shift &= 0x3f;
#ifdef DEBUG_SIMD
    printf("left shift = %ld adden = %d\n", shift, adden);
    printf("in_fr[%ld]:", ra);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[ra + 32 * i]);
    }
    printf("\n");
#endif

    for (i = 3; (i - adden) >= 0; i--) {
        dest = i * 32 + rc;
        src = (i - adden) * 32 + ra;
        env->fr[dest] = env->fr[src] << shift;
        if (((i - adden) > 0) && (shift != 0))
            env->fr[dest] |= (env->fr[src - 32] >> (64 - shift));
    }
    for (; i >= 0; i--) {
        env->fr[rc + i * 32] = 0;
    }
#ifdef DEBUG_SIMD
    printf("out_fr[%ld]:", rc);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[rc + 32 * i]);
    }
    printf("\n");
#endif
}

static uint64_t do_logzz(uint64_t va, uint64_t vb, uint64_t vc, uint64_t zz)
{
    int i;
    uint64_t ret = 0;
    int index;

    for (i = 0; i < 64; i++) {
        index = (((va >> i) & 1) << 2) | (((vb >> i) & 1) << 1) | ((vc >> i) & 1);
        ret |= ((zz >> index) & 1) << i;
    }

    return ret;
}

void helper_vlogzz(CPUSW64State *env, uint64_t args, uint64_t rd, uint64_t zz)
{
    int i;
    int ra, rb, rc;
    ra = args >> 16;
    rb = (args >> 8) & 0xff;
    rc = args & 0xff;
#ifdef DEBUG_SIMD
    printf("zz = %lx\n", zz);
    printf("in_fr[%d]:", ra);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[ra + 32 * i]);
    }
    printf("\n");
    printf("in_fr[%d]:", rb);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[rb + 32 * i]);
    }
    printf("\n");
    printf("in_fr[%d]:", rc);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[rc + 32 * i]);
    }
    printf("\n");
#endif
    for (i = 0; i < 4; i++) {
        env->fr[rd + i * 32] = do_logzz(env->fr[ra + i * 32], env->fr[rb + i * 32],
            env->fr[rc + i * 32], zz);
    }
#ifdef DEBUG_SIMD
    printf("out_fr[%ld]:", rd);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[rd + 32 * i]);
    }
    printf("\n");
#endif
}

void helper_v_print(CPUSW64State *env, uint64_t v)
{
    printf("PC[%lx]: fr[%lx]:\n", GETPC(), v);
}

void helper_vconw(CPUSW64State *env, uint64_t args, uint64_t rd,
    uint64_t byte4_len)
{
    int ra, rb;
    int count;
    int i;
    uint32_t *ptr_dst, *ptr_src;
    uint32_t tmp[8];

    ra = (args >> 8) & 0xff;
    rb = args & 0xff;
    count = 8 - byte4_len;

    for (i = 0; i < 8; i++) {
        ptr_dst = get_element_w(env, rd, i);
        if (i < count) {
            ptr_src = get_element_w(env, ra, i + byte4_len);
        } else {
            ptr_src = get_element_w(env, rb, i - count);
        }
        tmp[i] = *ptr_src;
    }
    for (i = 0; i < 8; i++) {
        ptr_dst = get_element_w(env, rd, i);
        *ptr_dst = tmp[i];
    }
}

void helper_vcond(CPUSW64State *env, uint64_t args, uint64_t rd,
    uint64_t byte8_len)
{
    int ra, rb;
    int count;
    int i;
    uint64_t *ptr_dst, *ptr_src;
    uint64_t tmp[8];

    ra = (args >> 8) & 0xff;
    rb = args & 0xff;
    count = 4 - byte8_len;

    for (i = 0; i < 4; i++) {
        if (i < count) {
            ptr_src = get_element_l(env, ra, i + byte8_len);
        } else {
            ptr_src = get_element_l(env, rb, i - count);
        }
        tmp[i] = *ptr_src;
    }
    for (i = 0; i < 4; i++) {
        ptr_dst = get_element_l(env, rd, i + byte8_len);
        *ptr_dst = tmp[i];
    }
}

void helper_vshfw(CPUSW64State *env, uint64_t args, uint64_t rd, uint64_t vc)
{
    int ra, rb;
    int i;
    uint32_t *ptr_dst, *ptr_src;
    uint32_t tmp[8];
    int flag, idx;

    ra = (args >> 8) & 0xff;
    rb = args & 0xff;

    for (i = 0; i < 8; i++) {
        flag = (vc >> (i * 4)) & 0x8;
        idx = (vc >> (i * 4)) & 0x7;
        if (flag == 0) {
            ptr_src = get_element_w(env, ra, idx);
        } else {
            ptr_src = get_element_w(env, rb, idx);
        }
        tmp[i] = *ptr_src;
    }
    for (i = 0; i < 8; i++) {
        ptr_dst = get_element_w(env, rd, i);
        *ptr_dst = tmp[i];
    }
}

uint64_t helper_ctlzow(CPUSW64State *env, uint64_t ra)
{
    int i, j;
    uint64_t val;
    uint64_t ctlz = 0;

    for (j = 3; j >= 0; j--) {
        val = env->fr[ra + 32 * j];
        for (i = 63; i >= 0; i--) {
            if ((val >> i) & 1)
                return ctlz << 29;
            else
                ctlz++;
        }
    }
    return ctlz << 29;
}

void helper_vucaddw(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    int a, b, c;
    int ret;
    int i;

    for (i = 0; i < 4; i++) {
        a = (int)(env->fr[ra + i * 32] & 0xffffffff);
        b = (int)(env->fr[rb + i * 32] & 0xffffffff);
        c = a + b;
        if ((c ^ a) < 0 && (c ^ b) < 0) {
            if (a < 0)
                c = 0x80000000;
            else
                c = 0x7fffffff;
        }
        ret = c;

        a = (int)(env->fr[ra + i * 32] >> 32);
        b = (int)(env->fr[rb + i * 32] >> 32);
        c = a + b;
        if ((c ^ a) < 0 && (c ^ b) < 0) {
            if (a < 0)
                c = 0x80000000;
            else
                c = 0x7fffffff;
        }
        env->fr[rc + i * 32] = ((uint64_t)(uint32_t)c << 32) |
            (uint64_t)(uint32_t)ret;
    }
}

void helper_vucaddwi(CPUSW64State *env, uint64_t ra, uint64_t vb, uint64_t rc)
{
    int a, b, c;
    int ret;
    int i;

    b = (int)vb;
    for (i = 0; i < 4; i++) {
        a = (int)(env->fr[ra + i * 32] & 0xffffffff);
        c = a + b;
        if ((c ^ a) < 0 && (c ^ b) < 0) {
            if (a < 0)
                c = 0x80000000;
            else
                c = 0x7fffffff;
        }
        ret = c;

        a = (int)(env->fr[ra + i * 32] >> 32);
        c = a + b;
        if ((c ^ a) < 0 && (c ^ b) < 0) {
            if (a < 0)
                c = 0x80000000;
            else
                c = 0x7fffffff;
        }
        env->fr[rc + i * 32] = ((uint64_t)(uint32_t)c << 32) |
            (uint64_t)(uint32_t)ret;
    }
}

void helper_vucsubw(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    int a, b, c;
    int ret;
    int i;

    for (i = 0; i < 4; i++) {
        a = (int)(env->fr[ra + i * 32] & 0xffffffff);
        b = (int)(env->fr[rb + i * 32] & 0xffffffff);
        c = a - b;
        if ((b ^ a) < 0 && (c ^ a) < 0) {
            if (a < 0)
                c = 0x80000000;
            else
                c = 0x7fffffff;
        }
        ret = c;

        a = (int)(env->fr[ra + i * 32] >> 32);
        b = (int)(env->fr[rb + i * 32] >> 32);
        c = a - b;
        if ((b ^ a) < 0 && (c ^ a) < 0) {
            if (a < 0)
                c = 0x80000000;
            else
                c = 0x7fffffff;
        }
        env->fr[rc + i * 32] = ((uint64_t)(uint32_t)c << 32) |
            (uint64_t)(uint32_t)ret;
    }
}

void helper_vucsubwi(CPUSW64State *env, uint64_t ra, uint64_t vb, uint64_t rc)
{
    int a, b, c;
    int ret;
    int i;

    b = (int)vb;
    for (i = 0; i < 4; i++) {
        a = (int)(env->fr[ra + i * 32] & 0xffffffff);
        c = a - b;
        if ((b ^ a) < 0 && (c ^ a) < 0) {
            if (a < 0)
                c = 0x80000000;
            else
                c = 0x7fffffff;
        }
        ret = c;

        a = (int)(env->fr[ra + i * 32] >> 32);
        c = a - b;
        if ((b ^ a) < 0 && (c ^ a) < 0) {
            if (a < 0)
                c = 0x80000000;
            else
                c = 0x7fffffff;
        }
        env->fr[rc + i * 32] = ((uint64_t)(uint32_t)c << 32) |
            (uint64_t)(uint32_t)ret;
    }
}

void helper_vucaddh(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    short a, b, c;
    uint64_t ret;
    int i, j;

    for (i = 0; i < 4; i++) {
        ret = 0;
        for (j = 0; j < 4; j++) {
            a = (short)((env->fr[ra + i * 32] >> (j * 16)) & 0xffff);
            b = (short)((env->fr[rb + i * 32] >> (j * 16)) & 0xffff);
            c = a + b;
            if ((c ^ a) < 0 && (c ^ b) < 0) {
                if (a < 0)
                    c = 0x8000;
                else
                    c = 0x7fff;
            }
            ret |= ((uint64_t)(uint16_t)c) << (j * 16);
        }
        env->fr[rc + i * 32] = ret;
    }
}

void helper_vucaddhi(CPUSW64State *env, uint64_t ra, uint64_t vb, uint64_t rc)
{
    short a, b, c;
    uint64_t ret;
    int i, j;

    b = (short)vb;
    for (i = 0; i < 4; i++) {
        ret = 0;
        for (j = 0; j < 4; j++) {
            a = (short)((env->fr[ra + i * 32] >> (j * 16)) & 0xffff);
            c = a + b;
            if ((c ^ a) < 0 && (c ^ b) < 0) {
                if (a < 0)
                    c = 0x8000;
                else
                    c = 0x7fff;
            }
            ret |= ((uint64_t)(uint16_t)c) << (j * 16);
        }
        env->fr[rc + i * 32] = ret;
    }
}

void helper_vucsubh(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    short a, b, c;
    uint64_t ret;
    int i, j;

    for (i = 0; i < 4; i++) {
        ret = 0;
        for (j = 0; j < 4; j++) {
            a = (short)((env->fr[ra + i * 32] >> (j * 16)) & 0xffff);
            b = (short)((env->fr[rb + i * 32] >> (j * 16)) & 0xffff);
            c = a - b;
            if ((b ^ a) < 0 && (c ^ a) < 0) {
                if (a < 0)
                    c = 0x8000;
                else
                    c = 0x7fff;
            }
            ret |= ((uint64_t)(uint16_t)c) << (j * 16);
        }
        env->fr[rc + i * 32] = ret;
    }
}

void helper_vucsubhi(CPUSW64State *env, uint64_t ra, uint64_t vb, uint64_t rc)
{
    short a, b, c;
    uint64_t ret;
    int i, j;

    b = (short)vb;
    for (i = 0; i < 4; i++) {
        ret = 0;
        for (j = 0; j < 4; j++) {
            a = (short)((env->fr[ra + i * 32] >> (j * 16)) & 0xffff);
            c = a - b;
            if ((b ^ a) < 0 && (c ^ a) < 0) {
                if (a < 0)
                    c = 0x8000;
                else
                    c = 0x7fff;
            }
            ret |= ((uint64_t)(uint16_t)c) << (j * 16);
        }
        env->fr[rc + i * 32] = ret;
    }
}

void helper_vucaddb(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    int8_t a, b, c;
    uint64_t ret;
    int i, j;

    for (i = 0; i < 4; i++) {
        ret = 0;
        for (j = 0; j < 8; j++) {
            a = (int8_t)((env->fr[ra + i * 32] >> (j * 8)) & 0xff);
            b = (int8_t)((env->fr[rb + i * 32] >> (j * 8)) & 0xff);
            c = a + b;
            if ((c ^ a) < 0 && (c ^ b) < 0) {
                if (a < 0)
                    c = 0x80;
                else
                    c = 0x7f;
            }
            ret |= ((uint64_t)(uint8_t)c) << (j * 8);
        }
        env->fr[rc + i * 32] = ret;
    }
}

void helper_vucaddbi(CPUSW64State *env, uint64_t ra, uint64_t vb, uint64_t rc)
{
    int8_t a, b, c;
    uint64_t ret;
    int i, j;

    b = (int8_t)(vb & 0xff);
    for (i = 0; i < 4; i++) {
        ret = 0;
        for (j = 0; j < 8; j++) {
            a = (int8_t)((env->fr[ra + i * 32] >> (j * 8)) & 0xff);
            c = a + b;
            if ((c ^ a) < 0 && (c ^ b) < 0) {
                if (a < 0)
                    c = 0x80;
                else
                    c = 0x7f;
            }
            ret |= ((uint64_t)(uint8_t)c) << (j * 8);
        }
        env->fr[rc + i * 32] = ret;
    }
}

void helper_vucsubb(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    int8_t a, b, c;
    uint64_t ret;
    int i, j;

    for (i = 0; i < 4; i++) {
        ret = 0;
        for (j = 0; j < 8; j++) {
            a = (int8_t)((env->fr[ra + i * 32] >> (j * 8)) & 0xff);
            b = (int8_t)((env->fr[rb + i * 32] >> (j * 8)) & 0xff);
            c = a - b;
            if ((b ^ a) < 0 && (c ^ a) < 0) {
                if (a < 0)
                    c = 0x80;
                else
                    c = 0x7f;
            }
            ret |= ((uint64_t)(uint8_t)c) << (j * 8);
        }
        env->fr[rc + i * 32] = ret;
    }
}

void helper_vucsubbi(CPUSW64State *env, uint64_t ra, uint64_t vb, uint64_t rc)
{
    int8_t a, b, c;
    uint64_t ret;
    int i, j;

    b = (int8_t)(vb & 0xff);
    for (i = 0; i < 4; i++) {
        ret = 0;
        for (j = 0; j < 8; j++) {
            a = (int8_t)((env->fr[ra + i * 32] >> (j * 8)) & 0xffff);
            c = a - b;
            if ((b ^ a) < 0 && (c ^ a) < 0) {
                if (a < 0)
                    c = 0x80;
                else
                    c = 0x7f;
            }
            ret |= ((uint64_t)(uint8_t)c) << (j * 8);
        }
        env->fr[rc + i * 32] = ret;
    }
}

uint64_t helper_vstw(CPUSW64State *env, uint64_t t0, uint64_t t1)
{
    uint64_t idx, shift;

    idx = t0 + (t1 / 2) * 32;
    shift = (t1 % 2) * 32;

    return (env->fr[idx] >> shift) & 0xffffffffUL;
}

uint64_t helper_vsts(CPUSW64State *env, uint64_t t0, uint64_t t1)
{
    uint64_t idx, val;

    idx = t0 + t1 * 32;
    val = env->fr[idx];

    return ((val >> 32) & 0xc0000000) | ((val >> 29) & 0x3fffffff);
}

uint64_t helper_vstd(CPUSW64State *env, uint64_t t0, uint64_t t1)
{
    uint64_t idx;

    idx = t0 + t1 * 32;
    return env->fr[idx];
}

#define HELPER_VMAX(name, _suffix, type, loop)                               \
    void glue(glue(helper_, name), _suffix)(CPUSW64State  *env, uint64_t ra, \
        uint64_t rb, uint64_t rc)                                            \
    {                                                                        \
        int i;                                                               \
        type *ptr_dst, *ptr_src_a, *ptr_src_b;                               \
                                                                             \
        for (i = 0; i < loop; i++) {                                         \
            ptr_dst = (type*)glue(get_element_, _suffix)(env, rc, i);        \
            ptr_src_a = (type*)glue(get_element_, _suffix)(env, ra, i);      \
            ptr_src_b = (type*)glue(get_element_, _suffix)(env, rb, i);      \
                                                                             \
            if (*ptr_src_a >= *ptr_src_b) {                                  \
                *ptr_dst = *ptr_src_a;                                       \
            } else {                                                         \
                *ptr_dst = *ptr_src_b;                                       \
            }                                                                \
        }                                                                    \
    }

#define HELPER_VMIN(name, _suffix, type, loop)                               \
    void glue(glue(helper_, name), _suffix)(CPUSW64State  *env, uint64_t ra, \
        uint64_t rb, uint64_t rc)                                            \
    {                                                                        \
        int i;                                                               \
        type *ptr_dst, *ptr_src_a, *ptr_src_b;                               \
                                                                             \
        for (i = 0; i < loop; i++) {                                         \
            ptr_dst = (type*)glue(get_element_, _suffix)(env, rc, i);        \
            ptr_src_a = (type*)glue(get_element_, _suffix)(env, ra, i);      \
            ptr_src_b = (type*)glue(get_element_, _suffix)(env, rb, i);      \
                                                                             \
            if (*ptr_src_a <= *ptr_src_b) {                                  \
                *ptr_dst = *ptr_src_a;                                       \
            } else {                                                         \
                *ptr_dst = *ptr_src_b;                                       \
            }                                                                \
        }                                                                    \
    }

HELPER_VMAX(vmax, b, int8_t, 32)
HELPER_VMIN(vmin, b, int8_t, 32)
HELPER_VMAX(vmax, h, int16_t, 16)
HELPER_VMIN(vmin, h, int16_t, 16)
HELPER_VMAX(vmax, w, int32_t, 8)
HELPER_VMIN(vmin, w, int32_t, 8)
HELPER_VMAX(vumax, b, uint8_t, 32)
HELPER_VMIN(vumin, b, uint8_t, 32)
HELPER_VMAX(vumax, h, uint16_t, 16)
HELPER_VMIN(vumin, h, uint16_t, 16)
HELPER_VMAX(vumax, w, uint32_t, 8)
HELPER_VMIN(vumin, w, uint32_t, 8)

void helper_sraow(CPUSW64State *env, uint64_t ra, uint64_t rc, uint64_t shift)
{
    int i;
    int adden;
    int dest, src;
    uint64_t sign;
    adden = shift >> 6;
    shift &= 0x3f;
    sign = (uint64_t)((int64_t)env->fr[ra + 96] >> 63);
#ifdef DEBUG_SIMD
    printf("right shift = %ld adden = %d\n", shift, adden);
    printf("in_fr[%ld]:", ra);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[ra + 32 * i]);
    }
    printf("\n");
#endif

    for (i = 0; (i + adden) < 4; i++) {
        dest = i * 32 + rc;
        src = (i + adden) * 32 + ra;
        env->fr[dest] = env->fr[src] >> shift;
        if (shift != 0) {
            if (((i + adden) < 3))
                env->fr[dest] |= (env->fr[src + 32] << (64 - shift));
            else
                env->fr[dest] |= (sign << (64 - shift));
        }
    }

    for (; i < 4; i++) {
        env->fr[rc + i * 32] = sign;
    }
#ifdef DEBUG_SIMD
    printf("out_fr[%ld]:", rc);
    for (i = 3 ; i >= 0; i--) {
	    printf("%016lx ", env->fr[rc + 32 * i]);
    }
    printf("\n");
#endif
}

static uint16_t sm4_sbox[16][16] = {
    { 0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7, 0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05 },
    { 0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3, 0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99 },
    { 0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a, 0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62 },
    { 0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95, 0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6 },
    { 0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba, 0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8 },
    { 0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b, 0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35 },
    { 0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2, 0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87 },
    { 0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52, 0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e },
    { 0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5, 0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1 },
    { 0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55, 0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3 },
    { 0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60, 0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f },
    { 0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f, 0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51 },
    { 0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f, 0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8 },
    { 0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd, 0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0 },
    { 0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e, 0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84 },
    { 0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20, 0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48 }
};

static uint32_t SBOX(uint32_t val)
{
    int ret = 0;
    int i;
    int idx_x, idx_y;
    for (i = 0; i < 4; i++) {
        idx_x = (val >> (i * 8)) & 0xff;
        idx_y = idx_x & 0xf;
        idx_x = idx_x >> 4;

        ret |= (sm4_sbox[idx_x][idx_y] << (i * 8));
    }
    return ret;
}

static uint32_t rotl(uint32_t val, int shift)
{
    uint64_t ret = (uint64_t)val;
    ret = (ret << (shift & 0x1f));
    return (uint32_t)((ret & 0xffffffff) | (ret >> 32));
}

void helper_vsm4r(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    uint32_t W[12], rk[8];
    uint32_t temp1, temp2;
    int i, j;

    for (i = 0; i < 8; i++) {
        rk[i] = *get_element_w(env, rb, i);
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 4; j++) {
            W[j] = *get_element_w(env, ra, i * 4 + j);
        }
        for (j = 0; j < 8; j++) {
            temp1 = W[j + 1] ^ W[j + 2] ^ W[j + 3] ^ rk[j];
            temp2 = SBOX(temp1);
            W[j + 4] = W[j] ^ temp2 ^ rotl(temp2, 2) ^ rotl(temp2, 10) ^ rotl(temp2, 18) ^ rotl(temp2, 24);
        }

        for (j = 0; j < 4; j++) {
            *get_element_w(env, rc, i * 4 + j) = W[8 + j];
        }
    }
}

void helper_vcmpueqb(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    uint8_t *ptr_a, *ptr_b, *ptr_c;
    int i;

    for (i = 0; i < 32; i++) {
        ptr_a = get_element_b(env, ra, i);
        ptr_b = get_element_b(env, rb, i);
        ptr_c = get_element_b(env, rc, i);

        *ptr_c = (*ptr_a == *ptr_b) ? 1 : 0;
        ;
    }
}

void helper_vcmpugtb(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    uint8_t *ptr_a, *ptr_b, *ptr_c;
    int i;

    for (i = 0; i < 32; i++) {
        ptr_a = get_element_b(env, ra, i);
        ptr_b = get_element_b(env, rb, i);
        ptr_c = get_element_b(env, rc, i);

        *ptr_c = (*ptr_a > *ptr_b) ? 1 : 0;
        ;
    }
}

void helper_vcmpueqbi(CPUSW64State *env, uint64_t ra, uint64_t vb,
    uint64_t rc)
{
    uint8_t *ptr_a, *ptr_c;
    int i;

    for (i = 0; i < 32; i++) {
        ptr_a = get_element_b(env, ra, i);
        ptr_c = get_element_b(env, rc, i);

        *ptr_c = (*ptr_a == vb) ? 1 : 0;
        ;
    }
}

void helper_vcmpugtbi(CPUSW64State *env, uint64_t ra, uint64_t vb,
    uint64_t rc)
{
    uint8_t *ptr_a, *ptr_c;
    int i;

    for (i = 0; i < 32; i++) {
        ptr_a = get_element_b(env, ra, i);
        ptr_c = get_element_b(env, rc, i);

        *ptr_c = (*ptr_a > vb) ? 1 : 0;
        ;
    }
}

void helper_vsm3msw(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rc)
{
    uint32_t W[24];
    uint32_t temp;
    int i;

    for (i = 0; i < 8; i++) {
        W[i + 0] = *get_element_w(env, ra, i);
        W[i + 8] = *get_element_w(env, rb, i);
    }
    for (i = 16; i < 24; i++) {
        temp = W[i - 16] ^ W[i - 9] ^ rotl(W[i - 3], 15);
        temp = temp ^ rotl(temp, 15) ^ rotl(temp, 23) ^ rotl(W[i - 13], 7) ^ W[i - 6];
        W[i] = temp;
    }
    for (i = 0; i < 8; i++) {
        *get_element_w(env, rc, i) = W[16 + i];
    }
}

static uint32_t selck[4][8] = {
    {0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269, 0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9},
    {0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249, 0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9},
    {0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229, 0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299},
    {0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209, 0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279}
};

void helper_vsm4key(CPUSW64State *env, uint64_t ra, uint64_t vb, uint64_t rc)
{
    uint32_t K[12], *CK;
    int i;
    uint32_t temp1, temp2;

    for (i = 4; i < 8; i++) {
        K[i - 4] = *get_element_w(env, ra, i);
    }
    CK = selck[vb];

    for (i = 0; i < 8; i++) {
        temp1 = K[i + 1] ^ K[i + 2] ^ K[i + 3] ^ CK[i];
        temp2 = SBOX(temp1);
        K[i + 4] = K[i] ^ temp2 ^ rotl(temp2, 13) ^ rotl(temp2, 23);
    }
    for (i = 0; i < 8; i++) {
        *get_element_w(env, rc, i) = K[i + 4];
    }
}

void helper_vinsb(CPUSW64State *env, uint64_t va, uint64_t rb, uint64_t vc,
    uint64_t rd)
{
    int i;

    for (i = 0; i < 128; i += 32) {
        env->fr[rd + i] = env->fr[rb + i];
    }

    *get_element_b(env, rd, vc) = (uint8_t)(va & 0xff);
}

void helper_vinsh(CPUSW64State *env, uint64_t va, uint64_t rb, uint64_t vc,
    uint64_t rd)
{
    int i;

    if (vc >= 16)
        return;

    for (i = 0; i < 128; i += 32) {
        env->fr[rd + i] = env->fr[rb + i];
    }

    *get_element_h(env, rd, vc) = (uint16_t)(va & 0xffff);
}

void helper_vinsectlh(CPUSW64State *env, uint64_t ra, uint64_t rb,
    uint64_t rd)
{
    int i;
    uint32_t temp[8];
    for (i = 0; i < 8; i++) {
        temp[i] = *get_element_h(env, ra, i) | ((uint32_t)*get_element_h(env, rb, i) << 16);
    }
    for (i = 0; i < 8; i++) {
        *get_element_w(env, rd, i) = temp[i];
    }
}
void helper_vinsectlw(CPUSW64State *env, uint64_t ra, uint64_t rb,
    uint64_t rd)
{
    int i;
    uint64_t temp[4];
    for (i = 0; i < 4; i++) {
        temp[i] = *get_element_w(env, ra, i) | ((uint64_t)*get_element_w(env, rb, i) << 32);
    }
    for (i = 0; i < 4; i++) {
        *get_element_l(env, rd, i) = temp[i];
    }
}

void helper_vinsectlb(CPUSW64State *env, uint64_t ra, uint64_t rb,
    uint64_t rd)
{
    int i;
    uint16_t temp[16];
    for (i = 0; i < 16; i++) {
        temp[i] = *get_element_b(env, ra, i) | ((uint16_t)*get_element_b(env, rb, i) << 8);
    }
    for (i = 0; i < 16; i++) {
        *get_element_h(env, rd, i) = temp[i];
    }
}

void helper_vshfq(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t vc,
    uint64_t rd)
{
    int i;
    int idx;
    uint64_t temp[4];
    for (i = 0; i < 2; i++) {
        idx = ((vc >> (i * 2)) & 1) * 64;
        if ((vc >> (i * 2 + 1)) & 1) {
            temp[i * 2] = env->fr[rb + idx];
            temp[i * 2 + 1] = env->fr[rb + idx + 32];
        } else {
            temp[i * 2] = env->fr[ra + idx];
            temp[i * 2 + 1] = env->fr[ra + idx + 32];
        }
    }
    for (i = 0; i < 4; i++) {
        env->fr[rd + i * 32] = temp[i];
    }
}

void helper_vshfqb(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t rd)
{
    int i;
    int idx;
    int vb;
    uint8_t temp[32];

    for (i = 0; i < 16; i++) {
        vb = *get_element_b(env, rb, i);
        if (vb >> 7) {
            temp[i] = 0;
        } else {
            idx = vb & 0xf;
            temp[i] = *get_element_b(env, ra, idx);
        }
        vb = *get_element_b(env, rb, i + 16);
        if (vb >> 7) {
            temp[i + 16] = 0;
        } else {
            idx = vb & 0xf;
            temp[i + 16] = *get_element_b(env, ra, idx + 16);
        }
    }
    for (i = 0; i < 4; i++) {
        env->fr[rd + i * 32] = *((uint64_t*)temp + i);
    }
}

void helper_vsm3r(CPUSW64State *env, uint64_t ra, uint64_t rb, uint64_t vc,
    uint64_t rd)
{
    uint32_t W[8];
    uint32_t A, B, C, D, E, F, G, H, T;
    int i;
    uint32_t SS1, SS2, TT1, TT2, P0;

    if (vc >= 16)
        return;
    for (i = 0; i < 8; i++) {
        W[i] = *get_element_w(env, ra, i);
    }
    A = *get_element_w(env, rb, 0);
    B = *get_element_w(env, rb, 1);
    C = *get_element_w(env, rb, 2);
    D = *get_element_w(env, rb, 3);
    E = *get_element_w(env, rb, 4);
    F = *get_element_w(env, rb, 5);
    G = *get_element_w(env, rb, 6);
    H = *get_element_w(env, rb, 7);

    if (vc < 4) {
        T = 0x79cc4519;
        for (i = 0; i < 4; i++) {
            SS1 = rotl(rotl(A, 12) + E + rotl(T, 4 * vc + i), 7);
            SS2 = SS1 ^ rotl(A, 12);
            TT1 = (A ^ B ^ C) + D + SS2 + (W[i] ^ W[i + 4]);
            TT2 = (E ^ F ^ G) + H + SS1 + W[i];

            P0 = TT2 ^ rotl(TT2, 9) ^ rotl(TT2, 17);

            H = G;
            G = rotl(F, 19);
            F = E;
            E = P0;
            D = C;
            C = rotl(B, 9);
            B = A;
            A = TT1;
        }
    } else {
        T = 0x7a879d8a;
        for (i = 0; i < 4; i++) {
            SS1 = rotl(rotl(A, 12) + E + rotl(T, 4 * vc + i), 7);
            SS2 = SS1 ^ rotl(A, 12);
            TT1 = ((A & B) | (A & C) | (B & C)) + D + SS2 + (W[i] ^ W[i + 4]);
            TT2 = ((E & F) | ((~E) & G)) + H + SS1 + W[i];

            P0 = TT2 ^ rotl(TT2, 9) ^ rotl(TT2, 17);

            H = G;
            G = rotl(F, 19);
            F = E;
            E = P0;
            D = C;
            C = rotl(B, 9);
            B = A;
            A = TT1;
        }
    }
    *get_element_w(env, rd, 0) = A;
    *get_element_w(env, rd, 1) = B;
    *get_element_w(env, rd, 2) = C;
    *get_element_w(env, rd, 3) = D;
    *get_element_w(env, rd, 4) = E;
    *get_element_w(env, rd, 5) = F;
    *get_element_w(env, rd, 6) = G;
    *get_element_w(env, rd, 7) = H;
}
