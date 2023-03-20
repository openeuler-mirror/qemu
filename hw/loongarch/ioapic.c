/*
 * LS7A1000 Northbridge IOAPIC support
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
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "qemu/log.h"
#include "sysemu/kvm.h"
#include "linux/kvm.h"
#include "migration/vmstate.h"

#define DEBUG_LS7A_APIC 0

#define DPRINTF(fmt, ...)                                                     \
    do {                                                                      \
        if (DEBUG_LS7A_APIC) {                                                \
            fprintf(stderr, "IOAPIC: " fmt, ##__VA_ARGS__);                   \
        }                                                                     \
    } while (0)

#define TYPE_LS7A_APIC "ioapic"
#define LS7A_APIC(obj) OBJECT_CHECK(LS7AApicState, (obj), TYPE_LS7A_APIC)

#define LS7A_IOAPIC_ROUTE_ENTRY_OFFSET 0x100
#define LS7A_IOAPIC_INT_ID_OFFSET 0x00
#define LS7A_INT_ID_VAL 0x7000000UL
#define LS7A_INT_ID_VER 0x1f0001UL
#define LS7A_IOAPIC_INT_MASK_OFFSET 0x20
#define LS7A_IOAPIC_INT_EDGE_OFFSET 0x60
#define LS7A_IOAPIC_INT_CLEAR_OFFSET 0x80
#define LS7A_IOAPIC_INT_STATUS_OFFSET 0x3a0
#define LS7A_IOAPIC_INT_POL_OFFSET 0x3e0
#define LS7A_IOAPIC_HTMSI_EN_OFFSET 0x40
#define LS7A_IOAPIC_HTMSI_VEC_OFFSET 0x200
#define LS7A_AUTO_CTRL0_OFFSET 0xc0
#define LS7A_AUTO_CTRL1_OFFSET 0xe0

typedef struct LS7AApicState {
    SysBusDevice parent_obj;
    qemu_irq parent_irq[257];
    uint64_t int_id;
    uint64_t int_mask; /*0x020 interrupt mask register*/
    uint64_t htmsi_en; /*0x040 1=msi*/
    uint64_t intedge;  /*0x060 edge=1 level  =0*/
    uint64_t intclr;   /*0x080 for clean edge int,set 1 clean,set 0 is noused*/
    uint64_t auto_crtl0;      /*0x0c0*/
    uint64_t auto_crtl1;      /*0x0e0*/
    uint8_t route_entry[64];  /*0x100 - 0x140*/
    uint8_t htmsi_vector[64]; /*0x200 - 0x240*/
    uint64_t intisr_chip0;    /*0x300*/
    uint64_t intisr_chip1;    /*0x320*/
    uint64_t last_intirr;     /* edge detection */
    uint64_t intirr;          /* 0x380 interrupt request register */
    uint64_t intisr;          /* 0x3a0 interrupt service register */
    /*
     * 0x3e0 interrupt level polarity
     * selection register 0 for high level tirgger
     */
    uint64_t int_polarity;
    MemoryRegion iomem;
} LS7AApicState;

static void update_irq(LS7AApicState *s)
{
    int i;
    if ((s->intirr & (~s->int_mask)) & (~s->htmsi_en)) {
        DPRINTF("7a update irqline up\n");
        s->intisr = (s->intirr & (~s->int_mask) & (~s->htmsi_en));
        qemu_set_irq(s->parent_irq[256], 1);
    } else {
        DPRINTF("7a update irqline down\n");
        s->intisr &= (~s->htmsi_en);
        qemu_set_irq(s->parent_irq[256], 0);
    }
    if (s->htmsi_en) {
        for (i = 0; i < 64; i++) {
            if ((((~s->intisr) & s->intirr) & s->htmsi_en) & (1ULL << i)) {
                s->intisr |= 1ULL << i;
                qemu_set_irq(s->parent_irq[s->htmsi_vector[i]], 1);
            } else if (((~(s->intisr | s->intirr)) & s->htmsi_en) &
                       (1ULL << i)) {
                qemu_set_irq(s->parent_irq[s->htmsi_vector[i]], 0);
            }
        }
    }
}

static void irq_handler(void *opaque, int irq, int level)
{
    LS7AApicState *s = opaque;

    assert(irq < 64);
    uint64_t mask = 1ULL << irq;
    DPRINTF("------ %s irq %d %d\n", __func__, irq, level);

    if (s->intedge & mask) {
        /* edge triggered */
        /*TODO*/
    } else {
        /* level triggered */
        if (level) {
            s->intirr |= mask;
        } else {
            s->intirr &= ~mask;
        }
    }
    update_irq(s);
}

static uint64_t ls7a_apic_reg_read(void *opaque, hwaddr addr, unsigned size)
{
    LS7AApicState *a = opaque;
    uint64_t val = 0;
    uint64_t offset;
    int64_t offset_tmp;
    offset = addr & 0xfff;
    if (8 == size) {
        switch (offset) {
        case LS7A_IOAPIC_INT_ID_OFFSET:
            val = LS7A_INT_ID_VER;
            val = (val << 32) + LS7A_INT_ID_VAL;
            break;
        case LS7A_IOAPIC_INT_MASK_OFFSET:
            val = a->int_mask;
            break;
        case LS7A_IOAPIC_INT_STATUS_OFFSET:
            val = a->intisr & (~a->int_mask);
            break;
        case LS7A_IOAPIC_INT_EDGE_OFFSET:
            val = a->intedge;
            break;
        case LS7A_IOAPIC_INT_POL_OFFSET:
            val = a->int_polarity;
            break;
        case LS7A_IOAPIC_HTMSI_EN_OFFSET:
            val = a->htmsi_en;
            break;
        case LS7A_AUTO_CTRL0_OFFSET:
        case LS7A_AUTO_CTRL1_OFFSET:
            break;
        default:
            break;
        }
    } else if (1 == size) {
        if (offset >= LS7A_IOAPIC_HTMSI_VEC_OFFSET) {
            offset_tmp = offset - LS7A_IOAPIC_HTMSI_VEC_OFFSET;
            if (offset_tmp >= 0 && offset_tmp < 64) {
                val = a->htmsi_vector[offset_tmp];
            }
        } else if (offset >= LS7A_IOAPIC_ROUTE_ENTRY_OFFSET) {
            offset_tmp = offset - LS7A_IOAPIC_ROUTE_ENTRY_OFFSET;
            if (offset_tmp >= 0 && offset_tmp < 64) {
                val = a->route_entry[offset_tmp];
                DPRINTF("addr %lx val %lx\n", addr, val);
            }
        }
    }
    DPRINTF(TARGET_FMT_plx " val %lx\n", addr, val);
    return val;
}

static void ls7a_apic_reg_write(void *opaque, hwaddr addr, uint64_t data,
                                unsigned size)
{
    LS7AApicState *a = opaque;
    int64_t offset_tmp;
    uint64_t offset;
    offset = addr & 0xfff;
    DPRINTF(TARGET_FMT_plx " size %d val %lx\n", addr, size, data);
    if (8 == size) {
        switch (offset) {
        case LS7A_IOAPIC_INT_MASK_OFFSET:
            a->int_mask = data;
            update_irq(a);
            break;
        case LS7A_IOAPIC_INT_STATUS_OFFSET:
            a->intisr = data;
            break;
        case LS7A_IOAPIC_INT_EDGE_OFFSET:
            a->intedge = data;
            break;
        case LS7A_IOAPIC_INT_CLEAR_OFFSET:
            a->intisr &= (~data);
            update_irq(a);
            break;
        case LS7A_IOAPIC_INT_POL_OFFSET:
            a->int_polarity = data;
            break;
        case LS7A_IOAPIC_HTMSI_EN_OFFSET:
            a->htmsi_en = data;
            break;
        case LS7A_AUTO_CTRL0_OFFSET:
        case LS7A_AUTO_CTRL1_OFFSET:
            break;
        default:
            break;
        }
    } else if (1 == size) {
        if (offset >= LS7A_IOAPIC_HTMSI_VEC_OFFSET) {
            offset_tmp = offset - LS7A_IOAPIC_HTMSI_VEC_OFFSET;
            if (offset_tmp >= 0 && offset_tmp < 64) {
                a->htmsi_vector[offset_tmp] = (uint8_t)(data & 0xff);
            }
        } else if (offset >= LS7A_IOAPIC_ROUTE_ENTRY_OFFSET) {
            offset_tmp = offset - LS7A_IOAPIC_ROUTE_ENTRY_OFFSET;
            if (offset_tmp >= 0 && offset_tmp < 64) {
                a->route_entry[offset_tmp] = (uint8_t)(data & 0xff);
            }
        }
    }
}

static const MemoryRegionOps ls7a_apic_ops = {
    .read = ls7a_apic_reg_read,
    .write = ls7a_apic_reg_write,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int kvm_ls7a_pre_save(void *opaque)
{
#ifdef CONFIG_KVM
    LS7AApicState *s = opaque;
    struct loongarch_kvm_irqchip *chip;
    struct ls7a_ioapic_state *state;
    int ret, i, length;

    if ((!kvm_enabled()) || (!kvm_irqchip_in_kernel())) {
        return 0;
    }

    length = sizeof(struct loongarch_kvm_irqchip) +
             sizeof(struct ls7a_ioapic_state);
    chip = g_malloc0(length);
    memset(chip, 0, length);
    chip->chip_id = KVM_IRQCHIP_LS7A_IOAPIC;
    chip->len = length;
    ret = kvm_vm_ioctl(kvm_state, KVM_GET_IRQCHIP, chip);
    if (ret < 0) {
        fprintf(stderr, "KVM_GET_IRQCHIP failed: %s\n", strerror(ret));
        abort();
    }
    state = (struct ls7a_ioapic_state *)chip->data;
    s->int_id = state->int_id;
    s->int_mask = state->int_mask;
    s->htmsi_en = state->htmsi_en;
    s->intedge = state->intedge;
    s->intclr = state->intclr;
    s->auto_crtl0 = state->auto_crtl0;
    s->auto_crtl1 = state->auto_crtl1;
    for (i = 0; i < 64; i++) {
        s->route_entry[i] = state->route_entry[i];
        s->htmsi_vector[i] = state->htmsi_vector[i];
    }
    s->intisr_chip0 = state->intisr_chip0;
    s->intisr_chip1 = state->intisr_chip1;
    s->intirr = state->intirr;
    s->intisr = state->intisr;
    s->int_polarity = state->int_polarity;
    g_free(chip);
#endif
    return 0;
}

static int kvm_ls7a_post_load(void *opaque, int version)
{
#ifdef CONFIG_KVM
    LS7AApicState *s = opaque;
    struct loongarch_kvm_irqchip *chip;
    struct ls7a_ioapic_state *state;
    int ret, i, length;

    if ((!kvm_enabled()) || (!kvm_irqchip_in_kernel())) {
        return 0;
    }
    length = sizeof(struct loongarch_kvm_irqchip) +
             sizeof(struct ls7a_ioapic_state);
    chip = g_malloc0(length);
    memset(chip, 0, length);
    chip->chip_id = KVM_IRQCHIP_LS7A_IOAPIC;
    chip->len = length;

    state = (struct ls7a_ioapic_state *)chip->data;
    state->int_id = s->int_id;
    state->int_mask = s->int_mask;
    state->htmsi_en = s->htmsi_en;
    state->intedge = s->intedge;
    state->intclr = s->intclr;
    state->auto_crtl0 = s->auto_crtl0;
    state->auto_crtl1 = s->auto_crtl1;
    for (i = 0; i < 64; i++) {
        state->route_entry[i] = s->route_entry[i];
        state->htmsi_vector[i] = s->htmsi_vector[i];
    }
    state->intisr_chip0 = s->intisr_chip0;
    state->intisr_chip1 = s->intisr_chip1;
    state->last_intirr = 0;
    state->intirr = s->intirr;
    state->intisr = s->intisr;
    state->int_polarity = s->int_polarity;

    ret = kvm_vm_ioctl(kvm_state, KVM_SET_IRQCHIP, chip);
    if (ret < 0) {
        fprintf(stderr, "KVM_GET_IRQCHIP failed: %s\n", strerror(ret));
        abort();
    }
    g_free(chip);
#endif
    return 0;
}

static void ls7a_apic_reset(DeviceState *d)
{
    LS7AApicState *s = LS7A_APIC(d);
    int i;

    s->int_id = 0x001f000107000000;
    s->int_mask = 0xffffffffffffffff;
    s->htmsi_en = 0x0;
    s->intedge = 0x0;
    s->intclr = 0x0;
    s->auto_crtl0 = 0x0;
    s->auto_crtl1 = 0x0;
    for (i = 0; i < 64; i++) {
        s->route_entry[i] = 0x1;
        s->htmsi_vector[i] = 0x0;
    }
    s->intisr_chip0 = 0x0;
    s->intisr_chip1 = 0x0;
    s->intirr = 0x0;
    s->intisr = 0x0;
    s->int_polarity = 0x0;
    kvm_ls7a_post_load(s, 0);
}

static void ls7a_apic_init(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    LS7AApicState *s = LS7A_APIC(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    int tmp;
    memory_region_init_io(&s->iomem, obj, &ls7a_apic_ops, s, TYPE_LS7A_APIC,
                          0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    for (tmp = 0; tmp < 257; tmp++) {
        sysbus_init_irq(sbd, &s->parent_irq[tmp]);
    }
    qdev_init_gpio_in(dev, irq_handler, 64);
}

static const VMStateDescription vmstate_ls7a_apic = {
    .name = TYPE_LS7A_APIC,
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_save = kvm_ls7a_pre_save,
    .post_load = kvm_ls7a_post_load,
    .fields =
        (VMStateField[]){ VMSTATE_UINT64(int_mask, LS7AApicState),
                          VMSTATE_UINT64(htmsi_en, LS7AApicState),
                          VMSTATE_UINT64(intedge, LS7AApicState),
                          VMSTATE_UINT64(intclr, LS7AApicState),
                          VMSTATE_UINT64(auto_crtl0, LS7AApicState),
                          VMSTATE_UINT64(auto_crtl1, LS7AApicState),
                          VMSTATE_UINT8_ARRAY(route_entry, LS7AApicState, 64),
                          VMSTATE_UINT8_ARRAY(htmsi_vector, LS7AApicState, 64),
                          VMSTATE_UINT64(intisr_chip0, LS7AApicState),
                          VMSTATE_UINT64(intisr_chip1, LS7AApicState),
                          VMSTATE_UINT64(last_intirr, LS7AApicState),
                          VMSTATE_UINT64(intirr, LS7AApicState),
                          VMSTATE_UINT64(intisr, LS7AApicState),
                          VMSTATE_UINT64(int_polarity, LS7AApicState),
                          VMSTATE_END_OF_LIST() }
};

static void ls7a_apic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = ls7a_apic_reset;
    dc->vmsd = &vmstate_ls7a_apic;
}

static const TypeInfo ls7a_apic_info = {
    .name = TYPE_LS7A_APIC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(LS7AApicState),
    .instance_init = ls7a_apic_init,
    .class_init = ls7a_apic_class_init,
};

static void ls7a_apic_register_types(void)
{
    type_register_static(&ls7a_apic_info);
}

type_init(ls7a_apic_register_types)
