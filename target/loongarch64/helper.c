/*
 * LOONGARCH emulation helpers for qemu.
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
#include "cpu.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "exec/log.h"
#include "hw/loongarch/cpudevs.h"

#if !defined(CONFIG_USER_ONLY)

static int ls3a5k_map_address_tlb_entry(CPULOONGARCHState *env,
                                        hwaddr *physical, int *prot,
                                        target_ulong address, int rw,
                                        int access_type, ls3a5k_tlb_t *tlb)
{
    uint64_t mask = tlb->PageMask;
    int n = !!(address & mask & ~(mask >> 1));
    uint32_t plv = env->CSR_CRMD & CSR_CRMD_PLV;

    /* Check access rights */
    if (!(n ? tlb->V1 : tlb->V0)) {
        return TLBRET_INVALID;
    }

    if (rw == MMU_INST_FETCH && (n ? tlb->XI1 : tlb->XI0)) {
        return TLBRET_XI;
    }

    if (rw == MMU_DATA_LOAD && (n ? tlb->RI1 : tlb->RI0)) {
        return TLBRET_RI;
    }

    if (plv > (n ? tlb->PLV1 : tlb->PLV0)) {
        return TLBRET_PE;
    }

    if (rw != MMU_DATA_STORE || (n ? tlb->WE1 : tlb->WE0)) {
        /*
         *         PPN     address
         *  4 KB: [47:13]   [12;0]
         * 16 KB: [47:15]   [14:0]
         */
        if (n) {
            *physical = tlb->PPN1 | (address & (mask >> 1));
        } else {
            *physical = tlb->PPN0 | (address & (mask >> 1));
        }
        *prot = PAGE_READ;
        if (n ? tlb->WE1 : tlb->WE0) {
            *prot |= PAGE_WRITE;
        }
        if (!(n ? tlb->XI1 : tlb->XI0)) {
            *prot |= PAGE_EXEC;
        }
        return TLBRET_MATCH;
    }

    return TLBRET_DIRTY;
}

/* Loongarch 3A5K -style MMU emulation */
int ls3a5k_map_address(CPULOONGARCHState *env, hwaddr *physical, int *prot,
                       target_ulong address, int rw, int access_type)
{
    uint16_t asid = env->CSR_ASID & 0x3ff;
    int i;
    ls3a5k_tlb_t *tlb;

    int ftlb_size = env->tlb->mmu.ls3a5k.ftlb_size;
    int vtlb_size = env->tlb->mmu.ls3a5k.vtlb_size;

    int ftlb_idx;

    uint64_t mask;
    uint64_t vpn; /* address to map */
    uint64_t tag; /* address in TLB entry */

    /* search VTLB */
    for (i = ftlb_size; i < ftlb_size + vtlb_size; ++i) {
        tlb = &env->tlb->mmu.ls3a5k.tlb[i];
        mask = tlb->PageMask;

        vpn = address & 0xffffffffe000 & ~mask;
        tag = tlb->VPN & ~mask;

        if ((tlb->G == 1 || tlb->ASID == asid) && vpn == tag &&
            tlb->EHINV != 1) {
            return ls3a5k_map_address_tlb_entry(env, physical, prot, address,
                                                rw, access_type, tlb);
        }
    }

    if (ftlb_size == 0) {
        return TLBRET_NOMATCH;
    }

    /* search FTLB */
    mask = env->tlb->mmu.ls3a5k.ftlb_mask;
    vpn = address & 0xffffffffe000 & ~mask;

    ftlb_idx = (address & 0xffffffffc000) >> 15; /* 16 KB */
    ftlb_idx = ftlb_idx & 0xff;                  /* [0,255] */

    for (i = 0; i < 8; ++i) {
        /*
         *  ---------- set  0     1     2    ...  7
         *  ftlb_idx  -----------------------------------
         *      0     |     0     1     2    ...  7
         *      1     |     8     9     10   ...  15
         *      2     |     16    17    18   ...  23
         *     ...    |
         *      255   |     2040  2041  2042 ...  2047
         */
        tlb = &env->tlb->mmu.ls3a5k.tlb[ftlb_idx * 8 + i];
        tag = tlb->VPN & ~mask;

        if ((tlb->G == 1 || tlb->ASID == asid) && vpn == tag &&
            tlb->EHINV != 1) {
            return ls3a5k_map_address_tlb_entry(env, physical, prot, address,
                                                rw, access_type, tlb);
        }
    }

    return TLBRET_NOMATCH;
}

static int get_physical_address(CPULOONGARCHState *env, hwaddr *physical,
                                int *prot, target_ulong real_address, int rw,
                                int access_type, int mmu_idx)
{
    int user_mode = mmu_idx == LARCH_HFLAG_UM;
    int kernel_mode = !user_mode;
    unsigned plv, base_c, base_v, tmp;

    /* effective address (modified for KVM T&E kernel segments) */
    target_ulong address = real_address;

    /* Check PG */
    if (!(env->CSR_CRMD & CSR_CRMD_PG)) {
        /* DA mode */
        *physical = address & 0xffffffffffffUL;
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TLBRET_MATCH;
    }

    plv = kernel_mode | (user_mode << 3);
    base_v = address >> CSR_DMW_BASE_SH;
    /* Check direct map window 0 */
    base_c = env->CSR_DMWIN0 >> CSR_DMW_BASE_SH;
    if ((plv & env->CSR_DMWIN0) && (base_c == base_v)) {
        *physical = dmwin_va2pa(address);
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TLBRET_MATCH;
    }
    /* Check direct map window 1 */
    base_c = env->CSR_DMWIN1 >> CSR_DMW_BASE_SH;
    if ((plv & env->CSR_DMWIN1) && (base_c == base_v)) {
        *physical = dmwin_va2pa(address);
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TLBRET_MATCH;
    }
    /* Check valid extension */
    tmp = address >> 47;
    if (!(tmp == 0 || tmp == 0x1ffff)) {
        return TLBRET_BADADDR;
    }
    /* mapped address */
    return env->tlb->map_address(env, physical, prot, real_address, rw,
                                 access_type);
}

void cpu_loongarch_tlb_flush(CPULOONGARCHState *env)
{
    LOONGARCHCPU *cpu = loongarch_env_get_cpu(env);

    /* Flush qemu's TLB and discard all shadowed entries.  */
    tlb_flush(CPU(cpu));
    env->tlb->tlb_in_use = env->tlb->nb_tlb;
}
#endif

static void raise_mmu_exception(CPULOONGARCHState *env, target_ulong address,
                                int rw, int tlb_error)
{
    CPUState *cs = CPU(loongarch_env_get_cpu(env));
    int exception = 0, error_code = 0;

    if (rw == MMU_INST_FETCH) {
        error_code |= EXCP_INST_NOTAVAIL;
    }

    switch (tlb_error) {
    default:
    case TLBRET_BADADDR:
        /* Reference to kernel address from user mode or supervisor mode */
        /* Reference to supervisor address from user mode */
        if (rw == MMU_DATA_STORE) {
            exception = EXCP_AdES;
        } else {
            exception = EXCP_AdEL;
        }
        break;
    case TLBRET_NOMATCH:
        /* No TLB match for a mapped address */
        if (rw == MMU_DATA_STORE) {
            exception = EXCP_TLBS;
        } else {
            exception = EXCP_TLBL;
        }
        error_code |= EXCP_TLB_NOMATCH;
        break;
    case TLBRET_INVALID:
        /* TLB match with no valid bit */
        if (rw == MMU_DATA_STORE) {
            exception = EXCP_TLBS;
        } else {
            exception = EXCP_TLBL;
        }
        break;
    case TLBRET_DIRTY:
        /* TLB match but 'D' bit is cleared */
        exception = EXCP_LTLBL;
        break;
    case TLBRET_XI:
        /* Execute-Inhibit Exception */
        exception = EXCP_TLBXI;
        break;
    case TLBRET_RI:
        /* Read-Inhibit Exception */
        exception = EXCP_TLBRI;
        break;
    case TLBRET_PE:
        /* Privileged Exception */
        exception = EXCP_TLBPE;
        break;
    }

    if (env->insn_flags & INSN_LOONGARCH) {
        if (tlb_error == TLBRET_NOMATCH) {
            env->CSR_TLBRBADV = address;
            env->CSR_TLBREHI = address & (TARGET_PAGE_MASK << 1);
            cs->exception_index = exception;
            env->error_code = error_code;
            return;
        }
    }

    /* Raise exception */
    env->CSR_BADV = address;
    cs->exception_index = exception;
    env->error_code = error_code;

    if (env->insn_flags & INSN_LOONGARCH) {
        env->CSR_TLBEHI = address & (TARGET_PAGE_MASK << 1);
    }
}

bool loongarch_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                            MMUAccessType access_type, int mmu_idx, bool probe,
                            uintptr_t retaddr)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
#if !defined(CONFIG_USER_ONLY)
    hwaddr physical;
    int prot;
    int loongarch_access_type;
#endif
    int ret = TLBRET_BADADDR;

    qemu_log_mask(CPU_LOG_MMU,
                  "%s pc " TARGET_FMT_lx " ad %" VADDR_PRIx " mmu_idx %d\n",
                  __func__, env->active_tc.PC, address, mmu_idx);

    /* data access */
#if !defined(CONFIG_USER_ONLY)
    /* XXX: put correct access by using cpu_restore_state() correctly */
    loongarch_access_type = ACCESS_INT;
    ret = get_physical_address(env, &physical, &prot, address, access_type,
                               loongarch_access_type, mmu_idx);
    switch (ret) {
    case TLBRET_MATCH:
        qemu_log_mask(CPU_LOG_MMU,
                      "%s address=%" VADDR_PRIx " physical " TARGET_FMT_plx
                      " prot %d asid %ld pc 0x%lx\n",
                      __func__, address, physical, prot, env->CSR_ASID,
                      env->active_tc.PC);
        break;
    default:
        qemu_log_mask(CPU_LOG_MMU,
                      "%s address=%" VADDR_PRIx " ret %d asid %ld pc 0x%lx\n",
                      __func__, address, ret, env->CSR_ASID,
                      env->active_tc.PC);
        break;
    }
    if (ret == TLBRET_MATCH) {
        tlb_set_page(cs, address & TARGET_PAGE_MASK,
                     physical & TARGET_PAGE_MASK, prot | PAGE_EXEC, mmu_idx,
                     TARGET_PAGE_SIZE);
        ret = true;
    }
    if (probe) {
        return false;
    }
#endif

    raise_mmu_exception(env, address, access_type, ret);
    do_raise_exception_err(env, cs->exception_index, env->error_code, retaddr);
}

#if !defined(CONFIG_USER_ONLY)
hwaddr cpu_loongarch_translate_address(CPULOONGARCHState *env,
                                       target_ulong address, int rw)
{
    hwaddr physical;
    int prot;
    int access_type;
    int ret = 0;

    /* data access */
    access_type = ACCESS_INT;
    ret = get_physical_address(env, &physical, &prot, address, rw, access_type,
                               cpu_mmu_index(env, false));
    if (ret != TLBRET_MATCH) {
        raise_mmu_exception(env, address, rw, ret);
        return -1LL;
    } else {
        return physical;
    }
}

static const char *const excp_names[EXCP_LAST + 1] = {
    [EXCP_RESET] = "reset",
    [EXCP_SRESET] = "soft reset",
    [EXCP_NMI] = "non-maskable interrupt",
    [EXCP_EXT_INTERRUPT] = "interrupt",
    [EXCP_AdEL] = "address error load",
    [EXCP_AdES] = "address error store",
    [EXCP_TLBF] = "TLB refill",
    [EXCP_IBE] = "instruction bus error",
    [EXCP_SYSCALL] = "syscall",
    [EXCP_BREAK] = "break",
    [EXCP_FPDIS] = "float unit unusable",
    [EXCP_LSXDIS] = "vector128 unusable",
    [EXCP_LASXDIS] = "vector256 unusable",
    [EXCP_RI] = "reserved instruction",
    [EXCP_OVERFLOW] = "arithmetic overflow",
    [EXCP_TRAP] = "trap",
    [EXCP_FPE] = "floating point",
    [EXCP_LTLBL] = "TLB modify",
    [EXCP_TLBL] = "TLB load",
    [EXCP_TLBS] = "TLB store",
    [EXCP_DBE] = "data bus error",
    [EXCP_TLBXI] = "TLB execute-inhibit",
    [EXCP_TLBRI] = "TLB read-inhibit",
    [EXCP_TLBPE] = "TLB priviledged error",
};
#endif

target_ulong exception_resume_pc(CPULOONGARCHState *env)
{
    target_ulong bad_pc;

    bad_pc = env->active_tc.PC;
    if (env->hflags & LARCH_HFLAG_BMASK) {
        /*
         * If the exception was raised from a delay slot, come back to
         * the jump.
         */
        bad_pc -= 4;
    }

    return bad_pc;
}

#if !defined(CONFIG_USER_ONLY)
static void set_hflags_for_handler(CPULOONGARCHState *env)
{
    /* Exception handlers are entered in 32-bit mode.  */
}

static inline void set_badinstr_registers(CPULOONGARCHState *env)
{
    if ((env->insn_flags & INSN_LOONGARCH)) {
        env->CSR_BADI = cpu_ldl_code(env, env->active_tc.PC);
        return;
    }
}
#endif

static inline unsigned int get_vint_size(CPULOONGARCHState *env)
{
    unsigned int size = 0;

    switch ((env->CSR_ECFG >> 16) & 0x7) {
    case 0:
        break;
    case 1:
        size = 2 * 4; /* #Insts * inst_size */
        break;
    case 2:
        size = 4 * 4;
        break;
    case 3:
        size = 8 * 4;
        break;
    case 4:
        size = 16 * 4;
        break;
    case 5:
        size = 32 * 4;
        break;
    case 6:
        size = 64 * 4;
        break;
    case 7:
        size = 128 * 4;
        break;
    default:
        printf("%s: unexpected value", __func__);
        assert(0);
    }

    return size;
}

#define is_refill(cs, env)                                                    \
    (((cs->exception_index == EXCP_TLBL) ||                                   \
      (cs->exception_index == EXCP_TLBS)) &&                                  \
     (env->error_code & EXCP_TLB_NOMATCH))

void loongarch_cpu_do_interrupt(CPUState *cs)
{
#if !defined(CONFIG_USER_ONLY)
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    bool update_badinstr = 0;
    int cause = -1;
    const char *name;

    if (qemu_loglevel_mask(CPU_LOG_INT) &&
        cs->exception_index != EXCP_EXT_INTERRUPT) {
        if (cs->exception_index < 0 || cs->exception_index > EXCP_LAST) {
            name = "unknown";
        } else {
            name = excp_names[cs->exception_index];
        }

        qemu_log("%s enter: PC " TARGET_FMT_lx " ERA " TARGET_FMT_lx
                 " TLBRERA 0x%016lx"
                 " %s exception\n",
                 __func__, env->active_tc.PC, env->CSR_ERA, env->CSR_TLBRERA,
                 name);
    }

    switch (cs->exception_index) {
    case EXCP_RESET:
        cpu_reset(CPU(cpu));
        break;
    case EXCP_NMI:
        env->CSR_ERRERA = exception_resume_pc(env);
        env->hflags &= ~LARCH_HFLAG_BMASK;
        env->hflags |= LARCH_HFLAG_64;
        env->hflags &= ~LARCH_HFLAG_AWRAP;
        env->hflags &= ~(LARCH_HFLAG_KSU);
        env->active_tc.PC = env->exception_base;
        set_hflags_for_handler(env);
        break;
    case EXCP_EXT_INTERRUPT:
        cause = 0;
        goto set_ERA;
    case EXCP_LTLBL:
        cause = 1;
        update_badinstr = !(env->error_code & EXCP_INST_NOTAVAIL);
        goto set_ERA;
    case EXCP_TLBL:
        cause = 2;
        update_badinstr = !(env->error_code & EXCP_INST_NOTAVAIL);
        goto set_ERA;
    case EXCP_TLBS:
        cause = 3;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_AdEL:
        cause = 4;
        update_badinstr = !(env->error_code & EXCP_INST_NOTAVAIL);
        goto set_ERA;
    case EXCP_AdES:
        cause = 5;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_IBE:
        cause = 6;
        goto set_ERA;
    case EXCP_DBE:
        cause = 7;
        goto set_ERA;
    case EXCP_SYSCALL:
        cause = 8;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_BREAK:
        cause = 9;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_RI:
        cause = 10;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_FPDIS:
    case EXCP_LSXDIS:
    case EXCP_LASXDIS:
        cause = 11;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_OVERFLOW:
        cause = 12;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_TRAP:
        cause = 13;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_FPE:
        cause = 15;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_TLBRI:
        cause = 19;
        update_badinstr = 1;
        goto set_ERA;
    case EXCP_TLBXI:
    case EXCP_TLBPE:
        cause = 20;
        goto set_ERA;
    set_ERA:
        if (is_refill(cs, env)) {
            env->CSR_TLBRERA = exception_resume_pc(env);
            env->CSR_TLBRERA |= 1;
        } else {
            env->CSR_ERA = exception_resume_pc(env);
        }

        if (update_badinstr) {
            set_badinstr_registers(env);
        }
        env->hflags &= ~(LARCH_HFLAG_KSU);

        env->hflags &= ~LARCH_HFLAG_BMASK;
        if (env->insn_flags & INSN_LOONGARCH) {
            /* save PLV and IE */
            if (is_refill(cs, env)) {
                env->CSR_TLBRPRMD &= (~0x7);
                env->CSR_TLBRPRMD |= (env->CSR_CRMD & 0x7);
            } else {
                env->CSR_PRMD &= (~0x7);
                env->CSR_PRMD |= (env->CSR_CRMD & 0x7);
            }

            env->CSR_CRMD &= ~(0x7);

            switch (cs->exception_index) {
            case EXCP_EXT_INTERRUPT:
                break;
            case EXCP_TLBL:
                if (env->error_code & EXCP_INST_NOTAVAIL) {
                    cause = EXCCODE_TLBI;
                } else {
                    cause = EXCCODE_TLBL;
                }
                break;
            case EXCP_TLBS:
                cause = EXCCODE_TLBS;
                break;
            case EXCP_LTLBL:
                cause = EXCCODE_MOD;
                break;
            case EXCP_TLBRI:
                cause = EXCCODE_TLBRI;
                break;
            case EXCP_TLBXI:
                cause = EXCCODE_TLBXI;
                break;
            case EXCP_TLBPE:
                cause = EXCCODE_TLBPE;
                break;
            case EXCP_AdEL:
            case EXCP_AdES:
            case EXCP_IBE:
            case EXCP_DBE:
                cause = EXCCODE_ADE;
                break;
            case EXCP_SYSCALL:
                cause = EXCCODE_SYS;
                break;
            case EXCP_BREAK:
                cause = EXCCODE_BP;
                break;
            case EXCP_RI:
                cause = EXCCODE_RI;
                break;
            case EXCP_FPDIS:
                cause = EXCCODE_FPDIS;
                break;
            case EXCP_LSXDIS:
                cause = EXCCODE_LSXDIS;
                break;
            case EXCP_LASXDIS:
                cause = EXCCODE_LASXDIS;
                break;
            case EXCP_FPE:
                cause = EXCCODE_FPE;
                break;
            default:
                printf("Error: exception(%d) '%s' has not been supported\n",
                       cs->exception_index, excp_names[cs->exception_index]);
                abort();
            }

            uint32_t vec_size = get_vint_size(env);
            env->active_tc.PC = env->CSR_EEPN;
            env->active_tc.PC += cause * vec_size;
            if (is_refill(cs, env)) {
                /* TLB Refill */
                env->active_tc.PC = env->CSR_TLBRENT;
                break; /* Do not modify excode */
            }
            if (cs->exception_index == EXCP_EXT_INTERRUPT) {
                /* Interrupt */
                uint32_t vector = 0;
                uint32_t pending = env->CSR_ESTAT & CSR_ESTAT_IPMASK;
                pending &= env->CSR_ECFG & CSR_ECFG_IPMASK;

                /* Find the highest-priority interrupt. */
                while (pending >>= 1) {
                    vector++;
                }
                env->active_tc.PC =
                    env->CSR_EEPN + (EXCODE_IP + vector) * vec_size;
                if (qemu_loglevel_mask(CPU_LOG_INT)) {
                    qemu_log("%s: PC " TARGET_FMT_lx " ERA " TARGET_FMT_lx
                             " cause %d\n"
                             "    A " TARGET_FMT_lx " D " TARGET_FMT_lx
                             " vector = %d ExC %08lx ExS %08lx\n",
                             __func__, env->active_tc.PC, env->CSR_ERA, cause,
                             env->CSR_BADV, env->CSR_DERA, vector,
                             env->CSR_ECFG, env->CSR_ESTAT);
                }
            }
            /* Excode */
            env->CSR_ESTAT = (env->CSR_ESTAT & ~(0x1f << CSR_ESTAT_EXC_SH)) |
                             (cause << CSR_ESTAT_EXC_SH);
        }
        set_hflags_for_handler(env);
        break;
    default:
        abort();
    }
    if (qemu_loglevel_mask(CPU_LOG_INT) &&
        cs->exception_index != EXCP_EXT_INTERRUPT) {
        qemu_log("%s: PC " TARGET_FMT_lx " ERA 0x%08lx"
                 " cause %d%s\n"
                 " ESTAT %08lx EXCFG 0x%08lx BADVA 0x%08lx BADI 0x%08lx \
                 SYS_NUM %lu cpu %d asid 0x%lx"
                 "\n",
                 __func__, env->active_tc.PC,
                 is_refill(cs, env) ? env->CSR_TLBRERA : env->CSR_ERA, cause,
                 is_refill(cs, env) ? "(refill)" : "", env->CSR_ESTAT,
                 env->CSR_ECFG,
                 is_refill(cs, env) ? env->CSR_TLBRBADV : env->CSR_BADV,
                 env->CSR_BADI, env->active_tc.gpr[11], cs->cpu_index,
                 env->CSR_ASID);
    }
#endif
    cs->exception_index = EXCP_NONE;
}

bool loongarch_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    if (interrupt_request & CPU_INTERRUPT_HARD) {
        LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
        CPULOONGARCHState *env = &cpu->env;

        if (cpu_loongarch_hw_interrupts_enabled(env) &&
            cpu_loongarch_hw_interrupts_pending(env)) {
            /* Raise it */
            cs->exception_index = EXCP_EXT_INTERRUPT;
            env->error_code = 0;
            loongarch_cpu_do_interrupt(cs);
            return true;
        }
    }
    return false;
}

void QEMU_NORETURN do_raise_exception_err(CPULOONGARCHState *env,
                                          uint32_t exception, int error_code,
                                          uintptr_t pc)
{
    CPUState *cs = CPU(loongarch_env_get_cpu(env));

    qemu_log_mask(CPU_LOG_INT, "%s: %d %d\n", __func__, exception, error_code);
    cs->exception_index = exception;
    env->error_code = error_code;

    cpu_loop_exit_restore(cs, pc);
}
