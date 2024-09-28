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

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "sysemu/kvm.h"
#include "exec/address-spaces.h"
#include "migration/blocker.h"
#include "migration/qemu-file.h"
#include "migration/misc.h"
#include "monitor/monitor.h"

#include <linux/kvm.h>
#include <linux/psp-sev.h>

#ifdef CONFIG_NUMA
#include <numaif.h>
#endif

#include "trace.h"
#include "cpu.h"
#include "sev.h"
#include "csv.h"

bool csv_kvm_cpu_reset_inhibit;
uint32_t kvm_hygon_coco_ext;
uint32_t kvm_hygon_coco_ext_inuse;

struct ConfidentialGuestMemoryEncryptionOps csv3_memory_encryption_ops = {
    .save_setup = sev_save_setup,
    .save_outgoing_page = NULL,
    .load_incoming_page = csv3_load_incoming_page,
    .is_gfn_in_unshared_region = NULL,
    .save_outgoing_shared_regions_list = sev_save_outgoing_shared_regions_list,
    .load_incoming_shared_regions_list = sev_load_incoming_shared_regions_list,
    .queue_outgoing_page = csv3_queue_outgoing_page,
    .save_queued_outgoing_pages = csv3_save_queued_outgoing_pages,
    .queue_incoming_page = NULL,
    .load_queued_incoming_pages = NULL,
    .save_outgoing_cpu_state = csv3_save_outgoing_context,
    .load_incoming_cpu_state = csv3_load_incoming_context,
};

#define CSV3_OUTGOING_PAGE_NUM \
        (CSV3_OUTGOING_PAGE_WINDOW_SIZE / TARGET_PAGE_SIZE)

Csv3GuestState csv3_guest = { 0 };

int
csv3_init(uint32_t policy, int fd, void *state, struct sev_ops *ops)
{
    int fw_error;
    int ret;
    struct kvm_csv3_init_data data = { 0 };

#ifdef CONFIG_NUMA
    int mode;
    unsigned long nodemask;

    /* Set flags as 0 to retrieve the default NUMA policy. */
    ret = get_mempolicy(&mode, &nodemask, sizeof(nodemask) * 8, NULL, 0);
    if (ret == 0 && mode == MPOL_BIND)
        data.nodemask = nodemask;
#endif

    if (!ops || !ops->sev_ioctl || !ops->fw_error_to_str)
        return -1;

    csv3_guest.policy = policy;
    if (csv3_enabled()) {
        ret = ops->sev_ioctl(fd, KVM_CSV3_INIT, &data, &fw_error);
        if (ret) {
            csv3_guest.policy = 0;
            error_report("%s: Fail to initialize ret=%d fw_error=%d '%s'",
                       __func__, ret, fw_error, ops->fw_error_to_str(fw_error));
            return -1;
        }

        kvm_csv3_allowed = true;

        csv3_guest.sev_fd = fd;
        csv3_guest.state = state;
        csv3_guest.sev_ioctl = ops->sev_ioctl;
        csv3_guest.fw_error_to_str = ops->fw_error_to_str;
        QTAILQ_INIT(&csv3_guest.dma_map_regions_list);
        qemu_mutex_init(&csv3_guest.dma_map_regions_list_mutex);
        csv3_guest.sev_send_start = ops->sev_send_start;
        csv3_guest.sev_receive_start = ops->sev_receive_start;
    }
    return 0;
}

bool
csv3_enabled(void)
{
    if (!is_hygon_cpu())
        return false;

    return sev_es_enabled() && (csv3_guest.policy & GUEST_POLICY_CSV3_BIT);
}

static bool
csv3_check_state(SevState state)
{
    return *((SevState *)csv3_guest.state) == state;
}

static int
csv3_ioctl(int cmd, void *data, int *error)
{
    if (csv3_guest.sev_ioctl)
        return csv3_guest.sev_ioctl(csv3_guest.sev_fd, cmd, data, error);
    else
        return -1;
}

static const char *
fw_error_to_str(int code)
{
    if (csv3_guest.fw_error_to_str)
        return csv3_guest.fw_error_to_str(code);
    else
        return NULL;
}

static int
csv3_launch_encrypt_data(uint64_t gpa, uint8_t *addr, uint64_t len)
{
    int ret, fw_error;
    struct kvm_csv3_launch_encrypt_data update;

    if (!addr || !len) {
        return 1;
    }

    update.gpa = (__u64)gpa;
    update.uaddr = (__u64)(unsigned long)addr;
    update.len = len;
    trace_kvm_csv3_launch_encrypt_data(gpa, addr, len);
    ret = csv3_ioctl(KVM_CSV3_LAUNCH_ENCRYPT_DATA, &update, &fw_error);
    if (ret) {
        error_report("%s: CSV3 LAUNCH_ENCRYPT_DATA ret=%d fw_error=%d '%s'",
                __func__, ret, fw_error, fw_error_to_str(fw_error));
    }

    return ret;
}

int
csv3_load_data(uint64_t gpa, uint8_t *ptr, uint64_t len, Error **errp)
{
    int ret = 0;

    if (!csv3_enabled()) {
        error_setg(errp, "%s: CSV3 is not enabled", __func__);
        return -1;
    }

    /* if CSV3 is in update state then load the data to secure memory */
    if (csv3_check_state(SEV_STATE_LAUNCH_UPDATE)) {
        ret = csv3_launch_encrypt_data(gpa, ptr, len);
        if (ret)
            error_setg(errp, "%s: CSV3 fail to encrypt data", __func__);
    }

    return ret;
}

int
csv3_launch_encrypt_vmcb(void)
{
    int ret, fw_error;

    if (!csv3_enabled()) {
        error_report("%s: CSV3 is not enabled", __func__);
        return -1;
    }

    ret = csv3_ioctl(KVM_CSV3_LAUNCH_ENCRYPT_VMCB, NULL, &fw_error);
    if (ret) {
        error_report("%s: CSV3 LAUNCH_ENCRYPT_VMCB ret=%d fw_error=%d '%s'",
                     __func__, ret, fw_error, fw_error_to_str(fw_error));
        goto err;
    }

err:
    return ret;
}

int csv3_shared_region_dma_map(uint64_t start, uint64_t end)
{
    MemoryRegionSection section;
    AddressSpace *as;
    QTAILQ_HEAD(, SharedRegionListener) *shared_region_listeners;
    SharedRegionListener *shl;
    MemoryListener *listener;
    uint64_t size;
    Csv3GuestState *s = &csv3_guest;
    struct dma_map_region *region, *pos;
    int ret = 0;

    if (!csv3_enabled())
        return 0;

    if (end <= start)
        return 0;

    shared_region_listeners = shared_region_listeners_get();
    if (QTAILQ_EMPTY(shared_region_listeners))
        return 0;

    size = end - start;

    qemu_mutex_lock(&s->dma_map_regions_list_mutex);
    QTAILQ_FOREACH(pos, &s->dma_map_regions_list, list) {
        if (start >= (pos->start + pos->size)) {
            continue;
        } else if ((start + size) <= pos->start) {
            break;
        } else {
            goto end;
        }
    }
    QTAILQ_FOREACH(shl, shared_region_listeners, next) {
        listener = shl->listener;
        as = shl->as;
        section = memory_region_find(as->root, start, size);
        if (!section.mr) {
            goto end;
        }

        if (!memory_region_is_ram(section.mr)) {
            memory_region_unref(section.mr);
            goto end;
        }

        if (listener->region_add) {
            listener->region_add(listener, &section);
        }
        memory_region_unref(section.mr);
    }

    region = g_malloc0(sizeof(*region));
    if (!region) {
        ret = -1;
        goto end;
    }
    region->start = start;
    region->size = size;

    if (pos) {
        QTAILQ_INSERT_BEFORE(pos, region, list);
    } else {
        QTAILQ_INSERT_TAIL(&s->dma_map_regions_list, region, list);
    }

end:
    qemu_mutex_unlock(&s->dma_map_regions_list_mutex);
    return ret;
}

void csv3_shared_region_dma_unmap(uint64_t start, uint64_t end)
{
    MemoryRegionSection section;
    AddressSpace *as;
    QTAILQ_HEAD(, SharedRegionListener) *shared_region_listeners;
    SharedRegionListener *shl;
    MemoryListener *listener;
    uint64_t size;
    Csv3GuestState *s = &csv3_guest;
    struct dma_map_region *pos, *next_pos;

    if (!csv3_enabled())
        return;

    if (end <= start)
        return;

    shared_region_listeners = shared_region_listeners_get();
    if (QTAILQ_EMPTY(shared_region_listeners))
        return;

    size = end - start;

    qemu_mutex_lock(&s->dma_map_regions_list_mutex);
    QTAILQ_FOREACH_SAFE(pos, &s->dma_map_regions_list, list, next_pos) {
        uint64_t l, r;
        uint64_t curr_end = pos->start + pos->size;

        l = MAX(start, pos->start);
        r = MIN(start + size, pos->start + pos->size);
        if (l < r) {
            if ((start <= pos->start) && (start + size >= pos->start + pos->size)) {
                QTAILQ_FOREACH(shl, shared_region_listeners, next) {
                    listener = shl->listener;
                    as = shl->as;
                    section = memory_region_find(as->root, pos->start, pos->size);
                    if (!section.mr) {
                        goto end;
                    }
                    if (listener->region_del) {
                        listener->region_del(listener, &section);
                    }
                    memory_region_unref(section.mr);
                }

                QTAILQ_REMOVE(&s->dma_map_regions_list, pos, list);
                g_free(pos);
            }
            break;
        }
        if ((start + size) <= curr_end) {
            break;
        }
    }
end:
    qemu_mutex_unlock(&s->dma_map_regions_list_mutex);
    return;
}

static inline hwaddr csv3_hva_to_gfn(uint8_t *ptr)
{
    ram_addr_t offset = RAM_ADDR_INVALID;

    kvm_physical_memory_addr_from_host(kvm_state, ptr, &offset);

    return offset >> TARGET_PAGE_BITS;
}

static int
csv3_send_start(QEMUFile *f, uint64_t *bytes_sent)
{
    if (csv3_guest.sev_send_start)
        return csv3_guest.sev_send_start(f, bytes_sent);
    else
        return -1;
}

static int
csv3_send_get_packet_len(int *fw_err)
{
    int ret;
    struct kvm_csv3_send_encrypt_data update = {0};

    update.hdr_len = 0;
    update.trans_len = 0;
    ret = csv3_ioctl(KVM_CSV3_SEND_ENCRYPT_DATA, &update, fw_err);
    if (*fw_err != SEV_RET_INVALID_LEN) {
        error_report("%s: failed to get session length ret=%d fw_error=%d '%s'",
                    __func__, ret, *fw_err, fw_error_to_str(*fw_err));
        ret = 0;
        goto err;
    }

    if (update.hdr_len <= INT_MAX)
        ret = update.hdr_len;
    else
        ret = 0;

err:
    return ret;
}

static int
csv3_send_encrypt_data(Csv3GuestState *s, QEMUFile *f,
                       uint8_t *ptr, uint32_t size, uint64_t *bytes_sent)
{
    int ret, fw_error = 0;
    guchar *trans;
    uint32_t guest_addr_entry_num;
    uint32_t i;
    struct kvm_csv3_send_encrypt_data update = { };

    /*
     * If this is first call then query the packet header bytes and allocate
     * the packet buffer.
     */
    if (!s->send_packet_hdr) {
        s->send_packet_hdr_len = csv3_send_get_packet_len(&fw_error);
        if (s->send_packet_hdr_len < 1) {
            error_report("%s: SEND_UPDATE fw_error=%d '%s'",
                         __func__, fw_error, fw_error_to_str(fw_error));
            return 1;
        }

        s->send_packet_hdr = g_new(gchar, s->send_packet_hdr_len);
    }

    if (!s->guest_addr_len || !s->guest_addr_data) {
        error_report("%s: invalid host address or size", __func__);
        return 1;
    } else {
        guest_addr_entry_num = s->guest_addr_len / sizeof(struct guest_addr_entry);
    }

    /* allocate transport buffer */
    trans = g_new(guchar, guest_addr_entry_num * TARGET_PAGE_SIZE);

    update.hdr_uaddr = (uintptr_t)s->send_packet_hdr;
    update.hdr_len = s->send_packet_hdr_len;
    update.guest_addr_data = (uintptr_t)s->guest_addr_data;
    update.guest_addr_len = s->guest_addr_len;
    update.trans_uaddr = (uintptr_t)trans;
    update.trans_len = guest_addr_entry_num * TARGET_PAGE_SIZE;

    trace_kvm_csv3_send_encrypt_data(trans, update.trans_len);

    ret = csv3_ioctl(KVM_CSV3_SEND_ENCRYPT_DATA, &update, &fw_error);
    if (ret) {
        error_report("%s: SEND_ENCRYPT_DATA ret=%d fw_error=%d '%s'",
                     __func__, ret, fw_error, fw_error_to_str(fw_error));
        goto err;
    }

    for (i = 0; i < guest_addr_entry_num; i++) {
        if (s->guest_addr_data[i].share)
            memcpy(trans + i * TARGET_PAGE_SIZE, (guchar *)s->guest_hva_data[i].hva,
                   TARGET_PAGE_SIZE);
    }

    qemu_put_be32(f, update.hdr_len);
    qemu_put_buffer(f, (uint8_t *)update.hdr_uaddr, update.hdr_len);
    *bytes_sent += 4 + update.hdr_len;

    qemu_put_be32(f, update.guest_addr_len);
    qemu_put_buffer(f, (uint8_t *)update.guest_addr_data, update.guest_addr_len);
    *bytes_sent += 4 + update.guest_addr_len;

    qemu_put_be32(f, update.trans_len);
    qemu_put_buffer(f, (uint8_t *)update.trans_uaddr, update.trans_len);
    *bytes_sent += (4 + update.trans_len);

err:
    s->guest_addr_len = 0;
    g_free(trans);
    return ret;
}

int
csv3_queue_outgoing_page(uint8_t *ptr, uint32_t sz, uint64_t addr)
{
    Csv3GuestState *s = &csv3_guest;
    uint32_t i = 0;

    if (!s->guest_addr_data) {
        s->guest_hva_data = g_new0(struct guest_hva_entry, CSV3_OUTGOING_PAGE_NUM);
        s->guest_addr_data = g_new0(struct guest_addr_entry, CSV3_OUTGOING_PAGE_NUM);
        s->guest_addr_len = 0;
    }

    if (s->guest_addr_len >= sizeof(struct guest_addr_entry) * CSV3_OUTGOING_PAGE_NUM) {
        error_report("Failed to queue outgoing page");
        return 1;
    }

    i = s->guest_addr_len / sizeof(struct guest_addr_entry);
    s->guest_hva_data[i].hva = (uintptr_t)ptr;
    s->guest_addr_data[i].share = 0;
    s->guest_addr_data[i].reserved = 0;
    s->guest_addr_data[i].gfn = csv3_hva_to_gfn(ptr);
    s->guest_addr_len += sizeof(struct guest_addr_entry);

    return 0;
}

int
csv3_save_queued_outgoing_pages(QEMUFile *f, uint64_t *bytes_sent)
{
    Csv3GuestState *s = &csv3_guest;

    /*
     * If this is a first buffer then create outgoing encryption context
     * and write our PDH, policy and session data.
     */
    if (!csv3_check_state(SEV_STATE_SEND_UPDATE) &&
        csv3_send_start(f, bytes_sent)) {
        error_report("Failed to create outgoing context");
        return 1;
    }

    return csv3_send_encrypt_data(s, f, NULL, 0, bytes_sent);
}

static int
csv3_receive_start(QEMUFile *f)
{
    if (csv3_guest.sev_receive_start)
        return csv3_guest.sev_receive_start(f);
    else
        return -1;
}

static int csv3_receive_encrypt_data(QEMUFile *f, uint8_t *ptr)
{
    int ret = 1, fw_error = 0;
    uint32_t i, guest_addr_entry_num;
    gchar *hdr = NULL, *trans = NULL;
    struct guest_addr_entry *guest_addr_data;
    struct kvm_csv3_receive_encrypt_data update = {};
    void *hva = NULL;
    MemoryRegion *mr = NULL;

    /* get packet header */
    update.hdr_len = qemu_get_be32(f);

    hdr = g_new(gchar, update.hdr_len);
    qemu_get_buffer(f, (uint8_t *)hdr, update.hdr_len);
    update.hdr_uaddr = (uintptr_t)hdr;

    /* get guest addr data */
    update.guest_addr_len = qemu_get_be32(f);

    guest_addr_data = (struct guest_addr_entry *)g_new(gchar, update.guest_addr_len);
    qemu_get_buffer(f, (uint8_t *)guest_addr_data, update.guest_addr_len);
    update.guest_addr_data = (uintptr_t)guest_addr_data;

    /* get transport buffer */
    update.trans_len = qemu_get_be32(f);

    trans = g_new(gchar, update.trans_len);
    update.trans_uaddr = (uintptr_t)trans;
    qemu_get_buffer(f, (uint8_t *)update.trans_uaddr, update.trans_len);

    /* update share memory. */
    guest_addr_entry_num = update.guest_addr_len / sizeof(struct guest_addr_entry);
    for (i = 0; i < guest_addr_entry_num; i++) {
        if (guest_addr_data[i].share) {
            hva = gpa2hva(&mr,
                          ((uint64_t)guest_addr_data[i].gfn << TARGET_PAGE_BITS),
                          TARGET_PAGE_SIZE,
                          NULL);
            if (hva)
                memcpy(hva, trans + i * TARGET_PAGE_SIZE, TARGET_PAGE_SIZE);
        }
    }

    trace_kvm_csv3_receive_encrypt_data(trans, update.trans_len, hdr, update.hdr_len);

    ret = csv3_ioctl(KVM_CSV3_RECEIVE_ENCRYPT_DATA, &update, &fw_error);
    if (ret) {
        error_report("Error RECEIVE_ENCRYPT_DATA ret=%d fw_error=%d '%s'",
                     ret, fw_error, fw_error_to_str(fw_error));
        goto err;
    }

err:
    g_free(trans);
    g_free(guest_addr_data);
    g_free(hdr);
    return ret;
}

int csv3_load_incoming_page(QEMUFile *f, uint8_t *ptr)
{
    /*
     * If this is first buffer and SEV is not in recieiving state then
     * use RECEIVE_START command to create a encryption context.
     */
    if (!csv3_check_state(SEV_STATE_RECEIVE_UPDATE) &&
        csv3_receive_start(f)) {
        return 1;
    }

    return csv3_receive_encrypt_data(f, ptr);
}

static int
csv3_send_get_context_len(int *fw_err, int *context_len, int *hdr_len)
{
    int ret = 0;
    struct kvm_csv3_send_encrypt_context update = { 0 };

    ret = csv3_ioctl(KVM_CSV3_SEND_ENCRYPT_CONTEXT, &update, fw_err);
    if (*fw_err != SEV_RET_INVALID_LEN) {
        error_report("%s: failed to get context length ret=%d fw_error=%d '%s'",
                    __func__, ret, *fw_err, fw_error_to_str(*fw_err));
        ret = -1;
        goto err;
    }

    if (update.trans_len <= INT_MAX && update.hdr_len <= INT_MAX) {
        *context_len = update.trans_len;
        *hdr_len = update.hdr_len;
    }
    ret = 0;
err:
    return ret;
}

static int
csv3_send_encrypt_context(Csv3GuestState *s, QEMUFile *f, uint64_t *bytes_sent)
{
    int ret, fw_error = 0;
    int context_len = 0;
    int hdr_len = 0;
    guchar *trans;
    guchar *hdr;
    struct kvm_csv3_send_encrypt_context update = { };

    ret = csv3_send_get_context_len(&fw_error, &context_len, &hdr_len);
    if (context_len < 1 || hdr_len < 1) {
        error_report("%s: fail to get context length fw_error=%d '%s'",
                     __func__, fw_error, fw_error_to_str(fw_error));
        return 1;
    }

    /* allocate transport buffer */
    trans = g_new(guchar, context_len);
    hdr = g_new(guchar, hdr_len);

    update.hdr_uaddr = (uintptr_t)hdr;
    update.hdr_len = hdr_len;
    update.trans_uaddr = (uintptr_t)trans;
    update.trans_len = context_len;

    trace_kvm_csv3_send_encrypt_context(trans, update.trans_len);

    ret = csv3_ioctl(KVM_CSV3_SEND_ENCRYPT_CONTEXT, &update, &fw_error);
    if (ret) {
        error_report("%s: SEND_ENCRYPT_CONTEXT ret=%d fw_error=%d '%s'",
                     __func__, ret, fw_error, fw_error_to_str(fw_error));
        goto err;
    }

    qemu_put_be32(f, update.hdr_len);
    qemu_put_buffer(f, (uint8_t *)update.hdr_uaddr, update.hdr_len);
    *bytes_sent += 4 + update.hdr_len;

    qemu_put_be32(f, update.trans_len);
    qemu_put_buffer(f, (uint8_t *)update.trans_uaddr, update.trans_len);
    *bytes_sent += 4 + update.trans_len;

err:
    g_free(trans);
    g_free(hdr);
    return ret;
}

static int
csv3_receive_encrypt_context(Csv3GuestState *s, QEMUFile *f)
{
    int ret = 1, fw_error = 0;
    gchar *hdr = NULL, *trans = NULL;
    struct kvm_csv3_receive_encrypt_context update = {};

    /* get packet header */
    update.hdr_len = qemu_get_be32(f);

    hdr = g_new(gchar, update.hdr_len);
    qemu_get_buffer(f, (uint8_t *)hdr, update.hdr_len);
    update.hdr_uaddr = (uintptr_t)hdr;

    /* get transport buffer */
    update.trans_len = qemu_get_be32(f);

    trans = g_new(gchar, update.trans_len);
    update.trans_uaddr = (uintptr_t)trans;
    qemu_get_buffer(f, (uint8_t *)update.trans_uaddr, update.trans_len);

    trace_kvm_csv3_receive_encrypt_context(trans, update.trans_len, hdr, update.hdr_len);

    ret = csv3_ioctl(KVM_CSV3_RECEIVE_ENCRYPT_CONTEXT, &update, &fw_error);
    if (ret) {
        error_report("Error RECEIVE_ENCRYPT_CONTEXT ret=%d fw_error=%d '%s'",
                     ret, fw_error, fw_error_to_str(fw_error));
        goto err;
    }

err:
    g_free(trans);
    g_free(hdr);
    return ret;
}

int csv3_save_outgoing_context(QEMUFile *f, uint64_t *bytes_sent)
{
    Csv3GuestState *s = &csv3_guest;

    /* send csv3 context. */
    return csv3_send_encrypt_context(s, f, bytes_sent);
}

int csv3_load_incoming_context(QEMUFile *f)
{
    Csv3GuestState *s = &csv3_guest;

    /* receive csv3 context. */
    return csv3_receive_encrypt_context(s, f);
}
