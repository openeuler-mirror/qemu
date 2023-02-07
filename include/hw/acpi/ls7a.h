/*
 * QEMU GMCH/LS7A PCI PM Emulation
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

#ifndef HW_ACPI_LS7A_H
#define HW_ACPI_LS7A_H

#include "hw/acpi/acpi.h"
#include "hw/acpi/cpu_hotplug.h"
#include "hw/acpi/cpu.h"
#include "hw/acpi/memory_hotplug.h"
#include "hw/acpi/acpi_dev_interface.h"
#include "hw/acpi/tco.h"

#define CPU_HOTPLUG_BASE 0x1e000000
#define MEMORY_HOTPLUG_BASE 0x1e00000c

typedef struct LS7APCIPMRegs {
    /*
     * In ls7a spec says that pm1_cnt register is 32bit width and
     * that the upper 16bits are reserved and unused.
     * PM1a_CNT_BLK = 2 in FADT so it is defined as uint16_t.
     */
    ACPIREGS acpi_regs;

    MemoryRegion iomem;
    MemoryRegion iomem_gpe;
    MemoryRegion iomem_smi;
    MemoryRegion iomem_reset;

    qemu_irq irq; /* SCI */

    uint32_t pm_io_base;
    Notifier powerdown_notifier;

    bool cpu_hotplug_legacy;
    AcpiCpuHotplug gpe_cpu;
    CPUHotplugState cpuhp_state;

    MemHotplugState acpi_memory_hotplug;

    uint8_t disable_s3;
    uint8_t disable_s4;
    uint8_t s4_val;
} LS7APCIPMRegs;

void ls7a_pm_init(LS7APCIPMRegs *ls7a, qemu_irq *sci_irq);

void ls7a_pm_iospace_update(LS7APCIPMRegs *pm, uint32_t pm_io_base);
extern const VMStateDescription vmstate_ls7a_pm;

void ls7a_pm_add_properties(Object *obj, LS7APCIPMRegs *pm, Error **errp);

void ls7a_pm_device_plug_cb(HotplugHandler *hotplug_dev, DeviceState *dev,
                            Error **errp);
void ls7a_pm_device_unplug_request_cb(HotplugHandler *hotplug_dev,
                                      DeviceState *dev, Error **errp);
void ls7a_pm_device_unplug_cb(HotplugHandler *hotplug_dev, DeviceState *dev,
                              Error **errp);

void ls7a_pm_ospm_status(AcpiDeviceIf *adev, ACPIOSTInfoList ***list);

void ls7a_send_gpe(AcpiDeviceIf *adev, AcpiEventStatusBits ev);
#endif /* HW_ACPI_LS7A_H */
