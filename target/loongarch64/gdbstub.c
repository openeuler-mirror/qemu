/*
 * LOONGARCH gdb server stub
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
#include "qemu-common.h"
#include "cpu.h"
#include "internal.h"
#include "exec/gdbstub.h"
#ifdef CONFIG_TCG
#include "exec/helper-proto.h"
#endif

uint64_t read_fcc(CPULOONGARCHState *env)
{
    uint64_t ret = 0;

    for (int i = 0; i < 8; ++i) {
        ret |= (uint64_t)env->active_fpu.cf[i] << (i * 8);
    }

    return ret;
}

void write_fcc(CPULOONGARCHState *env, uint64_t val)
{
    for (int i = 0; i < 8; ++i) {
        env->active_fpu.cf[i] = (val >> (i * 8)) & 1;
    }
}

int loongarch_cpu_gdb_read_register(CPUState *cs, GByteArray *mem_buf, int n)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    int size = 0;

    if (0 <= n && n < 32) {
        return gdb_get_regl(mem_buf, env->active_tc.gpr[n]);
    }

    switch (n) {
    case 32:
        size = gdb_get_regl(mem_buf, 0);
        break;
    case 33:
        size = gdb_get_regl(mem_buf, env->active_tc.PC);
        break;
    case 34:
        size = gdb_get_regl(mem_buf, env->CSR_BADV);
        break;
    default:
        break;
    }

    return size;
}

int loongarch_cpu_gdb_write_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    target_ulong tmp = ldtul_p(mem_buf);
    int size = 0;

    if (0 <= n && n < 32) {
        return env->active_tc.gpr[n] = tmp, sizeof(target_ulong);
    }

    size = sizeof(target_ulong);

    switch (n) {
    case 33:
        env->active_tc.PC = tmp;
        break;
    case 32:
    case 34:
    default:
        size = 0;
        break;
    }

    return size;
}

static int loongarch_gdb_get_fpu(CPULOONGARCHState *env, GByteArray *mem_buf,
                                 int n)
{
    if (0 <= n && n < 32) {
        return gdb_get_reg64(mem_buf, env->active_fpu.fpr[n].d);
    } else if (n == 32) {
        uint64_t val = read_fcc(env);
        return gdb_get_reg64(mem_buf, val);
    } else if (n == 33) {
        return gdb_get_reg32(mem_buf, env->active_fpu.fcsr0);
    }
    return 0;
}

static int loongarch_gdb_set_fpu(CPULOONGARCHState *env, uint8_t *mem_buf,
                                 int n)
{
    int length = 0;

    if (0 <= n && n < 32) {
        env->active_fpu.fpr[n].d = ldq_p(mem_buf);
        length = 8;
    } else if (n == 32) {
        uint64_t val = ldq_p(mem_buf);
        write_fcc(env, val);
        length = 8;
    } else if (n == 33) {
        env->active_fpu.fcsr0 = ldl_p(mem_buf);
        length = 4;
    }
    return length;
}

void loongarch_cpu_register_gdb_regs_for_features(CPUState *cs)
{
    gdb_register_coprocessor(cs, loongarch_gdb_get_fpu, loongarch_gdb_set_fpu,
                             34, "loongarch-fpu.xml", 0);
}

#ifdef CONFIG_TCG
int loongarch_read_qxfer(CPUState *cs, const char *annex, uint8_t *read_buf,
                         unsigned long offset, unsigned long len)
{
    if (strncmp(annex, "cpucfg", sizeof("cpucfg") - 1) == 0) {
        if (offset % 4 != 0 || len % 4 != 0) {
            return 0;
        }

        size_t i;
        for (i = offset; i < offset + len; i += 4)
            ((uint32_t *)read_buf)[(i - offset) / 4] =
                helper_cpucfg(&(LOONGARCH_CPU(cs)->env), i / 4);
        return 32 * 4;
    }
    return 0;
}

int loongarch_write_qxfer(CPUState *cs, const char *annex,
                          const uint8_t *write_buf, unsigned long offset,
                          unsigned long len)
{
    return 0;
}
#endif
