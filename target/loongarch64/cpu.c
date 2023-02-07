/*
 * QEMU LOONGARCH CPU
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
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "cpu.h"
#include "internal.h"
#include "kvm_larch.h"
#include "qemu-common.h"
#include "hw/qdev-properties.h"
#include "sysemu/kvm.h"
#include "exec/exec-all.h"
#include "sysemu/arch_init.h"
#include "cpu-csr.h"
#include "qemu/qemu-print.h"
#include "qapi/qapi-commands-machine-target.h"
#ifdef CONFIG_TCG
#include "hw/core/tcg-cpu-ops.h"
#endif /* CONFIG_TCG */

#define LOONGARCH_CONFIG1                                                     \
    ((0x8 << CSR_CONF1_KSNUM_SHIFT) | (0x2f << CSR_CONF1_TMRBITS_SHIFT) |     \
     (0x7 << CSR_CONF1_VSMAX_SHIFT))

#define LOONGARCH_CONFIG3                                                     \
    ((0x2 << CSR_CONF3_TLBORG_SHIFT) | (0x3f << CSR_CONF3_MTLBSIZE_SHIFT) |   \
     (0x7 << CSR_CONF3_STLBWAYS_SHIFT) | (0x8 << CSR_CONF3_STLBIDX_SHIFT))

/* LOONGARCH CPU definitions */
const loongarch_def_t loongarch_defs[] = {
    {
        .name = "Loongson-3A5000",

        /* for LoongISA CSR */
        .CSR_PRCFG1 = LOONGARCH_CONFIG1,
        .CSR_PRCFG2 = 0x3ffff000,
        .CSR_PRCFG3 = LOONGARCH_CONFIG3,
        .CSR_CRMD = (0 << CSR_CRMD_PLV_SHIFT) | (0 << CSR_CRMD_IE_SHIFT) |
                    (1 << CSR_CRMD_DA_SHIFT) | (0 << CSR_CRMD_PG_SHIFT) |
                    (1 << CSR_CRMD_DACF_SHIFT) | (1 << CSR_CRMD_DACM_SHIFT),
        .CSR_ECFG = 0x7 << 16,
        .CSR_STLBPGSIZE = 0xe,
        .CSR_RVACFG = 0x0,
        .CSR_ASID = 0xa0000,
        .FCSR0 = 0x0,
        .FCSR0_rw_bitmask = 0x1f1f03df,
        .PABITS = 48,
        .insn_flags = CPU_LARCH64 | INSN_LOONGARCH,
        .mmu_type = MMU_TYPE_LS3A5K,
    },
    {
        .name = "host",

        /* for LoongISA CSR */
        .CSR_PRCFG1 = LOONGARCH_CONFIG1,
        .CSR_PRCFG2 = 0x3ffff000,
        .CSR_PRCFG3 = LOONGARCH_CONFIG3,
        .CSR_CRMD = (0 << CSR_CRMD_PLV_SHIFT) | (0 << CSR_CRMD_IE_SHIFT) |
                    (1 << CSR_CRMD_DA_SHIFT) | (0 << CSR_CRMD_PG_SHIFT) |
                    (1 << CSR_CRMD_DACF_SHIFT) | (1 << CSR_CRMD_DACM_SHIFT),
        .CSR_ECFG = 0x7 << 16,
        .CSR_STLBPGSIZE = 0xe,
        .CSR_RVACFG = 0x0,
        .FCSR0 = 0x0,
        .FCSR0_rw_bitmask = 0x1f1f03df,
        .PABITS = 48,
        .insn_flags = CPU_LARCH64 | INSN_LOONGARCH,
        .mmu_type = MMU_TYPE_LS3A5K,
    },
};
const int loongarch_defs_number = ARRAY_SIZE(loongarch_defs);

void loongarch_cpu_list(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(loongarch_defs); i++) {
        qemu_printf("LOONGARCH '%s'\n", loongarch_defs[i].name);
    }
}

CpuDefinitionInfoList *qmp_query_cpu_definitions(Error **errp)
{
    CpuDefinitionInfoList *cpu_list = NULL;
    const loongarch_def_t *def;
    int i;

    for (i = 0; i < ARRAY_SIZE(loongarch_defs); i++) {
        CpuDefinitionInfoList *entry;
        CpuDefinitionInfo *info;

        def = &loongarch_defs[i];
        info = g_malloc0(sizeof(*info));
        info->name = g_strdup(def->name);

        entry = g_malloc0(sizeof(*entry));
        entry->value = info;
        entry->next = cpu_list;
        cpu_list = entry;
    }

    return cpu_list;
}

static void loongarch_cpu_set_pc(CPUState *cs, vaddr value)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;

    env->active_tc.PC = value & ~(target_ulong)1;
}

static bool loongarch_cpu_has_work(CPUState *cs)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    bool has_work = false;

    /*
     * It is implementation dependent if non-enabled
     * interrupts wake-up the CPU, however most of the implementations only
     * check for interrupts that can be taken.
     */
    if ((cs->interrupt_request & CPU_INTERRUPT_HARD) &&
        cpu_loongarch_hw_interrupts_pending(env)) {
        has_work = true;
    }

    return has_work;
}

const char *const regnames[] = {
    "r0", "ra", "tp", "sp", "a0", "a1", "a2", "a3", "a4", "a5", "a6",
    "a7", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "x0",
    "fp", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8",
};

const char *const fregnames[] = {
    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
};

static void fpu_dump_state(CPULOONGARCHState *env, FILE *f,
                           fprintf_function fpu_fprintf, int flags)
{
    int i;
    int is_fpu64 = 1;

#define printfpr(fp)                                                          \
    do {                                                                      \
        if (is_fpu64)                                                         \
            fpu_fprintf(                                                      \
                f, "w:%08x d:%016" PRIx64 " fd:%13g fs:%13g psu: %13g\n",     \
                (fp)->w[FP_ENDIAN_IDX], (fp)->d, (double)(fp)->fd,            \
                (double)(fp)->fs[FP_ENDIAN_IDX],                              \
                (double)(fp)->fs[!FP_ENDIAN_IDX]);                            \
        else {                                                                \
            fpr_t tmp;                                                        \
            tmp.w[FP_ENDIAN_IDX] = (fp)->w[FP_ENDIAN_IDX];                    \
            tmp.w[!FP_ENDIAN_IDX] = ((fp) + 1)->w[FP_ENDIAN_IDX];             \
            fpu_fprintf(f,                                                    \
                        "w:%08x d:%016" PRIx64 " fd:%13g fs:%13g psu:%13g\n", \
                        tmp.w[FP_ENDIAN_IDX], tmp.d, (double)tmp.fd,          \
                        (double)tmp.fs[FP_ENDIAN_IDX],                        \
                        (double)tmp.fs[!FP_ENDIAN_IDX]);                      \
        }                                                                     \
    } while (0)

    fpu_fprintf(f, "FCSR0 0x%08x  SR.FR %d  fp_status 0x%02x\n",
                env->active_fpu.fcsr0, is_fpu64,
                get_float_exception_flags(&env->active_fpu.fp_status));
    for (i = 0; i < 32; (is_fpu64) ? i++ : (i += 2)) {
        fpu_fprintf(f, "%3s: ", fregnames[i]);
        printfpr(&env->active_fpu.fpr[i]);
    }

#undef printfpr
}

void loongarch_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    int i;

    qemu_fprintf(f, "pc:\t  %lx\n", env->active_tc.PC);
    for (i = 0; i < 32; i++) {
        if ((i & 3) == 0) {
            qemu_fprintf(f, "GPR%02d:", i);
        }
        qemu_fprintf(f, " %s " TARGET_FMT_lx, regnames[i],
                     env->active_tc.gpr[i]);
        if ((i & 3) == 3) {
            qemu_fprintf(f, "\n");
        }
    }
    qemu_fprintf(f, "EUEN            0x%lx\n", env->CSR_EUEN);
    qemu_fprintf(f, "ESTAT           0x%lx\n", env->CSR_ESTAT);
    qemu_fprintf(f, "ERA             0x%lx\n", env->CSR_ERA);
    qemu_fprintf(f, "CRMD            0x%lx\n", env->CSR_CRMD);
    qemu_fprintf(f, "PRMD            0x%lx\n", env->CSR_PRMD);
    qemu_fprintf(f, "BadVAddr        0x%lx\n", env->CSR_BADV);
    qemu_fprintf(f, "TLB refill ERA  0x%lx\n", env->CSR_TLBRERA);
    qemu_fprintf(f, "TLB refill BadV 0x%lx\n", env->CSR_TLBRBADV);
    qemu_fprintf(f, "EEPN            0x%lx\n", env->CSR_EEPN);
    qemu_fprintf(f, "BadInstr        0x%lx\n", env->CSR_BADI);
    qemu_fprintf(f, "PRCFG1    0x%lx\nPRCFG2     0x%lx\nPRCFG3     0x%lx\n",
                 env->CSR_PRCFG1, env->CSR_PRCFG3, env->CSR_PRCFG3);
    if ((flags & CPU_DUMP_FPU) && (env->hflags & LARCH_HFLAG_FPU)) {
        fpu_dump_state(env, f, qemu_fprintf, flags);
    }
}

void cpu_state_reset(CPULOONGARCHState *env)
{
    LOONGARCHCPU *cpu = loongarch_env_get_cpu(env);
    CPUState *cs = CPU(cpu);

    /* Reset registers to their default values */
    env->CSR_PRCFG1 = env->cpu_model->CSR_PRCFG1;
    env->CSR_PRCFG2 = env->cpu_model->CSR_PRCFG2;
    env->CSR_PRCFG3 = env->cpu_model->CSR_PRCFG3;
    env->CSR_CRMD = env->cpu_model->CSR_CRMD;
    env->CSR_ECFG = env->cpu_model->CSR_ECFG;
    env->CSR_STLBPGSIZE = env->cpu_model->CSR_STLBPGSIZE;
    env->CSR_RVACFG = env->cpu_model->CSR_RVACFG;
    env->CSR_ASID = env->cpu_model->CSR_ASID;

    env->current_tc = 0;
    env->active_fpu.fcsr0_rw_bitmask = env->cpu_model->FCSR0_rw_bitmask;
    env->active_fpu.fcsr0 = env->cpu_model->FCSR0;
    env->insn_flags = env->cpu_model->insn_flags;

#if !defined(CONFIG_USER_ONLY)
    env->CSR_ERA = env->active_tc.PC;
    env->active_tc.PC = env->exception_base;
#ifdef CONFIG_TCG
    env->tlb->tlb_in_use = env->tlb->nb_tlb;
#endif
    env->CSR_TLBWIRED = 0;
    env->CSR_TMID = cs->cpu_index;
    env->CSR_CPUID = (cs->cpu_index & 0x1ff);
    env->CSR_EEPN |= (uint64_t)0x80000000;
    env->CSR_TLBRENT |= (uint64_t)0x80000000;
#endif

    /* Count register increments in debug mode, EJTAG version 1 */
    env->CSR_DEBUG = (1 << CSR_DEBUG_DINT) | (0x1 << CSR_DEBUG_DMVER);

    compute_hflags(env);
    restore_fp_status(env);
    cs->exception_index = EXCP_NONE;
}

/* CPUClass::reset() */
static void loongarch_cpu_reset(DeviceState *dev)
{
    CPUState *s = CPU(dev);
    LOONGARCHCPU *cpu = LOONGARCH_CPU(s);
    LOONGARCHCPUClass *mcc = LOONGARCH_CPU_GET_CLASS(cpu);
    CPULOONGARCHState *env = &cpu->env;

    mcc->parent_reset(dev);

    memset(env, 0, offsetof(CPULOONGARCHState, end_reset_fields));

    cpu_state_reset(env);

#ifndef CONFIG_USER_ONLY
    if (kvm_enabled()) {
        kvm_loongarch_reset_vcpu(cpu);
    }
#endif
}

static void loongarch_cpu_disas_set_info(CPUState *s, disassemble_info *info)
{
    info->print_insn = print_insn_loongarch;
}

static void fpu_init(CPULOONGARCHState *env, const loongarch_def_t *def)
{
    memcpy(&env->active_fpu, &env->fpus[0], sizeof(env->active_fpu));
}

void cpu_loongarch_realize_env(CPULOONGARCHState *env)
{
    env->exception_base = 0x1C000000;

#if defined(CONFIG_TCG) && !defined(CONFIG_USER_ONLY)
    mmu_init(env, env->cpu_model);
#endif
    fpu_init(env, env->cpu_model);
}

static void loongarch_cpu_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    LOONGARCHCPU *cpu = LOONGARCH_CPU(dev);
    LOONGARCHCPUClass *mcc = LOONGARCH_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }

    cpu_loongarch_realize_env(&cpu->env);

    loongarch_cpu_register_gdb_regs_for_features(cs);

    cpu_reset(cs);
    qemu_init_vcpu(cs);

    mcc->parent_realize(dev, errp);
    cpu->hotplugged = 1;
}

static void loongarch_cpu_unrealizefn(DeviceState *dev)
{
    LOONGARCHCPUClass *mcc = LOONGARCH_CPU_GET_CLASS(dev);

#ifndef CONFIG_USER_ONLY
    cpu_remove_sync(CPU(dev));
#endif

    mcc->parent_unrealize(dev);
}

static void loongarch_cpu_initfn(Object *obj)
{
    CPUState *cs = CPU(obj);
    LOONGARCHCPU *cpu = LOONGARCH_CPU(obj);
    CPULOONGARCHState *env = &cpu->env;
    LOONGARCHCPUClass *mcc = LOONGARCH_CPU_GET_CLASS(obj);
    cpu_set_cpustate_pointers(cpu);
    cs->env_ptr = env;
    env->cpu_model = mcc->cpu_def;
    cs->halted = 1;
    cpu->dtb_compatible = "loongarch,Loongson-3A5000";
}

static char *loongarch_cpu_type_name(const char *cpu_model)
{
    return g_strdup_printf(LOONGARCH_CPU_TYPE_NAME("%s"), cpu_model);
}

static ObjectClass *loongarch_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;
    char *typename;

    typename = loongarch_cpu_type_name(cpu_model);
    oc = object_class_by_name(typename);
    g_free(typename);
    return oc;
}

static int64_t loongarch_cpu_get_arch_id(CPUState *cs)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);

    return cpu->id;
}

static Property loongarch_cpu_properties[] = {
    DEFINE_PROP_INT32("core-id", LOONGARCHCPU, core_id, -1),
    DEFINE_PROP_INT32("id", LOONGARCHCPU, id, UNASSIGNED_CPU_ID),
    DEFINE_PROP_INT32("node-id", LOONGARCHCPU, node_id,
                      CPU_UNSET_NUMA_NODE_ID),

    DEFINE_PROP_END_OF_LIST()
};

#ifdef CONFIG_TCG
static void loongarch_cpu_synchronize_from_tb(CPUState *cs,
                                              const TranslationBlock *tb)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;

    env->active_tc.PC = tb->pc;
    env->hflags &= ~LARCH_HFLAG_BMASK;
    env->hflags |= tb->flags & LARCH_HFLAG_BMASK;
}

static const struct TCGCPUOps loongarch_tcg_ops = {
    .initialize = loongarch_tcg_init,
    .synchronize_from_tb = loongarch_cpu_synchronize_from_tb,

    .tlb_fill = loongarch_cpu_tlb_fill,
    .cpu_exec_interrupt = loongarch_cpu_exec_interrupt,
    .do_interrupt = loongarch_cpu_do_interrupt,

#ifndef CONFIG_USER_ONLY
    .do_unaligned_access = loongarch_cpu_do_unaligned_access,
#endif /* !CONFIG_USER_ONLY */
};
#endif /* CONFIG_TCG */

#if !defined(CONFIG_USER_ONLY)
static int get_physical_address(CPULOONGARCHState *env, hwaddr *physical,
                                int *prot, target_ulong real_address, int rw,
                                int access_type, int mmu_idx)
{
    int user_mode = mmu_idx == LARCH_HFLAG_UM;
    int kernel_mode = !user_mode;
    unsigned plv, base_c, base_v, tmp;

    /* effective address (modified for KVM T&E kernel segments) */
    target_ulong address = real_address;

    /* Check PG */
    if (!(env->CSR_CRMD & CSR_CRMD_PG)) {
        /* DA mode */
        *physical = address & 0xffffffffffffUL;
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TLBRET_MATCH;
    }

    plv = kernel_mode | (user_mode << 3);
    base_v = address >> CSR_DMW_BASE_SH;
    /* Check direct map window 0 */
    base_c = env->CSR_DMWIN0 >> CSR_DMW_BASE_SH;
    if ((plv & env->CSR_DMWIN0) && (base_c == base_v)) {
        *physical = dmwin_va2pa(address);
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TLBRET_MATCH;
    }
    /* Check direct map window 1 */
    base_c = env->CSR_DMWIN1 >> CSR_DMW_BASE_SH;
    if ((plv & env->CSR_DMWIN1) && (base_c == base_v)) {
        *physical = dmwin_va2pa(address);
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TLBRET_MATCH;
    }
    /* Check valid extension */
    tmp = address >> 47;
    if (!(tmp == 0 || tmp == 0x1ffff)) {
        return TLBRET_BADADDR;
    }
    /* mapped address */
    return env->tlb->map_address(env, physical, prot, real_address, rw,
                                 access_type);
}

hwaddr loongarch_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    LOONGARCHCPU *cpu = LOONGARCH_CPU(cs);
    CPULOONGARCHState *env = &cpu->env;
    hwaddr phys_addr;
    int prot;

    if (get_physical_address(env, &phys_addr, &prot, addr, 0, ACCESS_INT,
                             cpu_mmu_index(env, false)) != 0) {
        return -1;
    }
    return phys_addr;
}
#endif

#ifndef CONFIG_USER_ONLY
#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps loongarch_sysemu_ops = {
    .write_elf64_note = loongarch_cpu_write_elf64_note,
    .get_phys_page_debug = loongarch_cpu_get_phys_page_debug,
    .legacy_vmsd = &vmstate_loongarch_cpu,
};
#endif

static gchar *loongarch_gdb_arch_name(CPUState *cs)
{
    return g_strdup("loongarch64");
}

static void loongarch_cpu_class_init(ObjectClass *c, void *data)
{
    LOONGARCHCPUClass *mcc = LOONGARCH_CPU_CLASS(c);
    CPUClass *cc = CPU_CLASS(c);
    DeviceClass *dc = DEVICE_CLASS(c);

    device_class_set_props(dc, loongarch_cpu_properties);
    device_class_set_parent_realize(dc, loongarch_cpu_realizefn,
                                    &mcc->parent_realize);

    device_class_set_parent_unrealize(dc, loongarch_cpu_unrealizefn,
                                      &mcc->parent_unrealize);

    device_class_set_parent_reset(dc, loongarch_cpu_reset, &mcc->parent_reset);
    cc->get_arch_id = loongarch_cpu_get_arch_id;

    cc->class_by_name = loongarch_cpu_class_by_name;
    cc->has_work = loongarch_cpu_has_work;
    cc->dump_state = loongarch_cpu_dump_state;
    cc->set_pc = loongarch_cpu_set_pc;
    cc->gdb_read_register = loongarch_cpu_gdb_read_register;
    cc->gdb_write_register = loongarch_cpu_gdb_write_register;
    cc->disas_set_info = loongarch_cpu_disas_set_info;
    cc->gdb_arch_name = loongarch_gdb_arch_name;
#ifndef CONFIG_USER_ONLY
    cc->sysemu_ops = &loongarch_sysemu_ops;
#endif /* !CONFIG_USER_ONLY */

    cc->gdb_num_core_regs = 35;
    cc->gdb_core_xml_file = "loongarch-base64.xml";
    cc->gdb_stop_before_watchpoint = true;

    dc->user_creatable = true;
#ifdef CONFIG_TCG
    cc->tcg_ops = &loongarch_tcg_ops;
#endif /* CONFIG_TCG */
}

static const TypeInfo loongarch_cpu_type_info = {
    .name = TYPE_LOONGARCH_CPU,
    .parent = TYPE_CPU,
    .instance_size = sizeof(LOONGARCHCPU),
    .instance_init = loongarch_cpu_initfn,
    .abstract = true,
    .class_size = sizeof(LOONGARCHCPUClass),
    .class_init = loongarch_cpu_class_init,
};

static void loongarch_cpu_cpudef_class_init(ObjectClass *oc, void *data)
{
    LOONGARCHCPUClass *mcc = LOONGARCH_CPU_CLASS(oc);
    mcc->cpu_def = data;
}

static void loongarch_register_cpudef_type(const struct loongarch_def_t *def)
{
    char *typename = loongarch_cpu_type_name(def->name);
    TypeInfo ti = {
        .name = typename,
        .parent = TYPE_LOONGARCH_CPU,
        .class_init = loongarch_cpu_cpudef_class_init,
        .class_data = (void *)def,
    };

    type_register(&ti);
    g_free(typename);
}

static void loongarch_cpu_register_types(void)
{
    int i;

    type_register_static(&loongarch_cpu_type_info);
    for (i = 0; i < loongarch_defs_number; i++) {
        loongarch_register_cpudef_type(&loongarch_defs[i]);
    }
}

type_init(loongarch_cpu_register_types)
