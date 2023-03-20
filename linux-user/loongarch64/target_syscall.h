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

#ifndef LOONGARCH_TARGET_SYSCALL_H
#define LOONGARCH_TARGET_SYSCALL_H

/*
 * this struct defines the way the registers are stored on the
 * stack during a system call.
 */

struct target_pt_regs {
    /* Saved main processor registers. */
    target_ulong regs[32];

    /* Saved special registers. */
    /* Saved special registers. */
    target_ulong csr_crmd;
    target_ulong csr_prmd;
    target_ulong csr_euen;
    target_ulong csr_ecfg;
    target_ulong csr_estat;
    target_ulong csr_era;
    target_ulong csr_badvaddr;
    target_ulong orig_a0;
    target_ulong __last[0];
};

#define UNAME_MACHINE "loongarch"
#define UNAME_MINIMUM_RELEASE "2.6.32"

#define TARGET_CLONE_BACKWARDS
#define TARGET_MINSIGSTKSZ 2048
#define TARGET_MLOCKALL_MCL_CURRENT 1
#define TARGET_MLOCKALL_MCL_FUTURE 2

#define TARGET_FORCE_SHMLBA

static inline abi_ulong target_shmlba(CPULOONGARCHState *env)
{
    return 0x40000;
}

#define TARGET_PR_SET_FP_MODE 45
#define TARGET_PR_GET_FP_MODE 46
#define TARGET_PR_FP_MODE_FR (1 << 0)
#define TARGET_PR_FP_MODE_FRE (1 << 1)

#endif /* LOONGARCH_TARGET_SYSCALL_H */
