/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/arm/virt.h"
#include "hw/qdev-properties.h"
#include "hw/ub/ub.h"
#include "hw/ub/hisi/ummu.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_ummu.h"
#include "hw/ub/ub_config.h"
#include "hw/ub/hisi/ubc.h"
#include "migration/vmstate.h"
#include "ub_ummu_internal.h"
#include "sysemu/dma.h"
#include "hw/arm/mmu-translate-common.h"
#include "hw/ub/ub_ubc.h"
#include "qemu/error-report.h"
#include "trace.h"

QLIST_HEAD(, UMMUState) ub_umms;
UMMUState *ummu_find_by_bus_num(uint8_t bus_num)
{
    UMMUState *ummu;
    QLIST_FOREACH(ummu, &ub_umms, node) {
        if (ummu->bus_num == bus_num) {
            return ummu;
        }
    }
    return NULL;
}

static uint64_t ummu_reg_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void ummu_reg_write(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
}

static const MemoryRegionOps ummu_reg_ops = {
    .read = ummu_reg_read,
    .write = ummu_reg_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 2,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 2,
        .max_access_size = 8,
    },
};

static void ummu_registers_init(UMMUState *u)
{
    int i;

    memset(u->cap, 0, sizeof(u->cap));
    /* cap 0 init */
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, DSTEID_SIZE,          0x10);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, TOKENID_SIZE,         0x14);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, ATTR_PERMS_OVR,       0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, ATTR_TYPES_OVR,       0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, S2_ATTR_TYPE,         0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, TCT_LEVEL,            0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, TECT_MODE,            0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, TECT_LEVEL,           0x1);
    /* cap 1 init */
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, EVENTQ_SIZE,          0x13);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, EVENTQ_NUMB,          0x0);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, EVENTQ_SUPPORT,       0x1);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, MCMDQ_SIZE,           0xF);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, MCMDQ_NUMB,           0x3);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, MCMDQ_SUPPORT,        0x1);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, EVENT_GEN,            0x1);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, STALL_MAX,            0x80);
    /* cap 2 init */
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, VMID_TLBI,            0x0);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, TLB_BOARDCAST,        0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, RANGE_TLBI,           0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, OA_SIZE,              0x5);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, GRAN4K_T,             0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, GRAN16K_T,            0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, GRAN64K_T,            0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, VA_EXTEND,            0x0);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, S2_TRANS,             0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, S1_TRANS,             0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, SMALL_TRANS,          0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, TRANS_FORM,           0x2);
    /* cap 3 init */
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, HIER_ATTR_DISABLE,    0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, S2_EXEC_NEVER_CTRL,   0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, BBM_LEVEL,            0x2);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, COHERENT_ACCESS,      0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, TTENDIAN_MODE,        0x0);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, MTM_SUPPORT,          0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, HTTU_SUPPORT,         0x2);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, HYP_S1CONTEXT,        0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, USI_SUPPORT,          0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, STALL_MODEL,          0x0);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, TERM_MODEL,           0x0);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, SATI_MAX,             0x1);
    /* cap 4 init */
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UCMDQ_UCPLQ_NUMB,     0x10);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UCMDQ_SIZE,           0xF);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UCPLQ_SIZE,           0xF);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UIEQ_SIZE,            0xF);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UIEQ_NUMB,            0x5);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UIEQ_SUPPORT,         0x1);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, PPLB_SUPPORT,         0x0);

    /* cap 5 init */
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, MAPT_SUPPORT,         0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, MAPT_MODE,            0x3);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, GRAN2M_P,             0x0);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, GRAN4K_P,             0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, TOKENVAL_CHK,         0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, TOKENVAL_CHK_MODE,    0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, RANGE_PLBI,           0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, PLB_BORDCAST,         0x0);
    /* cap 6 init */
    u->cap[6] = FIELD_DP32(u->cap[6], CAP6, MTM_ID_MAX,           0x00FF);
    u->cap[6] = FIELD_DP32(u->cap[6], CAP6, MTM_GP_MAX,           0x03);

    /* ctrlr init */
    memset(u->ctrl, 0, sizeof(u->ctrl));
    u->ctrl[1] = FIELD_DP32(u->ctrl[1], CTRL1, TECT_MODE_SEL,     0x1);

    /* tect init */
    u->tect_base = 0;
    u->tect_base_cfg = 0;

    /* mcmdq init */
    for (i = 0; i < UMMU_MAX_MCMDQS; i++) {
        u->mcmdqs[i].queue.base = 0;
        u->mcmdqs[i].queue.prod = 0;
        u->mcmdqs[i].queue.cons = 0;
        u->mcmdqs[i].queue.entry_size = sizeof(UMMUMcmdqCmd);
    }

    /* eventq init */
    memset(&u->eventq, 0, sizeof(u->eventq));

    /* glb err init */
    memset(&u->glb_err, 0, sizeof(u->glb_err));

    /* evt queue init */
    u->eventq.queue.base = 0;
    u->eventq.queue.prod = 0;
    u->eventq.queue.cons = 0;
    u->eventq.queue.entry_size = sizeof(UMMUEvent);

    /* mapt cmdq ctxt base addr init */
    u->mapt_cmdq_ctxt_base = 0;

    /* umcmdq default page set to 4K */
    u->ucmdq_page_sel = MAPT_CMDQ_CTRLR_PAGE_SIZE_4K;
}

int ummu_associating_with_ubc(BusControllerState *ubc)
{
    UMMUState *ummu;
    unsigned int bus_num;

    if (1 != sscanf(ubc->bus->qbus.name, "ubus.%u", &bus_num)) {
        qemu_log("failed to get bus num %s\n",
                 ubc->bus->qbus.name);
        return -1;
    }
    ummu = ummu_find_by_bus_num(bus_num);
    if (!ummu) {
        qemu_log("failed to get ummu %u\n", bus_num);
        return -1;
    }
    return 0;
}

static void ub_save_ummu_list(UMMUState *u)
{
    QLIST_INSERT_HEAD(&ub_umms, u, node);
}

static void ub_remove_ummu_list(UMMUState *u)
{
    QLIST_REMOVE(u, node);
}

static void ummu_base_realize(DeviceState *dev, Error **errp)
{
    static uint8_t NO = 0;
    UMMUState *u = UB_UMMU(dev);
    SysBusDevice *sysdev = SYS_BUS_DEVICE(dev);

    u->bus_num = NO;
    sysdev->parent_obj.id = g_strdup_printf("ummu.%u", NO++);

    memory_region_init_io(&u->ummu_reg_mem, OBJECT(u), &ummu_reg_ops,
                          u, TYPE_UB_UMMU, u->ummu_reg_size);
    sysbus_init_mmio(sysdev, &u->ummu_reg_mem);
    ummu_registers_init(u);
    ub_save_ummu_list(u);
}

static void ummu_base_unrealize(DeviceState *dev)
{
    UMMUState *u = UB_UMMU(dev);
    SysBusDevice *sysdev = SYS_BUS_DEVICE(dev);

    ub_remove_ummu_list(u);
    if (sysdev->parent_obj.id) {
        g_free(sysdev->parent_obj.id);
    }

}

static void ummu_base_reset(DeviceState *dev)
{
    /* reset ummu relative struct later */
}

static Property ummu_dev_properties[] = {
    DEFINE_PROP_UINT64("ub-ummu-reg-size", UMMUState,
                       ummu_reg_size, 0),
    DEFINE_PROP_LINK("primary-bus", UMMUState, primary_bus,
                     TYPE_UB_BUS, UBBus *),
    DEFINE_PROP_BOOL("nested", UMMUState, nested, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void ummu_base_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, ummu_dev_properties);
    dc->realize = ummu_base_realize;
    dc->unrealize = ummu_base_unrealize;
    dc->reset = ummu_base_reset;
}

static const TypeInfo ummu_base_info = {
    .name          = TYPE_UB_UMMU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(UMMUState),
    .class_data    = NULL,
    .class_size    = sizeof(UMMUBaseClass),
    .class_init    = ummu_base_class_init,
};

static void ummu_base_register_types(void)
{
    type_register_static(&ummu_base_info);
}
type_init(ummu_base_register_types)
