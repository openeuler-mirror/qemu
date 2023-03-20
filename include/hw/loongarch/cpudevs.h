/*
 * cpu device emulation on Loongarch system.
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

#ifndef HW_LOONGARCH_CPUDEVS_H
#define HW_LOONGARCH_CPUDEVS_H

#include "target/loongarch64/cpu-qom.h"

/* Definitions for LOONGARCH CPU internal devices.  */
#define MAX_GIPI_CORE_NUM 256
#define MAX_GIPI_MBX_NUM 4

#define LS3A_INTC_IP 8
#define MAX_CORES 256
#define EXTIOI_IRQS (256)
#define EXTIOI_IRQS_BITMAP_SIZE (256 / 8)
/* map to ipnum per 32 irqs */
#define EXTIOI_IRQS_IPMAP_SIZE (256 / 32)

typedef struct gipi_core {
    uint32_t status;
    uint32_t en;
    uint32_t set;
    uint32_t clear;
    uint64_t buf[MAX_GIPI_MBX_NUM];
    qemu_irq irq;
} gipi_core;

typedef struct gipiState {
    gipi_core core[MAX_GIPI_CORE_NUM];
} gipiState;

typedef struct apicState {
    /* hardware state */
    uint8_t ext_en[EXTIOI_IRQS_BITMAP_SIZE];
    uint8_t ext_bounce[EXTIOI_IRQS_BITMAP_SIZE];
    uint8_t ext_isr[EXTIOI_IRQS_BITMAP_SIZE];
    uint8_t ext_coreisr[MAX_CORES][EXTIOI_IRQS_BITMAP_SIZE];
    uint8_t ext_ipmap[EXTIOI_IRQS_IPMAP_SIZE];
    uint8_t ext_coremap[EXTIOI_IRQS];
    uint16_t ext_nodetype[16];
    uint64_t ext_control;

    /* software state */
    uint8_t ext_sw_ipmap[EXTIOI_IRQS];
    uint8_t ext_sw_coremap[EXTIOI_IRQS];
    uint8_t ext_ipisr[MAX_CORES * LS3A_INTC_IP][EXTIOI_IRQS_BITMAP_SIZE];

    qemu_irq parent_irq[MAX_CORES][LS3A_INTC_IP];
    qemu_irq *irq;
} apicState;

void cpu_init_irq(LOONGARCHCPU *cpu);
void cpu_loongarch_clock_init(LOONGARCHCPU *cpu);
#endif
