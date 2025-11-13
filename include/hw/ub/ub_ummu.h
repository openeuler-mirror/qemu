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

#ifndef UB_UMMU_H
#define UB_UMMU_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/ub/hisi/ubc.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"

#define UMMU_INTERRUPT_ID 0x8989  // UMMU DEVICE ID need allocate later

#define __bf_shf(x) (__builtin_ffsll(x) - 1)

#define TYPE_UB_UMMU "ub-ummu"
OBJECT_DECLARE_TYPE(UMMUState, UMMUBaseClass, UB_UMMU)

typedef struct UMMUQueue {
    uint64_t base; /* base register */
    uint32_t prod;
    uint32_t cons;
    uint64_t entry_size;
    uint8_t log2size;
} UMMUQueue;

typedef struct UMMUMcmdQueue {
    UMMUQueue queue;
} UMMUMcmdQueue;

typedef struct UMMUEventQueue {
    UMMUQueue queue;
    uint64_t usi_addr;
    uint32_t usi_data;
    uint32_t usi_attr;
    bool event_que_int_en;
} UMMUEventQueue;

typedef struct UMMUGlbErr {
    uint64_t usi_addr;
    uint32_t usi_data;
    uint32_t usi_attr;
    bool glb_err_int_en;
    uint32_t glb_err;
    uint32_t glb_err_resp;
} UMMUGlbErr;

typedef enum UMMUUSIVectorType {
    UMMU_USI_VECTOR_EVETQ,
    UMMU_USI_VECTOR_GERROR,
    UMMU_USI_VECTOR_MAX,
} UMMUUSIVectorType;

typedef struct UMMUKVTblEntry {
    uint32_t dst_eid;
    uint32_t tecte_tag;
    QLIST_ENTRY(UMMUKVTblEntry) list;
} UMMUKVTblEntry;

#define UMMU_MAX_MCMDQS 32
#define UMMU_TECTE_TAG_MAX_NUM 32
struct UMMUState {
    /* <private> */
    SysBusDevice  dev;
    const char *mrtypename;
    MemoryRegion ummu_reg_mem;
    uint64_t ummu_reg_size;
    MemoryRegion root;
    MemoryRegion stage2;
    MemoryRegion sysmem;

    /* Nested */
    bool nested;
    UMMUViommu *viommu;

    /* spec register define */
    uint32_t cap[7];
    uint32_t ctrl[4];
    uint32_t ctrl0_ack;
    uint64_t tect_base;
    uint32_t tect_base_cfg;
    UMMUMcmdQueue mcmdqs[UMMU_MAX_MCMDQS];
    UMMUEventQueue eventq;
    UMMUGlbErr glb_err;
    uint64_t mapt_cmdq_ctxt_base;
    uint32_t release_um_queue;
    uint32_t release_um_queue_id;
    uint32_t ucmdq_page_sel;

    int usi_virq[UMMU_USI_VECTOR_MAX];
    uint8_t bus_num;
    QLIST_ENTRY(UMMUState) node;
    uint32_t tecte_tag_cache[UMMU_TECTE_TAG_MAX_NUM];
    uint32_t tecte_tag_num;

    UBBus *primary_bus;
    GHashTable *ummu_devs;
    GHashTable *configs;
    QLIST_HEAD(, UMMUKVTblEntry) kvtbl;
};

struct UMMUBaseClass {
    /* <private> */
    SysBusDeviceClass parent_class;
};

#endif
