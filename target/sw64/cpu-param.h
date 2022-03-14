/*
 * SW64 cpu parameters for qemu.
 *
 * Copyright (c) 2018 Lin Hainan
 */

#ifndef SW64_CPU_PARAM_H
#define SW64_CPU_PARAM_H 1

#define TARGET_LONG_BITS 64 /* if use th-1 ,TARGET_PAGE_BITS is 12 */
#define TARGET_PAGE_BITS 13

#ifdef CONFIG_USER_ONLY
#define TARGET_VIRT_ADDR_SPACE_BITS  64
#else
#define TARGET_PHYS_ADDR_SPACE_BITS  48
#define TARGET_VIRT_ADDR_SPACE_BITS  64
#endif

#ifndef CONFIG_USER_ONLY
#define NB_MMU_MODES 4
#endif

#endif
