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

#ifndef UB_H
#define UB_H
#include <linux/vfio.h>
#include "qemu/typedefs.h"
#include "exec/memory.h"

#define BYTE_SIZE 1
#define WORD_SIZE 2
#define DWORD_SIZE 4

static inline void ub_set_byte(uint8_t *config, uint8_t val)
{
    *config = val;
}

static inline uint8_t ub_get_byte(const uint8_t *config)
{
    return *config;
}

static inline void ub_set_word(uint8_t *config, uint16_t val)
{
    stw_le_p(config, val);
}

static inline uint16_t ub_get_word(const uint8_t *config)
{
    return lduw_le_p(config);
}

static inline void ub_set_long(uint8_t *config, uint32_t val)
{
    stl_le_p(config, val);
}

static inline uint32_t ub_get_long(const uint8_t *config)
{
    return ldl_le_p(config);
}

static inline void ub_set_quad(uint8_t *config, uint64_t val)
{
    stq_le_p(config, val);
}

static inline uint64_t ub_get_quad(const uint8_t *config)
{
    return ldq_le_p(config);
}

#endif
