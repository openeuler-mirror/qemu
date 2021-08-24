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

#include "cpu.h"
#include "sev.h"
#include "csv.h"

bool csv_kvm_cpu_reset_inhibit;

Csv3GuestState csv3_guest = { 0 };

bool
csv3_enabled(void)
{
    if (!is_hygon_cpu())
        return false;

    return sev_es_enabled() && (csv3_guest.policy & GUEST_POLICY_CSV3_BIT);
}
