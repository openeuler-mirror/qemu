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

static void ummu_base_realize(DeviceState *dev, Error **errp)
{
}

static void ummu_base_unrealize(DeviceState *dev)
{
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
