/* Support for generating ACPI tables and passing them to Guests
 *
 * ARM virt ACPI generation
 *
 * Copyright (C) 2008-2010  Kevin O'Connor <kevin@koconnor.net>
 * Copyright (C) 2006 Fabrice Bellard
 * Copyright (C) 2013 Red Hat Inc
 *
 * Author: Michael S. Tsirkin <mst@redhat.com>
 *
 * Copyright (c) 2015 HUAWEI TECHNOLOGIES CO.,LTD.
 *
 * Author: Shannon Zhao <zhaoshenglong@huawei.com>
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
#include "qapi/error.h"
#include "qemu/bitmap.h"
#include "qemu/error-report.h"
#include "trace.h"
#include "hw/core/cpu.h"
#include "target/arm/cpu.h"
#include "hw/acpi/acpi-defs.h"
#include "hw/acpi/acpi.h"
#include "hw/nvram/fw_cfg_acpi.h"
#include "hw/acpi/bios-linker-loader.h"
#include "hw/acpi/aml-build.h"
#include "hw/acpi/utils.h"
#include "hw/acpi/pci.h"
#include "hw/acpi/memory_hotplug.h"
#include "hw/acpi/generic_event_device.h"
#include "hw/acpi/tpm.h"
#include "hw/acpi/hmat.h"
#include "hw/pci/pcie_host.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci-host/gpex.h"
#include "hw/arm/virt.h"
#include "hw/intc/arm_gicv3_its_common.h"
#include "hw/mem/nvdimm.h"
#include "hw/platform-bus.h"
#include "sysemu/numa.h"
#include "sysemu/reset.h"
#include "sysemu/tpm.h"
#include "migration/vmstate.h"
#include "hw/acpi/ghes.h"
#include "hw/acpi/viot.h"
#include "kvm_arm.h"
#include "hw/virtio/virtio-acpi.h"
#ifdef CONFIG_PAS_EXPANSION
#include "hw/vfio/vfio-hisi-mmcd.h"
#endif
#ifdef CONFIG_UB
#include "hw/ub/ub_acpi.h"
#endif

#define ARM_SPI_BASE 32

#define ACPI_BUILD_TABLE_SIZE             0x20000

/*
 * PPTT Cache Type Structure (Type 1) constants
 * ACPI spec, Revision 6.3, 5.2.29.2
 */
#define PPTT_CACHE_NODE_TYPE             1
#define PPTT_CACHE_NODE_LENGTH           24

/* Field sizes in bytes */
#define PPTT_CACHE_RESERVED_BYTES        2
#define PPTT_CACHE_FLAGS_BYTES           4
#define PPTT_CACHE_NEXT_LEVEL_BYTES      4
#define PPTT_CACHE_SIZE_BYTES            4
#define PPTT_CACHE_SETS_BYTES            4
#define PPTT_CACHE_LINESIZE_BYTES        2

/* Attributes byte: bits [1:0] = allocation policy, bits [3:2] = cache type */
#define PPTT_CACHE_ATTR_ALLOC_POLICY     0x3
#define PPTT_CACHE_TYPE_SHIFT            2
#define PPTT_CACHE_TYPE_DATA_VAL         0
#define PPTT_CACHE_TYPE_INSTR_VAL        1
#define PPTT_CACHE_TYPE_UNIFIED_VAL      3

/* Number of private resources when I/D offsets differ or are unified */
#define PPTT_PRIV_RSRC_UNIFIED           1
#define PPTT_PRIV_RSRC_SEPARATE          2

/* Highest cache level supported (L3) */
#define PPTT_MAX_CACHE_LEVEL             3

/*
 * Constant context for PPTT cache node construction. These values never
 * change during the table build, so bundling them avoids passing four
 * separate arguments through every helper call.
 */
typedef struct PpttCacheCtx {
    GArray *table_data;
    uint32_t pptt_start;
    CPUCoreCaches *caches;
    int num_caches;
} PpttCacheCtx;

/*
 * Result of building caches at one topology level (socket/cluster/core).
 * Filled by build_topo_caches(), consumed by build_pptt_arm().
 */
typedef struct PpttLevelResult {
    uint32_t priv_rsrc[2];  /* I/D cache node offsets for hierarchy node */
    uint32_t num_priv;       /* count of valid entries in priv_rsrc */
    int bottom_level;        /* lowest cache level found at this topo level */
    bool found;              /* whether any cache exists at this topo level */
} PpttLevelResult;

/*
 * Hardcoded cache geometry per CacheLevelAndType, from virt.h macros.
 *
 * We read from hardcoded macros instead of KVM registers because KVM does not
 * support reading CCSIDR_EL1 via KVM_GET_ONE_REG (returns ENOENT).
 *
 * The values match the host hardware cache geometry (verified via sysfs).
 */
static const struct {
    enum CpuCacheType type;
    uint32_t level;
    uint32_t default_sz;
    uint32_t sets;
    uint16_t linesize;
    uint8_t associativity;
    uint8_t attributes;
} arm_cache_defaults[CACHE_LEVEL_AND_TYPE__MAX] = {
    [CACHE_LEVEL_AND_TYPE_L1D] = {
        CPU_CACHE_DATA, CACHE_LEVEL_L1,
        ARM_L1DCACHE_SIZE, ARM_L1DCACHE_SETS,
        ARM_L1DCACHE_LINE_SIZE, ARM_L1DCACHE_ASSOCIATIVITY,
        ARM_L1DCACHE_ATTRIBUTES,
    },
    [CACHE_LEVEL_AND_TYPE_L1I] = {
        CPU_CACHE_INSTRUCTION, CACHE_LEVEL_L1,
        ARM_L1ICACHE_SIZE, ARM_L1ICACHE_SETS,
        ARM_L1ICACHE_LINE_SIZE, ARM_L1ICACHE_ASSOCIATIVITY,
        ARM_L1ICACHE_ATTRIBUTES,
    },
    [CACHE_LEVEL_AND_TYPE_L1] = {
        CPU_CACHE_UNIFIED, CACHE_LEVEL_L1,
        ARM_L1CACHE_SIZE, ARM_L1CACHE_SETS,
        ARM_L1CACHE_LINE_SIZE, ARM_L1CACHE_ASSOCIATIVITY,
        ARM_L1CACHE_ATTRIBUTES,
    },
    [CACHE_LEVEL_AND_TYPE_L2] = {
        CPU_CACHE_UNIFIED, CACHE_LEVEL_L2,
        ARM_L2CACHE_SIZE, ARM_L2CACHE_SETS,
        ARM_L2CACHE_LINE_SIZE, ARM_L2CACHE_ASSOCIATIVITY,
        ARM_L2CACHE_ATTRIBUTES,
    },
    [CACHE_LEVEL_AND_TYPE_L3] = {
        CPU_CACHE_UNIFIED, CACHE_LEVEL_L3,
        ARM_L3CACHE_SIZE, ARM_L3CACHE_SETS,
        ARM_L3CACHE_LINE_SIZE, ARM_L3CACHE_ASSOCIATIVITY,
        ARM_L3CACHE_ATTRIBUTES,
    },
};

static void virt_fill_cache(CPUCoreCaches *cache, CacheLevelAndType key)
{
    const typeof(arm_cache_defaults[0]) *d = &arm_cache_defaults[key];

    cache->type = d->type;
    cache->level = d->level;
    cache->size = d->default_sz;
    cache->sets = d->sets;
    cache->linesize = d->linesize;
    cache->associativity = d->associativity;
    cache->attributes = d->attributes;
}

static unsigned int virt_get_caches(const VirtMachineState *vms,
                                    CPUCoreCaches *caches)
{
    const MachineState *ms = MACHINE(vms);
    int n = 0;

    bool has_l1 = ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L1].topology
                  != CPU_TOPOLOGY_LEVEL_DEFAULT;
    bool has_l1d = ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L1D].topology
                   != CPU_TOPOLOGY_LEVEL_DEFAULT;
    bool has_l1i = ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L1I].topology
                   != CPU_TOPOLOGY_LEVEL_DEFAULT;

    /*
     * L1 cache nodes must match the CLIDR value computed by
     * virt_get_vcpu_clidr() in virt.c. Only build cache nodes for
     * types that the user has configured. When no L1 cache is
     * configured, fall back to hardware CLIDR.
     */
    if (has_l1) {
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L1);
    } else if (has_l1d && has_l1i) {
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L1D);
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L1I);
    } else if (has_l1d) {
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L1D);
    } else if (has_l1i) {
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L1I);
    } else if (cpu_l1_cache_unified(0)) {
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L1);
    } else {
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L1D);
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L1I);
    }

    /* L2: only if configured */
    if (ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L2].topology
        != CPU_TOPOLOGY_LEVEL_DEFAULT) {
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L2);
    }

    /* L3: only if configured */
    if (ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L3].topology
        != CPU_TOPOLOGY_LEVEL_DEFAULT) {
        virt_fill_cache(&caches[n++], CACHE_LEVEL_AND_TYPE_L3);
    }

    return n;
}

/*
 * ACPI spec, Revision 6.3
 * 5.2.29.2 Cache Type Structure (Type 1)
 */
static void build_cache_nodes(GArray *tbl, CPUCoreCaches *cache,
                              uint32_t next_offset)
{
    int start_len = tbl->len;
    int val;

    build_append_byte(tbl, PPTT_CACHE_NODE_TYPE); /* Type 1 - cache */
    build_append_byte(tbl, PPTT_CACHE_NODE_LENGTH); /* Length */
    build_append_int_noprefix(tbl, 0, PPTT_CACHE_RESERVED_BYTES); /* Reserved */
    build_append_int_noprefix(tbl, 0x7f, PPTT_CACHE_FLAGS_BYTES); /* Flags */
    build_append_int_noprefix(tbl, next_offset, PPTT_CACHE_NEXT_LEVEL_BYTES);
    build_append_int_noprefix(tbl, cache->size, PPTT_CACHE_SIZE_BYTES);
    build_append_int_noprefix(tbl, cache->sets, PPTT_CACHE_SETS_BYTES);
    build_append_byte(tbl, cache->associativity); /* Associativity */
    val = PPTT_CACHE_ATTR_ALLOC_POLICY;
    switch (cache->type) {
    case CPU_CACHE_INSTRUCTION:
        val |= (PPTT_CACHE_TYPE_INSTR_VAL << PPTT_CACHE_TYPE_SHIFT);
        break;
    case CPU_CACHE_DATA:
        val |= (PPTT_CACHE_TYPE_DATA_VAL << PPTT_CACHE_TYPE_SHIFT);
        break;
    case CPU_CACHE_UNIFIED:
        val |= (PPTT_CACHE_TYPE_UNIFIED_VAL << PPTT_CACHE_TYPE_SHIFT);
        break;
    }
    build_append_byte(tbl, val); /* Attributes */
    build_append_int_noprefix(tbl, cache->linesize, PPTT_CACHE_LINESIZE_BYTES);
    g_assert(tbl->len == start_len + PPTT_CACHE_NODE_LENGTH);
}

/*
 * Build PPTT Cache Type structures (Type 1) from cache level `level_high`
 * down to `level_low` (both inclusive), appending them to the PPTT table.
 *
 * On output, `data_offset` and `instr_offset` hold the PPTT offsets of the
 * lowest-level data and instruction cache nodes respectively.
 */
static bool build_caches(PpttCacheCtx *ctx,
                         uint8_t level_high, /* Inclusive */
                         uint8_t level_low,  /* Inclusive */
                         uint32_t *data_offset,
                         uint32_t *instr_offset)
{
    GArray *table_data = ctx->table_data;
    uint32_t next_level_offset_data = 0, next_level_offset_instruction = 0;
    uint32_t this_offset, next_offset = 0;
    int c, level;
    bool found_cache = false;

    /* Walk caches from top to bottom */
    for (level = level_high; level >= level_low; level--) {
        for (c = 0; c < ctx->num_caches; c++) {
            if (ctx->caches[c].level != level) {
                continue;
            }

            this_offset = table_data->len - ctx->pptt_start;
            switch (ctx->caches[c].type) {
            case CPU_CACHE_INSTRUCTION:
                next_offset = next_level_offset_instruction;
                break;
            case CPU_CACHE_DATA:
                next_offset = next_level_offset_data;
                break;
            case CPU_CACHE_UNIFIED:
                next_offset = next_level_offset_instruction;
                break;
            }
            build_cache_nodes(table_data, &ctx->caches[c], next_offset);
            switch (ctx->caches[c].type) {
            case CPU_CACHE_INSTRUCTION:
                next_level_offset_instruction = this_offset;
                break;
            case CPU_CACHE_DATA:
                next_level_offset_data = this_offset;
                break;
            case CPU_CACHE_UNIFIED:
                next_level_offset_instruction = this_offset;
                next_level_offset_data = this_offset;
                break;
            }
            *data_offset = next_level_offset_data;
            *instr_offset = next_level_offset_instruction;

            found_cache = true;
        }
    }

    return found_cache;
}

/*
 * Check whether the user has configured any -smp-cache option.
 * Returns true when all cache topology properties remain at their
 * initial DEFAULT value, i.e. no -smp-cache was specified.
 */
static bool virt_smp_cache_all_default(const MachineState *ms)
{
    CacheLevelAndType i;

    for (i = 0; i < CACHE_LEVEL_AND_TYPE__MAX; i++) {
        if (ms->smp_cache.props[i].topology != CPU_TOPOLOGY_LEVEL_DEFAULT) {
            return false;
        }
    }
    return true;
}

/*
 * Set default cache topology when the user has not configured any
 * -smp-cache option, so that PPTT still contains cache information.
 * L1 type (unified vs separate) is determined by hardware CLIDR.
 */
static void virt_set_default_cache_topology(MachineState *ms)
{
    if (!virt_smp_cache_all_default(ms)) {
        return;
    }

    if (cpu_l1_cache_unified(0)) {
        ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L1].topology =
            CPU_TOPOLOGY_LEVEL_CORE;
    } else {
        ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L1D].topology =
            CPU_TOPOLOGY_LEVEL_CORE;
        ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L1I].topology =
            CPU_TOPOLOGY_LEVEL_CORE;
    }
    ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L2].topology =
        CPU_TOPOLOGY_LEVEL_CORE;
    ms->smp_cache.props[CACHE_LEVEL_AND_TYPE_L3].topology =
        CPU_TOPOLOGY_LEVEL_SOCKET;
}

/*
 * Find and build cache nodes at a given CPU topology level, filling the
 * private resource fields for the corresponding processor hierarchy node.
 *
 * On entry, @top_level is the highest cache level to search from.
 * On return, result->bottom_level is the lowest cache level found and
 * result->found indicates whether any cache exists at @topo.
 */
static void build_topo_caches(PpttCacheCtx *ctx, MachineState *ms,
                               CpuTopologyLevel topo, int top_level,
                               PpttLevelResult *result)
{
    uint32_t data_off = 0, instr_off = 0;

    result->priv_rsrc[0] = 0;
    result->priv_rsrc[1] = 0;
    result->num_priv = 0;
    result->bottom_level = top_level;
    result->found = false;

    result->found = machine_find_lowest_level_cache_at_topo_level(
        ms, &result->bottom_level, topo);
    if (!result->found) {
        return;
    }

    build_caches(ctx, top_level, result->bottom_level,
                 &data_off, &instr_off);

    result->priv_rsrc[0] = instr_off;
    result->priv_rsrc[1] = data_off;
    if (instr_off || data_off) {
        result->num_priv = (instr_off == data_off) ?
            PPTT_PRIV_RSRC_UNIFIED : PPTT_PRIV_RSRC_SEPARATE;
    }
}

/*
 * ACPI spec, Revision 6.3
 * 5.2.29 Processor Properties Topology Table (PPTT)
 */
static void build_pptt_arm(GArray *table_data, BIOSLinker *linker, MachineState *ms,
                const char *oem_id, const char *oem_table_id,
                int num_caches, CPUCoreCaches *caches)
{
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    GQueue *list = g_queue_new();
    guint pptt_start = table_data->len;
    PpttCacheCtx ctx = { table_data, pptt_start, caches, num_caches };
    guint parent_offset;
    guint length, i;
    int uid = 0;
    int socket;
    AcpiTable table = { .sig = "PPTT", .rev = 2,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    /* Cache topology level tracking */
    int top_node = PPTT_MAX_CACHE_LEVEL;
    int top_cluster = PPTT_MAX_CACHE_LEVEL;
    int top_core = PPTT_MAX_CACHE_LEVEL;

    acpi_table_begin(&table, table_data);

    /* === Socket level === */
    for (socket = 0; socket < ms->smp.sockets; socket++) {
        PpttLevelResult res = {};

        build_topo_caches(&ctx, ms, CPU_TOPOLOGY_LEVEL_SOCKET,
                          top_node, &res);
        if (res.found) {
            top_cluster = res.bottom_level - 1;
        }

        g_queue_push_tail(list,
            GUINT_TO_POINTER(table_data->len - pptt_start));
        build_processor_hierarchy_node(
            table_data,
            (1 << 0), /* Physical package */
            0, socket, res.priv_rsrc, res.num_priv);
    }

    /* === Cluster level === */
    if (mc->smp_props.clusters_supported) {
        length = g_queue_get_length(list);
        for (i = 0; i < length; i++) {
            int cluster;

            parent_offset = GPOINTER_TO_UINT(g_queue_pop_head(list));
            for (cluster = 0; cluster < ms->smp.clusters; cluster++) {
                PpttLevelResult res = {};

                build_topo_caches(&ctx, ms, CPU_TOPOLOGY_LEVEL_CLUSTER,
                                  top_cluster, &res);
                top_core = res.found ? res.bottom_level - 1 : top_cluster;

                g_queue_push_tail(list,
                    GUINT_TO_POINTER(table_data->len - pptt_start));
                build_processor_hierarchy_node(
                    table_data,
                    (0 << 0), /* not a physical package */
                    parent_offset, cluster, res.priv_rsrc, res.num_priv);
            }
        }
    } else {
        top_core = top_cluster;
    }

    /* === Core level === */
    length = g_queue_get_length(list);
    for (i = 0; i < length; i++) {
        int core;

        parent_offset = GPOINTER_TO_UINT(g_queue_pop_head(list));

        for (core = 0; core < ms->smp.cores; core++) {
            PpttLevelResult res = {};

            build_topo_caches(&ctx, ms, CPU_TOPOLOGY_LEVEL_CORE,
                              top_core, &res);

            if (ms->smp.threads > 1) {
                g_queue_push_tail(list,
                    GUINT_TO_POINTER(table_data->len - pptt_start));
                build_processor_hierarchy_node(
                    table_data,
                    (0 << 0), /* not a physical package */
                    parent_offset, core, res.priv_rsrc, res.num_priv);
            } else {
                build_processor_hierarchy_node(
                    table_data,
                    (1 << 1) | /* ACPI Processor ID valid */
                    (1 << 3),  /* Node is a Leaf */
                    parent_offset, uid++, res.priv_rsrc, res.num_priv);
            }
        }
    }

    /* === Thread level === */
    length = g_queue_get_length(list);
    for (i = 0; i < length; i++) {
        int thread;

        parent_offset = GPOINTER_TO_UINT(g_queue_pop_head(list));
        for (thread = 0; thread < ms->smp.threads; thread++) {
            build_processor_hierarchy_node(
                table_data,
                (1 << 1) | /* ACPI Processor ID valid */
                (1 << 2) | /* Processor is a Thread */
                (1 << 3),  /* Node is a Leaf */
                parent_offset, uid++, NULL, 0);
        }
    }

    g_queue_free(list);
    acpi_table_end(linker, &table);
}

static void acpi_dsdt_add_psd(Aml *dev, int cpus)
{
    Aml *pkg;
    Aml *sub;

    sub = aml_package(5);
    aml_append(sub, aml_int(5));
    aml_append(sub, aml_int(0));
    /* Assume all vCPUs belong to the same domain */
    aml_append(sub, aml_int(0));
    /* SW_ANY: OSPM coordinate, initiate on any processor */
    aml_append(sub, aml_int(0xFD));
    aml_append(sub, aml_int(cpus));

    pkg = aml_package(1);
    aml_append(pkg, sub);

    aml_append(dev, aml_name_decl("_PSD", pkg));
}

static void acpi_dsdt_add_cppc(Aml *dev, uint64_t cpu_base, int *regs_offset)
{
    Aml *cpc;
    int i;

    /* Use version 3 of CPPC table from ACPI 6.3 */
    cpc = aml_package(23);
    aml_append(cpc, aml_int(23));
    aml_append(cpc, aml_int(3));

    for (i = 0; i < CPPC_REG_COUNT; i++) {
        Aml *res;
        uint8_t reg_width;
        uint8_t acc_type;
        uint64_t addr;

        if (regs_offset[i] == -1) {
            reg_width = 0;
            acc_type = AML_ANY_ACC;
            addr = 0;
        } else {
            addr = cpu_base + regs_offset[i];
            if (i == REFERENCE_CTR || i == DELIVERED_CTR) {
                reg_width = 64;
                acc_type = AML_QWORD_ACC;
            } else {
                reg_width = 32;
                acc_type = AML_DWORD_ACC;
            }
        }

        res = aml_resource_template();
        aml_append(res, aml_generic_register(AML_SYSTEM_MEMORY, reg_width, 0,
                                             acc_type, addr));
        aml_append(cpc, res);
    }

    aml_append(dev, aml_name_decl("_CPC", cpc));
}

static void virt_acpi_dsdt_cpu_cppc(int ncpu, int num_cpu, Aml *dev) {
    VirtMachineState *vms = VIRT_MACHINE(qdev_get_machine());
    const MemMapEntry *cppc_memmap = &vms->memmap[VIRT_CPUFREQ];

    /*
     * Append _CPC and _PSD to support CPU frequence show
     * Check CPPC available by DESIRED_PERF register
     */
    if (cppc_regs_offset[DESIRED_PERF] != -1) {
        acpi_dsdt_add_cppc(dev,
                           cppc_memmap->base + ncpu * CPPC_REG_PER_CPU_STRIDE,
                           cppc_regs_offset);
        acpi_dsdt_add_psd(dev, num_cpu);
    }
}

static void acpi_dsdt_add_cpus(Aml *scope, VirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    uint16_t i;

    for (i = 0; i < ms->smp.cpus; i++) {
        Aml *dev = aml_device("C%.03X", i);
        aml_append(dev, aml_name_decl("_HID", aml_string("ACPI0007")));
        aml_append(dev, aml_name_decl("_UID", aml_int(i)));

        virt_acpi_dsdt_cpu_cppc(i, ms->smp.cpus, dev);

        aml_append(scope, dev);
    }
}

static void acpi_dsdt_add_uart(Aml *scope, const MemMapEntry *uart_memmap,
                                           uint32_t uart_irq)
{
    Aml *dev = aml_device("COM0");
    aml_append(dev, aml_name_decl("_HID", aml_string("ARMH0011")));
    aml_append(dev, aml_name_decl("_UID", aml_int(0)));

    Aml *crs = aml_resource_template();
    aml_append(crs, aml_memory32_fixed(uart_memmap->base,
                                       uart_memmap->size, AML_READ_WRITE));
    aml_append(crs,
               aml_interrupt(AML_CONSUMER, AML_LEVEL, AML_ACTIVE_HIGH,
                             AML_EXCLUSIVE, &uart_irq, 1));
    aml_append(dev, aml_name_decl("_CRS", crs));

    aml_append(scope, dev);
}

static void acpi_dsdt_add_flash(Aml *scope, const MemMapEntry *flash_memmap)
{
    Aml *dev, *crs;
    hwaddr base = flash_memmap->base;
    hwaddr size = flash_memmap->size / 2;

    dev = aml_device("FLS0");
    aml_append(dev, aml_name_decl("_HID", aml_string("LNRO0015")));
    aml_append(dev, aml_name_decl("_UID", aml_int(0)));

    crs = aml_resource_template();
    aml_append(crs, aml_memory32_fixed(base, size, AML_READ_WRITE));
    aml_append(dev, aml_name_decl("_CRS", crs));
    aml_append(scope, dev);

    dev = aml_device("FLS1");
    aml_append(dev, aml_name_decl("_HID", aml_string("LNRO0015")));
    aml_append(dev, aml_name_decl("_UID", aml_int(1)));
    crs = aml_resource_template();
    aml_append(crs, aml_memory32_fixed(base + size, size, AML_READ_WRITE));
    aml_append(dev, aml_name_decl("_CRS", crs));
    aml_append(scope, dev);
}

static void acpi_dsdt_add_hisi_sec(Aml *scope,
                                   const MemMapEntry *virtio_mmio_memmap,
                                   int dev_id)
{
    hwaddr size = 0x10000;

    /*
     * Calculate the base address for the sec device node.
     * Each device group contains one sec device and one hpre device,spaced by 2 * size.
     */
    hwaddr base = virtio_mmio_memmap->base + dev_id * 2 * size;

    Aml *dev = aml_device("SE%02u", dev_id);
    aml_append(dev, aml_name_decl("_HID", aml_string("SEC07")));
    aml_append(dev, aml_name_decl("_UID", aml_int(dev_id)));
    aml_append(dev, aml_name_decl("_CCA", aml_int(1)));

    Aml *crs = aml_resource_template();

    aml_append(crs, aml_memory32_fixed(base, size, AML_READ_WRITE));
    aml_append(dev, aml_name_decl("_CRS", crs));
    aml_append(scope, dev);
}

static void acpi_dsdt_add_hisi_hpre(Aml *scope,
                                    const MemMapEntry *virtio_mmio_memmap,
                                    int dev_id)
{
    hwaddr size = 0x10000;

    /*
     * Calculate the base address for the hpre device node.
     * Each hpre device follows the corresponding sec device by an additional offset of size.
     */
    hwaddr base = virtio_mmio_memmap->base + dev_id * 2 * size + size;

    Aml *dev = aml_device("HP%02u", dev_id);
    aml_append(dev, aml_name_decl("_HID", aml_string("HPRE07")));
    aml_append(dev, aml_name_decl("_UID", aml_int(dev_id)));
    aml_append(dev, aml_name_decl("_CCA", aml_int(1)));

    Aml *crs = aml_resource_template();

    aml_append(crs, aml_memory32_fixed(base, size, AML_READ_WRITE));
    aml_append(dev, aml_name_decl("_CRS", crs));
    aml_append(scope, dev);
}

static void acpi_dsdt_add_pci(Aml *scope, const MemMapEntry *memmap,
                              uint32_t irq, VirtMachineState *vms)
{
    int ecam_id = VIRT_ECAM_ID(vms->highmem_ecam);
    struct GPEXConfig cfg = {
        .mmio32 = memmap[VIRT_PCIE_MMIO],
        .pio    = memmap[VIRT_PCIE_PIO],
        .ecam   = memmap[ecam_id],
        .irq    = irq,
        .bus    = vms->bus,
    };

    /*
     * Accel SMMU requires RMRs for MSI 1-1 mapping, which
     * require _DSM for PreservingPCI Boot Configurations
     */
    if (vms->iommu == VIRT_IOMMU_SMMUV3_ACCEL) {
        cfg.preserve_config = true;
    }

    if (vms->highmem_mmio) {
        cfg.mmio64 = memmap[VIRT_HIGH_PCIE_MMIO];
    }

    acpi_dsdt_add_gpex(scope, &cfg);
}

static void acpi_dsdt_add_gpio(Aml *scope, const MemMapEntry *gpio_memmap,
                                           uint32_t gpio_irq)
{
    Aml *dev = aml_device("GPO0");
    aml_append(dev, aml_name_decl("_HID", aml_string("ARMH0061")));
    aml_append(dev, aml_name_decl("_UID", aml_int(0)));

    Aml *crs = aml_resource_template();
    aml_append(crs, aml_memory32_fixed(gpio_memmap->base, gpio_memmap->size,
                                       AML_READ_WRITE));
    aml_append(crs, aml_interrupt(AML_CONSUMER, AML_LEVEL, AML_ACTIVE_HIGH,
                                  AML_EXCLUSIVE, &gpio_irq, 1));
    aml_append(dev, aml_name_decl("_CRS", crs));

    Aml *aei = aml_resource_template();
    /* Pin 3 for power button */
    const uint32_t pin_list[1] = {3};
    aml_append(aei, aml_gpio_int(AML_CONSUMER, AML_EDGE, AML_ACTIVE_HIGH,
                                 AML_EXCLUSIVE, AML_PULL_UP, 0, pin_list, 1,
                                 "GPO0", NULL, 0));
    aml_append(dev, aml_name_decl("_AEI", aei));

    /* _E03 is handle for power button */
    Aml *method = aml_method("_E03", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_notify(aml_name(ACPI_POWER_BUTTON_DEVICE),
                                  aml_int(0x80)));
    aml_append(dev, method);
    aml_append(scope, dev);
}

#ifdef CONFIG_UBMEM_VMMU
static void build_ubmem_vmmu_aml(VirtMachineState *vms, Aml *table)
{
    Aml *crs = aml_resource_template();
    Aml *dev_node = aml_device("UBM");
    const MemMapEntry *reg_memmap = &vms->memmap[VIRT_UBMEM_VMMU_REG];
    const MemMapEntry *mem_memmap = &vms->memmap[VIRT_UBMEM_VMMU_MEM];

    aml_append(dev_node, aml_name_decl("_HID", aml_string("HISI0591")));

    aml_append(crs, aml_qword_memory(
        AML_POS_DECODE,
        AML_MIN_FIXED,
        AML_MAX_FIXED,
        AML_NON_CACHEABLE,
        AML_READ_WRITE,
        0,
        reg_memmap->base,
        reg_memmap->base + reg_memmap->size - 1,
        0,
        reg_memmap->size));

    aml_append(crs, aml_qword_memory(
        AML_POS_DECODE,
        AML_MIN_FIXED,
        AML_MAX_FIXED,
        AML_CACHEABLE,
        AML_READ_WRITE,
        0,
        mem_memmap->base,
        mem_memmap->base + mem_memmap->size - 1,
        0,
        mem_memmap->size));

    aml_append(dev_node, aml_name_decl("_CRS", crs));
    aml_append(table, dev_node);
}
#endif

#ifdef CONFIG_TPM
static void acpi_dsdt_add_tpm(Aml *scope, VirtMachineState *vms)
{
    PlatformBusDevice *pbus = PLATFORM_BUS_DEVICE(vms->platform_bus_dev);
    hwaddr pbus_base = vms->memmap[VIRT_PLATFORM_BUS].base;
    SysBusDevice *sbdev = SYS_BUS_DEVICE(tpm_find());
    MemoryRegion *sbdev_mr;
    hwaddr tpm_base;

    if (!sbdev) {
        return;
    }

    tpm_base = platform_bus_get_mmio_addr(pbus, sbdev, 0);
    assert(tpm_base != -1);

    tpm_base += pbus_base;

    sbdev_mr = sysbus_mmio_get_region(sbdev, 0);

    Aml *dev = aml_device("TPM0");
    aml_append(dev, aml_name_decl("_HID", aml_string("MSFT0101")));
    aml_append(dev, aml_name_decl("_STR", aml_string("TPM 2.0 Device")));
    aml_append(dev, aml_name_decl("_UID", aml_int(0)));

    Aml *crs = aml_resource_template();
    aml_append(crs,
               aml_memory32_fixed(tpm_base,
                                  (uint32_t)memory_region_size(sbdev_mr),
                                  AML_READ_WRITE));
    aml_append(dev, aml_name_decl("_CRS", crs));
    aml_append(scope, dev);
}
#endif

#ifdef CONFIG_PAS_EXPANSION
static void acpi_dsdt_add_hisi_mmcd(Aml *scope, VirtMachineState *vms)
{
    Object *obj = object_resolve_path_type("", TYPE_VFIO_HISI_MMCD, NULL);
    if (!obj) {
        return;
    }
    SysBusDevice *sbdev = SYS_BUS_DEVICE(obj);
    PlatformBusDevice *pbus = PLATFORM_BUS_DEVICE(vms->platform_bus_dev);
    hwaddr pbus_base = vms->memmap[VIRT_PLATFORM_BUS].base;
    VFIOPlatformDevice *vdev = VFIO_PLATFORM_DEVICE(obj);
    VFIODevice *vbasedev = &vdev->vbasedev;
    g_autofree char *tmpname = g_strdup(vbasedev->name);
    char *colon = strchr(tmpname, ':');
    if (colon) {
        *colon = '\0';
    }
    Aml *dev = aml_device("MMCD");
    aml_append(dev, aml_name_decl("_HID", aml_string("%s", tmpname)));
    aml_append(dev, aml_name_decl("_UID", aml_int(0)));
    Aml *crs = aml_resource_template();
    for (int i = 0; i < vbasedev->num_regions; i++) {
        MemoryRegion *region = sysbus_mmio_get_region(sbdev, i);
        if (!region) {
            continue;
        }
        hwaddr region_base = platform_bus_get_mmio_addr(pbus, sbdev, i);
        hwaddr region_size = memory_region_size(region);
        region_base += pbus_base;
        aml_append(crs,
                   aml_qword_memory(AML_POS_DECODE, AML_MIN_FIXED,
                                    AML_MAX_FIXED, AML_NON_CACHEABLE,
                                    AML_READ_WRITE, 0, region_base,
                                    region_base + region_size - 1, 0, region_size));
    }
    aml_append(dev, aml_name_decl("_CRS", crs));
    Aml *rst_method = aml_method("_RST", 0, AML_NOTSERIALIZED);
    aml_append(dev, rst_method);
    Aml *sta_method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(sta_method, aml_return(aml_int(0xF)));
    aml_append(dev, sta_method);
    aml_append(scope, dev);
}
#endif

#define ID_MAPPING_ENTRY_SIZE 20
#define SMMU_V3_ENTRY_SIZE 68
#define ROOT_COMPLEX_ENTRY_SIZE 36
#define IORT_NODE_OFFSET 48

static void build_iort_id_mapping(GArray *table_data, uint32_t input_base,
                                  uint32_t id_count, uint32_t out_ref, uint32_t flags)
{
    /* Table 4 ID mapping format */
    build_append_int_noprefix(table_data, input_base, 4); /* Input base */
    build_append_int_noprefix(table_data, id_count, 4); /* Number of IDs */
    build_append_int_noprefix(table_data, input_base, 4); /* Output base */
    build_append_int_noprefix(table_data, out_ref, 4); /* Output Reference */
    /* Flags */
    build_append_int_noprefix(table_data, flags, 4); /* Flags */
}

struct AcpiIortIdMapping {
    uint32_t input_base;
    uint32_t id_count;
};
typedef struct AcpiIortIdMapping AcpiIortIdMapping;

/* Build the iort ID mapping to SMMUv3 for a given PCI host bridge */
static int
iort_host_bridges(Object *obj, void *opaque)
{
    GArray *idmap_blob = opaque;

    if (object_dynamic_cast(obj, TYPE_PCI_HOST_BRIDGE)) {
        PCIBus *bus = PCI_HOST_BRIDGE(obj)->bus;

        if (bus && !pci_bus_bypass_iommu(bus)) {
            int min_bus, max_bus;

            pci_bus_range(bus, &min_bus, &max_bus);

            AcpiIortIdMapping idmap = {
                .input_base = min_bus << 8,
                .id_count = (max_bus - min_bus + 1) << 8,
            };
            g_array_append_val(idmap_blob, idmap);
        }
    }

    return 0;
}

static int iort_idmap_compare(gconstpointer a, gconstpointer b)
{
    AcpiIortIdMapping *idmap_a = (AcpiIortIdMapping *)a;
    AcpiIortIdMapping *idmap_b = (AcpiIortIdMapping *)b;

    return idmap_a->input_base - idmap_b->input_base;
}

static void
build_iort_rmr_nodes(GArray *table_data, GArray *smmu_idmaps,
                     size_t *smmu_offset, uint32_t *id)
{
    AcpiIortIdMapping *range;
    int i;

    for (i = 0; i < smmu_idmaps->len; i++) {
        range = &g_array_index(smmu_idmaps, AcpiIortIdMapping, i);
        int bdf = range->input_base;

        /* Table 18 Reserved Memory Range Node */

        build_append_int_noprefix(table_data, 6 /* RMR */, 1); /* Type */
        /* Length */
        build_append_int_noprefix(table_data, 28 + ID_MAPPING_ENTRY_SIZE + 20, 2);
        build_append_int_noprefix(table_data, 3, 1); /* Revision */
        build_append_int_noprefix(table_data, *id, 4); /* Identifier */
        /* Number of ID mappings */
        build_append_int_noprefix(table_data, 1, 4);
        /* Reference to ID Array */
        build_append_int_noprefix(table_data, 28, 4);

        /* RMR specific data */

        /* Flags */
        build_append_int_noprefix(table_data, 0 /* Disallow remapping */, 4);
        /* Number of Memory Range Descriptors */
        build_append_int_noprefix(table_data, 1 , 4);
        /* Reference to Memory Range Descriptors */
        build_append_int_noprefix(table_data, 28 + ID_MAPPING_ENTRY_SIZE, 4);
        build_iort_id_mapping(table_data, bdf, range->id_count, smmu_offset[i], 1);

        /* Table 19 Memory Range Descriptor */

        /* Physical Range offset */
        build_append_int_noprefix(table_data, 0x8000000, 8);
        /* Physical Range length */
        build_append_int_noprefix(table_data, 0x100000, 8);
        build_append_int_noprefix(table_data, 0, 4); /* Reserved */
        *id += 1;
    }
}

/*
 * Input Output Remapping Table (IORT)
 * Conforms to "IO Remapping Table System Software on ARM Platforms",
 * Document number: ARM DEN 0049E.b, Feb 2021
 */
static void
build_iort(GArray *table_data, BIOSLinker *linker, VirtMachineState *vms)
{
    int i, nb_nodes, rc_mapping_count;
    size_t node_size, *smmu_offset;
    AcpiIortIdMapping *idmap;
    hwaddr base;
    int irq, num_smmus = 0;
    uint32_t id = 0;
    GArray *smmu_idmaps = g_array_new(false, true, sizeof(AcpiIortIdMapping));
    GArray *its_idmaps = g_array_new(false, true, sizeof(AcpiIortIdMapping));

    AcpiTable table = { .sig = "IORT", .rev = 5, .oem_id = vms->oem_id,
                        .oem_table_id = vms->oem_table_id };
    /* Table 2 The IORT */
    acpi_table_begin(&table, table_data);

    if (vms->smmu_accel_count) {
        irq = vms->irqmap[VIRT_SMMU_ACCEL] + ARM_SPI_BASE;
        base = vms->memmap[VIRT_SMMU_ACCEL].base;
        num_smmus = vms->smmu_accel_count;
    } else if (virt_has_smmuv3(vms)) {
        irq = vms->irqmap[VIRT_SMMU] + ARM_SPI_BASE;
        base = vms->memmap[VIRT_SMMU].base;
        num_smmus = 1;
    }

    smmu_offset = g_new0(size_t, num_smmus);
    nb_nodes = 2; /* RC, ITS */
    nb_nodes += num_smmus; /* SMMU nodes */

    if (virt_has_smmuv3(vms)) {
        AcpiIortIdMapping next_range = {0};

        object_child_foreach_recursive(object_get_root(),
                                       iort_host_bridges, smmu_idmaps);

        /* Sort the smmu idmap by input_base */
        g_array_sort(smmu_idmaps, iort_idmap_compare);

        /*
         * Split the whole RIDs by mapping from RC to SMMU,
         * build the ID mapping from RC to ITS directly.
         */
        for (i = 0; i < smmu_idmaps->len; i++) {
            idmap = &g_array_index(smmu_idmaps, AcpiIortIdMapping, i);

            if (next_range.input_base < idmap->input_base) {
                next_range.id_count = idmap->input_base - next_range.input_base;
                g_array_append_val(its_idmaps, next_range);
            }

            next_range.input_base = idmap->input_base + idmap->id_count;
            if (vms->iommu == VIRT_IOMMU_SMMUV3_ACCEL) {
                nb_nodes++; /* RMR node per SMMU */
            }
        }

        /* Append the last RC -> ITS ID mapping */
        if (next_range.input_base < 0x10000) {
            next_range.id_count = 0x10000 - next_range.input_base;
            g_array_append_val(its_idmaps, next_range);
        }

        rc_mapping_count = smmu_idmaps->len + its_idmaps->len;
    } else {
        rc_mapping_count = 1;
    }

#ifdef CONFIG_UB
    nb_nodes += 3; /* UBC0, UMU0, PMU0 */
#endif

    /* Number of IORT Nodes */
    build_append_int_noprefix(table_data, nb_nodes, 4);

    /* Offset to Array of IORT Nodes */
    build_append_int_noprefix(table_data, IORT_NODE_OFFSET, 4);
    build_append_int_noprefix(table_data, 0, 4); /* Reserved */

    /* Table 12 ITS Group Format */
    build_append_int_noprefix(table_data, 0 /* ITS Group */, 1); /* Type */
    node_size =  20 /* fixed header size */ + 4 /* 1 GIC ITS Identifier */;
    build_append_int_noprefix(table_data, node_size, 2); /* Length */
    build_append_int_noprefix(table_data, 1, 1); /* Revision */
    build_append_int_noprefix(table_data, id++, 4); /* Identifier */
    build_append_int_noprefix(table_data, 0, 4); /* Number of ID mappings */
    build_append_int_noprefix(table_data, 0, 4); /* Reference to ID Array */
    build_append_int_noprefix(table_data, 1, 4); /* Number of ITSs */
    /* GIC ITS Identifier Array */
    build_append_int_noprefix(table_data, 0 /* MADT translation_id */, 4);

    for (i = 0; i < num_smmus; i++) {
        smmu_offset[i] = table_data->len - table.table_offset;

        /* Table 9 SMMUv3 Format */
        build_append_int_noprefix(table_data, 4 /* SMMUv3 */, 1); /* Type */
        node_size =  SMMU_V3_ENTRY_SIZE + ID_MAPPING_ENTRY_SIZE;
        build_append_int_noprefix(table_data, node_size, 2); /* Length */
        build_append_int_noprefix(table_data, 4, 1); /* Revision */
        build_append_int_noprefix(table_data, id++, 4); /* Identifier */
        build_append_int_noprefix(table_data, 1, 4); /* Number of ID mappings */
        /* Reference to ID Array */
        build_append_int_noprefix(table_data, SMMU_V3_ENTRY_SIZE, 4);
        /* Base address */
        build_append_int_noprefix(table_data, base + (i * SMMU_IO_LEN), 8);
        /* Flags */
        build_append_int_noprefix(table_data, 1 /* COHACC Override */, 4);
        build_append_int_noprefix(table_data, 0, 4); /* Reserved */
        build_append_int_noprefix(table_data, 0, 8); /* VATOS address */
        /* Model */
        build_append_int_noprefix(table_data, 0 /* Generic SMMU-v3 */, 4);
        build_append_int_noprefix(table_data, irq, 4); /* Event */
        build_append_int_noprefix(table_data, irq + 1, 4); /* PRI */
        build_append_int_noprefix(table_data, irq + 3, 4); /* GERR */
        build_append_int_noprefix(table_data, irq + 2, 4); /* Sync */
        irq += NUM_SMMU_IRQS;
        build_append_int_noprefix(table_data, 0, 4); /* Proximity domain */
        /* DeviceID mapping index (ignored since interrupts are GSIV based) */
        build_append_int_noprefix(table_data, 0, 4);

        /* output IORT node is the ITS group node (the first node) */
        build_iort_id_mapping(table_data, 0, 0x10000, IORT_NODE_OFFSET, 0);
    }

    /* Table 17 Root Complex Node */
    build_append_int_noprefix(table_data, 2 /* Root complex */, 1); /* Type */
    node_size =  ROOT_COMPLEX_ENTRY_SIZE +
                 ID_MAPPING_ENTRY_SIZE * rc_mapping_count;
    build_append_int_noprefix(table_data, node_size, 2); /* Length */
    build_append_int_noprefix(table_data, 3, 1); /* Revision */
    build_append_int_noprefix(table_data, id++, 4); /* Identifier */
    /* Number of ID mappings */
    build_append_int_noprefix(table_data, rc_mapping_count, 4);
    /* Reference to ID Array */
    build_append_int_noprefix(table_data, ROOT_COMPLEX_ENTRY_SIZE, 4);

    /* Table 14 Memory access properties */
    /* CCA: Cache Coherent Attribute */
    build_append_int_noprefix(table_data, 1 /* fully coherent */, 4);
    build_append_int_noprefix(table_data, 0, 1); /* AH: Note Allocation Hints */
    build_append_int_noprefix(table_data, 0, 2); /* Reserved */
    /* Table 15 Memory Access Flags */
    build_append_int_noprefix(table_data, 0x3 /* CCA = CPM = DACS = 1 */, 1);

    build_append_int_noprefix(table_data, 0, 4); /* ATS Attribute */
    /* MCFG pci_segment */
    build_append_int_noprefix(table_data, 0, 4); /* PCI Segment number */

    /* Memory address size limit */
    build_append_int_noprefix(table_data, 64, 1);

    build_append_int_noprefix(table_data, 0, 3); /* Reserved */

    /* Output Reference */
    if (virt_has_smmuv3(vms)) {
        AcpiIortIdMapping *range;

        /* translated RIDs connect to SMMUv3 node: RC -> SMMUv3 -> ITS */
        for (i = 0; i < smmu_idmaps->len; i++) {
            range = &g_array_index(smmu_idmaps, AcpiIortIdMapping, i);
            /* output IORT node is the smmuv3 node */
            build_iort_id_mapping(table_data, range->input_base,
                                  range->id_count, smmu_offset[i], 0);
        }

        /* bypassed RIDs connect to ITS group node directly: RC -> ITS */
        for (i = 0; i < its_idmaps->len; i++) {
            range = &g_array_index(its_idmaps, AcpiIortIdMapping, i);
            /* output IORT node is the ITS group node (the first node) */
            build_iort_id_mapping(table_data, range->input_base,
                                  range->id_count, IORT_NODE_OFFSET, 0);
        }
    } else {
        /* output IORT node is the ITS group node (the first node) */
        build_iort_id_mapping(table_data, 0, 0x10000, IORT_NODE_OFFSET, 0);
    }

    if (vms->iommu == VIRT_IOMMU_SMMUV3_ACCEL) {
        build_iort_rmr_nodes(table_data, smmu_idmaps, smmu_offset, &id);
    }

#ifdef CONFIG_UB
    acpi_iort_add_ub(table_data);
#endif

    acpi_table_end(linker, &table);
    g_array_free(smmu_idmaps, true);
    g_array_free(its_idmaps, true);
}

/*
 * Serial Port Console Redirection Table (SPCR)
 * Rev: 1.07
 */
static void
spcr_setup(GArray *table_data, BIOSLinker *linker, VirtMachineState *vms)
{
    AcpiSpcrData serial = {
        .interface_type = 3,       /* ARM PL011 UART */
        .base_addr.id = AML_AS_SYSTEM_MEMORY,
        .base_addr.width = 32,
        .base_addr.offset = 0,
        .base_addr.size = 3,
        .base_addr.addr = vms->memmap[VIRT_UART].base,
        .interrupt_type = (1 << 3),/* Bit[3] ARMH GIC interrupt*/
        .pc_interrupt = 0,         /* IRQ */
        .interrupt = (vms->irqmap[VIRT_UART] + ARM_SPI_BASE),
        .baud_rate = 3,            /* 9600 */
        .parity = 0,               /* No Parity */
        .stop_bits = 1,            /* 1 Stop bit */
        .flow_control = 1 << 1,    /* RTS/CTS hardware flow control */
        .terminal_type = 0,        /* VT100 */
        .language = 0,             /* Language */
        .pci_device_id = 0xffff,   /* not a PCI device*/
        .pci_vendor_id = 0xffff,   /* not a PCI device*/
        .pci_bus = 0,
        .pci_device = 0,
        .pci_function = 0,
        .pci_flags = 0,
        .pci_segment = 0,
    };
    /*
     * Passing NULL as the SPCR Table for Revision 2 doesn't support
     * NameSpaceString.
     */
    build_spcr(table_data, linker, &serial, 2, vms->oem_id, vms->oem_table_id,
               NULL);
}

/*
 * ACPI spec, Revision 5.1
 * 5.2.16 System Resource Affinity Table (SRAT)
 */
static void
build_srat(GArray *table_data, BIOSLinker *linker, VirtMachineState *vms)
{
    int i;
    uint64_t mem_base;
    MachineClass *mc = MACHINE_GET_CLASS(vms);
    MachineState *ms = MACHINE(vms);
    const CPUArchIdList *cpu_list = mc->possible_cpu_arch_ids(ms);
    AcpiTable table = { .sig = "SRAT", .rev = 3, .oem_id = vms->oem_id,
                        .oem_table_id = vms->oem_table_id };

    acpi_table_begin(&table, table_data);
    build_append_int_noprefix(table_data, 1, 4); /* Reserved */
    build_append_int_noprefix(table_data, 0, 8); /* Reserved */

    for (i = 0; i < cpu_list->len; ++i) {
        uint32_t nodeid = cpu_list->cpus[i].props.node_id;
        /*
         * 5.2.16.4 GICC Affinity Structure
         */
        build_append_int_noprefix(table_data, 3, 1);      /* Type */
        build_append_int_noprefix(table_data, 18, 1);     /* Length */
        build_append_int_noprefix(table_data, nodeid, 4); /* Proximity Domain */
        build_append_int_noprefix(table_data, i, 4); /* ACPI Processor UID */
        /* Flags, Table 5-76 */
        build_append_int_noprefix(table_data, 1 /* Enabled */, 4);
        build_append_int_noprefix(table_data, 0, 4); /* Clock Domain */
    }

    mem_base = vms->memmap[VIRT_MEM].base;
    for (i = 0; i < ms->numa_state->num_nodes; ++i) {
        if (ms->numa_state->nodes[i].node_mem > 0) {
            build_srat_memory(table_data, mem_base,
                              ms->numa_state->nodes[i].node_mem, i,
                              MEM_AFFINITY_ENABLED);
            mem_base += ms->numa_state->nodes[i].node_mem;
        }
    }

    if (ms->nvdimms_state->is_enabled) {
        nvdimm_build_srat(table_data);
    }

    if (ms->device_memory) {
        build_srat_memory(table_data, ms->device_memory->base,
                          memory_region_size(&ms->device_memory->mr),
                          ms->numa_state->num_nodes - 1,
                          MEM_AFFINITY_HOTPLUGGABLE | MEM_AFFINITY_ENABLED);
    }

    acpi_table_end(linker, &table);
}

/*
 * ACPI spec, Revision 5.1
 * 5.2.24 Generic Timer Description Table (GTDT)
 */
static void
build_gtdt(GArray *table_data, BIOSLinker *linker, VirtMachineState *vms)
{
    VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(vms);
    /*
     * Table 5-117 Flag Definitions
     * set only "Timer interrupt Mode" and assume "Timer Interrupt
     * polarity" bit as '0: Interrupt is Active high'
     */
    uint32_t irqflags = vmc->claim_edge_triggered_timers ?
        1 : /* Interrupt is Edge triggered */
        0;  /* Interrupt is Level triggered  */
    AcpiTable table = { .sig = "GTDT", .rev = 2, .oem_id = vms->oem_id,
                        .oem_table_id = vms->oem_table_id };

    acpi_table_begin(&table, table_data);

    /* CntControlBase Physical Address */
    build_append_int_noprefix(table_data, 0xFFFFFFFFFFFFFFFF, 8);
    build_append_int_noprefix(table_data, 0, 4); /* Reserved */
    /*
     * FIXME: clarify comment:
     * The interrupt values are the same with the device tree when adding 16
     */
    /* Secure EL1 timer GSIV */
    build_append_int_noprefix(table_data, ARCH_TIMER_S_EL1_IRQ, 4);
    /* Secure EL1 timer Flags */
    build_append_int_noprefix(table_data, irqflags, 4);
    /* Non-Secure EL1 timer GSIV */
    build_append_int_noprefix(table_data, ARCH_TIMER_NS_EL1_IRQ, 4);
    /* Non-Secure EL1 timer Flags */
    build_append_int_noprefix(table_data, irqflags |
                              1UL << 2, /* Always-on Capability */
                              4);
    /* Virtual timer GSIV */
    build_append_int_noprefix(table_data, ARCH_TIMER_VIRT_IRQ, 4);
    /* Virtual Timer Flags */
    build_append_int_noprefix(table_data, irqflags, 4);
    /* Non-Secure EL2 timer GSIV */
    build_append_int_noprefix(table_data, ARCH_TIMER_NS_EL2_IRQ, 4);
    /* Non-Secure EL2 timer Flags */
    build_append_int_noprefix(table_data, irqflags, 4);
    /* CntReadBase Physical address */
    build_append_int_noprefix(table_data, 0xFFFFFFFFFFFFFFFF, 8);
    /* Platform Timer Count */
    build_append_int_noprefix(table_data, 0, 4);
    /* Platform Timer Offset */
    build_append_int_noprefix(table_data, 0, 4);

    acpi_table_end(linker, &table);
}

/* Debug Port Table 2 (DBG2) */
static void
build_dbg2(GArray *table_data, BIOSLinker *linker, VirtMachineState *vms)
{
    AcpiTable table = { .sig = "DBG2", .rev = 0, .oem_id = vms->oem_id,
                        .oem_table_id = vms->oem_table_id };
    int dbg2devicelength;
    const char name[] = "COM0";
    const int namespace_length = sizeof(name);

    acpi_table_begin(&table, table_data);

    dbg2devicelength = 22 + /* BaseAddressRegister[] offset */
                       12 + /* BaseAddressRegister[] */
                       4 + /* AddressSize[] */
                       namespace_length /* NamespaceString[] */;

    /* OffsetDbgDeviceInfo */
    build_append_int_noprefix(table_data, 44, 4);
    /* NumberDbgDeviceInfo */
    build_append_int_noprefix(table_data, 1, 4);

    /* Table 2. Debug Device Information structure format */
    build_append_int_noprefix(table_data, 0, 1); /* Revision */
    build_append_int_noprefix(table_data, dbg2devicelength, 2); /* Length */
    /* NumberofGenericAddressRegisters */
    build_append_int_noprefix(table_data, 1, 1);
    /* NameSpaceStringLength */
    build_append_int_noprefix(table_data, namespace_length, 2);
    build_append_int_noprefix(table_data, 38, 2); /* NameSpaceStringOffset */
    build_append_int_noprefix(table_data, 0, 2); /* OemDataLength */
    /* OemDataOffset (0 means no OEM data) */
    build_append_int_noprefix(table_data, 0, 2);

    /* Port Type */
    build_append_int_noprefix(table_data, 0x8000 /* Serial */, 2);
    /* Port Subtype */
    build_append_int_noprefix(table_data, 0x3 /* ARM PL011 UART */, 2);
    build_append_int_noprefix(table_data, 0, 2); /* Reserved */
    /* BaseAddressRegisterOffset */
    build_append_int_noprefix(table_data, 22, 2);
    /* AddressSizeOffset */
    build_append_int_noprefix(table_data, 34, 2);

    /* BaseAddressRegister[] */
    build_append_gas(table_data, AML_AS_SYSTEM_MEMORY, 32, 0, 3,
                     vms->memmap[VIRT_UART].base);

    /* AddressSize[] */
    build_append_int_noprefix(table_data,
                              vms->memmap[VIRT_UART].size, 4);

    /* NamespaceString[] */
    g_array_append_vals(table_data, name, namespace_length);

    acpi_table_end(linker, &table);
};

/*
 * ACPI spec, Revision 6.0 Errata A
 * 5.2.12 Multiple APIC Description Table (MADT)
 */
static void build_append_gicr(GArray *table_data, uint64_t base, uint32_t size)
{
    build_append_int_noprefix(table_data, 0xE, 1);  /* Type */
    build_append_int_noprefix(table_data, 16, 1);   /* Length */
    build_append_int_noprefix(table_data, 0, 2);    /* Reserved */
    /* Discovery Range Base Address */
    build_append_int_noprefix(table_data, base, 8);
    build_append_int_noprefix(table_data, size, 4); /* Discovery Range Length */
}

static uint32_t virt_acpi_get_gicc_flags(CPUState *cpu, VirtMachineState *vms)
{
    /* can only exist in 'enabled' state */
    if (!vms->cpu_hotplug_enabled) {
        return 1;
    }

    /*
     * ARM GIC CPU Interface can be 'online-capable' or 'enabled' at boot. We
     * MUST set 'online-capable' bit for all hotpluggable CPUs.
     * Change Link: https://bugzilla.tianocore.org/show_bug.cgi?id=3706
     *
     *   UEFI ACPI Specification 6.5
     *   Section: 5.2.12.14. GIC CPU Interface (GICC) Structure
     *   Table:   5.37 GICC CPU Interface Flags
     *   Link: https://uefi.org/specs/ACPI/6.5
     *
     * Cold-booted CPUs, except for the first/boot CPU, SHOULD be allowed to be
     * hot(un)plug as well but for this to happen these MUST have
     * 'online-capable' bit set. Later creates compatibility problem with legacy
     * OS as it might ignore online-capable' bits during boot time and hence
     * some CPUs might not get detected. To fix this MADT GIC CPU interface flag
     * should be allowed to have both bits set i.e. 'online-capable' and
     * 'Enabled' bits together. This change will require UEFI ACPI standard
     * change. Till this happens exposing all cold-booted CPUs as 'enabled' only
     *
     */
    return cpu && cpu->cold_booted ? 1 : (1 << 3);
}

static void
build_madt(GArray *table_data, BIOSLinker *linker, VirtMachineState *vms)
{
    int i;
    VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(vms);
    MachineState *ms = MACHINE(vms);
    const MemMapEntry *memmap = vms->memmap;
    AcpiTable table = { .sig = "APIC", .rev = 4, .oem_id = vms->oem_id,
                        .oem_table_id = vms->oem_table_id };
    unsigned int max_cpus = ms->smp.max_cpus;

    if (!vms->cpu_hotplug_enabled) {
        max_cpus = ms->smp.cpus;
    }

    acpi_table_begin(&table, table_data);
    /* Local Interrupt Controller Address */
    build_append_int_noprefix(table_data, 0, 4);
    build_append_int_noprefix(table_data, 0, 4);   /* Flags */

    /* 5.2.12.15 GIC Distributor Structure */
    build_append_int_noprefix(table_data, 0xC, 1); /* Type */
    build_append_int_noprefix(table_data, 24, 1);  /* Length */
    build_append_int_noprefix(table_data, 0, 2);   /* Reserved */
    build_append_int_noprefix(table_data, 0, 4);   /* GIC ID */
    /* Physical Base Address */
    build_append_int_noprefix(table_data, memmap[VIRT_GIC_DIST].base, 8);
    build_append_int_noprefix(table_data, 0, 4);   /* System Vector Base */
    /* GIC version */
    build_append_int_noprefix(table_data, vms->gic_version, 1);
    build_append_int_noprefix(table_data, 0, 3);   /* Reserved */

    for (i = 0; i < max_cpus; i++) {
        CPUState *cpu = qemu_get_possible_cpu(i);
        uint64_t physical_base_address = 0, gich = 0, gicv = 0;
        uint32_t vgic_interrupt = vms->virt ? ARCH_GIC_MAINT_IRQ : 0;
        uint32_t pmu_interrupt = vms->pmu ? VIRTUAL_PMU_IRQ : 0;
        uint32_t flags = virt_acpi_get_gicc_flags(cpu, vms);
        uint64_t mpidr = qemu_get_cpu_archid(i);

        if (vms->gic_version == VIRT_GIC_VERSION_2) {
            physical_base_address = memmap[VIRT_GIC_CPU].base;
            gicv = memmap[VIRT_GIC_VCPU].base;
            gich = memmap[VIRT_GIC_HYP].base;
        }

        /* 5.2.12.14 GIC Structure */
        build_append_int_noprefix(table_data, 0xB, 1);  /* Type */
        build_append_int_noprefix(table_data, 80, 1);   /* Length */
        build_append_int_noprefix(table_data, 0, 2);    /* Reserved */
        build_append_int_noprefix(table_data, i, 4);    /* GIC ID */
        build_append_int_noprefix(table_data, i, 4);    /* ACPI Processor UID */
        /* Flags */
        build_append_int_noprefix(table_data, flags, 4);
        /* Parking Protocol Version */
        build_append_int_noprefix(table_data, 0, 4);
        /* Performance Interrupt GSIV */
        build_append_int_noprefix(table_data, pmu_interrupt, 4);
        build_append_int_noprefix(table_data, 0, 8); /* Parked Address */
        /* Physical Base Address */
        build_append_int_noprefix(table_data, physical_base_address, 8);
        build_append_int_noprefix(table_data, gicv, 8); /* GICV */
        build_append_int_noprefix(table_data, gich, 8); /* GICH */
        /* VGIC Maintenance interrupt */
        build_append_int_noprefix(table_data, vgic_interrupt, 4);
        build_append_int_noprefix(table_data, 0, 8);    /* GICR Base Address*/
        /* MPIDR */
        build_append_int_noprefix(table_data, mpidr, 8);
        /* Processor Power Efficiency Class */
        build_append_int_noprefix(table_data, 0, 1);
        /* Reserved */
        build_append_int_noprefix(table_data, 0, 3);
    }

    if (vms->gic_version != VIRT_GIC_VERSION_2) {
        build_append_gicr(table_data, memmap[VIRT_GIC_REDIST].base,
                                      memmap[VIRT_GIC_REDIST].size);
        if (virt_gicv3_redist_region_count(vms) == 2) {
            build_append_gicr(table_data, memmap[VIRT_HIGH_GIC_REDIST2].base,
                                          memmap[VIRT_HIGH_GIC_REDIST2].size);
        }

        if (its_class_name() && !vmc->no_its) {
            /*
             * ACPI spec, Revision 6.0 Errata A
             * (original 6.0 definition has invalid Length)
             * 5.2.12.18 GIC ITS Structure
             */
            build_append_int_noprefix(table_data, 0xF, 1);  /* Type */
            build_append_int_noprefix(table_data, 20, 1);   /* Length */
            build_append_int_noprefix(table_data, 0, 2);    /* Reserved */
            build_append_int_noprefix(table_data, 0, 4);    /* GIC ITS ID */
            /* Physical Base Address */
            build_append_int_noprefix(table_data, memmap[VIRT_GIC_ITS].base, 8);
            build_append_int_noprefix(table_data, 0, 4);    /* Reserved */
        }
    } else {
        const uint16_t spi_base = vms->irqmap[VIRT_GIC_V2M] + ARM_SPI_BASE;

        /* 5.2.12.16 GIC MSI Frame Structure */
        build_append_int_noprefix(table_data, 0xD, 1);  /* Type */
        build_append_int_noprefix(table_data, 24, 1);   /* Length */
        build_append_int_noprefix(table_data, 0, 2);    /* Reserved */
        build_append_int_noprefix(table_data, 0, 4);    /* GIC MSI Frame ID */
        /* Physical Base Address */
        build_append_int_noprefix(table_data, memmap[VIRT_GIC_V2M].base, 8);
        build_append_int_noprefix(table_data, 1, 4);    /* Flags */
        /* SPI Count */
        build_append_int_noprefix(table_data, NUM_GICV2M_SPIS, 2);
        build_append_int_noprefix(table_data, spi_base, 2); /* SPI Base */
    }
    acpi_table_end(linker, &table);
}

/* FADT */
static void build_fadt_rev6(GArray *table_data, BIOSLinker *linker,
                            VirtMachineState *vms, unsigned dsdt_tbl_offset)
{
    /* ACPI v6.0 */
    AcpiFadtData fadt = {
        .rev = 6,
        .minor_ver = 0,
        .flags = 1 << ACPI_FADT_F_HW_REDUCED_ACPI,
        .xdsdt_tbl_offset = &dsdt_tbl_offset,
    };

    switch (vms->psci_conduit) {
    case QEMU_PSCI_CONDUIT_DISABLED:
        fadt.arm_boot_arch = 0;
        break;
    case QEMU_PSCI_CONDUIT_HVC:
        fadt.arm_boot_arch = ACPI_FADT_ARM_PSCI_COMPLIANT |
                             ACPI_FADT_ARM_PSCI_USE_HVC;
        break;
    case QEMU_PSCI_CONDUIT_SMC:
        fadt.arm_boot_arch = ACPI_FADT_ARM_PSCI_COMPLIANT;
        break;
    default:
        g_assert_not_reached();
    }

    build_fadt(table_data, linker, &fadt, vms->oem_id, vms->oem_table_id);
}

static void build_virt_osc_method(Aml *scope, VirtMachineState *vms)
{
    Aml *if_uuid, *else_uuid, *if_rev, *if_caps_masked, *method;
    Aml *a_cdw1 = aml_name("CDW1");
    Aml *a_cdw2 = aml_local(0);

    method = aml_method("_OSC", 4, AML_NOTSERIALIZED);
    aml_append(method, aml_create_dword_field(aml_arg(3), aml_int(0), "CDW1"));

    /* match UUID */
    if_uuid = aml_if(aml_equal(
        aml_arg(0), aml_touuid("0811B06E-4A27-44F9-8D60-3CBBC22E7B48")));

    aml_append(if_uuid, aml_create_dword_field(aml_arg(3), aml_int(4), "CDW2"));
    aml_append(if_uuid, aml_store(aml_name("CDW2"), a_cdw2));

    /* check unknown revision in arg(1) */
    if_rev = aml_if(aml_lnot(aml_equal(aml_arg(1), aml_int(1))));
    /* set revision error bits,  DWORD1 Bit[3] */
    aml_append(if_rev, aml_or(a_cdw1, aml_int(0x08), a_cdw1));
    aml_append(if_uuid, if_rev);

    /*
     * check support for vCPU hotplug type(=enabled) platform-wide capability
     * in DWORD2 as sepcified in the below ACPI Specification ECR,
     *  # https://bugzilla.tianocore.org/show_bug.cgi?id=4481
     */
    if (vms->acpi_dev) {
        aml_append(if_uuid, aml_and(a_cdw2, aml_int(0x800000), a_cdw2));
        /* check if OSPM specified hotplug capability bits were masked */
        if_caps_masked = aml_if(aml_lnot(aml_equal(aml_name("CDW2"), a_cdw2)));
        aml_append(if_caps_masked, aml_or(a_cdw1, aml_int(0x10), a_cdw1));
        aml_append(if_uuid, if_caps_masked);
    }
    aml_append(if_uuid, aml_store(a_cdw2, aml_name("CDW2")));

    aml_append(method, if_uuid);
    else_uuid = aml_else();

    /* set unrecognized UUID error bits, DWORD1 Bit[2] */
    aml_append(else_uuid, aml_or(a_cdw1, aml_int(4), a_cdw1));
    aml_append(method, else_uuid);

    aml_append(method, aml_return(aml_arg(3)));
    aml_append(scope, method);

    return;
}

/* DSDT */
static void
build_dsdt(GArray *table_data, BIOSLinker *linker, VirtMachineState *vms)
{
    VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(vms);
    Aml *scope, *dsdt;
    MachineState *ms = MACHINE(vms);
    const MemMapEntry *memmap = vms->memmap;
    const int *irqmap = vms->irqmap;
    AcpiTable table = { .sig = "DSDT", .rev = 2, .oem_id = vms->oem_id,
                        .oem_table_id = vms->oem_table_id };

    acpi_table_begin(&table, table_data);
    dsdt = init_aml_allocator();

    /* When booting the VM with UEFI, UEFI takes ownership of the RTC hardware.
     * While UEFI can use libfdt to disable the RTC device node in the DTB that
     * it passes to the OS, it cannot modify AML. Therefore, we won't generate
     * the RTC ACPI device at all when using UEFI.
     */
    scope = aml_scope("\\_SB");

    if (vms->cpu_hotplug_enabled) {
        CPUHotplugFeatures opts = {
             .acpi_1_compatible = false,
             .has_legacy_cphp = false
        };

        build_cpus_aml(scope, ms, opts, NULL, virt_acpi_dsdt_cpu_cppc,
                       memmap[VIRT_CPUHP_ACPI].base,
                       "\\_SB", NULL, AML_SYSTEM_MEMORY);
    } else {
        acpi_dsdt_add_cpus(scope, vms);
    }

    build_virt_osc_method(scope, vms);

    acpi_dsdt_add_uart(scope, &memmap[VIRT_UART],
                       (irqmap[VIRT_UART] + ARM_SPI_BASE));
    if (vmc->acpi_expose_flash) {
        acpi_dsdt_add_flash(scope, &memmap[VIRT_FLASH]);
    }
    fw_cfg_acpi_dsdt_add(scope, &memmap[VIRT_FW_CFG]);
    virtio_acpi_dsdt_add(scope, memmap[VIRT_MMIO].base, memmap[VIRT_MMIO].size,
                         (irqmap[VIRT_MMIO] + ARM_SPI_BASE),
                         0, NUM_VIRTIO_TRANSPORTS);
    acpi_dsdt_add_pci(scope, memmap, irqmap[VIRT_PCIE] + ARM_SPI_BASE, vms);

    if (virtcca_cvm_enabled()) {
        int kae_num = tmm_get_kae_num();
        for (int i = 0; i < kae_num; i++) {
            acpi_dsdt_add_hisi_sec(scope, &memmap[VIRT_KAE_DEVICE], i);
            acpi_dsdt_add_hisi_hpre(scope, &memmap[VIRT_KAE_DEVICE], i);
        }
    }

    if (vms->acpi_dev) {
        build_ged_aml(scope, "\\_SB."GED_DEVICE,
                      HOTPLUG_HANDLER(vms->acpi_dev),
                      irqmap[VIRT_ACPI_GED] + ARM_SPI_BASE, AML_SYSTEM_MEMORY,
                      memmap[VIRT_ACPI_GED].base);
    } else {
        acpi_dsdt_add_gpio(scope, &memmap[VIRT_GPIO],
                           (irqmap[VIRT_GPIO] + ARM_SPI_BASE));
    }

    if (vms->acpi_dev) {
        uint32_t event = object_property_get_uint(OBJECT(vms->acpi_dev),
                                                  "ged-event", &error_abort);

        if (event & ACPI_GED_MEM_HOTPLUG_EVT) {
            build_memory_hotplug_aml(scope, ms->ram_slots, "\\_SB", NULL,
                                     AML_SYSTEM_MEMORY,
                                     memmap[VIRT_PCDIMM_ACPI].base);
        }
    }

    acpi_dsdt_add_power_button(scope);
#ifdef CONFIG_TPM
    acpi_dsdt_add_tpm(scope, vms);
#endif

#ifdef CONFIG_UB
    acpi_dsdt_add_ub(scope);
#endif
#ifdef CONFIG_PAS_EXPANSION
    acpi_dsdt_add_hisi_mmcd(scope, vms);
#endif
#ifdef CONFIG_UBMEM_VMMU
    if (vms->ubmem_vmmu_realized) {
        build_ubmem_vmmu_aml(vms, scope);
    }
#endif

    aml_append(dsdt, scope);

    /* copy AML table into ACPI tables blob */
    g_array_append_vals(table_data, dsdt->buf->data, dsdt->buf->len);

    acpi_table_end(linker, &table);
    free_aml_allocator();
}

typedef
struct AcpiBuildState {
    /* Copy of table in RAM (for patching). */
    MemoryRegion *table_mr;
    MemoryRegion *rsdp_mr;
    MemoryRegion *linker_mr;
    /* Is table patched? */
    bool patched;
} AcpiBuildState;

static void acpi_align_size(GArray *blob, unsigned align)
{
    /*
     * Align size to multiple of given size. This reduces the chance
     * we need to change size in the future (breaking cross version migration).
     */
    g_array_set_size(blob, ROUND_UP(acpi_data_len(blob), align));
}

static
void virt_acpi_build(VirtMachineState *vms, AcpiBuildTables *tables)
{
    VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(vms);
    GArray *table_offsets;
    unsigned dsdt, xsdt;
    GArray *tables_blob = tables->table_data;
    MachineState *ms = MACHINE(vms);
    CPUCoreCaches caches[CPU_MAX_CACHES];
    unsigned int num_caches;

    /*
     * Ensure default cache topology is set when the user has not
     * configured any -smp-cache option, so PPTT is not empty.
     * Must run before virt_get_caches() so that caches[] is filled
     * consistently with the topology configuration.
     */
    virt_set_default_cache_topology(ms);

    num_caches = virt_get_caches(vms, caches);

    table_offsets = g_array_new(false, true /* clear */,
                                        sizeof(uint32_t));

    bios_linker_loader_alloc(tables->linker,
                             ACPI_BUILD_TABLE_FILE, tables_blob,
                             64, false /* high memory */);

    /* DSDT is pointed to by FADT */
    dsdt = tables_blob->len;
    build_dsdt(tables_blob, tables->linker, vms);
#ifdef CONFIG_UB
    acpi_add_table(table_offsets, tables_blob);
    build_ubrt(tables_blob, tables->linker);
#endif
    /* FADT MADT PPTT GTDT MCFG SPCR DBG2 pointed to by RSDT */
    acpi_add_table(table_offsets, tables_blob);
    build_fadt_rev6(tables_blob, tables->linker, vms, dsdt);

    acpi_add_table(table_offsets, tables_blob);
    build_madt(tables_blob, tables->linker, vms);

    if (!vmc->no_cpu_topology) {
        acpi_add_table(table_offsets, tables_blob);
        build_pptt_arm(tables_blob, tables->linker, ms,
                   vms->oem_id, vms->oem_table_id,
                   num_caches, caches);
    }

    acpi_add_table(table_offsets, tables_blob);
    build_gtdt(tables_blob, tables->linker, vms);

    acpi_add_table(table_offsets, tables_blob);
    {
        AcpiMcfgInfo mcfg = {
           .base = vms->memmap[VIRT_ECAM_ID(vms->highmem_ecam)].base,
           .size = vms->memmap[VIRT_ECAM_ID(vms->highmem_ecam)].size,
        };
        build_mcfg(tables_blob, tables->linker, &mcfg, vms->oem_id,
                   vms->oem_table_id);
    }

    acpi_add_table(table_offsets, tables_blob);

    if (ms->acpi_spcr_enabled) {
        spcr_setup(tables_blob, tables->linker, vms);
    }

    acpi_add_table(table_offsets, tables_blob);
    build_dbg2(tables_blob, tables->linker, vms);

    if (vms->ras) {
        build_ghes_error_table(tables->hardware_errors, tables->linker);
        acpi_add_table(table_offsets, tables_blob);
        acpi_build_hest(tables_blob, tables->linker, vms->oem_id,
                        vms->oem_table_id);
    }

    if (ms->numa_state->num_nodes > 0) {
        acpi_add_table(table_offsets, tables_blob);
        build_srat(tables_blob, tables->linker, vms);
        if (ms->numa_state->have_numa_distance) {
            acpi_add_table(table_offsets, tables_blob);
            build_slit(tables_blob, tables->linker, ms, vms->oem_id,
                       vms->oem_table_id);
        }

        if (ms->numa_state->hmat_enabled) {
            acpi_add_table(table_offsets, tables_blob);
            build_hmat(tables_blob, tables->linker, ms->numa_state,
                       vms->oem_id, vms->oem_table_id);
        }
    }

    if (ms->nvdimms_state->is_enabled) {
        nvdimm_build_acpi(table_offsets, tables_blob, tables->linker,
                          ms->nvdimms_state, ms->ram_slots, vms->oem_id,
                          vms->oem_table_id);
    }

    if (its_class_name() && !vmc->no_its) {
        acpi_add_table(table_offsets, tables_blob);
        build_iort(tables_blob, tables->linker, vms);
    }

#ifdef CONFIG_TPM
    if (tpm_get_version(tpm_find()) == TPM_VERSION_2_0) {
        acpi_add_table(table_offsets, tables_blob);
        build_tpm2(tables_blob, tables->linker, tables->tcpalog, vms->oem_id,
                   vms->oem_table_id);
    }
#endif

    if (vms->iommu == VIRT_IOMMU_VIRTIO) {
        acpi_add_table(table_offsets, tables_blob);
        build_viot(ms, tables_blob, tables->linker, vms->virtio_iommu_bdf,
                   vms->oem_id, vms->oem_table_id);
    }

    /* XSDT is pointed to by RSDP */
    xsdt = tables_blob->len;
    build_xsdt(tables_blob, tables->linker, table_offsets, vms->oem_id,
               vms->oem_table_id);

    /* RSDP is in FSEG memory, so allocate it separately */
    {
        AcpiRsdpData rsdp_data = {
            .revision = 2,
            .oem_id = vms->oem_id,
            .xsdt_tbl_offset = &xsdt,
            .rsdt_tbl_offset = NULL,
        };
        build_rsdp(tables->rsdp, tables->linker, &rsdp_data);
    }

    /*
     * The align size is 128, warn if 64k is not enough therefore
     * the align size could be resized.
     */
    if (tables_blob->len > ACPI_BUILD_TABLE_SIZE / 2) {
        warn_report("ACPI table size %u exceeds %d bytes,"
                    " migration may not work",
                    tables_blob->len, ACPI_BUILD_TABLE_SIZE / 2);
        error_printf("Try removing CPUs, NUMA nodes, memory slots"
                     " or PCI bridges.");
    }
    acpi_align_size(tables_blob, ACPI_BUILD_TABLE_SIZE);
    /* Cleanup memory that's no longer used. */
    g_array_free(table_offsets, true);
}

static void acpi_ram_update(MemoryRegion *mr, GArray *data)
{
    uint32_t size = acpi_data_len(data);

    /* Make sure RAM size is correct - in case it got changed
     * e.g. by migration */
    memory_region_ram_resize(mr, size, &error_abort);

    memcpy(memory_region_get_ram_ptr(mr), data->data, size);
    memory_region_set_dirty(mr, 0, size);
}

static void virt_acpi_build_update(void *build_opaque)
{
    AcpiBuildState *build_state = build_opaque;
    AcpiBuildTables tables;

    /* No state to update or already patched? Nothing to do. */
    if (!build_state || build_state->patched) {
        return;
    }
    build_state->patched = true;

    acpi_build_tables_init(&tables);

    virt_acpi_build(VIRT_MACHINE(qdev_get_machine()), &tables);

    acpi_ram_update(build_state->table_mr, tables.table_data);
    acpi_ram_update(build_state->rsdp_mr, tables.rsdp);
    acpi_ram_update(build_state->linker_mr, tables.linker->cmd_blob);

    acpi_build_tables_cleanup(&tables, true);
}

static void virt_acpi_build_reset(void *build_opaque)
{
    AcpiBuildState *build_state = build_opaque;
    build_state->patched = false;
}

static const VMStateDescription vmstate_virt_acpi_build = {
    .name = "virt_acpi_build",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_BOOL(patched, AcpiBuildState),
        VMSTATE_END_OF_LIST()
    },
};

void virt_acpi_setup(VirtMachineState *vms)
{
    AcpiBuildTables tables;
    AcpiBuildState *build_state;
    AcpiGedState *acpi_ged_state;

    if (!vms->fw_cfg) {
        trace_virt_acpi_setup();
        return;
    }

    if (!virt_is_acpi_enabled(vms)) {
        trace_virt_acpi_setup();
        return;
    }

    build_state = g_malloc0(sizeof *build_state);

    acpi_build_tables_init(&tables);
    virt_acpi_build(vms, &tables);

    /* Now expose it all to Guest */
    build_state->table_mr = acpi_add_rom_blob(virt_acpi_build_update,
                                              build_state, tables.table_data,
                                              ACPI_BUILD_TABLE_FILE);
    assert(build_state->table_mr != NULL);

    build_state->linker_mr = acpi_add_rom_blob(virt_acpi_build_update,
                                               build_state,
                                               tables.linker->cmd_blob,
                                               ACPI_BUILD_LOADER_FILE);

    fw_cfg_add_file(vms->fw_cfg, ACPI_BUILD_TPMLOG_FILE, tables.tcpalog->data,
                    acpi_data_len(tables.tcpalog));

    if (vms->ras) {
        assert(vms->acpi_dev);
        acpi_ged_state = ACPI_GED(vms->acpi_dev);
        acpi_ghes_add_fw_cfg(&acpi_ged_state->ghes_state,
                             vms->fw_cfg, tables.hardware_errors);
    }

    build_state->rsdp_mr = acpi_add_rom_blob(virt_acpi_build_update,
                                             build_state, tables.rsdp,
                                             ACPI_BUILD_RSDP_FILE);

    qemu_register_reset(virt_acpi_build_reset, build_state);
    virt_acpi_build_reset(build_state);
    vmstate_register(NULL, 0, &vmstate_virt_acpi_build, build_state);

    /* Cleanup tables but don't free the memory: we track it
     * in build_state.
     */
    acpi_build_tables_cleanup(&tables, false);
}
