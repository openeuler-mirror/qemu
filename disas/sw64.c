/*
 * sw_64-dis.c -- Disassemble Sw_64 CORE3 instructions
 *
 * This file is part of libopcodes.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * It is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file COPYING.  If not, write to the Free
 * Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "qemu/osdep.h"
#include "disas/dis-asm.h"

#undef MAX

struct sw_64_opcode {
    /* The opcode name. */
    const char *name;

    /* The opcode itself.  Those bits which will be filled in with
       operands are zeroes. */
    unsigned opcode;

    /* The opcode mask.  This is used by the disassembler.  This is a
       mask containing ones indicating those bits which must match the
       opcode field, and zeroes indicating those bits which need not
       match (and are presumably filled in by operands). */
    unsigned mask;

    /* One bit flags for the opcode.  These are primarily used to
       indicate specific processors and environments support the
       instructions.  The defined values are listed below. */
    unsigned flags;

    /* An array of operand codes.  Each code is an index into the
       operand table.  They appear in the order which the operands must
       appear in assembly code, and are terminated by a zero. */
    unsigned char operands[5];
};

/* The table itself is sorted by major opcode number, and is otherwise
   in the order in which the disassembler should consider
   instructions. */
extern const struct sw_64_opcode sw_64_opcodes[];
extern const unsigned sw_64_num_opcodes;

/* Values defined for the flags field of a struct sw_64_opcode. */

/* CPU Availability */
#define SW_OPCODE_BASE		0x0001  /* Base architecture insns. */
#define SW_OPCODE_CORE3	0x0002  /* Core3 private insns. */
#define SW_LITOP(i)		(((i) >> 26) & 0x3D)

#define SW_OPCODE_NOHMCODE      (~(SW_OPCODE_BASE|SW_OPCODE_CORE3))

/* A macro to extract the major opcode from an instruction. */
#define SW_OP(i)               (((i) >> 26) & 0x3F)

/* The total number of major opcodes. */
#define SW_NOPS                0x40

/* The operands table is an array of struct sw_64_operand. */

struct sw_64_operand {
    /* The number of bits in the operand. */
    unsigned int bits : 5;

    /* How far the operand is left shifted in the instruction. */
    unsigned int shift : 5;

    /* The default relocation type for this operand. */
    signed int default_reloc : 16;

    /* One bit syntax flags. */
    unsigned int flags : 16;

    /* Insertion function.  This is used by the assembler.  To insert an
       operand value into an instruction, check this field.

       If it is NULL, execute
       i |= (op & ((1 << o->bits) - 1)) << o->shift;
       (i is the instruction which we are filling in, o is a pointer to
       this structure, and op is the opcode value; this assumes twos
       complement arithmetic).

       If this field is not NULL, then simply call it with the
       instruction and the operand value.  It will return the new value
       of the instruction.  If the ERRMSG argument is not NULL, then if
       the operand value is illegal, *ERRMSG will be set to a warning
       string (the operand will be inserted in any case).  If the
       operand value is legal, *ERRMSG will be unchanged (most operands
       can accept any value). */
    unsigned (*insert) (unsigned instruction, int op, const char **errmsg);

    /* Extraction function.  This is used by the disassembler.  To
       extract this operand type from an instruction, check this field.

       If it is NULL, compute
       op = ((i) >> o->shift) & ((1 << o->bits) - 1);
       if ((o->flags & SW_OPERAND_SIGNED) != 0
       && (op & (1 << (o->bits - 1))) != 0)
       op -= 1 << o->bits;
       (i is the instruction, o is a pointer to this structure, and op
       is the result; this assumes twos complement arithmetic).

       If this field is not NULL, then simply call it with the
       instruction value.  It will return the value of the operand.  If
       the INVALID argument is not NULL, *INVALID will be set to
       non-zero if this operand type can not actually be extracted from
       this operand (i.e., the instruction does not match).  If the
       operand is valid, *INVALID will not be changed. */
    int (*extract) (unsigned instruction, int *invalid);
};

/* Elements in the table are retrieved by indexing with values from
   the operands field of the sw_64_opcodes table. */

extern const struct sw_64_operand sw_64_operands[];
extern const unsigned sw_64_num_operands;
/* Values defined for the flags field of a struct sw_64_operand. */

/* Mask for selecting the type for typecheck purposes */
#define SW_OPERAND_TYPECHECK_MASK					\
    (SW_OPERAND_PARENS | SW_OPERAND_COMMA | SW_OPERAND_IR |		\
     SW_OPERAND_FPR | SW_OPERAND_RELATIVE | SW_OPERAND_SIGNED | 	\
     SW_OPERAND_UNSIGNED)

/* This operand does not actually exist in the assembler input.  This
   is used to support extended mnemonics, for which two operands fields
   are identical.  The assembler should call the insert function with
   any op value.  The disassembler should call the extract function,
   ignore the return value, and check the value placed in the invalid
   argument. */
#define SW_OPERAND_FAKE                01

/* The operand should be wrapped in parentheses rather than separated
   from the previous by a comma.  This is used for the load and store
   instructions which want their operands to look like "Ra,disp(Rb)". */
#define SW_OPERAND_PARENS              02

/* Used in combination with PARENS, this supresses the supression of
   the comma.  This is used for "jmp Ra,(Rb),hint". */
#define SW_OPERAND_COMMA               04

/* This operand names an integer register. */
#define SW_OPERAND_IR                  010

/* This operand names a floating point register. */
#define SW_OPERAND_FPR                 020

/* This operand is a relative branch displacement.  The disassembler
   prints these symbolically if possible. */
#define SW_OPERAND_RELATIVE            040

/* This operand takes signed values. */
#define SW_OPERAND_SIGNED              0100

/* This operand takes unsigned values.  This exists primarily so that
   a flags value of 0 can be treated as end-of-arguments. */
#define SW_OPERAND_UNSIGNED            0200

/* Supress overflow detection on this field.  This is used for hints. */
#define SW_OPERAND_NOOVERFLOW          0400

/* Mask for optional argument default value. */
#define SW_OPERAND_OPTIONAL_MASK       07000

/* This operand defaults to zero.  This is used for jump hints. */
#define SW_OPERAND_DEFAULT_ZERO        01000

/* This operand should default to the first (real) operand and is used
   in conjunction with SW_OPERAND_OPTIONAL.  This allows
   "and $0,3,$0" to be written as "and $0,3", etc.  I don't like
   it, but it's what DEC does. */
#define SW_OPERAND_DEFAULT_FIRST       02000

/* Similarly, this operand should default to the second (real) operand.
   This allows "negl $0" instead of "negl $0,$0". */
#define SW_OPERAND_DEFAULT_SECOND      04000

/* Register common names */

#define SW_REG_V0	0
#define SW_REG_T0	1
#define SW_REG_T1	2
#define SW_REG_T2	3
#define SW_REG_T3	4
#define SW_REG_T4	5
#define SW_REG_T5	6
#define SW_REG_T6	7
#define SW_REG_T7	8
#define SW_REG_S0	9
#define SW_REG_S1	10
#define SW_REG_S2	11
#define SW_REG_S3	12
#define SW_REG_S4	13
#define SW_REG_S5	14
#define SW_REG_FP	15
#define SW_REG_A0	16
#define SW_REG_A1	17
#define SW_REG_A2	18
#define SW_REG_A3	19
#define SW_REG_A4	20
#define SW_REG_A5	21
#define SW_REG_T8	22
#define SW_REG_T9	23
#define SW_REG_T10	24
#define SW_REG_T11	25
#define SW_REG_RA	26
#define SW_REG_PV	27
#define SW_REG_T12	27
#define SW_REG_AT	28
#define SW_REG_GP	29
#define SW_REG_SP	30
#define SW_REG_ZERO	31

enum bfd_reloc_code_real {
    BFD_RELOC_23_PCREL_S2,
    BFD_RELOC_SW_64_HINT
};

static unsigned insert_rba(unsigned insn, int value ATTRIBUTE_UNUSED,
        const char **errmsg ATTRIBUTE_UNUSED)
{
    return insn | (((insn >> 21) & 0x1f) << 16);
}

static int extract_rba(unsigned insn, int *invalid)
{
    if (invalid != (int *) NULL
            && ((insn >> 21) & 0x1f) != ((insn >> 16) & 0x1f))
        *invalid = 1;
    return 0;
}

/* The same for the RC field.  */
static unsigned insert_rca(unsigned insn, int value ATTRIBUTE_UNUSED,
        const char **errmsg ATTRIBUTE_UNUSED)
{
    return insn | ((insn >> 21) & 0x1f);
}

static unsigned insert_rdc(unsigned insn, int value ATTRIBUTE_UNUSED,
        const char **errmsg ATTRIBUTE_UNUSED)
{
    return insn | ((insn >> 5) & 0x1f);
}

static int extract_rdc(unsigned insn, int *invalid)
{
    if (invalid != (int *) NULL
            && ((insn >> 5) & 0x1f) != (insn & 0x1f))
        *invalid = 1;
    return 0;
}

static int extract_rca(unsigned insn, int *invalid)
{
    if (invalid != (int *) NULL
            && ((insn >> 21) & 0x1f) != (insn & 0x1f))
        *invalid = 1;
    return 0;
}

/* Fake arguments in which the registers must be set to ZERO. */
static unsigned insert_za(unsigned insn, int value ATTRIBUTE_UNUSED,
        const char **errmsg ATTRIBUTE_UNUSED)
{
    return insn | (31 << 21);
}

static int extract_za(unsigned insn, int *invalid)
{
    if (invalid != (int *) NULL && ((insn >> 21) & 0x1f) != 31)
        *invalid = 1;
    return 0;
}

static unsigned insert_zb(unsigned insn, int value ATTRIBUTE_UNUSED,
        const char **errmsg ATTRIBUTE_UNUSED)
{
    return insn | (31 << 16);
}

static int extract_zb(unsigned insn, int *invalid)
{
    if (invalid != (int *) NULL && ((insn >> 16) & 0x1f) != 31)
        *invalid = 1;
    return 0;
}

static unsigned insert_zc(unsigned insn, int value ATTRIBUTE_UNUSED,
        const char **errmsg ATTRIBUTE_UNUSED)
{
    return insn | 31;
}

static int extract_zc(unsigned insn, int *invalid)
{
    if (invalid != (int *) NULL && (insn & 0x1f) != 31)
        *invalid = 1;
    return 0;
}


/* The displacement field of a Branch format insn. */

static unsigned insert_bdisp(unsigned insn, int value, const char **errmsg)
{
    if (errmsg != (const char **)NULL && (value & 3))
        *errmsg = "branch operand unaligned";
    return insn | ((value / 4) & 0x1FFFFF);
}

static int extract_bdisp(unsigned insn, int *invalid ATTRIBUTE_UNUSED)
{
    return 4 * (((insn & 0x1FFFFF) ^ 0x100000) - 0x100000);
}

/* The hint field of a JMP/JSR insn. */
/* sw use 16 bits hint disp. */
static unsigned insert_jhint(unsigned insn, int value, const char **errmsg)
{
    if (errmsg != (const char **)NULL && (value & 3))
        *errmsg = "jump hint unaligned";
    return insn | ((value / 4) & 0xFFFF);
}

static int extract_jhint(unsigned insn, int *invalid ATTRIBUTE_UNUSED)
{
    return 4 * (((insn & 0xFFFF) ^ 0x8000) - 0x8000);
}

/* The hint field of an CORE3 HW_JMP/JSR insn. */

static unsigned insert_sw4hwjhint(unsigned insn, int value, const char **errmsg)
{
    if (errmsg != (const char **)NULL && (value & 3))
        *errmsg = "jump hint unaligned";
    return insn | ((value / 4) & 0x1FFF);
}

static int extract_sw4hwjhint(unsigned insn, int *invalid ATTRIBUTE_UNUSED)
{
    return 4 * (((insn & 0x1FFF) ^ 0x1000) - 0x1000);
}

/* The operands table. */

const struct sw_64_operand sw_64_operands[] = {
    /* The fields are bits, shift, insert, extract, flags */
    /* The zero index is used to indicate end-of-list */
#define UNUSED		0
    { 0, 0, 0, 0, 0, 0 },

    /* The plain integer register fields. */
#define RA		(UNUSED + 1)
    { 5, 21, 0, SW_OPERAND_IR, 0, 0 },
#define RB		(RA + 1)
    { 5, 16, 0, SW_OPERAND_IR, 0, 0 },
#define RC		(RB + 1)
    { 5, 0, 0, SW_OPERAND_IR, 0, 0 },

    /* The plain fp register fields. */
#define FA		(RC + 1)
    { 5, 21, 0, SW_OPERAND_FPR, 0, 0 },
#define FB		(FA + 1)
    { 5, 16, 0, SW_OPERAND_FPR, 0, 0 },
#define FC		(FB + 1)
    { 5, 0, 0, SW_OPERAND_FPR, 0, 0 },

    /* The integer registers when they are ZERO. */
#define ZA		(FC + 1)
    { 5, 21, 0, SW_OPERAND_FAKE, insert_za, extract_za },
#define ZB		(ZA + 1)
    { 5, 16, 0, SW_OPERAND_FAKE, insert_zb, extract_zb },
#define ZC		(ZB + 1)
    { 5, 0, 0, SW_OPERAND_FAKE, insert_zc, extract_zc },

    /* The RB field when it needs parentheses. */
#define PRB		(ZC + 1)
    { 5, 16, 0, SW_OPERAND_IR | SW_OPERAND_PARENS, 0, 0 },

    /* The RB field when it needs parentheses _and_ a preceding comma. */
#define CPRB		(PRB + 1)
    { 5, 16, 0,
        SW_OPERAND_IR | SW_OPERAND_PARENS | SW_OPERAND_COMMA, 0, 0 },

    /* The RB field when it must be the same as the RA field. */
#define RBA		(CPRB + 1)
    { 5, 16, 0, SW_OPERAND_FAKE, insert_rba, extract_rba },

    /* The RC field when it must be the same as the RB field. */
#define RCA		(RBA + 1)
    { 5, 0, 0, SW_OPERAND_FAKE, insert_rca, extract_rca },

#define RDC            (RCA + 1)
    { 5, 0, 0, SW_OPERAND_FAKE, insert_rdc, extract_rdc },

    /* The RC field when it can *default* to RA. */
#define DRC1		(RDC + 1)
    { 5, 0, 0,
        SW_OPERAND_IR | SW_OPERAND_DEFAULT_FIRST, 0, 0 },

    /* The RC field when it can *default* to RB. */
#define DRC2		(DRC1 + 1)
    { 5, 0, 0,
        SW_OPERAND_IR | SW_OPERAND_DEFAULT_SECOND, 0, 0 },

    /* The FC field when it can *default* to RA. */
#define DFC1		(DRC2 + 1)
    { 5, 0, 0,
        SW_OPERAND_FPR | SW_OPERAND_DEFAULT_FIRST, 0, 0 },

    /* The FC field when it can *default* to RB. */
#define DFC2		(DFC1 + 1)
    { 5, 0, 0,
        SW_OPERAND_FPR | SW_OPERAND_DEFAULT_SECOND, 0, 0 },

    /* The unsigned 8-bit literal of Operate format insns. */
#define LIT		(DFC2 + 1)
    { 8, 13, -LIT, SW_OPERAND_UNSIGNED, 0, 0 },

    /* The signed 16-bit displacement of Memory format insns.  From here
       we can't tell what relocation should be used, so don't use a default. */
#define MDISP		(LIT + 1)
    { 16, 0, -MDISP, SW_OPERAND_SIGNED, 0, 0 },

    /* The signed "23-bit" aligned displacement of Branch format insns. */
#define BDISP		(MDISP + 1)
    { 21, 0, BFD_RELOC_23_PCREL_S2,
        SW_OPERAND_RELATIVE, insert_bdisp, extract_bdisp },

    /* The 26-bit hmcode function for sys_call and sys_call / b. */
#define HMFN		(BDISP + 1)
    { 25, 0, -HMFN, SW_OPERAND_UNSIGNED, 0, 0 },

    /* sw jsr/ret insntructions has no function bits. */
    /* The optional signed "16-bit" aligned displacement of the JMP/JSR hint. */
#define JMPHINT		(HMFN + 1)
    { 16, 0, BFD_RELOC_SW_64_HINT,
        SW_OPERAND_RELATIVE | SW_OPERAND_DEFAULT_ZERO | SW_OPERAND_NOOVERFLOW,
        insert_jhint, extract_jhint },

    /* The optional hint to RET/JSR_COROUTINE. */
#define RETHINT		(JMPHINT + 1)
    { 16, 0, -RETHINT,
        SW_OPERAND_UNSIGNED | SW_OPERAND_DEFAULT_ZERO, 0, 0 },

    /* The 12-bit displacement for the core3 hw_{ld,st} (pal1b/pal1f) insns. */
#define HWDISP		(RETHINT + 1)
    { 12, 0, -HWDISP, SW_OPERAND_SIGNED, 0, 0 },

    /* The 16-bit combined index/scoreboard mask for the core3
       hw_m[ft]pr (pal19/pal1d) insns. */
#define HWINDEX		(HWDISP + 1)
    { 16, 0, -HWINDEX, SW_OPERAND_UNSIGNED, 0, 0 },

    /* The 13-bit branch hint for the core3 hw_jmp/jsr (pal1e) insn. */

    /* for the third operand of ternary operands integer insn. */
#define R3              (HWINDEX + 1)
    { 5, 5, 0, SW_OPERAND_IR, 0, 0 },
    /* The plain fp register fields */
#define F3              (R3 + 1)
    { 5, 5, 0, SW_OPERAND_FPR, 0, 0 },
    /* sw simd settle instruction lit */
#define FMALIT          (F3 + 1)
    { 5,  5, -FMALIT, SW_OPERAND_UNSIGNED, 0, 0 },    //V1.1
#define RPIINDEX        (FMALIT + 1)
    { 8, 0, -RPIINDEX, SW_OPERAND_UNSIGNED, 0, 0 },
#define ATMDISP         (RPIINDEX + 1)
    { 12, 0, -ATMDISP, SW_OPERAND_SIGNED, 0, 0 },
};

const unsigned sw_64_num_operands = sizeof(sw_64_operands) / sizeof(*sw_64_operands);

/* Macros used to form opcodes. */

/* The main opcode. */
#define OP(x)           (((x) & 0x3F) << 26)
#define OP_MASK		0xFC000000

/* Branch format instructions. */
#define BRA_(oo)	OP(oo)
#define BRA_MASK	OP_MASK
#define BRA(oo)		BRA_(oo), BRA_MASK

#ifdef HUANGLM20171113
/* Floating point format instructions. */
#define FP_(oo,fff)	(OP(oo) | (((fff) & 0x7FF) << 5))
#define FP_MASK		(OP_MASK | 0xFFE0)
#define FP(oo,fff)	FP_(oo,fff), FP_MASK

#else
/* Floating point format instructions. */
#define FP_(oo,fff)	(OP(oo) | (((fff) & 0xFF) << 5))
#define FP_MASK		(OP_MASK | 0x1FE0)
#define FP(oo,fff)	FP_(oo,fff), FP_MASK

#define FMA_(oo,fff)    (OP(oo) | (((fff) & 0x3F) << 10 ))
#define FMA_MASK        (OP_MASK | 0xFC00)
#define FMA(oo,fff)     FMA_(oo,fff), FMA_MASK
#endif

/* Memory format instructions. */
#define MEM_(oo)	OP(oo)
#define MEM_MASK	OP_MASK
#define MEM(oo)		MEM_(oo), MEM_MASK

/* Memory/Func Code format instructions. */
#define MFC_(oo,ffff)	(OP(oo) | ((ffff) & 0xFFFF))
#define MFC_MASK	(OP_MASK | 0xFFFF)
#define MFC(oo,ffff)	MFC_(oo,ffff), MFC_MASK

/* Memory/Branch format instructions. */
#define MBR_(oo,h)	(OP(oo) | (((h) & 3) << 14))
#define MBR_MASK	(OP_MASK | 0xC000)
#define MBR(oo,h)       MBR_(oo,h), MBR_MASK

/* Now sw Operate format instructions is different with SW1. */
#define OPR_(oo,ff)	(OP(oo) | (((ff) & 0xFF) << 5))
#define OPRL_(oo,ff)	(OPR_((oo), (ff)) )
#define OPR_MASK	(OP_MASK | 0x1FE0)
#define OPR(oo,ff)	OPR_(oo,ff), OPR_MASK
#define OPRL(oo,ff)     OPRL_(oo,ff), OPR_MASK

/* sw ternary operands Operate format instructions. */
#define TOPR_(oo,ff)    (OP(oo) | (((ff) & 0x07) << 10))
#define TOPRL_(oo,ff)   (TOPR_((oo), (ff)))
#define TOPR_MASK       (OP_MASK | 0x1C00)
#define TOPR(oo,ff)     TOPR_(oo,ff), TOPR_MASK
#define TOPRL(oo,ff)    TOPRL_(oo,ff), TOPR_MASK

/* sw atom instructions. */
#define ATMEM_(oo,h)    (OP(oo) | (((h) & 0xF) << 12))
#define ATMEM_MASK      (OP_MASK | 0xF000)
#define ATMEM(oo,h)     ATMEM_(oo,h), ATMEM_MASK

/* sw privilege instructions. */
#define PRIRET_(oo,h)   (OP(oo) | (((h) & 0x1) << 20))
#define PRIRET_MASK     (OP_MASK | 0x100000)
#define PRIRET(oo,h)    PRIRET_(oo,h), PRIRET_MASK

/* sw pri_rcsr,pri_wcsr. */
#define CSR_(oo,ff)     (OP(oo) | (((ff) & 0xFF) << 8))
#define CSR_MASK        (OP_MASK | 0xFF00)
#define CSR(oo,ff)      CSR_(oo,ff), CSR_MASK

#define PCD_(oo,ff)     (OP(oo) | (ff << 25))
#define PCD_MASK        OP_MASK
#define PCD(oo,ff)      PCD_(oo,ff), PCD_MASK

/* Hardware memory (hw_{ld,st}) instructions. */
#define HWMEM_(oo,f)    (OP(oo) | (((f) & 0xF) << 12))
#define HWMEM_MASK      (OP_MASK | 0xF000)
#define HWMEM(oo,f)     HWMEM_(oo,f), HWMEM_MASK

#define LOGX_(oo,ff)    (OP(oo) | (((ff) & 0x3F) << 10))
#define LOGX_MASK       (0xF0000000)
#define LOGX(oo,ff)     LOGX_(oo,ff), LOGX_MASK

/* Abbreviations for instruction subsets. */
#define BASE            SW_OPCODE_BASE
#define CORE3           SW_OPCODE_CORE3

/* Common combinations of arguments. */
#define ARG_NONE        { 0 }
#define ARG_BRA         { RA, BDISP }
#define ARG_FBRA	{ FA, BDISP }
#define ARG_FP		{ FA, FB, DFC1 }
#define ARG_FPZ1	{ ZA, FB, DFC1 }
#define ARG_MEM		{ RA, MDISP, PRB }
#define ARG_FMEM	{ FA, MDISP, PRB }
#define ARG_OPR		{ RA, RB, DRC1 }

#define ARG_OPRL	{ RA, LIT, DRC1 }
#define ARG_OPRZ1	{ ZA, RB, DRC1 }
#define ARG_OPRLZ1	{ ZA, LIT, RC }
#define ARG_PCD		{ HMFN }
#define ARG_HWMEM       { RA, HWDISP, PRB }
#define ARG_FPL         { FA,LIT, DFC1 }
#define ARG_FMA         { FA,FB,F3, DFC1 }
#define ARG_PREFETCH    { ZA, MDISP, PRB }
#define ARG_TOPR        { RA, RB,R3, DRC1 }
#define ARG_TOPRL       { RA, LIT, R3,DRC1 }
#define ARG_FMAL        { FA,FB,FMALIT, DFC1 }
#define ARG_ATMEM       { RA, ATMDISP, PRB }
#define ARG_VUAMEM      { FA, ATMDISP, PRB }

/* The opcode table.

   The format of the opcode table is:

   NAME OPCODE MASK { OPERANDS }

   NAME		is the name of the instruction.

   OPCODE	is the instruction opcode.

   MASK		is the opcode mask; this is used to tell the disassembler
   which bits in the actual opcode must match OPCODE.

   OPERANDS	is the list of operands.

   The preceding macros merge the text of the OPCODE and MASK fields.

   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.

   Otherwise, it is sorted by major opcode and minor function code.
   */

const struct sw_64_opcode sw_64_opcodes[] = {
    { "sys_call/b",     PCD(0x00,0x00), BASE, ARG_PCD },
    { "sys_call",       PCD(0x00,0x01), BASE, ARG_PCD },

    { "call",           MEM(0x01), BASE, { RA, CPRB, JMPHINT } },
    { "ret",            MEM(0x02), BASE, { RA, CPRB, RETHINT } },
    { "jmp",            MEM(0x03), BASE, { RA, CPRB, JMPHINT } },
    { "br",             BRA(0x04), BASE, { ZA, BDISP } },
    { "br",             BRA(0x04), BASE, ARG_BRA },
    { "bsr",            BRA(0x05), BASE, { ZA, BDISP } },
    { "bsr",            BRA(0x05), BASE, ARG_BRA },
    { "memb",           MFC(0x06,0x0000), BASE, ARG_NONE },
    { "imemb",          MFC(0x06,0x0001), BASE, ARG_NONE },
    { "rtc",            MFC(0x06,0x0020), BASE, { RA, ZB } },
    { "rtc",            MFC(0x06,0x0020), BASE, { RA, RB } },
    { "rcid",           MFC(0x06,0x0040), BASE, { RA , ZB} },
    { "halt",           MFC(0x06,0x0080), BASE, { ZA, ZB } },
    { "rd_f",           MFC(0x06,0x1000), CORE3, { RA, ZB } },
    { "wr_f",           MFC(0x06,0x1020), CORE3, { RA, ZB } },
    { "rtid",           MFC(0x06,0x1040), BASE, { RA } },
    { "pri_rcsr",       CSR(0x06,0xFE), CORE3, { RA, RPIINDEX ,ZB } },
    { "pri_wcsr",       CSR(0x06,0xFF), CORE3, { RA, RPIINDEX ,ZB } },
    { "pri_ret",        PRIRET(0x07,0x0),   BASE,  { RA } },
    { "pri_ret/b",      PRIRET(0x07,0x1),   BASE,  { RA } },
    { "lldw",           ATMEM(0x08,0x0), BASE, ARG_ATMEM },
    { "lldl",           ATMEM(0x08,0x1), BASE, ARG_ATMEM },
    { "ldw_inc",        ATMEM(0x08,0x2), CORE3, ARG_ATMEM },
    { "ldl_inc",        ATMEM(0x08,0x3), CORE3, ARG_ATMEM },
    { "ldw_dec",        ATMEM(0x08,0x4), CORE3, ARG_ATMEM },
    { "ldl_dec",        ATMEM(0x08,0x5), CORE3, ARG_ATMEM },
    { "ldw_set",        ATMEM(0x08,0x6), CORE3, ARG_ATMEM },
    { "ldl_set",        ATMEM(0x08,0x7), CORE3, ARG_ATMEM },
    { "lstw",           ATMEM(0x08,0x8), BASE, ARG_ATMEM },
    { "lstl",           ATMEM(0x08,0x9), BASE, ARG_ATMEM },
    { "ldw_nc",         ATMEM(0x08,0xA), BASE, ARG_ATMEM },
    { "ldl_nc",         ATMEM(0x08,0xB), BASE, ARG_ATMEM },
    { "ldd_nc",         ATMEM(0x08,0xC), BASE, ARG_VUAMEM },
    { "stw_nc",         ATMEM(0x08,0xD), BASE, ARG_ATMEM },
    { "stl_nc",         ATMEM(0x08,0xE), BASE, ARG_ATMEM },
    { "std_nc",         ATMEM(0x08,0xF), BASE, ARG_VUAMEM },
    { "fillcs",         MEM(0x09), BASE, ARG_PREFETCH },
    { "ldwe",           MEM(0x09), BASE, ARG_FMEM },
    { "e_fillcs",       MEM(0x0A), BASE, ARG_PREFETCH },
    { "ldse",           MEM(0x0A), BASE, ARG_FMEM },
    { "fillcs_e",       MEM(0x0B), BASE, ARG_PREFETCH },
    { "ldde",           MEM(0x0B), BASE, ARG_FMEM },
    { "vlds",           MEM(0x0C), BASE, ARG_FMEM },
    { "vldd",           MEM(0x0D), BASE, ARG_FMEM },
    { "vsts",           MEM(0x0E), BASE, ARG_FMEM },
    { "vstd",           MEM(0x0F), BASE, ARG_FMEM },
    { "addw",           OPR(0x10,0x00), BASE, ARG_OPR },
    { "addw",           OPRL(0x12,0x00), BASE, ARG_OPRL },
    { "subw",           OPR(0x10,0x01), BASE, ARG_OPR },
    { "subw",           OPRL(0x12,0x01), BASE, ARG_OPRL },
    { "s4addw",         OPR(0x10,0x02), BASE, ARG_OPR },
    { "s4addw",         OPRL(0x12,0x02), BASE, ARG_OPRL },
    { "s4subw",         OPR(0x10,0x03), BASE, ARG_OPR },
    { "s4subw",         OPRL(0x12,0x03), BASE, ARG_OPRL },
    { "s8addw",         OPR(0x10,0x04), BASE, ARG_OPR },
    { "s8addw",         OPRL(0x12,0x04), BASE, ARG_OPRL },
    { "s8subw",         OPR(0x10,0x05), BASE, ARG_OPR },
    { "s8subw",         OPRL(0x12,0x05), BASE, ARG_OPRL },
    { "addl",           OPR(0x10,0x08), BASE, ARG_OPR },
    { "addl",           OPRL(0x12,0x08), BASE, ARG_OPRL },
    { "subl",           OPR(0x10,0x09), BASE, ARG_OPR },
    { "subl",           OPRL(0x12,0x09), BASE, ARG_OPRL },
    { "s4addl",         OPR(0x10,0x0A), BASE, ARG_OPR },
    { "s4addl",         OPRL(0x12,0x0A), BASE, ARG_OPRL },
    { "s4subl",         OPR(0x10,0x0B), BASE, ARG_OPR },
    { "s4subl",         OPRL(0x12,0x0B), BASE, ARG_OPRL },
    { "s8addl",         OPR(0x10,0x0C), BASE, ARG_OPR },
    { "s8addl",         OPRL(0x12,0x0C), BASE, ARG_OPRL },
    { "s8subl",         OPR(0x10,0x0D), BASE, ARG_OPR },
    { "s8subl",         OPRL(0x12,0x0D), BASE, ARG_OPRL },
    { "mulw",		    OPR(0x10,0x10), BASE, ARG_OPR },
    { "mulw",		    OPRL(0x12,0x10), BASE, ARG_OPRL },
    { "mull",		    OPR(0x10,0x18), BASE, ARG_OPR },
    { "mull",		    OPRL(0x12,0x18), BASE, ARG_OPRL },
    { "umulh",		    OPR(0x10,0x19), BASE, ARG_OPR },
    { "umulh",		    OPRL(0x12,0x19), BASE, ARG_OPRL },
    { "cmpeq",          OPR(0x10,0x28), BASE, ARG_OPR },
    { "cmpeq",          OPRL(0x12,0x28), BASE, ARG_OPRL },
    { "cmplt",          OPR(0x10,0x29), BASE, ARG_OPR },
    { "cmplt",          OPRL(0x12,0x29), BASE, ARG_OPRL },
    { "cmple",          OPR(0x10,0x2A), BASE, ARG_OPR },
    { "cmple",          OPRL(0x12,0x2A), BASE, ARG_OPRL },
    { "cmpult",         OPR(0x10,0x2B), BASE, ARG_OPR },
    { "cmpult",         OPRL(0x12,0x2B), BASE, ARG_OPRL },
    { "cmpule",         OPR(0x10,0x2C), BASE, ARG_OPR },
    { "cmpule",         OPRL(0x12,0x2C), BASE, ARG_OPRL },

    { "and",            OPR(0x10,0x38), BASE, ARG_OPR },
    { "and",            OPRL(0x12,0x38),BASE, ARG_OPRL },
    { "bic",            OPR(0x10,0x39), BASE, ARG_OPR },
    { "bic",            OPRL(0x12,0x39),BASE, ARG_OPRL },
    { "bis",            OPR(0x10,0x3A), BASE, ARG_OPR },
    { "bis",            OPRL(0x12,0x3A),BASE, ARG_OPRL },
    { "ornot",          OPR(0x10,0x3B), BASE, ARG_OPR },
    { "ornot",          OPRL(0x12,0x3B),BASE, ARG_OPRL },
    { "xor",            OPR(0x10,0x3C), BASE, ARG_OPR },
    { "xor",            OPRL(0x12,0x3C),BASE, ARG_OPRL },
    { "eqv",            OPR(0x10,0x3D), BASE, ARG_OPR },
    { "eqv",            OPRL(0x12,0x3D),BASE, ARG_OPRL },
    { "inslb",          OPR(0x10,0x40), BASE, ARG_OPR },
    { "inslb",          OPRL(0x12,0x40),BASE, ARG_OPRL },
    { "inslh",          OPR(0x10,0x41), BASE, ARG_OPR },
    { "inslh",          OPRL(0x12,0x41),BASE, ARG_OPRL },
    { "inslw",          OPR(0x10,0x42), BASE, ARG_OPR },
    { "inslw",          OPRL(0x12,0x42),BASE, ARG_OPRL },
    { "insll",          OPR(0x10,0x43), BASE, ARG_OPR },
    { "insll",          OPRL(0x12,0x43),BASE, ARG_OPRL },
    { "inshb",          OPR(0x10,0x44), BASE, ARG_OPR },
    { "inshb",          OPRL(0x12,0x44),BASE, ARG_OPRL },
    { "inshh",          OPR(0x10,0x45), BASE, ARG_OPR },
    { "inshh",          OPRL(0x12,0x45),BASE, ARG_OPRL },
    { "inshw",          OPR(0x10,0x46), BASE, ARG_OPR },
    { "inshw",          OPRL(0x12,0x46),BASE, ARG_OPRL },
    { "inshl",          OPR(0x10,0x47), BASE, ARG_OPR },
    { "inshl",          OPRL(0x12,0x47),BASE, ARG_OPRL },

    { "sll",            OPR(0x10,0x48), BASE, ARG_OPR },
    { "sll",            OPRL(0x12,0x48),BASE, ARG_OPRL },
    { "srl",            OPR(0x10,0x49), BASE, ARG_OPR },
    { "srl",            OPRL(0x12,0x49),BASE, ARG_OPRL },
    { "sra",            OPR(0x10,0x4A), BASE, ARG_OPR },
    { "sra",            OPRL(0x12,0x4A),BASE, ARG_OPRL },
    { "extlb",          OPR(0x10,0x50), BASE, ARG_OPR },
    { "extlb",          OPRL(0x12,0x50),BASE, ARG_OPRL },
    { "extlh",          OPR(0x10,0x51), BASE, ARG_OPR },
    { "extlh",          OPRL(0x12,0x51),BASE, ARG_OPRL },
    { "extlw",          OPR(0x10,0x52), BASE, ARG_OPR },
    { "extlw",          OPRL(0x12,0x52),BASE, ARG_OPRL },
    { "extll",          OPR(0x10,0x53), BASE, ARG_OPR },
    { "extll",          OPRL(0x12,0x53),BASE, ARG_OPRL },
    { "exthb",          OPR(0x10,0x54), BASE, ARG_OPR },
    { "exthb",          OPRL(0x12,0x54),BASE, ARG_OPRL },
    { "exthh",          OPR(0x10,0x55), BASE, ARG_OPR },
    { "exthh",          OPRL(0x12,0x55),BASE, ARG_OPRL },
    { "exthw",          OPR(0x10,0x56), BASE, ARG_OPR },
    { "exthw",          OPRL(0x12,0x56),BASE, ARG_OPRL },
    { "exthl",          OPR(0x10,0x57), BASE, ARG_OPR },
    { "exthl",          OPRL(0x12,0x57),BASE, ARG_OPRL },
    { "ctpop",          OPR(0x10,0x58), BASE, ARG_OPRZ1 },
    { "ctlz",           OPR(0x10,0x59), BASE, ARG_OPRZ1 },
    { "cttz",           OPR(0x10,0x5A), BASE, ARG_OPRZ1 },
    { "masklb",         OPR(0x10,0x60), BASE, ARG_OPR },
    { "masklb",         OPRL(0x12,0x60),BASE, ARG_OPRL },
    { "masklh",         OPR(0x10,0x61), BASE, ARG_OPR },
    { "masklh",         OPRL(0x12,0x61),BASE, ARG_OPRL },
    { "masklw",         OPR(0x10,0x62), BASE, ARG_OPR },
    { "masklw",         OPRL(0x12,0x62),BASE, ARG_OPRL },
    { "maskll",         OPR(0x10,0x63), BASE, ARG_OPR },
    { "maskll",         OPRL(0x12,0x63),BASE, ARG_OPRL },
    { "maskhb",         OPR(0x10,0x64), BASE, ARG_OPR },
    { "maskhb",         OPRL(0x12,0x64),BASE, ARG_OPRL },
    { "maskhh",         OPR(0x10,0x65), BASE, ARG_OPR },
    { "maskhh",         OPRL(0x12,0x65),BASE, ARG_OPRL },
    { "maskhw",         OPR(0x10,0x66), BASE, ARG_OPR },
    { "maskhw",         OPRL(0x12,0x66),BASE, ARG_OPRL },
    { "maskhl",         OPR(0x10,0x67), BASE, ARG_OPR },
    { "maskhl",         OPRL(0x12,0x67),BASE, ARG_OPRL },
    { "zap",            OPR(0x10,0x68), BASE, ARG_OPR },
    { "zap",            OPRL(0x12,0x68),BASE, ARG_OPRL },
    { "zapnot",         OPR(0x10,0x69), BASE, ARG_OPR },
    { "zapnot",         OPRL(0x12,0x69),BASE, ARG_OPRL },
    { "sextb",          OPR(0x10,0x6A), BASE, ARG_OPRZ1},
    { "sextb",          OPRL(0x12,0x6A),BASE, ARG_OPRLZ1 },
    { "sexth",          OPR(0x10,0x6B), BASE, ARG_OPRZ1 },
    { "sexth",          OPRL(0x12,0x6B),BASE, ARG_OPRLZ1 },
    { "cmpgeb",         OPR(0x10,0x6C), BASE, ARG_OPR },
    { "cmpgeb",         OPRL(0x12,0x6C),BASE, ARG_OPRL },
    { "fimovs",         OPR(0x10,0x70), BASE, { FA, ZB, RC } },
    { "fimovd",         OPR(0x10,0x78), BASE, { FA, ZB, RC } },
    { "seleq",          TOPR(0x11,0x0), BASE, ARG_TOPR },
    { "seleq",          TOPRL(0x13,0x0),BASE, ARG_TOPRL },
    { "selge",          TOPR(0x11,0x1), BASE, ARG_TOPR },
    { "selge",          TOPRL(0x13,0x1),BASE, ARG_TOPRL },
    { "selgt",          TOPR(0x11,0x2), BASE, ARG_TOPR },
    { "selgt",          TOPRL(0x13,0x2),BASE, ARG_TOPRL },
    { "selle",          TOPR(0x11,0x3), BASE, ARG_TOPR },
    { "selle",          TOPRL(0x13,0x3),BASE, ARG_TOPRL },
    { "sellt",          TOPR(0x11,0x4), BASE, ARG_TOPR },
    { "sellt",          TOPRL(0x13,0x4),BASE, ARG_TOPRL },
    { "selne",          TOPR(0x11,0x5), BASE, ARG_TOPR },
    { "selne",          TOPRL(0x13,0x5),BASE, ARG_TOPRL },
    { "sellbc",         TOPR(0x11,0x6), BASE, ARG_TOPR },
    { "sellbc",         TOPRL(0x13,0x6),BASE, ARG_TOPRL },
    { "sellbs",         TOPR(0x11,0x7), BASE, ARG_TOPR },
    { "sellbs",         TOPRL(0x13,0x7),BASE, ARG_TOPRL },
    { "vlog",           LOGX(0x14,0x00), BASE, ARG_FMA },

    { "fadds",          FP(0x18,0x00), BASE, ARG_FP },
    { "faddd",          FP(0x18,0x01), BASE, ARG_FP },
    { "fsubs",          FP(0x18,0x02), BASE, ARG_FP },
    { "fsubd",          FP(0x18,0x03), BASE, ARG_FP },
    { "fmuls",          FP(0x18,0x04), BASE, ARG_FP },
    { "fmuld",          FP(0x18,0x05), BASE, ARG_FP },
    { "fdivs",          FP(0x18,0x06), BASE, ARG_FP },
    { "fdivd",          FP(0x18,0x07), BASE, ARG_FP },
    { "fsqrts",         FP(0x18,0x08), BASE, ARG_FPZ1 },
    { "fsqrtd",         FP(0x18,0x09), BASE, ARG_FPZ1 },
    { "fcmpeq",         FP(0x18,0x10), BASE, ARG_FP },
    { "fcmple",         FP(0x18,0x11), BASE, ARG_FP },
    { "fcmplt",         FP(0x18,0x12), BASE, ARG_FP },
    { "fcmpun",         FP(0x18,0x13), BASE, ARG_FP },

    { "fcvtsd",         FP(0x18,0x20), BASE, ARG_FPZ1 },
    { "fcvtds",         FP(0x18,0x21), BASE, ARG_FPZ1 },
    { "fcvtdl_g",       FP(0x18,0x22), BASE, ARG_FPZ1 },
    { "fcvtdl_p",       FP(0x18,0x23), BASE, ARG_FPZ1 },
    { "fcvtdl_z",       FP(0x18,0x24), BASE, ARG_FPZ1 },
    { "fcvtdl_n",       FP(0x18,0x25), BASE, ARG_FPZ1 },
    { "fcvtdl",         FP(0x18,0x27), BASE, ARG_FPZ1 },
    { "fcvtwl",         FP(0x18,0x28), BASE, ARG_FPZ1 },
    { "fcvtlw",         FP(0x18,0x29), BASE, ARG_FPZ1 },
    { "fcvtls",         FP(0x18,0x2d), BASE, ARG_FPZ1 },
    { "fcvtld",         FP(0x18,0x2f), BASE, ARG_FPZ1 },
    { "fcpys",          FP(0x18,0x30), BASE, ARG_FP },
    { "fcpyse",         FP(0x18,0x31), BASE, ARG_FP },
    { "fcpysn",         FP(0x18,0x32), BASE, ARG_FP },
    { "ifmovs",         FP(0x18,0x40), BASE, { RA, ZB, FC } },
    { "ifmovd",         FP(0x18,0x41), BASE, { RA, ZB, FC } },
    { "rfpcr",          FP(0x18,0x50), BASE, { FA, RBA, RCA } },
    { "wfpcr",          FP(0x18,0x51), BASE, { FA, RBA, RCA } },
    { "setfpec0",       FP(0x18,0x54), BASE, ARG_NONE },
    { "setfpec1",       FP(0x18,0x55), BASE, ARG_NONE },
    { "setfpec2",       FP(0x18,0x56), BASE, ARG_NONE },
    { "setfpec3",       FP(0x18,0x57), BASE, ARG_NONE },
    { "fmas",           FMA(0x19,0x00), BASE, ARG_FMA },
    { "fmad",           FMA(0x19,0x01), BASE, ARG_FMA },
    { "fmss",           FMA(0x19,0x02), BASE, ARG_FMA },
    { "fmsd",           FMA(0x19,0x03), BASE, ARG_FMA },
    { "fnmas",          FMA(0x19,0x04), BASE, ARG_FMA },
    { "fnmad",          FMA(0x19,0x05), BASE, ARG_FMA },
    { "fnmss",          FMA(0x19,0x06), BASE, ARG_FMA },
    { "fnmsd",          FMA(0x19,0x07), BASE, ARG_FMA },
    { "fseleq",         FMA(0x19,0x10), BASE, ARG_FMA },
    { "fselne",         FMA(0x19,0x11), BASE, ARG_FMA },
    { "fsellt",         FMA(0x19,0x12), BASE, ARG_FMA },
    { "fselle",         FMA(0x19,0x13), BASE, ARG_FMA },
    { "fselgt",         FMA(0x19,0x14), BASE, ARG_FMA },
    { "fselge",         FMA(0x19,0x15), BASE, ARG_FMA },
    { "vaddw",          FP(0x1A,0x00), BASE, ARG_FP },
    { "vaddw",          FP(0x1A,0x20), BASE, ARG_FPL },
    { "vsubw",          FP(0x1A,0x01), BASE, ARG_FP },
    { "vsubw",          FP(0x1A,0x21), BASE, ARG_FPL },
    { "vcmpgew",        FP(0x1A,0x02), BASE, ARG_FP },
    { "vcmpgew",        FP(0x1A,0x22), BASE, ARG_FPL },
    { "vcmpeqw",        FP(0x1A,0x03), BASE, ARG_FP },
    { "vcmpeqw",        FP(0x1A,0x23), BASE, ARG_FPL },
    { "vcmplew",        FP(0x1A,0x04), BASE, ARG_FP },
    { "vcmplew",        FP(0x1A,0x24), BASE, ARG_FPL },
    { "vcmpltw",        FP(0x1A,0x05), BASE, ARG_FP },
    { "vcmpltw",        FP(0x1A,0x25), BASE, ARG_FPL },
    { "vcmpulew",       FP(0x1A,0x06), BASE, ARG_FP },
    { "vcmpulew",       FP(0x1A,0x26), BASE, ARG_FPL },
    { "vcmpultw",       FP(0x1A,0x07), BASE, ARG_FP },
    { "vcmpultw",       FP(0x1A,0x27), BASE, ARG_FPL },

    { "vsllw",          FP(0x1A,0x08), BASE, ARG_FP },
    { "vsllw",          FP(0x1A,0x28), BASE, ARG_FPL },
    { "vsrlw",          FP(0x1A,0x09), BASE, ARG_FP },
    { "vsrlw",          FP(0x1A,0x29), BASE, ARG_FPL },
    { "vsraw",          FP(0x1A,0x0A), BASE, ARG_FP },
    { "vsraw",          FP(0x1A,0x2A), BASE, ARG_FPL },
    { "vrolw",          FP(0x1A,0x0B), BASE, ARG_FP },
    { "vrolw",          FP(0x1A,0x2B), BASE, ARG_FPL },
    { "sllow",          FP(0x1A,0x0C), BASE, ARG_FP },
    { "sllow",          FP(0x1A,0x2C), BASE, ARG_FPL },
    { "srlow",          FP(0x1A,0x0D), BASE, ARG_FP },
    { "srlow",          FP(0x1A,0x2D), BASE, ARG_FPL },
    { "vaddl",          FP(0x1A,0x0E), BASE, ARG_FP },
    { "vaddl",          FP(0x1A,0x2E), BASE, ARG_FPL },
    { "vsubl",          FP(0x1A,0x0F), BASE, ARG_FP },
    { "vsubl",          FP(0x1A,0x2F), BASE, ARG_FPL },
    { "ctpopow",        FP(0x1A,0x18), BASE, { FA, ZB, DFC1 } },
    { "ctlzow",         FP(0x1A,0x19), BASE, { FA, ZB, DFC1 } },
    { "vucaddw",        FP(0x1A,0x40), BASE, ARG_FP },
    { "vucaddw",        FP(0x1A,0x60), BASE, ARG_FPL },
    { "vucsubw",        FP(0x1A,0x41), BASE, ARG_FP },
    { "vucsubw",        FP(0x1A,0x61), BASE, ARG_FPL },
    { "vucaddh",        FP(0x1A,0x42), BASE, ARG_FP },
    { "vucaddh",        FP(0x1A,0x62), BASE, ARG_FPL },
    { "vucsubh",        FP(0x1A,0x43), BASE, ARG_FP },
    { "vucsubh",        FP(0x1A,0x63), BASE, ARG_FPL },
    { "vucaddb",        FP(0x1A,0x44), BASE, ARG_FP },
    { "vucaddb",        FP(0x1A,0x64), BASE, ARG_FPL },
    { "vucsubb",        FP(0x1A,0x45), BASE, ARG_FP },
    { "vucsubb",        FP(0x1A,0x65), BASE, ARG_FPL },
    { "vadds",          FP(0x1A,0x80), BASE, ARG_FP },
    { "vaddd",          FP(0x1A,0x81), BASE, ARG_FP },
    { "vsubs",          FP(0x1A,0x82), BASE, ARG_FP },
    { "vsubd",          FP(0x1A,0x83), BASE, ARG_FP },
    { "vmuls",          FP(0x1A,0x84), BASE, ARG_FP },
    { "vmuld",          FP(0x1A,0x85), BASE, ARG_FP },
    { "vdivs",          FP(0x1A,0x86), BASE, ARG_FP },
    { "vdivd",          FP(0x1A,0x87), BASE, ARG_FP },
    { "vsqrts",         FP(0x1A,0x88), BASE, ARG_FPZ1 },
    { "vsqrtd",         FP(0x1A,0x89), BASE, ARG_FPZ1 },
    { "vfcmpeq",        FP(0x1A,0x8C), BASE, ARG_FP },
    { "vfcmple",        FP(0x1A,0x8D), BASE, ARG_FP },
    { "vfcmplt",        FP(0x1A,0x8E), BASE, ARG_FP },
    { "vfcmpun",        FP(0x1A,0x8F), BASE, ARG_FP },
    { "vcpys",          FP(0x1A,0x90), BASE, ARG_FP },
    { "vcpyse",         FP(0x1A,0x91), BASE, ARG_FP },
    { "vcpysn",         FP(0x1A,0x92), BASE, ARG_FP },
    { "vmas",           FMA(0x1B,0x00), BASE, ARG_FMA },
    { "vmad",           FMA(0x1B,0x01), BASE, ARG_FMA },
    { "vmss",           FMA(0x1B,0x02), BASE, ARG_FMA },
    { "vmsd",           FMA(0x1B,0x03), BASE, ARG_FMA },
    { "vnmas",          FMA(0x1B,0x04), BASE, ARG_FMA },
    { "vnmad",          FMA(0x1B,0x05), BASE, ARG_FMA },
    { "vnmss",          FMA(0x1B,0x06), BASE, ARG_FMA },
    { "vnmsd",          FMA(0x1B,0x07), BASE, ARG_FMA },
    { "vfseleq",        FMA(0x1B,0x10), BASE, ARG_FMA },
    { "vfsellt",        FMA(0x1B,0x12), BASE, ARG_FMA },
    { "vfselle",        FMA(0x1B,0x13), BASE, ARG_FMA },
    { "vseleqw",        FMA(0x1B,0x18), BASE, ARG_FMA },
    { "vseleqw",        FMA(0x1B,0x38), BASE, ARG_FMAL },
    { "vsellbcw",       FMA(0x1B,0x19), BASE, ARG_FMA },
    { "vsellbcw",       FMA(0x1B,0x39), BASE, ARG_FMAL },
    { "vselltw",        FMA(0x1B,0x1A), BASE, ARG_FMA },
    { "vselltw",        FMA(0x1B,0x3A), BASE, ARG_FMAL },
    { "vsellew",        FMA(0x1B,0x1B), BASE, ARG_FMA },
    { "vsellew",        FMA(0x1B,0x3B), BASE, ARG_FMAL },
    { "vinsw",          FMA(0x1B,0x20), BASE, ARG_FMAL },
    { "vinsf",          FMA(0x1B,0x21), BASE, ARG_FMAL },
    { "vextw",          FMA(0x1B,0x22), BASE, { FA, FMALIT, DFC1 }},
    { "vextf",          FMA(0x1B,0x23), BASE, { FA, FMALIT, DFC1 }},
    { "vcpyw",          FMA(0x1B,0x24), BASE, { FA, DFC1 }},
    { "vcpyf",          FMA(0x1B,0x25), BASE, { FA, DFC1 }},
    { "vconw",          FMA(0x1B,0x26), BASE, ARG_FMA },
    { "vshfw",          FMA(0x1B,0x27), BASE, ARG_FMA },
    { "vcons",          FMA(0x1B,0x28), BASE, ARG_FMA },
    { "vcond",          FMA(0x1B,0x29), BASE, ARG_FMA },
    { "vldw_u",         ATMEM(0x1C,0x0), BASE, ARG_VUAMEM },
    { "vstw_u",         ATMEM(0x1C,0x1), BASE, ARG_VUAMEM },
    { "vlds_u",         ATMEM(0x1C,0x2), BASE, ARG_VUAMEM },
    { "vsts_u",         ATMEM(0x1C,0x3), BASE, ARG_VUAMEM },
    { "vldd_u",         ATMEM(0x1C,0x4), BASE, ARG_VUAMEM },
    { "vstd_u",         ATMEM(0x1C,0x5), BASE, ARG_VUAMEM },
    { "vstw_ul",        ATMEM(0x1C,0x8), BASE, ARG_VUAMEM },
    { "vstw_uh",        ATMEM(0x1C,0x9), BASE, ARG_VUAMEM },
    { "vsts_ul",        ATMEM(0x1C,0xA), BASE, ARG_VUAMEM },
    { "vsts_uh",        ATMEM(0x1C,0xB), BASE, ARG_VUAMEM },
    { "vstd_ul",        ATMEM(0x1C,0xC), BASE, ARG_VUAMEM },
    { "vstd_uh",        ATMEM(0x1C,0xD), BASE, ARG_VUAMEM },
    { "vldd_nc",        ATMEM(0x1C,0xE), BASE, ARG_VUAMEM },
    { "vstd_nc",        ATMEM(0x1C,0xF), BASE, ARG_VUAMEM },

    { "flushd",         MEM(0x20), BASE, ARG_PREFETCH },
    { "ldbu",           MEM(0x20), BASE, ARG_MEM },
    { "evictdg",        MEM(0x21), BASE, ARG_PREFETCH },
    { "ldhu",           MEM(0x21), BASE, ARG_MEM },
    { "s_fillcs",       MEM(0x22), BASE, ARG_PREFETCH },
    { "ldw",            MEM(0x22), BASE, ARG_MEM },
    { "s_fillde",       MEM(0x23), BASE, ARG_PREFETCH },
    { "ldl",            MEM(0x23), BASE, ARG_MEM },
    { "evictdl",        MEM(0x24), BASE, ARG_PREFETCH },
    { "ldl_u",          MEM(0x24), BASE, ARG_MEM },
    { "pri_ldw/p",      HWMEM(0x25,0x0), BASE, ARG_HWMEM },
    { "pri_ldw/v",      HWMEM(0x25,0x8), BASE, ARG_HWMEM },
    { "pri_ldl/p",      HWMEM(0x25,0x1), BASE, ARG_HWMEM },
    { "pri_ldl/v",      HWMEM(0x25,0x9), BASE, ARG_HWMEM },
    { "fillde",         MEM(0x26), BASE, ARG_PREFETCH },
    { "flds",           MEM(0x26), BASE, ARG_FMEM },
    { "fillde_e",       MEM(0x27), BASE, ARG_PREFETCH },
    { "fldd",           MEM(0x27), BASE, ARG_FMEM },

    { "stb",            MEM(0x28), BASE, ARG_MEM },
    { "sth",            MEM(0x29), BASE, ARG_MEM },
    { "stw",            MEM(0x2A), BASE, ARG_MEM },
    { "stl",            MEM(0x2B), BASE, ARG_MEM },
    { "stl_u",          MEM(0x2C), BASE, ARG_MEM },
    { "pri_stw/p",      HWMEM(0x2D,0x0), BASE, ARG_HWMEM },
    { "pri_stw/v",      HWMEM(0x2D,0x8), BASE, ARG_HWMEM },
    { "pri_stl/p",      HWMEM(0x2D,0x1), BASE, ARG_HWMEM },
    { "pri_stl/v",      HWMEM(0x2D,0x9), BASE, ARG_HWMEM },
    { "fsts",           MEM(0x2E), BASE, ARG_FMEM },
    { "fstd",           MEM(0x2F), BASE, ARG_FMEM },
    { "beq",            BRA(0x30), BASE, ARG_BRA },
    { "bne",            BRA(0x31), BASE, ARG_BRA },
    { "blt",            BRA(0x32), BASE, ARG_BRA },
    { "ble",            BRA(0x33), BASE, ARG_BRA },
    { "bgt",            BRA(0x34), BASE, ARG_BRA },
    { "bge",            BRA(0x35), BASE, ARG_BRA },
    { "blbc",           BRA(0x36), BASE, ARG_BRA },
    { "blbs",           BRA(0x37), BASE, ARG_BRA },

    { "fbeq",           BRA(0x38), BASE, ARG_FBRA },
    { "fbne",           BRA(0x39), BASE, ARG_FBRA },
    { "fblt",           BRA(0x3A), BASE, ARG_FBRA },
    { "fble",           BRA(0x3B), BASE, ARG_FBRA },
    { "fbgt",           BRA(0x3C), BASE, ARG_FBRA },
    { "fbge",           BRA(0x3D), BASE, ARG_FBRA },
    { "ldi",            MEM(0x3E), BASE, ARG_MEM },
    { "ldih",           MEM(0x3F), BASE, ARG_MEM },
};

const unsigned sw_64_num_opcodes = sizeof(sw_64_opcodes) / sizeof(*sw_64_opcodes);

/* OSF register names. */

static const char * const osf_regnames[64] = {
    "v0", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
    "t7", "s0", "s1", "s2", "s3", "s4", "s5", "fp",
    "a0", "a1", "a2", "a3", "a4", "a5", "t8", "t9",
    "t10", "t11", "ra", "t12", "at", "gp", "sp", "zero",
    "$f0", "$f1", "$f2", "$f3", "$f4", "$f5", "$f6", "$f7",
    "$f8", "$f9", "$f10", "$f11", "$f12", "$f13", "$f14", "$f15",
    "$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23",
    "$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30", "$f31"
};

/* VMS register names. */

static const char * const vms_regnames[64] = {
    "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
    "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
    "R16", "R17", "R18", "R19", "R20", "R21", "R22", "R23",
    "R24", "AI", "RA", "PV", "AT", "FP", "SP", "RZ",
    "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7",
    "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15",
    "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23",
    "F24", "F25", "F26", "F27", "F28", "F29", "F30", "FZ"
};

int print_insn_sw_64(bfd_vma memaddr, struct disassemble_info *info)
{
    static const struct sw_64_opcode *opcode_index[SW_NOPS + 1];
    const char * const * regnames;
    const struct sw_64_opcode *opcode, *opcode_end;
    const unsigned char *opindex;
    unsigned insn, op, isa_mask;
    int need_comma;

    /* Initialize the majorop table the first time through */
    if (!opcode_index[0]) {
        opcode = sw_64_opcodes;
        opcode_end = opcode + sw_64_num_opcodes;

        for (op = 0; op < SW_NOPS; ++op) {
            opcode_index[op] = opcode;
            if ((SW_LITOP (opcode->opcode) != 0x10) && (SW_LITOP (opcode->opcode) != 0x11)) {
                while (opcode < opcode_end && op == SW_OP (opcode->opcode))
                    ++opcode;
            } else {
                while (opcode < opcode_end && op == SW_LITOP (opcode->opcode))
                    ++opcode;
            }
        }
        opcode_index[op] = opcode;
    }

    if (info->flavour == bfd_target_evax_flavour)
        regnames = vms_regnames;
    else
        regnames = osf_regnames;
    isa_mask = SW_OPCODE_NOHMCODE;
    switch (info->mach) {
        case bfd_mach_sw_64_core3:
            isa_mask |= SW_OPCODE_BASE | SW_OPCODE_CORE3;
            break;
    }

    /* Read the insn into a host word */
    {
        bfd_byte buffer[4];
        int status = (*info->read_memory_func) (memaddr, buffer, 4, info);
        if (status != 0) {
            (*info->memory_error_func) (status, memaddr, info);
            return -1;
        }
        insn = bfd_getl32 (buffer);
    }

    /* Get the major opcode of the instruction. */
    if ((SW_LITOP (insn) == 0x10) || (SW_LITOP (insn) == 0x11))
        op = SW_LITOP (insn);
    else if ((SW_OP(insn) & 0x3C) == 0x14 )
        op = 0x14;
    else
        op = SW_OP (insn);

    /* Find the first match in the opcode table. */
    opcode_end = opcode_index[op + 1];
    for (opcode = opcode_index[op]; opcode < opcode_end; ++opcode) {
        if ((insn ^ opcode->opcode) & opcode->mask)
            continue;

        if (!(opcode->flags & isa_mask))
            continue;

        /* Make two passes over the operands.  First see if any of them
           have extraction functions, and, if they do, make sure the
           instruction is valid. */
        {
            int invalid = 0;
            for (opindex = opcode->operands; *opindex != 0; opindex++) {
                const struct sw_64_operand *operand = sw_64_operands + *opindex;
                if (operand->extract)
                    (*operand->extract) (insn, &invalid);
            }
            if (invalid)
                continue;
        }

        /* The instruction is valid. */
        goto found;
    }

    /* No instruction found */
    (*info->fprintf_func) (info->stream, ".long %#08x", insn);

    return 4;

found:
    if (!strncmp("sys_call",opcode->name,8)) {
        if (insn & (0x1 << 25))
            (*info->fprintf_func) (info->stream, "%s", "sys_call");
        else
            (*info->fprintf_func) (info->stream, "%s", "sys_call/b");
    } else
        (*info->fprintf_func) (info->stream, "%s", opcode->name);

    /* get zz[7:6] and zz[5:0] to form truth for vlog */
    if (!strcmp(opcode->name, "vlog"))
    {
        unsigned int truth;
        char tr[4];
        truth=(SW_OP(insn) & 3) << 6;
        truth = truth | ((insn &  0xFC00) >> 10);
        sprintf(tr,"%x",truth);
        (*info->fprintf_func) (info->stream, "%s", tr);
    }
    if (opcode->operands[0] != 0)
        (*info->fprintf_func) (info->stream, "\t");

    /* Now extract and print the operands. */
    need_comma = 0;
    for (opindex = opcode->operands; *opindex != 0; opindex++) {
        const struct sw_64_operand *operand = sw_64_operands + *opindex;
        int value;

        /* Operands that are marked FAKE are simply ignored.  We
           already made sure that the extract function considered
           the instruction to be valid. */
        if ((operand->flags & SW_OPERAND_FAKE) != 0)
            continue;

        /* Extract the value from the instruction. */
        if (operand->extract)
            value = (*operand->extract) (insn, (int *) NULL);
        else {
            value = (insn >> operand->shift) & ((1 << operand->bits) - 1);
            if (operand->flags & SW_OPERAND_SIGNED) {
                int signbit = 1 << (operand->bits - 1);
                value = (value ^ signbit) - signbit;
            }
        }

        if (need_comma &&
                ((operand->flags & (SW_OPERAND_PARENS | SW_OPERAND_COMMA))
                 != SW_OPERAND_PARENS)) {
            (*info->fprintf_func) (info->stream, ",");
        }
        if (operand->flags & SW_OPERAND_PARENS)
            (*info->fprintf_func) (info->stream, "(");

        /* Print the operand as directed by the flags. */
        if (operand->flags & SW_OPERAND_IR)
            (*info->fprintf_func) (info->stream, "%s", regnames[value]);
        else if (operand->flags & SW_OPERAND_FPR)
            (*info->fprintf_func) (info->stream, "%s", regnames[value + 32]);
        else if (operand->flags & SW_OPERAND_RELATIVE)
            (*info->print_address_func) (memaddr + 4 + value, info);
        else if (operand->flags & SW_OPERAND_SIGNED)
            (*info->fprintf_func) (info->stream, "%d", value);
        else
            (*info->fprintf_func) (info->stream, "%#x", value);

        if (operand->flags & SW_OPERAND_PARENS)
            (*info->fprintf_func) (info->stream, ")");
        need_comma = 1;
    }

    return 4;
}
