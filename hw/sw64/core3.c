/*
 * QEMU CORE3 hardware system emulator.
 *
 * Copyright (c) 2021 Lu Feifei
 *
 * This work is licensed under the GNU GPL license version 2 or later.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/datadir.h"
#include "cpu.h"
#include "hw/hw.h"
#include "elf.h"
#include "hw/loader.h"
#include "hw/boards.h"
#include "qemu/error-report.h"
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "sysemu/reset.h"
#include "hw/ide.h"
#include "hw/char/serial.h"
#include "qemu/cutils.h"
#include "ui/console.h"
#include "core.h"
#include "hw/boards.h"
#include "sysemu/numa.h"
#include "qemu/uuid.h"
#include "qemu/bswap.h"

#define VMUUID 0xFF40

static uint64_t cpu_sw64_virt_to_phys(void *opaque, uint64_t addr)
{
    return addr &= ~0xffffffff80000000 ;
}

static CpuInstanceProperties
sw64_cpu_index_to_props(MachineState *ms, unsigned cpu_index)
{
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    const CPUArchIdList *possible_cpus = mc->possible_cpu_arch_ids(ms);

    assert(cpu_index < possible_cpus->len);
    return possible_cpus->cpus[cpu_index].props;
}

static int64_t sw64_get_default_cpu_node_id(const MachineState *ms, int idx)
{
    int nb_numa_nodes = ms->numa_state->num_nodes;
    return idx % nb_numa_nodes;
}

static const CPUArchIdList *sw64_possible_cpu_arch_ids(MachineState *ms)
{
    int i;
    unsigned int max_cpus = ms->smp.max_cpus;

    if (ms->possible_cpus) {
        /*
         * make sure that max_cpus hasn't changed since the first use, i.e.
         * -smp hasn't been parsed after it
        */
        assert(ms->possible_cpus->len == max_cpus);
        return ms->possible_cpus;
    }

    ms->possible_cpus = g_malloc0(sizeof(CPUArchIdList) +
                                  sizeof(CPUArchId) * max_cpus);
    ms->possible_cpus->len = max_cpus;
    for (i = 0; i < ms->possible_cpus->len; i++) {
        ms->possible_cpus->cpus[i].type = ms->cpu_type;
        ms->possible_cpus->cpus[i].vcpus_count = 1;
        ms->possible_cpus->cpus[i].arch_id = i;
        ms->possible_cpus->cpus[i].props.has_thread_id = true;
        ms->possible_cpus->cpus[i].props.has_core_id = true;
        ms->possible_cpus->cpus[i].props.core_id = i;
    }

    return ms->possible_cpus;
}

static void core3_cpu_reset(void *opaque)
{
    SW64CPU *cpu = opaque;

    cpu_reset(CPU(cpu));
}

static void core3_init(MachineState *machine)
{
    ram_addr_t ram_size = machine->ram_size;
    ram_addr_t buf;
    SW64CPU *cpus[machine->smp.max_cpus];
    long i, size;
    const char *kernel_filename = machine->kernel_filename;
    const char *kernel_cmdline = machine->kernel_cmdline;
    char *hmcode_filename;
    char *uefi_filename;
    uint64_t hmcode_entry, hmcode_low, hmcode_high;
    uint64_t kernel_entry, kernel_low, kernel_high;
    BOOT_PARAMS *core3_boot_params = g_new0(BOOT_PARAMS, 1);
    uint64_t param_offset;
    QemuUUID uuid_out_put;

    memset(cpus, 0, sizeof(cpus));

    for (i = 0; i < machine->smp.cpus; ++i) {
	cpus[i] = SW64_CPU(cpu_create(machine->cpu_type));
	cpus[i]->env.csr[CID] = i;
	qemu_register_reset(core3_cpu_reset, cpus[i]);
    }
    core3_board_init(cpus, machine->ram);
    if (kvm_enabled())
        buf = ram_size;
    else
        buf = ram_size | (1UL << 63);

    rom_add_blob_fixed("ram_size", (char *)&buf, 0x8, 0x2040);

    uuid_out_put = qemu_uuid;
    uuid_out_put = qemu_uuid_bswap(uuid_out_put);
    pstrcpy_targphys("vm-uuid", VMUUID, 0x12, (char *)&(uuid_out_put));
    param_offset = 0x90B000UL;
    core3_boot_params->cmdline = param_offset | 0xfff0000000000000UL;
    rom_add_blob_fixed("core3_boot_params", (core3_boot_params), 0x48, 0x90A100);

    hmcode_filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, kvm_enabled() ? "core3-reset":"core3-hmcode");
    if (hmcode_filename == NULL) {
	if (kvm_enabled())
	    error_report("no core3-reset provided");
	else
	    error_report("no core3-hmcode provided");
	exit(1);
    }
    size = load_elf(hmcode_filename, NULL, cpu_sw64_virt_to_phys, NULL,
		    &hmcode_entry, &hmcode_low, &hmcode_high, NULL, 0, EM_SW64, 0, 0);
    if (size < 0) {
	if (kvm_enabled())
	    error_report("could not load core3-reset: '%s'", hmcode_filename);
	else
	    error_report("could not load core3-hmcode: '%s'", hmcode_filename);
	exit(1);
    }
    g_free(hmcode_filename);

    /* Start all cpus at the hmcode RESET entry point.  */
    for (i = 0; i < machine->smp.cpus; ++i) {
	if (kvm_enabled())
	    cpus[i]->env.pc = init_pc;
	else
	    cpus[i]->env.pc = hmcode_entry;
        cpus[i]->env.hm_entry = hmcode_entry;
    }

    if (!kernel_filename) {
	uefi_filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, "uefi-bios-sw");
	if (uefi_filename == NULL) {
	    error_report("no virtual bios provided");
	    exit(1);
	}
	size = load_image_targphys(uefi_filename, 0x2f00000UL, -1);
	if (size < 0) {
	    error_report("could not load virtual bios: '%s'", uefi_filename);
	    exit(1);
	}
	g_free(uefi_filename);
    } else {
	/* Load a kernel.  */
	size = load_elf(kernel_filename, NULL, cpu_sw64_virt_to_phys, NULL,
			&kernel_entry, &kernel_low, &kernel_high, NULL, 0, EM_SW64, 0, 0);
	if (size < 0) {
	    error_report("could not load kernel '%s'", kernel_filename);
	    exit(1);
	}
	cpus[0]->env.trap_arg1 = kernel_entry;
	if (kernel_cmdline)
	    pstrcpy_targphys("cmdline", param_offset, 0x400, kernel_cmdline);
    }
}

static void board_reset(MachineState *state)
{
    qemu_devices_reset();
}

static void core3_machine_init(MachineClass *mc)
{
    mc->desc = "core3 BOARD";
    mc->init = core3_init;
    mc->block_default_type = IF_IDE;
    mc->max_cpus = MAX_CPUS_CORE3;
    mc->pci_allow_0_address = true;
    mc->is_default = 0;
    mc->reset = board_reset;
    mc->possible_cpu_arch_ids = sw64_possible_cpu_arch_ids;
    mc->cpu_index_to_instance_props = sw64_cpu_index_to_props;
    mc->default_cpu_type = SW64_CPU_TYPE_NAME("core3");
    mc->default_ram_id = "ram";
    mc->get_default_cpu_node_id = sw64_get_default_cpu_node_id;
}

DEFINE_MACHINE("core3", core3_machine_init)
