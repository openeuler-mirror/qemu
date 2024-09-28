/*
 * QEMU CSV stub
 *
 * Copyright Hygon Info Technologies Ltd. 2024
 *
 * Authors:
 *      Han Liyang <hanliyang@hygon.cn>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "csv.h"

bool csv_kvm_cpu_reset_inhibit;
uint32_t kvm_hygon_coco_ext;
uint32_t kvm_hygon_coco_ext_inuse;
