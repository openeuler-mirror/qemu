/*
 * QEMU CSV support
 *
 * Copyright: Hygon Info Technologies Ltd. 2022
 *
 * Author:
 *      Jiang Xin <jiangxin@hygon.cn>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef I386_CSV_H
#define I386_CSV_H

#ifdef CONFIG_CSV

#include "cpu.h"

#define CPUID_VENDOR_HYGON_EBX   0x6f677948  /* "Hygo" */
#define CPUID_VENDOR_HYGON_ECX   0x656e6975  /* "uine" */
#define CPUID_VENDOR_HYGON_EDX   0x6e65476e  /* "nGen" */

static bool __attribute__((unused)) is_hygon_cpu(void)
{
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;

    host_cpuid(0, 0, NULL, &ebx, &ecx, &edx);

    if (ebx == CPUID_VENDOR_HYGON_EBX &&
        ecx == CPUID_VENDOR_HYGON_ECX &&
        edx == CPUID_VENDOR_HYGON_EDX)
        return true;
    else
        return false;
}

#else

#define is_hygon_cpu() (false)

#endif

#define CSV_OUTGOING_PAGE_WINDOW_SIZE     (4094 * TARGET_PAGE_SIZE)

typedef struct CsvBatchCmdList CsvBatchCmdList;
typedef void (*CsvDestroyCmdNodeFn) (void *data);

struct CsvBatchCmdList {
    struct kvm_csv_batch_list_node *head;
    struct kvm_csv_batch_list_node *tail;
    CsvDestroyCmdNodeFn destroy_fn;
};

int csv_queue_outgoing_page(uint8_t *ptr, uint32_t sz, uint64_t addr);
int csv_save_queued_outgoing_pages(QEMUFile *f, uint64_t *bytes_sent);
int csv_queue_incoming_page(QEMUFile *f, uint8_t *ptr);
int csv_load_queued_incoming_pages(QEMUFile *f);
int csv_save_outgoing_cpu_state(QEMUFile *f, uint64_t *bytes_sent);
int csv_load_incoming_cpu_state(QEMUFile *f);

#endif
