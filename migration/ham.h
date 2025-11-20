/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: HAM: Migrate Operations
 */

#ifndef HAM_H
#define HAM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include "sysemu/kvm_int.h"
#include "qapi/qapi-types-migration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_SIZE_2M (INT64_C(1) << 21)
#define BATCH_NUM 4

typedef enum {
    HAM_LOG_DEBUG = 0,
    HAM_LOG_ERROR = 3,
} HamLogLevel;

typedef struct {
    uint32_t uuid;
    uintptr_t hva;
    size_t size;
} HamRamBlock;

typedef struct {
    pid_t pid;
    uint16_t scna;
    uint32_t num;
    HamRamBlock blockList[BATCH_NUM];
} HamRamInfo;

typedef struct {
    uint32_t numaId;
    size_t size;
} HamNuma;

typedef struct {
    pid_t pid;
    uint32_t num;
    HamNuma numaList[BATCH_NUM];
} HamNumaInfo;

typedef struct {
    int32_t uuid;
    size_t hvaNum;
    uintptr_t *hvaList;
} HamRamPages;

typedef void (*ExternalLog)(int level, const char *funcname, int linenr, const char *logBuf);

typedef struct ham_migration_ops {
    void (*external_log_set)(ExternalLog logFunc);
    int32_t (*ham_register)(HamRamInfo *src, HamNumaInfo *dst);
    int32_t (*migrate)(HamRamPages *ramList, size_t ram_num, int32_t step);
    void (*ham_unregister)(void);
    int32_t (*rollback)(pid_t pid);
    int32_t (*pgtable_modify)(bool cacheable);
} ham_migration_ops;

int ham_pages_commit(void);

void ham_migrate_prepare(MigrationState *s);

void ham_migrate_cleanup(void);

bool ham_is_vm_ram(size_t page_size);

void ham_madvise_page(void);

bool ham_should_complete_migration(MigrationState *s);

bool ham_should_skip_dirty_log(KVMSlot *mem);

#ifdef __cplusplus
}
#endif
#endif
