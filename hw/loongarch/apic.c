/*
 * Loongarch 3A5000 interrupt controller emulation
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
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/irq.h"
#include "hw/loongarch/cpudevs.h"
#include "hw/sysbus.h"
#include "qemu/host-utils.h"
#include "qemu/error-report.h"
#include "sysemu/kvm.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "target/loongarch64/cpu.h"
#include "exec/address-spaces.h"
#include "hw/loongarch/larch.h"
#include "migration/vmstate.h"

#define DEBUG_APIC 0

#define DPRINTF(fmt, ...)                                                     \
    do {                                                                      \
        if (DEBUG_APIC) {                                                     \
            fprintf(stderr, "APIC: " fmt, ##__VA_ARGS__);                     \
        }                                                                     \
    } while (0)

#define APIC_OFFSET 0x400
#define APIC_BASE (0x1f010000ULL)
#define EXTIOI_NODETYPE_START (0x4a0 - APIC_OFFSET)
#define EXTIOI_NODETYPE_END (0x4c0 - APIC_OFFSET)
#define EXTIOI_IPMAP_START (0x4c0 - APIC_OFFSET)
#define EXTIOI_IPMAP_END (0x4c8 - APIC_OFFSET)
#define EXTIOI_ENABLE_START (0x600 - APIC_OFFSET)
#define EXTIOI_ENABLE_END (0x620 - APIC_OFFSET)
#define EXTIOI_BOUNCE_START (0x680 - APIC_OFFSET)
#define EXTIOI_BOUNCE_END (0x6a0 - APIC_OFFSET)
#define EXTIOI_ISR_START (0x700 - APIC_OFFSET)
#define EXTIOI_ISR_END (0x720 - APIC_OFFSET)
#define EXTIOI_COREMAP_START (0xC00 - APIC_OFFSET)
#define EXTIOI_COREMAP_END (0xD00 - APIC_OFFSET)
#define EXTIOI_COREISR_START (0x10000)
#define EXTIOI_COREISR_END (EXTIOI_COREISR_START + 0x10000)

static int ext_irq_pre_save(void *opaque)
{
#ifdef CONFIG_KVM
    apicState *apic = opaque;
    struct loongarch_kvm_irqchip *chip;
    struct kvm_loongarch_ls3a_extirq_state *kstate;
    int ret, length, i, vcpuid;
#endif
    if ((!kvm_enabled()) || (!kvm_irqchip_in_kernel())) {
        return 0;
    }
#ifdef CONFIG_KVM
    length = sizeof(struct loongarch_kvm_irqchip) +
             sizeof(struct kvm_loongarch_ls3a_extirq_state);
    chip = g_malloc0(length);
    memset(chip, 0, length);
    chip->chip_id = KVM_IRQCHIP_LS3A_EXTIRQ;
    chip->len = length;

    ret = kvm_vm_ioctl(kvm_state, KVM_GET_IRQCHIP, chip);
    if (ret < 0) {
        fprintf(stderr, "KVM_GET_IRQCHIP failed: %s\n", strerror(ret));
        abort();
    }

    kstate = (struct kvm_loongarch_ls3a_extirq_state *)chip->data;
    for (i = 0; i < EXTIOI_IRQS_BITMAP_SIZE; i++) {
        apic->ext_en[i] = kstate->ext_en_r.reg_u8[i];
        apic->ext_bounce[i] = kstate->bounce_r.reg_u8[i];
        apic->ext_isr[i] = kstate->ext_isr_r.reg_u8[i];
        for (vcpuid = 0; vcpuid < MAX_CORES; vcpuid++) {
            apic->ext_coreisr[vcpuid][i] =
                kstate->ext_core_isr_r.reg_u8[vcpuid][i];
        }
    }
    for (i = 0; i < EXTIOI_IRQS_IPMAP_SIZE; i++) {
        apic->ext_ipmap[i] = kstate->ip_map_r.reg_u8[i];
    }
    for (i = 0; i < EXTIOI_IRQS; i++) {
        apic->ext_coremap[i] = kstate->core_map_r.reg_u8[i];
    }
    for (i = 0; i < 16; i++) {
        apic->ext_nodetype[i] = kstate->node_type_r.reg_u16[i];
    }
    g_free(chip);
#endif
    return 0;
}

static int ext_irq_post_load(void *opaque, int version)
{
#ifdef CONFIG_KVM
    apicState *apic = opaque;
    struct loongarch_kvm_irqchip *chip;
    struct kvm_loongarch_ls3a_extirq_state *kstate;
    int ret, length, i, vcpuid;
#endif
    if ((!kvm_enabled()) || (!kvm_irqchip_in_kernel())) {
        return 0;
    }
#ifdef CONFIG_KVM
    length = sizeof(struct loongarch_kvm_irqchip) +
             sizeof(struct kvm_loongarch_ls3a_extirq_state);
    chip = g_malloc0(length);

    chip->chip_id = KVM_IRQCHIP_LS3A_EXTIRQ;
    chip->len = length;

    kstate = (struct kvm_loongarch_ls3a_extirq_state *)chip->data;
    for (i = 0; i < EXTIOI_IRQS_BITMAP_SIZE; i++) {
        kstate->ext_en_r.reg_u8[i] = apic->ext_en[i];
        kstate->bounce_r.reg_u8[i] = apic->ext_bounce[i];
        kstate->ext_isr_r.reg_u8[i] = apic->ext_isr[i];
        for (vcpuid = 0; vcpuid < MAX_CORES; vcpuid++) {
            kstate->ext_core_isr_r.reg_u8[vcpuid][i] =
                apic->ext_coreisr[vcpuid][i];
        }
    }
    for (i = 0; i < EXTIOI_IRQS_IPMAP_SIZE; i++) {
        kstate->ip_map_r.reg_u8[i] = apic->ext_ipmap[i];
    }
    for (i = 0; i < EXTIOI_IRQS; i++) {
        kstate->core_map_r.reg_u8[i] = apic->ext_coremap[i];
    }
    for (i = 0; i < 16; i++) {
        kstate->node_type_r.reg_u16[i] = apic->ext_nodetype[i];
    }

    ret = kvm_vm_ioctl(kvm_state, KVM_SET_IRQCHIP, chip);
    if (ret < 0) {
        fprintf(stderr, "KVM_SET_IRQCHIP failed: %s\n", strerror(ret));
        abort();
    }
    g_free(chip);
#endif
    return 0;
}

typedef struct nodeApicState {
    unsigned long addr;
    int nodeid;
    apicState *apic;
} nodeApicState;

static void ioapic_update_irq(void *opaque, int irq, int level)
{
    apicState *s = opaque;
    uint8_t ipnum, cpu, cpu_ipnum;
    unsigned long found1, found2;
    uint8_t reg_count, reg_bit;

    reg_count = irq / 32;
    reg_bit = irq % 32;

    ipnum = s->ext_sw_ipmap[irq];
    cpu = s->ext_sw_coremap[irq];
    cpu_ipnum = cpu * LS3A_INTC_IP + ipnum;
    if (level == 1) {
        if (test_bit(reg_bit, ((void *)s->ext_en + 0x4 * reg_count)) ==
            false) {
            return;
        }

        if (test_bit(reg_bit, ((void *)s->ext_isr + 0x4 * reg_count)) ==
            false) {
            return;
        }
        bitmap_set(((void *)s->ext_coreisr[cpu] + 0x4 * reg_count), reg_bit,
                   1);
        found1 =
            find_next_bit(((void *)s->ext_ipisr[cpu_ipnum] + 0x4 * reg_count),
                          EXTIOI_IRQS, 0);
        bitmap_set(((void *)s->ext_ipisr[cpu_ipnum] + 0x4 * reg_count),
                   reg_bit, 1);
        if (found1 >= EXTIOI_IRQS) {
            qemu_set_irq(s->parent_irq[cpu][ipnum], level);
        }
    } else {
        bitmap_clear(((void *)s->ext_isr + 0x4 * reg_count), reg_bit, 1);
        bitmap_clear(((void *)s->ext_coreisr[cpu] + 0x4 * reg_count), reg_bit,
                     1);
        found1 =
            find_next_bit(((void *)s->ext_ipisr[cpu_ipnum] + 0x4 * reg_count),
                          EXTIOI_IRQS, 0);
        found1 += reg_count * 32;
        bitmap_clear(((void *)s->ext_ipisr[cpu_ipnum] + 0x4 * reg_count),
                     reg_bit, 1);
        found2 =
            find_next_bit(((void *)s->ext_ipisr[cpu_ipnum] + 0x4 * reg_count),
                          EXTIOI_IRQS, 0);
        if ((found1 < EXTIOI_IRQS) && (found2 >= EXTIOI_IRQS)) {
            qemu_set_irq(s->parent_irq[cpu][ipnum], level);
        }
    }
}

static void ioapic_setirq(void *opaque, int irq, int level)
{
    apicState *s = opaque;
    uint8_t reg_count, reg_bit;

    reg_count = irq / 32;
    reg_bit = irq % 32;

    if (level) {
        bitmap_set(((void *)s->ext_isr + 0x4 * reg_count), reg_bit, 1);
    } else {
        bitmap_clear(((void *)s->ext_isr + 0x4 * reg_count), reg_bit, 1);
    }

    ioapic_update_irq(s, irq, level);
}

static uint32_t apic_readb(void *opaque, hwaddr addr)
{
    nodeApicState *node;
    apicState *state;
    unsigned long off;
    uint8_t ret;
    int cpu;

    node = (nodeApicState *)opaque;
    state = node->apic;
    off = addr & 0xfffff;
    ret = 0;
    if ((off >= EXTIOI_ENABLE_START) && (off < EXTIOI_ENABLE_END)) {
        off -= EXTIOI_ENABLE_START;
        ret = *(uint8_t *)((void *)state->ext_en + off);
    } else if ((off >= EXTIOI_BOUNCE_START) && (off < EXTIOI_BOUNCE_END)) {
        off -= EXTIOI_BOUNCE_START;
        ret = *(uint8_t *)((void *)state->ext_bounce + off);
    } else if ((off >= EXTIOI_ISR_START) && (off < EXTIOI_ISR_END)) {
        off -= EXTIOI_ISR_START;
        ret = *(uint8_t *)((void *)state->ext_isr + off);
    } else if ((off >= EXTIOI_COREISR_START) && (off < EXTIOI_COREISR_END)) {
        off -= EXTIOI_COREISR_START;
        cpu = (off >> 8) & 0xff;
        ret = *(uint8_t *)((void *)state->ext_coreisr[cpu] + (off & 0x1f));
    } else if ((off >= EXTIOI_IPMAP_START) && (off < EXTIOI_IPMAP_END)) {
        off -= EXTIOI_IPMAP_START;
        ret = *(uint8_t *)((void *)state->ext_ipmap + off);
    } else if ((off >= EXTIOI_COREMAP_START) && (off < EXTIOI_COREMAP_END)) {
        off -= EXTIOI_COREMAP_START;
        ret = *(uint8_t *)((void *)state->ext_coremap + off);
    } else if ((off >= EXTIOI_NODETYPE_START) && (off < EXTIOI_NODETYPE_END)) {
        off -= EXTIOI_NODETYPE_START;
        ret = *(uint8_t *)((void *)state->ext_nodetype + off);
    }

    DPRINTF("readb reg 0x" TARGET_FMT_plx " = %x\n", node->addr + addr, ret);
    return ret;
}

static uint32_t apic_readw(void *opaque, hwaddr addr)
{
    nodeApicState *node;
    apicState *state;
    unsigned long off;
    uint16_t ret;
    int cpu;

    node = (nodeApicState *)opaque;
    state = node->apic;
    off = addr & 0xfffff;
    ret = 0;
    if ((off >= EXTIOI_ENABLE_START) && (off < EXTIOI_ENABLE_END)) {
        off -= EXTIOI_ENABLE_START;
        ret = *(uint16_t *)((void *)state->ext_en + off);
    } else if ((off >= EXTIOI_BOUNCE_START) && (off < EXTIOI_BOUNCE_END)) {
        off -= EXTIOI_BOUNCE_START;
        ret = *(uint16_t *)((void *)state->ext_bounce + off);
    } else if ((off >= EXTIOI_ISR_START) && (off < EXTIOI_ISR_END)) {
        off -= EXTIOI_ISR_START;
        ret = *(uint16_t *)((void *)state->ext_isr + off);
    } else if ((off >= EXTIOI_COREISR_START) && (off < EXTIOI_COREISR_END)) {
        off -= EXTIOI_COREISR_START;
        cpu = (off >> 8) & 0xff;
        ret = *(uint16_t *)((void *)state->ext_coreisr[cpu] + (off & 0x1f));
    } else if ((off >= EXTIOI_IPMAP_START) && (off < EXTIOI_IPMAP_END)) {
        off -= EXTIOI_IPMAP_START;
        ret = *(uint16_t *)((void *)state->ext_ipmap + off);
    } else if ((off >= EXTIOI_COREMAP_START) && (off < EXTIOI_COREMAP_END)) {
        off -= EXTIOI_COREMAP_START;
        ret = *(uint16_t *)((void *)state->ext_coremap + off);
    } else if ((off >= EXTIOI_NODETYPE_START) && (off < EXTIOI_NODETYPE_END)) {
        off -= EXTIOI_NODETYPE_START;
        ret = *(uint16_t *)((void *)state->ext_nodetype + off);
    }

    DPRINTF("readw reg 0x" TARGET_FMT_plx " = %x\n", node->addr + addr, ret);
    return ret;
}

static uint32_t apic_readl(void *opaque, hwaddr addr)
{
    nodeApicState *node;
    apicState *state;
    unsigned long off;
    uint32_t ret;
    int cpu;

    node = (nodeApicState *)opaque;
    state = node->apic;
    off = addr & 0xfffff;
    ret = 0;
    if ((off >= EXTIOI_ENABLE_START) && (off < EXTIOI_ENABLE_END)) {
        off -= EXTIOI_ENABLE_START;
        ret = *(uint32_t *)((void *)state->ext_en + off);
    } else if ((off >= EXTIOI_BOUNCE_START) && (off < EXTIOI_BOUNCE_END)) {
        off -= EXTIOI_BOUNCE_START;
        ret = *(uint32_t *)((void *)state->ext_bounce + off);
    } else if ((off >= EXTIOI_ISR_START) && (off < EXTIOI_ISR_END)) {
        off -= EXTIOI_ISR_START;
        ret = *(uint32_t *)((void *)state->ext_isr + off);
    } else if ((off >= EXTIOI_COREISR_START) && (off < EXTIOI_COREISR_END)) {
        off -= EXTIOI_COREISR_START;
        cpu = (off >> 8) & 0xff;
        ret = *(uint32_t *)((void *)state->ext_coreisr[cpu] + (off & 0x1f));
    } else if ((off >= EXTIOI_IPMAP_START) && (off < EXTIOI_IPMAP_END)) {
        off -= EXTIOI_IPMAP_START;
        ret = *(uint32_t *)((void *)state->ext_ipmap + off);
    } else if ((off >= EXTIOI_COREMAP_START) && (off < EXTIOI_COREMAP_END)) {
        off -= EXTIOI_COREMAP_START;
        ret = *(uint32_t *)((void *)state->ext_coremap + off);
    } else if ((off >= EXTIOI_NODETYPE_START) && (off < EXTIOI_NODETYPE_END)) {
        off -= EXTIOI_NODETYPE_START;
        ret = *(uint32_t *)((void *)state->ext_nodetype + off);
    }

    DPRINTF("readl reg 0x" TARGET_FMT_plx " = %x\n", node->addr + addr, ret);
    return ret;
}

static void apic_writeb(void *opaque, hwaddr addr, uint32_t val)
{
    nodeApicState *node;
    apicState *state;
    unsigned long off;
    uint8_t old;
    int cpu, i, ipnum, level, mask;

    node = (nodeApicState *)opaque;
    state = node->apic;
    off = addr & 0xfffff;
    if ((off >= EXTIOI_ENABLE_START) && (off < EXTIOI_ENABLE_END)) {
        off -= EXTIOI_ENABLE_START;
        old = *(uint8_t *)((void *)state->ext_en + off);
        if (old != val) {
            *(uint8_t *)((void *)state->ext_en + off) = val;
            old = old ^ val;
            mask = 0x1;
            for (i = 0; i < 8; i++) {
                if (old & mask) {
                    level = !!(val & (0x1 << i));
                    ioapic_update_irq(state, i + off * 8, level);
                }
                mask = mask << 1;
            }
        }
    } else if ((off >= EXTIOI_BOUNCE_START) && (off < EXTIOI_BOUNCE_END)) {
        off -= EXTIOI_BOUNCE_START;
        *(uint8_t *)((void *)state->ext_bounce + off) = val;
    } else if ((off >= EXTIOI_ISR_START) && (off < EXTIOI_ISR_END)) {
        off -= EXTIOI_ISR_START;
        old = *(uint8_t *)((void *)state->ext_isr + off);
        *(uint8_t *)((void *)state->ext_isr + off) = old & ~val;
        mask = 0x1;
        for (i = 0; i < 8; i++) {
            if ((old & mask) && (val & mask)) {
                ioapic_update_irq(state, i + off * 8, 0);
            }
            mask = mask << 1;
        }
    } else if ((off >= EXTIOI_COREISR_START) && (off < EXTIOI_COREISR_END)) {
        off -= EXTIOI_COREISR_START;
        cpu = (off >> 8) & 0xff;
        off = off & 0x1f;
        old = *(uint8_t *)((void *)state->ext_coreisr[cpu] + off);
        *(uint8_t *)((void *)state->ext_coreisr[cpu] + off) = old & ~val;
        mask = 0x1;
        for (i = 0; i < 8; i++) {
            if ((old & mask) && (val & mask)) {
                ioapic_update_irq(state, i + off * 8, 0);
            }
            mask = mask << 1;
        }
    } else if ((off >= EXTIOI_IPMAP_START) && (off < EXTIOI_IPMAP_END)) {
        off -= EXTIOI_IPMAP_START;
        val = val & 0xf;
        *(uint8_t *)((void *)state->ext_ipmap + off) = val;
        ipnum = 0;
        for (i = 0; i < 4; i++) {
            if (val & (0x1 << i)) {
                ipnum = i;
                break;
            }
        }
        if (val) {
            for (i = 0; i < 32; i++) {
                cpu = off * 32 + i;
                state->ext_sw_ipmap[cpu] = ipnum;
            }
        }
    } else if ((off >= EXTIOI_COREMAP_START) && (off < EXTIOI_COREMAP_END)) {
        off -= EXTIOI_COREMAP_START;
        val = val & 0xff;
        *(uint8_t *)((void *)state->ext_coremap + off) = val;
        state->ext_sw_coremap[off] = val;
    } else if ((off >= EXTIOI_NODETYPE_START) && (off < EXTIOI_NODETYPE_END)) {
        off -= EXTIOI_NODETYPE_START;
        *(uint8_t *)((void *)state->ext_nodetype + off) = val;
    }

    DPRINTF("writeb reg 0x" TARGET_FMT_plx " = %x\n", node->addr + addr, val);
}

static void apic_writew(void *opaque, hwaddr addr, uint32_t val)
{
    nodeApicState *node;
    apicState *state;
    unsigned long off;
    uint16_t old;
    int cpu, i, level, mask;

    node = (nodeApicState *)opaque;
    state = node->apic;
    off = addr & 0xfffff;
    if ((off >= EXTIOI_ENABLE_START) && (off < EXTIOI_ENABLE_END)) {
        off -= EXTIOI_ENABLE_START;
        old = *(uint16_t *)((void *)state->ext_en + off);
        if (old != val) {
            *(uint16_t *)((void *)state->ext_en + off) = val;
            old = old ^ val;
            mask = 0x1;
            for (i = 0; i < 16; i++) {
                if (old & mask) {
                    level = !!(val & (0x1 << i));
                    ioapic_update_irq(state, i + off * 8, level);
                }
                mask = mask << 1;
            }
        }
    } else if ((off >= EXTIOI_BOUNCE_START) && (off < EXTIOI_BOUNCE_END)) {
        off -= EXTIOI_BOUNCE_START;
        *(uint16_t *)((void *)state->ext_bounce + off) = val;
    } else if ((off >= EXTIOI_ISR_START) && (off < EXTIOI_ISR_END)) {
        off -= EXTIOI_ISR_START;
        old = *(uint16_t *)((void *)state->ext_isr + off);
        *(uint16_t *)((void *)state->ext_isr + off) = old & ~val;
        mask = 0x1;
        for (i = 0; i < 16; i++) {
            if ((old & mask) && (val & mask)) {
                ioapic_update_irq(state, i + off * 8, 0);
            }
            mask = mask << 1;
        }
    } else if ((off >= EXTIOI_COREISR_START) && (off < EXTIOI_COREISR_END)) {
        off -= EXTIOI_COREISR_START;
        cpu = (off >> 8) & 0xff;
        off = off & 0x1f;
        old = *(uint16_t *)((void *)state->ext_coreisr[cpu] + off);
        *(uint16_t *)((void *)state->ext_coreisr[cpu] + off) = old & ~val;
        mask = 0x1;
        for (i = 0; i < 16; i++) {
            if ((old & mask) && (val & mask)) {
                ioapic_update_irq(state, i + off * 8, 0);
            }
            mask = mask << 1;
        }
    } else if ((off >= EXTIOI_IPMAP_START) && (off < EXTIOI_IPMAP_END)) {
        apic_writeb(opaque, addr, val & 0xff);
        apic_writeb(opaque, addr + 1, (val >> 8) & 0xff);

    } else if ((off >= EXTIOI_COREMAP_START) && (off < EXTIOI_COREMAP_END)) {
        apic_writeb(opaque, addr, val & 0xff);
        apic_writeb(opaque, addr + 1, (val >> 8) & 0xff);

    } else if ((off >= EXTIOI_NODETYPE_START) && (off < EXTIOI_NODETYPE_END)) {
        off -= EXTIOI_NODETYPE_START;
        *(uint16_t *)((void *)state->ext_nodetype + off) = val;
    }

    DPRINTF("writew reg 0x" TARGET_FMT_plx " = %x\n", node->addr + addr, val);
}

static void apic_writel(void *opaque, hwaddr addr, uint32_t val)
{
    nodeApicState *node;
    apicState *state;
    unsigned long off;
    uint32_t old;
    int cpu, i, level, mask;

    node = (nodeApicState *)opaque;
    state = node->apic;
    off = addr & 0xfffff;
    if ((off >= EXTIOI_ENABLE_START) && (off < EXTIOI_ENABLE_END)) {
        off -= EXTIOI_ENABLE_START;
        old = *(uint32_t *)((void *)state->ext_en + off);
        if (old != val) {
            *(uint32_t *)((void *)state->ext_en + off) = val;
            old = old ^ val;
            mask = 0x1;
            for (i = 0; i < 32; i++) {
                if (old & mask) {
                    level = !!(val & (0x1 << i));
                    ioapic_update_irq(state, i + off * 8, level);
                }
                mask = mask << 1;
            }
        }
    } else if ((off >= EXTIOI_BOUNCE_START) && (off < EXTIOI_BOUNCE_END)) {
        off -= EXTIOI_BOUNCE_START;
        *(uint32_t *)((void *)state->ext_bounce + off) = val;
    } else if ((off >= EXTIOI_ISR_START) && (off < EXTIOI_ISR_END)) {
        off -= EXTIOI_ISR_START;
        old = *(uint32_t *)((void *)state->ext_isr + off);
        *(uint32_t *)((void *)state->ext_isr + off) = old & ~val;
        mask = 0x1;
        for (i = 0; i < 32; i++) {
            if ((old & mask) && (val & mask)) {
                ioapic_update_irq(state, i + off * 8, 0);
            }
            mask = mask << 1;
        }
    } else if ((off >= EXTIOI_COREISR_START) && (off < EXTIOI_COREISR_END)) {
        off -= EXTIOI_COREISR_START;
        cpu = (off >> 8) & 0xff;
        off = off & 0x1f;
        old = *(uint32_t *)((void *)state->ext_coreisr[cpu] + off);
        *(uint32_t *)((void *)state->ext_coreisr[cpu] + off) = old & ~val;
        mask = 0x1;
        for (i = 0; i < 32; i++) {
            if ((old & mask) && (val & mask)) {
                ioapic_update_irq(state, i + off * 8, 0);
            }
            mask = mask << 1;
        }
    } else if ((off >= EXTIOI_IPMAP_START) && (off < EXTIOI_IPMAP_END)) {
        apic_writeb(opaque, addr, val & 0xff);
        apic_writeb(opaque, addr + 1, (val >> 8) & 0xff);
        apic_writeb(opaque, addr + 2, (val >> 16) & 0xff);
        apic_writeb(opaque, addr + 3, (val >> 24) & 0xff);

    } else if ((off >= EXTIOI_COREMAP_START) && (off < EXTIOI_COREMAP_END)) {
        apic_writeb(opaque, addr, val & 0xff);
        apic_writeb(opaque, addr + 1, (val >> 8) & 0xff);
        apic_writeb(opaque, addr + 2, (val >> 16) & 0xff);
        apic_writeb(opaque, addr + 3, (val >> 24) & 0xff);

    } else if ((off >= EXTIOI_NODETYPE_START) && (off < EXTIOI_NODETYPE_END)) {
        off -= EXTIOI_NODETYPE_START;
        *(uint32_t *)((void *)state->ext_nodetype + off) = val;
    }

    DPRINTF("writel reg 0x" TARGET_FMT_plx " = %x\n", node->addr + addr, val);
}

static uint64_t apic_readfn(void *opaque, hwaddr addr, unsigned size)
{
    switch (size) {
    case 1:
        return apic_readb(opaque, addr);
    case 2:
        return apic_readw(opaque, addr);
    case 4:
        return apic_readl(opaque, addr);
    default:
        g_assert_not_reached();
    }
}

static void apic_writefn(void *opaque, hwaddr addr, uint64_t value,
                         unsigned size)
{
    switch (size) {
    case 1:
        apic_writeb(opaque, addr, value);
        break;
    case 2:
        apic_writew(opaque, addr, value);
        break;
    case 4:
        apic_writel(opaque, addr, value);
        break;
    default:
        g_assert_not_reached();
    }
}

static const VMStateDescription vmstate_apic = {
    .name = "apic",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_save = ext_irq_pre_save,
    .post_load = ext_irq_post_load,
    .fields =
        (VMStateField[]){
            VMSTATE_UINT8_ARRAY(ext_en, apicState, EXTIOI_IRQS_BITMAP_SIZE),
            VMSTATE_UINT8_ARRAY(ext_bounce, apicState,
                                EXTIOI_IRQS_BITMAP_SIZE),
            VMSTATE_UINT8_ARRAY(ext_isr, apicState, EXTIOI_IRQS_BITMAP_SIZE),
            VMSTATE_UINT8_2DARRAY(ext_coreisr, apicState, MAX_CORES,
                                  EXTIOI_IRQS_BITMAP_SIZE),
            VMSTATE_UINT8_ARRAY(ext_ipmap, apicState, EXTIOI_IRQS_IPMAP_SIZE),
            VMSTATE_UINT8_ARRAY(ext_coremap, apicState, EXTIOI_IRQS),
            VMSTATE_UINT16_ARRAY(ext_nodetype, apicState, 16),
            VMSTATE_UINT64(ext_control, apicState),
            VMSTATE_UINT8_ARRAY(ext_sw_ipmap, apicState, EXTIOI_IRQS),
            VMSTATE_UINT8_ARRAY(ext_sw_coremap, apicState, EXTIOI_IRQS),
            VMSTATE_UINT8_2DARRAY(ext_ipisr, apicState,
                                  MAX_CORES *LS3A_INTC_IP,
                                  EXTIOI_IRQS_BITMAP_SIZE),
            VMSTATE_END_OF_LIST() }
};

static const MemoryRegionOps apic_ops = {
    .read = apic_readfn,
    .write = apic_writefn,
    .impl.min_access_size = 1,
    .impl.max_access_size = 4,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

int cpu_init_apic(LoongarchMachineState *ms, CPULOONGARCHState *env, int cpu)
{
    apicState *apic;
    nodeApicState *node;
    MemoryRegion *iomem;
    unsigned long base;
    int pin;
    char str[32];

    if (ms->apic == NULL) {
        apic = g_malloc0(sizeof(apicState));
        vmstate_register(NULL, 0, &vmstate_apic, apic);
        apic->irq = qemu_allocate_irqs(ioapic_setirq, apic, EXTIOI_IRQS);

        for (pin = 0; pin < LS3A_INTC_IP; pin++) {
            /* cpu_pin[9:2] <= intc_pin[7:0] */
            apic->parent_irq[cpu][pin] = env->irq[pin + 2];
        }
        ms->apic = apic;

        if (cpu == 0) {
            base = APIC_BASE;
            node = g_malloc0(sizeof(nodeApicState));
            node->apic = ms->apic;
            node->addr = base;

            iomem = g_new(MemoryRegion, 1);
            sprintf(str, "apic%d", cpu);
            /* extioi addr 0x1f010000~0x1f02ffff */
            memory_region_init_io(iomem, NULL, &apic_ops, node, str, 0x20000);
            memory_region_add_subregion(get_system_memory(), base, iomem);
        }

    } else {
        if (cpu != 0) {
            for (pin = 0; pin < LS3A_INTC_IP; pin++) {
                ms->apic->parent_irq[cpu][pin] = env->irq[pin + 2];
            }
        }
    }
    return 0;
}
