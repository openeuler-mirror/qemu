/*
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

#ifndef LOONGARCH_CPU_H
#define LOONGARCH_CPU_H

#define CPUArchState struct CPULOONGARCHState

#include "qemu-common.h"
#include "cpu-qom.h"
#include "larch-defs.h"
#include "exec/cpu-defs.h"
#include "fpu/softfloat.h"
#include "sysemu/sysemu.h"
#include "cpu-csr.h"

#define TCG_GUEST_DEFAULT_MO (0)

struct CPULOONGARCHState;
typedef LOONGARCHCPU ArchCPU;
typedef struct CPULOONGARCHTLBContext CPULOONGARCHTLBContext;

#define LASX_REG_WIDTH (256)
typedef union lasx_reg_t lasx_reg_t;
union lasx_reg_t
{
    int64_t val64[LASX_REG_WIDTH / 64];
};

typedef union fpr_t fpr_t;
union fpr_t
{
    float64 fd;    /* ieee double precision */
    float32 fs[2]; /* ieee single precision */
    uint64_t d;    /* binary double fixed-point */
    uint32_t w[2]; /* binary single fixed-point */
    /* FPU/LASX register mapping is not tested on big-endian hosts. */
    lasx_reg_t lasx; /* vector data */
};
/*
 * define FP_ENDIAN_IDX to access the same location
 * in the fpr_t union regardless of the host endianness
 */
#if defined(HOST_WORDS_BIGENDIAN)
#define FP_ENDIAN_IDX 1
#else
#define FP_ENDIAN_IDX 0
#endif

typedef struct CPULOONGARCHFPUContext {
    /* Floating point registers */
    fpr_t fpr[32];
    float_status fp_status;

    bool cf[8];
    /*
     * fcsr0
     * 31:29 |28:24 |23:21 |20:16 |15:10 |9:8 |7  |6  |5 |4:0
     *        Cause         Flags         RM   DAE TM     Enables
     */
    uint32_t fcsr0;
    uint32_t fcsr0_rw_bitmask;
    uint32_t vcsr16;
    uint64_t ftop;
} CPULOONGARCHFPUContext;

/* fp control and status register definition */
#define FCSR0_M1 0xdf       /* DAE, TM and Enables */
#define FCSR0_M2 0x1f1f0000 /* Cause and Flags */
#define FCSR0_M3 0x300      /* Round Mode */
#define FCSR0_RM 8          /* Round Mode bit num on fcsr0 */
#define GET_FP_CAUSE(reg) (((reg) >> 24) & 0x1f)
#define GET_FP_ENABLE(reg) (((reg) >> 0) & 0x1f)
#define GET_FP_FLAGS(reg) (((reg) >> 16) & 0x1f)
#define SET_FP_CAUSE(reg, v)                                                  \
    do {                                                                      \
        (reg) = ((reg) & ~(0x1f << 24)) | ((v & 0x1f) << 24);                 \
    } while (0)
#define SET_FP_ENABLE(reg, v)                                                 \
    do {                                                                      \
        (reg) = ((reg) & ~(0x1f << 0)) | ((v & 0x1f) << 0);                   \
    } while (0)
#define SET_FP_FLAGS(reg, v)                                                  \
    do {                                                                      \
        (reg) = ((reg) & ~(0x1f << 16)) | ((v & 0x1f) << 16);                 \
    } while (0)
#define UPDATE_FP_FLAGS(reg, v)                                               \
    do {                                                                      \
        (reg) |= ((v & 0x1f) << 16);                                          \
    } while (0)
#define FP_INEXACT 1
#define FP_UNDERFLOW 2
#define FP_OVERFLOW 4
#define FP_DIV0 8
#define FP_INVALID 16

#define TARGET_INSN_START_EXTRA_WORDS 2

typedef struct loongarch_def_t loongarch_def_t;

#define LOONGARCH_FPU_MAX 1
#define LOONGARCH_KSCRATCH_NUM 8

typedef struct TCState TCState;
struct TCState {
    target_ulong gpr[32];
    target_ulong PC;
};

#define N_IRQS 14
#define IRQ_TIMER 11
#define IRQ_IPI 12
#define IRQ_UART 2

typedef struct CPULOONGARCHState CPULOONGARCHState;
struct CPULOONGARCHState {
    TCState active_tc;
    CPULOONGARCHFPUContext active_fpu;

    uint32_t current_tc;
    uint64_t scr[4];
    uint32_t PABITS;

    /* LoongISA CSR register */
    CPU_LOONGARCH_CSR
    uint64_t lladdr;
    target_ulong llval;
    uint64_t llval_wp;
    uint32_t llnewval_wp;

    CPULOONGARCHFPUContext fpus[LOONGARCH_FPU_MAX];
    /* QEMU */
    int error_code;
#define EXCP_TLB_NOMATCH 0x1
#define EXCP_INST_NOTAVAIL 0x2 /* No valid instruction word for BadInstr */
    uint32_t hflags;           /* CPU State */
    /* TMASK defines different execution modes */
#define LARCH_HFLAG_TMASK 0x5F5807FF
    /*
     * The KSU flags must be the lowest bits in hflags. The flag order
     * must be the same as defined for CP0 Status. This allows to use
     * the bits as the value of mmu_idx.
     */
#define LARCH_HFLAG_KSU 0x00003   /* kernel/user mode mask   */
#define LARCH_HFLAG_UM 0x00003    /* user mode flag                     */
#define LARCH_HFLAG_KM 0x00000    /* kernel mode flag                   */
#define LARCH_HFLAG_64 0x00008    /* 64-bit instructions enabled        */
#define LARCH_HFLAG_FPU 0x00020   /* FPU enabled                        */
#define LARCH_HFLAG_AWRAP 0x00200 /* 32-bit compatibility address wrapping */
    /*
     * If translation is interrupted between the branch instruction and
     * the delay slot, record what type of branch it is so that we can
     * resume translation properly.  It might be possible to reduce
     * this from three bits to two.
     */
#define LARCH_HFLAG_BMASK 0x03800
#define LARCH_HFLAG_B 0x00800  /* Unconditional branch               */
#define LARCH_HFLAG_BC 0x01000 /* Conditional branch                 */
#define LARCH_HFLAG_BR 0x02000 /* branch to register (can't link TB) */
#define LARCH_HFLAG_LSX 0x1000000
#define LARCH_HFLAG_LASX 0x2000000
#define LARCH_HFLAG_LBT 0x40000000
    target_ulong btarget; /* Jump / branch target               */
    target_ulong bcond;   /* Branch condition (if needed)       */

    uint64_t insn_flags; /* Supported instruction set */
    int cpu_cfg[64];

    /* Fields up to this point are cleared by a CPU reset */
    struct {
    } end_reset_fields;

    /* Fields from here on are preserved across CPU reset. */
#if !defined(CONFIG_USER_ONLY)
    CPULOONGARCHTLBContext *tlb;
#endif

    const loongarch_def_t *cpu_model;
    void *irq[N_IRQS];
    QEMUTimer *timer;            /* Internal timer */
    MemoryRegion *itc_tag;       /* ITC Configuration Tags */
    target_ulong exception_base; /* ExceptionBase input to the core */
    struct {
        uint64_t guest_addr;
    } st;
    struct {
        /* scratch registers */
        unsigned long scr0;
        unsigned long scr1;
        unsigned long scr2;
        unsigned long scr3;
        /* loongarch eflag */
        unsigned long eflag;
    } lbt;
};

/*
 * CPU can't have 0xFFFFFFFF APIC ID, use that value to distinguish
 * that ID hasn't been set yet
 */
#define UNASSIGNED_CPU_ID 0xFFFFFFFF

/**
 * LOONGARCHCPU:
 * @env: #CPULOONGARCHState
 *
 * A LOONGARCH CPU.
 */
struct LOONGARCHCPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/
    CPUNegativeOffsetState neg;
    CPULOONGARCHState env;
    int32_t id;
    int hotplugged;
    uint8_t online_vcpus;
    uint8_t is_migrate;
    uint64_t counter_value;
    uint32_t cpu_freq;
    uint32_t count_ctl;
    uint64_t pending_exceptions;
    uint64_t pending_exceptions_clr;
    uint64_t core_ext_ioisr[4];
    VMChangeStateEntry *cpuStateEntry;
    int32_t node_id; /* NUMA node this CPU belongs to */
    int32_t core_id;
    struct kvm_msrs *kvm_csr_buf;
    /* 'compatible' string for this CPU for Linux device trees */
    const char *dtb_compatible;
};

static inline LOONGARCHCPU *loongarch_env_get_cpu(CPULOONGARCHState *env)
{
    return container_of(env, LOONGARCHCPU, env);
}

#define ENV_GET_CPU(e) CPU(loongarch_env_get_cpu(e))

#define ENV_OFFSET offsetof(LOONGARCHCPU, env)

void loongarch_cpu_list(void);

#define cpu_signal_handler cpu_loongarch_signal_handler
#define cpu_list loongarch_cpu_list

/*
 * MMU modes definitions. We carefully match the indices with our
 * hflags layout.
 */
#define MMU_MODE0_SUFFIX _kernel
#define MMU_MODE1_SUFFIX _super
#define MMU_MODE2_SUFFIX _user
#define MMU_MODE3_SUFFIX _error
#define MMU_USER_IDX 3

static inline int hflags_mmu_index(uint32_t hflags)
{
    return hflags & LARCH_HFLAG_KSU;
}

static inline int cpu_mmu_index(CPULOONGARCHState *env, bool ifetch)
{
    return hflags_mmu_index(env->hflags);
}

#include "exec/cpu-all.h"

/*
 * Memory access type :
 * may be needed for precise access rights control and precise exceptions.
 */
enum {
    /* 1 bit to define user level / supervisor access */
    ACCESS_USER = 0x00,
    ACCESS_SUPER = 0x01,
    /* 1 bit to indicate direction */
    ACCESS_STORE = 0x02,
    /* Type of instruction that generated the access */
    ACCESS_CODE = 0x10,  /* Code fetch access                */
    ACCESS_INT = 0x20,   /* Integer load/store access        */
    ACCESS_FLOAT = 0x30, /* floating point load/store access */
};

/* Exceptions */
enum {
    EXCP_NONE = -1,
    EXCP_RESET = 0,
    EXCP_SRESET,
    EXCP_DINT,
    EXCP_NMI,
    EXCP_EXT_INTERRUPT,
    EXCP_AdEL,
    EXCP_AdES,
    EXCP_TLBF,
    EXCP_IBE,
    EXCP_SYSCALL,
    EXCP_BREAK,
    EXCP_FPDIS,
    EXCP_LSXDIS,
    EXCP_LASXDIS,
    EXCP_RI,
    EXCP_OVERFLOW,
    EXCP_TRAP,
    EXCP_FPE,
    EXCP_LTLBL,
    EXCP_TLBL,
    EXCP_TLBS,
    EXCP_DBE,
    EXCP_TLBXI,
    EXCP_TLBRI,
    EXCP_TLBPE,
    EXCP_BTDIS,

    EXCP_LAST = EXCP_BTDIS,
};

/*
 * This is an internally generated WAKE request line.
 * It is driven by the CPU itself. Raised when the MT
 * block wants to wake a VPE from an inactive state and
 * cleared when VPE goes from active to inactive.
 */
#define CPU_INTERRUPT_WAKE CPU_INTERRUPT_TGT_INT_0

int cpu_loongarch_signal_handler(int host_signum, void *pinfo, void *puc);

#define LOONGARCH_CPU_TYPE_SUFFIX "-" TYPE_LOONGARCH_CPU
#define LOONGARCH_CPU_TYPE_NAME(model) model LOONGARCH_CPU_TYPE_SUFFIX
#define CPU_RESOLVING_TYPE TYPE_LOONGARCH_CPU

/* helper.c */
target_ulong exception_resume_pc(CPULOONGARCHState *env);

/* gdbstub.c */
void loongarch_cpu_register_gdb_regs_for_features(CPUState *cs);
void mmu_init(CPULOONGARCHState *env, const loongarch_def_t *def);

static inline void cpu_get_tb_cpu_state(CPULOONGARCHState *env,
                                        target_ulong *pc,
                                        target_ulong *cs_base, uint32_t *flags)
{
    *pc = env->active_tc.PC;
    *cs_base = 0;
    *flags = env->hflags & (LARCH_HFLAG_TMASK | LARCH_HFLAG_BMASK);
}

static inline bool cpu_refill_state(CPULOONGARCHState *env)
{
    return env->CSR_TLBRERA & 0x1;
}

extern const char *const regnames[];
extern const char *const fregnames[];
#endif /* LOONGARCH_CPU_H */
