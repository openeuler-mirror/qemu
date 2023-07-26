/*
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
#include "qemu/timer.h"

#include "cpu.h"
#include "exec/exec-all.h"
#include "fpu/softfloat.h"
#include "exec/helper-proto.h"
#include "hw/core/cpu.h"
#include "exec/memattrs.h"

#ifndef CONFIG_USER_ONLY
static target_ulong ldq_phys_clear(CPUState *cs, target_ulong phys)
{
    return ldq_phys(cs->as, phys & ~(3UL));
}

static int get_sw64_physical_address(CPUSW64State *env, target_ulong addr,
                                int prot_need, int mmu_idx, target_ulong *pphys,
                                int *pprot)
{
    CPUState *cs = CPU(sw64_env_get_cpu(env));
    target_ulong phys = 0;
    int prot = 0;
    int ret = MM_K_ACV;
    target_ulong L1pte, L2pte, L3pte, L4pte;
    target_ulong pt = 0, index = 0, pte_pfn_s = 0;

    if (((addr >> 28) & 0xffffffff8) == 0xffffffff8) {
        phys = (~(0xffffffff80000000)) & addr;
        prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        ret = -1;
        goto exit;
    } else if (((addr >> 32) & 0xfffff000) == 0xfffff000) {
        goto do_pgmiss;
    } else if (((addr >> 52) & 0xfff) == 0xfff) {
        phys = (~(0xfff0000000000000)) & addr;
        prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        ret = -1;
        goto exit;
    }
do_pgmiss:
    pte_pfn_s = 28;
    pt = env->csr[PTBR];
    index = (addr >> (TARGET_PAGE_BITS + 3 * TARGET_LEVEL_BITS)) & ((1 << TARGET_LEVEL_BITS)-1);
    L1pte = ldq_phys_clear(cs, pt + index * 8);
    if ((L1pte & PTE_VALID) == 0) {
        ret = MM_K_TNV;
        goto exit;
    }
    if (((L1pte >> 1) & 1) && prot_need == 0) {
        ret = MM_K_FOR;
        goto exit;
    }
    if (((L1pte >> 2) & 1) && prot_need == 1) {
        ret = MM_K_FOW;
        goto exit;
    }
    pt = L1pte >> pte_pfn_s << TARGET_PAGE_BITS;

    index = (addr >> (TARGET_PAGE_BITS + 2 * TARGET_LEVEL_BITS)) & ((1 << TARGET_LEVEL_BITS)-1);
    L2pte = ldq_phys_clear(cs, pt + index * 8);

    if ((L2pte & PTE_VALID) == 0) {
        ret = MM_K_TNV;
        goto exit;
    }
    if (((L2pte >> 1) & 1) && prot_need == 0) {
        ret = MM_K_FOR;
        goto exit;
    }
    if (((L2pte >> 2) & 1) && prot_need == 1) {
        ret = MM_K_FOW;
        goto exit;
    }

    pt = L2pte >> pte_pfn_s << TARGET_PAGE_BITS;

    index = (addr >> (TARGET_PAGE_BITS + 1 * TARGET_LEVEL_BITS)) & ((1 << TARGET_LEVEL_BITS)-1);
    L3pte = ldq_phys_clear(cs, pt + index * 8);

    if ((L3pte & PTE_VALID) == 0) {
        ret = MM_K_TNV;
        goto exit;
    }
    if (((L3pte >> 1) & 1) && prot_need == 0) {
        ret = MM_K_FOR;
        goto exit;
    }
    if (((L3pte >> 2) & 1) && prot_need == 1) {
        ret = MM_K_FOW;
        goto exit;
    }

    pt = L3pte >> pte_pfn_s << TARGET_PAGE_BITS;

    index = (addr >> TARGET_PAGE_BITS) & ((1 << TARGET_LEVEL_BITS)-1);
    L4pte = ldq_phys_clear(cs, pt + index * 8);
    if ((L4pte & PTE_VALID) == 0) {
        ret = MM_K_TNV;
        goto exit;
    }
#if PAGE_READ != 1 || PAGE_WRITE != 2 || PAGE_EXEC != 4
#error page bits out of date
#endif

    /* Check access violations.  */
    if ((L4pte & PTE_FOR) == 0) {
        prot |= PAGE_READ | PAGE_EXEC;
    }
    if ((L4pte & PTE_FOW) == 0) {
        prot |= PAGE_WRITE;
    }

    /* Check fault-on-operation violations.  */
    prot &= ~(L4pte >> 1);

    phys = (L4pte >> pte_pfn_s << TARGET_PAGE_BITS);

    if (unlikely((prot & prot_need) == 0)) {
        ret = (prot_need & PAGE_EXEC
                   ? MM_K_FOE
                   : prot_need & PAGE_WRITE
                         ? MM_K_FOW
                         : prot_need & PAGE_READ ? MM_K_FOR : -1);
        goto exit;
    }

    ret = -1;
exit:
    *pphys = phys;
    *pprot = prot;
    return ret;
}

bool sw64_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
		      MMUAccessType access_type, int mmu_idx,
		      bool probe, uintptr_t retaddr)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    target_ulong phys;
    int prot, fail;

    if (mmu_idx == MMU_PHYS_IDX) {
        phys = address;
        prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        fail = 0;
        if ((address >> 52) & 1) goto do_pgmiss;
        goto done;
    }

do_pgmiss:
    fail = get_sw64_physical_address(env, address, 1 << access_type, mmu_idx, &phys, &prot);
    if (unlikely(fail >= 0)) {
	if (probe) {
	    return false;
	}
	cs->exception_index = EXCP_MMFAULT;
	if (access_type == 2) {
            env->csr[DS_STAT] = fail;
            env->csr[DVA] = address & ~(3UL);
        } else {
            env->csr[DS_STAT] = fail | (((unsigned long)access_type + 1) << 3);
            env->csr[DVA] = address;
        }
        env->error_code = access_type;
        cpu_loop_exit_restore(cs, retaddr);
    }
done:
    tlb_set_page(cs, address & TARGET_PAGE_MASK, phys & TARGET_PAGE_MASK, prot,
                 mmu_idx, TARGET_PAGE_SIZE);
    return true;
}

hwaddr sw64_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    target_ulong phys;
    int prot, fail;
    int mmu_index = cpu_mmu_index(env, 0);
    if (mmu_index == MMU_PHYS_IDX) {
        phys = addr;
        prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        fail = -1;
        if ((addr >> 52) & 1) goto do_pgmiss;
        goto done;
    }
do_pgmiss:
    fail = get_sw64_physical_address(&cpu->env, addr, 1, mmu_index, &phys, &prot);
done:
    return (fail >= 0 ? -1 : phys);
}

#define a0(func) (((func & 0xFF) >> 6) & 0x1)
#define a1(func) ((((func & 0xFF) >> 6) & 0x2) >> 1)

#define t(func) ((a0(func) ^ a1(func)) & 0x1)
#define b0(func) (t(func) | a0(func))
#define b1(func) ((~t(func) & 1) | a1(func))

#define START_SYS_CALL_ADDR(func) \
    (b1(func) << 14) | (b0(func) << 13) | ((func & 0x3F) << 7)

void sw64_cpu_do_interrupt(CPUState *cs)
{
    int i = cs->exception_index;

    cs->exception_index = -1;
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    switch (i) {
    case EXCP_OPCDEC:
        cpu_abort(cs, "ILLEGAL INSN");
        break;
    case EXCP_CALL_SYS:
        i = START_SYS_CALL_ADDR(env->error_code);
        if (i <= 0x3F) {
            i += 0x4000;
        } else if (i >= 0x40 && i <= 0x7F) {
            i += 0x2000;
        } else if (i >= 0x80 && i <= 0x8F) {
            i += 0x6000;
        }
        break;
    case EXCP_ARITH:
        env->error_code = -1;
        env->csr[EXC_PC] = env->pc - 4;
        env->csr[EXC_SUM] = 1;
        i = 0xB80;
        break;
    case EXCP_UNALIGN:
        i = 0xB00;
        env->csr[EXC_PC] = env->pc - 4;
        break;
    case EXCP_CLK_INTERRUPT:
    case EXCP_DEV_INTERRUPT:
        i = 0xE80;
        break;
    case EXCP_MMFAULT:
        i = 0x980;
        env->csr[EXC_PC] = env->pc;
        break;
    case EXCP_II0:
        env->csr[EXC_PC] = env->pc;
        i = 0xE00;
        break;
    default:
        break;
    }
    env->pc = env->hm_entry + i;
    env->flags = ENV_FLAG_HM_MODE;
}

bool sw64_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    int idx = -1;
    /* We never take interrupts while in PALmode.  */
    if (env->flags & ENV_FLAG_HM_MODE)
        return false;

    if (interrupt_request & CPU_INTERRUPT_II0) {
        idx = EXCP_II0;
        env->csr[INT_STAT] |= 1UL << 6;
        if ((env->csr[IER] & env->csr[INT_STAT]) == 0)
            return false;
        cs->interrupt_request &= ~CPU_INTERRUPT_II0;
        goto done;
    }

    if (interrupt_request & CPU_INTERRUPT_TIMER) {
        idx = EXCP_CLK_INTERRUPT;
        env->csr[INT_STAT] |= 1UL << 4;
        if ((env->csr[IER] & env->csr[INT_STAT]) == 0)
            return false;
        cs->interrupt_request &= ~CPU_INTERRUPT_TIMER;
        goto done;
    }

    if (interrupt_request & CPU_INTERRUPT_HARD) {
        idx = EXCP_DEV_INTERRUPT;
        env->csr[INT_STAT] |= 1UL << 12;
        if ((env->csr[IER] & env->csr[INT_STAT]) == 0)
            return false;
        cs->interrupt_request &= ~CPU_INTERRUPT_HARD;
        goto done;
    }

    if (interrupt_request & CPU_INTERRUPT_PCIE) {
        idx = EXCP_DEV_INTERRUPT;
        env->csr[INT_STAT] |= 1UL << 1;
        env->csr[INT_PCI_INT] = 0x10;
        if ((env->csr[IER] & env->csr[INT_STAT]) == 0)
            return false;
        cs->interrupt_request &= ~CPU_INTERRUPT_PCIE;
        goto done;
    }

done:
    if (idx >= 0) {
        cs->exception_index = idx;
        env->error_code = 0;
        env->csr[EXC_PC] = env->pc;
        sw64_cpu_do_interrupt(cs);
        return true;
    }

    return false;
}
#endif

static void update_fpcr_status_mask(CPUSW64State* env) {
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

void cpu_sw64_store_fpcr(CPUSW64State* env, uint64_t val) {
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

    val &= ~0x3UL;
    val |= env->fpcr & 0x3UL;
    env->fpcr = val;
    update_fpcr_status_mask(env);
}

uint64_t helper_read_csr(CPUSW64State *env, uint64_t index)
{
    if (index == PRI_BASE)
	env->csr[index] = 0x10000;
    if (index == SHTCLOCK)
	env->csr[index] = qemu_clock_get_ns(QEMU_CLOCK_HOST) / 40;
    return env->csr[index];
}

uint64_t helper_rtc(void)
{
#ifndef CONFIG_USER_ONLY
    return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) * CPUFREQ_SCALE;
#else
    return 0;
#endif
}

void helper_write_csr(CPUSW64State *env, uint64_t index, uint64_t va)
{
    env->csr[index] = va;
#ifndef CONFIG_USER_ONLY
    CPUState *cs = &(sw64_env_get_cpu(env)->parent_obj);
    SW64CPU *cpu = SW64_CPU(cs);
    if ((index == DTB_IA) || (index == DTB_IV) || (index == DTB_IVP) ||
        (index == DTB_IU) || (index == DTB_IS) || (index == ITB_IA) ||
        (index == ITB_IV) || (index == ITB_IVP) || (index == ITB_IU) ||
        (index == ITB_IS) || (index == PTBR)) {
        tlb_flush(cs);
    }
    if (index == INT_CLR) {
        env->csr[INT_STAT] &= ~va;
    }
    if ((index == TIMER_CTL) && (va == 1)) {
	timer_mod(cpu->alarm_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + env->csr[TIMER_TH]);
    }

    if (index == TIMER_CTL && env->csr[index] == 1) {
	timer_mod(cpu->alarm_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 1000000000 / 250);
    }
#endif
}

uint64_t cpu_sw64_load_fpcr(CPUSW64State *env)
{
    return (uint64_t)env->fpcr;
}

void helper_tb_flush(CPUSW64State *env)
{
    tb_flush(CPU(sw64_env_get_cpu(env)));
}

void helper_cpustate_update(CPUSW64State *env, uint64_t pc)
{
    switch (pc & 0x3) {
    case 0x00:
        env->flags = ENV_FLAG_HM_MODE;
        break;
    case 0x01:
        env->flags &= ~(ENV_FLAG_PS_USER | ENV_FLAG_HM_MODE);
        break;
    case 0x02:
        env->flags &= ~(ENV_FLAG_PS_USER | ENV_FLAG_HM_MODE);
        break;
    case 0x03:
        env->flags = ENV_FLAG_PS_USER;
    }
}
