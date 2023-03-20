/*
 * LOONGARCH IPI support
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

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/loongarch/cpudevs.h"
#include "sysemu/sysemu.h"
#include "sysemu/cpus.h"
#include "sysemu/kvm.h"
#include "hw/core/cpu.h"
#include "qemu/log.h"
#include "hw/loongarch/bios.h"
#include "elf.h"
#include "linux/kvm.h"
#include "hw/loongarch/larch.h"
#include "hw/loongarch/ls7a.h"
#include "migration/vmstate.h"

static int gipi_pre_save(void *opaque)
{
#ifdef CONFIG_KVM
    gipiState *state = opaque;
    struct loongarch_gipiState *kstate;
    struct loongarch_kvm_irqchip *chip;
    int ret, i, j, length;
#endif

    if ((!kvm_enabled()) || (!kvm_irqchip_in_kernel())) {
        return 0;
    }

#ifdef CONFIG_KVM
    length = sizeof(struct loongarch_kvm_irqchip) +
             sizeof(struct loongarch_gipiState);
    chip = g_malloc0(length);
    memset(chip, 0, length);
    chip->chip_id = KVM_IRQCHIP_LS3A_GIPI;
    chip->len = length;
    ret = kvm_vm_ioctl(kvm_state, KVM_GET_IRQCHIP, chip);
    if (ret < 0) {
        fprintf(stderr, "KVM_GET_IRQCHIP failed: %s\n", strerror(ret));
        abort();
    }

    kstate = (struct loongarch_gipiState *)chip->data;

    for (i = 0; i < MAX_GIPI_CORE_NUM; i++) {
        state->core[i].status = kstate->core[i].status;
        state->core[i].en = kstate->core[i].en;
        state->core[i].set = kstate->core[i].set;
        state->core[i].clear = kstate->core[i].clear;
        for (j = 0; j < MAX_GIPI_MBX_NUM; j++) {
            state->core[i].buf[j] = kstate->core[i].buf[j];
        }
    }
    g_free(chip);
#endif

    return 0;
}

static int gipi_post_load(void *opaque, int version)
{
#ifdef CONFIG_KVM
    gipiState *state = opaque;
    struct loongarch_gipiState *kstate;
    struct loongarch_kvm_irqchip *chip;
    int ret, i, j, length;
#endif

    if ((!kvm_enabled()) || (!kvm_irqchip_in_kernel())) {
        return 0;
    }

#ifdef CONFIG_KVM
    length = sizeof(struct loongarch_kvm_irqchip) +
             sizeof(struct loongarch_gipiState);
    chip = g_malloc0(length);
    memset(chip, 0, length);
    chip->chip_id = KVM_IRQCHIP_LS3A_GIPI;
    chip->len = length;
    kstate = (struct loongarch_gipiState *)chip->data;

    for (i = 0; i < MAX_GIPI_CORE_NUM; i++) {
        kstate->core[i].status = state->core[i].status;
        kstate->core[i].en = state->core[i].en;
        kstate->core[i].set = state->core[i].set;
        kstate->core[i].clear = state->core[i].clear;
        for (j = 0; j < MAX_GIPI_MBX_NUM; j++) {
            kstate->core[i].buf[j] = state->core[i].buf[j];
        }
    }

    ret = kvm_vm_ioctl(kvm_state, KVM_SET_IRQCHIP, chip);
    if (ret < 0) {
        fprintf(stderr, "KVM_GET_IRQCHIP failed: %s\n", strerror(ret));
        abort();
    }
    g_free(chip);
#endif
    return 0;
}

static const VMStateDescription vmstate_gipi_core = {
    .name = "gipi-single",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields =
        (VMStateField[]){
            VMSTATE_UINT32(status, gipi_core), VMSTATE_UINT32(en, gipi_core),
            VMSTATE_UINT32(set, gipi_core), VMSTATE_UINT32(clear, gipi_core),
            VMSTATE_UINT64_ARRAY(buf, gipi_core, MAX_GIPI_MBX_NUM),
            VMSTATE_END_OF_LIST() }
};

static const VMStateDescription vmstate_gipi = {
    .name = "gipi",
    .pre_save = gipi_pre_save,
    .post_load = gipi_post_load,
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]){ VMSTATE_STRUCT_ARRAY(
                                    core, gipiState, MAX_GIPI_CORE_NUM, 0,
                                    vmstate_gipi_core, gipi_core),
                                VMSTATE_END_OF_LIST() }
};

static void gipi_writel(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    gipi_core *s = opaque;
    gipi_core *ss;
    void *pbuf;
    uint32_t cpu, action_data, mailaddr;
    LoongarchMachineState *ms = LoongarchMACHINE(qdev_get_machine());

    if ((size != 4) && (size != 8)) {
        hw_error("size not 4 and not 8");
    }
    addr &= 0xff;
    switch (addr) {
    case CORE0_STATUS_OFF:
        hw_error("CORE0_STATUS_OFF Can't be write\n");
        break;
    case CORE0_EN_OFF:
        s->en = val;
        break;
    case CORE0_IPI_SEND:
        cpu = (val >> 16) & 0x3ff;
        action_data = 1UL << (val & 0x1f);
        ss = &ms->gipi->core[cpu];
        ss->status |= action_data;
        if (ss->status != 0) {
            qemu_irq_raise(ss->irq);
        }
        break;
    case CORE0_MAIL_SEND:
        cpu = (val >> 16) & 0x3ff;
        mailaddr = (val >> 2) & 0x7;
        ss = &ms->gipi->core[cpu];
        pbuf = (void *)ss->buf + mailaddr * 4;
        *(unsigned int *)pbuf = (val >> 32);
        break;
    case CORE0_SET_OFF:
        hw_error("CORE0_SET_OFF Can't be write\n");
        break;
    case CORE0_CLEAR_OFF:
        s->status ^= val;
        if (s->status == 0) {
            qemu_irq_lower(s->irq);
        }
        break;
    case 0x20 ... 0x3c:
        pbuf = (void *)s->buf + (addr - 0x20);
        if (size == 1) {
            *(unsigned char *)pbuf = (unsigned char)val;
        } else if (size == 2) {
            *(unsigned short *)pbuf = (unsigned short)val;
        } else if (size == 4) {
            *(unsigned int *)pbuf = (unsigned int)val;
        } else if (size == 8) {
            *(unsigned long *)pbuf = (unsigned long)val;
        }
        break;
    default:
        break;
    }
}

static uint64_t gipi_readl(void *opaque, hwaddr addr, unsigned size)
{
    gipi_core *s = opaque;
    uint64_t ret = 0;
    void *pbuf;

    addr &= 0xff;
    if ((size != 4) && (size != 8)) {
        hw_error("size not 4 and not 8 size:%d\n", size);
    }
    switch (addr) {
    case CORE0_STATUS_OFF:
        ret = s->status;
        break;
    case CORE0_EN_OFF:
        ret = s->en;
        break;
    case CORE0_SET_OFF:
        ret = 0;
        break;
    case CORE0_CLEAR_OFF:
        ret = 0;
        break;
    case 0x20 ... 0x3c:
        pbuf = (void *)s->buf + (addr - 0x20);
        if (size == 1) {
            ret = *(unsigned char *)pbuf;
        } else if (size == 2) {
            ret = *(unsigned short *)pbuf;
        } else if (size == 4) {
            ret = *(unsigned int *)pbuf;
        } else if (size == 8) {
            ret = *(unsigned long *)pbuf;
        }
        break;
    default:
        break;
    }

    return ret;
}

static const MemoryRegionOps gipi_ops = {
    .read = gipi_readl,
    .write = gipi_writel,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

int cpu_init_ipi(LoongarchMachineState *ms, qemu_irq parent, int cpu)
{
    hwaddr addr;
    MemoryRegion *region;
    char str[32];

    if (ms->gipi == NULL) {
        ms->gipi = g_malloc0(sizeof(gipiState));
        vmstate_register(NULL, 0, &vmstate_gipi, ms->gipi);
    }

    ms->gipi->core[cpu].irq = parent;

    addr = SMP_GIPI_MAILBOX | (cpu << 8);
    region = g_new(MemoryRegion, 1);
    sprintf(str, "gipi%d", cpu);
    memory_region_init_io(region, NULL, &gipi_ops, &ms->gipi->core[cpu], str,
                          0x100);
    memory_region_add_subregion(get_system_memory(), addr, region);
    return 0;
}
