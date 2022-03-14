/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Define sw_64 target-specific operand constraints.
 * Copyright (c) 2021 Linaro
 */

/*
 * Define constraint letters for register sets:
 * REGS(letter, register_mask)
 */
REGS('r', ALL_GENERAL_REGS)
REGS('l', ALL_QLDST_REGS)
REGS('w', ALL_VECTOR_REGS)

/*
 * Define constraint letters for constants:
 * CONST(letter, TCG_CT_CONST_* bit set)
 */

CONST('Z', TCG_CT_CONST_ZERO)
CONST('A', TCG_CT_CONST_LONG)
CONST('M', TCG_CT_CONST_MONE)
CONST('O', TCG_CT_CONST_ORRI)
CONST('W', TCG_CT_CONST_WORD)
CONST('L', TCG_CT_CONST_LONG)
CONST('U', TCG_CT_CONST_U8)
CONST('S', TCG_CT_CONST_S8)

