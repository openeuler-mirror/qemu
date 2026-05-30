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
#include "qemu/module.h"
#include "qemu/cutils.h"
#include "qemu/units.h"
#include "hw/arm/virt.h"
#include "hw/qdev-core.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/ub/ub.h"
#include "hw/ub/ub_config.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_acpi.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qapi/util.h"
#include "qapi/qmp/qstring.h"
#include "hw/ub/ub_ummu.h"
#include "hw/ub/hisi/ub_mem.h"
#include "hw/ub/hisi/ub_fm.h"
#include "hw/ub/hisi/ubc.h"
#include "hw/acpi/aml-build.h"
#include "hw/ub/ub_common.h"
#define UBIOS_VERSION 1
#define DTS_SIG_UBCTL "bus controller"
#define DTS_SIG_UMMU "ummu"
#define DTS_SIG_RSV_MEM "rsv_mem"

typedef struct UBIdevErsAddrSpaceNode {
    uint64_t offset;
    uint64_t allocated_offset;
    uint64_t size;

    QTAILQ_ENTRY(UBIdevErsAddrSpaceNode) stailq_free;
    QTAILQ_ENTRY(UBIdevErsAddrSpaceNode) stailq_used;
} UBIdevErsAddrSpaceNode;

typedef struct UBIdevErsAddrSpaceManage {
    bool init;
    uint64_t size;
    hwaddr base_addr;

    QTAILQ_HEAD(, UBIdevErsAddrSpaceNode) as_free_list;
    QTAILQ_HEAD(, UBIdevErsAddrSpaceNode) as_used_list;
} UBIdevErsAddrSpaceManage;

UBIdevErsAddrSpaceManage g_idevErsAddrSpaceManage;

static uint8_t gpa_bits;
void ub_set_gpa_bits(uint8_t bits)
{
    gpa_bits = bits;
}

static void ub_init_table_header(DtsTableHeader *header,
                                 const char *name,
                                 uint32_t size, uint16_t version)
{
    strncpy(header->name, name, sizeof(header->name) - 1);
    header->total_size = size;
    header->version = version;
    header->remain_size = 0;
    qemu_log("%s total_size %u\n", name, size);
}

static void ub_init_vendor_info(UbcVendorInfo *vendor_info, VirtMachineState *vms)
{
    uint16_t mar_id;
    uint64_t base_reg = vms->memmap[VIRT_UBC_BASE_REG].base;
    uint64_t addr_cc = vms->memmap[VIRT_UB_MEM_CC].base;
    uint64_t addr_nc = vms->memmap[VIRT_UB_MEM_NC].base;
    UbMemDecoderInfo *mem_info;
    uint64_t local_reg_offset[] = {
        BA0_OFFSET,
        BA1_OFFSET,
        BA2_OFFSET,
        BA3_OFFSET,
        BA4_OFFSET,
    };
    uint64_t mar_space_size[] = {
        UB_MEM_MAR0_SPACE_SIZE,
        UB_MEM_MAR1_SPACE_SIZE,
        UB_MEM_MAR2_SPACE_SIZE,
        UB_MEM_MAR3_SPACE_SIZE,
        UB_MEM_MAR4_SPACE_SIZE,
    };

    memset(vendor_info, 0, sizeof(UbcVendorInfo));
    vendor_info->ub_mem_ver = 0;
    vendor_info->max_addr_bits = gpa_bits;
    /* now only support one UBC */
    vendor_info->cmd_queue_base = vms->memmap[VIRT_UBC_BASE_REG].base + CMDQ_BASE_ADDR;
    vendor_info->event_queue_base = vms->memmap[VIRT_UBC_BASE_REG].base + EVTQ_BASE_ADDR;
    vendor_info->vendor_feature_sets = (uint64_t)sysfs_get_ub_feature() << 32; // bit32~55: UB feature capability from sysfs

    for (mar_id = 0; mar_id < MAR_NUM_ONE_UDIE; mar_id++) {
        mem_info = &vendor_info->mem_info[mar_id];
        mem_info->decode_addr = base_reg + local_reg_offset[mar_id] + MAR_OFFSET;
        mem_info->cc_base_addr = mar_space_size[mar_id] ?
                                 addr_cc >> MB_SIZE_OFFSET : 0;
        mem_info->cc_base_size = mar_space_size[mar_id] >> MB_SIZE_OFFSET;
        mem_info->nc_base_addr = mar_space_size[mar_id] ?
                                 addr_nc >> MB_SIZE_OFFSET : 0;
        mem_info->nc_base_size = mar_space_size[mar_id] >> MB_SIZE_OFFSET;
        addr_cc += mar_space_size[mar_id];
        addr_nc += mar_space_size[mar_id];
        qemu_log("MAR%u decode_addr 0x%lx, cc ba 0x%x size 0x%x,"
                 " nc ba 0x%x size 0x%x\n",
                 mar_id, mem_info->decode_addr,
                 mem_info->cc_base_addr, mem_info->cc_base_size,
                 mem_info->nc_base_addr, mem_info->nc_base_size);
    }
}

static void ub_init_ubc_node(uint16_t ubc_count, UbcNode *ubc, VirtMachineState *vms)
{
    uint16_t i;
    uint64_t ub_mmio_addr = vms->memmap[VIRT_HIGH_UB_MMIO].base;
    for (i = 0; i < ubc_count; i++) {
        (ubc + i)->interrupt_id_start = UBC_INTERRUPT_ID_START + i * UBC_INTERRUPT_ID_CNT;
        (ubc + i)->interrupt_id_end = (ubc + i)->interrupt_id_start + UBC_INTERRUPT_ID_CNT - 1;
        (ubc + i)->gpa_base = ub_mmio_addr + i * UBIOS_MMIOS_SIZE_PER_UBC;
        (ubc + i)->gpa_size = UBIOS_MMIOS_SIZE_PER_UBC;
        (ubc + i)->memory_size_limit = gpa_bits;
        (ubc + i)->dma_cca = 1;  /* 1: DMA(Y) CCA(Y) */
        (ubc + i)->ummu_mapping = UBIOS_UMMU_TABLE_CNT ? 0 : 0xffff;
        (ubc + i)->proximity_domain = 0;
        (ubc + i)->msg_queue_base = vms->memmap[VIRT_UBC_BASE_REG].base +
                                    UBC_MSGQ_REG_OFFSET;
        (ubc + i)->msg_queue_size = UBC_MSGQ_REG_SIZE;
        (ubc + i)->msg_queue_depth = HI_MSGQ_DEPTH;
        (ubc + i)->msg_queue_interrupt = UBC_QUEUE_INTERRUPT_DEFAULT;
        /*
        * Interrupt attributes
        * BIT0: Triggering
        *      ACPI_LEVEL_SENSITIVE 0x00
        *      ACPI_EDGE_SENSITIVE  0x01
        * BIT1: Polarity
        *      ACPI_ACTIVE_HIGH     0x00
        *      ACPI_ACTIVE_LOW      0x01
        */
        (ubc + i)->msg_queue_interrupt_attr = 0x0;
        memset(&(ubc + i)->ubc_info, 0, sizeof(UbGuid));
        ub_init_vendor_info((UbcVendorInfo *)&(ubc + i)->vendor_info, vms);
        qemu_log("init ubc_table[%d]=0x%lx, interrupt_id=[0x%x-0x%x]\n",
                 i, (ubc + i)->gpa_base, (ubc + i)->interrupt_id_start,
                 (ubc + i)->interrupt_id_end);
    }
}

static void ub_init_ubios_ubc_table(DtsSubUbcTable *ubc_table, VirtMachineState *vms)
{
    UbcNode *ubc = NULL;

    ubc_table->ubc_count = UBIOS_UBC_TABLE_CNT;
    ub_init_table_header(&ubc_table->header, DTS_SIG_UBCTL,
                         UBIOS_UBC_TABLE_SIZE(ubc_table->ubc_count),
                         UBIOS_VERSION);
    ubc_table->local_cna_start = LOCAL_CNA_START;
    ubc_table->local_cna_end = LOCAL_CNA_END;
    ubc_table->local_eid_start = LOCAL_EID_START;
    ubc_table->local_eid_end = LOCAL_EID_END;
    ubc_table->feature_set = 0;
    /* ubc_table->cluster_mode
     *       System working mode
     *       0: single-node system
     *       1: cluster mode
     */
    ubc_table->cluster_mode = vms->ub_cluster_mode;
    qemu_log("init ub cluster mode %u\n", ubc_table->cluster_mode);
    ubc = (UbcNode *)ubc_table->node;
    ub_init_ubc_node(ubc_table->ubc_count, ubc, vms);
}

static void ub_init_ummu_vendor_info(UbMemMmuInfo *vendor_info, VirtMachineState *vms)
{
    vendor_info->valid_bits = UB_MEM_VALID_VALUE;
    vendor_info->protection_table_bits = 0xa;
    vendor_info->translation_table_bits = 0x11;
    vendor_info->ext_reg_base = (vms->memmap[VIRT_UBC_BASE_REG].base | UMMU_OFFSET | UB_MEM_REG_BASE);
    vendor_info->ext_reg_size = UMMU_EXT_REG_SIZE;
    qemu_log("ummu vendor info reg_base=0x%lx\n", vendor_info->ext_reg_base);
}

static void ub_init_ubios_ummu_table(DtsSubUmmuTable *ummu_table, VirtMachineState *vms)
{
    uint16_t i;
    UmmuNode *ummu = NULL;
    UbMemMmuInfo *vendor_info = NULL;

    ummu_table->count = UBIOS_UMMU_TABLE_CNT;
    ub_init_table_header(&ummu_table->header, DTS_SIG_UMMU,
                         UBIOS_UMMU_TABLE_SIZE(ummu_table->count),
                         UBIOS_VERSION);
    ummu = (UmmuNode *)ummu_table->node;
    for (i = 0; i < ummu_table->count; i++) {
        (ummu + i)->base_addr = vms->memmap[VIRT_UBC_BASE_REG].base + UMMU_REG_OFFSET +
                                i * SINGLE_UMMU_REG_SIZE;
        (ummu + i)->addr_size = UMMU_REG_SIZE;
        (ummu + i)->interrupt_id = UMMU_INTERRUPT_ID;
        (ummu + i)->proximity_domain = 0;
        (ummu + i)->its_index = 0;
        (ummu + i)->pmu_addr = (ummu + i)->base_addr + SINGLE_UMMU_REG_SIZE;
        (ummu + i)->pmu_size = SINGLE_UMMU_PMU_REG_SIZE;
        (ummu + i)->pmu_interrupt_id = UMMU_INTERRUPT_ID + 1;
        (ummu + i)->min_tid = UMMU_RESERVED_TID_NUM + 1;
        (ummu + i)->max_tid = 0xFFFFF;
        (ummu + i)->vender_id = VENDER_ID_HUAWEI;

        vendor_info = (UbMemMmuInfo *)(ummu + i)->vender_info;
        ub_init_ummu_vendor_info(vendor_info, vms);
        qemu_log("init ummu_table[%d]=0x%lx,pmu_addr=0x%lx,pmu_size=0x%lx,pmu_interrupt_id=0x%x\n",
                 i, (ummu + i)->base_addr, (ummu + i)->pmu_addr,
                 (ummu + i)->pmu_size, (ummu + i)->pmu_interrupt_id);
    }
}

static void ub_init_ubios_rsv_mem_table(DtsRsvMemTable *rsv_mem_table, VirtMachineState *vms)
{
    MemRange *mem_range;
    rsv_mem_table->count = UBIOS_UMMU_TABLE_CNT;
    ub_init_table_header(&rsv_mem_table->header, DTS_SIG_RSV_MEM,
                         UBIOS_RSV_MEM_TABLE_SIZE(rsv_mem_table->count),
                         UBIOS_VERSION);
    mem_range = (MemRange *)rsv_mem_table->node;
    mem_range->flags = 0x1; /* direct mapping */
    memset(mem_range->reserved, 0, sizeof(mem_range->reserved));
    mem_range->base = 0x8000000;  /* MSI_IOVA_BASE */
    mem_range->size = 0x100000;  /* MSI_IOVA_LENGTH */
}

void ub_init_ubios_info_table(uint64_t total_size)
{
    VirtMachineState *vms = VIRT_MACHINE(qdev_get_machine());
    uint64_t ubios_info_tables = vms->memmap[VIRT_UBIOS_INFO_TABLE].base;
    uint64_t ubc_tables_addr = ubios_info_tables + UBIOS_INFO_TABLE_SIZE;
    uint64_t ummu_tables_addr;
    uint64_t size = total_size;
    DtsRootTable *ubios = (DtsRootTable *)cpu_physical_memory_map(ubios_info_tables,
                                                                  &size, true);
    DtsSubUbcTable *ubc_table = (DtsSubUbcTable *)(ubios + 1);
    uint64_t ubc_table_size;
    DtsSubUmmuTable *ummu_table;
    uint64_t ummu_table_size;
    uint64_t rsv_mem_tables_addr;
    DtsRsvMemTable *rsv_mem_table;

    if (!ubios || size != total_size) {
        if (ubios) {
            cpu_physical_memory_unmap(ubios, size, true, size);
        }
        qemu_log("cpu_physical_memory_map failed, size %lu total %lu ptr %p\n",
                 size, total_size, ubios);
        return;
    }
    qemu_log("ubios_info_tables=0x%lx, ubc_tables_addr=0x%lx,"
             "ubios table size=%lu, UBIOS_UBC_TABLE_CNT %u,"
             "UBIOS_UMMU_TABLE_CNT %u\n",
             ubios_info_tables, ubc_tables_addr, total_size,
             UBIOS_UBC_TABLE_CNT, UBIOS_UMMU_TABLE_CNT);
    memset(ubios, 0, sizeof(DtsRootTable));
    ub_init_table_header(&ubios->header, "ubios root",
                         sizeof(DtsRootTable), UBIOS_VERSION);
    /* init ubc table */
    ubios->tables[ubios->count] = ubc_tables_addr;
    ub_init_ubios_ubc_table(ubc_table, vms);
    qemu_log("ubc ubios->tables[%u] = 0x%lx ubc_table = 0x%lx \n",
             ubios->count, ubc_tables_addr, (uint64_t)ubc_table);
    ubios->count++;
    ubc_table_size = UBIOS_UBC_TABLE_SIZE(ubc_table->ubc_count);

    /* init ummu table */
    ummu_tables_addr = ubc_tables_addr + ALIGN_UP(ubc_table_size, UB_ALIGNMENT);
    ummu_table = (DtsSubUmmuTable *)((uint8_t *)(ubc_table) +
                 ALIGN_UP(ubc_table_size, UB_ALIGNMENT));
    ubios->tables[ubios->count] = ummu_tables_addr;
    ub_init_ubios_ummu_table(ummu_table, vms);
    qemu_log("ummu ubios->tables[%u] = 0x%lx ummu_table=0x%lx\n",
             ubios->count, ummu_tables_addr, (uint64_t)ummu_table);
    ubios->count++;
    ummu_table_size = UBIOS_UMMU_TABLE_SIZE(UBIOS_UMMU_TABLE_CNT);

    /* init rsv mem table */
    rsv_mem_tables_addr = ummu_tables_addr + ALIGN_UP(ummu_table_size, UB_ALIGNMENT);
    rsv_mem_table = (DtsRsvMemTable *)((uint8_t *)(ummu_table) +
                 ALIGN_UP(ummu_table_size, UB_ALIGNMENT));
    ubios->tables[ubios->count] = rsv_mem_tables_addr;
    ub_init_ubios_rsv_mem_table(rsv_mem_table, vms);
    ubios->count++;

    cpu_physical_memory_unmap(ubios, size, true, size);
}

void ub_set_ubinfo_in_ubc_table(void)
{
    VirtMachineState *vms = VIRT_MACHINE(qdev_get_machine());
    uint64_t ubios_info_tables = vms->memmap[VIRT_UBIOS_INFO_TABLE].base;
    uint64_t total_size = ROUND_UP(UBIOS_TABLE_SIZE, 4 * KiB);
    uint64_t size = total_size;
    UBBus *bus = vms->ub_bus;

    if (!bus) {
        qemu_log("there is no ub bus\n");
        return;
    }

    BusControllerState *ubc = container_of_ubbus(bus);
    UbGuid guid = ubc->ubc_dev->parent.guid;
    DtsRootTable *ubios = (DtsRootTable *)cpu_physical_memory_map(ubios_info_tables,
                                                                  &size, true);
    DtsSubUbcTable *ubc_table = (DtsSubUbcTable *)(ubios + 1);
    UbcNode *ubc_node = (UbcNode *)ubc_table->node;

    if (!ubios || size != total_size) {
        if (ubios) {
            cpu_physical_memory_unmap(ubios, size, true, size);
        }
        qemu_log("cpu_physical_memory_map failed, size %lu total %lu ptr %p\n",
                 size, total_size, ubios);
        return;
    }
    /* The virtual machine currently supports only one ub controller. */
    ubc_node->ubc_info = guid;

    cpu_physical_memory_unmap(ubios, size, true, size);
}

static void ub_idev_ers_address_space_manage_init(void)
{
    VirtMachineState *vms = (VirtMachineState *)current_machine;
    UBIdevErsAddrSpaceNode *free_node = NULL;

    g_idevErsAddrSpaceManage.base_addr = vms->memmap[VIRT_UB_IDEV_ERS].base;
    g_idevErsAddrSpaceManage.size = vms->memmap[VIRT_UB_IDEV_ERS].size;

    QTAILQ_INIT(&g_idevErsAddrSpaceManage.as_free_list);
    QTAILQ_INIT(&g_idevErsAddrSpaceManage.as_used_list);

    free_node = g_new0(UBIdevErsAddrSpaceNode, 1);
    free_node->size = g_idevErsAddrSpaceManage.size;
    free_node->offset = 0;
    QTAILQ_INSERT_TAIL(&g_idevErsAddrSpaceManage.as_free_list, free_node, stailq_free);
    qemu_log("ub idev ers address space manage init success, base_addr: 0x%lx size: 0x%lx\n",
             g_idevErsAddrSpaceManage.base_addr, g_idevErsAddrSpaceManage.size);
}

static bool ers_addr_size_is_validate(uint64_t size, uint32_t sys_pgs)
{
    uint64_t max_support_size;

    if (!sys_pgs) {
        max_support_size = UINT64_MAX / UB_CFG1_BASIC_SYSTEM_GRANULE_SIZE_4K;
    } else {
        max_support_size = UINT64_MAX / UB_CFG1_BASIC_SYSTEM_GRANULE_SIZE_64K;
    }

    if (size >= max_support_size) {
        qemu_log("ers addr size %" PRIu64 " is too big, expect size < %" PRIu64 "\n",
                 size, max_support_size);
        return false;
    }

    return true;
}

hwaddr ub_idev_ers_alloc_address_space(uint64_t size, uint32_t sys_pgs)
{
    UBIdevErsAddrSpaceNode *free_node = NULL;
    UBIdevErsAddrSpaceNode *selected_free_node = NULL;
    UBIdevErsAddrSpaceNode *used_node = NULL;
    uint64_t need_node_size;
    uint64_t free_node_base_addr;
    uint64_t allocated_base_addr;
    uint64_t allocated_diff;

    if (!g_idevErsAddrSpaceManage.init) {
        g_idevErsAddrSpaceManage.init = true;
        ub_idev_ers_address_space_manage_init();
    }

    if (!ers_addr_size_is_validate(size, sys_pgs)) {
        return UINT64_MAX;
    }

    /* according UB Spec, if sys_pgs 0, unit is 4Kbytes, then unit is 64Kbytes */
    if (!sys_pgs) {
        size *= UB_CFG1_BASIC_SYSTEM_GRANULE_SIZE_4K;
    } else {
        size *= UB_CFG1_BASIC_SYSTEM_GRANULE_SIZE_64K;
    }

    QTAILQ_FOREACH(free_node, &g_idevErsAddrSpaceManage.as_free_list, stailq_free) {
        if (free_node->size < size) {
            continue;
        }

        free_node_base_addr = g_idevErsAddrSpaceManage.base_addr + free_node->offset;
        /* allocated base addr need align to allocated size */
        allocated_base_addr = ALIGN_UP(free_node_base_addr, size);
        allocated_diff = allocated_base_addr - free_node_base_addr;
        need_node_size = allocated_diff + size;
        if (free_node->size < need_node_size) {
            continue;
        }

        if (selected_free_node && selected_free_node->size < free_node->size) {
            continue;
        }

        selected_free_node = free_node;
        if (!used_node) {
            /* create used node */
            used_node = g_new0(UBIdevErsAddrSpaceNode, 1);
        }
        used_node->offset = selected_free_node->offset;
        used_node->allocated_offset = used_node->offset + allocated_diff;
        used_node->size = size + allocated_diff;
    }

    if (!selected_free_node) {
        g_free(used_node);
        return UINT64_MAX;
    }

    /* adjust free node */
    if (selected_free_node->size - size < UB_CFG1_BASIC_SYSTEM_GRANULE_SIZE_4K) {
        used_node->size = selected_free_node->size;
        QTAILQ_REMOVE(&g_idevErsAddrSpaceManage.as_free_list, selected_free_node, stailq_free);
        g_free(selected_free_node);
    } else {
        selected_free_node->size -= used_node->size;
        selected_free_node->offset += used_node->size;
    }

    QTAILQ_INSERT_TAIL(&g_idevErsAddrSpaceManage.as_used_list, used_node, stailq_used);

    return allocated_base_addr;
}

void ub_idev_ers_free_address_space(hwaddr offset)
{
    UBIdevErsAddrSpaceNode *used_node = NULL;
    UBIdevErsAddrSpaceNode *free_node = NULL;
    UBIdevErsAddrSpaceNode *next_free_node = NULL;
    uint64_t as_offset = offset - g_idevErsAddrSpaceManage.base_addr;

    QTAILQ_FOREACH(used_node, &g_idevErsAddrSpaceManage.as_used_list, stailq_used) {
        if (used_node->allocated_offset == as_offset) {
            QTAILQ_REMOVE(&g_idevErsAddrSpaceManage.as_used_list, used_node, stailq_used);
            break;
        }
    }

    if (!used_node) {
        qemu_log("idev ers address space free failed, unable to find offset 0x%lx.\n", offset);
        return;
    }

    /* adjust free node list */
    /* case 1: as free list is empty */
    if (QTAILQ_EMPTY(&g_idevErsAddrSpaceManage.as_free_list)) {
        QTAILQ_INSERT_HEAD(&g_idevErsAddrSpaceManage.as_free_list, used_node, stailq_free);
        return;
    }

    /* case 2: freed used_node->offset is minial  */
    free_node = QTAILQ_FIRST(&g_idevErsAddrSpaceManage.as_free_list);
    if (used_node->offset + used_node->size < free_node->offset) {
        QTAILQ_INSERT_HEAD(&g_idevErsAddrSpaceManage.as_free_list, used_node, stailq_free);
        return;
    } else if (used_node->offset + used_node->size == free_node->offset) { /* merge to first free node */
        free_node->offset = used_node->offset;
        free_node->size += used_node->size;
        g_free(used_node);
        return;
    }

    /* case 3: foreach all free node, insert freed address space to free node in order */
    QTAILQ_FOREACH(free_node, &g_idevErsAddrSpaceManage.as_free_list, stailq_free) {
        next_free_node = QTAILQ_NEXT(free_node, stailq_free);
        if (!next_free_node) {
            if (free_node->offset + free_node->size < used_node->offset) {
                QTAILQ_INSERT_TAIL(&g_idevErsAddrSpaceManage.as_free_list, used_node, stailq_free);
            } else if (free_node->offset + free_node->size == used_node->offset) {
                free_node->size += used_node->size;
                g_free(used_node);
            }
            return;
        }

        if (used_node->offset >= next_free_node->offset + next_free_node->size) {
            continue;
        }

        if (free_node->offset + free_node->size == used_node->offset &&
            used_node->offset + used_node->size < next_free_node->offset) {
            free_node->size += used_node->size;
            g_free(used_node);
            return;
        } else if (free_node->offset + free_node->size < used_node->offset &&
                   used_node->offset + used_node->size == next_free_node->offset) {
            next_free_node->offset = used_node->offset;
            next_free_node->size += used_node->size;
            g_free(used_node);
            return;
        } else if (free_node->offset + free_node->size < used_node->offset &&
                   used_node->offset + used_node->size < next_free_node->offset) {
            QTAILQ_INSERT_AFTER(&g_idevErsAddrSpaceManage.as_free_list, free_node, used_node, stailq_free);
            return;
        } else {
            next_free_node->offset = free_node->offset;
            next_free_node->size += free_node->size;
            next_free_node->size += used_node->size;
            QTAILQ_REMOVE(&g_idevErsAddrSpaceManage.as_free_list, free_node, stailq_free);
            g_free(used_node);
            g_free(free_node);
            return;
        }
    }
}

void build_ubrt(GArray *table_data, BIOSLinker *linker)
{
    VirtMachineState *vms = VIRT_MACHINE(qdev_get_machine());
    /* 3 subtables: ubc, ummu, UB Reserved Memory */
    uint8_t table_cnt = 3;
    uint64_t ubios_info_tables = vms->memmap[VIRT_UBIOS_INFO_TABLE].base;
    uint64_t ubc_tables_addr = ubios_info_tables + UBIOS_INFO_TABLE_SIZE;
    uint64_t ubc_table_size = UBIOS_UBC_TABLE_SIZE(UBIOS_UBC_TABLE_CNT);
    uint64_t ummu_tables_addr = ubc_tables_addr + ALIGN_UP(ubc_table_size, UB_ALIGNMENT);
    uint64_t ummu_table_size = UBIOS_UMMU_TABLE_SIZE(UBIOS_UMMU_TABLE_CNT);
    uint64_t rsv_mem_tables_addr = ummu_tables_addr + ALIGN_UP(ummu_table_size, UB_ALIGNMENT);
    AcpiTable table = { .sig = "UBRT", .rev = 0, .oem_id = vms->oem_id,
                        .oem_table_id = vms->oem_table_id };

    acpi_table_begin(&table, table_data);
    build_append_int_noprefix(table_data, table_cnt, 4);

    build_append_int_noprefix(table_data, ACPI_UB_TABLE_TYPE_BUS_CONTROLLER, 1);
    build_append_int_noprefix(table_data, 0, 7);
    build_append_int_noprefix(table_data, ubc_tables_addr, 8);

    build_append_int_noprefix(table_data, ACPI_UB_TABLE_TYPE_UMMU, 1);
    build_append_int_noprefix(table_data, 0, 7);
    build_append_int_noprefix(table_data, ummu_tables_addr, 8);

    build_append_int_noprefix(table_data, ACPI_UB_TABLE_TYPE_RSV_MEM, 1);
    build_append_int_noprefix(table_data, 0, 7);
    build_append_int_noprefix(table_data, rsv_mem_tables_addr, 8);

    acpi_table_end(linker, &table);
    qemu_log("init UBRT: ubc_tbl=0x%lx, ummu_tbl=0x%lx, rsv_mem_tbl=0x%lx\n",
             ubc_tables_addr, ummu_tables_addr, rsv_mem_tables_addr);
}

void acpi_dsdt_add_ub(Aml *scope)
{
    Aml *dev_ubc = aml_device("UBC0");
    Aml *dev_ummu = aml_device("UMU0");
    Aml *dev_pmu = aml_device("PMU0");

    aml_append(dev_ubc, aml_name_decl("_HID", aml_string("HISI0541")));
    aml_append(dev_ubc, aml_name_decl("_UID", aml_int(0)));
    aml_append(scope, dev_ubc);

    aml_append(dev_ummu, aml_name_decl("_HID", aml_string("HISI0551")));
    aml_append(dev_ummu, aml_name_decl("_UID", aml_int(0)));
    aml_append(scope, dev_ummu);

    aml_append(dev_pmu, aml_name_decl("_HID", aml_string("HISI0571")));
    aml_append(dev_pmu, aml_name_decl("_UID", aml_int(0)));
    aml_append(scope, dev_pmu);
}

void acpi_iort_add_ub(GArray *table_data)
{
    char name_ubc[11] = "\\_SB_.UBC0";
    char name_ummu[11] = "\\_SB_.UMU0";
    char name_pmu[11] = "\\_SB_.PMU0";
    int name_ubc_len = sizeof(name_ubc);
    int name_ummu_len = sizeof(name_ummu);
    int name_pmu_len = sizeof(name_pmu);

    /* Table 16 UBC */
    build_append_int_noprefix(table_data, 1 /* Named component */, 1); /* Type */
    build_append_int_noprefix(table_data, 0x40, 2); /* Length */
    build_append_int_noprefix(table_data, 0, 1); /* Revision */
    build_append_int_noprefix(table_data, 0, 4); /* Identifier */
    build_append_int_noprefix(table_data, 1, 4); /* Number of ID mappings */
    build_append_int_noprefix(table_data, 0x2c, 4); /* Reference to ID Array */
    /* Named component specific data */
    build_append_int_noprefix(table_data, 0, 4); /* Node Flags */
    build_append_int_noprefix(table_data, 0, 4); /* Memory access properties: Cache Coherency */
    build_append_int_noprefix(table_data, 0, 1); /* Memory access properties: Hints */
    build_append_int_noprefix(table_data, 0, 2); /* Memory access properties: Reserved */
    build_append_int_noprefix(table_data, 0, 1); /* Memory access properties: Memory Flags */
    build_append_int_noprefix(table_data, 0, 1); /* Memory Size Limit */
    g_array_append_vals(table_data, name_ubc, name_ubc_len); /* Device object name */
    build_append_int_noprefix(table_data, 0, 4); /* Padding */
    build_append_int_noprefix(table_data, 0, 4); /* Input base */
    build_append_int_noprefix(table_data, 1, 4); /* Number of IDs */
    build_append_int_noprefix(table_data, UBC_INTERRUPT_ID_START, 4); /* Output base */
    build_append_int_noprefix(table_data, 0x30, 4); /* Output Reference */
    build_append_int_noprefix(table_data, 1, 4); /* Flags */

    /* Table 16 UMMU */
    build_append_int_noprefix(table_data, 1 /* Named component */, 1); /* Type */
    build_append_int_noprefix(table_data, 0x40, 2); /* Length */
    build_append_int_noprefix(table_data, 0, 1); /* Revision */
    build_append_int_noprefix(table_data, 0, 4); /* Identifier */
    build_append_int_noprefix(table_data, 1, 4); /* Number of ID mappings */
    build_append_int_noprefix(table_data, 0x2c, 4); /* Reference to ID Array */
    /* Named component specific data */
    build_append_int_noprefix(table_data, 0, 4); /* Node Flags */
    build_append_int_noprefix(table_data, 0, 4); /* Memory access properties: Cache Coherency */
    build_append_int_noprefix(table_data, 0, 1); /* Memory access properties: Hints */
    build_append_int_noprefix(table_data, 0, 2); /* Memory access properties: Reserved */
    build_append_int_noprefix(table_data, 0, 1); /* Memory access properties: Memory Flags */
    build_append_int_noprefix(table_data, 0, 1); /* Memory Size Limit */
    g_array_append_vals(table_data, name_ummu, name_ummu_len); /* Device object name */
    build_append_int_noprefix(table_data, 0, 4); /* Padding */
    build_append_int_noprefix(table_data, 0, 4); /* Input base */
    build_append_int_noprefix(table_data, 1, 4); /* Number of IDs */
    build_append_int_noprefix(table_data, UMMU_INTERRUPT_ID, 4); /* Output base */
    build_append_int_noprefix(table_data, 0x30, 4); /* Output Reference */
    build_append_int_noprefix(table_data, 1, 4); /* Flags */

    /* Table 16 PMU */
    build_append_int_noprefix(table_data, 1 /* Named component */, 1); /* Type */
    build_append_int_noprefix(table_data, 0x40, 2); /* Length */
    build_append_int_noprefix(table_data, 0, 1); /* Revision */
    build_append_int_noprefix(table_data, 0, 4); /* Identifier */
    build_append_int_noprefix(table_data, 1, 4); /* Number of ID mappings */
    build_append_int_noprefix(table_data, 0x2c, 4); /* Reference to ID Array */
    /* Named component specific data */
    build_append_int_noprefix(table_data, 0, 4); /* Node Flags */
    build_append_int_noprefix(table_data, 0, 4); /* Memory access properties: Cache Coherency */
    build_append_int_noprefix(table_data, 0, 1); /* Memory access properties: Hints */
    build_append_int_noprefix(table_data, 0, 2); /* Memory access properties: Reserved */
    build_append_int_noprefix(table_data, 0, 1); /* Memory access properties: Memory Flags */
    build_append_int_noprefix(table_data, 0, 1); /* Memory Size Limit */
    g_array_append_vals(table_data, name_pmu, name_pmu_len); /* Device object name */
    build_append_int_noprefix(table_data, 0, 4); /* Padding */
    build_append_int_noprefix(table_data, 0, 4); /* Input base */
    build_append_int_noprefix(table_data, 1, 4); /* Number of IDs */
    build_append_int_noprefix(table_data, UMMU_INTERRUPT_ID + 1, 4); /* Output base */
    build_append_int_noprefix(table_data, 0x30, 4); /* Output Reference */
    build_append_int_noprefix(table_data, 1, 4); /* Flags */
}
