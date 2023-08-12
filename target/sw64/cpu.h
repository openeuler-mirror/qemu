/*
 *  SW64 emulation cpu definitions for qemu.
 *
 *  Copyright (c) 2018 Lin Hainan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */
#ifndef SW64_CPU_H
#define SW64_CPU_H

#include "cpu-qom.h"
#include "fpu/softfloat.h"
#include "profile.h"

/* QEMU addressing/paging config */
#define TARGET_PAGE_BITS 13
#define TARGET_LONG_BITS 64
#define TARGET_LEVEL_BITS 10
//#define ALIGNED_ONLY

#include "exec/cpu-defs.h"

/* FIXME: LOCKFIX */
#define SW64_FIXLOCK 1

/* swcore processors have a weak memory model */
#define TCG_GUEST_DEFAULT_MO (0)

#define SOFTMMU 1

#ifndef CONFIG_USER_ONLY
#define MMU_MODE0_SUFFIX _phys
#define MMU_MODE3_SUFFIX _user
#define MMU_MODE2_SUFFIX _kernel
#endif
#define MMU_PHYS_IDX 0
#define MMU_KERNEL_IDX 2
#define MMU_USER_IDX 3

/* FIXME:Bits 4 and 5 are the mmu mode.  The VMS hmcode uses all 4 modes;
   The Unix hmcode only uses bit 4.  */
#define PS_USER_MODE 8u

#define ENV_FLAG_HM_SHIFT 0
#define ENV_FLAG_PS_SHIFT 8
#define ENV_FLAG_FEN_SHIFT 24

#define ENV_FLAG_HM_MODE (1u << ENV_FLAG_HM_SHIFT)
#define ENV_FLAG_PS_USER (PS_USER_MODE << ENV_FLAG_PS_SHIFT)
#define ENV_FLAG_FEN (1u << ENV_FLAG_FEN_SHIFT)

#define MCU_CLOCK 25000000

#define init_pc 0xffffffff80011100

typedef struct CPUSW64State CPUSW64State;
typedef CPUSW64State CPUArchState;
typedef SW64CPU ArchCPU;

struct CPUSW64State {
    uint64_t ir[32];
    uint64_t fr[128];
    uint64_t pc;
    bool is_slave;

    uint64_t csr[0x100];
    uint64_t fpcr;
    uint64_t fpcr_exc_enable;
    uint8_t fpcr_round_mode;
    uint8_t fpcr_flush_to_zero;

    float_status fp_status;

    uint64_t hm_entry;

#if !defined(CONFIG_USER_ONLY)
    uint64_t sr[10]; /* shadow regs 1,2,4-7,20-23 */
#endif

    uint32_t flags;
    uint64_t error_code;
    uint64_t unique;
    uint64_t lock_addr;
    uint64_t lock_valid;
    uint64_t lock_flag;
    uint64_t lock_success;
#ifdef SW64_FIXLOCK
    uint64_t lock_value;
#endif

    uint64_t trap_arg0;
    uint64_t trap_arg1;
    uint64_t trap_arg2;

    uint64_t features;
    uint64_t insn_count[537];

    /* reserve for slave */
    uint64_t ca[4];
    uint64_t scala_gpr[64];
    uint64_t vec_gpr[224];
    uint64_t fpcr_base;
    uint64_t fpcr_ext;
    uint64_t pendding_flag;
    uint64_t pendding_status;
    uint64_t synr_pendding_status;
    uint64_t sync_pendding_status;
    uint8_t vlenma_idxa;
    uint8_t stable;
};
#define SW64_FEATURE_CORE3  0x2

static inline void set_feature(CPUSW64State *env, int feature)
{
    env->features |= feature;
}

/**
 * SW64CPU:
 * @env: #CPUSW64State
 *
 * An SW64 CPU
 */
struct SW64CPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/
    CPUNegativeOffsetState neg;
    CPUSW64State env;

    uint64_t k_regs[158];
    uint64_t k_vcb[48];
    QEMUTimer *alarm_timer;
    target_ulong irq;
    uint32_t cid;
};

enum {
    IDX_V0 = 0,
    IDX_T0 = 1,
    IDX_T1 = 2,
    IDX_T2 = 3,
    IDX_T3 = 4,
    IDX_T4 = 5,
    IDX_T5 = 6,
    IDX_T6 = 7,
    IDX_T7 = 8,
    IDX_S0 = 9,
    IDX_S1 = 10,
    IDX_S2 = 11,
    IDX_S3 = 12,
    IDX_S4 = 13,
    IDX_S5 = 14,
    IDX_S6 = 15,
    IDX_FP = IDX_S6,
    IDX_A0 = 16,
    IDX_A1 = 17,
    IDX_A2 = 18,
    IDX_A3 = 19,
    IDX_A4 = 20,
    IDX_A5 = 21,
    IDX_T8 = 22,
    IDX_T9 = 23,
    IDX_T10 = 24,
    IDX_T11 = 25,
    IDX_RA = 26,
    IDX_T12 = 27,
    IDX_PV = IDX_T12,
    IDX_AT = 28,
    IDX_GP = 29,
    IDX_SP = 30,
    IDX_ZERO = 31,
};

enum {
    MM_K_TNV = 0x0,
    MM_K_ACV = 0x1,
    MM_K_FOR = 0x2,
    MM_K_FOE = 0x3,
    MM_K_FOW = 0x4
};

enum {
    PTE_VALID = 0x0001,
    PTE_FOR = 0x0002, /* used for page protection (fault on read) */
    PTE_FOW = 0x0004, /* used for page protection (fault on write) */
    PTE_FOE = 0x0008,
    PTE_KS = 0x0010,
    PTE_PSE = 0x0040,
    PTE_GH = 0x0060,
    PTE_HRE = 0x0100,
    PTE_VRE = 0x0200,
    PTE_KRE = 0x0400,
    PTE_URE = 0x0800,
    PTE_HWE = 0x1000,
    PTE_VWE = 0x2000,
    PTE_KWE = 0x4000,
    PTE_UWE = 0x8000
};

static inline int cpu_mmu_index(CPUSW64State *env, bool ifetch)
{
    int ret = env->flags & ENV_FLAG_PS_USER ? MMU_USER_IDX : MMU_KERNEL_IDX;
    if (env->flags & ENV_FLAG_HM_MODE) {
        ret = MMU_PHYS_IDX;
    }
    return ret;
}

static inline SW64CPU *sw64_env_get_cpu(CPUSW64State *env)
{
    return container_of(env, SW64CPU, env);
}

#define ENV_GET_CPU(e) CPU(sw64_env_get_cpu(e))
#define ENV_OFFSET offsetof(SW64CPU, env)

#define cpu_init(cpu_model) cpu_generic_init(TYPE_SW64_CPU, cpu_model)

#define SW64_CPU_TYPE_SUFFIX "-" TYPE_SW64_CPU
#define SW64_CPU_TYPE_NAME(name) (name SW64_CPU_TYPE_SUFFIX)
int cpu_sw64_signal_handler(int host_signum, void *pinfo, void *puc);
int sw64_cpu_gdb_read_register(CPUState *cs, uint8_t *buf, int reg);
int sw64_cpu_gdb_write_register(CPUState *cs, uint8_t *buf, int reg);
bool sw64_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
		      MMUAccessType access_type, int mmu_idx,
		      bool probe, uintptr_t retaddr);
uint64_t sw64_ldl_phys(CPUState *cs, hwaddr addr);
hwaddr sw64_cpu_get_phys_page_debug(CPUState *cs, vaddr addr);
void sw64_stl_phys(CPUState *cs, hwaddr addr, uint64_t val);
uint64_t sw64_ldw_phys(CPUState *cs, hwaddr addr);
void sw64_stw_phys(CPUState *cs, hwaddr addr, uint64_t val);
uint64_t cpu_sw64_load_fpcr(CPUSW64State *env);
#ifndef CONFIG_USER_ONLY
void sw64_cpu_do_interrupt(CPUState *cs);
bool sw64_cpu_exec_interrupt(CPUState *cpu, int int_req);
#endif
void cpu_sw64_store_fpcr(CPUSW64State *env, uint64_t val);
void sw64_cpu_do_unaligned_access(CPUState *cs, vaddr addr,
    MMUAccessType access_type, int mmu_idx,
    uintptr_t retaddr) QEMU_NORETURN;
bool sw64_cpu_has_work(CPUState *cs);
extern struct VMStateDescription vmstate_sw64_cpu;

/* SW64-specific interrupt pending bits */
#define CPU_INTERRUPT_TIMER CPU_INTERRUPT_TGT_EXT_0
#define CPU_INTERRUPT_II0 CPU_INTERRUPT_TGT_EXT_1
#define CPU_INTERRUPT_MCHK CPU_INTERRUPT_TGT_EXT_2
#define CPU_INTERRUPT_PCIE CPU_INTERRUPT_TGT_EXT_3
#define CPU_INTERRUPT_WAKEUP CPU_INTERRUPT_TGT_EXT_3
#define CPU_INTERRUPT_SLAVE CPU_INTERRUPT_TGT_EXT_4

#define cpu_signal_handler cpu_sw64_signal_handler
#define CPU_RESOLVING_TYPE TYPE_SW64_CPU

#define SWCSR(x, y) x = y
enum {
        SWCSR(ITB_TAG,          0x0),
        SWCSR(ITB_PTE,          0x1),
        SWCSR(ITB_IA,           0x2),
        SWCSR(ITB_IV,           0x3),
        SWCSR(ITB_IVP,          0x4),
        SWCSR(ITB_IU,           0x5),
        SWCSR(ITB_IS,           0x6),
        SWCSR(EXC_SUM,          0xd),
        SWCSR(EXC_PC,           0xe),
        SWCSR(DS_STAT,          0x48),
        SWCSR(CID,              0xc4),
        SWCSR(TID,              0xc7),

        SWCSR(DTB_TAG,          0x40),
        SWCSR(DTB_PTE,          0x41),
        SWCSR(DTB_IA,           0x42),
        SWCSR(DTB_IV,           0x43),
        SWCSR(DTB_IVP,          0x44),
        SWCSR(DTB_IU,           0x45),
        SWCSR(DTB_IS,           0x46),
        SWCSR(II_REQ,           0x82),

        SWCSR(PTBR,             0x8),
        SWCSR(PRI_BASE,         0x10),
        SWCSR(TIMER_CTL,        0x2a),
	SWCSR(TIMER_TH,      	0x2b),
        SWCSR(INT_STAT,         0x30),
        SWCSR(INT_CLR,          0x31),
        SWCSR(IER,              0x32),
        SWCSR(INT_PCI_INT,      0x33),
        SWCSR(DVA,              0x4e),
	SWCSR(SOFT_CID,         0xc9),
	SWCSR(SHTCLOCK,         0xca),
};

#include "exec/cpu-all.h"
static inline void cpu_get_tb_cpu_state(CPUSW64State *env, target_ulong *pc,
    target_ulong *cs_base, uint32_t *pflags)
{
    *pc = env->pc;
    *cs_base = 0;
    *pflags = env->flags;
}

void sw64_translate_init(void);

enum {
    EXCP_NONE,
    EXCP_HALT,
    EXCP_II0,
    EXCP_OPCDEC,
    EXCP_CALL_SYS,
    EXCP_ARITH,
    EXCP_UNALIGN,
#ifdef SOFTMMU
    EXCP_MMFAULT,
#else
    EXCP_DTBD,
    EXCP_DTBS_U,
    EXCP_DTBS_K,
    EXCP_ITB_U,
    EXCP_ITB_K,
#endif
    EXCP_CLK_INTERRUPT,
    EXCP_DEV_INTERRUPT,
    EXCP_SLAVE,
};

#define CSR_SHIFT_AND_MASK(name, func, shift, bits) \
    name##_##func##_S = shift,                      \
    name##_##func##_V = bits,                       \
    name##_##func##_M = (1UL << bits) - 1

#define FPCR_MASK(name) ((uint64_t)FPCR_##name##_M << FPCR_##name##_S)
/*  FPCR */
enum {
    CSR_SHIFT_AND_MASK(FPCR, EXC_CTL, 0, 2),
    CSR_SHIFT_AND_MASK(FPCR, EXC_CTL_WEN, 2, 1),
    CSR_SHIFT_AND_MASK(FPCR, RSV0, 3, 1),
    CSR_SHIFT_AND_MASK(FPCR, INV3, 4, 1),
    CSR_SHIFT_AND_MASK(FPCR, ZERO0, 5, 1),
    CSR_SHIFT_AND_MASK(FPCR, OVF3, 6, 1),
    CSR_SHIFT_AND_MASK(FPCR, UNF3, 7, 1),
    CSR_SHIFT_AND_MASK(FPCR, INE3, 8, 1),
    CSR_SHIFT_AND_MASK(FPCR, ZERO1, 9, 1),
    CSR_SHIFT_AND_MASK(FPCR, RSV1, 10, 10),
    CSR_SHIFT_AND_MASK(FPCR, INV2, 20, 1),
    CSR_SHIFT_AND_MASK(FPCR, ZERO2, 21, 1),
    CSR_SHIFT_AND_MASK(FPCR, OVF2, 22, 1),
    CSR_SHIFT_AND_MASK(FPCR, UNF2, 23, 1),
    CSR_SHIFT_AND_MASK(FPCR, INE2, 24, 1),
    CSR_SHIFT_AND_MASK(FPCR, ZERO3, 25, 1),
    CSR_SHIFT_AND_MASK(FPCR, RSV2, 26, 10),
    CSR_SHIFT_AND_MASK(FPCR, INV1, 36, 1),
    CSR_SHIFT_AND_MASK(FPCR, ZERO4, 37, 1),
    CSR_SHIFT_AND_MASK(FPCR, OVF1, 38, 1),
    CSR_SHIFT_AND_MASK(FPCR, UNF1, 39, 1),
    CSR_SHIFT_AND_MASK(FPCR, INE1, 40, 1),
    CSR_SHIFT_AND_MASK(FPCR, ZERO5, 41, 1),
    CSR_SHIFT_AND_MASK(FPCR, RSV3, 42, 6),
    CSR_SHIFT_AND_MASK(FPCR, DNZ, 48, 1),
    CSR_SHIFT_AND_MASK(FPCR, INVD, 49, 1),
    CSR_SHIFT_AND_MASK(FPCR, DZED, 50, 1),
    CSR_SHIFT_AND_MASK(FPCR, OVFD, 51, 1),
    CSR_SHIFT_AND_MASK(FPCR, INV0, 52, 1),
    CSR_SHIFT_AND_MASK(FPCR, DZE0, 53, 1),
    CSR_SHIFT_AND_MASK(FPCR, OVF0, 54, 1),
    CSR_SHIFT_AND_MASK(FPCR, UNF0, 55, 1),
    CSR_SHIFT_AND_MASK(FPCR, INE0, 56, 1),
    CSR_SHIFT_AND_MASK(FPCR, OVI0, 57, 1),
    CSR_SHIFT_AND_MASK(FPCR, DYN, 58, 2),
    CSR_SHIFT_AND_MASK(FPCR, UNDZ, 60, 1),
    CSR_SHIFT_AND_MASK(FPCR, UNFD, 61, 1),
    CSR_SHIFT_AND_MASK(FPCR, INED, 62, 1),
    CSR_SHIFT_AND_MASK(FPCR, SUM, 63, 1),
};

/* Arithmetic exception (entArith) constants.  */
#define EXC_M_SWC 1 /* Software completion */
#define EXC_M_INV 2 /* Invalid operation */
#define EXC_M_DZE 4 /* Division by zero */
#define EXC_M_OVF 8 /* Overflow */
#define EXC_M_UNF 16 /* Underflow */
#define EXC_M_INE 32 /* Inexact result */
#define EXC_M_IOV 64 /* Integer Overflow */
#define EXC_M_DNO 128 /* Denomal operation */

void QEMU_NORETURN dynamic_excp(CPUSW64State *env, uintptr_t retaddr, int excp,
    int error);
void QEMU_NORETURN arith_excp(CPUSW64State *env, uintptr_t retaddr, int exc,
    uint64_t mask);

#define DEBUG_ARCH
#ifdef DEBUG_ARCH
#define arch_assert(x)                                           \
    do {                                                         \
        g_assert(x); /*fprintf(stderr, "+6b %d\n", __LINE__); */ \
    } while (0)
#else
#define arch_assert(x)
#endif

typedef struct SW64CPUInfo {
    const char *name;
    void (*initfn)(Object *obj);
    void (*class_init)(ObjectClass *oc, void *data);
} SW64CPUInfo;
#define test_feature(env, x) (env->features & (x))

/* Slave */
#endif
