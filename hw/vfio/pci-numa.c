/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
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
#include "hw/nvram/fw_cfg.h"
#include "hw/boards.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "hw/vfio/pci.h"

static uint8_t vfio_to_virtual_node(int node)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    NodeInfo *node_info;
    int node_bitmap_item_len, off, mask;
    int virtual_nodes;
    int i;

    if (!ms || !ms->numa_state) {
        return NUMA_NODE_UNASSIGNED;
    }

    /*
     * QEMU use an unsigned long array to save the map from virtual nodes
     * to host nodes. So we should match the bitmap of virtual nodes.
     */
    node_bitmap_item_len = BITS_PER_BYTE * sizeof(unsigned long);
    off = node / node_bitmap_item_len;
    mask = 1 << (node % node_bitmap_item_len);
    virtual_nodes = ms->numa_state->num_nodes;

    for (i = 0; i < virtual_nodes; i++) {
        node_info = &ms->numa_state->nodes[i];
        if (node_info && node_info->node_memdev &&
            (node_info->node_memdev->host_nodes[off] & mask)) {
                return i;
        }
    }

    return NUMA_NODE_UNASSIGNED;
}

uint8_t vfio_get_dev_node(char *sysfsdev)
{
    char *node_path;
    struct stat st;
    int node_fd;
    char buf[1024];
    int numa_node = NUMA_NODE_UNASSIGNED;

    node_path = g_strdup_printf("%s/numa_node", sysfsdev);
    if (stat(node_path, &st) < 0) {
        goto failed;
    }

    node_fd = qemu_open_old(node_path, O_RDONLY);
    if (node_fd < 0) {
        qemu_log("vfio device %s failed to open %s\n", sysfsdev, node_path);
        goto failed;
    }

    if (read(node_fd, buf, 1024) < 0) {
        qemu_log("vfio device %s failed to read %s\n", sysfsdev, node_path);
        goto read_failed;
    }
    close(node_fd);

    if (sscanf(buf, "%d", &numa_node) != 1) {
        qemu_log("vfio device %s failed to parse %s\n", sysfsdev, node_path);
        goto failed;
    }

    qemu_log("vfio device %s on NUMA node %d\n", sysfsdev, numa_node);
    if (numa_node < 0 || numa_node >= MAX_NODES) {
        goto failed;
    }
    g_free(node_path);

    return vfio_to_virtual_node(numa_node);

read_failed:
    close(node_fd);
failed:
    g_free(node_path);

    return NUMA_NODE_UNASSIGNED;
}
