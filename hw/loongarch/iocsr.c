/*
 * LOONGARCH IOCSR support
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
#include "qemu/log.h"
#include "sysemu/kvm.h"
#include "linux/kvm.h"
#include "migration/vmstate.h"
#include "hw/boards.h"
#include "hw/loongarch/larch.h"

#define BIT_ULL(nr) (1ULL << (nr))
#define LOONGARCH_IOCSR_FEATURES 0x8
#define IOCSRF_TEMP BIT_ULL(0)
#define IOCSRF_NODECNT BIT_ULL(1)
#define IOCSRF_MSI BIT_ULL(2)
#define IOCSRF_EXTIOI BIT_ULL(3)
#define IOCSRF_CSRIPI BIT_ULL(4)
#define IOCSRF_FREQCSR BIT_ULL(5)
#define IOCSRF_FREQSCALE BIT_ULL(6)
#define IOCSRF_DVFSV1 BIT_ULL(7)
#define IOCSRF_GMOD BIT_ULL(9)
#define IOCSRF_VM BIT_ULL(11)
#define LOONGARCH_IOCSR_VENDOR 0x10
#define LOONGARCH_IOCSR_CPUNAME 0x20
#define LOONGARCH_IOCSR_NODECNT 0x408
#define LOONGARCH_IOCSR_MISC_FUNC 0x420
#define IOCSR_MISC_FUNC_TIMER_RESET BIT_ULL(21)
#define IOCSR_MISC_FUNC_EXT_IOI_EN BIT_ULL(48)

enum {
    IOCSR_FEATURES,
    IOCSR_VENDOR,
    IOCSR_CPUNAME,
    IOCSR_NODECNT,
    IOCSR_MISC_FUNC,
    IOCSR_MAX
};

#ifdef CONFIG_KVM
static uint32_t iocsr_array[IOCSR_MAX] = {
    [IOCSR_FEATURES] = LOONGARCH_IOCSR_FEATURES,
    [IOCSR_VENDOR] = LOONGARCH_IOCSR_VENDOR,
    [IOCSR_CPUNAME] = LOONGARCH_IOCSR_CPUNAME,
    [IOCSR_NODECNT] = LOONGARCH_IOCSR_NODECNT,
    [IOCSR_MISC_FUNC] = LOONGARCH_IOCSR_MISC_FUNC,
};
#endif

#define TYPE_IOCSR "iocsr"
#define IOCSR(obj) OBJECT_CHECK(IOCSRState, (obj), TYPE_IOCSR)

typedef struct IOCSRState {
    SysBusDevice parent_obj;
    uint64_t iocsr_val[IOCSR_MAX];
} IOCSRState;

IOCSRState iocsr_init = { .iocsr_val = {
                              IOCSRF_NODECNT | IOCSRF_MSI | IOCSRF_EXTIOI |
                                  IOCSRF_CSRIPI | IOCSRF_GMOD | IOCSRF_VM,
                              0x6e6f73676e6f6f4c, /* Loongson */
                              0x303030354133,     /*3A5000*/
                              0x4,
                              0x0,
                          } };

static int kvm_iocsr_pre_save(void *opaque)
{
#ifdef CONFIG_KVM
    IOCSRState *s = opaque;
    struct kvm_iocsr_entry entry;
    int i = 0;

    if ((!kvm_enabled())) {
        return 0;
    }

    for (i = 0; i < IOCSR_MAX; i++) {
        entry.addr = iocsr_array[i];
        kvm_vm_ioctl(kvm_state, KVM_LOONGARCH_GET_IOCSR, &entry);
        s->iocsr_val[i] = entry.data;
    }
#endif
    return 0;
}

static int kvm_iocsr_post_load(void *opaque, int version)
{
#ifdef CONFIG_KVM
    IOCSRState *s = opaque;
    struct kvm_iocsr_entry entry;
    int i = 0;

    if (!kvm_enabled()) {
        return 0;
    }

    for (i = 0; i < IOCSR_MAX; i++) {
        entry.addr = iocsr_array[i];
        entry.data = s->iocsr_val[i];
        kvm_vm_ioctl(kvm_state, KVM_LOONGARCH_SET_IOCSR, &entry);
    }
#endif
    return 0;
}

static void iocsr_reset(DeviceState *d)
{
    IOCSRState *s = IOCSR(d);
    int i;

    for (i = 0; i < IOCSR_MAX; i++) {
        s->iocsr_val[i] = iocsr_init.iocsr_val[i];
    }
    kvm_iocsr_post_load(s, 0);
}

static void init_vendor_cpuname(uint64_t *vendor, uint64_t *cpu_name,
                                char *cpuname)
{
    int i = 0, len = 0;
    char *index = NULL, *index_end = NULL;
    char *vendor_c = (char *)vendor;
    char *cpu_name_c = (char *)cpu_name;

    index = strstr(cpuname, "-");
    len = strlen(cpuname);
    if ((index == NULL) || (len <= 0)) {
        return;
    }

    *vendor = 0;
    *cpu_name = 0;
    index_end = cpuname + len;

    while (((cpuname + i) < index) && (i < sizeof(uint64_t))) {
        vendor_c[i] = cpuname[i];
        i++;
    }

    index += 1;
    i = 0;

    while (((index + i) < index_end) && (i < sizeof(uint64_t))) {
        cpu_name_c[i] = index[i];
        i++;
    }

    return;
}

static void iocsr_instance_init(Object *obj)
{
    IOCSRState *s = IOCSR(obj);
    int i;
    LoongarchMachineState *lsms;
    LoongarchMachineClass *lsmc;
    Object *machine = qdev_get_machine();
    ObjectClass *mc = object_get_class(machine);

    /* 'lams' should be initialized */
    if (!strcmp(MACHINE_CLASS(mc)->name, "none")) {
        return;
    }

    lsms = LoongarchMACHINE(machine);
    lsmc = LoongarchMACHINE_GET_CLASS(lsms);

    init_vendor_cpuname((uint64_t *)&iocsr_init.iocsr_val[IOCSR_VENDOR],
                        (uint64_t *)&iocsr_init.iocsr_val[IOCSR_CPUNAME],
                        lsmc->cpu_name);

    for (i = 0; i < IOCSR_MAX; i++) {
        s->iocsr_val[i] = iocsr_init.iocsr_val[i];
    }
}

static const VMStateDescription vmstate_iocsr = {
    .name = TYPE_IOCSR,
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_save = kvm_iocsr_pre_save,
    .post_load = kvm_iocsr_post_load,
    .fields = (VMStateField[]){ VMSTATE_UINT64_ARRAY(iocsr_val, IOCSRState,
                                                     IOCSR_MAX),
                                VMSTATE_END_OF_LIST() }
};

static void iocsr_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = iocsr_reset;
    dc->vmsd = &vmstate_iocsr;
}

static const TypeInfo iocsr_info = {
    .name = TYPE_IOCSR,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IOCSRState),
    .instance_init = iocsr_instance_init,
    .class_init = iocsr_class_init,
};

static void iocsr_register_types(void)
{
    type_register_static(&iocsr_info);
}

type_init(iocsr_register_types)
