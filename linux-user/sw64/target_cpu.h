/*
 * SW64 specific CPU ABI and functions for linux-user
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
#ifndef SW64_TARGET_CPU_H
#define SW64_TARGET_CPU_H

static inline void cpu_clone_regs_child(CPUSW64State *env, target_ulong newsp, unsigned flags)
{
    if (newsp) {
        env->ir[IDX_SP] = newsp;
    }
    env->ir[IDX_V0] = 0;
    env->ir[IDX_A3] = 0;
    env->ir[IDX_A4] = 1;  /* OSF/1 secondary return: child */
}

static inline void cpu_clone_regs_parent(CPUSW64State *env, unsigned flags)
{
    /*
     * OSF/1 secondary return: parent
     * Note that the kernel does not do this if SETTLS, because the
     * settls argument register is still live after copy_thread.
     */
    if (!(flags & CLONE_SETTLS)) {
        env->ir[IDX_A4] = 0;
    }
}

static inline void cpu_set_tls(CPUSW64State *env, target_ulong newtls)
{
    env->unique = newtls;
}

static inline abi_ulong get_sp_from_cpustate(CPUSW64State *state)
{
    return state->ir[IDX_SP];
}
#endif
