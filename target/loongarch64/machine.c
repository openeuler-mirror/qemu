/*
 * Loongarch 3A5000 machine emulation
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
#include "qemu-common.h"
#include "cpu.h"
#include "internal.h"
#include "hw/hw.h"
#include "kvm_larch.h"
#include "migration/cpu.h"
#include "linux/kvm.h"
#include "sysemu/kvm.h"
#include "qemu/error-report.h"

static int cpu_post_load(void *opaque, int version_id)
{
    LOONGARCHCPU *cpu = opaque;
    CPULOONGARCHState *env = &cpu->env;
    int r = 0;

    if (!kvm_enabled()) {
        return 0;
    }

#ifdef CONFIG_KVM
    struct kvm_loongarch_vcpu_state vcpu_state;
    int i;

    vcpu_state.online_vcpus = cpu->online_vcpus;
    vcpu_state.is_migrate = cpu->is_migrate;
    vcpu_state.cpu_freq = cpu->cpu_freq;
    vcpu_state.count_ctl = cpu->count_ctl;
    vcpu_state.pending_exceptions = cpu->pending_exceptions;
    vcpu_state.pending_exceptions_clr = cpu->pending_exceptions_clr;
    for (i = 0; i < 4; i++) {
        vcpu_state.core_ext_ioisr[i] = cpu->core_ext_ioisr[i];
    }
    r = kvm_vcpu_ioctl(CPU(cpu), KVM_LARCH_SET_VCPU_STATE, &vcpu_state);
    if (r) {
        error_report("set vcpu state failed %d", r);
    }

    kvm_loongarch_put_pvtime(cpu);
#endif

    restore_fp_status(env);
    compute_hflags(env);

    return r;
}

static int cpu_pre_save(void *opaque)
{
#ifdef CONFIG_KVM
    LOONGARCHCPU *cpu = opaque;
    struct kvm_loongarch_vcpu_state vcpu_state;
    int i, r = 0;
    if (!kvm_enabled()) {
        return 0;
    }

    r = kvm_vcpu_ioctl(CPU(cpu), KVM_LARCH_GET_VCPU_STATE, &vcpu_state);
    if (r < 0) {
        error_report("get vcpu state failed %d", r);
        return r;
    }

    cpu->online_vcpus = vcpu_state.online_vcpus;
    cpu->is_migrate = vcpu_state.is_migrate;
    cpu->cpu_freq = vcpu_state.cpu_freq;
    cpu->count_ctl = vcpu_state.count_ctl;
    cpu->pending_exceptions = vcpu_state.pending_exceptions;
    cpu->pending_exceptions_clr = vcpu_state.pending_exceptions_clr;
    for (i = 0; i < 4; i++) {
        cpu->core_ext_ioisr[i] = vcpu_state.core_ext_ioisr[i];
    }

    kvm_loongarch_get_pvtime(cpu);
#endif
    return 0;
}

/* FPU state */

static int get_fpr(QEMUFile *f, void *pv, size_t size,
                   const VMStateField *field)
{
    fpr_t *v = pv;
    qemu_get_be64s(f, &v->d);
    return 0;
}

static int put_fpr(QEMUFile *f, void *pv, size_t size,
                   const VMStateField *field, JSONWriter *vmdesc)
{
    fpr_t *v = pv;
    qemu_put_be64s(f, &v->d);
    return 0;
}

const VMStateInfo vmstate_info_fpr = {
    .name = "fpr",
    .get = get_fpr,
    .put = put_fpr,
};

#define VMSTATE_FPR_ARRAY_V(_f, _s, _n, _v)                                   \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_fpr, fpr_t)

#define VMSTATE_FPR_ARRAY(_f, _s, _n) VMSTATE_FPR_ARRAY_V(_f, _s, _n, 0)

static VMStateField vmstate_fpu_fields[] = {
    VMSTATE_FPR_ARRAY(fpr, CPULOONGARCHFPUContext, 32),
    VMSTATE_UINT32(fcsr0, CPULOONGARCHFPUContext), VMSTATE_END_OF_LIST()
};

const VMStateDescription vmstate_fpu = { .name = "cpu/fpu",
                                         .version_id = 1,
                                         .minimum_version_id = 1,
                                         .fields = vmstate_fpu_fields };

const VMStateDescription vmstate_inactive_fpu = { .name = "cpu/inactive_fpu",
                                                  .version_id = 1,
                                                  .minimum_version_id = 1,
                                                  .fields =
                                                      vmstate_fpu_fields };

/* TC state */

static VMStateField vmstate_tc_fields[] = {
    VMSTATE_UINTTL_ARRAY(gpr, TCState, 32), VMSTATE_UINTTL(PC, TCState),
    VMSTATE_END_OF_LIST()
};

const VMStateDescription vmstate_tc = { .name = "cpu/tc",
                                        .version_id = 1,
                                        .minimum_version_id = 1,
                                        .fields = vmstate_tc_fields };

const VMStateDescription vmstate_inactive_tc = { .name = "cpu/inactive_tc",
                                                 .version_id = 1,
                                                 .minimum_version_id = 1,
                                                 .fields = vmstate_tc_fields };

/* TLB state */

static int get_tlb(QEMUFile *f, void *pv, size_t size,
                   const VMStateField *field)
{
    ls3a5k_tlb_t *v = pv;
    uint32_t flags;

    qemu_get_betls(f, &v->VPN);
    qemu_get_be64s(f, &v->PageMask);
    qemu_get_be32s(f, &v->PageSize);
    qemu_get_be16s(f, &v->ASID);
    qemu_get_be32s(f, &flags);
    v->RPLV1 = (flags >> 21) & 1;
    v->RPLV0 = (flags >> 20) & 1;
    v->PLV1 = (flags >> 18) & 3;
    v->PLV0 = (flags >> 16) & 3;
    v->EHINV = (flags >> 15) & 1;
    v->RI1 = (flags >> 14) & 1;
    v->RI0 = (flags >> 13) & 1;
    v->XI1 = (flags >> 12) & 1;
    v->XI0 = (flags >> 11) & 1;
    v->WE1 = (flags >> 10) & 1;
    v->WE0 = (flags >> 9) & 1;
    v->V1 = (flags >> 8) & 1;
    v->V0 = (flags >> 7) & 1;
    v->C1 = (flags >> 4) & 7;
    v->C0 = (flags >> 1) & 7;
    v->G = (flags >> 0) & 1;
    qemu_get_be64s(f, &v->PPN0);
    qemu_get_be64s(f, &v->PPN1);

    return 0;
}

static int put_tlb(QEMUFile *f, void *pv, size_t size,
                   const VMStateField *field, JSONWriter *vmdesc)
{
    ls3a5k_tlb_t *v = pv;

    uint16_t asid = v->ASID;
    uint32_t flags =
        ((v->RPLV1 << 21) | (v->RPLV0 << 20) | (v->PLV1 << 18) |
         (v->PLV0 << 16) | (v->EHINV << 15) | (v->RI1 << 14) | (v->RI0 << 13) |
         (v->XI1 << 12) | (v->XI0 << 11) | (v->WE1 << 10) | (v->WE0 << 9) |
         (v->V1 << 8) | (v->V0 << 7) | (v->C1 << 4) | (v->C0 << 1) |
         (v->G << 0));

    qemu_put_betls(f, &v->VPN);
    qemu_put_be64s(f, &v->PageMask);
    qemu_put_be32s(f, &v->PageSize);
    qemu_put_be16s(f, &asid);
    qemu_put_be32s(f, &flags);
    qemu_put_be64s(f, &v->PPN0);
    qemu_put_be64s(f, &v->PPN1);

    return 0;
}

const VMStateInfo vmstate_info_tlb = {
    .name = "tlb_entry",
    .get = get_tlb,
    .put = put_tlb,
};

#define VMSTATE_TLB_ARRAY_V(_f, _s, _n, _v)                                   \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_tlb, ls3a5k_tlb_t)

#define VMSTATE_TLB_ARRAY(_f, _s, _n) VMSTATE_TLB_ARRAY_V(_f, _s, _n, 0)

const VMStateDescription vmstate_tlb = {
    .name = "cpu/tlb",
    .version_id = 2,
    .minimum_version_id = 2,
    .fields =
        (VMStateField[]){ VMSTATE_UINT32(nb_tlb, CPULOONGARCHTLBContext),
                          VMSTATE_UINT32(tlb_in_use, CPULOONGARCHTLBContext),
                          VMSTATE_TLB_ARRAY(mmu.ls3a5k.tlb,
                                            CPULOONGARCHTLBContext,
                                            LOONGARCH_TLB_MAX),
                          VMSTATE_END_OF_LIST() }
};

/* LOONGARCH CPU state */

const VMStateDescription vmstate_loongarch_cpu = {
    .name = "cpu",
    .version_id = 15,
    .minimum_version_id = 15,
    .post_load = cpu_post_load,
    .pre_save = cpu_pre_save,
    .fields =
        (VMStateField[]){
            /* Active TC */
            VMSTATE_STRUCT(env.active_tc, LOONGARCHCPU, 1, vmstate_tc,
                           TCState),

            /* Active FPU */
            VMSTATE_STRUCT(env.active_fpu, LOONGARCHCPU, 1, vmstate_fpu,
                           CPULOONGARCHFPUContext),

            /* TLB */
            VMSTATE_STRUCT_POINTER(env.tlb, LOONGARCHCPU, vmstate_tlb,
                                   CPULOONGARCHTLBContext),
            /* CPU metastate */
            VMSTATE_UINT32(env.current_tc, LOONGARCHCPU),
            VMSTATE_INT32(env.error_code, LOONGARCHCPU),
            VMSTATE_UINTTL(env.btarget, LOONGARCHCPU),
            VMSTATE_UINTTL(env.bcond, LOONGARCHCPU),

            VMSTATE_UINT64(env.lladdr, LOONGARCHCPU),

            /* PV time */
            VMSTATE_UINT64(env.st.guest_addr, LOONGARCHCPU),

            /* Remaining CSR registers */
            VMSTATE_UINT64(env.CSR_CRMD, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PRMD, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_EUEN, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_MISC, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ECFG, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ESTAT, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ERA, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_BADV, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_BADI, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_EEPN, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBIDX, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBEHI, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBELO0, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBELO1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBWIRED, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_GTLBC, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TRGP, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PGDL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PGDH, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PGD, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PWCTL0, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PWCTL1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_STLBPGSIZE, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_RVACFG, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_CPUID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PRCFG1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PRCFG2, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PRCFG3, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_KS0, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_KS1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_KS2, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_KS3, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_KS4, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_KS5, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_KS6, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_KS7, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TMID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TCFG, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TVAL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_CNTC, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TINTCLR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_GSTAT, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_GCFG, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_GINTC, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_GCNTC, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_LLBCTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IMPCTL1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IMPCTL2, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_GNMI, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBRENT, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBRBADV, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBRERA, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBRSAVE, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBRELO0, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBRELO1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBREHI, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_TLBRPRMD, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ERRCTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ERRINFO, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ERRINFO1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ERRENT, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ERRERA, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_ERRSAVE, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_CTAG, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DMWIN0, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DMWIN1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DMWIN2, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DMWIN3, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PERFCTRL0, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PERFCNTR0, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PERFCTRL1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PERFCNTR1, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PERFCTRL2, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PERFCNTR2, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PERFCTRL3, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_PERFCNTR3, LOONGARCHCPU),
            /* debug */
            VMSTATE_UINT64(env.CSR_MWPC, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_MWPS, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB0ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB0MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB0CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB0ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB1ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB1MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB1CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB1ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB2ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB2MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB2CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB2ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB3ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB3MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB3CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DB3ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_FWPC, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_FWPS, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB0ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB0MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB0CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB0ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB1ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB1MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB1CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB1ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB2ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB2MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB2CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB2ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB3ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB3MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB3CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB3ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB4ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB4MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB4CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB4ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB5ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB5MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB5CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB5ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB6ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB6MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB6CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB6ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB7ADDR, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB7MASK, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB7CTL, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_IB7ASID, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DEBUG, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DERA, LOONGARCHCPU),
            VMSTATE_UINT64(env.CSR_DESAVE, LOONGARCHCPU),

            VMSTATE_STRUCT_ARRAY(env.fpus, LOONGARCHCPU, LOONGARCH_FPU_MAX, 1,
                                 vmstate_inactive_fpu, CPULOONGARCHFPUContext),
            VMSTATE_UINT8(online_vcpus, LOONGARCHCPU),
            VMSTATE_UINT8(is_migrate, LOONGARCHCPU),
            VMSTATE_UINT64(counter_value, LOONGARCHCPU),
            VMSTATE_UINT32(cpu_freq, LOONGARCHCPU),
            VMSTATE_UINT32(count_ctl, LOONGARCHCPU),
            VMSTATE_UINT64(pending_exceptions, LOONGARCHCPU),
            VMSTATE_UINT64(pending_exceptions_clr, LOONGARCHCPU),
            VMSTATE_UINT64_ARRAY(core_ext_ioisr, LOONGARCHCPU, 4),

            VMSTATE_END_OF_LIST() },
};
