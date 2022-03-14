#ifndef SW64_TRANSLATE_H
#define SW64_TRANSLATE_H
#include "qemu/osdep.h"
#include "cpu.h"
#include "sysemu/cpus.h"
#include "disas/disas.h"
#include "qemu/host-utils.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "tcg/tcg-op.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"
#include "trace-tcg.h"
#include "exec/translator.h"
#include "exec/log.h"

#define DISAS_PC_UPDATED_NOCHAIN	DISAS_TARGET_0
#define DISAS_PC_UPDATED		DISAS_TARGET_1
#define DISAS_PC_STALE			DISAS_TARGET_2
#define DISAS_PC_UPDATED_T		DISAS_TOO_MANY

typedef struct DisasContext DisasContext;
struct DisasContext {
    DisasContextBase base;

    uint32_t tbflags;

    /* The set of registers active in the current context.  */
    TCGv *ir;

    /* Accel: Temporaries for $31 and $f31 as source and destination.  */
    TCGv zero;
    int mem_idx;
    CPUSW64State *env;
    DisasJumpType (*translate_one)(DisasContextBase *dcbase, uint32_t insn,
            CPUState *cpu);
};

extern TCGv cpu_pc;
extern TCGv cpu_std_ir[31];
extern TCGv cpu_fr[128];
extern TCGv cpu_lock_addr;
extern TCGv cpu_lock_flag;
extern TCGv cpu_lock_success;
#ifdef SW64_FIXLOCK
extern TCGv cpu_lock_value;
#endif
#ifndef CONFIG_USER_ONLY
extern TCGv cpu_hm_ir[31];
#endif

DisasJumpType translate_one(DisasContextBase *dcbase, uint32_t insn,
                            CPUState *cpu);
DisasJumpType th1_translate_one(DisasContextBase *dcbase, uint32_t insn,
                                   CPUState *cpu);
bool use_exit_tb(DisasContext *ctx);
bool use_goto_tb(DisasContext *ctx, uint64_t dest);
void insn_profile(DisasContext *ctx, uint32_t insn);
extern void gen_fold_mzero(TCGCond cond, TCGv dest, TCGv src);
#endif
