/*
 * SW64 implementation of KVM hooks
 *
 * Copyright (c) 2018 Lin Hainan
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include <sys/ioctl.h>

#include <linux/kvm.h>

#include "qemu-common.h"
#include "qemu/timer.h"
#include "qemu/error-report.h"
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "kvm_sw64.h"
#include "cpu.h"
#include "exec/memattrs.h"
#include "exec/address-spaces.h"
#include "hw/boards.h"
#include "qemu/log.h"

const KVMCapabilityInfo kvm_arch_required_capabilities[] = {
    KVM_CAP_LAST_INFO
};
/* 50000 jump to bootlader while 2f00000 jump to bios*/
int kvm_sw64_vcpu_init(CPUState *cs)
{
    struct kvm_regs *regs;
    SW64CPU *cpu = SW64_CPU(cs);
    regs = (struct kvm_regs *)cpu->k_regs;
    regs->pc = init_pc;
    return kvm_vcpu_ioctl(cs, KVM_SET_REGS, &regs);
}

static void kvm_sw64_host_cpu_class_init(ObjectClass *oc, void *data)
{
}

static void kvm_sw64_host_cpu_initfn(Object *obj)
{
}


static const TypeInfo host_sw64_cpu_type_info = {
    .name = TYPE_SW64_HOST_CPU,
    .parent = TYPE_SW64_CPU,
    .instance_init = kvm_sw64_host_cpu_initfn,
    .class_init = kvm_sw64_host_cpu_class_init,
    .class_size = sizeof(SW64HostCPUClass),
};

int kvm_arch_init(MachineState *ms, KVMState *s)
{
    kvm_async_interrupts_allowed = true;

    type_register_static(&host_sw64_cpu_type_info);

    return 0;
}

/* 50000 jump to bootlader while 2f00000 jump to bios*/
void kvm_sw64_reset_vcpu(SW64CPU *cpu)
{
    CPUState *cs = CPU(cpu);
    struct kvm_regs *regs;
    int ret;
    struct vcpucb *vcb;

    regs = (struct kvm_regs *)cpu->k_regs;
    regs->pc = init_pc;

    ret = kvm_vcpu_ioctl(cs, KVM_SET_REGS, &regs);

    if (ret < 0) {
        fprintf(stderr, "kvm_sw64_vcpu_init failed: %s\n", strerror(-ret));
        abort();
    }

    vcb = (struct vcpucb *)cpu->k_vcb;
    vcb->vcpu_irq_disabled = 1;

    ret = kvm_vcpu_ioctl(cs, KVM_SW64_VCPU_INIT, NULL);

    if (ret < 0) {
        fprintf(stderr, "kvm_sw64_vcpu_init failed: %s\n", strerror(-ret));
        abort();
    }
}

unsigned long kvm_arch_vcpu_id(CPUState *cpu)
{
    return cpu->cpu_index;
}

#include <pthread.h>
int kvm_arch_init_vcpu(CPUState *cs)
{
    int ret;
    ret = kvm_sw64_vcpu_init(cs);
    if (ret) {
        return ret;
    }
    return 0;
}

int kvm_arch_destroy_vcpu(CPUState *cs)
{
    return 0;
}

int kvm_arch_get_registers(CPUState *cs)
{
    int ret, i;
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;

    ret = kvm_vcpu_ioctl(cs, KVM_GET_REGS, &cpu->k_regs);
    if (ret < 0)
        return ret;

    ret = kvm_vcpu_ioctl(cs, KVM_SW64_GET_VCB, &cpu->k_vcb);
    if (ret < 0)
	return ret;

    for (i = 0; i < 16; i++)
	env->ir[i] = cpu->k_regs[i];

    env->ir[16] = cpu->k_regs[155];
    env->ir[17] = cpu->k_regs[156];
    env->ir[18] = cpu->k_regs[157];

    for (i = 19; i < 29; i++)
	env->ir[i] = cpu->k_regs[i-3];

    env->ir[29] = cpu->k_regs[154];

    if (cpu->k_regs[152] >> 3)
	env->ir[30] = cpu->k_vcb[3];	/* usp */
    else
	env->ir[30] = cpu->k_vcb[2];	/* ksp */

    env->pc = cpu->k_regs[153];

    return 0;
}

int kvm_arch_put_registers(CPUState *cs, int level)
{
    int ret;
    SW64CPU *cpu = SW64_CPU(cs);
    struct vcpucb *vcb;

    if (level == KVM_PUT_RUNTIME_STATE) {
	int i;
	CPUSW64State *env = &cpu->env;

	for (i = 0; i < 16; i++)
	    cpu->k_regs[i] = env->ir[i];

	for (i = 19; i < 29; i++)
	    cpu->k_regs[i-3] = env->ir[i];

	cpu->k_regs[155] = env->ir[16];
	cpu->k_regs[156] = env->ir[17];
	cpu->k_regs[157] = env->ir[18];

	cpu->k_regs[154] = env->ir[29];

	if (cpu->k_regs[152] >> 3)
	    cpu->k_vcb[3] = env->ir[30];	/* usp */
	else
	    cpu->k_vcb[2] = env->ir[30];	/* ksp */

	cpu->k_regs[153] = env->pc;
    }

    ret = kvm_vcpu_ioctl(cs, KVM_SET_REGS, &cpu->k_regs);
    if (ret < 0)
        return ret;
    vcb = (struct vcpucb *)cpu->k_vcb;
    vcb->whami = kvm_arch_vcpu_id(cs);
    fprintf(stderr,"vcpu %ld init.\n", vcb->whami);

    if (level == KVM_PUT_RESET_STATE)
	vcb->pcbb = 0;

    return kvm_vcpu_ioctl(cs, KVM_SW64_SET_VCB, &cpu->k_vcb);
}

static const uint32_t brk_insn = 0x00000080;

int kvm_arch_insert_sw_breakpoint(CPUState *cs, struct kvm_sw_breakpoint *bp)
{
    if (cpu_memory_rw_debug(cs, bp->pc, (uint8_t *)&bp->saved_insn, 4, 0) ||
	cpu_memory_rw_debug(cs, bp->pc, (uint8_t *)&brk_insn, 4, 1)) {
	return -EINVAL;
    }

    return 0;
}

int kvm_arch_remove_sw_breakpoint(CPUState *cs, struct kvm_sw_breakpoint *bp)
{
    static uint32_t brk;

    if (cpu_memory_rw_debug(cs, bp->pc, (uint8_t *)&brk, 4, 0) ||
	brk != brk_insn ||
	cpu_memory_rw_debug(cs, bp->pc, (uint8_t *)&bp->saved_insn, 4, 1)) {
	return -EINVAL;
    }

    return 0;
}

int kvm_arch_insert_hw_breakpoint(target_ulong addr,
				  target_ulong len, int type)
{
    qemu_log_mask(LOG_UNIMP, "%s: not implemented\n", __func__);
    return -EINVAL;
}

int kvm_arch_remove_hw_breakpoint(target_ulong addr,
				  target_ulong len, int type)
{
    qemu_log_mask(LOG_UNIMP, "%s: not implemented\n", __func__);
    return -EINVAL;
}

void kvm_arch_remove_all_hw_breakpoints(void)
{
    qemu_log_mask(LOG_UNIMP, "%s: not implemented\n", __func__);
}

int kvm_arch_add_msi_route_post(struct kvm_irq_routing_entry *route,
                                int vector, PCIDevice *dev)
{
    return -1;
}

int kvm_arch_fixup_msi_route(struct kvm_irq_routing_entry *route,
                             uint64_t address, uint32_t data, PCIDevice *dev)
{
    return 0;
}

void kvm_arch_pre_run(CPUState *cs, struct kvm_run *run)
{
}

MemTxAttrs kvm_arch_post_run(CPUState *cs, struct kvm_run *run)
{
    return MEMTXATTRS_UNSPECIFIED;
}

bool kvm_sw64_handle_debug(CPUState *cs, struct kvm_debug_exit_arch *debug_exit)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;

    /* Ensure PC is synchronised */
    kvm_cpu_synchronize_state(cs);

    if (cs->singlestep_enabled) {
	return true;
    } else if (kvm_find_sw_breakpoint(cs, debug_exit->epc)) {
	return true;
    } else {
	error_report("%s: unhandled debug exit (%"PRIx64", %"PRIx64")",
		     __func__, env->pc, debug_exit->epc);
    }

    return false;
}

int kvm_arch_handle_exit(CPUState *cs, struct kvm_run *run)
{
    int ret = 0;

    switch (run->exit_reason) {
    case KVM_EXIT_DEBUG:
	if (kvm_sw64_handle_debug(cs, &run->debug.arch)) {
	    ret = EXCP_DEBUG;
	} /* otherwise return to guest */
	break;
    default:
	qemu_log_mask(LOG_UNIMP, "%s: un-handled exit reason %d\n",
		      __func__, run->exit_reason);
	break;
    }
    return ret;
}

bool kvm_arch_stop_on_emulation_error(CPUState *cs)
{
    return true;
}

int kvm_arch_process_async_events(CPUState *cs)
{
    return 0;
}

void kvm_arch_update_guest_debug(CPUState *cs, struct kvm_guest_debug *dbg)
{
}

void kvm_arch_init_irq_routing(KVMState *s)
{
    /* We know at this point that we're using the in-kernel
     * irqchip, so we can use irqfds, and on x86 we know
     * we can use msi via irqfd and GSI routing.
     */
    kvm_msi_via_irqfd_allowed = true;
    kvm_gsi_routing_allowed = true;
}

int kvm_arch_irqchip_create(KVMState *s)
{
    return 0;
}

int kvm_arch_release_virq_post(int virq)
{
    return -1;
}

int kvm_arch_msi_data_to_gsi(uint32_t data)
{
    return -1;
}


void kvm_sw64_register_slave(SW64CPU *cpu)
{
    CPUState *cs = CPU(cpu);

    kvm_vcpu_ioctl(cs, KVM_SW64_USE_SLAVE, NULL);
}

bool kvm_arch_cpu_check_are_resettable(void)
{
    return true;
}

void kvm_arch_accel_class_init(ObjectClass *oc)
{
}
