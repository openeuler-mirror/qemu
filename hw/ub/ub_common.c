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
#include "hw/arm/virt.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/ub/ub_common.h"
#include "sysemu/dma.h"

/* tmp for vfio-ub run with stub, remove later */

uint32_t fill_rq(BusControllerState *s, void *rsp, uint32_t rsp_size)
{
    uint32_t ci = ub_get_long(s->msgq_reg + RQ_CI);
    uint32_t pi = ub_get_long(s->msgq_reg + RQ_PI);
    uint32_t pi_new;
    uint32_t depth = ub_get_long(s->msgq_reg + RQ_DEPTH);
    uint32_t remain;
    hwaddr dst_rqe;

    if (!s->msgq.rq_base_addr_gpa) {
        qemu_log("rq_base_addr_gpa is NULL\n");
        return UINT32_MAX;
    }

    if (depth > HI_MSGQ_MAX_DEPTH || depth < HI_MSGQ_MIN_DEPTH || ci >= depth || pi >= depth) {
        qemu_log("Invalid RQ indices: ci=%u pi=%u depth=%u\n", ci, pi, depth);
        return UINT32_MAX;
    }

    remain = depth - (pi + depth - ci) % depth;
    if (remain < 1) {
        qemu_log("RQ is full! depth=%u ci=%u pi=%u\n", depth, ci, pi);
        return UINT32_MAX;
    }

    dst_rqe = (uint64_t)((uint8_t *)s->msgq.rq_base_addr_gpa + pi * HI_MSG_RQE_SIZE);
    dma_memory_write(&address_space_memory, dst_rqe, rsp, rsp_size,
                     MEMTXATTRS_UNSPECIFIED);
    pi_new = (pi + DIV_ROUND_UP((rsp_size), HI_MSG_RQE_SIZE)) % depth;
    ub_set_long(s->msgq_reg + RQ_PI, pi_new);
    return pi;
}

uint32_t fill_cq(BusControllerState *s, HiMsgCqe *cqe)
{
    uint32_t ci = ub_get_long(s->msgq_reg + CQ_CI);
    uint32_t pi = ub_get_long(s->msgq_reg + CQ_PI);
    uint32_t depth = ub_get_long(s->msgq_reg + CQ_DEPTH);
    uint32_t remain;
    hwaddr dst_cqe;

    if (depth > HI_MSGQ_MAX_DEPTH || depth < HI_MSGQ_MIN_DEPTH || ci >= depth || pi >= depth) {
        qemu_log("Invalid CQ indices: ci=%u pi=%u depth=%u\n", ci, pi, depth);
        return UINT32_MAX;
    }

    if (!s->msgq.cq_base_addr_gpa) {
        qemu_log("sq_base_addr_gpa is NULL\n");
        return UINT32_MAX;
    }

    remain = depth - (pi + depth - ci) % depth;
    if (remain <= 1) {
        qemu_log("CQ is full! depth=%u ci=%u pi=%u\n", depth, ci, pi);
        return UINT32_MAX;
    }

    dst_cqe = (uint64_t)((HiMsgCqe *)s->msgq.cq_base_addr_gpa + pi);
    dma_memory_write(&address_space_memory, dst_cqe, cqe,
                     sizeof(HiMsgCqe), MEMTXATTRS_UNSPECIFIED);
    ub_set_long(s->msgq_reg + CQ_PI, ++pi % depth);

    return pi;
}

bool ub_guid_is_none(UbGuid *guid)
{
    if (guid->seq_num == 0 &&
        guid->device_id == 0 && guid->version == 0 &&
        guid->type == 0 && guid->vendor == 0) {
        return true;
    }

    return false;
}
