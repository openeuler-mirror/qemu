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

#ifndef QEMU_LOONGARCH_DEFS_H
#define QEMU_LOONGARCH_DEFS_H

/* If we want to use host float regs... */
/* #define USE_HOST_FLOAT_REGS */

/* Real pages are variable size... */
#define TARGET_PAGE_BITS 14
#define LOONGARCH_TLB_MAX 2112
#define TARGET_LONG_BITS 64
#define TARGET_PHYS_ADDR_SPACE_BITS 48
#define TARGET_VIRT_ADDR_SPACE_BITS 48

/*
 * bit definitions for insn_flags (ISAs/ASEs flags)
 * ------------------------------------------------
 */
#define ISA_LARCH32 0x00000001ULL
#define ISA_LARCH64 0x00000002ULL
#define INSN_LOONGARCH 0x00010000ULL

#define CPU_LARCH32 (ISA_LARCH32)
#define CPU_LARCH64 (ISA_LARCH32 | ISA_LARCH64)

#endif /* QEMU_LOONGARCH_DEFS_H */
