/*
 * Loongarch acpi emulation
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
 */

#include "qemu/osdep.h"
#include "sysemu/sysemu.h"
#include "sysemu/runstate.h"
#include "sysemu/reset.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/ls7a.h"
#include "hw/nvram/fw_cfg.h"
#include "qemu/config-file.h"
#include "qapi/opts-visitor.h"
#include "qapi/qapi-events-run-state.h"
#include "qapi/error.h"
#include "hw/loongarch/ls7a.h"
#include "hw/mem/pc-dimm.h"
#include "hw/mem/nvdimm.h"
#include "migration/vmstate.h"

static void ls7a_pm_update_sci_fn(ACPIREGS *regs)
{
    LS7APCIPMRegs *pm = container_of(regs, LS7APCIPMRegs, acpi_regs);
    acpi_update_sci(&pm->acpi_regs, pm->irq);
}

static uint64_t ls7a_gpe_readb(void *opaque, hwaddr addr, unsigned width)
{
    LS7APCIPMRegs *pm = opaque;
    return acpi_gpe_ioport_readb(&pm->acpi_regs, addr);
}

static void ls7a_gpe_writeb(void *opaque, hwaddr addr, uint64_t val,
                            unsigned width)
{
    LS7APCIPMRegs *pm = opaque;
    acpi_gpe_ioport_writeb(&pm->acpi_regs, addr, val);
    acpi_update_sci(&pm->acpi_regs, pm->irq);
}

static const MemoryRegionOps ls7a_gpe_ops = {
    .read = ls7a_gpe_readb,
    .write = ls7a_gpe_writeb,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .impl.min_access_size = 1,
    .impl.max_access_size = 1,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

#define VMSTATE_GPE_ARRAY(_field, _state)                                     \
{                                                                             \
    .name = (stringify(_field)), .version_id = 0, .num = ACPI_GPE0_LEN,       \
    .info = &vmstate_info_uint8, .size = sizeof(uint8_t),                     \
    .flags = VMS_ARRAY | VMS_POINTER,                                         \
    .offset = vmstate_offset_pointer(_state, _field, uint8_t),                \
}

static uint64_t ls7a_reset_readw(void *opaque, hwaddr addr, unsigned width)
{
    return 0;
}

static void ls7a_reset_writew(void *opaque, hwaddr addr, uint64_t val,
                              unsigned width)
{
    if (val & 1) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}

static const MemoryRegionOps ls7a_reset_ops = {
    .read = ls7a_reset_readw,
    .write = ls7a_reset_writew,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static bool vmstate_test_use_memhp(void *opaque)
{
    LS7APCIPMRegs *s = opaque;
    return s->acpi_memory_hotplug.is_enabled;
}

static const VMStateDescription vmstate_memhp_state = {
    .name = "ls7a_pm/memhp",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .needed = vmstate_test_use_memhp,
    .fields = (VMStateField[]){ VMSTATE_MEMORY_HOTPLUG(acpi_memory_hotplug,
                                                       LS7APCIPMRegs),
                                VMSTATE_END_OF_LIST() }
};

static const VMStateDescription vmstate_cpuhp_state = {
    .name = "ls7a_pm/cpuhp",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields =
        (VMStateField[]){ VMSTATE_CPU_HOTPLUG(cpuhp_state, LS7APCIPMRegs),
                          VMSTATE_END_OF_LIST() }
};

const VMStateDescription vmstate_ls7a_pm = {
    .name = "ls7a_pm",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (VMStateField[]){
            VMSTATE_UINT16(acpi_regs.pm1.evt.sts, LS7APCIPMRegs),
            VMSTATE_UINT16(acpi_regs.pm1.evt.en, LS7APCIPMRegs),
            VMSTATE_UINT16(acpi_regs.pm1.cnt.cnt, LS7APCIPMRegs),
            VMSTATE_TIMER_PTR(acpi_regs.tmr.timer, LS7APCIPMRegs),
            VMSTATE_INT64(acpi_regs.tmr.overflow_time, LS7APCIPMRegs),
            VMSTATE_GPE_ARRAY(acpi_regs.gpe.sts, LS7APCIPMRegs),
            VMSTATE_GPE_ARRAY(acpi_regs.gpe.en, LS7APCIPMRegs),
            VMSTATE_END_OF_LIST() },
    .subsections = (const VMStateDescription *[]){ &vmstate_memhp_state,
                                                   &vmstate_cpuhp_state, NULL }
};

static inline int64_t acpi_pm_tmr_get_clock(void)
{
    return muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL), PM_TIMER_FREQUENCY,
                    NANOSECONDS_PER_SECOND);
}

static uint32_t acpi_pm_tmr_get(ACPIREGS *ar)
{
    uint32_t d = acpi_pm_tmr_get_clock();
    return d & 0xffffff;
}

static void acpi_pm_tmr_timer(void *opaque)
{
    ACPIREGS *ar = opaque;
    qemu_system_wakeup_request(QEMU_WAKEUP_REASON_PMTIMER, NULL);
    ar->tmr.update_sci(ar);
}

static uint64_t acpi_pm_tmr_read(void *opaque, hwaddr addr, unsigned width)
{
    return acpi_pm_tmr_get(opaque);
}

static void acpi_pm_tmr_write(void *opaque, hwaddr addr, uint64_t val,
                              unsigned width)
{
    /* nothing */
}

static const MemoryRegionOps acpi_pm_tmr_ops = {
    .read = acpi_pm_tmr_read,
    .write = acpi_pm_tmr_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void ls7a_pm_tmr_init(ACPIREGS *ar, acpi_update_sci_fn update_sci,
                             MemoryRegion *parent, uint64_t offset)
{
    ar->tmr.update_sci = update_sci;
    ar->tmr.timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, acpi_pm_tmr_timer, ar);
    memory_region_init_io(&ar->tmr.io, memory_region_owner(parent),
                          &acpi_pm_tmr_ops, ar, "acpi-tmr", 4);
    memory_region_add_subregion(parent, offset, &ar->tmr.io);
}

static void acpi_pm1_evt_write_sts(ACPIREGS *ar, uint16_t val)
{
    uint16_t pm1_sts = acpi_pm1_evt_get_sts(ar);
    if (pm1_sts & val & ACPI_BITMASK_TIMER_STATUS) {
        /* if TMRSTS is reset, then compute the new overflow time */
        acpi_pm_tmr_calc_overflow_time(ar);
    }
    ar->pm1.evt.sts &= ~val;
}

static uint64_t acpi_pm_evt_read(void *opaque, hwaddr addr, unsigned width)
{
    ACPIREGS *ar = opaque;
    switch (addr) {
    case 0:
        return acpi_pm1_evt_get_sts(ar);
    case 4:
        return ar->pm1.evt.en;
    default:
        return 0;
    }
}

static void acpi_pm1_evt_write_en(ACPIREGS *ar, uint16_t val)
{
    ar->pm1.evt.en = val;
    qemu_system_wakeup_enable(QEMU_WAKEUP_REASON_RTC,
                              val & ACPI_BITMASK_RT_CLOCK_ENABLE);
    qemu_system_wakeup_enable(QEMU_WAKEUP_REASON_PMTIMER,
                              val & ACPI_BITMASK_TIMER_ENABLE);
}

static void acpi_pm_evt_write(void *opaque, hwaddr addr, uint64_t val,
                              unsigned width)
{
    ACPIREGS *ar = opaque;
    switch (addr) {
    case 0:
        acpi_pm1_evt_write_sts(ar, val);
        ar->pm1.evt.update_sci(ar);
        break;
    case 4:
        acpi_pm1_evt_write_en(ar, val);
        ar->pm1.evt.update_sci(ar);
        break;
    default:
        break;
    }
}

static const MemoryRegionOps acpi_pm_evt_ops = {
    .read = acpi_pm_evt_read,
    .write = acpi_pm_evt_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void ls7a_pm1_evt_init(ACPIREGS *ar, acpi_update_sci_fn update_sci,
                              MemoryRegion *parent, uint64_t offset)
{
    ar->pm1.evt.update_sci = update_sci;
    memory_region_init_io(&ar->pm1.evt.io, memory_region_owner(parent),
                          &acpi_pm_evt_ops, ar, "acpi-evt", 8);
    memory_region_add_subregion(parent, offset, &ar->pm1.evt.io);
}

static uint64_t acpi_pm_cnt_read(void *opaque, hwaddr addr, unsigned width)
{
    ACPIREGS *ar = opaque;
    return ar->pm1.cnt.cnt;
}

/* ACPI PM1aCNT */
static void acpi_pm1_cnt_write(ACPIREGS *ar, uint16_t val)
{
    ar->pm1.cnt.cnt = val & ~(ACPI_BITMASK_SLEEP_ENABLE);
    if (val & ACPI_BITMASK_SLEEP_ENABLE) {
        /* change suspend type */
        uint16_t sus_typ = (val >> 10) & 7;
        switch (sus_typ) {
        /* s3,s4 not support */
        case 5:
        case 6:
            warn_report("acpi s3,s4 state not support");
            break;
        /* s5: soft off */
        case 7:
            qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
            break;
        default:
            break;
        }
    }
}

static void acpi_pm_cnt_write(void *opaque, hwaddr addr, uint64_t val,
                              unsigned width)
{
    acpi_pm1_cnt_write(opaque, val);
}

static const MemoryRegionOps acpi_pm_cnt_ops = {
    .read = acpi_pm_cnt_read,
    .write = acpi_pm_cnt_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void acpi_notify_wakeup(Notifier *notifier, void *data)
{
    ACPIREGS *ar = container_of(notifier, ACPIREGS, wakeup);
    WakeupReason *reason = data;

    switch (*reason) {
    case QEMU_WAKEUP_REASON_RTC:
        ar->pm1.evt.sts |=
            (ACPI_BITMASK_WAKE_STATUS | ACPI_BITMASK_RT_CLOCK_STATUS);
        break;
    case QEMU_WAKEUP_REASON_PMTIMER:
        ar->pm1.evt.sts |=
            (ACPI_BITMASK_WAKE_STATUS | ACPI_BITMASK_TIMER_STATUS);
        break;
    case QEMU_WAKEUP_REASON_OTHER:
        /*
         * ACPI_BITMASK_WAKE_STATUS should be set on resume.
         * Pretend that resume was caused by power button
         */
        ar->pm1.evt.sts |=
            (ACPI_BITMASK_WAKE_STATUS | ACPI_BITMASK_POWER_BUTTON_STATUS);
        break;
    default:
        break;
    }
}

static void ls7a_pm1_cnt_init(ACPIREGS *ar, MemoryRegion *parent,
                              bool disable_s3, bool disable_s4, uint8_t s4_val,
                              uint64_t offset)
{
    FWCfgState *fw_cfg;

    ar->pm1.cnt.s4_val = s4_val;
    ar->wakeup.notify = acpi_notify_wakeup;
    qemu_register_wakeup_notifier(&ar->wakeup);
    memory_region_init_io(&ar->pm1.cnt.io, memory_region_owner(parent),
                          &acpi_pm_cnt_ops, ar, "acpi-cnt", 4);
    memory_region_add_subregion(parent, offset, &ar->pm1.cnt.io);

    fw_cfg = fw_cfg_find();
    if (fw_cfg) {
        uint8_t suspend[6] = { 128, 0, 0, 129, 128, 128 };
        suspend[3] = 1 | ((!disable_s3) << 7);
        suspend[4] = s4_val | ((!disable_s4) << 7);
        fw_cfg_add_file(fw_cfg, "etc/system-states", g_memdup(suspend, 6), 6);
    }
}

static void ls7a_pm_reset(void *opaque)
{
    LS7APCIPMRegs *pm = opaque;

    acpi_pm1_evt_reset(&pm->acpi_regs);
    acpi_pm1_cnt_reset(&pm->acpi_regs);
    acpi_pm_tmr_reset(&pm->acpi_regs);
    acpi_gpe_reset(&pm->acpi_regs);

    acpi_update_sci(&pm->acpi_regs, pm->irq);
}

static void pm_powerdown_req(Notifier *n, void *opaque)
{
    LS7APCIPMRegs *pm = container_of(n, LS7APCIPMRegs, powerdown_notifier);

    acpi_pm1_evt_power_down(&pm->acpi_regs);
}

void ls7a_pm_init(LS7APCIPMRegs *pm, qemu_irq *pic)
{
    unsigned long base, gpe_len, acpi_aci_irq;

    /*
     * ls7a board acpi hardware info, including
     * acpi system io base address
     * acpi gpe length
     * acpi sci irq number
     */
    base = ACPI_IO_BASE;
    gpe_len = ACPI_GPE0_LEN;
    acpi_aci_irq = ACPI_SCI_IRQ;

    pm->irq = pic[acpi_aci_irq - 64];
    memory_region_init(&pm->iomem, NULL, "ls7a_pm", ACPI_IO_SIZE);
    memory_region_add_subregion(get_system_memory(), base, &pm->iomem);

    cpu_hotplug_hw_init(get_system_memory(), NULL, &pm->cpuhp_state,
                        CPU_HOTPLUG_BASE);

    ls7a_pm_tmr_init(&pm->acpi_regs, ls7a_pm_update_sci_fn, &pm->iomem,
                     LS7A_PM_TMR_BLK);
    ls7a_pm1_evt_init(&pm->acpi_regs, ls7a_pm_update_sci_fn, &pm->iomem,
                      LS7A_PM_EVT_BLK);
    ls7a_pm1_cnt_init(&pm->acpi_regs, &pm->iomem, false, false, 2,
                      LS7A_PM_CNT_BLK);

    acpi_gpe_init(&pm->acpi_regs, gpe_len);
    memory_region_init_io(&pm->iomem_gpe, NULL, &ls7a_gpe_ops, pm, "acpi-gpe0",
                          gpe_len);
    memory_region_add_subregion(&pm->iomem, LS7A_GPE0_STS_REG, &pm->iomem_gpe);

    memory_region_init_io(&pm->iomem_reset, NULL, &ls7a_reset_ops, pm,
                          "acpi-reset", 4);
    memory_region_add_subregion(&pm->iomem, LS7A_GPE0_RESET_REG,
                                &pm->iomem_reset);

    qemu_register_reset(ls7a_pm_reset, pm);

    pm->powerdown_notifier.notify = pm_powerdown_req;
    qemu_register_powerdown_notifier(&pm->powerdown_notifier);

    if (pm->acpi_memory_hotplug.is_enabled) {
        acpi_memory_hotplug_init(get_system_memory(), NULL,
                                 &pm->acpi_memory_hotplug,
                                 MEMORY_HOTPLUG_BASE);
    }
}

static void ls7a_pm_get_gpe0_blk(Object *obj, Visitor *v, const char *name,
                                 void *opaque, Error **errp)
{
    uint64_t value = ACPI_IO_BASE + LS7A_GPE0_STS_REG;

    visit_type_uint64(v, name, &value, errp);
}

static bool ls7a_pm_get_memory_hotplug_support(Object *obj, Error **errp)
{
    LS7APCIState *ls7a = get_ls7a_type(obj);

    return ls7a->pm.acpi_memory_hotplug.is_enabled;
}

static void ls7a_pm_set_memory_hotplug_support(Object *obj, bool value,
                                               Error **errp)
{
    LS7APCIState *ls7a = get_ls7a_type(obj);

    ls7a->pm.acpi_memory_hotplug.is_enabled = value;
}

static void ls7a_pm_get_disable_s3(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    LS7APCIPMRegs *pm = opaque;
    uint8_t value = pm->disable_s3;

    visit_type_uint8(v, name, &value, errp);
}

static void ls7a_pm_set_disable_s3(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    LS7APCIPMRegs *pm = opaque;
    Error *local_err = NULL;
    uint8_t value;

    visit_type_uint8(v, name, &value, &local_err);
    if (local_err) {
        goto out;
    }
    pm->disable_s3 = value;
out:
    error_propagate(errp, local_err);
}

static void ls7a_pm_get_disable_s4(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    LS7APCIPMRegs *pm = opaque;
    uint8_t value = pm->disable_s4;

    visit_type_uint8(v, name, &value, errp);
}

static void ls7a_pm_set_disable_s4(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    LS7APCIPMRegs *pm = opaque;
    Error *local_err = NULL;
    uint8_t value;

    visit_type_uint8(v, name, &value, &local_err);
    if (local_err) {
        goto out;
    }
    pm->disable_s4 = value;
out:
    error_propagate(errp, local_err);
}

static void ls7a_pm_get_s4_val(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    LS7APCIPMRegs *pm = opaque;
    uint8_t value = pm->s4_val;

    visit_type_uint8(v, name, &value, errp);
}

static void ls7a_pm_set_s4_val(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    LS7APCIPMRegs *pm = opaque;
    Error *local_err = NULL;
    uint8_t value;

    visit_type_uint8(v, name, &value, &local_err);
    if (local_err) {
        goto out;
    }
    pm->s4_val = value;
out:
    error_propagate(errp, local_err);
}

void ls7a_pm_add_properties(Object *obj, LS7APCIPMRegs *pm, Error **errp)
{
    static const uint32_t gpe0_len = ACPI_GPE0_LEN;
    pm->acpi_memory_hotplug.is_enabled = true;
    pm->disable_s3 = 0;
    pm->disable_s4 = 0;
    pm->s4_val = 2;

    object_property_add_uint32_ptr(obj, ACPI_PM_PROP_PM_IO_BASE,
                                   &pm->pm_io_base, OBJ_PROP_FLAG_READ);
    object_property_add(obj, ACPI_PM_PROP_GPE0_BLK, "uint32",
                        ls7a_pm_get_gpe0_blk, NULL, NULL, pm);
    object_property_add_uint32_ptr(obj, ACPI_PM_PROP_GPE0_BLK_LEN, &gpe0_len,
                                   OBJ_PROP_FLAG_READ);
    object_property_add_bool(obj, "memory-hotplug-support",
                             ls7a_pm_get_memory_hotplug_support,
                             ls7a_pm_set_memory_hotplug_support);
    object_property_add(obj, ACPI_PM_PROP_S3_DISABLED, "uint8",
                        ls7a_pm_get_disable_s3, ls7a_pm_set_disable_s3, NULL,
                        pm);
    object_property_add(obj, ACPI_PM_PROP_S4_DISABLED, "uint8",
                        ls7a_pm_get_disable_s4, ls7a_pm_set_disable_s4, NULL,
                        pm);
    object_property_add(obj, ACPI_PM_PROP_S4_VAL, "uint8", ls7a_pm_get_s4_val,
                        ls7a_pm_set_s4_val, NULL, pm);
}

void ls7a_pm_device_plug_cb(HotplugHandler *hotplug_dev, DeviceState *dev,
                            Error **errp)
{
    LS7APCIState *ls7a = get_ls7a_type(OBJECT(hotplug_dev));

    if (ls7a->pm.acpi_memory_hotplug.is_enabled &&
        object_dynamic_cast(OBJECT(dev), TYPE_PC_DIMM)) {
        if (object_dynamic_cast(OBJECT(dev), TYPE_NVDIMM)) {
            nvdimm_acpi_plug_cb(hotplug_dev, dev);
        } else {
            acpi_memory_plug_cb(hotplug_dev, &ls7a->pm.acpi_memory_hotplug,
                                dev, errp);
        }
    } else if (object_dynamic_cast(OBJECT(dev), TYPE_CPU)) {
        acpi_cpu_plug_cb(hotplug_dev, &ls7a->pm.cpuhp_state, dev, errp);
    } else {
        error_setg(errp,
                   "acpi: device plug request for not supported device"
                   " type: %s",
                   object_get_typename(OBJECT(dev)));
    }
}

void ls7a_pm_device_unplug_request_cb(HotplugHandler *hotplug_dev,
                                      DeviceState *dev, Error **errp)
{
    LS7APCIState *ls7a = get_ls7a_type(OBJECT(hotplug_dev));

    if (ls7a->pm.acpi_memory_hotplug.is_enabled &&
        object_dynamic_cast(OBJECT(dev), TYPE_PC_DIMM)) {
        acpi_memory_unplug_request_cb(
            hotplug_dev, &ls7a->pm.acpi_memory_hotplug, dev, errp);
    } else if (object_dynamic_cast(OBJECT(dev), TYPE_CPU)) {
        acpi_cpu_unplug_request_cb(hotplug_dev, &ls7a->pm.cpuhp_state, dev,
                                   errp);
    } else {
        error_setg(errp,
                   "acpi: device unplug request for not supported device"
                   " type: %s",
                   object_get_typename(OBJECT(dev)));
    }
}

void ls7a_pm_device_unplug_cb(HotplugHandler *hotplug_dev, DeviceState *dev,
                              Error **errp)
{
    LS7APCIState *ls7a = get_ls7a_type(OBJECT(hotplug_dev));

    if (ls7a->pm.acpi_memory_hotplug.is_enabled &&
        object_dynamic_cast(OBJECT(dev), TYPE_PC_DIMM)) {
        acpi_memory_unplug_cb(&ls7a->pm.acpi_memory_hotplug, dev, errp);
    } else if (object_dynamic_cast(OBJECT(dev), TYPE_CPU)) {
        acpi_cpu_unplug_cb(&ls7a->pm.cpuhp_state, dev, errp);
    } else {
        error_setg(errp,
                   "acpi: device unplug for not supported device"
                   " type: %s",
                   object_get_typename(OBJECT(dev)));
    }
}

void ls7a_pm_ospm_status(AcpiDeviceIf *adev, ACPIOSTInfoList ***list)
{
    LS7APCIState *ls7a = get_ls7a_type(OBJECT(adev));

    acpi_memory_ospm_status(&ls7a->pm.acpi_memory_hotplug, list);
    acpi_cpu_ospm_status(&ls7a->pm.cpuhp_state, list);
}

void ls7a_send_gpe(AcpiDeviceIf *adev, AcpiEventStatusBits ev)
{
    LS7APCIState *ls7a = get_ls7a_type(OBJECT(adev));

    acpi_send_gpe_event(&ls7a->pm.acpi_regs, ls7a->pm.irq, ev);
}
