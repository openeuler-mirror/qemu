#ifndef SW64_TARGET_SYSCALL_H
#define SW64_TARGET_SYSCALL_H

/* TODO */
struct target_pt_regs {
    abi_ulong r0;
    abi_ulong r1;
    abi_ulong r2;
    abi_ulong r3;
    abi_ulong r4;
    abi_ulong r5;
    abi_ulong r6;
    abi_ulong r7;
    abi_ulong r8;
    abi_ulong r19;
    abi_ulong r20;
    abi_ulong r21;
    abi_ulong r22;
    abi_ulong r23;
    abi_ulong r24;
    abi_ulong r25;
    abi_ulong r26;
    abi_ulong r27;
    abi_ulong r28;
    abi_ulong hae;
/* JRP - These are the values provided to a0-a2 by hmcode */
    abi_ulong trap_a0;
    abi_ulong trap_a1;
    abi_ulong trap_a2;
/* These are saved by hmcode: */
    abi_ulong ps;
    abi_ulong pc;
    abi_ulong gp;
    abi_ulong r16;
    abi_ulong r17;
    abi_ulong r18;
/* Those is needed by qemu to temporary store the user stack pointer */
    abi_ulong usp;
    abi_ulong unique;
};


#define TARGET_MCL_CURRENT      0x2000
#define TARGET_MCL_FUTURE       0x4000
#define TARGET_MCL_ONFAULT      0x8000

#define UNAME_MACHINE "sw64"
#define UNAME_MINIMUM_RELEASE "2.6.32"
#undef TARGET_EOPNOTSUPP
#define TARGET_EOPNOTSUPP  45  /* Operation not supported on transport endpoint */
#define SWCR_STATUS_INV0        (1UL<<17)
#define SWCR_STATUS_DZE0        (1UL<<18)
#define SWCR_STATUS_OVF0        (1UL<<19)
#define SWCR_STATUS_UNF0        (1UL<<20)
#define SWCR_STATUS_INE0        (1UL<<21)
#define SWCR_STATUS_DNO0        (1UL<<22)

#define SWCR_STATUS_MASK0   (SWCR_STATUS_INV0 | SWCR_STATUS_DZE0 |  \
                 SWCR_STATUS_OVF0 | SWCR_STATUS_UNF0 |  \
                 SWCR_STATUS_INE0 | SWCR_STATUS_DNO0)

#define SWCR_STATUS0_TO_EXCSUM_SHIFT    16

#define SWCR_STATUS_INV1        (1UL<<23)
#define SWCR_STATUS_DZE1        (1UL<<24)
#define SWCR_STATUS_OVF1        (1UL<<25)
#define SWCR_STATUS_UNF1        (1UL<<26)
#define SWCR_STATUS_INE1        (1UL<<27)
#define SWCR_STATUS_DNO1        (1UL<<28)

#define SWCR_STATUS_MASK1   (SWCR_STATUS_INV1 | SWCR_STATUS_DZE1 |  \
                 SWCR_STATUS_OVF1 | SWCR_STATUS_UNF1 |  \
                 SWCR_STATUS_INE1 | SWCR_STATUS_DNO1)

#define SWCR_STATUS1_TO_EXCSUM_SHIFT    22
#define SWCR_STATUS_INV2        (1UL<<34)
#define SWCR_STATUS_DZE2        (1UL<<35)
#define SWCR_STATUS_OVF2        (1UL<<36)
#define SWCR_STATUS_UNF2        (1UL<<37)
#define SWCR_STATUS_INE2        (1UL<<38)
#define SWCR_STATUS_DNO2        (1UL<<39)

#define SWCR_STATUS_MASK2   (SWCR_STATUS_INV2 | SWCR_STATUS_DZE2 |  \
                 SWCR_STATUS_OVF2 | SWCR_STATUS_UNF2 |  \
                 SWCR_STATUS_INE2 | SWCR_STATUS_DNO2)
#define SWCR_STATUS_INV3        (1UL<<40)
#define SWCR_STATUS_DZE3        (1UL<<41)
#define SWCR_STATUS_OVF3        (1UL<<42)
#define SWCR_STATUS_UNF3        (1UL<<43)
#define SWCR_STATUS_INE3        (1UL<<44)
#define SWCR_STATUS_DNO3        (1UL<<45)

#define SWCR_STATUS_MASK3   (SWCR_STATUS_INV3 | SWCR_STATUS_DZE3 |  \
                 SWCR_STATUS_OVF3 | SWCR_STATUS_UNF3 |  \
                 SWCR_STATUS_INE3 | SWCR_STATUS_DNO3)
#define SWCR_TRAP_ENABLE_INV    (1UL<<1)    /* invalid op */
#define SWCR_TRAP_ENABLE_DZE    (1UL<<2)    /* division by zero */
#define SWCR_TRAP_ENABLE_OVF    (1UL<<3)    /* overflow */
#define SWCR_TRAP_ENABLE_UNF    (1UL<<4)    /* underflow */
#define SWCR_TRAP_ENABLE_INE    (1UL<<5)    /* inexact */
#define SWCR_TRAP_ENABLE_DNO    (1UL<<6)    /* denorm */
#define SWCR_TRAP_ENABLE_MASK   (SWCR_TRAP_ENABLE_INV | SWCR_TRAP_ENABLE_DZE | \
                 SWCR_TRAP_ENABLE_OVF | SWCR_TRAP_ENABLE_UNF | \
                 SWCR_TRAP_ENABLE_INE | SWCR_TRAP_ENABLE_DNO)

/* Denorm and Underflow flushing */
#define SWCR_MAP_DMZ        (1UL<<12)   /* Map denorm inputs to zero */
#define SWCR_MAP_UMZ        (1UL<<13)   /* Map underflowed outputs to zero */

#define SWCR_MAP_MASK       (SWCR_MAP_DMZ | SWCR_MAP_UMZ)

/* status bits coming from fpcr: */
#define SWCR_STATUS_INV     (1UL<<17)
#define SWCR_STATUS_DZE     (1UL<<18)
#define SWCR_STATUS_OVF     (1UL<<19)
#define SWCR_STATUS_UNF     (1UL<<20)
#define SWCR_STATUS_INE     (1UL<<21)
#define SWCR_STATUS_DNO     (1UL<<22)

#define SWCR_STATUS_MASK    (SWCR_STATUS_INV | SWCR_STATUS_DZE |    \
                 SWCR_STATUS_OVF | SWCR_STATUS_UNF |    \
                 SWCR_STATUS_INE | SWCR_STATUS_DNO)
#define TARGET_GSI_IEEE_FP_CONTROL	45
#define TARGET_SSI_IEEE_FP_CONTROL	14
#endif
