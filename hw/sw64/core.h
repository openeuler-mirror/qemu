#ifndef HW_SW64_SYS_H
#define HW_SW64_SYS_H

typedef struct boot_params {
        unsigned long initrd_size;                      /* size of initrd */
        unsigned long initrd_start;                     /* logical address of initrd */
        unsigned long dtb_start;                        /* logical address of dtb */
        unsigned long efi_systab;                       /* logical address of EFI system table */
        unsigned long efi_memmap;                       /* logical address of EFI memory map */
        unsigned long efi_memmap_size;                  /* size of EFI memory map */
        unsigned long efi_memdesc_size;                 /* size of an EFI memory map descriptor */
        unsigned long efi_memdesc_version;              /* memory descriptor version */
        unsigned long cmdline;                          /* logical address of cmdline */
} BOOT_PARAMS;

void core3_board_init(SW64CPU *cpus[4], MemoryRegion *ram);
#endif

#define MAX_CPUS 64

#ifdef CONFIG_KVM
#define MAX_CPUS_CORE3 64
#else
#define MAX_CPUS_CORE3 32
#endif
