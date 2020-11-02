/*
 * QEMU AArch64 CPU
 *
 * Copyright (c) 2013 Linaro Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "cpu.h"
#include "qemu/module.h"
#if !defined(CONFIG_USER_ONLY)
#include "hw/loader.h"
#endif
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "kvm_arm.h"
#include "qapi/visitor.h"
#include "hw/qdev-properties.h"

static inline void set_feature(CPUARMState *env, int feature)
{
    env->features |= 1ULL << feature;
}

static inline void unset_feature(CPUARMState *env, int feature)
{
    env->features &= ~(1ULL << feature);
}

#ifndef CONFIG_USER_ONLY
static uint64_t a57_a53_l2ctlr_read(CPUARMState *env, const ARMCPRegInfo *ri)
{
    ARMCPU *cpu = env_archcpu(env);

    /* Number of cores is in [25:24]; otherwise we RAZ */
    return (cpu->core_count - 1) << 24;
}
#endif

static const ARMCPRegInfo cortex_a72_a57_a53_cp_reginfo[] = {
#ifndef CONFIG_USER_ONLY
    { .name = "L2CTLR_EL1", .state = ARM_CP_STATE_AA64,
      .opc0 = 3, .opc1 = 1, .crn = 11, .crm = 0, .opc2 = 2,
      .access = PL1_RW, .readfn = a57_a53_l2ctlr_read,
      .writefn = arm_cp_write_ignore },
    { .name = "L2CTLR",
      .cp = 15, .opc1 = 1, .crn = 9, .crm = 0, .opc2 = 2,
      .access = PL1_RW, .readfn = a57_a53_l2ctlr_read,
      .writefn = arm_cp_write_ignore },
#endif
    { .name = "L2ECTLR_EL1", .state = ARM_CP_STATE_AA64,
      .opc0 = 3, .opc1 = 1, .crn = 11, .crm = 0, .opc2 = 3,
      .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "L2ECTLR",
      .cp = 15, .opc1 = 1, .crn = 9, .crm = 0, .opc2 = 3,
      .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "L2ACTLR", .state = ARM_CP_STATE_BOTH,
      .opc0 = 3, .opc1 = 1, .crn = 15, .crm = 0, .opc2 = 0,
      .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "CPUACTLR_EL1", .state = ARM_CP_STATE_AA64,
      .opc0 = 3, .opc1 = 1, .crn = 15, .crm = 2, .opc2 = 0,
      .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "CPUACTLR",
      .cp = 15, .opc1 = 0, .crm = 15,
      .access = PL1_RW, .type = ARM_CP_CONST | ARM_CP_64BIT, .resetvalue = 0 },
    { .name = "CPUECTLR_EL1", .state = ARM_CP_STATE_AA64,
      .opc0 = 3, .opc1 = 1, .crn = 15, .crm = 2, .opc2 = 1,
      .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "CPUECTLR",
      .cp = 15, .opc1 = 1, .crm = 15,
      .access = PL1_RW, .type = ARM_CP_CONST | ARM_CP_64BIT, .resetvalue = 0 },
    { .name = "CPUMERRSR_EL1", .state = ARM_CP_STATE_AA64,
      .opc0 = 3, .opc1 = 1, .crn = 15, .crm = 2, .opc2 = 2,
      .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "CPUMERRSR",
      .cp = 15, .opc1 = 2, .crm = 15,
      .access = PL1_RW, .type = ARM_CP_CONST | ARM_CP_64BIT, .resetvalue = 0 },
    { .name = "L2MERRSR_EL1", .state = ARM_CP_STATE_AA64,
      .opc0 = 3, .opc1 = 1, .crn = 15, .crm = 2, .opc2 = 3,
      .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "L2MERRSR",
      .cp = 15, .opc1 = 3, .crm = 15,
      .access = PL1_RW, .type = ARM_CP_CONST | ARM_CP_64BIT, .resetvalue = 0 },
    REGINFO_SENTINEL
};

static void aarch64_a57_initfn(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);

    cpu->dtb_compatible = "arm,cortex-a57";
    set_feature(&cpu->env, ARM_FEATURE_V8);
    set_feature(&cpu->env, ARM_FEATURE_VFP4);
    set_feature(&cpu->env, ARM_FEATURE_NEON);
    set_feature(&cpu->env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(&cpu->env, ARM_FEATURE_AARCH64);
    set_feature(&cpu->env, ARM_FEATURE_CBAR_RO);
    set_feature(&cpu->env, ARM_FEATURE_EL2);
    set_feature(&cpu->env, ARM_FEATURE_EL3);
    set_feature(&cpu->env, ARM_FEATURE_PMU);
    cpu->kvm_target = QEMU_KVM_ARM_TARGET_CORTEX_A57;
    cpu->midr = 0x411fd070;
    cpu->revidr = 0x00000000;
    cpu->reset_fpsid = 0x41034070;
    cpu->isar.regs[MVFR0] = 0x10110222;
    cpu->isar.regs[MVFR1] = 0x12111111;
    cpu->isar.regs[MVFR2] = 0x00000043;
    cpu->ctr = 0x8444c004;
    cpu->reset_sctlr = 0x00c50838;
    cpu->id_pfr0 = 0x00000131;
    cpu->id_pfr1 = 0x00011011;
    cpu->isar.regs[ID_DFR0] = 0x03010066;
    cpu->id_afr0 = 0x00000000;
    cpu->isar.regs[ID_MMFR0] = 0x10101105;
    cpu->isar.regs[ID_MMFR1] = 0x40000000;
    cpu->isar.regs[ID_MMFR2] = 0x01260000;
    cpu->isar.regs[ID_MMFR3] = 0x02102211;
    cpu->isar.regs[ID_ISAR0] = 0x02101110;
    cpu->isar.regs[ID_ISAR1] = 0x13112111;
    cpu->isar.regs[ID_ISAR2] = 0x21232042;
    cpu->isar.regs[ID_ISAR3] = 0x01112131;
    cpu->isar.regs[ID_ISAR4] = 0x00011142;
    cpu->isar.regs[ID_ISAR5] = 0x00011121;
    cpu->isar.regs[ID_ISAR6] = 0;
    cpu->isar.regs[ID_AA64PFR0] = 0x00002222;
    cpu->isar.regs[ID_AA64DFR0] = 0x10305106;
    cpu->isar.regs[ID_AA64ISAR0] = 0x00011120;
    cpu->isar.regs[ID_AA64MMFR0] = 0x00001124;
    cpu->isar.regs[DBGDIDR] = 0x3516d000;
    cpu->clidr = 0x0a200023;
    cpu->ccsidr[0] = 0x701fe00a; /* 32KB L1 dcache */
    cpu->ccsidr[1] = 0x201fe012; /* 48KB L1 icache */
    cpu->ccsidr[2] = 0x70ffe07a; /* 2048KB L2 cache */
    cpu->dcz_blocksize = 4; /* 64 bytes */
    cpu->gic_num_lrs = 4;
    cpu->gic_vpribits = 5;
    cpu->gic_vprebits = 5;
    define_arm_cp_regs(cpu, cortex_a72_a57_a53_cp_reginfo);
}

static void aarch64_a53_initfn(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);

    cpu->dtb_compatible = "arm,cortex-a53";
    set_feature(&cpu->env, ARM_FEATURE_V8);
    set_feature(&cpu->env, ARM_FEATURE_VFP4);
    set_feature(&cpu->env, ARM_FEATURE_NEON);
    set_feature(&cpu->env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(&cpu->env, ARM_FEATURE_AARCH64);
    set_feature(&cpu->env, ARM_FEATURE_CBAR_RO);
    set_feature(&cpu->env, ARM_FEATURE_EL2);
    set_feature(&cpu->env, ARM_FEATURE_EL3);
    set_feature(&cpu->env, ARM_FEATURE_PMU);
    cpu->kvm_target = QEMU_KVM_ARM_TARGET_CORTEX_A53;
    cpu->midr = 0x410fd034;
    cpu->revidr = 0x00000000;
    cpu->reset_fpsid = 0x41034070;
    cpu->isar.regs[MVFR0] = 0x10110222;
    cpu->isar.regs[MVFR1] = 0x12111111;
    cpu->isar.regs[MVFR2] = 0x00000043;
    cpu->ctr = 0x84448004; /* L1Ip = VIPT */
    cpu->reset_sctlr = 0x00c50838;
    cpu->id_pfr0 = 0x00000131;
    cpu->id_pfr1 = 0x00011011;
    cpu->isar.regs[ID_DFR0] = 0x03010066;
    cpu->id_afr0 = 0x00000000;
    cpu->isar.regs[ID_MMFR0] = 0x10101105;
    cpu->isar.regs[ID_MMFR1] = 0x40000000;
    cpu->isar.regs[ID_MMFR2] = 0x01260000;
    cpu->isar.regs[ID_MMFR3] = 0x02102211;
    cpu->isar.regs[ID_ISAR0] = 0x02101110;
    cpu->isar.regs[ID_ISAR1] = 0x13112111;
    cpu->isar.regs[ID_ISAR2] = 0x21232042;
    cpu->isar.regs[ID_ISAR3] = 0x01112131;
    cpu->isar.regs[ID_ISAR4] = 0x00011142;
    cpu->isar.regs[ID_ISAR5] = 0x00011121;
    cpu->isar.regs[ID_ISAR6] = 0;
    cpu->isar.regs[ID_AA64PFR0] = 0x00002222;
    cpu->isar.regs[ID_AA64DFR0] = 0x10305106;
    cpu->isar.regs[ID_AA64ISAR0] = 0x00011120;
    cpu->isar.regs[ID_AA64MMFR0] = 0x00001122; /* 40 bit physical addr */
    cpu->isar.regs[DBGDIDR] = 0x3516d000;
    cpu->clidr = 0x0a200023;
    cpu->ccsidr[0] = 0x700fe01a; /* 32KB L1 dcache */
    cpu->ccsidr[1] = 0x201fe00a; /* 32KB L1 icache */
    cpu->ccsidr[2] = 0x707fe07a; /* 1024KB L2 cache */
    cpu->dcz_blocksize = 4; /* 64 bytes */
    cpu->gic_num_lrs = 4;
    cpu->gic_vpribits = 5;
    cpu->gic_vprebits = 5;
    define_arm_cp_regs(cpu, cortex_a72_a57_a53_cp_reginfo);
}

static void aarch64_a72_initfn(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);

    cpu->dtb_compatible = "arm,cortex-a72";
    cpu->kvm_target = QEMU_KVM_ARM_TARGET_GENERIC_V8;
    set_feature(&cpu->env, ARM_FEATURE_V8);
    set_feature(&cpu->env, ARM_FEATURE_VFP4);
    set_feature(&cpu->env, ARM_FEATURE_NEON);
    set_feature(&cpu->env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(&cpu->env, ARM_FEATURE_AARCH64);
    set_feature(&cpu->env, ARM_FEATURE_CBAR_RO);
    set_feature(&cpu->env, ARM_FEATURE_EL2);
    set_feature(&cpu->env, ARM_FEATURE_EL3);
    set_feature(&cpu->env, ARM_FEATURE_PMU);
    cpu->midr = 0x410fd083;
    cpu->revidr = 0x00000000;
    cpu->reset_fpsid = 0x41034080;
    cpu->isar.regs[MVFR0] = 0x10110222;
    cpu->isar.regs[MVFR1] = 0x12111111;
    cpu->isar.regs[MVFR2] = 0x00000043;
    cpu->ctr = 0x8444c004;
    cpu->reset_sctlr = 0x00c50838;
    cpu->id_pfr0 = 0x00000131;
    cpu->id_pfr1 = 0x00011011;
    cpu->isar.regs[ID_DFR0] = 0x03010066;
    cpu->id_afr0 = 0x00000000;
    cpu->isar.regs[ID_MMFR0] = 0x10201105;
    cpu->isar.regs[ID_MMFR1] = 0x40000000;
    cpu->isar.regs[ID_MMFR2] = 0x01260000;
    cpu->isar.regs[ID_MMFR3] = 0x02102211;
    cpu->isar.regs[ID_ISAR0] = 0x02101110;
    cpu->isar.regs[ID_ISAR1] = 0x13112111;
    cpu->isar.regs[ID_ISAR2] = 0x21232042;
    cpu->isar.regs[ID_ISAR3] = 0x01112131;
    cpu->isar.regs[ID_ISAR4] = 0x00011142;
    cpu->isar.regs[ID_ISAR5] = 0x00011121;
    cpu->isar.regs[ID_AA64PFR0] = 0x00002222;
    cpu->isar.regs[ID_AA64DFR0] = 0x10305106;
    cpu->isar.regs[ID_AA64ISAR0] = 0x00011120;
    cpu->isar.regs[ID_AA64MMFR0] = 0x00001124;
    cpu->isar.regs[DBGDIDR] = 0x3516d000;
    cpu->clidr = 0x0a200023;
    cpu->ccsidr[0] = 0x701fe00a; /* 32KB L1 dcache */
    cpu->ccsidr[1] = 0x201fe012; /* 48KB L1 icache */
    cpu->ccsidr[2] = 0x707fe07a; /* 1MB L2 cache */
    cpu->dcz_blocksize = 4; /* 64 bytes */
    cpu->gic_num_lrs = 4;
    cpu->gic_vpribits = 5;
    cpu->gic_vprebits = 5;
    define_arm_cp_regs(cpu, cortex_a72_a57_a53_cp_reginfo);
    if(kvm_enabled()) {
        kvm_arm_add_vcpu_properties(obj);
    }
}

static void aarch64_kunpeng_920_initfn(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);

    /*
     * Hisilicon Kunpeng-920 CPU is similar to cortex-a72,
     * so first initialize cpu data as cortex-a72 CPU,
     * and then update the special registers.
     */
    aarch64_a72_initfn(obj);

    cpu->midr = 0x480fd010;
    cpu->ctr = 0x84448004;
    cpu->isar.regs[ID_ISAR0] = 0;
    cpu->isar.regs[ID_ISAR1] = 0;
    cpu->isar.regs[ID_ISAR2] = 0;
    cpu->isar.regs[ID_ISAR3] = 0;
    cpu->isar.regs[ID_ISAR4] = 0;
    cpu->isar.regs[ID_ISAR5] = 0;
    cpu->isar.regs[ID_MMFR0] = 0;
    cpu->isar.regs[ID_MMFR1] = 0;
    cpu->isar.regs[ID_MMFR2] = 0;
    cpu->isar.regs[ID_MMFR3] = 0;
    cpu->isar.regs[ID_MMFR4] = 0;
    cpu->isar.regs[MVFR0] = 0;
    cpu->isar.regs[MVFR1] = 0;
    cpu->isar.regs[MVFR2] = 0;
    cpu->isar.regs[ID_DFR0] = 0;
    cpu->isar.regs[MVFR2] = 0;
    cpu->isar.regs[MVFR2] = 0;
    cpu->isar.regs[MVFR2] = 0;
    cpu->id_pfr0 = 0;
    cpu->id_pfr1 = 0;
    cpu->isar.regs[ID_AA64PFR0] = 0x0000010011111111;
    cpu->isar.regs[ID_AA64DFR0] = 0x110305408;
    cpu->isar.regs[ID_AA64ISAR0] = 0x0001100010211120;
    cpu->isar.regs[ID_AA64ISAR1] = 0x00011001;
    cpu->isar.regs[ID_AA64MMFR0] = 0x101125;
    cpu->isar.regs[ID_AA64MMFR1] = 0x10211122;
    cpu->isar.regs[ID_AA64MMFR2] = 0x00001011;
}

static void cpu_max_get_sve_vq(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    ARMCPU *cpu = ARM_CPU(obj);
    visit_type_uint32(v, name, &cpu->sve_max_vq, errp);
}

static void cpu_max_set_sve_vq(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    ARMCPU *cpu = ARM_CPU(obj);
    Error *err = NULL;

    visit_type_uint32(v, name, &cpu->sve_max_vq, &err);

    if (!err && (cpu->sve_max_vq == 0 || cpu->sve_max_vq > ARM_MAX_VQ)) {
        error_setg(&err, "unsupported SVE vector length");
        error_append_hint(&err, "Valid sve-max-vq in range [1-%d]\n",
                          ARM_MAX_VQ);
    }
    error_propagate(errp, err);
}

/* -cpu max: if KVM is enabled, like -cpu host (best possible with this host);
 * otherwise, a CPU with as many features enabled as our emulation supports.
 * The version of '-cpu max' for qemu-system-arm is defined in cpu.c;
 * this only needs to handle 64 bits.
 */
static void aarch64_max_initfn(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);

    if (kvm_enabled()) {
        kvm_arm_set_cpu_features_from_host(cpu);
        kvm_arm_add_vcpu_properties(obj);
    } else {
        uint64_t t;
        uint32_t u;
        aarch64_a57_initfn(obj);

        t = cpu->isar.regs[ID_AA64ISAR0];
        t = FIELD_DP64(t, ID_AA64ISAR0, AES, 2); /* AES + PMULL */
        t = FIELD_DP64(t, ID_AA64ISAR0, SHA1, 1);
        t = FIELD_DP64(t, ID_AA64ISAR0, SHA2, 2); /* SHA512 */
        t = FIELD_DP64(t, ID_AA64ISAR0, CRC32, 1);
        t = FIELD_DP64(t, ID_AA64ISAR0, ATOMIC, 2);
        t = FIELD_DP64(t, ID_AA64ISAR0, RDM, 1);
        t = FIELD_DP64(t, ID_AA64ISAR0, SHA3, 1);
        t = FIELD_DP64(t, ID_AA64ISAR0, SM3, 1);
        t = FIELD_DP64(t, ID_AA64ISAR0, SM4, 1);
        t = FIELD_DP64(t, ID_AA64ISAR0, DP, 1);
        t = FIELD_DP64(t, ID_AA64ISAR0, FHM, 1);
        t = FIELD_DP64(t, ID_AA64ISAR0, TS, 2); /* v8.5-CondM */
        t = FIELD_DP64(t, ID_AA64ISAR0, RNDR, 1);
        cpu->isar.regs[ID_AA64ISAR0] = t;

        t = cpu->isar.regs[ID_AA64ISAR1];
        t = FIELD_DP64(t, ID_AA64ISAR1, JSCVT, 1);
        t = FIELD_DP64(t, ID_AA64ISAR1, FCMA, 1);
        t = FIELD_DP64(t, ID_AA64ISAR1, APA, 1); /* PAuth, architected only */
        t = FIELD_DP64(t, ID_AA64ISAR1, API, 0);
        t = FIELD_DP64(t, ID_AA64ISAR1, GPA, 1);
        t = FIELD_DP64(t, ID_AA64ISAR1, GPI, 0);
        t = FIELD_DP64(t, ID_AA64ISAR1, SB, 1);
        t = FIELD_DP64(t, ID_AA64ISAR1, SPECRES, 1);
        t = FIELD_DP64(t, ID_AA64ISAR1, FRINTTS, 1);
        cpu->isar.regs[ID_AA64ISAR1] = t;

        t = cpu->isar.regs[ID_AA64PFR0];
        t = FIELD_DP64(t, ID_AA64PFR0, SVE, 1);
        t = FIELD_DP64(t, ID_AA64PFR0, FP, 1);
        t = FIELD_DP64(t, ID_AA64PFR0, ADVSIMD, 1);
        cpu->isar.regs[ID_AA64PFR0] = t;

        t = cpu->isar.regs[ID_AA64PFR1];
        t = FIELD_DP64(t, ID_AA64PFR1, BT, 1);
        cpu->isar.regs[ID_AA64PFR1] = t;

        t = cpu->isar.regs[ID_AA64MMFR1];
        t = FIELD_DP64(t, ID_AA64MMFR1, HPDS, 1); /* HPD */
        t = FIELD_DP64(t, ID_AA64MMFR1, LO, 1);
        t = FIELD_DP64(t, ID_AA64MMFR1, PAN, 2); /* ATS1E1 */
        cpu->isar.regs[ID_AA64MMFR1] = t;

        /* Replicate the same data to the 32-bit id registers.  */
        u = cpu->isar.regs[ID_ISAR5];
        u = FIELD_DP32(u, ID_ISAR5, AES, 2); /* AES + PMULL */
        u = FIELD_DP32(u, ID_ISAR5, SHA1, 1);
        u = FIELD_DP32(u, ID_ISAR5, SHA2, 1);
        u = FIELD_DP32(u, ID_ISAR5, CRC32, 1);
        u = FIELD_DP32(u, ID_ISAR5, RDM, 1);
        u = FIELD_DP32(u, ID_ISAR5, VCMA, 1);
        cpu->isar.regs[ID_ISAR5] = u;

        u = cpu->isar.regs[ID_ISAR6];
        u = FIELD_DP32(u, ID_ISAR6, JSCVT, 1);
        u = FIELD_DP32(u, ID_ISAR6, DP, 1);
        u = FIELD_DP32(u, ID_ISAR6, FHM, 1);
        u = FIELD_DP32(u, ID_ISAR6, SB, 1);
        u = FIELD_DP32(u, ID_ISAR6, SPECRES, 1);
        cpu->isar.regs[ID_ISAR6] = u;

        u = cpu->isar.regs[ID_MMFR3];
        u = FIELD_DP32(u, ID_MMFR3, PAN, 2); /* ATS1E1 */
        cpu->isar.regs[ID_MMFR3] = u;

        /*
         * FIXME: We do not yet support ARMv8.2-fp16 for AArch32 yet,
         * so do not set MVFR1.FPHP.  Strictly speaking this is not legal,
         * but it is also not legal to enable SVE without support for FP16,
         * and enabling SVE in system mode is more useful in the short term.
         */

#ifdef CONFIG_USER_ONLY
        /* For usermode -cpu max we can use a larger and more efficient DCZ
         * blocksize since we don't have to follow what the hardware does.
         */
        cpu->ctr = 0x80038003; /* 32 byte I and D cacheline size, VIPT icache */
        cpu->dcz_blocksize = 7; /*  512 bytes */
#endif

        cpu->sve_max_vq = ARM_MAX_VQ;
        object_property_add(obj, "sve-max-vq", "uint32", cpu_max_get_sve_vq,
                            cpu_max_set_sve_vq, NULL, NULL, &error_fatal);
    }
}

struct ARMCPUInfo {
    const char *name;
    void (*initfn)(Object *obj);
    void (*class_init)(ObjectClass *oc, void *data);
};

static const ARMCPUInfo aarch64_cpus[] = {
    { .name = "cortex-a57",         .initfn = aarch64_a57_initfn },
    { .name = "cortex-a53",         .initfn = aarch64_a53_initfn },
    { .name = "cortex-a72",         .initfn = aarch64_a72_initfn },
    { .name = "Kunpeng-920",        .initfn = aarch64_kunpeng_920_initfn },
    { .name = "max",                .initfn = aarch64_max_initfn },
    { .name = NULL }
};

static bool aarch64_cpu_get_aarch64(Object *obj, Error **errp)
{
    ARMCPU *cpu = ARM_CPU(obj);

    return arm_feature(&cpu->env, ARM_FEATURE_AARCH64);
}

static void aarch64_cpu_set_aarch64(Object *obj, bool value, Error **errp)
{
    ARMCPU *cpu = ARM_CPU(obj);

    /* At this time, this property is only allowed if KVM is enabled.  This
     * restriction allows us to avoid fixing up functionality that assumes a
     * uniform execution state like do_interrupt.
     */
    if (!kvm_enabled()) {
        error_setg(errp, "'aarch64' feature cannot be disabled "
                         "unless KVM is enabled");
        return;
    }

    if (value == false) {
        unset_feature(&cpu->env, ARM_FEATURE_AARCH64);
    } else {
        set_feature(&cpu->env, ARM_FEATURE_AARCH64);
    }
}

static void aarch64_cpu_initfn(Object *obj)
{
    object_property_add_bool(obj, "aarch64", aarch64_cpu_get_aarch64,
                             aarch64_cpu_set_aarch64, NULL);
    object_property_set_description(obj, "aarch64",
                                    "Set on/off to enable/disable aarch64 "
                                    "execution state ",
                                    NULL);
}

static void aarch64_cpu_finalizefn(Object *obj)
{
}

static gchar *aarch64_gdb_arch_name(CPUState *cs)
{
    return g_strdup("aarch64");
}

/* Parse "+feature,-feature,feature=foo" CPU feature string
 */
static void arm_cpu_parse_featurestr(const char *typename, char *features,
                                     Error **errp)
{
    char *featurestr;
    char *val;
    static bool cpu_globals_initialized;

    if (cpu_globals_initialized) {
        return;
    }
    cpu_globals_initialized = true;

    featurestr = features ? strtok(features, ",") : NULL;
    while (featurestr) {
        val = strchr(featurestr, '=');
        if (val) {
            GlobalProperty *prop = g_new0(typeof(*prop), 1);
            *val = 0;
            val++;
            prop->driver = typename;
            prop->property = g_strdup(featurestr);
            prop->value = g_strdup(val);
            qdev_prop_register_global(prop);
        } else if (featurestr[0] == '+' || featurestr[0] == '-') {
            warn_report("Ignore %s feature\n", featurestr);
        } else {
            error_setg(errp, "Expected key=value format, found %s.",
                       featurestr);
            return;
        }
        featurestr = strtok(NULL, ",");
    }
}

static const char *unconfigurable_feats[] = {
    "evtstrm",
    "cpuid",
    NULL
};

static bool is_configurable_feat(const char *name)
{
    int i;

    for (i = 0; unconfigurable_feats[i]; ++i) {
        if (g_strcmp0(unconfigurable_feats[i], name) == 0) {
            return false;
        }
    }

    return true;
}

static void
cpu_add_feat_as_prop(const char *typename, const char *name, const char *val)
{
    GlobalProperty *prop;

    if (!is_configurable_feat(name)) {
        info_report("CPU feature '%s' is not configurable by QEMU.  Ignore it.",
                    name);
        return;
    }

    prop = g_new0(typeof(*prop), 1);
    prop->driver = typename;
    prop->property = g_strdup(name);
    prop->value = g_strdup(val);
    qdev_prop_register_global(prop);
}

static gint compare_string(gconstpointer a, gconstpointer b)
{
    return g_strcmp0(a, b);
}

static GList *plus_features, *minus_features;

static void aarch64_cpu_parse_features(const char *typename, char *features,
                                     Error **errp)
{
    GList *l;
    char *featurestr; /* Single 'key=value" string being parsed */
    static bool cpu_globals_initialized;

    if (cpu_globals_initialized) {
        return;
    }
    cpu_globals_initialized = true;

    if (!features) {
        return;
    }
    for (featurestr = strtok(features, ",");
        featurestr;
        featurestr = strtok(NULL, ",")) {
        const char *name;
        const char *val = NULL;
        char *eq = NULL;

        /* Compatibility syntax: */
        if (featurestr[0] == '+') {
            plus_features = g_list_append(plus_features,
                                          g_strdup(featurestr + 1));
            continue;
        } else if (featurestr[0] == '-') {
            minus_features = g_list_append(minus_features,
                                           g_strdup(featurestr + 1));
            continue;
        }

        eq = strchr(featurestr, '=');
        name = featurestr;
        if (eq) {
            *eq++ = 0;
            val = eq;
        } else {
            error_setg(errp, "Unsupported property format: %s", name);
            return;
        }

        if (g_list_find_custom(plus_features, name, compare_string)) {
            warn_report("Ambiguous CPU model string. "
                        "Don't mix both \"+%s\" and \"%s=%s\"",
                        name, name, val);
        }
        if (g_list_find_custom(minus_features, name, compare_string)) {
            warn_report("Ambiguous CPU model string. "
                        "Don't mix both \"-%s\" and \"%s=%s\"",
                        name, name, val);
        }
        cpu_add_feat_as_prop(typename, name, val);
    }

    for (l = plus_features; l; l = l->next) {
        cpu_add_feat_as_prop(typename, l->data, "on");
    }

    for (l = minus_features; l; l = l->next) {
        cpu_add_feat_as_prop(typename, l->data, "off");
    }
}

static void aarch64_cpu_class_init(ObjectClass *oc, void *data)
{
    CPUClass *cc = CPU_CLASS(oc);

    cc->parse_features = arm_cpu_parse_featurestr;
    cc->cpu_exec_interrupt = arm_cpu_exec_interrupt;
    cc->gdb_read_register = aarch64_cpu_gdb_read_register;
    cc->gdb_write_register = aarch64_cpu_gdb_write_register;
    cc->gdb_num_core_regs = 34;
    cc->gdb_core_xml_file = "aarch64-core.xml";
    cc->gdb_arch_name = aarch64_gdb_arch_name;
    cc->parse_features = aarch64_cpu_parse_features;
}

static void aarch64_cpu_instance_init(Object *obj)
{
    ARMCPUClass *acc = ARM_CPU_GET_CLASS(obj);

    acc->info->initfn(obj);
    arm_cpu_post_init(obj);
}

static void cpu_register_class_init(ObjectClass *oc, void *data)
{
    ARMCPUClass *acc = ARM_CPU_CLASS(oc);

    acc->info = data;
}

static void aarch64_cpu_register(const ARMCPUInfo *info)
{
    TypeInfo type_info = {
        .parent = TYPE_AARCH64_CPU,
        .instance_size = sizeof(ARMCPU),
        .instance_init = aarch64_cpu_instance_init,
        .class_size = sizeof(ARMCPUClass),
        .class_init = info->class_init ?: cpu_register_class_init,
        .class_data = (void *)info,
    };

    type_info.name = g_strdup_printf("%s-" TYPE_ARM_CPU, info->name);
    type_register(&type_info);
    g_free((void *)type_info.name);
}

static const TypeInfo aarch64_cpu_type_info = {
    .name = TYPE_AARCH64_CPU,
    .parent = TYPE_ARM_CPU,
    .instance_size = sizeof(ARMCPU),
    .instance_init = aarch64_cpu_initfn,
    .instance_finalize = aarch64_cpu_finalizefn,
    .abstract = true,
    .class_size = sizeof(AArch64CPUClass),
    .class_init = aarch64_cpu_class_init,
};

static void aarch64_cpu_register_types(void)
{
    const ARMCPUInfo *info = aarch64_cpus;

    type_register_static(&aarch64_cpu_type_info);

    while (info->name) {
        aarch64_cpu_register(info);
        info++;
    }
}

type_init(aarch64_cpu_register_types)
