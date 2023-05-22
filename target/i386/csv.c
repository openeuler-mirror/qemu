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

#include <linux/kvm.h>

#ifdef CONFIG_NUMA
#include <numaif.h>
#endif

#include "trace.h"
#include "cpu.h"
#include "sev.h"
#include "csv.h"

bool csv_kvm_cpu_reset_inhibit;

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
