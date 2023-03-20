/*
 * loongarch tlb emulation helpers for qemu.
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
#include "qemu/main-loop.h"
#include "cpu.h"
#include "internal.h"
#include "qemu/host-utils.h"
#include "exec/helper-proto.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "sysemu/kvm.h"
#include "hw/irq.h"
#include "cpu-csr.h"
#include "instmap.h"

#ifndef CONFIG_USER_ONLY
target_ulong helper_csr_rdq(CPULOONGARCHState *env, uint64_t csr)
{
    int64_t v;

#define CASE_CSR_RDQ(csr)                                                     \
    case LOONGARCH_CSR_##csr: {                                               \
        v = env->CSR_##csr;                                                   \
        break;                                                                \
    };

    switch (csr) {
    CASE_CSR_RDQ(CRMD)
    CASE_CSR_RDQ(PRMD)
    CASE_CSR_RDQ(EUEN)
    CASE_CSR_RDQ(MISC)
    CASE_CSR_RDQ(ECFG)
    CASE_CSR_RDQ(ESTAT)
    CASE_CSR_RDQ(ERA)
    CASE_CSR_RDQ(BADV)
    CASE_CSR_RDQ(BADI)
    CASE_CSR_RDQ(EEPN)
    CASE_CSR_RDQ(TLBIDX)
    CASE_CSR_RDQ(TLBEHI)
    CASE_CSR_RDQ(TLBELO0)
    CASE_CSR_RDQ(TLBELO1)
    CASE_CSR_RDQ(TLBWIRED)
    CASE_CSR_RDQ(GTLBC)
    CASE_CSR_RDQ(TRGP)
    CASE_CSR_RDQ(ASID)
    CASE_CSR_RDQ(PGDL)
    CASE_CSR_RDQ(PGDH)
    CASE_CSR_RDQ(PGD)
    CASE_CSR_RDQ(PWCTL0)
    CASE_CSR_RDQ(PWCTL1)
    CASE_CSR_RDQ(STLBPGSIZE)
    CASE_CSR_RDQ(RVACFG)
    CASE_CSR_RDQ(CPUID)
    CASE_CSR_RDQ(PRCFG1)
    CASE_CSR_RDQ(PRCFG2)
    CASE_CSR_RDQ(PRCFG3)
    CASE_CSR_RDQ(KS0)
    CASE_CSR_RDQ(KS1)
    CASE_CSR_RDQ(KS2)
    CASE_CSR_RDQ(KS3)
    CASE_CSR_RDQ(KS4)
    CASE_CSR_RDQ(KS5)
    CASE_CSR_RDQ(KS6)
    CASE_CSR_RDQ(KS7)
    CASE_CSR_RDQ(KS8)
    CASE_CSR_RDQ(TMID)
    CASE_CSR_RDQ(TCFG)
    case LOONGARCH_CSR_TVAL:
        v = cpu_loongarch_get_stable_timer_ticks(env);
        break;
    CASE_CSR_RDQ(CNTC)
    CASE_CSR_RDQ(TINTCLR)
    CASE_CSR_RDQ(GSTAT)
    CASE_CSR_RDQ(GCFG)
    CASE_CSR_RDQ(GINTC)
    CASE_CSR_RDQ(GCNTC)
    CASE_CSR_RDQ(LLBCTL)
    CASE_CSR_RDQ(IMPCTL1)
    CASE_CSR_RDQ(IMPCTL2)
    CASE_CSR_RDQ(GNMI)
    CASE_CSR_RDQ(TLBRENT)
    CASE_CSR_RDQ(TLBRBADV)
    CASE_CSR_RDQ(TLBRERA)
    CASE_CSR_RDQ(TLBRSAVE)
    CASE_CSR_RDQ(TLBRELO0)
    CASE_CSR_RDQ(TLBRELO1)
    CASE_CSR_RDQ(TLBREHI)
    CASE_CSR_RDQ(TLBRPRMD)
    CASE_CSR_RDQ(ERRCTL)
    CASE_CSR_RDQ(ERRINFO)
    CASE_CSR_RDQ(ERRINFO1)
    CASE_CSR_RDQ(ERRENT)
    CASE_CSR_RDQ(ERRERA)
    CASE_CSR_RDQ(ERRSAVE)
    CASE_CSR_RDQ(CTAG)
    CASE_CSR_RDQ(DMWIN0)
    CASE_CSR_RDQ(DMWIN1)
    CASE_CSR_RDQ(DMWIN2)
    CASE_CSR_RDQ(DMWIN3)
    CASE_CSR_RDQ(PERFCTRL0)
    CASE_CSR_RDQ(PERFCNTR0)
    CASE_CSR_RDQ(PERFCTRL1)
    CASE_CSR_RDQ(PERFCNTR1)
    CASE_CSR_RDQ(PERFCTRL2)
    CASE_CSR_RDQ(PERFCNTR2)
    CASE_CSR_RDQ(PERFCTRL3)
    CASE_CSR_RDQ(PERFCNTR3)
    /* debug */
    CASE_CSR_RDQ(MWPC)
    CASE_CSR_RDQ(MWPS)
    CASE_CSR_RDQ(DB0ADDR)
    CASE_CSR_RDQ(DB0MASK)
    CASE_CSR_RDQ(DB0CTL)
    CASE_CSR_RDQ(DB0ASID)
    CASE_CSR_RDQ(DB1ADDR)
    CASE_CSR_RDQ(DB1MASK)
    CASE_CSR_RDQ(DB1CTL)
    CASE_CSR_RDQ(DB1ASID)
    CASE_CSR_RDQ(DB2ADDR)
    CASE_CSR_RDQ(DB2MASK)
    CASE_CSR_RDQ(DB2CTL)
    CASE_CSR_RDQ(DB2ASID)
    CASE_CSR_RDQ(DB3ADDR)
    CASE_CSR_RDQ(DB3MASK)
    CASE_CSR_RDQ(DB3CTL)
    CASE_CSR_RDQ(DB3ASID)
    CASE_CSR_RDQ(FWPC)
    CASE_CSR_RDQ(FWPS)
    CASE_CSR_RDQ(IB0ADDR)
    CASE_CSR_RDQ(IB0MASK)
    CASE_CSR_RDQ(IB0CTL)
    CASE_CSR_RDQ(IB0ASID)
    CASE_CSR_RDQ(IB1ADDR)
    CASE_CSR_RDQ(IB1MASK)
    CASE_CSR_RDQ(IB1CTL)
    CASE_CSR_RDQ(IB1ASID)
    CASE_CSR_RDQ(IB2ADDR)
    CASE_CSR_RDQ(IB2MASK)
    CASE_CSR_RDQ(IB2CTL)
    CASE_CSR_RDQ(IB2ASID)
    CASE_CSR_RDQ(IB3ADDR)
    CASE_CSR_RDQ(IB3MASK)
    CASE_CSR_RDQ(IB3CTL)
    CASE_CSR_RDQ(IB3ASID)
    CASE_CSR_RDQ(IB4ADDR)
    CASE_CSR_RDQ(IB4MASK)
    CASE_CSR_RDQ(IB4CTL)
    CASE_CSR_RDQ(IB4ASID)
    CASE_CSR_RDQ(IB5ADDR)
    CASE_CSR_RDQ(IB5MASK)
    CASE_CSR_RDQ(IB5CTL)
    CASE_CSR_RDQ(IB5ASID)
    CASE_CSR_RDQ(IB6ADDR)
    CASE_CSR_RDQ(IB6MASK)
    CASE_CSR_RDQ(IB6CTL)
    CASE_CSR_RDQ(IB6ASID)
    CASE_CSR_RDQ(IB7ADDR)
    CASE_CSR_RDQ(IB7MASK)
    CASE_CSR_RDQ(IB7CTL)
    CASE_CSR_RDQ(IB7ASID)
    CASE_CSR_RDQ(DEBUG)
    CASE_CSR_RDQ(DERA)
    CASE_CSR_RDQ(DESAVE)
    default :
        assert(0);
    }

#undef CASE_CSR_RDQ
    compute_hflags(env);
    return v;
}

target_ulong helper_csr_wrq(CPULOONGARCHState *env, target_ulong val,
                            uint64_t csr)
{
    int64_t old_v, v;
    old_v = -1;
    v = val;

#define CASE_CSR_WRQ(csr)                                                     \
    case LOONGARCH_CSR_##csr: {                                               \
        old_v = env->CSR_##csr;                                               \
        env->CSR_##csr = v;                                                   \
        break;                                                                \
    };

    switch (csr) {
    CASE_CSR_WRQ(CRMD)
    CASE_CSR_WRQ(PRMD)
    CASE_CSR_WRQ(EUEN)
    CASE_CSR_WRQ(MISC)
    CASE_CSR_WRQ(ECFG)
    CASE_CSR_WRQ(ESTAT)
    CASE_CSR_WRQ(ERA)
    CASE_CSR_WRQ(BADV)
    CASE_CSR_WRQ(BADI)
    CASE_CSR_WRQ(EEPN)
    CASE_CSR_WRQ(TLBIDX)
    CASE_CSR_WRQ(TLBEHI)
    CASE_CSR_WRQ(TLBELO0)
    CASE_CSR_WRQ(TLBELO1)
    CASE_CSR_WRQ(TLBWIRED)
    CASE_CSR_WRQ(GTLBC)
    CASE_CSR_WRQ(TRGP)
    CASE_CSR_WRQ(ASID)
    CASE_CSR_WRQ(PGDL)
    CASE_CSR_WRQ(PGDH)
    CASE_CSR_WRQ(PGD)
    CASE_CSR_WRQ(PWCTL0)
    CASE_CSR_WRQ(PWCTL1)
    CASE_CSR_WRQ(STLBPGSIZE)
    CASE_CSR_WRQ(RVACFG)
    CASE_CSR_WRQ(CPUID)
    CASE_CSR_WRQ(PRCFG1)
    CASE_CSR_WRQ(PRCFG2)
    CASE_CSR_WRQ(PRCFG3)
    CASE_CSR_WRQ(KS0)
    CASE_CSR_WRQ(KS1)
    CASE_CSR_WRQ(KS2)
    CASE_CSR_WRQ(KS3)
    CASE_CSR_WRQ(KS4)
    CASE_CSR_WRQ(KS5)
    CASE_CSR_WRQ(KS6)
    CASE_CSR_WRQ(KS7)
    CASE_CSR_WRQ(KS8)
    CASE_CSR_WRQ(TMID)
    case LOONGARCH_CSR_TCFG:
        old_v = env->CSR_TCFG;
        cpu_loongarch_store_stable_timer_config(env, v);
        break;
    CASE_CSR_WRQ(TVAL)
    CASE_CSR_WRQ(CNTC)
    case LOONGARCH_CSR_TINTCLR:
        old_v = 0;
        qemu_irq_lower(env->irq[IRQ_TIMER]);
        break;
    CASE_CSR_WRQ(GSTAT)
    CASE_CSR_WRQ(GCFG)
    CASE_CSR_WRQ(GINTC)
    CASE_CSR_WRQ(GCNTC)
    CASE_CSR_WRQ(LLBCTL)
    CASE_CSR_WRQ(IMPCTL1)
    case LOONGARCH_CSR_IMPCTL2:
        if (v & CSR_IMPCTL2_MTLB) {
            ls3a5k_flush_vtlb(env);
        }
        if (v & CSR_IMPCTL2_STLB) {
            ls3a5k_flush_ftlb(env);
        }
        break;
    CASE_CSR_WRQ(GNMI)
    CASE_CSR_WRQ(TLBRENT)
    CASE_CSR_WRQ(TLBRBADV)
    CASE_CSR_WRQ(TLBRERA)
    CASE_CSR_WRQ(TLBRSAVE)
    CASE_CSR_WRQ(TLBRELO0)
    CASE_CSR_WRQ(TLBRELO1)
    CASE_CSR_WRQ(TLBREHI)
    CASE_CSR_WRQ(TLBRPRMD)
    CASE_CSR_WRQ(ERRCTL)
    CASE_CSR_WRQ(ERRINFO)
    CASE_CSR_WRQ(ERRINFO1)
    CASE_CSR_WRQ(ERRENT)
    CASE_CSR_WRQ(ERRERA)
    CASE_CSR_WRQ(ERRSAVE)
    CASE_CSR_WRQ(CTAG)
    CASE_CSR_WRQ(DMWIN0)
    CASE_CSR_WRQ(DMWIN1)
    CASE_CSR_WRQ(DMWIN2)
    CASE_CSR_WRQ(DMWIN3)
    CASE_CSR_WRQ(PERFCTRL0)
    CASE_CSR_WRQ(PERFCNTR0)
    CASE_CSR_WRQ(PERFCTRL1)
    CASE_CSR_WRQ(PERFCNTR1)
    CASE_CSR_WRQ(PERFCTRL2)
    CASE_CSR_WRQ(PERFCNTR2)
    CASE_CSR_WRQ(PERFCTRL3)
    CASE_CSR_WRQ(PERFCNTR3)
    /* debug */
    CASE_CSR_WRQ(MWPC)
    CASE_CSR_WRQ(MWPS)
    CASE_CSR_WRQ(DB0ADDR)
    CASE_CSR_WRQ(DB0MASK)
    CASE_CSR_WRQ(DB0CTL)
    CASE_CSR_WRQ(DB0ASID)
    CASE_CSR_WRQ(DB1ADDR)
    CASE_CSR_WRQ(DB1MASK)
    CASE_CSR_WRQ(DB1CTL)
    CASE_CSR_WRQ(DB1ASID)
    CASE_CSR_WRQ(DB2ADDR)
    CASE_CSR_WRQ(DB2MASK)
    CASE_CSR_WRQ(DB2CTL)
    CASE_CSR_WRQ(DB2ASID)
    CASE_CSR_WRQ(DB3ADDR)
    CASE_CSR_WRQ(DB3MASK)
    CASE_CSR_WRQ(DB3CTL)
    CASE_CSR_WRQ(DB3ASID)
    CASE_CSR_WRQ(FWPC)
    CASE_CSR_WRQ(FWPS)
    CASE_CSR_WRQ(IB0ADDR)
    CASE_CSR_WRQ(IB0MASK)
    CASE_CSR_WRQ(IB0CTL)
    CASE_CSR_WRQ(IB0ASID)
    CASE_CSR_WRQ(IB1ADDR)
    CASE_CSR_WRQ(IB1MASK)
    CASE_CSR_WRQ(IB1CTL)
    CASE_CSR_WRQ(IB1ASID)
    CASE_CSR_WRQ(IB2ADDR)
    CASE_CSR_WRQ(IB2MASK)
    CASE_CSR_WRQ(IB2CTL)
    CASE_CSR_WRQ(IB2ASID)
    CASE_CSR_WRQ(IB3ADDR)
    CASE_CSR_WRQ(IB3MASK)
    CASE_CSR_WRQ(IB3CTL)
    CASE_CSR_WRQ(IB3ASID)
    CASE_CSR_WRQ(IB4ADDR)
    CASE_CSR_WRQ(IB4MASK)
    CASE_CSR_WRQ(IB4CTL)
    CASE_CSR_WRQ(IB4ASID)
    CASE_CSR_WRQ(IB5ADDR)
    CASE_CSR_WRQ(IB5MASK)
    CASE_CSR_WRQ(IB5CTL)
    CASE_CSR_WRQ(IB5ASID)
    CASE_CSR_WRQ(IB6ADDR)
    CASE_CSR_WRQ(IB6MASK)
    CASE_CSR_WRQ(IB6CTL)
    CASE_CSR_WRQ(IB6ASID)
    CASE_CSR_WRQ(IB7ADDR)
    CASE_CSR_WRQ(IB7MASK)
    CASE_CSR_WRQ(IB7CTL)
    CASE_CSR_WRQ(IB7ASID)
    CASE_CSR_WRQ(DEBUG)
    CASE_CSR_WRQ(DERA)
    CASE_CSR_WRQ(DESAVE)
    default :
        assert(0);
    }

    if (csr == LOONGARCH_CSR_ASID) {
        if (old_v != v) {
            tlb_flush(CPU(loongarch_env_get_cpu(env)));
        }
    }

#undef CASE_CSR_WRQ
    compute_hflags(env);
    return old_v;
}

target_ulong helper_csr_xchgq(CPULOONGARCHState *env, target_ulong val,
                              target_ulong mask, uint64_t csr)
{
    target_ulong v, tmp;
    v = val & mask;

#define CASE_CSR_XCHGQ(csr)                                                   \
    case LOONGARCH_CSR_##csr: {                                               \
        val = env->CSR_##csr;                                                 \
        env->CSR_##csr = (env->CSR_##csr) & (~mask);                          \
        env->CSR_##csr = (env->CSR_##csr) | v;                                \
        break;                                                                \
    };

    switch (csr) {
    CASE_CSR_XCHGQ(CRMD)
    CASE_CSR_XCHGQ(PRMD)
    CASE_CSR_XCHGQ(EUEN)
    CASE_CSR_XCHGQ(MISC)
    CASE_CSR_XCHGQ(ECFG)
    case LOONGARCH_CSR_ESTAT:
        val = env->CSR_ESTAT;
        qatomic_and(&env->CSR_ESTAT, ~mask);
        qatomic_or(&env->CSR_ESTAT, v);
        break;
    CASE_CSR_XCHGQ(ERA)
    CASE_CSR_XCHGQ(BADV)
    CASE_CSR_XCHGQ(BADI)
    CASE_CSR_XCHGQ(EEPN)
    CASE_CSR_XCHGQ(TLBIDX)
    CASE_CSR_XCHGQ(TLBEHI)
    CASE_CSR_XCHGQ(TLBELO0)
    CASE_CSR_XCHGQ(TLBELO1)
    CASE_CSR_XCHGQ(TLBWIRED)
    CASE_CSR_XCHGQ(GTLBC)
    CASE_CSR_XCHGQ(TRGP)
    CASE_CSR_XCHGQ(ASID)
    CASE_CSR_XCHGQ(PGDL)
    CASE_CSR_XCHGQ(PGDH)
    CASE_CSR_XCHGQ(PGD)
    CASE_CSR_XCHGQ(PWCTL0)
    CASE_CSR_XCHGQ(PWCTL1)
    CASE_CSR_XCHGQ(STLBPGSIZE)
    CASE_CSR_XCHGQ(RVACFG)
    CASE_CSR_XCHGQ(CPUID)
    CASE_CSR_XCHGQ(PRCFG1)
    CASE_CSR_XCHGQ(PRCFG2)
    CASE_CSR_XCHGQ(PRCFG3)
    CASE_CSR_XCHGQ(KS0)
    CASE_CSR_XCHGQ(KS1)
    CASE_CSR_XCHGQ(KS2)
    CASE_CSR_XCHGQ(KS3)
    CASE_CSR_XCHGQ(KS4)
    CASE_CSR_XCHGQ(KS5)
    CASE_CSR_XCHGQ(KS6)
    CASE_CSR_XCHGQ(KS7)
    CASE_CSR_XCHGQ(KS8)
    CASE_CSR_XCHGQ(TMID)
    case LOONGARCH_CSR_TCFG:
        val = env->CSR_TCFG;
        tmp = val & ~mask;
        tmp |= v;
        cpu_loongarch_store_stable_timer_config(env, tmp);
        break;
    CASE_CSR_XCHGQ(TVAL)
    CASE_CSR_XCHGQ(CNTC)
    CASE_CSR_XCHGQ(TINTCLR)
    CASE_CSR_XCHGQ(GSTAT)
    CASE_CSR_XCHGQ(GCFG)
    CASE_CSR_XCHGQ(GINTC)
    CASE_CSR_XCHGQ(GCNTC)
    CASE_CSR_XCHGQ(LLBCTL)
    CASE_CSR_XCHGQ(IMPCTL1)
    CASE_CSR_XCHGQ(IMPCTL2)
    CASE_CSR_XCHGQ(GNMI)
    CASE_CSR_XCHGQ(TLBRENT)
    CASE_CSR_XCHGQ(TLBRBADV)
    CASE_CSR_XCHGQ(TLBRERA)
    CASE_CSR_XCHGQ(TLBRSAVE)
    CASE_CSR_XCHGQ(TLBRELO0)
    CASE_CSR_XCHGQ(TLBRELO1)
    CASE_CSR_XCHGQ(TLBREHI)
    CASE_CSR_XCHGQ(TLBRPRMD)
    CASE_CSR_XCHGQ(ERRCTL)
    CASE_CSR_XCHGQ(ERRINFO)
    CASE_CSR_XCHGQ(ERRINFO1)
    CASE_CSR_XCHGQ(ERRENT)
    CASE_CSR_XCHGQ(ERRERA)
    CASE_CSR_XCHGQ(ERRSAVE)
    CASE_CSR_XCHGQ(CTAG)
    CASE_CSR_XCHGQ(DMWIN0)
    CASE_CSR_XCHGQ(DMWIN1)
    CASE_CSR_XCHGQ(DMWIN2)
    CASE_CSR_XCHGQ(DMWIN3)
    CASE_CSR_XCHGQ(PERFCTRL0)
    CASE_CSR_XCHGQ(PERFCNTR0)
    CASE_CSR_XCHGQ(PERFCTRL1)
    CASE_CSR_XCHGQ(PERFCNTR1)
    CASE_CSR_XCHGQ(PERFCTRL2)
    CASE_CSR_XCHGQ(PERFCNTR2)
    CASE_CSR_XCHGQ(PERFCTRL3)
    CASE_CSR_XCHGQ(PERFCNTR3)
    /* debug */
    CASE_CSR_XCHGQ(MWPC)
    CASE_CSR_XCHGQ(MWPS)
    CASE_CSR_XCHGQ(DB0ADDR)
    CASE_CSR_XCHGQ(DB0MASK)
    CASE_CSR_XCHGQ(DB0CTL)
    CASE_CSR_XCHGQ(DB0ASID)
    CASE_CSR_XCHGQ(DB1ADDR)
    CASE_CSR_XCHGQ(DB1MASK)
    CASE_CSR_XCHGQ(DB1CTL)
    CASE_CSR_XCHGQ(DB1ASID)
    CASE_CSR_XCHGQ(DB2ADDR)
    CASE_CSR_XCHGQ(DB2MASK)
    CASE_CSR_XCHGQ(DB2CTL)
    CASE_CSR_XCHGQ(DB2ASID)
    CASE_CSR_XCHGQ(DB3ADDR)
    CASE_CSR_XCHGQ(DB3MASK)
    CASE_CSR_XCHGQ(DB3CTL)
    CASE_CSR_XCHGQ(DB3ASID)
    CASE_CSR_XCHGQ(FWPC)
    CASE_CSR_XCHGQ(FWPS)
    CASE_CSR_XCHGQ(IB0ADDR)
    CASE_CSR_XCHGQ(IB0MASK)
    CASE_CSR_XCHGQ(IB0CTL)
    CASE_CSR_XCHGQ(IB0ASID)
    CASE_CSR_XCHGQ(IB1ADDR)
    CASE_CSR_XCHGQ(IB1MASK)
    CASE_CSR_XCHGQ(IB1CTL)
    CASE_CSR_XCHGQ(IB1ASID)
    CASE_CSR_XCHGQ(IB2ADDR)
    CASE_CSR_XCHGQ(IB2MASK)
    CASE_CSR_XCHGQ(IB2CTL)
    CASE_CSR_XCHGQ(IB2ASID)
    CASE_CSR_XCHGQ(IB3ADDR)
    CASE_CSR_XCHGQ(IB3MASK)
    CASE_CSR_XCHGQ(IB3CTL)
    CASE_CSR_XCHGQ(IB3ASID)
    CASE_CSR_XCHGQ(IB4ADDR)
    CASE_CSR_XCHGQ(IB4MASK)
    CASE_CSR_XCHGQ(IB4CTL)
    CASE_CSR_XCHGQ(IB4ASID)
    CASE_CSR_XCHGQ(IB5ADDR)
    CASE_CSR_XCHGQ(IB5MASK)
    CASE_CSR_XCHGQ(IB5CTL)
    CASE_CSR_XCHGQ(IB5ASID)
    CASE_CSR_XCHGQ(IB6ADDR)
    CASE_CSR_XCHGQ(IB6MASK)
    CASE_CSR_XCHGQ(IB6CTL)
    CASE_CSR_XCHGQ(IB6ASID)
    CASE_CSR_XCHGQ(IB7ADDR)
    CASE_CSR_XCHGQ(IB7MASK)
    CASE_CSR_XCHGQ(IB7CTL)
    CASE_CSR_XCHGQ(IB7ASID)
    CASE_CSR_XCHGQ(DEBUG)
    CASE_CSR_XCHGQ(DERA)
    CASE_CSR_XCHGQ(DESAVE)
    default :
        assert(0);
    }

#undef CASE_CSR_XCHGQ
    compute_hflags(env);
    return val;
}

static target_ulong confbus_addr(CPULOONGARCHState *env, int cpuid,
                                 target_ulong csr_addr)
{
    target_ulong addr;
    target_ulong node_addr;
    int cores_per_node = ((0x60018 >> 3) & 0xff) + 1;

    switch (cores_per_node) {
    case 4:
        assert(cpuid < 64);
        node_addr = ((target_ulong)(cpuid & 0x3c) << 42);
        break;
    case 8:
        assert(cpuid < 128);
        node_addr = ((target_ulong)(cpuid & 0x78) << 41) +
                    ((target_ulong)(cpuid & 0x4) << 14);
        break;
    case 16:
        assert(cpuid < 256);
        node_addr = ((target_ulong)(cpuid & 0xf0) << 40) +
                    ((target_ulong)(cpuid & 0xc) << 14);
        break;
    default:
        assert(0);
        break;
    }

    /*
     * per core address
     *0x10xx => ipi
     * 0x18xx => extioi isr
     */
    if (((csr_addr & 0xff00) == 0x1000)) {
        addr = (csr_addr & 0xff) + (target_ulong)(cpuid << 8);
        addr = 0x800000001f000000UL + addr;
        return addr;
    } else if ((csr_addr & 0xff00) == 0x1800) {
        addr = (csr_addr & 0xff) + ((target_ulong)(cpuid << 8));
        addr = 0x800000001f020000UL + addr;
        return addr;
    } else if ((csr_addr & 0xff00) >= 0x1400 && (csr_addr & 0xff00) < 0x1d00) {
        addr = 0x800000001f010000UL + ((csr_addr & 0xfff) - 0x400);
        return addr;
    } else if (csr_addr == 0x408) {
        addr = csr_addr;
    } else {
        addr = csr_addr + node_addr;
    }

    addr = 0x800000001fe00000UL + addr;
    return addr;
}

void helper_iocsr(CPULOONGARCHState *env, target_ulong r_addr,
                  target_ulong r_val, uint32_t op)
{
    target_ulong addr;
    target_ulong val = env->active_tc.gpr[r_val];
    int mask;

    addr = confbus_addr(env, CPU(loongarch_env_get_cpu(env))->cpu_index,
                        env->active_tc.gpr[r_addr]);

    switch (env->active_tc.gpr[r_addr]) {
    /* IPI send */
    case 0x1040:
        if (op != OPC_LARCH_ST_W) {
            return;
        }
        op = OPC_LARCH_ST_W;
        break;

    /* Mail send */
    case 0x1048:
        if (op != OPC_LARCH_ST_D) {
            return;
        }
        op = OPC_LARCH_ST_D;
        break;

    /* ANY send */
    case 0x1158:
        if (op != OPC_LARCH_ST_D) {
            return;
        }
        addr = confbus_addr(env, (val >> 16) & 0x3ff, val & 0xffff);
        mask = (val >> 27) & 0xf;
        val = (val >> 32);
        switch (mask) {
        case 0:
            op = OPC_LARCH_ST_W;
            break;
        case 0x7:
            op = OPC_LARCH_ST_B;
            addr += 3;
            val >>= 24;
            break;
        case 0xb:
            op = OPC_LARCH_ST_B;
            addr += 2;
            val >>= 16;
            break;
        case 0xd:
            op = OPC_LARCH_ST_B;
            addr += 1;
            val >>= 8;
            break;
        case 0xe:
            op = OPC_LARCH_ST_B;
            break;
        case 0xc:
            op = OPC_LARCH_ST_H;
            break;
        case 0x3:
            op = OPC_LARCH_ST_H;
            addr += 2;
            val >>= 16;
            break;
        default:
            qemu_log("Unsupported any_send mask0x%x\n", mask);
            break;
        }
        break;

    default:
        break;
    }

    switch (op) {
    case OPC_LARCH_LD_D:
        env->active_tc.gpr[r_val] = cpu_ldq_data_ra(env, addr, GETPC());
        break;
    case OPC_LARCH_LD_W:
        env->active_tc.gpr[r_val] = cpu_ldl_data_ra(env, addr, GETPC());
        break;
    case OPC_LARCH_LD_H:
        assert(0);
        break;
    case OPC_LARCH_LD_B:
        assert(0);
        break;
    case OPC_LARCH_ST_D:
        cpu_stq_data_ra(env, addr, val, GETPC());
        break;
    case OPC_LARCH_ST_W:
        cpu_stl_data_ra(env, addr, val, GETPC());
        break;
    case OPC_LARCH_ST_H:
        cpu_stb_data_ra(env, addr, val, GETPC());
        break;
    case OPC_LARCH_ST_B:
        cpu_stb_data_ra(env, addr, val, GETPC());
        break;
    default:
        qemu_log("Unknown op 0x%x", op);
        assert(0);
    }
}
#endif

target_ulong helper_cpucfg(CPULOONGARCHState *env, target_ulong rj)
{
    return 0;
}
