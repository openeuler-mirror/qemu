/*
 *  qemu user cpu loop
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
#include "qemu.h"
#include "cpu_loop-common.h"
#include "elf.h"

/* Break codes */
enum { BRK_OVERFLOW = 6, BRK_DIVZERO = 7 };

void force_sig_fault(CPULOONGARCHState *env, target_siginfo_t *info,
                     unsigned int code)
{

    switch (code) {
    case BRK_OVERFLOW:
    case BRK_DIVZERO:
        info->si_signo = TARGET_SIGFPE;
        info->si_errno = 0;
        info->si_code = (code == BRK_OVERFLOW) ? FPE_INTOVF : FPE_INTDIV;
        queue_signal(env, info->si_signo, QEMU_SI_FAULT, &*info);
        ret = 0;
        break;
    default:
        info->si_signo = TARGET_SIGTRAP;
        info->si_errno = 0;
        queue_signal(env, info->si_signo, QEMU_SI_FAULT, &*info);
        ret = 0;
        break;
    }
}

void cpu_loop(CPULOONGARCHState *env)
{
    CPUState *cs = CPU(loongarch_env_get_cpu(env));
    target_siginfo_t info;
    int trapnr;
    abi_long ret;

    for (;;) {
        cpu_exec_start(cs);
        trapnr = cpu_exec(cs);
        cpu_exec_end(cs);
        process_queued_cpu_work(cs);

        switch (trapnr) {
        case EXCP_SYSCALL:
            env->active_tc.PC += 4;
            ret =
                do_syscall(env, env->active_tc.gpr[11], env->active_tc.gpr[4],
                           env->active_tc.gpr[5], env->active_tc.gpr[6],
                           env->active_tc.gpr[7], env->active_tc.gpr[8],
                           env->active_tc.gpr[9], -1, -1);
            if (ret == -TARGET_ERESTARTSYS) {
                env->active_tc.PC -= 4;
                break;
            }
            if (ret == -TARGET_QEMU_ESIGRETURN) {
                /*
                 * Returning from a successful sigreturn syscall.
                 * Avoid clobbering register state.
                 */
                break;
            }
            env->active_tc.gpr[4] = ret;
            break;
        case EXCP_TLBL:
        case EXCP_TLBS:
        case EXCP_AdEL:
        case EXCP_AdES:
            info.si_signo = TARGET_SIGSEGV;
            info.si_errno = 0;
            /* XXX: check env->error_code */
            info.si_code = TARGET_SEGV_MAPERR;
            info._sifields._sigfault._addr = env->CSR_BADV;
            queue_signal(env, info.si_signo, QEMU_SI_FAULT, &info);
            break;
        case EXCP_FPDIS:
        case EXCP_LSXDIS:
        case EXCP_LASXDIS:
        case EXCP_RI:
            info.si_signo = TARGET_SIGILL;
            info.si_errno = 0;
            info.si_code = 0;
            queue_signal(env, info.si_signo, QEMU_SI_FAULT, &info);
            break;
        case EXCP_INTERRUPT:
            /* just indicate that signals should be handled asap */
            break;
        case EXCP_DEBUG:
            info.si_signo = TARGET_SIGTRAP;
            info.si_errno = 0;
            info.si_code = TARGET_TRAP_BRKPT;
            queue_signal(env, info.si_signo, QEMU_SI_FAULT, &info);
            break;
        case EXCP_FPE:
            info.si_signo = TARGET_SIGFPE;
            info.si_errno = 0;
            info.si_code = TARGET_FPE_FLTUNK;
            if (GET_FP_CAUSE(env->active_fpu.fcsr0) & FP_INVALID) {
                info.si_code = TARGET_FPE_FLTINV;
            } else if (GET_FP_CAUSE(env->active_fpu.fcsr0) & FP_DIV0) {
                info.si_code = TARGET_FPE_FLTDIV;
            } else if (GET_FP_CAUSE(env->active_fpu.fcsr0) & FP_OVERFLOW) {
                info.si_code = TARGET_FPE_FLTOVF;
            } else if (GET_FP_CAUSE(env->active_fpu.fcsr0) & FP_UNDERFLOW) {
                info.si_code = TARGET_FPE_FLTUND;
            } else if (GET_FP_CAUSE(env->active_fpu.fcsr0) & FP_INEXACT) {
                info.si_code = TARGET_FPE_FLTRES;
            }
            queue_signal(env, info.si_signo, QEMU_SI_FAULT, &info);
            break;
        case EXCP_BREAK: {
            abi_ulong trap_instr;
            unsigned int code;

            ret = get_user_u32(trap_instr, env->active_tc.PC);
            if (ret != 0) {
                goto error;
            }

            code = trap_instr & 0x7fff;
            force_sig_fault(env, &info, code);
        } break;
        case EXCP_TRAP: {
            abi_ulong trap_instr;
            unsigned int code = 0;

            ret = get_user_u32(trap_instr, env->active_tc.PC);

            if (ret != 0) {
                goto error;
            }

            /* The immediate versions don't provide a code.  */
            if (!(trap_instr & 0xFC000000)) {
                code = ((trap_instr >> 6) & ((1 << 10) - 1));
            }
            force_sig_fault(env, &info, code);
        } break;
        case EXCP_ATOMIC:
            cpu_exec_step_atomic(cs);
            break;
        default:
        error:
            EXCP_DUMP(env, "qemu: unhandled CPU exception 0x%x - aborting\n",
                      trapnr);
            abort();
        }
        process_pending_signals(env);
    }
}

void target_cpu_copy_regs(CPUArchState *env, struct target_pt_regs *regs)
{
    int i;

    for (i = 0; i < 32; i++) {
        env->active_tc.gpr[i] = regs->regs[i];
    }
    env->active_tc.PC = regs->csr_era & ~(target_ulong)1;
}
