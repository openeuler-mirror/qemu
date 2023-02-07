/*
 * QEMU LOONGARCH CPU
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

#ifndef QEMU_LOONGARCH_CPU_QOM_H
#define QEMU_LOONGARCH_CPU_QOM_H

#include "hw/core/cpu.h"

#define TYPE_LOONGARCH_CPU "loongarch-cpu"

#define LOONGARCH_CPU_CLASS(klass)                                            \
    OBJECT_CLASS_CHECK(LOONGARCHCPUClass, (klass), TYPE_LOONGARCH_CPU)
#define LOONGARCH_CPU(obj)                                                    \
    OBJECT_CHECK(LOONGARCHCPU, (obj), TYPE_LOONGARCH_CPU)
#define LOONGARCH_CPU_GET_CLASS(obj)                                          \
    OBJECT_GET_CLASS(LOONGARCHCPUClass, (obj), TYPE_LOONGARCH_CPU)

/**
 * LOONGARCHCPUClass:
 * @parent_realize: The parent class' realize handler.
 * @parent_reset: The parent class' reset handler.
 *
 * A LOONGARCH CPU model.
 */
typedef struct LOONGARCHCPUClass {
    /*< private >*/
    CPUClass parent_class;
    /*< public >*/

    DeviceRealize parent_realize;
    DeviceUnrealize parent_unrealize;
    DeviceReset parent_reset;
    const struct loongarch_def_t *cpu_def;
} LOONGARCHCPUClass;

typedef struct LOONGARCHCPU LOONGARCHCPU;

#endif
