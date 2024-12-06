/*
 * QEMU CSV system stub
 *
 * Copyright: Hygon Info Technologies Ltd. 2022
 *
 * Author:
 *      Jiang Xin <jiangxin@hygon.cn>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "sev.h"
#include "csv.h"

int csv3_init(uint32_t policy, int fd, void *state, struct sev_ops *ops)
{
    return 0;
}

int csv3_load_data(uint64_t gpa, uint8_t *ptr, uint64_t len, Error **errp)
{
    g_assert_not_reached();
}

int csv3_launch_encrypt_vmcb(void)
{
    g_assert_not_reached();
}

int csv3_shared_region_dma_map(uint64_t start, uint64_t end)
{
    return 0;
}

void csv3_shared_region_dma_unmap(uint64_t start, uint64_t end)
{

}
