/*
 * QEMU add virtcca cvm live migration feature.
 * 
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2027. All rights reserved.
 *
 * Authors:
 *      Zhu Yifan <zhuyifan30@huawei.com>
 *      He JingXian <hejingxian@huawei.com>
 *      Pan HengChang <panhengchang@huawei.com>
 *      Liu Hao <liuhao365@h-partners.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 * 
 */

#include "qemu/osdep.h"
#include "qemu-file.h"
#include "cgs.h"
#include "target/arm/kvm_arm.h"
#include "migration/misc.h"
#include "qemu/error-report.h"
#include "hw/boards.h"

static inline uint64_t virtcca_mig_stream_get_mbmd_bytes(virtCCAMigStream *stream)
{
    return 0;
}

static int virtcca_mig_savevm_state_start(QEMUFile *f)
{
    return 0;
}

static long virtcca_mig_savevm_state_ram_start_epoch(QEMUFile *f)
{
    return 0;
}

bool virtcca_is_zero_page(uint32_t channel_id, hwaddr cgs_private_gpa, size_t len)
{
    return 0;
}

int virtcca_import_zero_page(uint32_t channel_id, void *host)
{
    return 0;
}

static long virtcca_mig_savevm_state_ram(QEMUFile *f, uint32_t channel_id,
                                         hwaddr gpa)
{
    return 0;
}

static int virtcca_mig_savevm_state_pause(void)
{
    return 0;
}

static int virtcca_mig_savevm_state_end(QEMUFile *f)
{
    return 0;
}

/* check the mig */
static bool virtcca_mig_is_ready(bool is_src, const char *dst_ip, uint16_t dst_port)
{
    return false;
}

/* after the cgs savevm setup, enter the virtcca stream setup procedure */
static int virtcca_mig_stream_setup(uint32_t nr_channels, uint32_t nr_pages)
{
    return 0;
}

static void virtcca_mig_cleanup(void)
{
    return;
}

static void virtcca_mig_loadvm_state_cleanup(void)
{
    return;
}

static int virtcca_mig_savevm_state_abort(void)
{
    return 0;
}


static int virtcca_mig_loadvm_state(QEMUFile *f, uint32_t channel_id)
{
    return 0;
}

static int virtcca_mig_create_tec(QEMUFile *f)
{
    return tmm_create_tec();
}

void vircca_mig_init(CgsMig *cgs_mig)
{
    cgs_mig->is_ready = virtcca_mig_is_ready;
    cgs_mig->savevm_state_setup = virtcca_mig_stream_setup;
    cgs_mig->savevm_state_start = virtcca_mig_savevm_state_start;
    cgs_mig->savevm_state_ram_start_epoch =
                        virtcca_mig_savevm_state_ram_start_epoch;
    cgs_mig->savevm_state_ram = virtcca_mig_savevm_state_ram;
    cgs_mig->savevm_state_pause = virtcca_mig_savevm_state_pause;
    cgs_mig->savevm_state_end = virtcca_mig_savevm_state_end;
    cgs_mig->savevm_state_cleanup = virtcca_mig_cleanup;
    cgs_mig->savevm_state_abort = virtcca_mig_savevm_state_abort;
    cgs_mig->loadvm_state_setup = virtcca_mig_stream_setup;
    cgs_mig->loadvm_state = virtcca_mig_loadvm_state;
    cgs_mig->loadvm_create_tec = virtcca_mig_create_tec;
    cgs_mig->loadvm_state_cleanup = virtcca_mig_loadvm_state_cleanup;
}
