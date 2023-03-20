/*
 * Hotplug emulation on Loongarch system.
 *
 * Copyright (c) 2023 Loongarch Technology
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/queue.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qemu/bcd.h"
#include "hw/hotplug.h"
#include "hw/loongarch/cpudevs.h"
#include "hw/mem/memory-device.h"
#include "sysemu/numa.h"
#include "sysemu/cpus.h"
#include "hw/loongarch/larch.h"
#include "hw/cpu/core.h"
#include "hw/nvram/fw_cfg.h"
#include "hw/platform-bus.h"

/* find cpu slot in machine->possible_cpus by core_id */
static CPUArchId *loongarch_find_cpu_slot(MachineState *ms, uint32_t id,
                                          int *idx)
{
    int index = id;

    if (index >= ms->possible_cpus->len) {
        return NULL;
    }
    if (idx) {
        *idx = index;
    }
    return &ms->possible_cpus->cpus[index];
}

static void loongarch_memory_plug(HotplugHandler *hotplug_dev,
                                  DeviceState *dev, Error **errp)
{
    Error *local_err = NULL;
    LoongarchMachineState *lsms = LoongarchMACHINE(hotplug_dev);
    HotplugHandlerClass *hhc;
    uint64_t size;

    size = memory_device_get_region_size(MEMORY_DEVICE(dev), &error_abort);
    if (size % LOONGARCH_HOTPLUG_MEM_ALIGN) {
        error_setg(&local_err,
                   "Hotplugged memory size must be a multiple of "
                   "%lld MB",
                   LOONGARCH_HOTPLUG_MEM_ALIGN / MiB);
        goto out;
    }

    pc_dimm_plug(PC_DIMM(dev), MACHINE(lsms));

    hhc = HOTPLUG_HANDLER_GET_CLASS(lsms->acpi_dev);
    hhc->plug(HOTPLUG_HANDLER(lsms->acpi_dev), dev, &error_abort);
out:
    error_propagate(errp, local_err);
}

static void loongarch_memory_unplug_request(HotplugHandler *hotplug_dev,
                                            DeviceState *dev, Error **errp)
{
    Error *local_err = NULL;
    HotplugHandlerClass *hhc;
    LoongarchMachineState *lsms = LoongarchMACHINE(hotplug_dev);

    if (!lsms->acpi_dev || !loongarch_is_acpi_enabled(lsms)) {
        error_setg(
            &local_err,
            "memory hotplug is not enabled: missing acpi device or acpi disabled");
        goto out;
    }
    hhc = HOTPLUG_HANDLER_GET_CLASS(lsms->acpi_dev);
    hhc->unplug_request(HOTPLUG_HANDLER(lsms->acpi_dev), dev, &local_err);

out:
    error_propagate(errp, local_err);
}

static void loongarch_cpu_unplug(HotplugHandler *hotplug_dev, DeviceState *dev,
                                 Error **errp)
{
    CPUArchId *found_cpu;
    HotplugHandlerClass *hhc;
    Error *local_err = NULL;
    LOONGARCHCPU *cpu = LOONGARCH_CPU(dev);
    MachineState *machine = MACHINE(OBJECT(hotplug_dev));
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);

    hhc = HOTPLUG_HANDLER_GET_CLASS(lsms->acpi_dev);
    hhc->unplug(HOTPLUG_HANDLER(lsms->acpi_dev), dev, &local_err);

    if (local_err) {
        goto out;
    }

    loongarch_cpu_destroy(machine, cpu);

    found_cpu = loongarch_find_cpu_slot(MACHINE(lsms), cpu->id, NULL);
    found_cpu->cpu = NULL;
    object_unparent(OBJECT(dev));
    lsms->hotpluged_cpu_num -= 1;
out:
    error_propagate(errp, local_err);
}

static void loongarch_memory_unplug(HotplugHandler *hotplug_dev,
                                    DeviceState *dev, Error **errp)
{
    Error *local_err = NULL;
    HotplugHandlerClass *hhc;
    LoongarchMachineState *lsms = LoongarchMACHINE(hotplug_dev);

    hhc = HOTPLUG_HANDLER_GET_CLASS(lsms->acpi_dev);
    hhc->unplug(HOTPLUG_HANDLER(lsms->acpi_dev), dev, &local_err);

    if (local_err) {
        goto out;
    }

    pc_dimm_unplug(PC_DIMM(dev), MACHINE(hotplug_dev));
    object_unparent(OBJECT(dev));

out:
    error_propagate(errp, local_err);
}

static void loongarch_cpu_pre_plug(HotplugHandler *hotplug_dev,
                                   DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(OBJECT(hotplug_dev));
    MachineClass *mc = MACHINE_GET_CLASS(hotplug_dev);
    LoongarchMachineState *lsms = LoongarchMACHINE(ms);
    LOONGARCHCPU *cpu = LOONGARCH_CPU(dev);
    CPUArchId *cpu_slot;
    Error *local_err = NULL;
    int index;
    int free_index = lsms->hotpluged_cpu_num + ms->smp.cpus;
    int max_cpus = ms->smp.max_cpus;

    if (dev->hotplugged && !mc->has_hotpluggable_cpus) {
        error_setg(&local_err, "CPU hotplug not supported for this machine");
        goto out;
    }

    if (!object_dynamic_cast(OBJECT(cpu), ms->cpu_type)) {
        error_setg(errp, "Invalid CPU type, expected cpu type: '%s'",
                   ms->cpu_type);
        return;
    }

    /* if ID is not set, set it based on core properties */
    if (cpu->id == UNASSIGNED_CPU_ID) {
        if ((cpu->core_id) > (max_cpus - 1)) {
            error_setg(errp, "Invalid CPU core-id: %u must be in range 0:%u",
                       cpu->core_id, max_cpus - 1);
            return;
        }

        if (free_index > (max_cpus - 1)) {
            error_setg(errp, "The maximum number of CPUs cannot exceed %u.",
                       max_cpus);
            return;
        }

        if (cpu->core_id != free_index) {
            error_setg(errp, "Invalid CPU core-id: %u must be :%u",
                       cpu->core_id, free_index);
            return;
        }

        cpu->id = cpu->core_id;
    }

    cpu_slot = loongarch_find_cpu_slot(MACHINE(hotplug_dev), cpu->id, &index);
    if (!cpu_slot) {
        error_setg(&local_err, "core id %d out of range", cpu->id);
        goto out;
    }

    if (cpu_slot->cpu) {
        error_setg(&local_err, "core %d already populated", cpu->id);
        goto out;
    }

    numa_cpu_pre_plug(cpu_slot, dev, &local_err);

    return;
out:
    error_propagate(errp, local_err);
}

static void loongarch_memory_pre_plug(HotplugHandler *hotplug_dev,
                                      DeviceState *dev, Error **errp)
{
    MachineState *machine = MACHINE(OBJECT(hotplug_dev));
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);
    PCDIMMDevice *dimm = PC_DIMM(dev);
    Error *local_err = NULL;
    uint64_t size;

    if (!lsms->acpi_dev || !loongarch_is_acpi_enabled(lsms)) {
        error_setg(
            errp,
            "memory hotplug is not enabled: missing acpi device or acpi disabled");
        return;
    }

    size = memory_device_get_region_size(MEMORY_DEVICE(dimm), &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    if (size % LOONGARCH_HOTPLUG_MEM_ALIGN) {
        error_setg(errp,
                   "Hotplugged memory size must be a multiple of "
                   "%lld MB",
                   LOONGARCH_HOTPLUG_MEM_ALIGN / MiB);
        return;
    }

    pc_dimm_pre_plug(dimm, MACHINE(hotplug_dev), NULL, errp);
}

static void loongarch_cpu_plug(HotplugHandler *hotplug_dev, DeviceState *dev,
                               Error **errp)
{
    CPUArchId *found_cpu;
    HotplugHandlerClass *hhc;
    Error *local_err = NULL;
    MachineState *machine = MACHINE(OBJECT(hotplug_dev));
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);
    LOONGARCHCPU *cpu = LOONGARCH_CPU(dev);

    if (lsms->acpi_dev) {
        loongarch_cpu_create(machine, cpu, errp);
        hhc = HOTPLUG_HANDLER_GET_CLASS(lsms->acpi_dev);
        hhc->plug(HOTPLUG_HANDLER(lsms->acpi_dev), dev, &local_err);
        if (local_err) {
            goto out;
        }
    }

    found_cpu = loongarch_find_cpu_slot(MACHINE(lsms), cpu->id, NULL);
    found_cpu->cpu = OBJECT(dev);
    lsms->hotpluged_cpu_num += 1;
out:
    error_propagate(errp, local_err);
}

static void loongarch_cpu_unplug_request(HotplugHandler *hotplug_dev,
                                         DeviceState *dev, Error **errp)
{
    MachineState *machine = MACHINE(OBJECT(hotplug_dev));
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);
    LOONGARCHCPU *cpu = LOONGARCH_CPU(dev);
    Error *local_err = NULL;
    HotplugHandlerClass *hhc;
    int idx = -1;

    if (!lsms->acpi_dev) {
        error_setg(&local_err, "CPU hot unplug not supported without ACPI");
        goto out;
    }

    loongarch_find_cpu_slot(MACHINE(lsms), cpu->id, &idx);
    assert(idx != -1);
    if (idx == 0) {
        error_setg(&local_err, "Boot CPU is unpluggable");
        goto out;
    }

    hhc = HOTPLUG_HANDLER_GET_CLASS(lsms->acpi_dev);
    hhc->unplug_request(HOTPLUG_HANDLER(lsms->acpi_dev), dev, &local_err);

    if (local_err) {
        goto out;
    }

out:
    error_propagate(errp, local_err);
}

void longson_machine_device_unplug(HotplugHandler *hotplug_dev,
                                   DeviceState *dev, Error **errp)
{
    MachineClass *mc = MACHINE_GET_CLASS(qdev_get_machine());

    if (object_dynamic_cast(OBJECT(dev), TYPE_PC_DIMM)) {
        loongarch_memory_unplug(hotplug_dev, dev, errp);
    } else if (object_dynamic_cast(OBJECT(dev), TYPE_LOONGARCH_CPU)) {
        if (!mc->has_hotpluggable_cpus) {
            error_setg(errp, "CPU hot unplug not supported on this machine");
            return;
        }
        loongarch_cpu_unplug(hotplug_dev, dev, errp);
    } else {
        error_setg(errp,
                   "acpi: device unplug for not supported device"
                   " type: %s",
                   object_get_typename(OBJECT(dev)));
    }

    return;
}

void loongarch_machine_device_unplug_request(HotplugHandler *hotplug_dev,
                                             DeviceState *dev, Error **errp)
{
    if (object_dynamic_cast(OBJECT(dev), TYPE_PC_DIMM)) {
        loongarch_memory_unplug_request(hotplug_dev, dev, errp);
    } else if (object_dynamic_cast(OBJECT(dev), TYPE_LOONGARCH_CPU)) {
        loongarch_cpu_unplug_request(hotplug_dev, dev, errp);
    }
}

HotplugHandler *loongarch_get_hotpug_handler(MachineState *machine,
                                             DeviceState *dev)
{
    if (object_dynamic_cast(OBJECT(dev), TYPE_PC_DIMM) ||
        object_dynamic_cast(OBJECT(dev), TYPE_LOONGARCH_CPU) ||
        object_dynamic_cast(OBJECT(dev), TYPE_SYS_BUS_DEVICE)) {
        return HOTPLUG_HANDLER(machine);
    }
    return NULL;
}

void loongarch_machine_device_pre_plug(HotplugHandler *hotplug_dev,
                                       DeviceState *dev, Error **errp)
{
    if (object_dynamic_cast(OBJECT(dev), TYPE_PC_DIMM)) {
        loongarch_memory_pre_plug(hotplug_dev, dev, errp);
    } else if (object_dynamic_cast(OBJECT(dev), TYPE_LOONGARCH_CPU)) {
        loongarch_cpu_pre_plug(hotplug_dev, dev, errp);
    }
}

void loongarch_machine_device_plug(HotplugHandler *hotplug_dev,
                                   DeviceState *dev, Error **errp)
{
    LoongarchMachineState *lsms = LoongarchMACHINE(hotplug_dev);

    if (lsms->platform_bus_dev) {
        MachineClass *mc = MACHINE_GET_CLASS(lsms);

        if (device_is_dynamic_sysbus(mc, dev)) {
            platform_bus_link_device(
                PLATFORM_BUS_DEVICE(lsms->platform_bus_dev),
                SYS_BUS_DEVICE(dev));
        }
    }

    if (object_dynamic_cast(OBJECT(dev), TYPE_PC_DIMM)) {
        loongarch_memory_plug(hotplug_dev, dev, errp);
    } else if (object_dynamic_cast(OBJECT(dev), TYPE_LOONGARCH_CPU)) {
        loongarch_cpu_plug(hotplug_dev, dev, errp);
    }
}
