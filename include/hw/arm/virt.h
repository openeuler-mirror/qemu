/*
 *
 * Copyright (c) 2015 Linaro Limited
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
 * Emulate a virtual board which works by passing Linux all the information
 * it needs about what devices are present via the device tree.
 * There are some restrictions about what we can do here:
 *  + we can only present devices whose Linux drivers will work based
 *    purely on the device tree with no platform data at all
 *  + we want to present a very stripped-down minimalist platform,
 *    both because this reduces the security attack surface from the guest
 *    and also because it reduces our exposure to being broken when
 *    the kernel updates its device tree bindings and requires further
 *    information in a device binding that we aren't providing.
 * This is essentially the same approach kvmtool uses.
 */

#ifndef QEMU_ARM_VIRT_H
#define QEMU_ARM_VIRT_H

#include "exec/hwaddr.h"
#include "qemu/notify.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/block/flash.h"
#include "sysemu/kvm.h"
#include "hw/intc/arm_gicv3_common.h"
#include "qom/object.h"
#include "hw/acpi/acpi_dev_interface.h"

#define NUM_GICV2M_SPIS       64
#define NUM_VIRTIO_TRANSPORTS 32
#define NUM_SMMU_IRQS          4

#define ARCH_GIC_MAINT_IRQ  9

#define ARCH_TIMER_VIRT_IRQ   11
#define ARCH_TIMER_S_EL1_IRQ  13
#define ARCH_TIMER_NS_EL1_IRQ 14
#define ARCH_TIMER_NS_EL2_IRQ 10

#define VIRTUAL_PMU_IRQ 7

#define PPI(irq) ((irq) + 16)

/* See Linux kernel arch/arm64/include/asm/pvclock-abi.h */
#define PVTIME_SIZE_PER_CPU 64

/* ARM CLIDR_EL1 related definitions */
/* Ctypen, bits[3(n - 1) + 2 : 3(n - 1)], for n = 1 to 7 */
#define CTYPE_NONE     0b000
#define CTYPE_INS      0b001
#define CTYPE_DATA     0b010
#define CTYPE_INS_DATA 0b011
#define CTYPE_UNIFIED  0b100

#define ARM64_REG_CLIDR_EL1 ARM64_SYS_REG(3, 1, 0, 0, 1)

#define CLIDR_CTYPE_SHIFT(level) (3 * (level - 1))
#define CLIDR_CTYPE_MASK(level) (7 << CLIDR_CTYPE_SHIFT(level))
#define CLIDR_CTYPE(clidr, level) \
    (((clidr) & CLIDR_CTYPE_MASK(level)) >> CLIDR_CTYPE_SHIFT(level))

/* L1 data cache */
#define ARM_L1DCACHE_SIZE 65536
#define ARM_L1DCACHE_SETS 256
#define ARM_L1DCACHE_ASSOCIATIVITY 4
#define ARM_L1DCACHE_ATTRIBUTES 2
#define ARM_L1DCACHE_LINE_SIZE 64

/* L1 instruction cache */
#define ARM_L1ICACHE_SIZE 65536
#define ARM_L1ICACHE_SETS 256
#define ARM_L1ICACHE_ASSOCIATIVITY 4
#define ARM_L1ICACHE_ATTRIBUTES 4
#define ARM_L1ICACHE_LINE_SIZE 64

/* L1 unified cache */
#define ARM_L1CACHE_SIZE 131072
#define ARM_L1CACHE_SETS 256
#define ARM_L1CACHE_ASSOCIATIVITY 4
#define ARM_L1CACHE_ATTRIBUTES 10
#define ARM_L1CACHE_LINE_SIZE 128

/* L2 unified cache */
#define ARM_L2CACHE_SIZE 524288
#define ARM_L2CACHE_SETS 1024
#define ARM_L2CACHE_ASSOCIATIVITY 8
#define ARM_L2CACHE_ATTRIBUTES 10
#define ARM_L2CACHE_LINE_SIZE 64

/* L3 unified cache */
#define ARM_L3CACHE_SIZE 33554432
#define ARM_L3CACHE_SETS 2048
#define ARM_L3CACHE_ASSOCIATIVITY 15
#define ARM_L3CACHE_ATTRIBUTES 10
#define ARM_L3CACHE_LINE_SIZE 128

/* Definitions of the hardcoded cache info */
typedef enum {
    ARM_L1D_CACHE,
    ARM_L1I_CACHE,
    ARM_L1_CACHE,
    ARM_L2_CACHE,
    ARM_L3_CACHE
} ArmCacheType;

enum {
    VIRT_FLASH,
    VIRT_MEM,
    VIRT_CPUPERIPHS,
    VIRT_GIC_DIST,
    VIRT_GIC_CPU,
    VIRT_GIC_V2M,
    VIRT_GIC_HYP,
    VIRT_GIC_VCPU,
    VIRT_GIC_ITS,
    VIRT_GIC_REDIST,
    VIRT_SMMU,
    VIRT_UART,
    VIRT_CPUFREQ,
    VIRT_MMIO,
    VIRT_RTC,
    VIRT_FW_CFG,
    VIRT_PCIE,
    VIRT_PCIE_MMIO,
    VIRT_PCIE_PIO,
    VIRT_PCIE_ECAM,
    VIRT_PLATFORM_BUS,
    VIRT_GPIO,
    VIRT_SECURE_UART,
    VIRT_SECURE_MEM,
    VIRT_SECURE_GPIO,
    VIRT_PCDIMM_ACPI,
    VIRT_ACPI_GED,
    VIRT_NVDIMM_ACPI,
    VIRT_PVTIME,
    VIRT_CPU_ACPI,
    VIRT_LOWMEMMAP_LAST,
};

/* indices of IO regions located after the RAM */
enum {
    VIRT_HIGH_GIC_REDIST2 =  VIRT_LOWMEMMAP_LAST,
    VIRT_HIGH_PCIE_ECAM,
    VIRT_HIGH_PCIE_MMIO,
};

typedef enum VirtIOMMUType {
    VIRT_IOMMU_NONE,
    VIRT_IOMMU_SMMUV3,
    VIRT_IOMMU_VIRTIO,
} VirtIOMMUType;

typedef enum VirtMSIControllerType {
    VIRT_MSI_CTRL_NONE,
    VIRT_MSI_CTRL_GICV2M,
    VIRT_MSI_CTRL_ITS,
} VirtMSIControllerType;

typedef enum VirtGICType {
    VIRT_GIC_VERSION_MAX,
    VIRT_GIC_VERSION_HOST,
    VIRT_GIC_VERSION_2,
    VIRT_GIC_VERSION_3,
    VIRT_GIC_VERSION_NOSEL,
} VirtGICType;

struct VirtMachineClass {
    MachineClass parent;
    bool disallow_affinity_adjustment;
    bool no_its;
    bool no_tcg_its;
    bool no_pmu;
    bool claim_edge_triggered_timers;
    bool smbios_old_sys_ver;
    bool no_highmem_ecam;
    bool no_ged;   /* Machines < 4.2 have no support for ACPI GED device */
    bool kvm_no_adjvtime;
    bool no_kvm_steal_time;
    bool acpi_expose_flash;
    bool no_secure_gpio;
    /* Machines < 6.2 have no support for describing cpu topology to guest */
    bool no_cpu_topology;
};

struct VirtMachineState {
    MachineState parent;
    Notifier machine_done;
    DeviceState *platform_bus_dev;
    FWCfgState *fw_cfg;
    PFlashCFI01 *flash[2];
    bool secure;
    bool highmem;
    bool highmem_ecam;
    bool its;
    bool tcg_its;
    bool virt;
    bool cpu_hotplug_enabled;
    bool ras;
    bool mte;
    OnOffAuto acpi;
    VirtGICType gic_version;
    VirtIOMMUType iommu;
    bool default_bus_bypass_iommu;
    VirtMSIControllerType msi_controller;
    uint16_t virtio_iommu_bdf;
    struct arm_boot_info bootinfo;
    MemMapEntry *memmap;
    char *pciehb_nodename;
    const int *irqmap;
    int fdt_size;
    uint32_t clock_phandle;
    uint32_t gic_phandle;
    uint32_t msi_phandle;
    uint32_t iommu_phandle;
    int psci_conduit;
    uint32_t boot_cpus;
    hwaddr highest_gpa;
    DeviceState *gic;
    DeviceState *acpi_dev;
    Notifier powerdown_notifier;
    PCIBus *bus;
    char *oem_id;
    char *oem_table_id;
    char *kvm_type;
};

#define VIRT_ECAM_ID(high) (high ? VIRT_HIGH_PCIE_ECAM : VIRT_PCIE_ECAM)

#define TYPE_VIRT_MACHINE   MACHINE_TYPE_NAME("virt")
OBJECT_DECLARE_TYPE(VirtMachineState, VirtMachineClass, VIRT_MACHINE)

void virt_acpi_setup(VirtMachineState *vms);
bool virt_is_acpi_enabled(VirtMachineState *vms);
void virt_madt_cpu_entry(AcpiDeviceIf *adev, int uid,
                         const CPUArchIdList *cpu_list, GArray *entry,
                         bool force_enabled);
void virt_acpi_dsdt_cpu_cppc(AcpiDeviceIf *adev, int uid,
                             int num_cpu, Aml *dev);
bool cpu_l1_cache_unified(int cpu);

/* Return the number of used redistributor regions  */
static inline int virt_gicv3_redist_region_count(VirtMachineState *vms)
{
    uint32_t redist0_capacity =
                vms->memmap[VIRT_GIC_REDIST].size / GICV3_REDIST_SIZE;

    assert(vms->gic_version == VIRT_GIC_VERSION_3);
    GICv3State *s = ARM_GICV3_COMMON(vms->gic);

    return s->num_cpu > redist0_capacity ? 2 : 1;
}

#endif /* QEMU_ARM_VIRT_H */
