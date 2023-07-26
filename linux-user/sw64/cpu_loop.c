/*
 *  qemu user cpu loop
 *
 *  Copyright (c) 2003-2008 Fabrice Bellard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu.h"
#include "user-internals.h"
#include "cpu_loop-common.h"
#include "signal-common.h"

void cpu_loop(CPUSW64State *env)
{
    CPUState *cs = CPU(sw64_env_get_cpu(env));
    int trapnr;
    target_siginfo_t info;
    abi_long sysret;

    while (1) {
        cpu_exec_start(cs);
        trapnr = cpu_exec(cs);
        cpu_exec_end(cs);
        process_queued_cpu_work(cs);

        switch (trapnr) {
	case EXCP_OPCDEC:
            cpu_abort(cs, "ILLEGAL SW64 insn at line %d!", __LINE__);
	case EXCP_CALL_SYS:
	    switch (env->error_code) {
            case 0x83:
                /* CALLSYS */
                trapnr = env->ir[IDX_V0];
                sysret = do_syscall(env, trapnr,
                                    env->ir[IDX_A0], env->ir[IDX_A1],
                                    env->ir[IDX_A2], env->ir[IDX_A3],
                                    env->ir[IDX_A4], env->ir[IDX_A5],
                                    0, 0);
                if (sysret == -TARGET_ERESTARTSYS) {
                    env->pc -= 4;
                    break;
                }
                if (sysret == -TARGET_QEMU_ESIGRETURN) {
                    break;
                }
                /* Syscall writes 0 to V0 to bypass error check, similar
                   to how this is handled internal to Linux kernel.
                   (Ab)use trapnr temporarily as boolean indicating error. */
                trapnr = (env->ir[IDX_V0] != 0 && sysret < 0);
                env->ir[IDX_V0] = (trapnr ? -sysret : sysret);
                env->ir[IDX_A3] = trapnr;
                break;
            default:
                printf("UNDO sys_call %lx\n", env->error_code);
                exit(-1);
            }
            break;
        case EXCP_MMFAULT:
            info.si_signo = TARGET_SIGSEGV;
            info.si_errno = 0;
            info.si_code = (page_get_flags(env->trap_arg0) & PAGE_VALID
                            ? TARGET_SEGV_ACCERR : TARGET_SEGV_MAPERR);
            info._sifields._sigfault._addr = env->trap_arg0;
            queue_signal(env, info.si_signo, QEMU_SI_FAULT, &info);
            break;
        case EXCP_ARITH:
            info.si_signo = TARGET_SIGFPE;
            info.si_errno = 0;
            info.si_code = TARGET_FPE_FLTINV;
            info._sifields._sigfault._addr = env->pc;
            queue_signal(env, info.si_signo, QEMU_SI_FAULT, &info);
            break;
        case EXCP_INTERRUPT:
            /* just indicate that signals should be handled asap */
            break;
        default:
            cpu_abort(cs, "UNDO");
        }
        process_pending_signals (env);

        /* Most of the traps imply a transition through hmcode, which
           implies an REI instruction has been executed.  Which means
           that RX and LOCK_ADDR should be cleared.  But there are a
           few exceptions for traps internal to QEMU.  */
        }
}

void target_cpu_copy_regs(CPUArchState *env, struct target_pt_regs *regs)
{
    int i;

    for(i = 0; i < 28; i++) {
        env->ir[i] = ((abi_ulong *)regs)[i];
    }
    env->ir[IDX_SP] = regs->usp;
    env->pc = regs->pc;
}
