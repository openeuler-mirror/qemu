/*
 * SW64 gdb server stub
 *
 * Copyright (c) 2023 Lu Feifei
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"
#include "qemu-common.h"
#include "cpu.h"
#include "exec/gdbstub.h"

int sw64_cpu_gdb_read_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;

    if (n < 31) {
        return gdb_get_regl(mem_buf, env->ir[n]);
    } else if (n == 31) {
	return gdb_get_regl(mem_buf, 0);
    } else if (n == 64) {
	return gdb_get_regl(mem_buf, env->pc);
    }
    return 0;
}

int sw64_cpu_gdb_write_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;

    if (n < 31) {
        env->ir[n] = ldtul_p(mem_buf);
        return sizeof(target_ulong);
    } else if (n == 31) {
        /* discard writes to r31 */
	return sizeof(target_ulong);
    } else if (n == 64) {
        env->pc = ldtul_p(mem_buf);
        return sizeof(target_ulong);
    }

    return 0;
}
