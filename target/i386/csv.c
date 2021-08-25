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

#include <linux/kvm.h>

#ifdef CONFIG_NUMA
#include <numaif.h>
#endif

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

        csv3_guest.sev_fd = fd;
        csv3_guest.state = state;
        csv3_guest.sev_ioctl = ops->sev_ioctl;
        csv3_guest.fw_error_to_str = ops->fw_error_to_str;
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
