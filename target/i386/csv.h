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

#include "qapi/qapi-commands-misc-target.h"
#include "qemu/thread.h"
#include "qemu/queue.h"
#include "exec/hwaddr.h"
#include "sev.h"

#define GUEST_POLICY_CSV3_BIT    (1 << 6)
#define GUEST_POLICY_REUSE_ASID  (1 << 7)

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

bool csv3_enabled(void);

#else

#define is_hygon_cpu() (false)
#define csv3_enabled() (false)

#endif

#define CSV_OUTGOING_PAGE_WINDOW_SIZE     (4094 * TARGET_PAGE_SIZE)

extern bool csv_kvm_cpu_reset_inhibit;
extern uint32_t kvm_hygon_coco_ext;
extern uint32_t kvm_hygon_coco_ext_inuse;

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

/* CSV3 */
struct dma_map_region {
    uint64_t start, size;
    QTAILQ_ENTRY(dma_map_region) list;
};

#define CSV3_OUTGOING_PAGE_WINDOW_SIZE (512 * TARGET_PAGE_SIZE)

struct guest_addr_entry {
    uint64_t share:    1;
    uint64_t reserved: 11;
    uint64_t gfn:      52;
};

struct guest_hva_entry {
    uint64_t  hva;
};

struct Csv3GuestState {
    uint32_t policy;
    int sev_fd;
    void *state;
    int (*sev_ioctl)(int fd, int cmd, void *data, int *error);
    const char *(*fw_error_to_str)(int code);
    QTAILQ_HEAD(, dma_map_region) dma_map_regions_list;
    QemuMutex dma_map_regions_list_mutex;
    gchar *send_packet_hdr;
    size_t send_packet_hdr_len;
    struct guest_hva_entry *guest_hva_data;
    struct guest_addr_entry *guest_addr_data;
    size_t guest_addr_len;

    int (*sev_send_start)(QEMUFile *f, uint64_t *bytes_sent);
    int (*sev_receive_start)(QEMUFile *f);
};

typedef struct Csv3GuestState Csv3GuestState;

extern struct Csv3GuestState csv3_guest;
extern struct ConfidentialGuestMemoryEncryptionOps csv3_memory_encryption_ops;
extern int csv3_init(uint32_t policy, int fd, void *state, struct sev_ops *ops);
extern int csv3_launch_encrypt_vmcb(void);
void csv3_launch_finish_ex(char *host_data);

int csv3_load_data(uint64_t gpa, uint8_t *ptr, uint64_t len, Error **errp);

int csv3_shared_region_dma_map(uint64_t start, uint64_t end);
void csv3_shared_region_dma_unmap(uint64_t start, uint64_t end);
void csv3_shared_region_release(uint64_t gpa, uint32_t num_pages);
void csv3_shared_region_get(uint64_t gpa, uint32_t num_pages);
int csv3_load_incoming_page(QEMUFile *f, uint8_t *ptr);
int csv3_load_incoming_context(QEMUFile *f);
int csv3_queue_outgoing_page(uint8_t *ptr, uint32_t sz, uint64_t addr);
int csv3_save_queued_outgoing_pages(QEMUFile *f, uint64_t *bytes_sent);
int csv3_save_outgoing_context(QEMUFile *f, uint64_t *bytes_sent);

int csv3_set_guest_private_memory(Error **errp);

#endif
