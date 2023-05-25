/*
 * KVM/LOONGARCH: LOONGARCH specific KVM APIs
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
#include <sys/ioctl.h>

#include <linux/kvm.h>

#include "qemu-common.h"
#include "cpu.h"
#include "internal.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "qemu/main-loop.h"
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "sysemu/runstate.h"
#include "sysemu/cpus.h"
#include "kvm_larch.h"
#include "exec/memattrs.h"
#include "exec/gdbstub.h"

#define DEBUG_KVM 0
/*
 * A 16384-byte buffer can hold the 8-byte kvm_msrs header, plus
 * 2047 kvm_msr_entry structs
 */
#define CSR_BUF_SIZE 16384

#define DPRINTF(fmt, ...)                                                     \
    do {                                                                      \
        if (DEBUG_KVM) {                                                      \
            fprintf(stderr, fmt, ##__VA_ARGS__);                              \
        }                                                                     \
    } while (0)

/*
 * Define loongarch kvm version.
 * Add version number when
 * qemu/kvm interface changed
 */
#define KVM_LOONGARCH_VERSION 1

static struct {
    target_ulong addr;
    int len;
    int type;
} inst_breakpoint[8], data_breakpoint[8];

int nb_data_breakpoint = 0, nb_inst_breakpoint = 0;
static int kvm_loongarch_version_cap;

/*
 * Hardware breakpoint control register
 * 4:1 plv0-plv3 enable
 * 6:5 config virtualization mode
 * 9:8 load store
 */
static const int type_code[] = { [GDB_BREAKPOINT_HW] = 0x5e,
                                 [GDB_WATCHPOINT_READ] = (0x5e | 1 << 8),
                                 [GDB_WATCHPOINT_WRITE] = (0x5e | 1 << 9),
                                 [GDB_WATCHPOINT_ACCESS] =
                                     (0x5e | 1 << 8 | 1 << 9) };

const KVMCapabilityInfo kvm_arch_required_capabilities[] = {
    KVM_CAP_LAST_INFO
};

static void kvm_loongarch_update_state(void *opaque, bool running,
                                       RunState state);
static inline int kvm_larch_putq(CPUState *cs, uint64_t reg_id,
                                 uint64_t *addr);

unsigned long kvm_arch_vcpu_id(CPUState *cs)
{
    return cs->cpu_index;
}

int kvm_arch_init(MachineState *ms, KVMState *s)
{
    /* LOONGARCH has 128 signals */
    kvm_set_sigmask_len(s, 16);

    kvm_loongarch_version_cap = kvm_check_extension(s, KVM_CAP_LOONGARCH_VZ);

    if (kvm_loongarch_version_cap != KVM_LOONGARCH_VERSION) {
        warn_report("QEMU/KVM version not match, qemu_la_version: lvz-%d,\
                     kvm_la_version: lvz-%d \n",
                    KVM_LOONGARCH_VERSION, kvm_loongarch_version_cap);
    }
    return 0;
}

int kvm_arch_irqchip_create(KVMState *s)
{
    return 0;
}

static void kvm_csr_set_addr(uint64_t **addr, uint32_t index, uint64_t *p)
{
    addr[index] = p;
}

int kvm_arch_init_vcpu(CPUState *cs)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    uint64_t **addr;
    CPULOONGARCHState *env = &cpu->env;
    int ret = 0;

    kvm_vcpu_enable_cap(cs, KVM_CAP_LOONGARCH_FPU, 0, 0);
    kvm_vcpu_enable_cap(cs, KVM_CAP_LOONGARCH_LSX, 0, 0);

    cpu->cpuStateEntry =
        qemu_add_vm_change_state_handler(kvm_loongarch_update_state, cs);
    cpu->kvm_csr_buf = g_malloc0(CSR_BUF_SIZE + CSR_BUF_SIZE);

    addr = (void *)cpu->kvm_csr_buf + CSR_BUF_SIZE;
    kvm_csr_set_addr(addr, LOONGARCH_CSR_CRMD, &env->CSR_CRMD);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PRMD, &env->CSR_PRMD);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_EUEN, &env->CSR_EUEN);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_MISC, &env->CSR_MISC);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ECFG, &env->CSR_ECFG);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ESTAT, &env->CSR_ESTAT);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ERA, &env->CSR_ERA);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_BADV, &env->CSR_BADV);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_BADI, &env->CSR_BADI);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_EEPN, &env->CSR_EEPN);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBIDX, &env->CSR_TLBIDX);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBEHI, &env->CSR_TLBEHI);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBELO0, &env->CSR_TLBELO0);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBELO1, &env->CSR_TLBELO1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_GTLBC, &env->CSR_GTLBC);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TRGP, &env->CSR_TRGP);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ASID, &env->CSR_ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PGDL, &env->CSR_PGDL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PGDH, &env->CSR_PGDH);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PGD, &env->CSR_PGD);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PWCTL0, &env->CSR_PWCTL0);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PWCTL1, &env->CSR_PWCTL1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_STLBPGSIZE, &env->CSR_STLBPGSIZE);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_RVACFG, &env->CSR_RVACFG);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_CPUID, &env->CSR_CPUID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PRCFG1, &env->CSR_PRCFG1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PRCFG2, &env->CSR_PRCFG2);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PRCFG3, &env->CSR_PRCFG3);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_KS0, &env->CSR_KS0);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_KS1, &env->CSR_KS1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_KS2, &env->CSR_KS2);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_KS3, &env->CSR_KS3);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_KS4, &env->CSR_KS4);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_KS5, &env->CSR_KS5);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_KS6, &env->CSR_KS6);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_KS7, &env->CSR_KS7);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TMID, &env->CSR_TMID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_CNTC, &env->CSR_CNTC);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TINTCLR, &env->CSR_TINTCLR);

    kvm_csr_set_addr(addr, LOONGARCH_CSR_GSTAT, &env->CSR_GSTAT);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_GCFG, &env->CSR_GCFG);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_GINTC, &env->CSR_GINTC);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_GCNTC, &env->CSR_GCNTC);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_LLBCTL, &env->CSR_LLBCTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IMPCTL1, &env->CSR_IMPCTL1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IMPCTL2, &env->CSR_IMPCTL2);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_GNMI, &env->CSR_GNMI);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBRENT, &env->CSR_TLBRENT);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBRBADV, &env->CSR_TLBRBADV);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBRERA, &env->CSR_TLBRERA);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBRSAVE, &env->CSR_TLBRSAVE);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBRELO0, &env->CSR_TLBRELO0);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBRELO1, &env->CSR_TLBRELO1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBREHI, &env->CSR_TLBREHI);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_TLBRPRMD, &env->CSR_TLBRPRMD);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ERRCTL, &env->CSR_ERRCTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ERRINFO, &env->CSR_ERRINFO);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ERRINFO1, &env->CSR_ERRINFO1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ERRENT, &env->CSR_ERRENT);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ERRERA, &env->CSR_ERRERA);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_ERRSAVE, &env->CSR_ERRSAVE);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_CTAG, &env->CSR_CTAG);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DMWIN0, &env->CSR_DMWIN0);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DMWIN1, &env->CSR_DMWIN1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DMWIN2, &env->CSR_DMWIN2);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DMWIN3, &env->CSR_DMWIN3);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PERFCTRL0, &env->CSR_PERFCTRL0);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PERFCNTR0, &env->CSR_PERFCNTR0);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PERFCTRL1, &env->CSR_PERFCTRL1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PERFCNTR1, &env->CSR_PERFCNTR1);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PERFCTRL2, &env->CSR_PERFCTRL2);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PERFCNTR2, &env->CSR_PERFCNTR2);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PERFCTRL3, &env->CSR_PERFCTRL3);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_PERFCNTR3, &env->CSR_PERFCNTR3);

    /* debug */
    kvm_csr_set_addr(addr, LOONGARCH_CSR_MWPC, &env->CSR_MWPC);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_MWPS, &env->CSR_MWPS);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB0ADDR, &env->CSR_DB0ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB0MASK, &env->CSR_DB0MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB0CTL, &env->CSR_DB0CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB0ASID, &env->CSR_DB0ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB1ADDR, &env->CSR_DB1ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB1MASK, &env->CSR_DB1MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB1CTL, &env->CSR_DB1CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB1ASID, &env->CSR_DB1ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB2ADDR, &env->CSR_DB2ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB2MASK, &env->CSR_DB2MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB2CTL, &env->CSR_DB2CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB2ASID, &env->CSR_DB2ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB3ADDR, &env->CSR_DB3ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB3MASK, &env->CSR_DB3MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB3CTL, &env->CSR_DB3CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DB3ASID, &env->CSR_DB3ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_FWPC, &env->CSR_FWPC);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_FWPS, &env->CSR_FWPS);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB0ADDR, &env->CSR_IB0ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB0MASK, &env->CSR_IB0MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB0CTL, &env->CSR_IB0CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB0ASID, &env->CSR_IB0ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB1ADDR, &env->CSR_IB1ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB1MASK, &env->CSR_IB1MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB1CTL, &env->CSR_IB1CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB1ASID, &env->CSR_IB1ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB2ADDR, &env->CSR_IB2ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB2MASK, &env->CSR_IB2MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB2CTL, &env->CSR_IB2CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB2ASID, &env->CSR_IB2ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB3ADDR, &env->CSR_IB3ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB3MASK, &env->CSR_IB3MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB3CTL, &env->CSR_IB3CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB3ASID, &env->CSR_IB3ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB4ADDR, &env->CSR_IB4ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB4MASK, &env->CSR_IB4MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB4CTL, &env->CSR_IB4CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB4ASID, &env->CSR_IB4ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB5ADDR, &env->CSR_IB5ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB5MASK, &env->CSR_IB5MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB5CTL, &env->CSR_IB5CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB5ASID, &env->CSR_IB5ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB6ADDR, &env->CSR_IB6ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB6MASK, &env->CSR_IB6MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB6CTL, &env->CSR_IB6CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB6ASID, &env->CSR_IB6ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB7ADDR, &env->CSR_IB7ADDR);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB7MASK, &env->CSR_IB7MASK);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB7CTL, &env->CSR_IB7CTL);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_IB7ASID, &env->CSR_IB7ASID);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DEBUG, &env->CSR_DEBUG);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DERA, &env->CSR_DERA);
    kvm_csr_set_addr(addr, LOONGARCH_CSR_DESAVE, &env->CSR_DESAVE);

    DPRINTF("%s\n", __func__);
    return ret;
}

int kvm_arch_destroy_vcpu(CPUState *cs)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);

    g_free(cpu->kvm_csr_buf);
    cpu->kvm_csr_buf = NULL;
    return 0;
}

static void kvm_csr_buf_reset(LOONGARCHCPU *cpu)
{
    memset(cpu->kvm_csr_buf, 0, CSR_BUF_SIZE);
}

static void kvm_csr_entry_add(LOONGARCHCPU *cpu, uint32_t index,
                              uint64_t value)
{
    struct kvm_msrs *msrs = cpu->kvm_csr_buf;
    void *limit = ((void *)msrs) + CSR_BUF_SIZE;
    struct kvm_csr_entry *entry = &msrs->entries[msrs->ncsrs];

    assert((void *)(entry + 1) <= limit);

    entry->index = index;
    entry->reserved = 0;
    entry->data = value;
    msrs->ncsrs++;
}

void kvm_loongarch_reset_vcpu(LOONGARCHCPU *cpu)
{
    int ret = 0;
    uint64_t reset = 1;

    if (CPU(cpu)->kvm_fd > 0) {
        ret = kvm_larch_putq(CPU(cpu), KVM_REG_LOONGARCH_VCPU_RESET, &reset);
        if (ret < 0) {
            error_report("%s reset vcpu failed:%d", __func__, ret);
        }
    }

    DPRINTF("%s\n", __func__);
}

void kvm_arch_update_guest_debug(CPUState *cpu, struct kvm_guest_debug *dbg)
{
    int n;
    if (kvm_sw_breakpoints_active(cpu)) {
        dbg->control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP;
    }
    if (nb_data_breakpoint > 0) {
        dbg->control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_HW_BP;
        for (n = 0; n < nb_data_breakpoint; n++) {
            dbg->arch.data_breakpoint[n].addr = data_breakpoint[n].addr;
            dbg->arch.data_breakpoint[n].mask = 0;
            dbg->arch.data_breakpoint[n].asid = 0;
            dbg->arch.data_breakpoint[n].ctrl =
                type_code[data_breakpoint[n].type];
        }
        dbg->arch.data_bp_nums = nb_data_breakpoint;
    } else {
        dbg->arch.data_bp_nums = 0;
    }
    if (nb_inst_breakpoint > 0) {
        dbg->control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_HW_BP;
        for (n = 0; n < nb_inst_breakpoint; n++) {
            dbg->arch.inst_breakpoint[n].addr = inst_breakpoint[n].addr;
            dbg->arch.inst_breakpoint[n].mask = 0;
            dbg->arch.inst_breakpoint[n].asid = 0;
            dbg->arch.inst_breakpoint[n].ctrl =
                type_code[inst_breakpoint[n].type];
        }
        dbg->arch.inst_bp_nums = nb_inst_breakpoint;
    } else {
        dbg->arch.inst_bp_nums = 0;
    }
}

static const unsigned int brk_insn = 0x002b8005;

int kvm_arch_insert_sw_breakpoint(CPUState *cs, struct kvm_sw_breakpoint *bp)
{
    DPRINTF("%s\n", __func__);
    if (cpu_memory_rw_debug(cs, bp->pc, (uint8_t *)&bp->saved_insn, 4, 0) ||
        cpu_memory_rw_debug(cs, bp->pc, (uint8_t *)&brk_insn, 4, 1)) {
        error_report("%s failed", __func__);
        return -EINVAL;
    }
    return 0;
}

int kvm_arch_remove_sw_breakpoint(CPUState *cs, struct kvm_sw_breakpoint *bp)
{
    static uint32_t brk;

    DPRINTF("%s\n", __func__);
    if (cpu_memory_rw_debug(cs, bp->pc, (uint8_t *)&brk, 4, 0) ||
        brk != brk_insn ||
        cpu_memory_rw_debug(cs, bp->pc, (uint8_t *)&bp->saved_insn, 4, 1)) {
        error_report("%s failed", __func__);
        return -EINVAL;
    }
    return 0;
}

static int find_hw_breakpoint(uint64_t addr, int len, int type)
{
    int n;
    switch (type) {
    case GDB_BREAKPOINT_HW:
        if (nb_inst_breakpoint == 0) {
            return -1;
        }
        for (n = 0; n < nb_inst_breakpoint; n++) {
            if (inst_breakpoint[n].addr == addr &&
                inst_breakpoint[n].type == type) {
                return n;
            }
        }
        break;
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_READ:
    case GDB_WATCHPOINT_ACCESS:
        if (nb_data_breakpoint == 0) {
            return -1;
        }
        for (n = 0; n < nb_data_breakpoint; n++) {
            if (data_breakpoint[n].addr == addr &&
                data_breakpoint[n].type == type &&
                data_breakpoint[n].len == len) {
                return n;
            }
        }
        break;
    default:
        return -1;
    }
    return -1;
}

int kvm_arch_insert_hw_breakpoint(target_ulong addr, target_ulong len,
                                  int type)
{
    switch (type) {
    case GDB_BREAKPOINT_HW:
        len = 1;
        if (nb_inst_breakpoint == 8) {
            return -ENOBUFS;
        }
        if (find_hw_breakpoint(addr, len, type) >= 0) {
            return -EEXIST;
        }
        inst_breakpoint[nb_inst_breakpoint].addr = addr;
        inst_breakpoint[nb_inst_breakpoint].len = len;
        inst_breakpoint[nb_inst_breakpoint].type = type;
        nb_inst_breakpoint++;
        break;
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_READ:
    case GDB_WATCHPOINT_ACCESS:
        switch (len) {
        case 1:
        case 2:
        case 4:
        case 8:
            if (addr & (len - 1)) {
                return -EINVAL;
            }
            if (nb_data_breakpoint == 8) {
                return -ENOBUFS;
            }
            if (find_hw_breakpoint(addr, len, type) >= 0) {
                return -EEXIST;
            }
            data_breakpoint[nb_data_breakpoint].addr = addr;
            data_breakpoint[nb_data_breakpoint].len = len;
            data_breakpoint[nb_data_breakpoint].type = type;
            nb_data_breakpoint++;
            break;
        default:
            return -EINVAL;
        }
        break;
    default:
        return -ENOSYS;
    }
    return 0;
}

int kvm_arch_remove_hw_breakpoint(target_ulong addr, target_ulong len,
                                  int type)
{
    int n;
    n = find_hw_breakpoint(addr, (type == GDB_BREAKPOINT_HW) ? 1 : len, type);
    if (n < 0) {
        printf("err not find remove target\n");
        return -ENOENT;
    }
    switch (type) {
    case GDB_BREAKPOINT_HW:
        nb_inst_breakpoint--;
        inst_breakpoint[n] = inst_breakpoint[nb_inst_breakpoint];
        break;
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_READ:
    case GDB_WATCHPOINT_ACCESS:
        nb_data_breakpoint--;
        data_breakpoint[n] = data_breakpoint[nb_data_breakpoint];
        break;
    default:
        return -1;
    }
    return 0;
}

void kvm_arch_remove_all_hw_breakpoints(void)
{
    DPRINTF("%s\n", __func__);
    nb_data_breakpoint = 0;
    nb_inst_breakpoint = 0;
}

static inline int cpu_loongarch_io_interrupts_pending(LOONGARCHCPU *cpu)
{
    CPULOONGARCHState *env = &cpu->env;

    return env->CSR_ESTAT & (0x1 << 2);
}

void kvm_arch_pre_run(CPUState *cs, struct kvm_run *run)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    int r;
    struct kvm_loongarch_interrupt intr;

    qemu_mutex_lock_iothread();

    if ((cs->interrupt_request & CPU_INTERRUPT_HARD) &&
        cpu_loongarch_io_interrupts_pending(cpu)) {
        intr.cpu = -1;
        intr.irq = 2;
        r = kvm_vcpu_ioctl(cs, KVM_INTERRUPT, &intr);
        if (r < 0) {
            error_report("%s: cpu %d: failed to inject IRQ %x", __func__,
                         cs->cpu_index, intr.irq);
        }
    }

    qemu_mutex_unlock_iothread();
}

MemTxAttrs kvm_arch_post_run(CPUState *cs, struct kvm_run *run)
{
    return MEMTXATTRS_UNSPECIFIED;
}

int kvm_arch_process_async_events(CPUState *cs)
{
    return cs->halted;
}

static CPUWatchpoint hw_watchpoint;

static bool kvm_loongarch_handle_debug(CPUState *cs, struct kvm_run *run)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    int i;
    bool ret = false;
    kvm_cpu_synchronize_state(cs);
    if (cs->singlestep_enabled) {
        return true;
    }
    if (kvm_find_sw_breakpoint(cs, env->active_tc.PC)) {
        return true;
    }
    /* hw breakpoint */
    if (run->debug.arch.exception == EXCCODE_WATCH) {
        for (i = 0; i < 8; i++) {
            if (run->debug.arch.fwps & (1 << i)) {
                ret = true;
                break;
            }
        }
        for (i = 0; i < 8; i++) {
            if (run->debug.arch.mwps & (1 << i)) {
                cs->watchpoint_hit = &hw_watchpoint;
                hw_watchpoint.vaddr = data_breakpoint[i].addr;
                switch (data_breakpoint[i].type) {
                case GDB_WATCHPOINT_READ:
                    ret = true;
                    hw_watchpoint.flags = BP_MEM_READ;
                    break;
                case GDB_WATCHPOINT_WRITE:
                    ret = true;
                    hw_watchpoint.flags = BP_MEM_WRITE;
                    break;
                case GDB_WATCHPOINT_ACCESS:
                    ret = true;
                    hw_watchpoint.flags = BP_MEM_ACCESS;
                    break;
                }
            }
        }
        run->debug.arch.exception = 0;
        run->debug.arch.fwps = 0;
        run->debug.arch.mwps = 0;
    }
    return ret;
}

int kvm_arch_handle_exit(CPUState *cs, struct kvm_run *run)
{
    int ret;

    DPRINTF("%s\n", __func__);
    switch (run->exit_reason) {
    case KVM_EXIT_HYPERCALL:
        DPRINTF("handle LOONGARCH hypercall\n");
        ret = 0;
        run->hypercall.ret = ret;
        break;

    case KVM_EXIT_DEBUG:
        ret = 0;
        if (kvm_loongarch_handle_debug(cs, run)) {
            ret = EXCP_DEBUG;
        }
        break;
    default:
        error_report("%s: unknown exit reason %d", __func__, run->exit_reason);
        ret = -1;
        break;
    }

    return ret;
}

bool kvm_arch_stop_on_emulation_error(CPUState *cs)
{
    DPRINTF("%s\n", __func__);
    return true;
}

void kvm_arch_init_irq_routing(KVMState *s)
{
}

int kvm_loongarch_set_interrupt(LOONGARCHCPU *cpu, int irq, int level)
{
    CPUState *cs = CPU(cpu);
    struct kvm_loongarch_interrupt intr;

    if (!kvm_enabled()) {
        return 0;
    }

    intr.cpu = -1;

    if (level) {
        intr.irq = irq;
    } else {
        intr.irq = -irq;
    }

    kvm_vcpu_ioctl(cs, KVM_INTERRUPT, &intr);

    return 0;
}

int kvm_loongarch_set_ipi_interrupt(LOONGARCHCPU *cpu, int irq, int level)
{
    CPUState *cs = current_cpu;
    CPUState *dest_cs = CPU(cpu);
    struct kvm_loongarch_interrupt intr;

    if (!kvm_enabled()) {
        return 0;
    }

    intr.cpu = dest_cs->cpu_index;

    if (level) {
        intr.irq = irq;
    } else {
        intr.irq = -irq;
    }

    DPRINTF("%s:  IRQ: %d\n", __func__, intr.irq);
    if (!current_cpu) {
        cs = dest_cs;
    }
    kvm_vcpu_ioctl(cs, KVM_INTERRUPT, &intr);

    return 0;
}

static inline int kvm_loongarch_put_one_reg(CPUState *cs, uint64_t reg_id,
                                            int32_t *addr)
{
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)addr };

    return kvm_vcpu_ioctl(cs, KVM_SET_ONE_REG, &csrreg);
}

static inline int kvm_loongarch_put_one_ureg(CPUState *cs, uint64_t reg_id,
                                             uint32_t *addr)
{
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)addr };

    return kvm_vcpu_ioctl(cs, KVM_SET_ONE_REG, &csrreg);
}

static inline int kvm_loongarch_put_one_ulreg(CPUState *cs, uint64_t reg_id,
                                              target_ulong *addr)
{
    uint64_t val64 = *addr;
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)&val64 };

    return kvm_vcpu_ioctl(cs, KVM_SET_ONE_REG, &csrreg);
}

static inline int kvm_loongarch_put_one_reg64(CPUState *cs, int64_t reg_id,
                                              int64_t *addr)
{
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)addr };

    return kvm_vcpu_ioctl(cs, KVM_SET_ONE_REG, &csrreg);
}

static inline int kvm_larch_putq(CPUState *cs, uint64_t reg_id, uint64_t *addr)
{
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)addr };

    return kvm_vcpu_ioctl(cs, KVM_SET_ONE_REG, &csrreg);
}

static inline int kvm_loongarch_get_one_reg(CPUState *cs, uint64_t reg_id,
                                            int32_t *addr)
{
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)addr };

    return kvm_vcpu_ioctl(cs, KVM_GET_ONE_REG, &csrreg);
}

static inline int kvm_loongarch_get_one_ureg(CPUState *cs, uint64_t reg_id,
                                             uint32_t *addr)
{
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)addr };

    return kvm_vcpu_ioctl(cs, KVM_GET_ONE_REG, &csrreg);
}

static inline int kvm_loongarch_get_one_ulreg(CPUState *cs, uint64_t reg_id,
                                              target_ulong *addr)
{
    int ret;
    uint64_t val64 = 0;
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)&val64 };

    ret = kvm_vcpu_ioctl(cs, KVM_GET_ONE_REG, &csrreg);
    if (ret >= 0) {
        *addr = val64;
    }
    return ret;
}

static inline int kvm_loongarch_get_one_reg64(CPUState *cs, int64_t reg_id,
                                              int64_t *addr)
{
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)addr };

    return kvm_vcpu_ioctl(cs, KVM_GET_ONE_REG, &csrreg);
}

static inline int kvm_larch_getq(CPUState *cs, uint64_t reg_id, uint64_t *addr)
{
    struct kvm_one_reg csrreg = { .id = reg_id, .addr = (uintptr_t)addr };

    return kvm_vcpu_ioctl(cs, KVM_GET_ONE_REG, &csrreg);
}

static inline int kvm_loongarch_change_one_reg(CPUState *cs, uint64_t reg_id,
                                               int32_t *addr, int32_t mask)
{
    int err;
    int32_t tmp, change;

    err = kvm_loongarch_get_one_reg(cs, reg_id, &tmp);
    if (err < 0) {
        return err;
    }

    /* only change bits in mask */
    change = (*addr ^ tmp) & mask;
    if (!change) {
        return 0;
    }

    tmp = tmp ^ change;
    return kvm_loongarch_put_one_reg(cs, reg_id, &tmp);
}

static inline int kvm_loongarch_change_one_reg64(CPUState *cs, uint64_t reg_id,
                                                 int64_t *addr, int64_t mask)
{
    int err;
    int64_t tmp, change;

    err = kvm_loongarch_get_one_reg64(cs, reg_id, &tmp);
    if (err < 0) {
        DPRINTF("%s: Failed to get CSR_CONFIG7 (%d)\n", __func__, err);
        return err;
    }

    /* only change bits in mask */
    change = (*addr ^ tmp) & mask;
    if (!change) {
        return 0;
    }

    tmp = tmp ^ change;
    return kvm_loongarch_put_one_reg64(cs, reg_id, &tmp);
}
/*
 * Handle the VM clock being started or stopped
 */
static void kvm_loongarch_update_state(void *opaque, bool running,
                                       RunState state)
{
    CPUState *cs = opaque;
    int ret;
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);

    /*
     * If state is already dirty (synced to QEMU) then the KVM timer state is
     * already saved and can be restored when it is synced back to KVM.
     */
    if (!running) {
        ret =
            kvm_larch_getq(cs, KVM_REG_LOONGARCH_COUNTER, &cpu->counter_value);
        if (ret < 0) {
            printf("%s: Failed to get counter_value (%d)\n", __func__, ret);
        }

    } else {
        ret = kvm_larch_putq(cs, KVM_REG_LOONGARCH_COUNTER,
                             &(LOONGARCH_CPU(cs))->counter_value);
        if (ret < 0) {
            printf("%s: Failed to put counter_value (%d)\n", __func__, ret);
        }
    }
}

static int kvm_loongarch_put_fpu_registers(CPUState *cs, int level)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    int err, ret = 0;
    unsigned int i;
    struct kvm_fpu fpu;

    fpu.fcsr = env->active_fpu.fcsr0;
    for (i = 0; i < 32; i++) {
        memcpy(&fpu.fpr[i], &env->active_fpu.fpr[i],
               sizeof(struct kvm_fpureg));
    }
    for (i = 0; i < 8; i++) {
        ((char *)&fpu.fcc)[i] = env->active_fpu.cf[i];
    }
    fpu.vcsr = env->active_fpu.vcsr16;

    err = kvm_vcpu_ioctl(cs, KVM_SET_FPU, &fpu);
    if (err < 0) {
        DPRINTF("%s: Failed to get FPU (%d)\n", __func__, err);
        ret = err;
    }

    return ret;
}

static int kvm_loongarch_get_fpu_registers(CPUState *cs)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    int err, ret = 0;
    unsigned int i;
    struct kvm_fpu fpu;

    err = kvm_vcpu_ioctl(cs, KVM_GET_FPU, &fpu);
    if (err < 0) {
        DPRINTF("%s: Failed to get FPU (%d)\n", __func__, err);
        ret = err;
    } else {
        env->active_fpu.fcsr0 = fpu.fcsr;
        for (i = 0; i < 32; i++) {
            memcpy(&env->active_fpu.fpr[i], &fpu.fpr[i],
                   sizeof(struct kvm_fpureg));
        }
        for (i = 0; i < 8; i++) {
            env->active_fpu.cf[i] = ((char *)&fpu.fcc)[i];
        }
        env->active_fpu.vcsr16 = fpu.vcsr;
    }

    return ret;
}

#define KVM_PUT_ONE_UREG64(cs, regidx, addr)                                  \
    ({                                                                        \
        int err;                                                              \
        uint64_t csrid = 0;                                                   \
        csrid = (KVM_IOC_CSRID(regidx));                                      \
        err = kvm_larch_putq(cs, csrid, addr);                                \
        if (err < 0) {                                                        \
            DPRINTF("%s: Failed to put regidx 0x%x err:%d\n", __func__,       \
                    regidx, err);                                             \
        }                                                                     \
        err;                                                                  \
    })

static int kvm_loongarch_put_csr_registers(CPUState *cs, int level)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    int ret = 0;

    (void)level;

    kvm_csr_buf_reset(cpu);

    kvm_csr_entry_add(cpu, LOONGARCH_CSR_CRMD, env->CSR_CRMD);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PRMD, env->CSR_PRMD);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_EUEN, env->CSR_EUEN);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_MISC, env->CSR_MISC);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ECFG, env->CSR_ECFG);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ESTAT, env->CSR_ESTAT);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERA, env->CSR_ERA);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_BADV, env->CSR_BADV);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_BADI, env->CSR_BADI);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_EEPN, env->CSR_EEPN);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBIDX, env->CSR_TLBIDX);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBEHI, env->CSR_TLBEHI);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBELO0, env->CSR_TLBELO0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBELO1, env->CSR_TLBELO1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GTLBC, env->CSR_GTLBC);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TRGP, env->CSR_TRGP);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ASID, env->CSR_ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PGDL, env->CSR_PGDL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PGDH, env->CSR_PGDH);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PGD, env->CSR_PGD);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PWCTL0, env->CSR_PWCTL0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PWCTL1, env->CSR_PWCTL1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_STLBPGSIZE, env->CSR_STLBPGSIZE);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_RVACFG, env->CSR_RVACFG);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_CPUID, env->CSR_CPUID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PRCFG1, env->CSR_PRCFG1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PRCFG2, env->CSR_PRCFG2);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PRCFG3, env->CSR_PRCFG3);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS0, env->CSR_KS0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS1, env->CSR_KS1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS2, env->CSR_KS2);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS3, env->CSR_KS3);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS4, env->CSR_KS4);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS5, env->CSR_KS5);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS6, env->CSR_KS6);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS7, env->CSR_KS7);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TMID, env->CSR_TMID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_CNTC, env->CSR_CNTC);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TINTCLR, env->CSR_TINTCLR);

    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GSTAT, env->CSR_GSTAT);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GCFG, env->CSR_GCFG);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GINTC, env->CSR_GINTC);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GCNTC, env->CSR_GCNTC);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_LLBCTL, env->CSR_LLBCTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IMPCTL1, env->CSR_IMPCTL1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IMPCTL2, env->CSR_IMPCTL2);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GNMI, env->CSR_GNMI);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRENT, env->CSR_TLBRENT);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRBADV, env->CSR_TLBRBADV);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRERA, env->CSR_TLBRERA);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRSAVE, env->CSR_TLBRSAVE);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRELO0, env->CSR_TLBRELO0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRELO1, env->CSR_TLBRELO1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBREHI, env->CSR_TLBREHI);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRPRMD, env->CSR_TLBRPRMD);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRCTL, env->CSR_ERRCTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRINFO, env->CSR_ERRINFO);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRINFO1, env->CSR_ERRINFO1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRENT, env->CSR_ERRENT);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRERA, env->CSR_ERRERA);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRSAVE, env->CSR_ERRSAVE);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_CTAG, env->CSR_CTAG);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DMWIN0, env->CSR_DMWIN0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DMWIN1, env->CSR_DMWIN1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DMWIN2, env->CSR_DMWIN2);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DMWIN3, env->CSR_DMWIN3);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCTRL0, env->CSR_PERFCTRL0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCNTR0, env->CSR_PERFCNTR0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCTRL1, env->CSR_PERFCTRL1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCNTR1, env->CSR_PERFCNTR1);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCTRL2, env->CSR_PERFCTRL2);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCNTR2, env->CSR_PERFCNTR2);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCTRL3, env->CSR_PERFCTRL3);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCNTR3, env->CSR_PERFCNTR3);

    /* debug */
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_MWPC, env->CSR_MWPC);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_MWPS, env->CSR_MWPS);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB0ADDR, env->CSR_DB0ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB0MASK, env->CSR_DB0MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB0CTL, env->CSR_DB0CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB0ASID, env->CSR_DB0ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB1ADDR, env->CSR_DB1ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB1MASK, env->CSR_DB1MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB1CTL, env->CSR_DB1CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB1ASID, env->CSR_DB1ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB2ADDR, env->CSR_DB2ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB2MASK, env->CSR_DB2MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB2CTL, env->CSR_DB2CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB2ASID, env->CSR_DB2ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB3ADDR, env->CSR_DB3ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB3MASK, env->CSR_DB3MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB3CTL, env->CSR_DB3CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB3ASID, env->CSR_DB3ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_FWPC, env->CSR_FWPC);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_FWPS, env->CSR_FWPS);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB0ADDR, env->CSR_IB0ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB0MASK, env->CSR_IB0MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB0CTL, env->CSR_IB0CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB0ASID, env->CSR_IB0ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB1ADDR, env->CSR_IB1ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB1MASK, env->CSR_IB1MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB1CTL, env->CSR_IB1CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB1ASID, env->CSR_IB1ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB2ADDR, env->CSR_IB2ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB2MASK, env->CSR_IB2MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB2CTL, env->CSR_IB2CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB2ASID, env->CSR_IB2ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB3ADDR, env->CSR_IB3ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB3MASK, env->CSR_IB3MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB3CTL, env->CSR_IB3CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB3ASID, env->CSR_IB3ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB4ADDR, env->CSR_IB4ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB4MASK, env->CSR_IB4MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB4CTL, env->CSR_IB4CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB4ASID, env->CSR_IB4ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB5ADDR, env->CSR_IB5ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB5MASK, env->CSR_IB5MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB5CTL, env->CSR_IB5CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB5ASID, env->CSR_IB5ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB6ADDR, env->CSR_IB6ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB6MASK, env->CSR_IB6MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB6CTL, env->CSR_IB6CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB6ASID, env->CSR_IB6ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB7ADDR, env->CSR_IB7ADDR);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB7MASK, env->CSR_IB7MASK);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB7CTL, env->CSR_IB7CTL);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB7ASID, env->CSR_IB7ASID);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DEBUG, env->CSR_DEBUG);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DERA, env->CSR_DERA);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DESAVE, env->CSR_DESAVE);

    ret = kvm_vcpu_ioctl(cs, KVM_SET_MSRS, cpu->kvm_csr_buf);
    if (ret < cpu->kvm_csr_buf->ncsrs) {
        struct kvm_csr_entry *e = &cpu->kvm_csr_buf->entries[ret];
        printf("error: failed to set CSR 0x%" PRIx32 " to 0x%" PRIx64 "\n",
               (uint32_t)e->index, (uint64_t)e->data);
    }

    /*
     * timer cfg must be put at last since it is used to enable
     * guest timer
     */
    ret |= KVM_PUT_ONE_UREG64(cs, LOONGARCH_CSR_TVAL, &env->CSR_TVAL);
    ret |= KVM_PUT_ONE_UREG64(cs, LOONGARCH_CSR_TCFG, &env->CSR_TCFG);
    return ret;
}

#define KVM_GET_ONE_UREG64(cs, regidx, addr)                                  \
    ({                                                                        \
        int err;                                                              \
        uint64_t csrid = 0;                                                   \
        csrid = (KVM_IOC_CSRID(regidx));                                      \
        err = kvm_larch_getq(cs, csrid, addr);                                \
        if (err < 0) {                                                        \
            DPRINTF("%s: Failed to put regidx 0x%x err:%d\n", __func__,       \
                    regidx, err);                                             \
        }                                                                     \
        err;                                                                  \
    })

static int kvm_loongarch_get_csr_registers(CPUState *cs)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    int ret = 0, i;
    struct kvm_csr_entry *csrs = cpu->kvm_csr_buf->entries;
    uint64_t **addr;

    kvm_csr_buf_reset(cpu);
    addr = (void *)cpu->kvm_csr_buf + CSR_BUF_SIZE;

    kvm_csr_entry_add(cpu, LOONGARCH_CSR_CRMD, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PRMD, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_EUEN, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_MISC, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ECFG, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ESTAT, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERA, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_BADV, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_BADI, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_EEPN, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBIDX, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBEHI, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBELO0, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBELO1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GTLBC, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TRGP, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PGDL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PGDH, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PGD, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PWCTL0, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PWCTL1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_STLBPGSIZE, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_RVACFG, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_CPUID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PRCFG1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PRCFG2, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PRCFG3, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS0, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS2, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS3, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS4, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS5, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS6, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_KS7, 0);

    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TMID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_CNTC, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TINTCLR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GSTAT, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GCFG, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GINTC, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GCNTC, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_LLBCTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IMPCTL1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IMPCTL2, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_GNMI, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRENT, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRBADV, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRERA, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRSAVE, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRELO0, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRELO1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBREHI, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_TLBRPRMD, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRCTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRINFO, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRINFO1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRENT, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRERA, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_ERRSAVE, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_CTAG, 0);

    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DMWIN0, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DMWIN1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DMWIN2, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DMWIN3, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCTRL0, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCNTR0, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCTRL1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCNTR1, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCTRL2, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCNTR2, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCTRL3, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_PERFCNTR3, 0);

    /* debug */
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_MWPC, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_MWPS, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB0ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB0MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB0CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB0ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB1ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB1MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB1CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB1ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB2ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB2MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB2CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB2ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB3ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB3MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB3CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DB3ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_FWPC, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_FWPS, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB0ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB0MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB0CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB0ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB1ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB1MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB1CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB1ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB2ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB2MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB2CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB2ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB3ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB3MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB3CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB3ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB4ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB4MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB4CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB4ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB5ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB5MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB5CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB5ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB6ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB6MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB6CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB6ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB7ADDR, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB7MASK, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB7CTL, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_IB7ASID, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DEBUG, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DERA, 0);
    kvm_csr_entry_add(cpu, LOONGARCH_CSR_DESAVE, 0);

    ret = kvm_vcpu_ioctl(cs, KVM_GET_MSRS, cpu->kvm_csr_buf);
    if (ret < cpu->kvm_csr_buf->ncsrs) {
        struct kvm_csr_entry *e = &cpu->kvm_csr_buf->entries[ret];
        printf("error: failed to get CSR 0x%" PRIx32 "\n", (uint32_t)e->index);
    }

    for (i = 0; i < ret; i++) {
        uint32_t index = csrs[i].index;
        if (addr[index]) {
            *addr[index] = csrs[i].data;
        } else {
            printf("Failed to get addr CSR 0x%" PRIx32 "\n", i);
        }
    }

    ret |= KVM_GET_ONE_UREG64(cs, LOONGARCH_CSR_TVAL, &env->CSR_TVAL);
    ret |= KVM_GET_ONE_UREG64(cs, LOONGARCH_CSR_TCFG, &env->CSR_TCFG);
    return ret;
}

int kvm_loongarch_put_pvtime(LOONGARCHCPU *cpu)
{
    CPULOONGARCHState *env = &cpu->env;
    int err;
    struct kvm_device_attr attr = {
        .group = KVM_LARCH_VCPU_PVTIME_CTRL,
        .attr = KVM_LARCH_VCPU_PVTIME_IPA,
        .addr = (uint64_t)&env->st.guest_addr,
    };

    err = kvm_vcpu_ioctl(CPU(cpu), KVM_HAS_DEVICE_ATTR, attr);
    if (err != 0) {
        /* It's ok even though kvm has not such attr */
        return 0;
    }

    err = kvm_vcpu_ioctl(CPU(cpu), KVM_SET_DEVICE_ATTR, attr);
    if (err != 0) {
        error_report("PVTIME IPA: KVM_SET_DEVICE_ATTR: %s", strerror(-err));
        return err;
    }

    return 0;
}

int kvm_loongarch_get_pvtime(LOONGARCHCPU *cpu)
{
    CPULOONGARCHState *env = &cpu->env;
    int err;
    struct kvm_device_attr attr = {
        .group = KVM_LARCH_VCPU_PVTIME_CTRL,
        .attr = KVM_LARCH_VCPU_PVTIME_IPA,
        .addr = (uint64_t)&env->st.guest_addr,
    };

    err = kvm_vcpu_ioctl(CPU(cpu), KVM_HAS_DEVICE_ATTR, attr);
    if (err != 0) {
        /* It's ok even though kvm has not such attr */
        return 0;
    }

    err = kvm_vcpu_ioctl(CPU(cpu), KVM_GET_DEVICE_ATTR, attr);
    if (err != 0) {
        error_report("PVTIME IPA: KVM_GET_DEVICE_ATTR: %s", strerror(-err));
        return err;
    }

    return 0;
}


static int kvm_loongarch_put_lbt_registers(CPUState *cs)
{
    int ret = 0;
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;

    ret |= kvm_larch_putq(cs, KVM_REG_LBT_SCR0,  &env->lbt.scr0);
    ret |= kvm_larch_putq(cs, KVM_REG_LBT_SCR1,  &env->lbt.scr1);
    ret |= kvm_larch_putq(cs, KVM_REG_LBT_SCR2,  &env->lbt.scr2);
    ret |= kvm_larch_putq(cs, KVM_REG_LBT_SCR3,  &env->lbt.scr3);
    ret |= kvm_larch_putq(cs, KVM_REG_LBT_FLAGS, &env->lbt.eflag);
    ret |= kvm_larch_putq(cs, KVM_REG_LBT_FTOP,  &env->active_fpu.ftop);

    return ret;
}

static int kvm_loongarch_get_lbt_registers(CPUState *cs)
{
    int ret = 0;
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;

    ret |= kvm_larch_getq(cs, KVM_REG_LBT_SCR0,  &env->lbt.scr0);
    ret |= kvm_larch_getq(cs, KVM_REG_LBT_SCR1,  &env->lbt.scr1);
    ret |= kvm_larch_getq(cs, KVM_REG_LBT_SCR2,  &env->lbt.scr2);
    ret |= kvm_larch_getq(cs, KVM_REG_LBT_SCR3,  &env->lbt.scr3);
    ret |= kvm_larch_getq(cs, KVM_REG_LBT_FLAGS, &env->lbt.eflag);
    ret |= kvm_larch_getq(cs, KVM_REG_LBT_FTOP,  &env->active_fpu.ftop);

    return ret;
}

int kvm_arch_put_registers(CPUState *cs, int level)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    struct kvm_regs regs;
    int ret;
    int i;

    /* Set the registers based on QEMU's view of things */
    for (i = 0; i < 32; i++) {
        regs.gpr[i] = (int64_t)(target_long)env->active_tc.gpr[i];
    }

    regs.pc = (int64_t)(target_long)env->active_tc.PC;

    ret = kvm_vcpu_ioctl(cs, KVM_SET_REGS, &regs);

    if (ret < 0) {
        return ret;
    }

    ret = kvm_loongarch_put_csr_registers(cs, level);
    if (ret < 0) {
        return ret;
    }

    ret = kvm_loongarch_put_fpu_registers(cs, level);
    if (ret < 0) {
        return ret;
    }

    kvm_loongarch_put_lbt_registers(cs);
    return ret;
}

int kvm_arch_get_registers(CPUState *cs)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    int ret = 0;
    struct kvm_regs regs;
    int i;

    /* Get the current register set as KVM seems it */
    ret = kvm_vcpu_ioctl(cs, KVM_GET_REGS, &regs);

    if (ret < 0) {
        return ret;
    }

    for (i = 0; i < 32; i++) {
        env->active_tc.gpr[i] = regs.gpr[i];
    }

    env->active_tc.PC = regs.pc;

    kvm_loongarch_get_csr_registers(cs);
    kvm_loongarch_get_fpu_registers(cs);
    kvm_loongarch_get_lbt_registers(cs);

    return ret;
}

int kvm_arch_fixup_msi_route(struct kvm_irq_routing_entry *route,
                             uint64_t address, uint32_t data, PCIDevice *dev)
{
    return 0;
}

int kvm_arch_add_msi_route_post(struct kvm_irq_routing_entry *route,
                                int vector, PCIDevice *dev)
{
    return 0;
}

bool kvm_arch_cpu_check_are_resettable(void)
{
    return true;
}

int kvm_arch_release_virq_post(int virq)
{
    return 0;
}

int kvm_arch_msi_data_to_gsi(uint32_t data)
{
    abort();
}
void kvm_arch_accel_class_init(ObjectClass *oc)
{
}
