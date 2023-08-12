/*
 * QEMU SW64 CPU
 *
 * Copyright (c) 2018 Lin Hainan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "sysemu/kvm.h"
#include "disas/dis-asm.h"
#include "kvm_sw64.h"
#include "sysemu/reset.h"
#include "hw/qdev-properties.h"

static void sw64_cpu_set_pc(CPUState *cs, vaddr value)
{
    SW64CPU *cpu = SW64_CPU(cs);

    cpu->env.pc = value;
}

static void sw64_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    int i;

    static const char ireg_names[31][4] = {
        "v0", "t0", "t1",  "t2",  "t3", "t4",  "t5", "t6", "t7", "s0", "s1",
        "s2", "s3", "s4",  "s5",  "fp", "a0",  "a1", "a2", "a3", "a4", "a5",
        "t8", "t9", "t10", "t11", "ra", "t12", "at", "gp", "sp"};
    static const char freg_names[128][4] = {
        "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",  "f8",  "f9",
        "f10", "f11", "f12", "f13", "f14", "f15", "f16", "f17", "f18", "f19",
        "f20", "f21", "f22", "f23", "f24", "f25", "f26", "f27", "f28", "f29",
        "f30", "f31", "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
        "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15", "f16", "f17",
        "f18", "f19", "f20", "f21", "f22", "f23", "f24", "f25", "f26", "f27",
        "f28", "f29", "f30", "f31", "f0",  "f1",  "f2",  "f3",  "f4",  "f5",
        "f6",  "f7",  "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
        "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23", "f24", "f25",
        "f26", "f27", "f28", "f29", "f30", "f31", "f0",  "f1",  "f2",  "f3",
        "f4",  "f5",  "f6",  "f7",  "f8",  "f9",  "f10", "f11", "f12", "f13",
        "f14", "f15", "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
        "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"};
    qemu_fprintf(f, "PC=%016" PRIx64 "  SP=%016" PRIx64 "\n", env->pc,
                env->ir[IDX_SP]);
    for (i = 0; i < 31; i++) {
        qemu_fprintf(f, "%s=%016" PRIx64, ireg_names[i], env->ir[i]);
        if ((i % 4) == 3) {
            qemu_fprintf(f, "\n");
        } else {
            qemu_fprintf(f, " ");
        }
    }
    qemu_fprintf(f, "\n");
#ifndef CONFIG_USER_ONLY
    static const char sreg_names[10][4] = {"p1", "p2",  "p4",  "p5",  "p6",
                                           "p7", "p20", "p21", "p22", "p23"};
    for (i = 0; i < 10; i++) {
        qemu_fprintf(f, "%s=%016" PRIx64, sreg_names[i], env->sr[i]);
        if ((i % 4) == 3) {
            qemu_fprintf(f, "\n");
        } else {
            qemu_fprintf(f, " ");
        }
    }
    qemu_fprintf(f, "\n");
#endif
    for (i = 0; i < 32; i++) {
        qemu_fprintf(f, "%s=%016" PRIx64, freg_names[i + 96], env->fr[i + 96]);
        qemu_fprintf(f, " %016" PRIx64, env->fr[i + 64]);
        qemu_fprintf(f, " %016" PRIx64, env->fr[i + 32]);
        qemu_fprintf(f, " %016" PRIx64, env->fr[i]);
        qemu_fprintf(f, "\n");
    }
    qemu_fprintf(f, "\n");
}

#ifndef CONFIG_USER_ONLY
static void sw64_machine_cpu_reset(void *opaque)
{
    SW64CPU *cpu = opaque;

    cpu_reset(CPU(cpu));
}
#endif

static void sw64_cpu_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    SW64CPUClass *scc = SW64_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }
#ifndef CONFIG_USER_ONLY
    qemu_register_reset(sw64_machine_cpu_reset, cs);
#endif

    qemu_init_vcpu(cs);

    scc->parent_realize(dev, errp);
}

static void sw64_cpu_disas_set_info(CPUState *cs, disassemble_info *info)
{
    info->mach = bfd_mach_sw_64_core3;
    info->print_insn = print_insn_sw_64;
}

#include "fpu/softfloat.h"

static void core3_init(Object *obj)
{
    CPUState *cs = CPU(obj);
    CPUSW64State *env = cs->env_ptr;
#ifdef CONFIG_USER_ONLY
    env->fpcr = 0x680e800000000000;
#endif
    set_feature(env, SW64_FEATURE_CORE3);
}

static ObjectClass *sw64_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;
    char *typename;
    char **cpuname;

    cpuname = g_strsplit(cpu_model, ",", 1);
    typename = g_strdup_printf(SW64_CPU_TYPE_NAME("%s"), cpu_model);

    oc = object_class_by_name(typename);
    g_strfreev(cpuname);
    g_free(typename);

    if (oc && object_class_dynamic_cast(oc, TYPE_SW64_CPU) &&
        !object_class_is_abstract(oc)) {
        return oc;
    }
    return NULL;
}

bool sw64_cpu_has_work(CPUState *cs)
{
    /* If CPU has gotten into asleep(halt), then it may be
     * wake up by hard interrupt, timer, ii, mail or mchk.
     */
    return cs->interrupt_request & (CPU_INTERRUPT_HARD | CPU_INTERRUPT_TIMER |
                                    CPU_INTERRUPT_II0| CPU_INTERRUPT_MCHK);
}

static void sw64_cpu_initfn(Object *obj)
{
    CPUState *cs = CPU(obj);
    SW64CPU *cpu = SW64_CPU(obj);
    CPUSW64State *env = &cpu->env;

    cpu_set_cpustate_pointers(cpu);

    cs->env_ptr = env;
#ifndef CONFIG_USER_ONLY
    env->flags = ENV_FLAG_HM_MODE;
#else
    env->flags = ENV_FLAG_PS_USER;
#endif
    tlb_flush(cs);
}

#ifndef CONFIG_USER_ONLY
static void sw64_cpu_do_transaction_failed(CPUState *cs, hwaddr physaddr, vaddr addr,
                              unsigned size, MMUAccessType access_type,
                              int mmu_idx, MemTxAttrs attrs,
                              MemTxResult response, uintptr_t retaddr)
{
#ifdef DEBUG_TRANS
    if (retaddr) {
        cpu_restore_state(cs, retaddr, true);
    }
    fprintf(stderr, "PC = %lx, Wrong IO addr. Hwaddr = %lx, vaddr = %lx, access_type = %d\n",
            env->pc, physaddr, addr, access_type);
#endif
}
#endif

static void sw64_cpu_reset(DeviceState *dev)
{
    CPUState *s = CPU(dev);
    SW64CPU *cpu = SW64_CPU(s);
    SW64CPUClass *scc = SW64_CPU_GET_CLASS(cpu);

    scc->parent_reset(dev);

#ifndef CONFIG_USER_ONLY
    if (kvm_enabled()) {
        kvm_sw64_reset_vcpu(cpu);
    }
#endif
}

static Property sw64_cpu_properties[] = {
#ifdef CONFIG_USER_ONLY
    /* apic_id = 0 by default for *-user, see commit 9886e834 */
    DEFINE_PROP_UINT32("cid", SW64CPU, cid, 0),
#else
    DEFINE_PROP_UINT32("cid", SW64CPU, cid, 0xFFFFFFFF),
#endif
    DEFINE_PROP_END_OF_LIST()
};

#ifndef CONFIG_USER_ONLY
#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps sw64_sysemu_ops = {
    .get_phys_page_debug = sw64_cpu_get_phys_page_debug,
};
#endif

#include "hw/core/tcg-cpu-ops.h"

static const struct TCGCPUOps sw64_tcg_ops = {
    .initialize = sw64_translate_init,

#ifndef CONFIG_USER_ONLY
    .tlb_fill = sw64_cpu_tlb_fill,
    .do_unaligned_access = sw64_cpu_do_unaligned_access,
    .cpu_exec_interrupt = sw64_cpu_exec_interrupt,
    .do_transaction_failed = sw64_cpu_do_transaction_failed,
    .do_interrupt = sw64_cpu_do_interrupt,
#endif /* !CONFIG_USER_ONLY */
};

static void sw64_cpu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);
    SW64CPUClass *scc = SW64_CPU_CLASS(oc);

    device_class_set_parent_realize(dc, sw64_cpu_realizefn, &scc->parent_realize);
    device_class_set_parent_reset(dc, sw64_cpu_reset, &scc->parent_reset);
    device_class_set_props(dc, sw64_cpu_properties);

    cc->class_by_name = sw64_cpu_class_by_name;
#ifndef CONFIG_USER_ONLY
    dc->vmsd = &vmstate_sw64_cpu;
    cc->sysemu_ops = &sw64_sysemu_ops;
#endif
    cc->has_work = sw64_cpu_has_work;
    cc->set_pc = sw64_cpu_set_pc;
    cc->disas_set_info = sw64_cpu_disas_set_info;
    cc->dump_state = sw64_cpu_dump_state;

    cc->gdb_read_register = sw64_cpu_gdb_read_register;
    cc->gdb_write_register = sw64_cpu_gdb_write_register;
    cc->gdb_num_core_regs = 67;
    cc->gdb_core_xml_file = "sw64-core.xml";

    cc->tcg_ops = &sw64_tcg_ops;
}

static const SW64CPUInfo sw64_cpus[] =
{
    {
        .name = "core3",
        .initfn = core3_init,
    },
    {
        .name = NULL
    },
};

static void cpu_register(const SW64CPUInfo *info)
{
    TypeInfo type_info = {
        .parent = TYPE_SW64_CPU,
        .instance_size = sizeof(SW64CPU),
        .instance_init = info->initfn,
        .class_size = sizeof(SW64CPUClass),
        .class_init = info->class_init,
    };

    type_info.name = g_strdup_printf("%s-" TYPE_SW64_CPU, info->name);
    type_register(&type_info);
    g_free((void*)type_info.name);
}

static const TypeInfo sw64_cpu_type_info = {
    .name = TYPE_SW64_CPU,
    .parent = TYPE_CPU,
    .instance_size = sizeof(SW64CPU),
    .instance_init = sw64_cpu_initfn,
    .abstract = true,
    .class_size = sizeof(SW64CPUClass),
    .class_init = sw64_cpu_class_init,
};

static void sw64_cpu_register_types(void)
{
    const SW64CPUInfo *info = sw64_cpus;

    type_register_static(&sw64_cpu_type_info);

    while (info->name) {
        cpu_register(info);
        info++;
    }
}

type_init(sw64_cpu_register_types)
