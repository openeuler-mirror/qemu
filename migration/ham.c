/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: HAM: Migrate Operations
 */

#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "qemu/log.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "migration.h"
#include "dlfcn.h"
#include "ram.h"
#include "qapi/qapi-commands-migration.h"
#include "exec/ramblock.h"
#include "options.h"
#include "qemu-file.h"
#include "ham.h"

#define HAM_LIB_PATH    "libham.so"

static void *handle_ham = NULL;

static ham_migration_ops ham_migration_ops_instance;

typedef struct dl_funcs {
    const char *func_name;
    void **func;
} dl_funcs;

static dl_funcs ham_dlfunc_list[] = {
    {.func_name = "ubturbo_ham_external_log_set", .func = (void**)&ham_migration_ops_instance.external_log_set},
    {.func_name = "ubturbo_ham_register", .func = (void**)&ham_migration_ops_instance.ham_register},
    {.func_name = "ubturbo_ham_migrate", .func = (void**)&ham_migration_ops_instance.migrate},
    {.func_name = "ubturbo_ham_pgtable_modify", .func = (void**)&ham_migration_ops_instance.pgtable_modify},
    {.func_name = "ubturbo_ham_unregister", .func = (void**)&ham_migration_ops_instance.ham_unregister},
    {.func_name = "ubturbo_ham_rollback", .func = (void**)&ham_migration_ops_instance.rollback},
};

const char *log_level_str[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

static int32_t g_migrate_round = 0;
static HamNumaInfo g_dst_numa = { .num = 0 };
static HamRamInfo g_src_ram = { .num = 0 };

bool ham_is_vm_ram(size_t page_size)
{
    return page_size == PAGE_SIZE_2M;
}

static void ham_dlfunc_list_set_null(void)
{
    int num = sizeof(ham_dlfunc_list) / sizeof(ham_dlfunc_list[0]);
    for (int i = 0; i < num; i++) {
        *ham_dlfunc_list[i].func = NULL;
    }
}

static void ham_dlfunc_close(void)
{
    if (handle_ham) {
        (void)dlclose(handle_ham);
        handle_ham = NULL;
    }
    ham_dlfunc_list_set_null();
}

static int ham_dlfunc_open(void)
{
    char *error = NULL;
    int num = sizeof(ham_dlfunc_list) / sizeof(ham_dlfunc_list[0]);

    ham_dlfunc_list_set_null();
    handle_ham = dlopen(HAM_LIB_PATH, RTLD_LAZY);
    if (!handle_ham) {
        qemu_log("HAM: dlopen error: %s\n", dlerror());
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        *ham_dlfunc_list[i].func = dlsym(handle_ham, ham_dlfunc_list[i].func_name);
        if ((error = dlerror()) != NULL) {
            qemu_log("HAM: dlsym error: %s while getting %s\n", error, ham_dlfunc_list[i].func_name);
            ham_dlfunc_close();
            return -1;
        }
    }

    return 0;
}

static int ham_dlfunc_init(void)
{
    int ret;

    ret = ham_dlfunc_open();
    if (ret < 0) {
        qemu_log("HAM: open ham dlfunc failed\n");
        return ret;
    }

    return 0;
}

static void ham_external_log(int level, const char *funcname, int linenr, const char *logBuf)
{
    if (level >= HAM_LOG_DEBUG && level <= HAM_LOG_ERROR) {
        qemu_log("[%s][%s:%d]:%s", log_level_str[level], funcname, linenr, logBuf);
    } else {
        qemu_log("[UNKNOWN][%s:%d]:%s", funcname, linenr, logBuf);
    }
}

static int ham_prepare(HamRamInfo *src)
{
    int ret;

    ret = ham_dlfunc_init();
    if (ret) {
        qemu_log("HAM: dlfunc init fail, ret:%d\n", ret);
        return ret;
    }

    g_migrate_round = 0;
    ham_migration_ops_instance.external_log_set(ham_external_log);

    ret = ham_migration_ops_instance.ham_register(src, &g_dst_numa);
    if (ret) {
        qemu_log("HAM: start migration fail, ret:%d\n", ret);
        return ret;
    }
    return 0;
}

int ham_pages_commit(void)
{
    int ret = ham_migration_ops_instance.migrate(NULL, 0, g_migrate_round);
    if (ret != 0) {
        qemu_log("HAM: page migration failed, ret:%d\n", ret);
        return ret;
    }

    g_migrate_round++;
    return 0;
}

static int ham_pages_rollback(void)
{
    pid_t pid = getpid();
    int ret = ham_dlfunc_init();
    if (ret) {
        return ret;
    }

    ret = ham_migration_ops_instance.rollback(pid);
    ham_dlfunc_close();
    return ret;
}

static int ham_init_ram_blocks(HamRamInfo *ram_info)
{
    RAMBlock *ram_block = NULL;
    uint32_t uuid = 0;

    ram_info->pid = getpid();
    ram_info->num = 0;
    WITH_RCU_READ_LOCK_GUARD() {
        RAMBLOCK_FOREACH_MIGRATABLE(ram_block) {
            if (!ham_is_vm_ram(ram_block->page_size)) {
                continue;
            }
            if (uuid >= BATCH_NUM) {
                qemu_log("HAM: ram block num exceeds, limit:%u\n", BATCH_NUM);
                return -E2BIG;
            }
            ram_info->blockList[ram_info->num].uuid = uuid++;
            ram_info->blockList[ram_info->num].hva = (uintptr_t)ram_block->host;
            ram_info->blockList[ram_info->num].size = ram_block->used_length;
            ram_info->num++;
        }
    }
    return 0;
}

void ham_migrate_prepare(MigrationState *s)
{
    Error *err = NULL;
    int ret;

    if (!migrate_use_ldst()) {
        return;
    }

    ret = ham_init_ram_blocks(&g_src_ram);
    if (ret) {
        error_setg(&err, "init ram block fail, ret:%d", ret);
        goto fail;
    }

    ret = ham_prepare(&g_src_ram);
    if (ret) {
        error_setg(&err, "migrate ham prepare fail, ret:%d", ret);
        goto fail;
    }
    return;

fail:
    migrate_set_error(s, err);
    error_report_err(err);
    qemu_file_set_error(s->to_dst_file, ret);
}

void ham_migrate_cleanup(void)
{
    if (!migrate_use_ldst()) {
        return;
    }

    ham_migration_ops_instance.ham_unregister();
    ham_dlfunc_close();
}

void ham_madvise_page(void)
{
    RAMBlock *ram_block;
    int64_t start, end;

    start = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);
    WITH_RCU_READ_LOCK_GUARD() {
        RAMBLOCK_FOREACH_MIGRATABLE(ram_block) {
            if (!ham_is_vm_ram(ram_block->page_size)) {
                continue;
            }
            madvise(ram_block->host, ram_block->used_length, MADV_POPULATE_WRITE);
        }
    }
    end = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);
    qemu_log("HAM: madvise cost time:%ld ms\n", end - start);
}

static int ham_modify_pgtable(void)
{
    HamRamInfo ramInfo = { .num = 0 };

    int ret = ham_dlfunc_init();
    if (ret) {
        return ret;
    }
    ret = ham_init_ram_blocks(&ramInfo);
    if (ret) {
        goto close_dlfunc;
    }
    ret = ham_migration_ops_instance.ham_register(&ramInfo, NULL);
    if (ret) {
        qemu_log("HAM: start migration fail, ret:%d\n", ret);
        goto stop_mig;
    }
    ret = ham_migration_ops_instance.pgtable_modify(true);
    if (ret) {
        qemu_log("HAM: modify pgtable fail, ret:%d\n", ret);
    }

stop_mig:
    ham_migration_ops_instance.ham_unregister();
close_dlfunc:
    ham_dlfunc_close();
    return ret;
}