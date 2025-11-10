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

#ifndef HISI_UBC_H
#define HISI_UBC_H

/*
 * Address space layout of the UB controller
 * References: LinQuickCV100 Programming User Guide
 *
 * +----------------------------+ BA_ADDR+0xFFFF_FFFF
 * |   UMMU REG (3G)            |
 * +----------------------------+ BA_ADDR+0x4000_0000
 * |    ...                     |
 * +----------------------------+
 * | UB MSGQ(32M) only 2MB used |
 * +----------------------------+ BA_ADDR+0x1000_0000
 * | Local Register (256M)      |
 * +----------------------------+ BA_ADDR+0x0000_0000
*/
#define BASE_REG_SIZE        0x100000000 /* 4GiB */
#define LOCAL_REG_SIZE       0x10000000 /* 256MiB */
#define UBC_MSGQ_REG_SIZE    0x100000   /* 1MiB */
#define UMMU_REG_SIZE        0xC0000000 /* 3GiB */
#define UMMU_REG_OFFSET      0x40000000
#define UBC_MSGQ_REG_OFFSET  LOCAL_REG_SIZE
#define LOCAL_REG_OFFSET 0
#define SINGLE_UMMU_REG_SIZE     0x5000 /* 20KiB */
#define SINGLE_UMMU_PMU_REG_SIZE 0x1000 /* 4KiB */
#define UBC_INTERRUPT_ID_START      0x1FFF
#define UBC_INTERRUPT_ID_CNT        0x1000
#define VENDER_ID_HUAWEI            0xCC08

#endif
