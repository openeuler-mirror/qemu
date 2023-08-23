/*
 * Hotplug emulation on Loongarch system.
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

#ifndef HW_LOONGARCH_H
#define HW_LOONGARCH_H

#include "target/loongarch64/cpu.h"
#include "qemu-common.h"
#include "exec/memory.h"
#include "hw/mem/pc-dimm.h"
#include "hw/hotplug.h"
#include "hw/boards.h"
#include "hw/acpi/acpi.h"
#include "qemu/notify.h"
#include "qemu/error-report.h"
#include "qemu/queue.h"
#include "hw/acpi/memory_hotplug.h"
#include "hw/loongarch/cpudevs.h"
#include "hw/block/flash.h"

#define LOONGARCH_MAX_VCPUS 256
#define LOONGARCH_MAX_PFLASH 2
/* 256MB alignment for hotplug memory region */
#define LOONGARCH_HOTPLUG_MEM_ALIGN (1ULL << 28)
#define LOONGARCH_MAX_RAM_SLOTS 10

#ifdef CONFIG_KVM
#define LS_ISA_IO_SIZE          0x02000000
#else
#define LS_ISA_IO_SIZE          0x00010000
#endif

/* Memory types: */
#define SYSTEM_RAM 1
#define SYSTEM_RAM_RESERVED 2
#define ACPI_TABLE 3
#define ACPI_NVS 4
#define SYSTEM_PMEM 5

#define MAX_MEM_MAP 128

typedef struct LoongarchMachineClass {
    /*< private >*/
    MachineClass parent_class;

    /* Methods: */
    HotplugHandler *(*get_hotplug_handler)(MachineState *machine,
                                           DeviceState *dev);

    bool has_acpi_build;

    /* save different cpu address*/
    uint64_t isa_io_base;
    uint64_t ht_control_regs_base;
    uint64_t hpet_mmio_addr;
    uint64_t smbus_cfg_base;
    uint64_t pciecfg_base;
    uint64_t ls7a_ioapic_reg_base;
    uint32_t node_shift;
    char cpu_name[40];
    char bridge_name[16];

} LoongarchMachineClass;

typedef struct ResetData {
    LOONGARCHCPU *cpu;
    uint64_t vector;
} ResetData;

typedef struct LoongarchMachineState {
    /*< private >*/
    MachineState parent_obj;

    /* <public> */
    ram_addr_t hotplug_memory_size;

    /* State for other subsystems/APIs: */
    Notifier machine_done;
    /* Pointers to devices and objects: */
    HotplugHandler *acpi_dev;
    int ram_slots;
    ResetData *reset_info[LOONGARCH_MAX_VCPUS];
    DeviceState *rtc;
    gipiState *gipi;
    apicState *apic;

    FWCfgState *fw_cfg;
    bool acpi_build_enabled;
    bool apic_xrupt_override;
    CPUArchIdList *possible_cpus;
    PFlashCFI01 *flash[LOONGARCH_MAX_PFLASH];
    void *fdt;
    int fdt_size;
    unsigned int hotpluged_cpu_num;
    DeviceState *platform_bus_dev;
    OnOffAuto acpi;
    char *oem_id;
    char *oem_table_id;
} LoongarchMachineState;

#define LOONGARCH_MACHINE_ACPI_DEVICE_PROP "loongarch-acpi-device"
#define TYPE_LOONGARCH_MACHINE "loongarch-machine"

#define LoongarchMACHINE(obj)                                                 \
    OBJECT_CHECK(LoongarchMachineState, (obj), TYPE_LOONGARCH_MACHINE)
#define LoongarchMACHINE_GET_CLASS(obj)                                       \
    OBJECT_GET_CLASS(LoongarchMachineClass, (obj), TYPE_LOONGARCH_MACHINE)
#define LoongarchMACHINE_CLASS(klass)                                         \
    OBJECT_CLASS_CHECK(LoongarchMachineClass, (klass), TYPE_LOONGARCH_MACHINE)

#define DEFINE_LOONGARCH_MACHINE(suffix, namestr, initfn, optsfn)             \
    static void loongarch_machine_##suffix##_class_init(ObjectClass *oc,      \
                                                        void *data)           \
    {                                                                         \
        MachineClass *mc = MACHINE_CLASS(oc);                                 \
        optsfn(mc);                                                           \
        mc->init = initfn;                                                    \
    }                                                                         \
    static const TypeInfo loongarch_machine_type_##suffix = {                 \
        .name = namestr TYPE_MACHINE_SUFFIX,                                  \
        .parent = TYPE_LOONGARCH_MACHINE,                                     \
        .class_init = loongarch_machine_##suffix##_class_init,                \
    };                                                                        \
    static void loongarch_machine_init_##suffix(void)                         \
    {                                                                         \
        type_register(&loongarch_machine_type_##suffix);                      \
    }                                                                         \
    type_init(loongarch_machine_init_##suffix)

void loongarch_machine_device_unplug_request(HotplugHandler *hotplug_dev,
                                             DeviceState *dev, Error **errp);
void longson_machine_device_unplug(HotplugHandler *hotplug_dev,
                                   DeviceState *dev, Error **errp);
HotplugHandler *loongarch_get_hotpug_handler(MachineState *machine,
                                             DeviceState *dev);
void loongarch_machine_device_pre_plug(HotplugHandler *hotplug_dev,
                                       DeviceState *dev, Error **errp);
void loongarch_machine_device_plug(HotplugHandler *hotplug_dev,
                                   DeviceState *dev, Error **errp);

LOONGARCHCPU *loongarch_cpu_create(MachineState *machine, LOONGARCHCPU *cpu,
                                   Error **errp);
void loongarch_cpu_destroy(MachineState *machine, LOONGARCHCPU *cpu);
int cpu_init_ipi(LoongarchMachineState *ms, qemu_irq parent, int cpu);
int cpu_init_apic(LoongarchMachineState *ms, CPULOONGARCHState *env, int cpu);
int la_memmap_add_entry(uint64_t address, uint64_t length, uint32_t type);
bool loongarch_is_acpi_enabled(LoongarchMachineState *vms);

/* acpi-build.c */
void ls7a_madt_cpu_entry(AcpiDeviceIf *adev, int uid,
                         const CPUArchIdList *apic_ids, GArray *entry,
                         bool force_enabled);
void slave_cpu_reset(void *opaque);
#endif
