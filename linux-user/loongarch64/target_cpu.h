/*
 * loongarch specific CPU ABI and functions for linux-user
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

#ifndef LOONGARCH_TARGET_CPU_H
#define LOONGARCH_TARGET_CPU_H

static inline void cpu_clone_regs_child(CPULOONGARCHState *env,
                                        target_ulong newsp, unsigned flags)
{
    if (newsp) {
        env->active_tc.gpr[3] = newsp;
    }
    env->active_tc.gpr[7] = 0;
    env->active_tc.gpr[4] = 0;
}

static inline void cpu_clone_regs_parent(CPULOONGARCHState *env,
                                         unsigned flags)
{
}

static inline void cpu_set_tls(CPULOONGARCHState *env, target_ulong newtls)
{
    env->active_tc.gpr[2] = newtls;
}

static inline abi_ulong get_sp_from_cpustate(CPULOONGARCHState *state)
{
    return state->active_tc.gpr[3];
}
#endif
