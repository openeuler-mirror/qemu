#ifndef __LINUX_KVM_SW64_H
#define __LINUX_KVM_SW64_H

#include <linux/types.h>

#define __KVM_HAVE_GUEST_DEBUG

/*
 * for KVM_GET_REGS and KVM_SET_REGS
 */
struct kvm_regs {
	unsigned long r0;
	unsigned long r1;
	unsigned long r2;
	unsigned long r3;

	unsigned long r4;
	unsigned long r5;
	unsigned long r6;
	unsigned long r7;

	unsigned long r8;
	unsigned long r9;
	unsigned long r10;
	unsigned long r11;

	unsigned long r12;
	unsigned long r13;
	unsigned long r14;
	unsigned long r15;

	unsigned long r19;
	unsigned long r20;
	unsigned long r21;
	unsigned long r22;

	unsigned long r23;
	unsigned long r24;
	unsigned long r25;
	unsigned long r26;

	unsigned long r27;
	unsigned long r28;
	unsigned long __padding0;
	unsigned long fpcr;

	unsigned long fp[124];
	/* These are saved by hmcode: */
	unsigned long ps;
	unsigned long pc;
	unsigned long gp;
	unsigned long r16;
	unsigned long r17;
	unsigned long r18;
};

struct vcpucb {
        unsigned long go_flag;
        unsigned long pcbb;
        unsigned long ksp;
        unsigned long usp;
        unsigned long kgp;
        unsigned long ent_arith;
        unsigned long ent_if;
        unsigned long ent_int;
        unsigned long ent_mm;
        unsigned long ent_sys;
        unsigned long ent_una;
        unsigned long stack_pc;
        unsigned long new_a0;
        unsigned long new_a1;
        unsigned long new_a2;
        unsigned long whami;
        unsigned long csr_save;
        unsigned long wakeup_magic;
        unsigned long host_vcpucb;
        unsigned long upcr;
        unsigned long vpcr;
        unsigned long dtb_pcr;
        unsigned long guest_ksp;
        unsigned long guest_usp;
        unsigned long vcpu_irq_disabled;
        unsigned long vcpu_irq;
        unsigned long ptbr;
        unsigned long int_stat0;
        unsigned long int_stat1;
        unsigned long int_stat2;
        unsigned long int_stat3;
        unsigned long reset_entry;
        unsigned long pvcpu;
        unsigned long exit_reason;
        unsigned long ipaddr;
        unsigned long vcpu_irq_vector;
        unsigned long pri_base;
        unsigned long stack_pc_dfault;
        unsigned long guest_p20;
        unsigned long guest_dfault_double;
        unsigned long guest_irqs_pending;
        unsigned long guest_hm_r30;
        unsigned long migration_mark;
        unsigned long guest_longtime;
        unsigned long guest_longtime_offset;
        unsigned long reserved[3];
};

/*
 * for KVM_GET_FPU and KVM_SET_FPU
 */
struct kvm_fpu {
};

/*
 * KVM SW_64 specific structures and definitions
 */
struct kvm_debug_exit_arch {
	unsigned long epc;
};

/* for KVM_SET_GUEST_DEBUG */
struct kvm_guest_debug_arch {
};

/* definition of registers in kvm_run */
struct kvm_sync_regs {
};

/* dummy definition */
struct kvm_sregs {
};

#define KVM_SW64_VCPU_INIT    _IO(KVMIO, 0xba)
#define KVM_SW64_USE_SLAVE    _IO(KVMIO, 0xbb)
#define KVM_SW64_GET_VCB      _IO(KVMIO, 0xbc)
#define KVM_SW64_SET_VCB      _IO(KVMIO, 0xbd)

#endif /* __LINUX_KVM_SW64_H */
