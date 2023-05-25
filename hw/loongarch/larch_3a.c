/*
 * QEMU loongarch 3a develop board emulation
 *
 * Copyright (c) 2023 Loongarch Technology
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/datadir.h"
#include "hw/hw.h"
#include "hw/loongarch/cpudevs.h"
#include "hw/i386/pc.h"
#include "hw/char/serial.h"
#include "hw/isa/isa.h"
#include "hw/qdev-core.h"
#include "sysemu/sysemu.h"
#include "sysemu/runstate.h"
#include "sysemu/reset.h"
#include "migration/vmstate.h"
#include "sysemu/cpus.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "hw/loongarch/bios.h"
#include "hw/loader.h"
#include "elf.h"
#include "exec/address-spaces.h"
#include "hw/ide.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/msi.h"
#include "linux/kvm.h"
#include "sysemu/kvm.h"
#include "sysemu/numa.h"
#include "hw/rtc/mc146818rtc.h"
#include "hw/irq.h"
#include "net/net.h"
#include "hw/platform-bus.h"
#include "hw/timer/i8254.h"
#include "hw/loongarch/larch.h"
#include "hw/loongarch/ls7a.h"
#include "hw/nvram/fw_cfg.h"
#include "hw/firmware/smbios.h"
#include "acpi-build.h"
#include <sys/wait.h>
#include <sys/resource.h>
#include "sysemu/block-backend.h"
#include "hw/block/flash.h"
#include "sysemu/device_tree.h"
#include "qapi/visitor.h"
#include "qapi/qapi-visit-common.h"
#include "sysemu/tpm.h"
#include "hw/loongarch/sysbus-fdt.h"

#include <libfdt.h>

#define DMA64_SUPPORTED 0x2
#define MAX_IDE_BUS 2

#define BOOTPARAM_PHYADDR 0x0ff00000ULL
#define BOOTPARAM_ADDR (0x9000000000000000ULL + BOOTPARAM_PHYADDR)
#define SMBIOS_PHYSICAL_ADDRESS 0x0fe00000
#define SMBIOS_SIZE_LIMIT 0x200000
#define RESERVED_SIZE_LIMIT 0x1100000
#define COMMAND_LINE_SIZE 4096
#define FW_CONF_ADDR 0x0fff0000

#define PHYS_TO_VIRT(x) ((x) | 0x9000000000000000ULL)

#define TARGET_REALPAGE_MASK (TARGET_PAGE_MASK << 2)

#ifdef CONFIG_KVM
#define LS_ISA_IO_SIZE 0x02000000
#else
#define LS_ISA_IO_SIZE 0x00010000
#endif

#ifdef CONFIG_KVM
#define align(x) (((x) + 63) & ~63)
#else
#define align(x) (((x) + 15) & ~15)
#endif

#define DEBUG_LOONGARCH3A 0
#define FLASH_SECTOR_SIZE 4096

#define DPRINTF(fmt, ...)                                                     \
    do {                                                                      \
        if (DEBUG_LOONGARCH3A) {                                              \
            fprintf(stderr, fmt, ##__VA_ARGS__);                              \
        }                                                                     \
    } while (0)

#define DEFINE_LS3A5K_MACHINE(suffix, name, optionfn)                         \
    static void ls3a5k_init_##suffix(MachineState *machine)                   \
    {                                                                         \
        ls3a5k_init(machine);                                                 \
    }                                                                         \
    DEFINE_LOONGARCH_MACHINE(suffix, name, ls3a5k_init_##suffix, optionfn)

struct efi_memory_map_loongarch {
    uint16_t vers;     /* version of efi_memory_map */
    uint32_t nr_map;   /* number of memory_maps */
    uint32_t mem_freq; /* memory frequence */
    struct mem_map {
        uint32_t node_id;   /* node_id which memory attached to */
        uint32_t mem_type;  /* system memory, pci memory, pci io, etc. */
        uint64_t mem_start; /* memory map start address */
        uint32_t mem_size;  /* each memory_map size, not the total size */
    } map[128];
} __attribute__((packed));

enum loongarch_cpu_type { Loongson3 = 0x1, Loongson3_comp = 0x2 };

struct GlobalProperty loongarch_compat[] = {
    {
        .driver = "rtl8139",
        .property = "romfile",
        .value = "",
    },
    {
        .driver = "e1000",
        .property = "romfile",
        .value = "",
    },
    {
        .driver = "virtio-net-pci",
        .property = "romfile",
        .value = "",
    },
    {
        .driver = "qxl-vga",
        .property = "romfile",
        .value = "",
    },
    {
        .driver = "VGA",
        .property = "romfile",
        .value = "",
    },
    {
        .driver = "cirrus-vga",
        .property = "romfile",
        .value = "",
    },
    {
        .driver = "virtio-vga",
        .property = "romfile",
        .value = "",
    },
    {
        .driver = "vmware-svga",
        .property = "romfile",
        .value = "",
    },
};
const size_t loongarch_compat_len = G_N_ELEMENTS(loongarch_compat);

/*
 * Capability and feature descriptor structure for LOONGARCH CPU
 */
struct efi_cpuinfo_loongarch {
    uint16_t vers;                   /* version of efi_cpuinfo_loongarch */
    uint32_t processor_id;           /* PRID, e.g. 6305, 6306 */
    enum loongarch_cpu_type cputype; /* 3A, 3B, etc. */
    uint32_t total_node;             /* num of total numa nodes */
    uint16_t cpu_startup_core_id;    /* Core id */
    uint16_t reserved_cores_mask;
    uint32_t cpu_clock_freq; /* cpu_clock */
    uint32_t nr_cpus;
} __attribute__((packed));

#define MAX_UARTS 64
struct uart_device {
    uint32_t iotype; /* see include/linux/serial_core.h */
    uint32_t uartclk;
    uint32_t int_offset;
    uint64_t uart_base;
} __attribute__((packed));

#define MAX_SENSORS 64
#define SENSOR_TEMPER 0x00000001
#define SENSOR_VOLTAGE 0x00000002
#define SENSOR_FAN 0x00000004
struct sensor_device {
    char name[32];  /* a formal name */
    char label[64]; /* a flexible description */
    uint32_t type;  /* SENSOR_* */
    uint32_t id;    /* instance id of a sensor-class */
    /*
     * see arch/loongarch/include/
     * asm/mach-loongarch/loongarch_hwmon.h
     */
    uint32_t fan_policy;
    uint32_t fan_percent; /* only for constant speed policy */
    uint64_t base_addr;   /* base address of device registers */
} __attribute__((packed));

struct system_loongarch {
    uint16_t vers;                /* version of system_loongarch */
    uint32_t ccnuma_smp;          /* 0: no numa; 1: has numa */
    uint32_t sing_double_channel; /* 1:single; 2:double */
    uint32_t nr_uarts;
    struct uart_device uarts[MAX_UARTS];
    uint32_t nr_sensors;
    struct sensor_device sensors[MAX_SENSORS];
    char has_ec;
    char ec_name[32];
    uint64_t ec_base_addr;
    char has_tcm;
    char tcm_name[32];
    uint64_t tcm_base_addr;
    uint64_t workarounds; /* see workarounds.h */
} __attribute__((packed));

struct irq_source_routing_table {
    uint16_t vers;
    uint16_t size;
    uint16_t rtr_bus;
    uint16_t rtr_devfn;
    uint32_t vendor;
    uint32_t device;
    uint32_t PIC_type;   /* conform use HT or PCI to route to CPU-PIC */
    uint64_t ht_int_bit; /* 3A: 1<<24; 3B: 1<<16 */
    uint64_t ht_enable;  /* irqs used in this PIC */
    uint32_t node_id;    /* node id: 0x0-0; 0x1-1; 0x10-2; 0x11-3 */
    uint64_t pci_mem_start_addr;
    uint64_t pci_mem_end_addr;
    uint64_t pci_io_start_addr;
    uint64_t pci_io_end_addr;
    uint64_t pci_config_addr;
    uint32_t dma_mask_bits;
    uint16_t dma_noncoherent;
} __attribute__((packed));

struct interface_info {
    uint16_t vers; /* version of the specificition */
    uint16_t size;
    uint8_t flag;
    char description[64];
} __attribute__((packed));

#define MAX_RESOURCE_NUMBER 128
struct resource_loongarch {
    uint64_t start; /* resource start address */
    uint64_t end;   /* resource end address */
    char name[64];
    uint32_t flags;
};

struct archdev_data {
}; /* arch specific additions */

struct board_devices {
    char name[64];          /* hold the device name */
    uint32_t num_resources; /* number of device_resource */
    /* for each device's resource */
    struct resource_loongarch resource[MAX_RESOURCE_NUMBER];
    /* arch specific additions */
    struct archdev_data archdata;
};

struct loongarch_special_attribute {
    uint16_t vers;                   /* version of this special */
    char special_name[64];           /* special_atribute_name */
    uint32_t loongarch_special_type; /* type of special device */
    /* for each device's resource */
    struct resource_loongarch resource[MAX_RESOURCE_NUMBER];
};

struct loongarch_params {
    uint64_t memory_offset;    /* efi_memory_map_loongarch struct offset */
    uint64_t cpu_offset;       /* efi_cpuinfo_loongarch struct offset */
    uint64_t system_offset;    /* system_loongarch struct offset */
    uint64_t irq_offset;       /* irq_source_routing_table struct offset */
    uint64_t interface_offset; /* interface_info struct offset */
    uint64_t special_offset;   /* loongarch_special_attribute struct offset */
    uint64_t boarddev_table_offset; /* board_devices offset */
};

struct smbios_tables {
    uint16_t vers;     /* version of smbios */
    uint64_t vga_bios; /* vga_bios address */
    struct loongarch_params lp;
};

struct efi_reset_system_t {
    uint64_t ResetCold;
    uint64_t ResetWarm;
    uint64_t ResetType;
    uint64_t Shutdown;
    uint64_t DoSuspend; /* NULL if not support */
};

struct efi_loongarch {
    uint64_t mps;                /* MPS table */
    uint64_t acpi;               /* ACPI table (IA64 ext 0.71) */
    uint64_t acpi20;             /* ACPI table (ACPI 2.0) */
    struct smbios_tables smbios; /* SM BIOS table */
    uint64_t sal_systab;         /* SAL system table */
    uint64_t boot_info;          /* boot info table */
};

struct boot_params {
    struct efi_loongarch efi;
    struct efi_reset_system_t reset_system;
};

static struct _loaderparams {
    unsigned long ram_size;
    const char *kernel_filename;
    const char *kernel_cmdline;
    const char *initrd_filename;
    unsigned long a0, a1, a2;
} loaderparams;

static struct _firmware_config {
    unsigned long ram_size;
    unsigned int mem_freq;
    unsigned int cpu_nr;
    unsigned int cpu_clock_freq;
} fw_config;

struct la_memmap_entry {
    uint64_t address;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
};

static void *boot_params_buf;
static void *boot_params_p;
static struct la_memmap_entry *la_memmap_table;
static unsigned la_memmap_entries;

CPULOONGARCHState *cpu_states[LOONGARCH_MAX_VCPUS];

struct kvm_cpucfg ls3a5k_cpucfgs = {
    .cpucfg[LOONGARCH_CPUCFG0] = CPUCFG0_3A5000_PRID,
    .cpucfg[LOONGARCH_CPUCFG1] =
        CPUCFG1_ISGR64 | CPUCFG1_PAGING | CPUCFG1_IOCSR | CPUCFG1_PABITS |
        CPUCFG1_VABITS | CPUCFG1_UAL | CPUCFG1_RI | CPUCFG1_XI | CPUCFG1_RPLV |
        CPUCFG1_HUGEPG | CPUCFG1_IOCSRBRD,
    .cpucfg[LOONGARCH_CPUCFG2] =
        CPUCFG2_FP | CPUCFG2_FPSP | CPUCFG2_FPDP | CPUCFG2_FPVERS |
        CPUCFG2_LSX | CPUCFG2_LASX | CPUCFG2_COMPLEX | CPUCFG2_CRYPTO |
        CPUCFG2_LLFTP | CPUCFG2_LLFTPREV | CPUCFG2_LSPW | CPUCFG2_LAM,
    .cpucfg[LOONGARCH_CPUCFG3] =
        CPUCFG3_CCDMA | CPUCFG3_SFB | CPUCFG3_UCACC | CPUCFG3_LLEXC |
        CPUCFG3_SCDLY | CPUCFG3_LLDBAR | CPUCFG3_ITLBT | CPUCFG3_ICACHET |
        CPUCFG3_SPW_LVL | CPUCFG3_SPW_HG_HF | CPUCFG3_RVA | CPUCFG3_RVAMAX,
    .cpucfg[LOONGARCH_CPUCFG4] = CCFREQ_100M,
    .cpucfg[LOONGARCH_CPUCFG5] = CPUCFG5_CCMUL | CPUCFG5_CCDIV,
    .cpucfg[LOONGARCH_CPUCFG6] = CPUCFG6_PMP | CPUCFG6_PAMVER | CPUCFG6_PMNUM |
                                 CPUCFG6_PMBITS | CPUCFG6_UPM,
    .cpucfg[LOONGARCH_CPUCFG16] = CPUCFG16_L1_IUPRE | CPUCFG16_L1_DPRE |
                                  CPUCFG16_L2_IUPRE | CPUCFG16_L2_IUUNIFY |
                                  CPUCFG16_L2_IUPRIV | CPUCFG16_L3_IUPRE |
                                  CPUCFG16_L3_IUUNIFY | CPUCFG16_L3_IUINCL,
    .cpucfg[LOONGARCH_CPUCFG17] =
        CPUCFG17_L1I_WAYS_M | CPUCFG17_L1I_SETS_M | CPUCFG17_L1I_SIZE_M,
    .cpucfg[LOONGARCH_CPUCFG18] =
        CPUCFG18_L1D_WAYS_M | CPUCFG18_L1D_SETS_M | CPUCFG18_L1D_SIZE_M,
    .cpucfg[LOONGARCH_CPUCFG19] =
        CPUCFG19_L2_WAYS_M | CPUCFG19_L2_SETS_M | CPUCFG19_L2_SIZE_M,
    .cpucfg[LOONGARCH_CPUCFG20] =
        CPUCFG20_L3_WAYS_M | CPUCFG20_L3_SETS_M | CPUCFG20_L3_SIZE_M,
};

bool loongarch_is_acpi_enabled(LoongarchMachineState *vms)
{
    if (vms->acpi == ON_OFF_AUTO_OFF) {
        return false;
    }
    return true;
}

static void loongarch_get_acpi(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    LoongarchMachineState *lsms = LoongarchMACHINE(obj);
    OnOffAuto acpi = lsms->acpi;

    visit_type_OnOffAuto(v, name, &acpi, errp);
}

static void loongarch_set_acpi(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    LoongarchMachineState *lsms = LoongarchMACHINE(obj);

    visit_type_OnOffAuto(v, name, &lsms->acpi, errp);
}

int la_memmap_add_entry(uint64_t address, uint64_t length, uint32_t type)
{
    int i;

    for (i = 0; i < la_memmap_entries; i++) {
        if (la_memmap_table[i].address == address) {
            fprintf(stderr, "%s address:0x%lx length:0x%lx already exists\n",
                    __func__, address, length);
            return 0;
        }
    }

    la_memmap_table = g_renew(struct la_memmap_entry, la_memmap_table,
                              la_memmap_entries + 1);
    la_memmap_table[la_memmap_entries].address = cpu_to_le64(address);
    la_memmap_table[la_memmap_entries].length = cpu_to_le64(length);
    la_memmap_table[la_memmap_entries].type = cpu_to_le32(type);
    la_memmap_entries++;

    return la_memmap_entries;
}

static ram_addr_t get_hotplug_membase(ram_addr_t ram_size)
{
    ram_addr_t sstart;

    if (ram_size <= 0x10000000) {
        sstart = 0x90000000;
    } else {
        sstart = 0x90000000 + ROUND_UP((ram_size - 0x10000000),
                                       LOONGARCH_HOTPLUG_MEM_ALIGN);
    }
    return sstart;
}

static struct efi_memory_map_loongarch *init_memory_map(void *g_map)
{
    struct efi_memory_map_loongarch *emap = g_map;

    emap->nr_map = 4;
    emap->mem_freq = 266000000;

    emap->map[0].node_id = 0;
    emap->map[0].mem_type = 1;
    emap->map[0].mem_start = 0x0;
#ifdef CONFIG_KVM
    emap->map[0].mem_size =
        (loaderparams.ram_size > 0x10000000 ? 256
                                            : (loaderparams.ram_size >> 20)) -
        18;
#else
    emap->map[0].mem_size = atoi(getenv("memsize"));
#endif

    emap->map[1].node_id = 0;
    emap->map[1].mem_type = 2;
    emap->map[1].mem_start = 0x90000000;
#ifdef CONFIG_KVM
    emap->map[1].mem_size = (loaderparams.ram_size > 0x10000000
                                 ? (loaderparams.ram_size >> 20) - 256
                                 : 0);
#else
    emap->map[1].mem_size = atoi(getenv("highmemsize"));
#endif

    /* support for smbios */
    emap->map[2].node_id = 0;
    emap->map[2].mem_type = 10;
    emap->map[2].mem_start = SMBIOS_PHYSICAL_ADDRESS;
    emap->map[2].mem_size = SMBIOS_SIZE_LIMIT >> 20;

    emap->map[3].node_id = 0;
    emap->map[3].mem_type = 3;
    emap->map[3].mem_start = 0xee00000;
    emap->map[3].mem_size = RESERVED_SIZE_LIMIT >> 20;

    return emap;
}

static uint64_t get_host_cpu_freq(void)
{
    int fd = 0;
    char buf[1024];
    uint64_t freq = 0, size = 0;
    char *buf_p;

    fd = open("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
              O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "/sys/devices/system/cpu/cpu0/cpufreq/ \
                cpuinfo_max_freq not exist!\n");
        fprintf(stderr, "Trying /proc/cpuinfo...\n");
    } else {
        size = read(fd, buf, 16);
        if (size == -1) {
            fprintf(stderr, "read err...\n");
        }
        close(fd);
        freq = (uint64_t)atoi(buf);
        return freq * 1000;
    }

    fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open /proc/cpuinfo!\n");
        return 0;
    }

    size = read(fd, buf, 1024);
    if (size == -1) {
        fprintf(stderr, "read err...\n");
    }
    close(fd);

    buf_p = strstr(buf, "MHz");
    if (buf_p) {
        while (*buf_p != ':') {
            buf_p++;
        }
        buf_p += 2;
    } else {
        buf_p = strstr(buf, "name");
        while (*buf_p != '@') {
            buf_p++;
        }
        buf_p += 2;
    }

    memcpy(buf, buf_p, 12);
    buf_p = buf;
    while ((*buf_p >= '0') && (*buf_p <= '9')) {
        buf_p++;
    }
    *buf_p = '\0';

    freq = (uint64_t)atoi(buf);
    return freq * 1000 * 1000;
}

static char *get_host_cpu_model_name(void)
{
    int fd = 0;
    int size = 0;
    static char buf[1024];
    char *buf_p;

    fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open /proc/cpuinfo!\n");
        return 0;
    }

    size = read(fd, buf, 1024);
    if (size == -1) {
        fprintf(stderr, "read err...\n");
    }
    close(fd);
    buf_p = strstr(buf, "Name");
    if (!buf_p) {
        buf_p = strstr(buf, "name");
    }
    if (!buf_p) {
        fprintf(stderr, "Can't find cpu name\n");
        return 0;
    }

    while (*buf_p != ':') {
        buf_p++;
    }
    buf_p = buf_p + 2;
    memcpy(buf, buf_p, 40);
    buf_p = buf;
    while (*buf_p != '\n') {
        buf_p++;
    }

    *(buf_p) = '\0';

    return buf;
}

static void fw_conf_init(unsigned long ramsize)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    int smp_cpus = ms->smp.cpus;
    fw_config.ram_size = ramsize;
    fw_config.mem_freq = 266000000;
    fw_config.cpu_nr = smp_cpus;
    fw_config.cpu_clock_freq = get_host_cpu_freq();
}

static struct efi_cpuinfo_loongarch *init_cpu_info(void *g_cpuinfo_loongarch)
{
    struct efi_cpuinfo_loongarch *c = g_cpuinfo_loongarch;
    MachineState *ms = MACHINE(qdev_get_machine());
    int smp_cpus = ms->smp.cpus;
    LoongarchMachineState *lsms = LoongarchMACHINE(qdev_get_machine());
    LoongarchMachineClass *lsmc = LoongarchMACHINE_GET_CLASS(lsms);

    if (strstr(lsmc->cpu_name, "5000")) {
        c->processor_id = 0x14c010;
    }
    c->cputype = Loongson3_comp;
    c->cpu_clock_freq = get_host_cpu_freq();
    if (!c->cpu_clock_freq) {
        c->cpu_clock_freq = 200000000;
    }
    c->total_node = 1;
    c->nr_cpus = smp_cpus;
    c->cpu_startup_core_id = 0;
    c->reserved_cores_mask = 0xffff & (0xffff << smp_cpus);

    return c;
}

static struct system_loongarch *init_system_loongarch(void *g_sysitem)
{
    struct system_loongarch *s = g_sysitem;

    s->ccnuma_smp = 1;
    s->ccnuma_smp = 0;
    s->sing_double_channel = 1;

    return s;
}

enum loongarch_irq_source_enum { HT, I8259, UNKNOWN };

static struct irq_source_routing_table *init_irq_source(void *g_irq_source)
{
    struct irq_source_routing_table *irq_info = g_irq_source;
    LoongarchMachineState *lsms = LoongarchMACHINE(qdev_get_machine());
    LoongarchMachineClass *lsmc = LoongarchMACHINE_GET_CLASS(lsms);

    irq_info->PIC_type = HT;
    irq_info->ht_int_bit = 1 << 24;
    irq_info->ht_enable = 0x0000d17b;
    irq_info->node_id = 0;

    irq_info->pci_mem_start_addr = PCIE_MEMORY_BASE;
    irq_info->pci_mem_end_addr =
        irq_info->pci_mem_start_addr + PCIE_MEMORY_SIZE - 1;

    if (strstr(lsmc->cpu_name, "5000")) {
        irq_info->pci_io_start_addr = LS3A5K_ISA_IO_BASE;
    }
    irq_info->dma_noncoherent = 1;
    return irq_info;
}

static struct interface_info *init_interface_info(void *g_interface)
{
    struct interface_info *inter = g_interface;
    int flashsize = 0x80000;

    inter->vers = 0x0001;
    inter->size = flashsize / 0x400;
    inter->flag = 1;

    strcpy(inter->description, "PMON_Version_v2.1");

    return inter;
}

static struct board_devices *board_devices_info(void *g_board)
{
    struct board_devices *bd = g_board;
    LoongarchMachineState *lsms = LoongarchMACHINE(qdev_get_machine());
    LoongarchMachineClass *lsmc = LoongarchMACHINE_GET_CLASS(lsms);

    if (!strcmp(lsmc->bridge_name, "ls7a")) {
        strcpy(bd->name, "Loongarch-3A-7A-1w-V1.03-demo");
    }
    bd->num_resources = 10;

    return bd;
}

static struct loongarch_special_attribute *init_special_info(void *g_special)
{
    struct loongarch_special_attribute *special = g_special;
    char update[11] = "2013-01-01";
    int VRAM_SIZE = 0x20000;

    strcpy(special->special_name, update);
    special->resource[0].flags = 0;
    special->resource[0].start = 0;
    special->resource[0].end = VRAM_SIZE;
    strcpy(special->resource[0].name, "SPMODULE");
    special->resource[0].flags |= DMA64_SUPPORTED;

    return special;
}

static void init_loongarch_params(struct loongarch_params *lp)
{
    void *p = boot_params_p;

    lp->memory_offset =
        (unsigned long long)init_memory_map(p) - (unsigned long long)lp;
    p += align(sizeof(struct efi_memory_map_loongarch));

    lp->cpu_offset =
        (unsigned long long)init_cpu_info(p) - (unsigned long long)lp;
    p += align(sizeof(struct efi_cpuinfo_loongarch));

    lp->system_offset =
        (unsigned long long)init_system_loongarch(p) - (unsigned long long)lp;
    p += align(sizeof(struct system_loongarch));

    lp->irq_offset =
        (unsigned long long)init_irq_source(p) - (unsigned long long)lp;
    p += align(sizeof(struct irq_source_routing_table));

    lp->interface_offset =
        (unsigned long long)init_interface_info(p) - (unsigned long long)lp;
    p += align(sizeof(struct interface_info));

    lp->boarddev_table_offset =
        (unsigned long long)board_devices_info(p) - (unsigned long long)lp;
    p += align(sizeof(struct board_devices));

    lp->special_offset =
        (unsigned long long)init_special_info(p) - (unsigned long long)lp;
    p += align(sizeof(struct loongarch_special_attribute));

    boot_params_p = p;
}

static void init_smbios(struct smbios_tables *smbios)
{
    smbios->vers = 1;
    smbios->vga_bios = 1;
    init_loongarch_params(&(smbios->lp));
}

static void init_efi(struct efi_loongarch *efi)
{
    init_smbios(&(efi->smbios));
}

static int init_boot_param(struct boot_params *bp)
{
    init_efi(&(bp->efi));

    return 0;
}

static unsigned int ls3a5k_aui_boot_code[] = {
    0x0380200d, /* ori $r13,$r0,0x8             */
    0x0400002d, /* csrwr   $r13,0x0             */
    0x0401000e, /* csrrd   $r14,0x40            */
    0x0343fdce, /* andi    $r14,$r14,0xff       */
    0x143fc02c, /* lu12i.w $r12,261889(0x1fe01) */
    0x1600000c, /* lu32i.d $r12,0               */
    0x0320018c, /* lu52i.d $r12,$r12,-1792(0x800) */
    0x03400dcf, /* andi    $r15,$r14,0x3        */
    0x004121ef, /* slli.d  $r15,$r15,0x8        */
    0x00153d8c, /* or  $r12,$r12,$r15           */
    0x034031d0, /* andi    $r16,$r14,0xc        */
    0x0041aa10, /* slli.d  $r16,$r16,0x2a       */
    0x0015418c, /* or  $r12,$r12,$r16           */
    0x28808184, /* ld.w    $r4,$r12,32(0x20)    */
    0x43fffc9f, /* beqz    $r4,0   -4 <waitforinit+0x4> */
    0x28c08184, /* ld.d    $r4,$r12,32(0x20)    */
    0x28c0a183, /* ld.d    $r3,$r12,40(0x28)    */
    0x28c0c182, /* ld.d    $r2,$r12,48(0x30)    */
    0x28c0e185, /* ld.d    $r5,$r12,56(0x38)    */
    0x4c000080, /* jirl    $r0,$r4,0            */
};

static int set_bootparam_uefi(ram_addr_t initrd_offset, long initrd_size)
{
    long params_size;
    char memenv[32];
    char highmemenv[32];
    void *params_buf;
    unsigned long *parg_env;
    int ret = 0;

    /* Allocate params_buf for command line. */
    params_size = 0x100000;
    params_buf = g_malloc0(params_size);

    /*
     * Layout of params_buf looks like this:
     * argv[0], argv[1], 0, env[0], env[1], ...env[i], 0,
     * argv[0]'s data, argv[1]'s data, env[0]'data, ..., env[i]'s data, 0
     */
    parg_env = (void *)params_buf;

    ret = (3 + 1) * sizeof(target_ulong);
    *parg_env++ = (BOOTPARAM_ADDR + ret);
    ret += (1 + snprintf(params_buf + ret, COMMAND_LINE_SIZE - ret, "g"));

    /* argv1 */
    *parg_env++ = BOOTPARAM_ADDR + ret;
    if (initrd_size > 0)
        ret += (1 + snprintf(params_buf + ret, COMMAND_LINE_SIZE - ret,
                             "rd_start=0x%llx rd_size=%li %s",
                             PHYS_TO_VIRT((uint32_t)initrd_offset),
                             initrd_size, loaderparams.kernel_cmdline));
    else
        ret += (1 + snprintf(params_buf + ret, COMMAND_LINE_SIZE - ret, "%s",
                             loaderparams.kernel_cmdline));

    /* argv2 */
    *parg_env++ = 0;

    /* env */
    sprintf(memenv, "%lu",
            loaderparams.ram_size > 0x10000000
                ? 256
                : (loaderparams.ram_size >> 20));
    sprintf(highmemenv, "%lu",
            loaderparams.ram_size > 0x10000000
                ? (loaderparams.ram_size >> 20) - 256
                : 0);

    setenv("memsize", memenv, 1);
    setenv("highmemsize", highmemenv, 1);

    ret = ((ret + 32) & ~31);

    boot_params_buf = (void *)(params_buf + ret);
    boot_params_p = boot_params_buf + align(sizeof(struct boot_params));
    init_boot_param(boot_params_buf);
    rom_add_blob_fixed("params", params_buf, params_size, BOOTPARAM_PHYADDR);
    loaderparams.a0 = 2;
    loaderparams.a1 = BOOTPARAM_ADDR;
    loaderparams.a2 = BOOTPARAM_ADDR + ret;

    return 0;
}

static uint64_t cpu_loongarch_virt_to_phys(void *opaque, uint64_t addr)
{
    return addr & 0x1fffffffll;
}

static void fw_cfg_add_kernel_info(FWCfgState *fw_cfg, uint64_t highram_size,
                                   uint64_t phyAddr_initrd)
{
    int64_t entry, kernel_low, kernel_high;
    long initrd_size = 0;
    uint64_t initrd_offset = 0;
    void *cmdline_buf;
    int ret = 0;

    ret = load_elf(loaderparams.kernel_filename, NULL,
                   cpu_loongarch_virt_to_phys, NULL, (uint64_t *)&entry,
                   (uint64_t *)&kernel_low, (uint64_t *)&kernel_high, NULL, 0,
                   EM_LOONGARCH, 1, 0);

    if (0 > ret) {
        error_report("kernel image load error");
        exit(1);
    }

    fw_cfg_add_i64(fw_cfg, FW_CFG_KERNEL_ENTRY, entry);

    if (loaderparams.initrd_filename) {
        initrd_size = get_image_size(loaderparams.initrd_filename);
        if (0 < initrd_size) {
            if (initrd_size > highram_size) {
                error_report("initrd size is too big, should below %ld MB",
                             highram_size / MiB);
                /*prevent write io memory address space*/
                exit(1);
            }
            initrd_offset =
                (phyAddr_initrd - initrd_size) & TARGET_REALPAGE_MASK;
            initrd_size = load_image_targphys(
                loaderparams.initrd_filename, initrd_offset,
                loaderparams.ram_size - initrd_offset);
            fw_cfg_add_i64(fw_cfg, FW_CFG_INITRD_ADDR, initrd_offset);
            fw_cfg_add_i64(fw_cfg, FW_CFG_INITRD_SIZE, initrd_size);
        } else {
            error_report("initrd image size is error");
        }
    }

    cmdline_buf = g_malloc0(COMMAND_LINE_SIZE);
    if (initrd_size > 0)
        ret = (1 + snprintf(cmdline_buf, COMMAND_LINE_SIZE,
                            "rd_start=0x%llx rd_size=%li %s",
                            PHYS_TO_VIRT(initrd_offset), initrd_size,
                            loaderparams.kernel_cmdline));
    else
        ret = (1 + snprintf(cmdline_buf, COMMAND_LINE_SIZE, "%s",
                            loaderparams.kernel_cmdline));

    fw_cfg_add_i32(fw_cfg, FW_CFG_CMDLINE_SIZE, ret);
    fw_cfg_add_string(fw_cfg, FW_CFG_CMDLINE_DATA, (const char *)cmdline_buf);

    return;
}

static int64_t load_kernel(void)
{
    int64_t entry, kernel_low, kernel_high;
    long initrd_size = 0;
    ram_addr_t initrd_offset = 0;

    load_elf(loaderparams.kernel_filename, NULL, cpu_loongarch_virt_to_phys,
             NULL, (uint64_t *)&entry, (uint64_t *)&kernel_low,
             (uint64_t *)&kernel_high, NULL, 0, EM_LOONGARCH, 1, 0);

    if (loaderparams.initrd_filename) {
        initrd_size = get_image_size(loaderparams.initrd_filename);

        if (initrd_size > 0) {
            initrd_offset = (kernel_high * 4 + ~TARGET_REALPAGE_MASK) &
                            TARGET_REALPAGE_MASK;
            initrd_size = load_image_targphys(
                loaderparams.initrd_filename, initrd_offset,
                loaderparams.ram_size - initrd_offset);
        }
    }
    set_bootparam_uefi(initrd_offset, initrd_size);

    return entry;
}

static void main_cpu_reset(void *opaque)
{
    ResetData *s = (ResetData *)opaque;
    CPULOONGARCHState *env = &s->cpu->env;

    cpu_reset(CPU(s->cpu));
    env->active_tc.PC = s->vector;
    env->active_tc.gpr[4] = loaderparams.a0;
    env->active_tc.gpr[5] = loaderparams.a1;
    env->active_tc.gpr[6] = loaderparams.a2;
}

void slave_cpu_reset(void *opaque)
{
    ResetData *s = (ResetData *)opaque;

    cpu_reset(CPU(s->cpu));
}

/* KVM_IRQ_LINE irq field index values */
#define KVM_LOONGARCH_IRQ_TYPE_SHIFT 24
#define KVM_LOONGARCH_IRQ_TYPE_MASK 0xff
#define KVM_LOONGARCH_IRQ_VCPU_SHIFT 16
#define KVM_LOONGARCH_IRQ_VCPU_MASK 0xff
#define KVM_LOONGARCH_IRQ_NUM_SHIFT 0
#define KVM_LOONGARCH_IRQ_NUM_MASK 0xffff

/* irq_type field */
#define KVM_LOONGARCH_IRQ_TYPE_CPU_IP 0
#define KVM_LOONGARCH_IRQ_TYPE_CPU_IO 1
#define KVM_LOONGARCH_IRQ_TYPE_HT 2
#define KVM_LOONGARCH_IRQ_TYPE_MSI 3
#define KVM_LOONGARCH_IRQ_TYPE_IOAPIC 4

static void legacy_set_irq(void *opaque, int irq, int level)
{
    qemu_irq *pic = opaque;

    qemu_set_irq(pic[irq], level);
}

typedef struct ls3a_intctlstate {
    uint8_t nodecounter_reg[0x100];
    uint8_t pm_reg[0x100];
    uint8_t msi_reg[0x8];
    CPULOONGARCHState **env;
    DeviceState *apicdev;
    qemu_irq *ioapic_irq;
#ifdef CONFIG_KVM
    struct loongarch_kvm_irqchip chip;
#endif
} ls3a_intctlstate;

typedef struct ls3a_func_args {
    ls3a_intctlstate *state;
    uint64_t base;
    uint32_t mask;
    uint8_t *mem;
} ls3a_func_args;

static uint64_t ls3a_msi_mem_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void ls3a_msi_mem_write(void *opaque, hwaddr addr, uint64_t val,
                               unsigned size)
{
    struct kvm_msi msi;
    apicState *apic;

    apic = (apicState *)opaque;
    msi.address_lo = 0;
    msi.address_hi = 0;
    msi.data = val & 0xff;
    msi.flags = 0;
    memset(msi.pad, 0, sizeof(msi.pad));

    if (kvm_irqchip_in_kernel()) {
        kvm_vm_ioctl(kvm_state, KVM_SIGNAL_MSI, &msi);
    } else {
        qemu_set_irq(apic->irq[msi.data], 1);
    }
}

static const MemoryRegionOps ls3a_msi_ops = {
    .read = ls3a_msi_mem_read,
    .write = ls3a_msi_mem_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_ls3a_msi = {
    .name = "ls3a-msi",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields =
        (VMStateField[]){ VMSTATE_UINT8_ARRAY(msi_reg, ls3a_intctlstate, 0x8),
                          VMSTATE_END_OF_LIST() }
};

static void ioapic_handler(void *opaque, int irq, int level)
{
    apicState *apic;
    int kvm_irq;

    apic = (apicState *)opaque;

    if (kvm_irqchip_in_kernel()) {
        kvm_irq =
            (KVM_LOONGARCH_IRQ_TYPE_IOAPIC << KVM_LOONGARCH_IRQ_TYPE_SHIFT) |
            (0 << KVM_LOONGARCH_IRQ_VCPU_SHIFT) | irq;
        kvm_set_irq(kvm_state, kvm_irq, !!level);
    } else {
        qemu_set_irq(apic->irq[irq], level);
    }
}

static void *ls3a_intctl_init(MachineState *machine, CPULOONGARCHState *env[])
{
    qemu_irq *irqhandler;
    ls3a_intctlstate *s;
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);
    LoongarchMachineClass *mc = LoongarchMACHINE_GET_CLASS(lsms);
    DeviceState *dev;
    SysBusDevice *busdev;
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *iomem = NULL;
#ifdef CONFIG_KVM
    int i;
#endif

    s = g_malloc0(sizeof(ls3a_intctlstate));

    if (!s) {
        return NULL;
    }

    /*Add MSI mmio memory*/
    iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, NULL, &ls3a_msi_ops, lsms->apic, "ls3a_msi",
                          0x8);
    memory_region_add_subregion(address_space_mem, MSI_ADDR_LOW, iomem);
    vmstate_register(NULL, 0, &vmstate_ls3a_msi, s);

    s->env = env;

    if (!strcmp(mc->bridge_name, "ls7a")) {
        if (lsms->apic_xrupt_override) {
            DPRINTF("irqchip in kernel %d\n", kvm_irqchip_in_kernel());
#ifdef CONFIG_KVM
            if (kvm_has_gsi_routing()) {
                for (i = 0; i < 32; ++i) {
                    kvm_irqchip_add_irq_route(kvm_state, i, 0, i);
                }
                kvm_gsi_routing_allowed = true;
            }
            kvm_msi_via_irqfd_allowed = kvm_irqfds_enabled();
#endif
        }

        irqhandler = qemu_allocate_irqs(ioapic_handler, lsms->apic, 64);
        dev = qdev_new("ioapic");
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_realize_and_unref(busdev, &error_fatal);
        sysbus_mmio_map(busdev, 0, mc->ls7a_ioapic_reg_base);
        s->ioapic_irq = irqhandler;
        s->apicdev = dev;
        return s->ioapic_irq;
    }
    return NULL;
}

/* Network support */
static void network_init(PCIBus *pci_bus)
{
    int i;

    for (i = 0; i < nb_nics; i++) {
        NICInfo *nd = &nd_table[i];

        if (!nd->model) {
            nd->model = g_strdup("virtio-net-pci");
        }

        pci_nic_init_nofail(nd, pci_bus, nd->model, NULL);
    }
}

void loongarch_cpu_destroy(MachineState *machine, LOONGARCHCPU *cpu)
{
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);
    unsigned int id;
    int smp_cpus = machine->smp.cpus;
    id = cpu->id;
    qemu_unregister_reset(slave_cpu_reset, lsms->reset_info[id]);
    g_free(lsms->reset_info[id]);
    lsms->reset_info[id] = NULL;

    smp_cpus -= 1;
    if (lsms->fw_cfg) {
        fw_cfg_modify_i16(lsms->fw_cfg, FW_CFG_NB_CPUS, (uint16_t)smp_cpus);
    }

    qemu_del_vm_change_state_handler(cpu->cpuStateEntry);
}

LOONGARCHCPU *loongarch_cpu_create(MachineState *machine, LOONGARCHCPU *cpu,
                                   Error **errp)
{
    CPULOONGARCHState *env;
    unsigned int id;
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);
    int smp_cpus = machine->smp.cpus;
    id = cpu->id;
    env = &cpu->env;
    cpu_states[id] = env;
    env->CSR_TMID |= id;

    lsms = LoongarchMACHINE(machine);
    lsms->reset_info[id] = g_malloc0(sizeof(ResetData));
    lsms->reset_info[id]->cpu = cpu;
    lsms->reset_info[id]->vector = env->active_tc.PC;
    qemu_register_reset(slave_cpu_reset, lsms->reset_info[id]);

    /* Init CPU internal devices */
    cpu_init_irq(cpu);
    cpu_loongarch_clock_init(cpu);

    smp_cpus += 1;
    if (lsms->fw_cfg) {
        fw_cfg_modify_i16(lsms->fw_cfg, FW_CFG_NB_CPUS, (uint16_t)smp_cpus);
    }
    cpu_init_ipi(lsms, env->irq[12], id);
    cpu_init_apic(lsms, env, id);

    return cpu;
}

static void fw_cfg_boot_set(void *opaque, const char *boot_device,
                            Error **errp)
{
    fw_cfg_modify_i16(opaque, FW_CFG_BOOT_DEVICE, boot_device[0]);
}

static FWCfgState *loongarch_fw_cfg_init(ram_addr_t ram_size,
                                         LoongarchMachineState *lsms)
{
    FWCfgState *fw_cfg;
    uint64_t *numa_fw_cfg;
    int i;
    const CPUArchIdList *cpus;
    MachineClass *mc = MACHINE_GET_CLASS(lsms);
    MachineState *ms = MACHINE(OBJECT(lsms));
    int max_cpus = ms->smp.max_cpus;
    int smp_cpus = ms->smp.cpus;
    int nb_numa_nodes = ms->numa_state->num_nodes;
    NodeInfo *numa_info = ms->numa_state->nodes;

    fw_cfg = fw_cfg_init_mem_wide(FW_CFG_ADDR + 8, FW_CFG_ADDR, 8, 0, NULL);
    fw_cfg_add_i16(fw_cfg, FW_CFG_MAX_CPUS, (uint16_t)max_cpus);
    fw_cfg_add_i64(fw_cfg, FW_CFG_RAM_SIZE, (uint64_t)ram_size);
    fw_cfg_add_i16(fw_cfg, FW_CFG_NB_CPUS, (uint16_t)smp_cpus);

    /*
     * allocate memory for the NUMA channel: one (64bit) word for the number
     * of nodes, one word for each VCPU->node and one word for each node to
     * hold the amount of memory.
     */
    numa_fw_cfg = g_new0(uint64_t, 1 + max_cpus + nb_numa_nodes);
    numa_fw_cfg[0] = cpu_to_le64(nb_numa_nodes);
    cpus = mc->possible_cpu_arch_ids(MACHINE(lsms));
    for (i = 0; i < cpus->len; i++) {
        unsigned int apic_id = cpus->cpus[i].arch_id;
        assert(apic_id < max_cpus);
        numa_fw_cfg[apic_id + 1] = cpu_to_le64(cpus->cpus[i].props.node_id);
    }
    for (i = 0; i < nb_numa_nodes; i++) {
        numa_fw_cfg[max_cpus + 1 + i] = cpu_to_le64(numa_info[i].node_mem);
    }
    fw_cfg_add_bytes(fw_cfg, FW_CFG_NUMA, numa_fw_cfg,
                     (1 + max_cpus + nb_numa_nodes) * sizeof(*numa_fw_cfg));

    qemu_register_boot_set(fw_cfg_boot_set, fw_cfg);
    return fw_cfg;
}

static void loongarch_build_smbios(LoongarchMachineState *lsms)
{
    LoongarchMachineClass *lsmc = LoongarchMACHINE_GET_CLASS(lsms);
    MachineState *ms = MACHINE(OBJECT(lsms));
    uint8_t *smbios_tables, *smbios_anchor;
    size_t smbios_tables_len, smbios_anchor_len;
    const char *product = "QEMU Virtual Machine";

    if (!lsms->fw_cfg) {
        return;
    }

    if (kvm_enabled()) {
        if (strstr(lsmc->cpu_name, "5000")) {
            product = "KVM";
        }
    } else {
        product = "Loongarch-3A5K-7A1000-TCG";
    }

    smbios_set_defaults("Loongson", product, lsmc->cpu_name, false, true,
                        SMBIOS_ENTRY_POINT_30);

    smbios_get_tables(ms, NULL, 0, &smbios_tables, &smbios_tables_len,
                      &smbios_anchor, &smbios_anchor_len, &error_fatal);

    if (smbios_anchor) {
        fw_cfg_add_file(lsms->fw_cfg, "etc/smbios/smbios-tables",
                        smbios_tables, smbios_tables_len);
        fw_cfg_add_file(lsms->fw_cfg, "etc/smbios/smbios-anchor",
                        smbios_anchor, smbios_anchor_len);
    }
}

static void loongarch_machine_done(Notifier *notifier, void *data)
{
    LoongarchMachineState *lsms =
        container_of(notifier, LoongarchMachineState, machine_done);

    platform_bus_add_all_fdt_nodes(
        lsms->fdt, NULL, VIRT_PLATFORM_BUS_BASEADDRESS, VIRT_PLATFORM_BUS_SIZE,
        VIRT_PLATFORM_BUS_IRQ);

    qemu_fdt_dumpdtb(lsms->fdt, lsms->fdt_size);
    /* load fdt */
    MemoryRegion *fdt_rom = g_new(MemoryRegion, 1);
    memory_region_init_rom(fdt_rom, NULL, "fdt", LS_FDT_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), LS_FDT_BASE, fdt_rom);
    rom_add_blob_fixed("fdt", lsms->fdt, lsms->fdt_size, LS_FDT_BASE);

    loongarch_acpi_setup();
    loongarch_build_smbios(lsms);
}

#ifdef CONFIG_TCG
#define FEATURE_REG 0x1fe00008
#define VENDOR_REG 0x1fe00010
#define CPUNAME_REG 0x1fe00020
#define OTHER_FUNC_REG 0x1fe00420
#define _str(x) #x
#define str(x) _str(x)
#define SIMPLE_OPS(ADDR, SIZE)                                                \
    ({                                                                        \
        MemoryRegion *iomem = g_new(MemoryRegion, 1);                         \
        memory_region_init_io(iomem, NULL, &loongarch_qemu_ops, (void *)ADDR, \
                              str(ADDR), SIZE);                               \
        memory_region_add_subregion_overlap(address_space_mem, ADDR, iomem,   \
                                            1);                               \
    })

static int reg180;

static void loongarch_qemu_write(void *opaque, hwaddr addr, uint64_t val,
                                 unsigned size)
{
    addr = ((hwaddr)(long)opaque) + addr;
    addr = addr & 0xffffffff;
    switch (addr) {
    case 0x1fe00180:
        reg180 = val;
        break;
    }
}

static uint64_t loongarch_qemu_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t feature = 0UL;
    addr = ((hwaddr)(long)opaque) + addr;
    addr = addr & 0xffffffff;
    switch (addr) {
    case 0x1fe00180:
        return reg180;
    case 0x1001041c:
        return 0xa800;
    case FEATURE_REG:
        feature |= 1UL << 2 | 1UL << 3 | 1UL << 4 | 1UL << 11;
        return feature;
    case VENDOR_REG:
        return *(uint64_t *)"Loongson-3A5000";
    case CPUNAME_REG:
        return *(uint64_t *)"3A5000";
    case 0x10013ffc:
        return 0x80;
    }
    return 0;
}

static const MemoryRegionOps loongarch_qemu_ops = {
    .read = loongarch_qemu_read,
    .write = loongarch_qemu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
};
#endif

static void loongarch_system_flash_cleanup_unused(LoongarchMachineState *lsms)
{
    char *prop_name;
    int i;
    Object *dev_obj;

    for (i = 0; i < ARRAY_SIZE(lsms->flash); i++) {
        dev_obj = OBJECT(lsms->flash[i]);
        if (!object_property_get_bool(dev_obj, "realized", &error_abort)) {
            prop_name = g_strdup_printf("pflash%d", i);
            object_property_del(OBJECT(lsms), prop_name);
            g_free(prop_name);
            object_unparent(dev_obj);
            lsms->flash[i] = NULL;
        }
    }
}

static bool loongarch_system_flash_init(LoongarchMachineState *lsms)
{
    int i = 0;
    int64_t size = 0;
    PFlashCFI01 *pflash = NULL;
    BlockBackend *pflash_blk;

    for (i = 0; i < ARRAY_SIZE(lsms->flash); i++) {
        pflash_blk = NULL;
        pflash = NULL;

        pflash = lsms->flash[i];
        pflash_cfi01_legacy_drive(pflash, drive_get(IF_PFLASH, 0, i));

        pflash_blk = pflash_cfi01_get_blk(pflash);
        /*The pflash0 must be exist, or not support boot by pflash*/
        if (pflash_blk == NULL) {
            if (i == 0) {
                return false;
            } else {
                break;
            }
        }

        size = blk_getlength(pflash_blk);
        if (size == 0 || size % FLASH_SECTOR_SIZE != 0) {
            error_report("system firmware block device %s has invalid size "
                         "%" PRId64,
                         blk_name(pflash_blk), size);
            error_report("its size must be a non-zero multiple of 0x%x",
                         FLASH_SECTOR_SIZE);
            exit(1);
        }
        qdev_prop_set_uint32(DEVICE(pflash), "num-blocks",
                             size / FLASH_SECTOR_SIZE);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(pflash), &error_fatal);
        if (i == 0) {
            sysbus_mmio_map(SYS_BUS_DEVICE(pflash), 0, LS_BIOS_BASE);
        } else {
            sysbus_mmio_map_overlap(SYS_BUS_DEVICE(pflash), 0,
                                    LS_BIOS_VAR_BASE, 1);
        }
    }

    return true;
}

static void ls3a5k_bios_init(LoongarchMachineState *lsms, ram_addr_t ram_size,
                             uint64_t highram_size, uint64_t phyAddr_initrd,
                             const char *kernel_filename,
                             const char *kernel_cmdline,
                             const char *initrd_filename)
{
    MemoryRegion *bios;
    bool fw_cfg_used = false;
    LoongarchMachineClass *lsmc = LoongarchMACHINE_GET_CLASS(lsms);
    char *filename;
    int bios_size;
    const char *bios_name;

    bios_name = MACHINE(lsms)->firmware;
    if (kernel_filename) {
        loaderparams.ram_size = ram_size;
        loaderparams.kernel_filename = kernel_filename;
        loaderparams.kernel_cmdline = kernel_cmdline;
        loaderparams.initrd_filename = initrd_filename;
    }

    if (loongarch_system_flash_init(lsms)) {
        fw_cfg_used = true;
    } else {
        bios = g_new(MemoryRegion, 1);
        memory_region_init_ram(bios, NULL, "loongarch.bios", LS_BIOS_SIZE,
                               &error_fatal);
        memory_region_set_readonly(bios, true);
        memory_region_add_subregion(get_system_memory(), LS_BIOS_BASE, bios);

        /* BIOS load */
        if (bios_name) {
            if (access(bios_name, R_OK) == 0) {
                load_image_targphys(bios_name, LS_BIOS_BASE, LS_BIOS_SIZE);
            } else {
                filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
                load_image_targphys(filename, LS_BIOS_BASE, LS_BIOS_SIZE);
                g_free(filename);
            }
            fw_cfg_used = true;
        } else {
            if (strstr(lsmc->cpu_name, "5000")) {
                bios_size = sizeof(ls3a5k_aui_boot_code);
                rom_add_blob_fixed("bios", ls3a5k_aui_boot_code, bios_size,
                                   LS_BIOS_BASE);
            }

            if (kernel_filename) {
                lsms->reset_info[0]->vector = load_kernel();
            }
        }
    }

    loongarch_system_flash_cleanup_unused(lsms);

    if (fw_cfg_used) {
        lsms->fw_cfg = loongarch_fw_cfg_init(ram_size, lsms);
        rom_set_fw(lsms->fw_cfg);
        fw_conf_init(ram_size);
        rom_add_blob_fixed("fw_conf", (void *)&fw_config, sizeof(fw_config),
                           FW_CONF_ADDR);

        if (kernel_filename) {
            fw_cfg_add_kernel_info(lsms->fw_cfg, highram_size, phyAddr_initrd);
        }
    }

    if (lsms->fw_cfg != NULL) {
        fw_cfg_add_file(lsms->fw_cfg, "etc/memmap", la_memmap_table,
                        sizeof(struct la_memmap_entry) * (la_memmap_entries));
    }

    return;
}

static void create_fdt(LoongarchMachineState *lsms)
{
    lsms->fdt = create_device_tree(&lsms->fdt_size);
    if (!lsms->fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    /* Header */
    qemu_fdt_setprop_string(lsms->fdt, "/", "compatible",
                            "linux,dummy-loongson3");
    qemu_fdt_setprop_cell(lsms->fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_cell(lsms->fdt, "/", "#size-cells", 0x2);
}

static void fdt_add_cpu_nodes(const LoongarchMachineState *lsms)
{
    int num;
    const MachineState *ms = MACHINE(lsms);
    int smp_cpus = ms->smp.cpus;

    qemu_fdt_add_subnode(lsms->fdt, "/cpus");
    qemu_fdt_setprop_cell(lsms->fdt, "/cpus", "#address-cells", 0x1);
    qemu_fdt_setprop_cell(lsms->fdt, "/cpus", "#size-cells", 0x0);

    /* cpu nodes */
    for (num = smp_cpus - 1; num >= 0; num--) {
        char *nodename = g_strdup_printf("/cpus/cpu@%d", num);
        LOONGARCHCPU *cpu = LOONGARCH_CPU(qemu_get_cpu(num));

        qemu_fdt_add_subnode(lsms->fdt, nodename);
        qemu_fdt_setprop_string(lsms->fdt, nodename, "device_type", "cpu");
        qemu_fdt_setprop_string(lsms->fdt, nodename, "compatible",
                                cpu->dtb_compatible);
        qemu_fdt_setprop_cell(lsms->fdt, nodename, "reg", num);
        qemu_fdt_setprop_cell(lsms->fdt, nodename, "phandle",
                              qemu_fdt_alloc_phandle(lsms->fdt));
        g_free(nodename);
    }

    /*cpu map */
    qemu_fdt_add_subnode(lsms->fdt, "/cpus/cpu-map");

    for (num = smp_cpus - 1; num >= 0; num--) {
        char *cpu_path = g_strdup_printf("/cpus/cpu@%d", num);
        char *map_path;

        if (ms->smp.threads > 1) {
            map_path =
                g_strdup_printf("/cpus/cpu-map/socket%d/core%d/thread%d",
                                num / (ms->smp.cores * ms->smp.threads),
                                (num / ms->smp.threads) % ms->smp.cores,
                                num % ms->smp.threads);
        } else {
            map_path =
                g_strdup_printf("/cpus/cpu-map/socket%d/core%d",
                                num / ms->smp.cores, num % ms->smp.cores);
        }
        qemu_fdt_add_path(lsms->fdt, map_path);
        qemu_fdt_setprop_phandle(lsms->fdt, map_path, "cpu", cpu_path);

        g_free(map_path);
        g_free(cpu_path);
    }
}

static void fdt_add_fw_cfg_node(const LoongarchMachineState *lsms)
{
    char *nodename;
    hwaddr base = FW_CFG_ADDR;

    nodename = g_strdup_printf("/fw_cfg@%" PRIx64, base);
    qemu_fdt_add_subnode(lsms->fdt, nodename);
    qemu_fdt_setprop_string(lsms->fdt, nodename, "compatible",
                            "qemu,fw-cfg-mmio");
    qemu_fdt_setprop_sized_cells(lsms->fdt, nodename, "reg", 2, base, 2, 0x8);
    qemu_fdt_setprop(lsms->fdt, nodename, "dma-coherent", NULL, 0);
    g_free(nodename);
}

static void fdt_add_pcie_node(const LoongarchMachineState *lsms)
{
    char *nodename;
    hwaddr base_mmio = PCIE_MEMORY_BASE;
    hwaddr size_mmio = PCIE_MEMORY_SIZE;
    hwaddr base_pio = LS3A5K_ISA_IO_BASE;
    hwaddr size_pio = LS_ISA_IO_SIZE;
    hwaddr base_pcie = LS_PCIECFG_BASE;
    hwaddr size_pcie = LS_PCIECFG_SIZE;
    hwaddr base = base_pcie;

    nodename = g_strdup_printf("/pcie@%" PRIx64, base);
    qemu_fdt_add_subnode(lsms->fdt, nodename);
    qemu_fdt_setprop_string(lsms->fdt, nodename, "compatible",
                            "pci-host-ecam-generic");
    qemu_fdt_setprop_string(lsms->fdt, nodename, "device_type", "pci");
    qemu_fdt_setprop_cell(lsms->fdt, nodename, "#address-cells", 3);
    qemu_fdt_setprop_cell(lsms->fdt, nodename, "#size-cells", 2);
    qemu_fdt_setprop_cell(lsms->fdt, nodename, "linux,pci-domain", 0);
    qemu_fdt_setprop_cells(lsms->fdt, nodename, "bus-range", 0,
                           PCIE_MMCFG_BUS(LS_PCIECFG_SIZE - 1));
    qemu_fdt_setprop(lsms->fdt, nodename, "dma-coherent", NULL, 0);
    qemu_fdt_setprop_sized_cells(lsms->fdt, nodename, "reg", 2, base_pcie, 2,
                                 size_pcie);
    qemu_fdt_setprop_sized_cells(lsms->fdt, nodename, "ranges", 1,
                                 FDT_PCI_RANGE_IOPORT, 2, 0, 2, base_pio, 2,
                                 size_pio, 1, FDT_PCI_RANGE_MMIO, 2, base_mmio,
                                 2, base_mmio, 2, size_mmio);
    g_free(nodename);
}

static void create_platform_bus(LoongarchMachineState *s, qemu_irq *pic)
{
    DeviceState *dev;
    SysBusDevice *sysbus;
    int i;
    MemoryRegion *sysmem = get_system_memory();

    dev = qdev_new(TYPE_PLATFORM_BUS_DEVICE);
    dev->id = g_strdup(TYPE_PLATFORM_BUS_DEVICE);
    qdev_prop_set_uint32(dev, "num_irqs", VIRT_PLATFORM_BUS_NUM_IRQS);
    qdev_prop_set_uint32(dev, "mmio_size", VIRT_PLATFORM_BUS_SIZE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    s->platform_bus_dev = dev;

    sysbus = SYS_BUS_DEVICE(dev);
    for (i = 0; i < VIRT_PLATFORM_BUS_NUM_IRQS; i++) {
        int irq = VIRT_PLATFORM_BUS_IRQ + i;
        sysbus_connect_irq(sysbus, i, pic[irq - LOONGARCH_PCH_IRQ_BASE]);
    }

    memory_region_add_subregion(sysmem, VIRT_PLATFORM_BUS_BASEADDRESS,
                                sysbus_mmio_get_region(sysbus, 0));
}

static void ls3a5k_init(MachineState *args)
{
    int i;
    const char *cpu_model = args->cpu_type;
    const char *kernel_filename = args->kernel_filename;
    const char *kernel_cmdline = args->kernel_cmdline;
    const char *initrd_filename = args->initrd_filename;

    ram_addr_t ram_size = args->ram_size;
    MemoryRegion *address_space_mem = get_system_memory();
    ram_addr_t offset = 0;
    MemoryRegion *isa_io = g_new(MemoryRegion, 1);
    MemoryRegion *isa_mem = g_new(MemoryRegion, 1);
    MachineState *machine = args;
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);
    LoongarchMachineClass *lsmc = LoongarchMACHINE_GET_CLASS(lsms);
    int smp_cpus = machine->smp.cpus;
    int nb_numa_nodes = machine->numa_state->num_nodes;
    NodeInfo *numa_info = machine->numa_state->nodes;
    LOONGARCHCPU *cpu;
    CPULOONGARCHState *env;
    qemu_irq *ls7a_apic = NULL;
    qemu_irq *pirq = NULL;
    PCIBus *pci_bus = NULL;
    char *ramName = NULL;
    uint64_t lowram_size = 0, highram_size = 0, phyAddr = 0, memmap_size = 0,
             highram_end_addr = 0;

    CPUArchIdList *possible_cpus;
    if (strstr(lsmc->cpu_name, "5000")) {
        if (strcmp(cpu_model, LOONGARCH_CPU_TYPE_NAME("Loongson-3A5000")) &&
            strcmp(cpu_model, LOONGARCH_CPU_TYPE_NAME("host"))) {
            error_report("machine type %s does not match cpu type %s",
                         lsmc->cpu_name, cpu_model);
            exit(1);
        }
        if (kvm_enabled()) {
            kvm_vm_ioctl(kvm_state, KVM_LARCH_SET_CPUCFG, ls3a5k_cpucfgs);
        }
    }

    create_fdt(lsms);

    DPRINTF("isa 0x%lx\n", lsmc->isa_io_base);
    DPRINTF("cpu_name %s bridge_name %s\n", lsmc->cpu_name, lsmc->bridge_name);

    /* init CPUs */
    mc->possible_cpu_arch_ids(machine);
    possible_cpus = machine->possible_cpus;

    for (i = 0; i < smp_cpus; i++) {
        Object *obj = NULL;
        Error *local_err = NULL;

        obj = object_new(possible_cpus->cpus[i].type);

        object_property_set_uint(obj, "id", possible_cpus->cpus[i].arch_id,
                                 &local_err);
        object_property_set_bool(obj, "realized", true, &local_err);

        object_unref(obj);
        error_propagate(&error_fatal, local_err);

        cpu = LOONGARCH_CPU(CPU(obj));
        if (cpu == NULL) {
            fprintf(stderr, "Unable to find CPU definition\n");
            exit(1);
        }

        env = &cpu->env;
        cpu_states[i] = env;
        env->CSR_TMID |= i;

        lsms->reset_info[i] = g_malloc0(sizeof(ResetData));
        lsms->reset_info[i]->cpu = cpu;
        lsms->reset_info[i]->vector = env->active_tc.PC;
        if (i == 0) {
            qemu_register_reset(main_cpu_reset, lsms->reset_info[i]);
        } else {
            qemu_register_reset(slave_cpu_reset, lsms->reset_info[i]);
        }

        /* Init CPU internal devices */
        cpu_init_irq(cpu);
        cpu_loongarch_clock_init(cpu);
        cpu_init_ipi(lsms, env->irq[12], i);
        cpu_init_apic(lsms, env, i);
    }

    lsms->hotpluged_cpu_num = 0;
    fdt_add_cpu_nodes(lsms);
    env = cpu_states[0];

    /* node0 mem*/
    phyAddr = (uint64_t)0;
    MemoryRegion *lowmem = g_new(MemoryRegion, 1);
    ramName = g_strdup_printf("loongarch_ls3a.node%d.lowram", 0);

    lowram_size = MIN(ram_size, 256 * 0x100000);
    memory_region_init_alias(lowmem, NULL, ramName, machine->ram, 0,
                             lowram_size);
    memory_region_add_subregion(address_space_mem, phyAddr, lowmem);

    offset += lowram_size;
    if (nb_numa_nodes > 0) {
        highram_size = numa_info[0].node_mem - 256 * MiB;
        if (numa_info[0].node_mem > GiB) {
            memmap_size = numa_info[0].node_mem - GiB;
            la_memmap_add_entry(0xc0000000ULL, memmap_size, SYSTEM_RAM);
        }
    } else {
        highram_size = ram_size - 256 * MiB;
        if (ram_size > GiB) {
            memmap_size = ram_size - GiB;
            la_memmap_add_entry(0xc0000000ULL, memmap_size, SYSTEM_RAM);
        }
    }

    phyAddr = (uint64_t)0x90000000;
    MemoryRegion *highmem = g_new(MemoryRegion, 1);
    ramName = g_strdup_printf("loongarch_ls3a.node%d.highram", 0);
    memory_region_init_alias(highmem, NULL, ramName, machine->ram, offset,
                             highram_size);
    memory_region_add_subregion(address_space_mem, phyAddr, highmem);
    offset += highram_size;
    phyAddr += highram_size;

    /* initrd address use high mem from high to low */
    highram_end_addr = phyAddr;
    /* node1~ nodemax */
    for (i = 1; i < nb_numa_nodes; i++) {
        MemoryRegion *nodemem = g_new(MemoryRegion, 1);
        ramName = g_strdup_printf("loongarch_ls3a.node%d.ram", i);
        memory_region_init_alias(nodemem, NULL, ramName, machine->ram, offset,
                                 numa_info[i].node_mem);
        memory_region_add_subregion(address_space_mem, phyAddr, nodemem);
        la_memmap_add_entry(phyAddr, numa_info[i].node_mem, SYSTEM_RAM);
        offset += numa_info[i].node_mem;
        phyAddr += numa_info[i].node_mem;
    }

    fdt_add_fw_cfg_node(lsms);
    ls3a5k_bios_init(lsms, ram_size, highram_size, highram_end_addr,
                     kernel_filename, kernel_cmdline, initrd_filename);

    lsms->machine_done.notify = loongarch_machine_done;
    qemu_add_machine_init_done_notifier(&lsms->machine_done);
    /*vmstate_register_ram_global(bios);*/

    /* initialize hotplug memory address space */
    lsms->hotplug_memory_size = 0;

    /* always allocate the device memory information */
    machine->device_memory = g_malloc0(sizeof(*machine->device_memory));
    if (machine->ram_size < machine->maxram_size) {
        int max_memslots;

        lsms->hotplug_memory_size = machine->maxram_size - machine->ram_size;
        /*
         * Limit the number of hotpluggable memory slots to half the number
         * slots that KVM supports, leaving the other half for PCI and other
         * devices. However ensure that number of slots doesn't drop below 32.
         */
        max_memslots = LOONGARCH_MAX_RAM_SLOTS;
        if (kvm_enabled()) {
            max_memslots = kvm_get_max_memslots() / 2;
        }

        if (machine->ram_slots == 0)
            machine->ram_slots =
                lsms->hotplug_memory_size / LOONGARCH_HOTPLUG_MEM_ALIGN;

        if (machine->ram_slots > max_memslots) {
            error_report("Specified number of memory slots %" PRIu64
                         " exceeds max supported %d",
                         machine->ram_slots, max_memslots);
            exit(1);
        }

        lsms->ram_slots = machine->ram_slots;

        machine->device_memory->base = get_hotplug_membase(machine->ram_size);
        memory_region_init(&machine->device_memory->mr, OBJECT(lsms),
                           "device-memory", lsms->hotplug_memory_size);
        memory_region_add_subregion(get_system_memory(),
                                    machine->device_memory->base,
                                    &machine->device_memory->mr);
    }

    memory_region_init_alias(isa_io, NULL, "isa-io", get_system_io(), 0,
                             LS_ISA_IO_SIZE);
    memory_region_init(isa_mem, NULL, "isa-mem", PCIE_MEMORY_SIZE);
    memory_region_add_subregion(get_system_memory(), lsmc->isa_io_base,
                                isa_io);
    memory_region_add_subregion(get_system_memory(), PCIE_MEMORY_BASE,
                                isa_mem);

    if (!strcmp(lsmc->bridge_name, "ls7a")) {
        /*Initialize the 7A IO interrupt subsystem*/
        DeviceState *ls7a_dev;
        lsms->apic_xrupt_override = kvm_irqchip_in_kernel();
        ls7a_apic = ls3a_intctl_init(machine, cpu_states);
        if (!ls7a_apic) {
            perror("Init 7A APIC failed\n");
            exit(1);
        }
        pci_bus = ls7a_init(machine, ls7a_apic, &ls7a_dev);

        object_property_add_link(
            OBJECT(machine), LOONGARCH_MACHINE_ACPI_DEVICE_PROP,
            TYPE_HOTPLUG_HANDLER, (Object **)&lsms->acpi_dev,
            object_property_allow_set_link, OBJ_PROP_LINK_STRONG);
        object_property_set_link(OBJECT(machine),
                                 LOONGARCH_MACHINE_ACPI_DEVICE_PROP,
                                 OBJECT(ls7a_dev), &error_abort);

        create_platform_bus(lsms, ls7a_apic);

#ifdef CONFIG_KVM
        if (kvm_enabled()) {
            kvm_direct_msi_allowed =
                (kvm_check_extension(kvm_state, KVM_CAP_SIGNAL_MSI) > 0);
        } else {
            kvm_direct_msi_allowed = 0;
        }
        msi_nonbroken = kvm_direct_msi_allowed;
#else
        msi_nonbroken = true;
#endif
        sysbus_create_simple("ls7a_rtc", LS7A_RTC_REG_BASE,
                             ls7a_apic[LS7A_RTC_IRQ - LOONGARCH_PCH_IRQ_BASE]);
    }

    /*Initialize the CPU serial device*/

    if (serial_hd(0)) {
        pirq = qemu_allocate_irqs(
            legacy_set_irq,
            ls7a_apic + (LS7A_UART_IRQ - LOONGARCH_PCH_IRQ_BASE), 1);
        serial_mm_init(address_space_mem, LS7A_UART_BASE, 0, pirq[0], 115200,
                       serial_hd(0), DEVICE_NATIVE_ENDIAN);
    }

    /*network card*/
    network_init(pci_bus);
    /* VGA setup.  Don't bother loading the bios.  */
    pci_vga_init(pci_bus);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(qdev_new("iocsr")), &error_fatal);

#ifdef CONFIG_TCG
    int nb_nodes = (smp_cpus - 1) / 4;
    for (i = 0; i <= nb_nodes; i++) {
        uint64_t off = (uint64_t)i << 44;
        SIMPLE_OPS(((hwaddr)0x1fe00180 | off), 0x8);
        SIMPLE_OPS(((hwaddr)0x1fe0019c | off), 0x8);
        SIMPLE_OPS(((hwaddr)0x1fe001d0 | off), 0x8);
        SIMPLE_OPS(((hwaddr)FEATURE_REG | off), 0x8);
        SIMPLE_OPS(((hwaddr)VENDOR_REG | off), 0x8);
        SIMPLE_OPS(((hwaddr)CPUNAME_REG | off), 0x8);
        SIMPLE_OPS(((hwaddr)OTHER_FUNC_REG | off), 0x8);
    }

    SIMPLE_OPS(0x1001041c, 0x4);
    SIMPLE_OPS(0x10002000, 0x14);
    SIMPLE_OPS(0x10013ffc, 0x4);
#endif

    fdt_add_pcie_node(lsms);
}

static const CPUArchIdList *loongarch_possible_cpu_arch_ids(MachineState *ms)
{
    int i;
    int max_cpus = ms->smp.max_cpus;

    if (ms->possible_cpus) {
        /*
         * make sure that max_cpus hasn't changed since the first use, i.e.
         * -smp hasn't been parsed after it
         */
        assert(ms->possible_cpus->len == max_cpus);
        return ms->possible_cpus;
    }

    ms->possible_cpus =
        g_malloc0(sizeof(CPUArchIdList) + sizeof(CPUArchId) * max_cpus);
    ms->possible_cpus->len = max_cpus;
    for (i = 0; i < ms->possible_cpus->len; i++) {
        ms->possible_cpus->cpus[i].type = ms->cpu_type;
        ms->possible_cpus->cpus[i].vcpus_count = 1;
        ms->possible_cpus->cpus[i].props.has_core_id = true;
        ms->possible_cpus->cpus[i].props.core_id = i;
        ms->possible_cpus->cpus[i].arch_id = i;
    }
    return ms->possible_cpus;
}

static PFlashCFI01 *loongarch_pflash_create(LoongarchMachineState *lsms,
                                            const char *name,
                                            const char *alias_prop_name)
{
    DeviceState *dev = qdev_new(TYPE_PFLASH_CFI01);

    qdev_prop_set_uint64(dev, "sector-length", FLASH_SECTOR_SIZE);
    qdev_prop_set_uint8(dev, "width", 1);
    qdev_prop_set_string(dev, "name", name);
    object_property_add_child(OBJECT(lsms), name, OBJECT(dev));
    object_property_add_alias(OBJECT(lsms), alias_prop_name, OBJECT(dev),
                              "drive");
    return PFLASH_CFI01(dev);
}

static void loongarch_system_flash_create(LoongarchMachineState *lsms)
{
    lsms->flash[0] = loongarch_pflash_create(lsms, "system.flash0", "pflash0");
    lsms->flash[1] = loongarch_pflash_create(lsms, "system.flash1", "pflash1");
}

static void loongarch_machine_initfn(Object *obj)
{
    LoongarchMachineState *lsms = LoongarchMACHINE(obj);
    LoongarchMachineClass *lsmc = LoongarchMACHINE_GET_CLASS(lsms);
    lsms->acpi_build_enabled = lsmc->has_acpi_build;
    loongarch_system_flash_create(lsms);
    lsms->oem_id = g_strndup(EFI_ACPI_OEM_ID, 6);
    lsms->oem_table_id = g_strndup(EFI_ACPI_OEM_TABLE_ID, 6);
}

static void ls3a5k_ls7a_machine_options(MachineClass *m)
{
    char *cpu_name = get_host_cpu_model_name();
    LoongarchMachineClass *lsmc = LoongarchMACHINE_CLASS(m);
    m->desc = "Loongarch3a5k LS7A1000 machine";
    m->max_cpus = LOONGARCH_MAX_VCPUS;
    m->alias = "loongson7a";
    m->is_default = 1;
    lsmc->isa_io_base = LS3A5K_ISA_IO_BASE;
    lsmc->pciecfg_base = LS_PCIECFG_BASE;
    lsmc->ls7a_ioapic_reg_base = LS3A5K_LS7A_IOAPIC_REG_BASE;
    lsmc->node_shift = 44;
    strncpy(lsmc->cpu_name, cpu_name, sizeof(lsmc->cpu_name) - 1);
    lsmc->cpu_name[sizeof(lsmc->cpu_name) - 1] = 0;
    strncpy(lsmc->bridge_name, "ls7a", sizeof(lsmc->bridge_name) - 1);
    lsmc->bridge_name[sizeof(lsmc->bridge_name) - 1] = 0;
    compat_props_add(m->compat_props, loongarch_compat, loongarch_compat_len);
}

static void ls3a_board_reset(MachineState *ms)
{
    qemu_devices_reset();
#ifdef CONFIG_KVM
    struct loongarch_kvm_irqchip *chip;
    int length;

    if (!kvm_enabled()) {
        return;
    }
    length = sizeof(struct loongarch_kvm_irqchip) +
             sizeof(struct loongarch_gipiState);
    chip = g_malloc0(length);
    memset(chip, 0, length);
    chip->chip_id = KVM_IRQCHIP_LS3A_GIPI;
    chip->len = length;
    kvm_vm_ioctl(kvm_state, KVM_SET_IRQCHIP, chip);

    length = sizeof(struct loongarch_kvm_irqchip) +
             sizeof(struct ls7a_ioapic_state);
    chip = g_realloc(chip, length);
    memset(chip, 0, length);
    chip->chip_id = KVM_IRQCHIP_LS7A_IOAPIC;
    chip->len = length;
    kvm_vm_ioctl(kvm_state, KVM_SET_IRQCHIP, chip);

    g_free(chip);
#endif
}

static CpuInstanceProperties ls3a_cpu_index_to_props(MachineState *ms,
                                                     unsigned cpu_index)
{
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    const CPUArchIdList *possible_cpus = mc->possible_cpu_arch_ids(ms);

    assert(cpu_index < possible_cpus->len);
    return possible_cpus->cpus[cpu_index].props;
}

static int64_t ls3a_get_default_cpu_node_id(const MachineState *ms, int idx)
{
    int nb_numa_nodes = ms->numa_state->num_nodes;
    int smp_cores = ms->smp.cores;

    if (nb_numa_nodes == 0) {
        nb_numa_nodes = 1;
    }
    return idx / smp_cores % nb_numa_nodes;
}

static void loongarch_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    HotplugHandlerClass *hc = HOTPLUG_HANDLER_CLASS(oc);
    LoongarchMachineClass *lsmc = LoongarchMACHINE_CLASS(oc);

    lsmc->get_hotplug_handler = mc->get_hotplug_handler;
    lsmc->has_acpi_build = true;
    mc->get_hotplug_handler = loongarch_get_hotpug_handler;
    mc->has_hotpluggable_cpus = true;
    mc->cpu_index_to_instance_props = ls3a_cpu_index_to_props;
    mc->possible_cpu_arch_ids = loongarch_possible_cpu_arch_ids;
    mc->get_default_cpu_node_id = ls3a_get_default_cpu_node_id;
    mc->default_ram_size = 1 * GiB;
    mc->default_cpu_type = LOONGARCH_CPU_TYPE_NAME("Loongson-3A5000");
    mc->default_ram_id = "loongarch_ls3a.ram";

#ifdef CONFIG_TPM
    machine_class_allow_dynamic_sysbus_dev(mc, TYPE_TPM_TIS_SYSBUS);
#endif

    mc->reset = ls3a_board_reset;
    mc->max_cpus = LOONGARCH_MAX_VCPUS;
    hc->pre_plug = loongarch_machine_device_pre_plug;
    hc->plug = loongarch_machine_device_plug;
    hc->unplug = longson_machine_device_unplug;
    hc->unplug_request = loongarch_machine_device_unplug_request;

    object_class_property_add(oc, "acpi", "OnOffAuto", loongarch_get_acpi,
                              loongarch_set_acpi, NULL, NULL);
    object_class_property_set_description(oc, "acpi", "Enable ACPI");
}

static const TypeInfo loongarch_info = {
    .name = TYPE_LOONGARCH_MACHINE,
    .parent = TYPE_MACHINE,
    .abstract = true,
    .instance_size = sizeof(LoongarchMachineState),
    .instance_init = loongarch_machine_initfn,
    .class_size = sizeof(LoongarchMachineClass),
    .class_init = loongarch_class_init,
    .interfaces = (InterfaceInfo[]){ { TYPE_HOTPLUG_HANDLER }, {} },
};

static void loongarch_machine_register_types(void)
{
    type_register_static(&loongarch_info);
}

type_init(loongarch_machine_register_types)

    DEFINE_LS3A5K_MACHINE(loongson7a_v1_0, "loongson7a_v1.0",
                          ls3a5k_ls7a_machine_options);
