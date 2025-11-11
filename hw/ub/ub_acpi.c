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
#include "qemu/module.h"
#include "qemu/cutils.h"
#include "qemu/units.h"
#include "hw/arm/virt.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_acpi.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qapi/util.h"
#include "qapi/qmp/qstring.h"
#include "hw/ub/hisi/ub_fm.h"
#include "hw/acpi/aml-build.h"

