/*
 * Define LoongArch target-specific operand constraints.
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

/*
 * Define constraint letters for register sets:
 * REGS(letter, register_mask)
 */
REGS('r', ALL_GENERAL_REGS)
REGS('L', ALL_GENERAL_REGS & ~SOFTMMU_RESERVE_REGS)

/*
 * Define constraint letters for constants:
 * CONST(letter, TCG_CT_CONST_* bit set)
 */
CONST('I', TCG_CT_CONST_S12)
CONST('N', TCG_CT_CONST_N12)
CONST('U', TCG_CT_CONST_U12)
CONST('Z', TCG_CT_CONST_ZERO)
CONST('C', TCG_CT_CONST_C12)
CONST('W', TCG_CT_CONST_WSZ)
