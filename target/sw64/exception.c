#include "qemu/osdep.h"
#include "qemu/timer.h"

#include "cpu.h"
#include "exec/exec-all.h"
#include "fpu/softfloat.h"
#include "exec/helper-proto.h"
#include "hw/core/cpu.h"

#ifndef CONFIG_USER_ONLY
void QEMU_NORETURN sw64_cpu_do_unaligned_access(CPUState *cs, vaddr addr,
                                  MMUAccessType access_type,
				  int mmu_idx, uintptr_t retaddr)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    uint32_t insn = 0;

    if (retaddr) {
        cpu_restore_state(cs, retaddr, true);
    }

    fprintf(stderr, "Error %s addr = %lx\n", __func__, addr);
	env->csr[DVA] = addr;

    env->csr[EXC_SUM] = ((insn >> 21) & 31) << 8;	/* opcode */
    env->csr[DS_STAT] = (insn >> 26) << 4;        	/* dest regno */
    cs->exception_index = EXCP_UNALIGN;
    env->error_code = 0;
    cpu_loop_exit(cs);
}

#endif

/* This should only be called from translate, via gen_excp.
   We expect that ENV->PC has already been updated.  */
void QEMU_NORETURN helper_excp(CPUSW64State *env, int excp, int error)
{
    SW64CPU *cpu = sw64_env_get_cpu(env);
    CPUState *cs = CPU(cpu);

    cs->exception_index = excp;
    env->error_code = error;
    cpu_loop_exit(cs);
}

/* This may be called from any of the helpers to set up EXCEPTION_INDEX.  */
void QEMU_NORETURN dynamic_excp(CPUSW64State *env, uintptr_t retaddr, int excp,
                                int error)
{
    SW64CPU *cpu = sw64_env_get_cpu(env);
    CPUState *cs = CPU(cpu);

    cs->exception_index = excp;
    env->error_code = error;
    if (retaddr) {
        /* FIXME: Not jump to another tb, but jump to next insn emu */
        cpu_restore_state(cs, retaddr, true);
        /* Floating-point exceptions (our only users) point to the next PC.  */
        env->pc += 4;
    }
    cpu_loop_exit(cs);
}

void QEMU_NORETURN arith_excp(CPUSW64State *env, uintptr_t retaddr, int exc,
                              uint64_t mask)
{
    env->csr[EXC_SUM] = exc;
    dynamic_excp(env, retaddr, EXCP_ARITH, 0);
}


void helper_trace_mem(CPUSW64State *env, uint64_t addr, uint64_t val)
{
    /* printf("pc = %lx: Access mem addr =%lx, val = %lx\n", env->pc, addr,val); */
}
