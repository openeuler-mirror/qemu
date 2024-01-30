/*
 * QEMU KVM stub
 *
 * Copyright Red Hat, Inc. 2010
 *
 * Author: Paolo Bonzini     <pbonzini@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "sysemu/kvm.h"
#include "hw/pci/msi.h"
#include "hw/ub/ub_usi.h"

KVMState *kvm_state;
bool kvm_kernel_irqchip;
bool kvm_async_interrupts_allowed;
bool kvm_resamplefds_allowed;
bool kvm_msi_via_irqfd_allowed;
bool kvm_gsi_routing_allowed;
bool kvm_gsi_direct_mapping;
bool kvm_allowed;
bool kvm_readonly_mem_allowed;
bool kvm_msi_use_devid;
bool kvm_usi_via_irqfd_allowed;

bool virtcca_cvm_allowed;

bool kvm_csv3_allowed;

void kvm_flush_coalesced_mmio_buffer(void)
{
}

void kvm_cpu_synchronize_state(CPUState *cpu)
{
}

bool kvm_has_sync_mmu(void)
{
    return false;
}

int kvm_on_sigbus_vcpu(CPUState *cpu, int code, void *addr)
{
    return 1;
}

int kvm_on_sigbus(int code, void *addr)
{
    return 1;
}

int kvm_irqchip_add_msi_route(KVMRouteChange *c, int vector, PCIDevice *dev)
{
    return -ENOSYS;
}

void kvm_init_irq_routing(KVMState *s)
{
}

void kvm_irqchip_release_virq(KVMState *s, int virq)
{
}

int kvm_irqchip_update_msi_route(KVMRouteChange *c, int virq, MSIMessage msg,
                                 PCIDevice *dev)
{
    return -ENOSYS;
}

void kvm_irqchip_commit_routes(KVMState *s)
{
}

void kvm_irqchip_add_change_notifier(Notifier *n)
{
}

void kvm_irqchip_remove_change_notifier(Notifier *n)
{
}

void kvm_irqchip_change_notify(void)
{
}

int kvm_irqchip_add_irqfd_notifier_gsi(KVMState *s, EventNotifier *n,
                                       EventNotifier *rn, int virq)
{
    return -ENOSYS;
}

int kvm_irqchip_remove_irqfd_notifier_gsi(KVMState *s, EventNotifier *n,
                                          int virq)
{
    return -ENOSYS;
}

unsigned int kvm_get_max_memslots(void)
{
    return 0;
}

unsigned int kvm_get_free_memslots(void)
{
    return 0;
}

void kvm_init_cpu_signals(CPUState *cpu)
{
    abort();
}

bool kvm_arm_supports_user_irq(void)
{
    return false;
}

bool kvm_arm_rme_enabled(void)
{
    return false;
}

hwaddr rme_mask_share_bit(hwaddr addr)
{
    return addr;
}

void kvm_update_hdbss_cap(bool enable, int hdbss_buffer_size)
{
    g_assert_not_reached();
}

bool kvm_dirty_ring_enabled(void)
{
    return false;
}

uint32_t kvm_dirty_ring_size(void)
{
    return 0;
}


bool kvm_hwpoisoned_mem(void)
{
    return false;
}
int kvm_load_user_data(hwaddr loader_start, hwaddr image_end, hwaddr initrd_start, hwaddr dtb_end, hwaddr ram_size,
                       struct kvm_numa_info *numa_info)
{
    return -ENOSYS;
}

#ifdef CONFIG_UB
int kvm_irqchip_add_usi_route(KVMRouteChange *c, USIMessage msg, uint32_t devid, UBDevice *udev)
{
    return -ENOSYS;
}

int kvm_irqchip_update_usi_route(KVMRouteChange *c, int virq, USIMessage msg, UBDevice *udev)
{
    return -ENOSYS;
}
#endif // CONFIG_UB

#ifdef CONFIG_HUGEPAGE_POD
int kvm_update_touched_log(void)
{
    return -ENOSYS;
}
int kvm_clear_slot_dirty_bitmap(void *ram)
{
    return -ENOSYS;
}
#endif
