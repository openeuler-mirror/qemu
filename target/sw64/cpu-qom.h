/*
 * QEMU SW64 CPU
 *
 * Copyright (c) 2018 Lin Hainan
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef QEMU_SW64_CPU_QOM
#define QEMU_SW64_CPU_QOM

#include "hw/core/cpu.h"

#define TYPE_SW64_CPU "sw64-cpu"

#define SW64_CPU_CLASS(kclass) \
    OBJECT_CLASS_CHECK(SW64CPUClass, (kclass), TYPE_SW64_CPU)
#define SW64_CPU(obj) \
    OBJECT_CHECK(SW64CPU, (obj), TYPE_SW64_CPU)
#define SW64_CPU_GET_CLASS(obj) \
    OBJECT_GET_CLASS(SW64CPUClass, (obj), TYPE_SW64_CPU)

/**
 * SW64CPUClass:
 * @parent_realize: The parent class' realize handler.
 * @parent_reset: The parent class' reset handler.
 *
 * An SW64 CPU model.
 */
typedef struct SW64CPUClass {
    /* private */
    CPUClass parent_class;
    /* public */
    DeviceRealize parent_realize;
    DeviceReset parent_reset;
} SW64CPUClass;

typedef struct SW64CPU SW64CPU;
#endif
