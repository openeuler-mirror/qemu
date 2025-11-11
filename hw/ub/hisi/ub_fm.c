/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/ub/hisi/ub_fm.h"
#include "qemu/log.h"

uint64_t ub_fm_msgq_reg_read(void *opaque, hwaddr addr, unsigned len)
{
    BusControllerState *s = opaque;
    uint64_t val;

    switch (len) {
    case BYTE_SIZE:
        val = ub_get_byte(s->fm_msgq_reg + addr);
        break;
    case WORD_SIZE:
        val = ub_get_word(s->fm_msgq_reg + addr);
        break;
    case DWORD_SIZE:
        val = ub_get_long(s->fm_msgq_reg + addr);
        break;
    default:
        qemu_log("invalid argument len 0x%x\n", len);
        val = ~0x0;
        break;
    }

    qemu_log("ub_fm_msgq_reg_read addr 0x%lx len 0x%x val 0x%lx\n",
             addr, len, val);
    return val;
}

void ub_fm_msgq_reg_write(void *opaque, hwaddr addr, uint64_t val, unsigned len)
{
    BusControllerState *s = opaque;

    switch (len) {
    case BYTE_SIZE:
        ub_set_byte(s->fm_msgq_reg + addr, val);
        break;
    case WORD_SIZE:
        ub_set_word(s->fm_msgq_reg + addr, val);
        break;
    case DWORD_SIZE:
        ub_set_long(s->fm_msgq_reg + addr, val);
        break;
    default:
        /* As length is under guest control, handle illegal values. */
        qemu_log("invalid argument len 0x%x addr 0x%lx val 0x%lx\n",
                 len, addr, val);
        return;
    }
    qemu_log("ub_fm_msgq_reg_write addr 0x%lx len 0x%x val 0x%lx\n",
             addr, len, val);
}