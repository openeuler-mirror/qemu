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

#ifndef LOONGARCH_TARGET_SIGNAL_H
#define LOONGARCH_TARGET_SIGNAL_H

/* this struct defines a stack used during syscall handling */

typedef struct target_sigaltstack {
    abi_long ss_sp;
    abi_int ss_flags;
    abi_ulong ss_size;
} target_stack_t;

/*
 * sigaltstack controls
 */
#define TARGET_SS_ONSTACK 1
#define TARGET_SS_DISABLE 2

#define TARGET_MINSIGSTKSZ 2048
#define TARGET_SIGSTKSZ 8192

#include "../generic/signal.h"

#endif /* LOONGARCH_TARGET_SIGNAL_H */
