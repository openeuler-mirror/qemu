/*
 * Support for writing ELF notes for RM architectures
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
#include "cpu.h"
#include "elf.h"
#include "sysemu/dump.h"
#include "internal.h"

/* struct user_pt_regs from arch/loongarch/include/uapi/asm/ptrace.h */
struct loongarch_user_regs {
    uint64_t gpr[32];
    uint64_t lo;
    uint64_t hi;
    uint64_t csr_era;
    uint64_t csr_badvaddr;
    uint64_t csr_crmd;
    uint64_t csr_ecfg;
    uint64_t pad[7];
} QEMU_PACKED;

QEMU_BUILD_BUG_ON(sizeof(struct loongarch_user_regs) != 360);

/* struct elf_prstatus from include/uapi/linux/elfcore.h */
struct loongarch_elf_prstatus {
    char pad1[32]; /* 32 == offsetof(struct elf_prstatus, pr_pid) */
    uint32_t pr_pid;
    /*
     * 76 == offsetof(struct elf_prstatus, pr_reg) -
     * offsetof(struct elf_prstatus, pr_ppid)
     */
    char pad2[76];
    struct loongarch_user_regs pr_reg;
    uint32_t pr_fpvalid;
    char pad3[4];
} QEMU_PACKED;

QEMU_BUILD_BUG_ON(sizeof(struct loongarch_elf_prstatus) != 480);

/*
 * struct user_fpsimd_state from arch/arm64/include/uapi/asm/ptrace.h
 *
 * While the vregs member of user_fpsimd_state is of type __uint128_t,
 * QEMU uses an array of uint64_t, where the high half of the 128-bit
 * value is always in the 2n+1'th index. Thus we also break the 128-
 * bit values into two halves in this reproduction of user_fpsimd_state.
 */

struct loongarch_fpu_struct {
    uint64_t fpr[32];
    unsigned int fir;
    unsigned int fcsr;
} QEMU_PACKED;

QEMU_BUILD_BUG_ON(sizeof(struct loongarch_fpu_struct) != 264);

struct loongarch_note {
    Elf64_Nhdr hdr;
    char name[8]; /* align_up(sizeof("CORE"), 4) */
    union
    {
        struct loongarch_elf_prstatus prstatus;
        struct loongarch_fpu_struct fpu;
    };
} QEMU_PACKED;

#define LOONGARCH_NOTE_HEADER_SIZE offsetof(struct loongarch_note, prstatus)
#define LOONGARCH_PRSTATUS_NOTE_SIZE                                          \
    (LOONGARCH_NOTE_HEADER_SIZE + sizeof(struct loongarch_elf_prstatus))
#define LOONGARCH_PRFPREG_NOTE_SIZE                                           \
    (LOONGARCH_NOTE_HEADER_SIZE + sizeof(struct loongarch_fpu_struct))

static void loongarch_note_init(struct loongarch_note *note, DumpState *s,
                                const char *name, Elf64_Word namesz,
                                Elf64_Word type, Elf64_Word descsz)
{
    memset(note, 0, sizeof(*note));

    note->hdr.n_namesz = cpu_to_dump32(s, namesz);
    note->hdr.n_descsz = cpu_to_dump32(s, descsz);
    note->hdr.n_type = cpu_to_dump32(s, type);

    memcpy(note->name, name, namesz);
}

static int loongarch_write_elf64_fprpreg(WriteCoreDumpFunction f,
                                         CPULOONGARCHState *env, int cpuid,
                                         DumpState *s)
{
    struct loongarch_note note;
    int ret, i;

    loongarch_note_init(&note, s, "CORE", 5, NT_PRFPREG, sizeof(note.fpu));

    note.fpu.fcsr = cpu_to_dump64(s, env->active_fpu.fcsr0);

    for (i = 0; i < 32; ++i) {
        note.fpu.fpr[i] = cpu_to_dump64(s, env->active_fpu.fpr[i].fd);
    }

    ret = f(&note, LOONGARCH_PRFPREG_NOTE_SIZE, s);
    if (ret < 0) {
        return -1;
    }

    return 0;
}

int loongarch_cpu_write_elf64_note(WriteCoreDumpFunction f, CPUState *cs,
                                   int cpuid, void *opaque)
{
    struct loongarch_note note;
    CPULOONGARCHState *env = &LOONGARCH_CPU(cs)->env;
    DumpState *s = opaque;
    int ret, i;

    loongarch_note_init(&note, s, "CORE", 5, NT_PRSTATUS,
                        sizeof(note.prstatus));

    note.prstatus.pr_pid = cpu_to_dump32(s, cpuid);
    note.prstatus.pr_fpvalid = cpu_to_dump32(s, 1);

    for (i = 0; i < 32; ++i) {
        note.prstatus.pr_reg.gpr[i] = cpu_to_dump64(s, env->active_tc.gpr[i]);
    }
    note.prstatus.pr_reg.csr_era = cpu_to_dump64(s, env->CSR_ERA);
    note.prstatus.pr_reg.csr_badvaddr = cpu_to_dump64(s, env->CSR_BADV);
    note.prstatus.pr_reg.csr_crmd = cpu_to_dump64(s, env->CSR_CRMD);
    note.prstatus.pr_reg.csr_ecfg = cpu_to_dump64(s, env->CSR_ECFG);

    ret = f(&note, LOONGARCH_PRSTATUS_NOTE_SIZE, s);
    if (ret < 0) {
        return -1;
    }

    ret = loongarch_write_elf64_fprpreg(f, env, cpuid, s);
    if (ret < 0) {
        return -1;
    }

    return ret;
}

int cpu_get_dump_info(ArchDumpInfo *info,
                      const GuestPhysBlockList *guest_phys_blocks)
{
    info->d_machine = EM_LOONGARCH;
    info->d_endian = ELFDATA2LSB;
    info->d_class = ELFCLASS64;

    return 0;
}

ssize_t cpu_get_note_size(int class, int machine, int nr_cpus)
{
    size_t note_size = 0;

    if (class == ELFCLASS64) {
        note_size = LOONGARCH_PRSTATUS_NOTE_SIZE + LOONGARCH_PRFPREG_NOTE_SIZE;
    }

    return note_size * nr_cpus;
}
