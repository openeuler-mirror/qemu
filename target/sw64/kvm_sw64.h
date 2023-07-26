/*
 * QEMU KVM support -- SW64 specific functions.
 *
 * Copyright (c) 2018 Lin Hainan
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_KVM_SW64_H
#define QEMU_KVM_SW64_H

#include "sysemu/kvm.h"
#include "exec/memory.h"
#include "qemu/error-report.h"

/**
 * kvm_sw64_vcpu_init:
 * @cs: CPUState
 *
 * Initialize (or reinitialize) the VCPU by invoking the
 * KVM_SW64_VCPU_INIT ioctl with the CPU type and feature
 * bitmask specified in the CPUState.
 *
 * Returns: 0 if success else < 0 error code
 */
int kvm_sw64_vcpu_init(CPUState *cs);
void kvm_sw64_reset_vcpu(SW64CPU *cpu);
void kvm_sw64_register_slave(SW64CPU *cpu);

#define TYPE_SW64_HOST_CPU "host-" TYPE_SW64_CPU
#define SW64_HOST_CPU_CLASS(klass) \
    OBJECT_CLASS_CHECK(SW64HostCPUClass, (klass), TYPE_SW64_HOST_CPU)
#define SW64_HOST_CPU_GET_CLASS(obj) \
    OBJECT_GET_CLASS(SW64HostCPUClass, (obj), TYPE_SW64_HOST_CPU)

typedef struct SW64HostCPUClass {
    /*< private >*/
    SW64CPUClass parent_class;
    /*< public >*/

    uint64_t features;
    uint32_t target;
    const char *dtb_compatible;
} SW64HostCPUClass;

/**
 * kvm_sw64_handle_debug:
 * @cs: CPUState
 * @debug_exit: debug part of the KVM exit structure
 *
 * Returns: TRUE if the debug exception was handled.
 */
bool kvm_sw64_handle_debug(CPUState *cs, struct kvm_debug_exit_arch *debug_exit);
#endif
