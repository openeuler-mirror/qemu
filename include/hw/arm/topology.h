/*
 * ARM CPU topology data structures and functions
 *
 * Copyright (c) 2020 HUAWEI TECHNOLOGIES CO.,LTD.
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

#ifndef HW_ARM_TOPOLOGY_H
#define HW_ARM_TOPOLOGY_H

typedef struct ARMCPUTopoInfo {
    unsigned pkg_id;
    unsigned cluster_id;
    unsigned core_id;
    unsigned smt_id;
} ARMCPUTopoInfo;

/* Calculate (contiguous) CPU index based on topology */
static inline unsigned idx_from_topo_ids(unsigned nr_clusters,
                                         unsigned nr_cores,
                                         unsigned nr_threads,
                                         const ARMCPUTopoInfo *topo)
{
    assert(nr_clusters > 0);
    assert(nr_cores > 0);
    assert(nr_threads > 0);
    assert(topo != NULL);

    return topo->pkg_id * nr_clusters * nr_cores * nr_threads +
           topo->cluster_id * nr_cores +
           topo->core_id * nr_threads +
           topo->smt_id;
}

/* Calculate thread/core/cluster/package topology
 * based on (contiguous) CPU index
 */
static inline void topo_ids_from_idx(unsigned cpu_index,
                                     unsigned nr_clusters,
                                     unsigned nr_cores,
                                     unsigned nr_threads,
                                     ARMCPUTopoInfo *topo)
{
    assert(nr_clusters > 0);
    assert(nr_cores > 0);
    assert(nr_threads > 0);
    assert(topo != NULL);

    topo->smt_id = cpu_index % nr_threads;
    topo->core_id = cpu_index / nr_threads % nr_cores;
    topo->cluster_id = cpu_index / nr_threads / nr_cores % nr_clusters;
    topo->pkg_id = cpu_index / nr_threads / nr_cores / nr_clusters;
}

#endif /* HW_ARM_TOPOLOGY_H */

