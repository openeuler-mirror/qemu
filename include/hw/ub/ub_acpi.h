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

#ifndef HW_UB_ACPI_H
#define HW_UB_ACPI_H
#include "hw/acpi/acpi-defs.h"
#include "hw/acpi/bios-linker-loader.h"
#include "hw/acpi/aml-build.h"
#include "hw/acpi/utils.h"
#include "hw/ub/ub.h"

#define DTS_TABLE_HEADER_RESERVE_LEN 3
#define DTS_ROOT_TABLE_RESERVE_LEN 6
#define DTS_TABLE_HEADER_NAME_LEN 16
/* ummu reserved tid num , don't modify */
#define UMMU_RESERVED_TID_NUM 64

#define UBIOS_UBC_TABLE_CNT 1
#define UBIOS_UMMU_TABLE_CNT 1
#define UBIOS_RSV_MEM_TABLE_CNT 1
#define UBIOS_CALL_ID_SERVICE_TABLE_CNT 1
#define UBIOS_TABLE_TOTAL_CNT (UBIOS_UBC_TABLE_CNT + UBIOS_UMMU_TABLE_CNT + UBIOS_RSV_MEM_TABLE_CNT + UBIOS_CALL_ID_SERVICE_TABLE_CNT)

typedef struct DtsTableHeader {
    char name[DTS_TABLE_HEADER_NAME_LEN];
    uint32_t total_size;
    uint8_t version;
    uint8_t reserved[DTS_TABLE_HEADER_RESERVE_LEN];
    uint32_t remain_size;
    uint32_t checksum;
} DtsTableHeader;

/* DTS UBIOS INFO TABLE */
typedef struct DtsRootTable {
    DtsTableHeader header;
    uint16_t count;
    uint8_t reserved[DTS_ROOT_TABLE_RESERVE_LEN];
    uint64_t tables[UBIOS_TABLE_TOTAL_CNT];
} DtsRootTable;

#define UBC_QUEUE_INTERRUPT_DEFAULT 443

#define UBC_VENDOR_INFO_LEN 256
/* ub controller block */
typedef struct UbcNode {
    uint32_t interrupt_id_start;
    uint32_t interrupt_id_end;
    uint64_t gpa_base;
    uint64_t gpa_size;
    uint8_t memory_size_limit;
    uint8_t dma_cca;  /* 0: DMA(y) CCA(N) ; 1: DMA(Y) CCA(Y); other: DMA(N) */
    uint16_t ummu_mapping;
    uint16_t proximity_domain;
    uint8_t reserved1[2];
    uint64_t msg_queue_base;
    uint64_t msg_queue_size;
    uint16_t msg_queue_depth;
    uint16_t msg_queue_interrupt;
    uint8_t msg_queue_interrupt_attr;
    uint8_t reserved2[59];
    UbGuid ubc_info;  /* UB controller's GUID */
    uint8_t vendor_info[UBC_VENDOR_INFO_LEN];  /* vendor private info */
} UbcNode;

#define UMMU_VEND_LEN 80
typedef struct UmmuNode {
    uint64_t base_addr;
    uint64_t addr_size;
    uint32_t interrupt_id;
    uint16_t proximity_domain;
    uint16_t its_index;
    uint64_t pmu_addr;
    uint64_t pmu_size;
    uint32_t pmu_interrupt_id;
    uint32_t min_tid;
    uint32_t max_tid;
    uint8_t reserved2[26];
    uint16_t vender_id;
    uint8_t vender_info[UMMU_VEND_LEN];
} UmmuNode;

/*  UMMU   table */
typedef struct DtsSubUmmuTable {
    DtsTableHeader header;
    uint32_t count;
    uint32_t flag;
    UmmuNode node[0];
} DtsSubUmmuTable;

#define LOCAL_CNA_START 1
#define LOCAL_CNA_END 65535
#define LOCAL_EID_START 1
#define LOCAL_EID_END 65535

/* UB Controller table */
typedef struct DtsSubUbcTable {
    DtsTableHeader header;
    uint32_t local_cna_start;
    uint32_t local_cna_end;
    uint32_t local_eid_start;
    uint32_t local_eid_end;
    uint8_t feature_set;
    uint8_t reserved[3];
    uint16_t cluster_mode;
    uint16_t ubc_count;
    UbcNode node[0];
} DtsSubUbcTable;

typedef struct MemRange {
    uint8_t flags;
    uint8_t reserved[7];
    uint64_t base;
    uint64_t size;
} MemRange;
/* UB Reserved Memory table */
typedef struct DtsRsvMemTable {
    DtsTableHeader header;
    uint16_t count;
    uint8_t reserved[6];
    MemRange node[0];
} DtsRsvMemTable;

/* UBRT subtable */
typedef struct UbrtSubtable {
    uint8_t type;
    uint8_t reserved[7];
    uint64_t pointer;
} UbrtSubtable;

typedef struct acpi_table_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    char asl_compiler_id[4];
    uint32_t asl_compiler_revision;
} ACPI_TABLE_HEADER;

/* UBRT table */
typedef struct AcpiUbrtTable {
    ACPI_TABLE_HEADER header;
    uint32_t count;
    UbrtSubtable subtables[];
} AcpiUbrtTable;
#define ACPI_UB_TABLE_TYPE_BUS_CONTROLLER      0
#define ACPI_UB_TABLE_TYPE_UMMU                1
#define ACPI_UB_TABLE_TYPE_RSV_MEM             2
#define ACPI_UB_TABLE_TYPE_VIRTUAL_BUS         3
#define ACPI_UB_TABLE_TYPE_CALL_ID_SERVICE     4
#define ACPI_UB_TABLE_TYPE_DEVICE              5
#define ACPI_UB_TABLE_TYPE_TOPOLOGY            6

#define UBIOS_MMIOS_SIZE_PER_UBC (512 * GiB)
#define UBIOS_INFO_TABLE_SIZE (sizeof(DtsRootTable))

/* Call ID Service table constants */
#define UB_CALL_ID_USAGE_UB_MSG     3
#define UB_CALL_ID_OWNER_OS         0x20000000
#define DTS_SIG_CALL_ID_SERVICE     "call_id_service"
#define UB_CIS_INFO_QUERY 0xC00B0040
#define UB_CIS_INFO_REFRESH 0xC00B0041

/* ---- ODS (Object Description Structure) builder helpers ----
 * Encodes the CIS table in the self-describing binary format consumed by the
 * Guest ODF driver (drivers/firmware/uvb/odf/).  Each member stores its name
 * (NUL-terminated string), a 1-byte type, and data; composite types carry a
 * 4-byte data_length between type and data so the consumer can skip forward.
 */
#define ODS_TYPE_U8      0x01
#define ODS_TYPE_U16     0x02
#define ODS_TYPE_U32     0x03
#define ODS_TYPE_STRUCT  0x30
#define ODS_TYPE_LIST    0x80  /* flag bit, OR'd with element type */

#define UBIOS_UBC_TABLE_SIZE(cnt) (sizeof(DtsSubUbcTable) + (cnt) * sizeof(UbcNode))
#define UBIOS_UMMU_TABLE_SIZE(cnt) (sizeof(DtsSubUmmuTable) + (cnt) * sizeof(UmmuNode))
#define UBIOS_RSV_MEM_TABLE_SIZE(cnt) (sizeof(DtsRsvMemTable) + (cnt) * sizeof(MemRange))
#define UBIOS_CALL_ID_SERVICE_TABLE_SIZE  256

#define UBIOS_TABLE_SIZE (UBIOS_INFO_TABLE_SIZE + \
                          UBIOS_UBC_TABLE_SIZE(UBIOS_UBC_TABLE_CNT) + \
                          UBIOS_UMMU_TABLE_SIZE(UBIOS_UMMU_TABLE_CNT) + \
                          UBIOS_RSV_MEM_TABLE_SIZE(UBIOS_UMMU_TABLE_CNT) + \
                          UBIOS_CALL_ID_SERVICE_TABLE_SIZE)

void ub_init_ubios_info_table(uint64_t total_size);
hwaddr ub_idev_ers_alloc_address_space(uint64_t size, uint32_t sys_pgs);
void ub_idev_ers_free_address_space(hwaddr offset);
void ub_set_gpa_bits(uint8_t bits);
void build_ubrt(GArray *table_data, BIOSLinker *linker);
void ub_set_ubinfo_in_ubc_table(void);
void acpi_dsdt_add_ub(Aml *scope);
void acpi_iort_add_ub(GArray *table_data);
#endif