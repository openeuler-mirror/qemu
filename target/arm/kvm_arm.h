/*
 * QEMU KVM support -- ARM specific functions.
 *
 * Copyright (c) 2012 Linaro Limited
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_KVM_ARM_H
#define QEMU_KVM_ARM_H

#include "sysemu/kvm.h"
#include "exec/memory.h"
#include "qemu/error-report.h"

#define KVM_ARM_VGIC_V2   (1 << 0)
#define KVM_ARM_VGIC_V3   (1 << 1)

#define KVM_REG_ARM_ID_AA64DFR0_EL1     ARM64_SYS_REG(3, 0, 0, 5, 0)
#define KVM_REG_ARM_PMCR_EL0            ARM64_SYS_REG(3, 3, 9, 12, 0)

#define MAX_TARGET_IMPL_CPUS    8
typedef struct TargetImplCpu {
     uint32_t midr;
     uint32_t revidr;
} TargetImplCpu;

/**
 * kvm_arm_init_debug() - initialize guest debug capabilities
 * @s: KVMState
 *
 * Should be called only once before using guest debug capabilities.
 */
void kvm_arm_init_debug(KVMState *s);

/**
 * kvm_arm_vcpu_init:
 * @cs: CPUState
 *
 * Initialize (or reinitialize) the VCPU by invoking the
 * KVM_ARM_VCPU_INIT ioctl with the CPU type and feature
 * bitmask specified in the CPUState.
 *
 * Returns: 0 if success else < 0 error code
 */
int kvm_arm_vcpu_init(CPUState *cs);

/**
 * kvm_arm_register_device:
 * @mr: memory region for this device
 * @devid: the KVM device ID
 * @group: device control API group for setting addresses
 * @attr: device control API address type
 * @dev_fd: device control device file descriptor (or -1 if not supported)
 * @addr_ormask: value to be OR'ed with resolved address
 *
 * Remember the memory region @mr, and when it is mapped by the
 * machine model, tell the kernel that base address using the
 * KVM_ARM_SET_DEVICE_ADDRESS ioctl or the newer device control API.  @devid
 * should be the ID of the device as defined by KVM_ARM_SET_DEVICE_ADDRESS or
 * the arm-vgic device in the device control API.
 * The machine model may map
 * and unmap the device multiple times; the kernel will only be told the final
 * address at the point where machine init is complete.
 */
void kvm_arm_register_device(MemoryRegion *mr, uint64_t devid, uint64_t group,
                             uint64_t attr, int dev_fd, uint64_t addr_ormask);

void virtcca_kvm_get_mmio_addr(hwaddr *mmio_start, hwaddr *mmio_size);

/**
 * kvm_arm_init_cpreg_list:
 * @cpu: ARMCPU
 *
 * Initialize the ARMCPU cpreg list according to the kernel's
 * definition of what CPU registers it knows about (and throw away
 * the previous TCG-created cpreg list).
 *
 * Returns: 0 if success, else < 0 error code
 */
int kvm_arm_init_cpreg_list(ARMCPU *cpu);

/**
 * kvm_arm_reg_syncs_via_cpreg_list:
 * @regidx: KVM register index
 *
 * Return true if this KVM register should be synchronized via the
 * cpreg list of arbitrary system registers, false if it is synchronized
 * by hand using code in kvm_arch_get/put_registers().
 */
bool kvm_arm_reg_syncs_via_cpreg_list(uint64_t regidx);

/**
 * kvm_arm_cpreg_level:
 * @regidx: KVM register index
 *
 * Return the level of this coprocessor/system register.  Return value is
 * either KVM_PUT_RUNTIME_STATE, KVM_PUT_RESET_STATE, or KVM_PUT_FULL_STATE.
 */
int kvm_arm_cpreg_level(uint64_t regidx);

/**
 * write_list_to_kvmstate:
 * @cpu: ARMCPU
 * @level: the state level to sync
 *
 * For each register listed in the ARMCPU cpreg_indexes list, write
 * its value from the cpreg_values list into the kernel (via ioctl).
 * This updates KVM's working data structures from TCG data or
 * from incoming migration state.
 *
 * Returns: true if all register values were updated correctly,
 * false if some register was unknown to the kernel or could not
 * be written (eg constant register with the wrong value).
 * Note that we do not stop early on failure -- we will attempt
 * writing all registers in the list.
 */
bool write_list_to_kvmstate(ARMCPU *cpu, int level);

/**
 * write_kvmstate_to_list:
 * @cpu: ARMCPU
 *
 * For each register listed in the ARMCPU cpreg_indexes list, write
 * its value from the kernel into the cpreg_values list. This is used to
 * copy info from KVM's working data structures into TCG or
 * for outbound migration.
 *
 * Returns: true if all register values were read correctly,
 * false if some register was unknown or could not be read.
 * Note that we do not stop early on failure -- we will attempt
 * reading all registers in the list.
 */
bool write_kvmstate_to_list(ARMCPU *cpu);

/**
 * kvm_arm_cpu_pre_save:
 * @cpu: ARMCPU
 *
 * Called after write_kvmstate_to_list() from cpu_pre_save() to update
 * the cpreg list with KVM CPU state.
 */
void kvm_arm_cpu_pre_save(ARMCPU *cpu);

/**
 * kvm_arm_cpu_post_load:
 * @cpu: ARMCPU
 *
 * Called from cpu_post_load() to update KVM CPU state from the cpreg list.
 */
void kvm_arm_cpu_post_load(ARMCPU *cpu);

/**
 * kvm_arm_reset_vcpu:
 * @cpu: ARMCPU
 *
 * Called at reset time to kernel registers to their initial values.
 */
void kvm_arm_reset_vcpu(ARMCPU *cpu);

/**
 * kvm_arm_create_host_vcpu:
 * @cpu: ARMCPU
 *
 * Called at to pre create all possible kvm vCPUs within the the host at the
 * virt machine init time. This will also init this pre-created vCPU and
 * hence result in vCPU reset at host. These pre created and inited vCPUs
 * shall be parked for use when ARM vCPUs are actually realized.
 */
void kvm_arm_create_host_vcpu(ARMCPU *cpu);

/**
 * kvm_arm_init_serror_injection:
 * @cs: CPUState
 *
 * Check whether KVM can set guest SError syndrome.
 */
void kvm_arm_init_serror_injection(CPUState *cs);

/**
 * kvm_get_vcpu_events:
 * @cpu: ARMCPU
 *
 * Get VCPU related state from kvm.
 *
 * Returns: 0 if success else < 0 error code
 */
int kvm_get_vcpu_events(ARMCPU *cpu);

/**
 * kvm_put_vcpu_events:
 * @cpu: ARMCPU
 *
 * Put VCPU related state to kvm.
 *
 * Returns: 0 if success else < 0 error code
 */
int kvm_put_vcpu_events(ARMCPU *cpu);

#ifdef CONFIG_KVM
/**
 * kvm_arm_create_scratch_host_vcpu:
 * @cpus_to_try: array of QEMU_KVM_ARM_TARGET_* values (terminated with
 * QEMU_KVM_ARM_TARGET_NONE) to try as fallback if the kernel does not
 * know the PREFERRED_TARGET ioctl. Passing NULL is the same as passing
 * an empty array.
 * @fdarray: filled in with kvmfd, vmfd, cpufd file descriptors in that order
 * @init: filled in with the necessary values for creating a host
 * vcpu. If NULL is provided, will not init the vCPU (though the cpufd
 * will still be set up).
 *
 * Create a scratch vcpu in its own VM of the type preferred by the host
 * kernel (as would be used for '-cpu host'), for purposes of probing it
 * for capabilities.
 *
 * Returns: true on success (and fdarray and init are filled in),
 * false on failure (and fdarray and init are not valid).
 */
bool kvm_arm_create_scratch_host_vcpu(const uint32_t *cpus_to_try,
                                      int *fdarray,
                                      struct kvm_vcpu_init *init);

/**
 * kvm_arm_destroy_scratch_host_vcpu:
 * @fdarray: array of fds as set up by kvm_arm_create_scratch_host_vcpu
 *
 * Tear down the scratch vcpu created by kvm_arm_create_scratch_host_vcpu.
 */
void kvm_arm_destroy_scratch_host_vcpu(int *fdarray);

/**
 * ARMHostCPUFeatures: information about the host CPU (identified
 * by asking the host kernel)
 */
typedef struct ARMHostCPUFeatures {
    ARMISARegisters isar;
    uint64_t features;
    uint32_t target;
    const char *dtb_compatible;
} ARMHostCPUFeatures;

/**
 * kvm_arm_get_host_cpu_features:
 * @cpu: cpu object
 * @ahcf: ARMHostCPUClass to fill in
 * @exhaustive: if true, all the feature ID regs are queried instead of
 *              a subset
 
 * Probe the capabilities of the host kernel's preferred CPU and fill
 * in the ARMHostCPUClass struct accordingly.
 *
 * Returns true on success and false otherwise.
 */
bool kvm_arm_get_host_cpu_features(ARMCPU *cpu, ARMHostCPUFeatures *ahcf,
                              bool exhaustive);

/**
 * kvm_arm_sve_get_vls:
 * @cs: CPUState
 *
 * Get all the SVE vector lengths supported by the KVM host, setting
 * the bits corresponding to their length in quadwords minus one
 * (vq - 1) up to ARM_MAX_VQ.  Return the resulting map.
 */
uint32_t kvm_arm_sve_get_vls(CPUState *cs);

/**
 * kvm_arm_set_cpu_features_from_host:
 * @cpu: ARMCPU to set the features for
 *
 * Set up the ARMCPU struct fields up to match the information probed
 * from the host CPU.
 *
 * @cpu: cpu object
 * @exhaustive: if true, all the feature ID regs are queried instead of
 *              a subset
 */
void kvm_arm_set_cpu_features_from_host(ARMCPU *cpu, bool exhaustive);

/**
 * kvm_arm_add_vcpu_properties:
 * @obj: The CPU object to add the properties to
 *
 * Add all KVM specific CPU properties to the CPU object. These
 * are the CPU properties with "kvm-" prefixed names.
 */
void kvm_arm_add_vcpu_properties(Object *obj);

typedef struct ARM64SysReg ARM64SysReg;
/**
 * kvm_arm_expose_idreg_properties:
 * @cpu: The CPU object to generate the properties for
 * @reg: registers from the host
 *
 * analyze the writable mask and generate properties for writable fields
 */
void kvm_arm_expose_idreg_properties(ARMCPU *cpu, ARM64SysReg *regs);

/**
 * @cs: CPUState
 * @feature: a KVM_ARM_VCPU_* feature
 *
 * Finalize the configuration of the given vcpu feature.
 */
int kvm_arm_vcpu_finalize(CPUState *cs, int feature);

/**
 * kvm_arm_steal_time_finalize:
 * @cpu: ARMCPU for which to finalize kvm-steal-time
 * @errp: Pointer to Error* for error propagation
 *
 * Validate the kvm-steal-time property selection and set its default
 * based on KVM support and guest configuration.
 */
void kvm_arm_steal_time_finalize(ARMCPU *cpu, Error **errp);

/**
 * kvm_arm_steal_time_supported:
 *
 * Returns: true if KVM can enable steal time reporting
 * and false otherwise.
 */
bool kvm_arm_steal_time_supported(void);

/**
 * kvm_arm_vtimer_status_finalize:
 * @cpu: ARMCPU for which to finalize kvm-vtimer-status
 * @errp: Pointer to Error* for error propagation
 *
 * Validate the kvm-vtimer-status property selection and set its default
 * based on KVM support and guest configuration.
 */
void kvm_arm_vtimer_status_finalize(ARMCPU *cpu, Error **errp);

/**
 * kvm_arm_vtimer_status_supported:
 *
 * Returns: true if KVM can enable vtiemr status
 * and false otherwise.
 */
bool kvm_arm_vtimer_status_supported(void);

/**
 * kvm_arm_aarch32_supported:
 *
 * Returns: true if KVM can enable AArch32 mode
 * and false otherwise.
 */
bool kvm_arm_aarch32_supported(void);

/**
 * kvm_arm_pmu_supported:
 *
 * Returns: true if KVM can enable the PMU
 * and false otherwise.
 */
bool kvm_arm_pmu_supported(void);

/**
 * kvm_arm_sve_supported:
 *
 * Returns true if KVM can enable SVE and false otherwise.
 */
bool kvm_arm_sve_supported(void);

/**
 * kvm_arm_get_max_vm_ipa_size:
 * @ms: Machine state handle
 * @fixed_ipa: True when the IPA limit is fixed at 40. This is the case
 * for legacy KVM.
 *
 * Returns the number of bits in the IPA address space supported by KVM
 */
int kvm_arm_get_max_vm_ipa_size(MachineState *ms, bool *fixed_ipa);

/**
 * kvm_arm_sync_mpstate_to_kvm:
 * @cpu: ARMCPU
 *
 * If supported set the KVM MP_STATE based on QEMU's model.
 *
 * Returns 0 on success and -1 on failure.
 */
int kvm_arm_sync_mpstate_to_kvm(ARMCPU *cpu);

/**
 * kvm_arm_sync_mpstate_to_qemu:
 * @cpu: ARMCPU
 *
 * If supported get the MP_STATE from KVM and store in QEMU's model.
 *
 * Returns 0 on success and aborts on failure.
 */
int kvm_arm_sync_mpstate_to_qemu(ARMCPU *cpu);

/**
 * kvm_arm_get_virtual_time:
 * @cs: CPUState
 *
 * Gets the VCPU's virtual counter and stores it in the KVM CPU state.
 */
void kvm_arm_get_virtual_time(CPUState *cs);

/**
 * kvm_arm_put_virtual_time:
 * @cs: CPUState
 *
 * Sets the VCPU's virtual counter to the value stored in the KVM CPU state.
 */
void kvm_arm_put_virtual_time(CPUState *cs);

void kvm_arm_vm_state_change(void *opaque, bool running, RunState state);

int kvm_arm_vgic_probe(void);

void kvm_arm_pmu_set_irq(CPUState *cs, int irq);
void kvm_arm_pmu_init(CPUState *cs);

/**
 * kvm_arm_pvtime_init:
 * @cs: CPUState
 * @ipa: Per-vcpu guest physical base address of the pvtime structures
 *
 * Initializes PVTIME for the VCPU, setting the PVTIME IPA to @ipa.
 */
void kvm_arm_pvtime_init(CPUState *cs, uint64_t ipa);

/**
 * kvm_arm_pvtimer_status_init:
 * @cs: CPUState
 * @ipa: Per-vcpu guest physical base address of the vtimer status structures
 *
 * Initializes VTIMER STATUS for the VCPU, setting the VTIEMR STATUS IPA to @ipa.
 */
void kvm_arm_pvtimer_status_init(CPUState *cs, uint64_t ipa);

/**
 * kvm_arm_timer_early_inject_supported:
 *
 * Returns: true if KVM can support timer early inject
 * and false otherwise.
 */
bool kvm_arm_timer_early_inject_supported(void);

/**
 * kvm_arm_timer_early_inject_init:
 * @ipa: Guest physical base address of the timer early inject structure
 *
 * Initializes TIMER EARLY INJECT for the VM, setting the TIMER EARLY INJECT
 * IPA to @ipa. Called once per VM, not per vCPU.
 */
void kvm_arm_timer_early_inject_init(uint64_t ipa);

int kvm_arm_set_irq(int cpu, int irqtype, int irq, int level);

void tmm_add_ram_region(hwaddr base1, hwaddr len1, hwaddr base2, hwaddr len2, bool populate);

int tmm_get_kae_num(void);
void tmm_set_sec_addr(hwaddr base, int num);
void tmm_set_hpre_addr(hwaddr base, int num);

int kvm_arm_tmm_init(ConfidentialGuestSupport *cgs, Error **errp);
bool kvm_arm_tmm_enabled(void);

/**
 * kvm_arm_set_smccc_filter
 * @func: funcion
 * @faction: SMCCC filter action(handle, deny, fwd-to-user) to be deployed
 *
 * Sets the ARMs SMC-CC filter in KVM Host for selective hypercall exits
 */
int kvm_arm_set_smccc_filter(uint64_t func, uint8_t faction);

/**
 * kvm_arm_rme_init
 * @ms: the machine state
 *
 * Prepare the machine to be a Realm, if the user enabled it.
 */
int kvm_arm_rme_init(MachineState *ms);

/**
 * kvm_arm_rme_vm_type
 * @ms: the machine state
 *
 * Returns the Realm KVM VM type if the user requested a Realm, 0 otherwise.
 */
int kvm_arm_rme_vm_type(MachineState *ms);

/**
 * kvm_arm_rme_vcpu_init
 * @cs: the CPU
 *
 * If the user requested a Realm, setup the given vCPU accordingly. Realm vCPUs
 * behave a little differently, for example most of their register state is
 * hidden from the host.
 */
int kvm_arm_rme_vcpu_init(CPUState *cs);

/*
 * kvm_arm_rme_init_guest_ram
 * @base: base address of RAM
 * @size: size of RAM
 *
 * If the user requested a Realm, set the base and size of guest RAM, in order
 * to initialize the Realm IPA space.
 */
void kvm_arm_rme_init_guest_ram(hwaddr base, size_t size);

/**
 * kvm_arm_rme_init_gpa_space
 * @highest_gpa: highest address of the lower half of the guest address space
 * @pci_bus: The main PCI bus, for which PCI queries DMA address spaces
 *
 * Setup the guest-physical address space for a Realm. Install a memory region
 * and notifier to manage the shared upper half of the address space.
 */
void kvm_arm_rme_init_gpa_space(hwaddr highest_gpa, PCIBus *pci_bus);

/**
 * kvm_arm_rme_get_measurement_log
 *
 * Obtain the measurement log object if enabled, in order to get its size and
 * set its base address.
 *
 * Returns NULL if measurement log is disabled.
 */
Object *kvm_arm_rme_get_measurement_log(void);

int kvm_arm_handle_rme_dev(CPUState *cs, struct kvm_run *run);

int kvm_arm_get_writable_id_regs(ARMCPU *cpu, IdRegMap *idregmap);

void kvm_arm_writable_idregs_to_cpreg_list(ARMCPU *cpu);

#else

/*
 * It's safe to call these functions without KVM support.
 * They should either do nothing or return "not supported".
 */
static inline bool kvm_arm_aarch32_supported(void)
{
    return false;
}

static inline bool kvm_arm_pmu_supported(void)
{
    return false;
}

static inline bool kvm_arm_sve_supported(void)
{
    return false;
}

static inline bool kvm_arm_steal_time_supported(void)
{
    return false;
}

static inline bool kvm_arm_vtimer_status_supported(void)
{
    return false;
}

static inline void kvm_arm_rme_init_guest_ram(hwaddr base, size_t size)
{
}

/**
 * kvm_arm_rme_setup_gpa
 * @highest_gpa: highest address of the lower half of the guest address space
 * @pci_bus: The main PCI bus, for which PCI queries DMA address spaces
 *
 * Setup the guest-physical address space for a Realm. Install a memory region
 * and notifier to manage the shared upper half of the address space.
 */
void kvm_arm_rme_init_gpa_space(hwaddr highest_gpa, PCIBus *pci_bus);

static inline Object *kvm_arm_rme_get_measurement_log(void)
{
    return NULL;
}

static inline int kvm_arm_get_writable_id_regs(ARMCPU *cpu, IdRegMap *idregmap)
{
    return -ENOSYS;
}

static inline void kvm_arm_writable_idregs_to_cpreg_list(ARMCPU *cpu)
{
    g_assert_not_reached();
}
/*
 * These functions should never actually be called without KVM support.
 */
static inline void kvm_arm_set_cpu_features_from_host(ARMCPU *cpu,
                                                      bool exhaustive)
{
    g_assert_not_reached();
}

static inline void kvm_arm_add_vcpu_properties(Object *obj)
{
    g_assert_not_reached();
}

static inline int kvm_arm_vcpu_finalize(CPUState *cs, int feature)
{
    g_assert_not_reached();
}

static inline int kvm_arm_get_max_vm_ipa_size(MachineState *ms, bool *fixed_ipa)
{
    g_assert_not_reached();
}

static inline int kvm_arm_vgic_probe(void)
{
    g_assert_not_reached();
}

static inline void kvm_arm_pmu_set_irq(CPUState *cs, int irq)
{
    g_assert_not_reached();
}

static inline void kvm_arm_pmu_init(CPUState *cs)
{
    g_assert_not_reached();
}

static inline void kvm_arm_pvtime_init(CPUState *cs, uint64_t ipa)
{
    g_assert_not_reached();
}

static inline void kvm_arm_steal_time_finalize(ARMCPU *cpu, Error **errp)
{
    g_assert_not_reached();
}

static inline void kvm_arm_pvtimer_status_init(CPUState *cs, uint64_t ipa)
{
    g_assert_not_reached();
}

static inline bool kvm_arm_timer_early_inject_supported(void)
{
    return false;
}

static inline void kvm_arm_timer_early_inject_init(uint64_t ipa)
{
    g_assert_not_reached();
}

static inline void kvm_arm_vtimer_status_finalize(ARMCPU *cpu, Error **errp)
{
    g_assert_not_reached();
}

static inline uint32_t kvm_arm_sve_get_vls(CPUState *cs)
{
    g_assert_not_reached();
}

static inline int kvm_arm_set_smccc_filter(uint64_t func, uint8_t faction)
{
    g_assert_not_reached();
}

static inline int kvm_arm_tmm_init(ConfidentialGuestSupport *cgs, Error **errp G_GNUC_UNUSED)
{
    g_assert_not_reached();
}

static inline void tmm_add_ram_region(hwaddr base1, hwaddr len1, hwaddr base2,
                                      hwaddr len2, bool populate)
{
    g_assert_not_reached();
}

static inline void tmm_set_sec_addr(hwaddr base, int num)
{
    g_assert_not_reached();
}

static inline void tmm_set_hpre_addr(hwaddr base, int num)
{
    g_assert_not_reached();
}

static inline int tmm_get_kae_num(void)
{
    g_assert_not_reached();
}

static inline int kvm_arm_rme_init(MachineState *ms)
{
    g_assert_not_reached();
}

static inline int kvm_arm_rme_vm_type(MachineState *ms)
{
    g_assert_not_reached();
}

static inline int kvm_arm_rme_vcpu_init(CPUState *cs)
{
    g_assert_not_reached();
}

#endif

/**
 * kvm_arm_handle_debug:
 * @cs: CPUState
 * @debug_exit: debug part of the KVM exit structure
 *
 * Returns: TRUE if the debug exception was handled.
 */
bool kvm_arm_handle_debug(CPUState *cs, struct kvm_debug_exit_arch *debug_exit);

/**
 * kvm_arm_hw_debug_active:
 * @cs: CPU State
 *
 * Return: TRUE if any hardware breakpoints in use.
 */
bool kvm_arm_hw_debug_active(CPUState *cs);

/**
 * kvm_arm_copy_hw_debug_data:
 * @ptr: kvm_guest_debug_arch structure
 *
 * Copy the architecture specific debug registers into the
 * kvm_guest_debug ioctl structure.
 */
struct kvm_guest_debug_arch;
void kvm_arm_copy_hw_debug_data(struct kvm_guest_debug_arch *ptr);

/**
 * kvm_arm_verify_ext_dabt_pending:
 * @cs: CPUState
 *
 * Verify the fault status code wrt the Ext DABT injection
 *
 * Returns: true if the fault status code is as expected, false otherwise
 */
bool kvm_arm_verify_ext_dabt_pending(CPUState *cs);

#endif
