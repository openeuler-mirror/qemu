/*
 * QEMU Migration for Confidential Guest Support
 *
 * Copyright (C) 2022 Intel Corp.
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2027. All rights reserved.
 *
 * Authors:
 *      Wei Wang <wei.w.wang@intel.com>
 *      Zhu Yifan <zhuyifan30@huawei.com>
 *      He JingXian <hejingxian@huawei.com>
 *      Pan HengChang <panhengchang@huawei.com>
 *      Liu Hao <liuhao365@h-partners.com>
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_MIGRATION_CGS_H
#define QEMU_MIGRATION_CGS_H
#include <linux/kvm.h>
#include "qemu/osdep.h"
#include "migration.h"
#include "multifd.h"

#define CGS_PRIVATE_GPA_INVALID (~0UL)
#define UEFI_MAX_SIZE 0x8000000

typedef struct CgsMig {
    bool (*is_ready)(bool is_src, const char *dst_ip, uint16_t dst_port);
    int (*savevm_state_setup)(uint32_t nr_channels, uint32_t nr_pages);
    int (*savevm_state_start)(QEMUFile *f);
    long (*savevm_state_ram_start_epoch)(QEMUFile *f);
    long (*savevm_state_ram)(QEMUFile *f, uint32_t channel_id, hwaddr gpa);
    int (*savevm_state_pause)(void);
    int (*savevm_state_end)(QEMUFile *f);
    int (*savevm_state_abort)(void);
    long (*savevm_state_ram_cancel)(QEMUFile *f, hwaddr gpa);
    void (*savevm_state_cleanup)(void);
    int (*loadvm_state_setup)(uint32_t nr_channels, uint32_t nr_pages);
    int (*loadvm_state)(QEMUFile *f, uint32_t channel_id);
    int (*loadvm_create_tec)(QEMUFile *f);
    void (*loadvm_state_cleanup)(void);
    /* Multifd support */
    uint32_t (*iov_num)(uint32_t page_batch_num);
    int (*multifd_send_prepare)(MultiFDSendParams *p, Error **errp);
    int (*multifd_recv_pages)(MultiFDRecvParams *p, Error **errp);
} CgsMig;

bool cgs_mig_is_ready(bool is_src, const char *dst_ip, uint16_t dst_port);
int cgs_mig_savevm_state_setup(QEMUFile *f);
int cgs_mig_savevm_state_start(QEMUFile *f);
long cgs_ram_save_start_epoch(QEMUFile *f);
long cgs_mig_savevm_state_ram(QEMUFile *f,
                              RAMBlock *block, ram_addr_t offset, hwaddr gpa);
bool cgs_mig_savevm_state_need_ram_cancel(void);
long cgs_mig_savevm_state_ram_cancel(QEMUFile *f, RAMBlock *block,
                                     ram_addr_t offset, hwaddr gpa);
int cgs_mig_savevm_state_pause(void);
int cgs_mig_savevm_state_end(QEMUFile *f);
int cgs_mig_savevm_state_abort(void);
void cgs_mig_savevm_state_cleanup(void);
int cgs_mig_loadvm_state_setup(QEMUFile *f);
int cgs_mig_loadvm_state(QEMUFile *f, uint32_t channel_id);
int cgs_mig_loadvm_create_tec(QEMUFile *f);
void cgs_mig_loadvm_state_cleanup(void);
int cgs_mig_multifd_send_prepare(MultiFDSendParams *p, Error **errp);
int cgs_mig_multifd_recv_pages(MultiFDRecvParams *p, Error **errp);
uint32_t cgs_mig_iov_num(uint32_t page_batch_num);
void cgs_mig_init(void);
int virtcca_import_zero_page(uint32_t channel_id, void *host);
void vircca_mig_init(CgsMig *cgs_mig);
bool virtcca_is_swiotlb(void *host);

bool virtcca_is_zero_page(uint32_t channel_id, hwaddr cgs_private_gpa, size_t len);

typedef struct virtCCAMigInfo {
    uint64_t swiotlb_start;
    uint64_t swiotlb_end;
} virtCCAMigInfo;
 
typedef struct virtCCAMigStream {
    int fd;
    void *mbmd;
    void *buf_list;
    void *mac_list;
    void *gpa_list;
} virtCCAMigStream;
 
typedef struct virtCCAMigState {
    uint32_t nr_streams;
    virtCCAMigStream *streams;
    uint64_t swiotlb_start;
    uint64_t swiotlb_end;
} virtCCAMigState;
 
extern virtCCAMigState virtCCA_mig;

struct mig_cvm {
    /* used by guest cvm */
    uint8_t  version; /* kvm version of migcvm */
    uint64_t migvm_cid; /* hash of migcvm, from and used by guest cvm */
};
#endif
