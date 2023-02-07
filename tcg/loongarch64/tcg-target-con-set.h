/*
 * Define LoongArch target-specific constraint sets.
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
 * C_On_Im(...) defines a constraint set with <n> outputs and <m> inputs.
 * Each operand should be a sequence of constraint letters as defined by
 * tcg-target-con-str.h; the constraint combination is inclusive or.
 */
C_O0_I1(r)
C_O0_I2(rZ, r)
C_O0_I2(rZ, rZ)
C_O0_I2(LZ, L)
C_O1_I1(r, r)
C_O1_I1(r, L)
C_O1_I2(r, r, rC)
C_O1_I2(r, r, ri)
C_O1_I2(r, r, rI)
C_O1_I2(r, r, rU)
C_O1_I2(r, r, rW)
C_O1_I2(r, r, rZ)
C_O1_I2(r, 0, rZ)
C_O1_I2(r, rZ, rN)
C_O1_I2(r, rZ, rZ)
