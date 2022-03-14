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

#define  init_pc  0xffffffff80011000
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

    regs = (struct kvm_regs *)cpu->k_regs;
    regs->pc = init_pc;

    ret = kvm_vcpu_ioctl(cs, KVM_SET_REGS, &regs);

    if (ret < 0) {
        fprintf(stderr, "kvm_sw64_vcpu_init failed: %s\n", strerror(-ret));
        abort();
    }

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
    int ret;
    SW64CPU *cpu = SW64_CPU(cs);
    ret = kvm_vcpu_ioctl(cs, KVM_GET_REGS, &cpu->k_regs);
    if (ret < 0)
        return ret;
    return kvm_vcpu_ioctl(cs, KVM_SW64_GET_VCB, &cpu->k_vcb);
}

int kvm_arch_put_registers(CPUState *cs, int level)
{
    int ret;
    SW64CPU *cpu = SW64_CPU(cs);
    struct vcpucb *vcb;
    ret = kvm_vcpu_ioctl(cs, KVM_SET_REGS, &cpu->k_regs);
    if (ret < 0)
        return ret;
    vcb = (struct vcpucb *)cpu->k_vcb;
    vcb->whami = kvm_arch_vcpu_id(cs);
    fprintf(stderr,"vcpu %ld init.\n", vcb->whami);
    return kvm_vcpu_ioctl(cs, KVM_SW64_SET_VCB, &cpu->k_vcb);
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


int kvm_arch_handle_exit(CPUState *cs, struct kvm_run *run)
{
    return -1;
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
