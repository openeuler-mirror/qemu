/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2025. All rights reserved.
 *
 * Description: Support vm migration using the protocol and interfaces provided by the URMA component.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"

#include "urma.h"
#include "migration.h"
#include "multifd.h"
#include "migration-stats.h"
#include "qemu-file.h"
#include "ram.h"
#include "rdma.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qemu/module.h"
#include "qemu/rcu.h"
#include "qemu/sockets.h"
#include "qemu/bitmap.h"
#include "qemu/coroutine.h"
#include "exec/memory.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "trace.h"
#include "qom/object.h"
#include "options.h"
#include <poll.h>
#include "qemu/log.h"
#include <stdatomic.h>
#include "socket.h"
#include "exec/target_page.h"
#include "sysemu/sysemu.h"
#include <dlfcn.h>
#include "crypto/random.h"
#include <ub/umdk/urma/uvs_api.h>

#define URMA_REG_CHUNK_SHIFT 21  /* 2 MB */

/* Do not merge data if larger than this. */
#define URMA_CHUNK_MERGE_MAX    (1 << URMA_REG_CHUNK_SHIFT)

#define URMA_MAX_POLL_TIME      100000000  /* ms */

#define URMA_DEV_DEFAULT_NAME   "bonding_dev_0"  /* default use bonding dev */
#define URMA_DEV_DEFAULT_IDX    0
#define UVS_SO_PATH             "libtpsa.so"

void *handle_urma = NULL;
static void *handle_uvs = NULL;
static const char *urma_dev_name = URMA_DEV_DEFAULT_NAME;
static int urma_dev_idx = URMA_DEV_DEFAULT_IDX;

urma_status_t (*urma_init_p)(urma_init_attr_t *conf);
urma_status_t (*urma_uninit_p)(void);
urma_device_t **(*urma_get_device_list_p)(int *num_devices);
void (*urma_free_device_list_p)(urma_device_t **device_list);
urma_device_t *(*urma_get_device_by_eid_p)(urma_eid_t eid, urma_transport_type_t type);
urma_eid_info_t *(*urma_get_eid_list_p)(urma_device_t *dev, uint32_t *cnt);
void (*urma_free_eid_list_p)(urma_eid_info_t *eid_list);
urma_status_t (*urma_query_device_p)(urma_device_t *dev, urma_device_attr_t *dev_attr);
urma_context_t *(*urma_create_context_p)(urma_device_t *dev, uint32_t eid_index);
urma_status_t (*urma_delete_context_p)(urma_context_t *ctx);
urma_jfc_t *(*urma_create_jfc_p)(urma_context_t *ctx, urma_jfc_cfg_t *jfc_cfg);
urma_status_t (*urma_delete_jfc_p)(urma_jfc_t *jfc);
urma_jfs_t *(*urma_create_jfs_p)(urma_context_t *ctx, urma_jfs_cfg_t *jfs_cfg);
urma_status_t (*urma_delete_jfs_p)(urma_jfs_t *jfs);
urma_jfr_t *(*urma_create_jfr_p)(urma_context_t *ctx, urma_jfr_cfg_t *jfr_cfg);
urma_status_t (*urma_delete_jfr_p)(urma_jfr_t *jfr);
urma_target_jetty_t *(*urma_import_jfr_p)(urma_context_t *ctx, urma_rjfr_t *rjfr, urma_token_t *token_value);
urma_status_t (*urma_unimport_jfr_p)(urma_target_jetty_t *target_jfr);
urma_status_t (*urma_advise_jfr_p)(urma_jfs_t *jfs, urma_target_jetty_t *tjfr);
urma_jfce_t *(*urma_create_jfce_p)(urma_context_t *ctx);
urma_status_t (*urma_delete_jfce_p)(urma_jfce_t *jfce);
urma_target_seg_t *(*urma_register_seg_p)(urma_context_t *ctx, urma_seg_cfg_t *seg_cfg);
urma_status_t (*urma_unregister_seg_p)(urma_target_seg_t *target_seg);
urma_target_seg_t *(*urma_import_seg_p)(
    urma_context_t *ctx, urma_seg_t *seg, urma_token_t *token_value, uint64_t addr, urma_import_seg_flag_t flag);
urma_status_t (*urma_unimport_seg_p)(urma_target_seg_t *tseg);
urma_status_t (*urma_write_p)(urma_jfs_t *jfs, urma_target_jetty_t *target_jfr, urma_target_seg_t *dst_tseg,
    urma_target_seg_t *src_tseg, uint64_t dst, uint64_t src, uint32_t len, urma_jfs_wr_flag_t flag, uint64_t user_ctx);
int (*urma_poll_jfc_p)(urma_jfc_t *jfc, int cr_cnt, urma_cr_t *cr);
urma_status_t (*urma_user_ctl_p)(urma_context_t *ctx, urma_user_ctl_in_t *in, urma_user_ctl_out_t *out);
urma_status_t (*urma_post_jfs_wr_p)(urma_jfs_t *jfs, urma_jfs_wr_t *wr, urma_jfs_wr_t **bad_wr);
int (*uvs_get_path_set_p)(const uvs_eid_t *src_bonding_eid, const uvs_eid_t *dst_bonding_eid,
                          enum uvs_tp_type tp_type, bool iodie_level, uvs_path_set_t *uvs_path_set);

typedef struct dl_functions {
    const char *func_name;
    void **func;
} dl_functions;

dl_functions urma_dlfunc_list[] = {
    {.func_name = "urma_init", .func = (void **)&urma_init_p},
    {.func_name = "urma_uninit", .func = (void **)&urma_uninit_p},
    {.func_name = "urma_get_device_list", .func = (void **)&urma_get_device_list_p},
    {.func_name = "urma_free_device_list", .func = (void **)&urma_free_device_list_p},
    {.func_name = "urma_get_device_by_eid", .func = (void **)&urma_get_device_by_eid_p},
    {.func_name = "urma_get_eid_list", .func = (void **)&urma_get_eid_list_p},
    {.func_name = "urma_free_eid_list", .func = (void **)&urma_free_eid_list_p},
    {.func_name = "urma_query_device", .func = (void **)&urma_query_device_p},
    {.func_name = "urma_create_context", .func = (void **)&urma_create_context_p},
    {.func_name = "urma_delete_context", .func = (void **)&urma_delete_context_p},
    {.func_name = "urma_create_jfc", .func = (void **)&urma_create_jfc_p},
    {.func_name = "urma_delete_jfc", .func = (void **)&urma_delete_jfc_p},
    {.func_name = "urma_create_jfs", .func = (void **)&urma_create_jfs_p},
    {.func_name = "urma_delete_jfs", .func = (void **)&urma_delete_jfs_p},
    {.func_name = "urma_create_jfr", .func = (void **)&urma_create_jfr_p},
    {.func_name = "urma_delete_jfr", .func = (void **)&urma_delete_jfr_p},
    {.func_name = "urma_import_jfr", .func = (void **)&urma_import_jfr_p},
    {.func_name = "urma_unimport_jfr", .func = (void **)&urma_unimport_jfr_p},
    {.func_name = "urma_advise_jfr", .func = (void **)&urma_advise_jfr_p},
    {.func_name = "urma_create_jfce", .func = (void **)&urma_create_jfce_p},
    {.func_name = "urma_delete_jfce", .func = (void **)&urma_delete_jfce_p},
    {.func_name = "urma_register_seg", .func = (void **)&urma_register_seg_p},
    {.func_name = "urma_unregister_seg", .func = (void **)&urma_unregister_seg_p},
    {.func_name = "urma_import_seg", .func = (void **)&urma_import_seg_p},
    {.func_name = "urma_unimport_seg", .func = (void **)&urma_unimport_seg_p},
    {.func_name = "urma_write", .func = (void **)&urma_write_p},
    {.func_name = "urma_poll_jfc", .func = (void **)&urma_poll_jfc_p},
    {.func_name = "urma_user_ctl", .func = (void **)&urma_user_ctl_p},
    {.func_name = "urma_post_jfs_wr", .func = (void **)&urma_post_jfs_wr_p},
};

static dl_functions uvs_dlfunc_list[] = {
    {.func_name = "uvs_get_path_set", .func = (void **)&uvs_get_path_set_p},
};

static void urma_dlfunc_list_set_null(void)
{
    for (int i = 0; i < ARRAY_SIZE(urma_dlfunc_list); i++) {
        *urma_dlfunc_list[i].func = NULL;
    }
}

static void uvs_dlfunc_list_set_null(void)
{
    for (int i = 0; i < ARRAY_SIZE(uvs_dlfunc_list); i++) {
        *uvs_dlfunc_list[i].func = NULL;
    }
}

static int dlfunc_dlsym_table(void *handle, dl_functions *table, size_t n)
{
    char *error = NULL;

    for (size_t i = 0; i < n; i++) {
        *table[i].func = dlsym(handle, table[i].func_name);
        if ((error = dlerror()) != NULL) {
            qemu_log("dlsym error: %s while getting %s", error, table[i].func_name);
            return -1;
        }
    }
    return 0;
}

static void urma_dlfunc_close(void)
{
    if (handle_urma) {
        (void)dlclose(handle_urma);
        handle_urma = NULL;
    }
    if (handle_uvs) {
        (void)dlclose(handle_uvs);
        handle_uvs = NULL;
    }
    urma_dlfunc_list_set_null();
    uvs_dlfunc_list_set_null();
}

static int migrate_get_urma_dlfunc(Error **errp)
{
    urma_dlfunc_list_set_null();
    uvs_dlfunc_list_set_null();
    handle_urma = dlopen(URMA_SO_PATH, RTLD_LAZY | RTLD_GLOBAL);
    if (!handle_urma) {
        qemu_log("dlopen error: %s", dlerror());
        return -1;
    }

    handle_uvs = dlopen(UVS_SO_PATH, RTLD_LAZY | RTLD_GLOBAL);
    if (!handle_uvs) {
        qemu_log("dlopen error: %s", dlerror());
        urma_dlfunc_close();
        return -1;
    }

    if (dlfunc_dlsym_table(handle_urma, urma_dlfunc_list, ARRAY_SIZE(urma_dlfunc_list)) < 0) {
        urma_dlfunc_close();
        return -1;
    }

    if (dlfunc_dlsym_table(handle_uvs, uvs_dlfunc_list, ARRAY_SIZE(uvs_dlfunc_list)) < 0) {
        urma_dlfunc_close();
        return -1;
    }

    return 0;
}

static int urma_dlfunc_init(Error **errp)
{
    int r;

    r = migrate_get_urma_dlfunc(errp);
    if (r < 0) {
        qemu_log("dlsym error, open urma dlfunc failed\n");
        return r;
    }

    return r;
}

static inline uint64_t urma_ram_chunk_index(const uint8_t *start,
                                            const uint8_t *host)
{
    return ((uintptr_t) host - (uintptr_t) start) >> URMA_REG_CHUNK_SHIFT;
}

static inline uint8_t *urma_ram_chunk_start(const URMALocalBlock *ram_block,
                                            uint64_t i)
{
    return (uint8_t *)(uintptr_t)(ram_block->local_host_addr +
                                  (i << URMA_REG_CHUNK_SHIFT));
}

static inline uint8_t *urma_ram_chunk_end(const URMALocalBlock *ram_block,
                                          uint64_t i)
{
    uint8_t *result = urma_ram_chunk_start(ram_block, i) +
                                           (1UL << URMA_REG_CHUNK_SHIFT);

    if (result > (ram_block->local_host_addr + ram_block->length)) {
        result = ram_block->local_host_addr + ram_block->length;
    }

    return result;
}

static void urma_add_block(URMAContext *urma, const char *block_name,
                           void *host_addr,
                           ram_addr_t block_offset, uint64_t length)
{
    URMALocalBlocks *local = &urma->local_ram_blocks;
    URMALocalBlock *block;
    URMALocalBlock *old = local->block;

    local->block = g_new0(URMALocalBlock, local->nb_blocks + 1);

    if (local->nb_blocks) {
        int x;
        if (urma->blockmap) {
            for (x = 0; x < local->nb_blocks; x++) {
                g_hash_table_remove(urma->blockmap,
                                    (void *)(uintptr_t)old[x].offset);
                g_hash_table_insert(urma->blockmap,
                                    (void *)(uintptr_t)old[x].offset,
                                    &local->block[x]);
            }
        }
        memcpy(local->block, old, sizeof(URMALocalBlock) * local->nb_blocks);
        g_free(old);
    }

    block = &local->block[local->nb_blocks];

    block->block_name = g_strdup(block_name);
    block->local_host_addr = host_addr;
    block->offset = block_offset;
    block->length = length;
    block->index = local->nb_blocks;
    block->src_index = ~0U;                /* Filled in by the receipt of the block list */
    block->nb_chunks = urma_ram_chunk_index(host_addr, host_addr + length) + 1UL;
    block->transit_bitmap = bitmap_new(block->nb_chunks);
    bitmap_clear(block->transit_bitmap, 0, block->nb_chunks);
    block->unregister_bitmap = bitmap_new(block->nb_chunks);
    bitmap_clear(block->unregister_bitmap, 0, block->nb_chunks);

    block->is_ram_block = local->init ? false : true;

    if (urma->blockmap) {
        g_hash_table_insert(urma->blockmap, (void *)(uintptr_t)block_offset, block);
    }

    local->nb_blocks++;
}

static int qemu_urma_init_one_block(RAMBlock *rb, void *opaque)
{
    const char *block_name = qemu_ram_get_idstr(rb);
    void *host_addr = qemu_ram_get_host_addr(rb);
    ram_addr_t block_offset = qemu_ram_get_offset(rb);
    ram_addr_t length = qemu_ram_get_used_length(rb);
    urma_add_block(opaque, block_name, host_addr, block_offset, length);
    return 0;
}

static void qemu_urma_free_blocks(URMAContext *urma)
{
    URMALocalBlocks *local = &urma->local_ram_blocks;
    int i;

    for (i = 0; i < local->nb_blocks; i++) {
        URMALocalBlock *block = &local->block[i];

        if (urma->blockmap) {
            g_hash_table_remove(urma->blockmap, (void *)(uintptr_t)block->offset);
        }

        g_free(block->transit_bitmap);
        block->transit_bitmap = NULL;

        g_free(block->unregister_bitmap);
        block->unregister_bitmap = NULL;

        g_free(block->block_name);
        block->block_name = NULL;

        if (urma->blockmap) {
            g_hash_table_destroy(urma->blockmap);
            urma->blockmap = NULL;
        }
    }

    g_free(local->block);
    local->block = NULL;
    local->nb_blocks = 0;

    g_free(urma->dest_blocks);
    urma->dest_blocks = NULL;
}

static int qemu_urma_init_ram_blocks(URMAContext *urma)
{
    URMALocalBlocks *local = &urma->local_ram_blocks;
    int ret;

    if (urma->blockmap != NULL) {
        qemu_log("Ram blocks have been inited before!\n");
        return -EINVAL;
    }

    memset(local, 0, sizeof *local);
    ret = foreach_not_ignored_block(qemu_urma_init_one_block, urma);
    if (ret) {
        qemu_log("do qemu_urma_init_one_block failed, %d\n", ret);
        return ret;
    }

    urma->dest_blocks = g_new0(URMADestBlock,
                               urma->local_ram_blocks.nb_blocks);
    local->init = true;

    /* Build the hash that maps from offset to RAMBlock */
    urma->blockmap = g_hash_table_new(g_direct_hash, g_direct_equal);
    for (int i = 0; i < urma->local_ram_blocks.nb_blocks; i++) {
        g_hash_table_insert(urma->blockmap,
                (void *)(uintptr_t)urma->local_ram_blocks.block[i].offset,
                &urma->local_ram_blocks.block[i]);
    }

    return 0;
}

static void qemu_urma_data_free(URMAContext *urma)
{
    if (urma == NULL) {
        return;
    }

    g_free(urma->host);
    g_free(urma);
}

static URMAContext *qemu_urma_data_init(InetSocketAddress *saddr)
{
    URMAContext *urma = NULL;

    urma = g_new0(URMAContext, 1);
    urma->current_index = -1;
    urma->current_chunk = -1;

    urma->host = g_strdup(saddr->host);
    urma->port = atoi(saddr->port);

    return urma;
}

static int qemu_urma_pick_eid_from_device(urma_device_t *dev, urma_eid_info_t *info_out)
{
    urma_eid_info_t *eid_list;
    uint32_t eid_cnt;
    unsigned int pick;

    if (dev == NULL || info_out == NULL) {
        return -EINVAL;
    }

    eid_list = urma_get_eid_list_p(dev, &eid_cnt);
    if (eid_list == NULL || eid_cnt == 0) {
        return -EINVAL;
    }

    if (urma_dev_idx >= 0 && (uint32_t)urma_dev_idx < eid_cnt) {
        pick = urma_dev_idx;
    } else {
        qemu_log("Invalid urma_dev_idx, use the first one.\n");
        pick = 0;
    }

    *info_out = eid_list[pick];
    urma_free_eid_list_p(eid_list);

    qemu_log("Use the eid%d: "EID_FMT".\n", info_out->eid_index, EID_ARGS(info_out->eid));

    return 0;
}

static int qemu_get_urma_eid_index(urma_device_t *dev)
{
    urma_eid_info_t info;
    int ret;

    ret = qemu_urma_pick_eid_from_device(dev, &info);
    if (ret) {
        return -1;
    }
    return info.eid_index;
}

static int qemu_get_route_eid(const urma_eid_t *src_eid, const urma_eid_t *dst_eid,
                              urma_eid_t *route_src_eid, urma_eid_t *route_dst_eid)
{
    uvs_path_set_t path_set = {0};
    int ret_fwd;
    uint32_t len_fwd;

    if (src_eid == NULL || dst_eid == NULL ||
        route_src_eid == NULL || route_dst_eid == NULL) {
        return -EINVAL;
    }

    if (uvs_get_path_set_p == NULL) {
        qemu_log("uvs_get_path_set is not loaded\n");
        return -EINVAL;
    }

    qemu_log("route query input src_eid: " EID_FMT ", dst_eid: " EID_FMT "\n",
             EID_ARGS(*src_eid), EID_ARGS(*dst_eid));

    ret_fwd = uvs_get_path_set_p((uvs_eid_t *)src_eid, (uvs_eid_t *)dst_eid, UVS_CTP, true, &path_set);
    len_fwd = path_set.path_count;
    if (ret_fwd != 0 || len_fwd == 0) {
        qemu_log("failed to call uvs_get_path_set: ret=%d len=%u\n",
                 ret_fwd, len_fwd);
        return -EINVAL;
    }

    memcpy(route_src_eid->raw, path_set.paths[0].src_eid.raw, sizeof(route_src_eid->raw));
    memcpy(route_dst_eid->raw, path_set.paths[0].dst_eid.raw, sizeof(route_dst_eid->raw));
    qemu_log("Use route 0 src_eid: "EID_FMT", dst_eid: "EID_FMT".\n",
             EID_ARGS(*route_src_eid),
             EID_ARGS(*route_dst_eid));

    return 0;
}

static int qemu_get_urma_create_context_args_by_route(const urma_eid_t *src_eid,
                                                       const urma_eid_t *dst_eid,
                                                       const urma_eid_t *local_eid,
                                                       urma_device_t **dev_out,
                                                       uint32_t *eid_index_out)
{
    urma_eid_t route_src_eid, route_dst_eid;
    urma_eid_t local_route_eid;
    urma_device_t *route_dev = NULL;
    urma_eid_info_t *eid_list = NULL;
    uint32_t cnt = 0;
    uint32_t eid_index = (uint32_t)-1;
    int i;
    int ret;

    if (src_eid == NULL || dst_eid == NULL || local_eid == NULL ||
        dev_out == NULL || eid_index_out == NULL) {
        return -EINVAL;
    }

    ret = qemu_get_route_eid(src_eid, dst_eid, &route_src_eid, &route_dst_eid);
    if (ret) {
        return ret;
    }

    if (memcmp(local_eid, src_eid, sizeof(urma_eid_t)) == 0) {
        local_route_eid = route_src_eid;
    } else if (memcmp(local_eid, dst_eid, sizeof(urma_eid_t)) == 0) {
        local_route_eid = route_dst_eid;
    } else {
        qemu_log("URMA: resolve by route local_eid mismatch local=" EID_FMT " src=" EID_FMT
                 " dst=" EID_FMT "\n",
                 EID_ARGS(*local_eid), EID_ARGS(*src_eid), EID_ARGS(*dst_eid));
        return -EINVAL;
    }

    if (urma_get_device_by_eid_p == NULL) {
        qemu_log("URMA: urma_get_device_by_eid is not loaded\n");
        return -EINVAL;
    }

    route_dev = urma_get_device_by_eid_p(local_route_eid, URMA_TRANSPORT_UB);
    if (route_dev == NULL) {
        qemu_log("URMA: get route device by eid failed, route_eid=" EID_FMT "\n",
                 EID_ARGS(local_route_eid));
        return -ENODEV;
    }

    eid_list = urma_get_eid_list_p(route_dev, &cnt);
    if (eid_list == NULL || cnt == 0) {
        qemu_log("URMA: route device %s eid list empty\n", route_dev->name);
        return -EINVAL;
    }

    for (i = 0; i < (int)cnt; i++) {
        if (memcmp(&eid_list[i].eid, &local_route_eid, sizeof(urma_eid_t)) == 0) {
            eid_index = eid_list[i].eid_index;
            break;
        }
    }

    urma_free_eid_list_p(eid_list);

    if (eid_index == (uint32_t)-1) {
        qemu_log("URMA: no eid_index on %s for route_eid=" EID_FMT "\n",
                 route_dev->name, EID_ARGS(local_route_eid));
        return -EINVAL;
    }

    *dev_out = route_dev;
    *eid_index_out = eid_index;
    return 0;
}

static urma_device_t *qemu_get_urma_device(URMAContext *ctx)
{
    int i, device_num = 0;
    urma_device_t *urma_dev = NULL;
    urma_device_t **device_list = urma_get_device_list_p(&device_num);

    if (device_list == NULL || device_num == 0) {
        qemu_log("Failed to get device list, errno: %d\n", errno);
        return NULL;
    }

    for (i = 0; i < device_num; i++) {
        if (urma_dev_name != NULL && strcmp(device_list[i]->name, urma_dev_name) == 0) {
            urma_dev = device_list[i];
            break;
        }
    }

    /* If the specified device cannot be found, use the first device */
    if (urma_dev == NULL) {
        qemu_log("Cannot find the device %s, use the first device\n", urma_dev_name);
        urma_dev = device_list[0];
    }

    urma_free_device_list_p(device_list);
    return urma_dev;
}

static int qemu_get_default_bonding_eid(URMAContext *ctx, urma_eid_t *bond_eid_out)
{
    urma_device_t *bond_dev;
    urma_eid_info_t info;
    int ret;

    if (ctx == NULL || bond_eid_out == NULL) {
        return -EINVAL;
    }

    bond_dev = qemu_get_urma_device(ctx);
    if (bond_dev == NULL) {
        return -EINVAL;
    }

    ret = qemu_urma_pick_eid_from_device(bond_dev, &info);
    if (ret) {
        return ret;
    }

    *bond_eid_out = info.eid;
    return 0;
}

static int qemu_send_bonding_eid_packet(QEMUFile *f_write, const urma_eid_t *eid)
{
    if (f_write == NULL || eid == NULL) {
        return -EINVAL;
    }

    qemu_put_buffer(f_write, (const uint8_t *)eid, sizeof(*eid));
    if (qemu_fflush(f_write) < 0) {
        qemu_log("URMA: flush bonding eid packet failed\n");
        return -EINVAL;
    }
    return 0;
}

static int qemu_recv_bonding_eid_packet(QEMUFile *f_read, urma_eid_t *eid)
{
    if (f_read == NULL || eid == NULL) {
        return -EINVAL;
    }

    if (qemu_get_buffer(f_read, (uint8_t *)eid, sizeof(*eid)) != sizeof(*eid)) {
        qemu_log("URMA: recv bonding eid packet failed\n");
        return -EINVAL;
    }

    return 0;
}

static int qemu_exchange_bonding_eid(QEMUFile *f_read, QEMUFile *f_write,
                                     const urma_eid_t *local_bond,
                                     urma_eid_t *peer_bond, bool server)
{
    if (f_read == NULL || f_write == NULL || local_bond == NULL || peer_bond == NULL) {
        return -EINVAL;
    }

    if (server) {
        if (qemu_recv_bonding_eid_packet(f_read, peer_bond)) {
            return -EINVAL;
        }
        if (qemu_send_bonding_eid_packet(f_write, local_bond)) {
            return -EINVAL;
        }
    } else {
        if (qemu_send_bonding_eid_packet(f_write, local_bond)) {
            return -EINVAL;
        }

        if (qemu_recv_bonding_eid_packet(f_read, peer_bond)) {
            return -EINVAL;
        }
    }
    return 0;
}

static int qemu_urma_resolve_create_context_args(URMAContext *ctx,
                                                  urma_device_t **dev_out,
                                                  uint32_t *eid_index_out)
{
    int eid_index;
    int ret;
    urma_device_t *urma_dev;
    urma_eid_t local_bond;
    urma_eid_t peer_bond;
    uint32_t route_eid_index = 0;
    bool server = ctx->is_incoming;
    QEMUFile *f_read;
    QEMUFile *f_write;

    if (ctx == NULL || dev_out == NULL || eid_index_out == NULL) {
        return -EINVAL;
    }

    if (migrate_onecopy_ram()) {
        urma_dev = qemu_get_urma_device(ctx);
        if (urma_dev == NULL) {
            qemu_log("URMA: urma get device failed, errno: %d\n", errno);
            return -EINVAL;
        }

        ret = urma_query_device_p(urma_dev, &ctx->dev_attr);
        if (ret) {
            qemu_log("URMA: Failed to query device %s, ret: %d, errno: %d\n", urma_dev->name, ret, errno);
            return ret;
        }

        eid_index = qemu_get_urma_eid_index(urma_dev);
        if (eid_index < 0) {
            qemu_log("URMA: Failed to get eid index, ret: %d, errno: %d.\n", eid_index, errno);
            return eid_index;
        }

        *dev_out = urma_dev;
        *eid_index_out = (uint32_t)eid_index;
        return 0;
    }

    ret = qemu_get_default_bonding_eid(ctx, &local_bond);
    if (ret) {
        return ret;
    }

    peer_bond = local_bond;

    if (server) {
        MigrationIncomingState *mis = migration_incoming_get_current();

        if (mis == NULL || mis->from_src_file == NULL) {
            qemu_log("URMA: migration incoming file not ready\n");
            return -EINVAL;
        }

        f_read = mis->from_src_file;
        f_write = qemu_file_get_return_path(mis->from_src_file);
    } else {
        MigrationState *s = migrate_get_current();

        if (s == NULL || s->to_dst_file == NULL) {
            qemu_log("URMA: migration destination file not ready\n");
            return -EINVAL;
        }

        f_read = qemu_file_get_return_path(s->to_dst_file);
        f_write = s->to_dst_file;
    }

    if (f_read == NULL || f_write == NULL) {
        qemu_log("URMA: bonding eid exchange files not ready\n");
        return -EINVAL;
    }

    ret = qemu_exchange_bonding_eid(f_read, f_write, &local_bond, &peer_bond, server);
    if (ret) {
        return ret;
    }

    if (server) {
        ret = qemu_get_urma_create_context_args_by_route(&peer_bond, &local_bond,
                                                         &local_bond,
                                                         &urma_dev, &route_eid_index);
    } else {
        ret = qemu_get_urma_create_context_args_by_route(&local_bond, &peer_bond,
                                                         &local_bond,
                                                         &urma_dev, &route_eid_index);
    }
    if (ret) {
        return ret;
    }

    ret = urma_query_device_p(urma_dev, &ctx->dev_attr);
    if (ret) {
        qemu_log("URMA: Failed to query device %s, ret: %d, errno: %d\n", urma_dev->name, ret, errno);
        return ret;
    }

    *dev_out = urma_dev;
    *eid_index_out = route_eid_index;
    return 0;
}

static int qemu_init_jfs_post_list(URMAContext *urma)
{
    int i;
    urma_jfs_wr_t *wr;
    urma_jfs_wr_flag_t flag = { 0 };

    flag.bs.complete_enable = 1;

    for (i = 0; i < URMA_JFS_WR_LIST_LEN; i++) {
        wr = &urma->jfs_wr_list[i];

        wr->opcode = URMA_OPC_WRITE;
        wr->flag = flag;
        wr->rw.src.num_sge = 1;
        wr->rw.src.sge = &urma->src_sge[i];
        wr->rw.dst.num_sge = 1;
        wr->rw.dst.sge = &urma->dst_sge[i];
        wr->next = NULL;
    }

    return 0;
}

static int qemu_get_random_u32(uint32_t *rand_value)
{
    char random_char[URMA_TOKEN_LEN / CHAR_BIT];
    Error *local_err = NULL;

    if (qcrypto_random_bytes(random_char, sizeof(random_char), &local_err)) {
        qemu_log("cannot get qcrypto random bytes, %s\n", error_get_pretty(local_err));
        error_free(local_err);
        return -EINVAL;
    }

    memcpy(rand_value, random_char, sizeof(uint32_t));

    return 0;
}

static void qemu_urma_cleanup_context(URMAContext *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->tjfr) {
        urma_unimport_jfr_p(ctx->tjfr);
        ctx->tjfr = NULL;
    }

    if (ctx->jfr) {
        urma_delete_jfr_p(ctx->jfr);
        ctx->jfr = NULL;
    }

    if (ctx->jfs) {
        urma_delete_jfs_p(ctx->jfs);
        ctx->jfs = NULL;
    }

    if (ctx->jfc) {
        urma_delete_jfc_p(ctx->jfc);
        ctx->jfc = NULL;
    }

    if (ctx->jfce) {
        urma_delete_jfce_p(ctx->jfce);
        ctx->jfce = NULL;
    }

    if (ctx->urma_ctx) {
        (void)urma_delete_context_p(ctx->urma_ctx);
        ctx->urma_ctx = NULL;
    }

    qemu_log("clean up urma context success.\n");
}

int qemu_urma_init_context(URMAContext *ctx)
{
    int ret;
    urma_context_aggr_mode_t aggr_mode = URMA_AGGR_MODE_BALANCE;

    ctx->event_mode = false;

    urma_dev_name = URMA_DEV_DEFAULT_NAME;
    urma_dev_idx = URMA_DEV_DEFAULT_IDX;

    urma_device_t *urma_dev = NULL;
    uint32_t eid_index = 0;
    ret = qemu_urma_resolve_create_context_args(ctx, &urma_dev, &eid_index);
    if (ret) {
        qemu_log("URMA: Failed to resolve create context args, ret: %d, errno: %d\n", ret, errno);
        return ret;
    }

    ctx->urma_ctx = urma_create_context_p(urma_dev, eid_index);
    if (ctx->urma_ctx == NULL) {
        qemu_log("URMA: Failed to create instance with eid: %u, errno: %d.\n", eid_index,
                 errno);
        return -EINVAL;
    }

    if (migrate_onecopy_ram()) {
        bondp_set_bonding_mode_in_t in_arg = {
            .bonding_mode = aggr_mode,
            .bonding_level = BONDP_BONDING_LEVEL_IODIE,
        };
        urma_user_ctl_in_t user_ctl_in = {
            .addr = (uint64_t)&in_arg,
            .len = sizeof(in_arg),
            .opcode = BONDP_USER_CTL_SET_BONDING_MODE,
        };
        urma_user_ctl_out_t user_ctl_out = {0};

        qemu_log("Set bonding mode balance during onecopy\n");
        ret = urma_user_ctl_p(ctx->urma_ctx, &user_ctl_in, &user_ctl_out);
        if (ret) {
            qemu_log("URMA: Failed to set bonding mode, ret: %d, errno: %d\n", ret, errno);
        }
    }

    ret = qemu_init_jfs_post_list(ctx);
    if (ret) {
        qemu_log("URMA: Failed to init jfr post list, errno: %d\n", errno);
        return ret;
    }

    ctx->jfce = urma_create_jfce_p(ctx->urma_ctx);
    if (ctx->jfce == NULL) {
        qemu_log("URMA: Failed to create jfce, errno: %d.\n", errno);
        goto err;
    }

    urma_jfc_cfg_t jfc_cfg = {
        .depth = ctx->dev_attr.dev_cap.max_jfc_depth,
        .flag = {.value = 0},
        .jfce = ctx->jfce,
        .user_ctx = (uint64_t)NULL,
    };
    ctx->jfc = urma_create_jfc_p(ctx->urma_ctx, &jfc_cfg);
    if (ctx->jfc == NULL) {
        qemu_log("URMA: Failed to create jfc, errno: %d\n", errno);
        goto err;
    }

    urma_jfs_cfg_t jfs_cfg = {
        .depth = ctx->dev_attr.dev_cap.max_jfs_depth,
        .trans_mode = URMA_TM_RM,
        .priority = URMA_MAX_PRIORITY, /* Highest priority */
        .max_sge = 1,
        .max_inline_data = 0,
        .rnr_retry = URMA_TYPICAL_RNR_RETRY,
        .err_timeout = URMA_TYPICAL_ERR_TIMEOUT,
        .jfc = ctx->jfc,
        .flag.bs.multi_path = 1,
        .user_ctx = (uint64_t)NULL
    };
    ctx->jfs = urma_create_jfs_p(ctx->urma_ctx, &jfs_cfg);
    if (ctx->jfs == NULL) {
        qemu_log("URMA: Failed to create jfs, errno: %d\n", errno);
        goto err;
    }

    ctx->max_jfs_depth = ctx->dev_attr.dev_cap.max_jfs_depth;

    ctx->cr = g_new0(urma_cr_t, ctx->max_jfs_depth);
    if (ctx->cr == NULL) {
        qemu_log("URMA: Failed to malloc cr, errno: %d\n", errno);
        goto err;
    }

    if (qemu_get_random_u32(&ctx->jfr_token.token) < 0) {
        qemu_log("get jfr random token failed, errno: %d\n", errno);
        goto err;
    }

    urma_jfr_cfg_t jfr_cfg = {
        .depth = ctx->dev_attr.dev_cap.max_jfr_depth,
        .max_sge = 1,
        .flag.bs.tag_matching = URMA_NO_TAG_MATCHING,
        .trans_mode = URMA_TM_RM,
        .min_rnr_timer = URMA_TYPICAL_MIN_RNR_TIMER,
        .jfc = ctx->jfc,
        .token_value = ctx->jfr_token,
        .id = 0
    };
    ctx->jfr = urma_create_jfr_p(ctx->urma_ctx, &jfr_cfg);
    if (ctx->jfr == NULL) {
        qemu_log("Failed to create jfr, errno: %d\n", errno);
        goto err;
    }

    qemu_log("init urma context success.\n");
    return 0;

err:
    qemu_urma_cleanup_context(ctx);
    return -EINVAL;
}

int qemu_urma_prepare_incoming(QEMUFile *f)
{
    MigrationState *s = migrate_get_current();
    int ret;

    ret = qemu_urma_init_context(s->urma_ctx);
    if (ret) {
        return ret;
    }

    ret = qemu_urma_reg_whole_ram_blocks(s->urma_ctx);
    if (ret) {
        return ret;
    }

    if (qemu_exchange_urma_info(qemu_file_get_return_path(f), s->urma_ctx, true)) {
        return -EINVAL;
    }

    autostart = true;
    return 0;
}

static int urma_init_lib(void)
{
    int ret;
    urma_init_attr_t init_attr = {
        .uasid = 0,
    };

    ret = urma_init_p(&init_attr);
    if (ret != URMA_SUCCESS) {
        qemu_log("URMA: urma_init failed, ret: %d, errno: %d\n", ret, errno);
        return ret;
    }

    return 0;
}

static void qemu_urma_unreg_ram_blocks(URMAContext *urma)
{
    int i;
    URMALocalBlocks *local = &urma->local_ram_blocks;

    for (i = 0; i < local->nb_blocks; i++) {
        URMALocalBlock *block = &local->block[i];

        if (block->local_tseg) {
            urma_unregister_seg_p(block->local_tseg);
            block->local_tseg = NULL;
        }
    }

    if (urma->ram_discard_disabled) {
        ram_block_discard_disable(false);
        urma->ram_discard_disabled = false;
    }

    qemu_log("unreg all ram blocks success.\n");
}

int qemu_urma_reg_whole_ram_blocks(URMAContext *urma)
{
    int i;
    int64_t start_time;
    URMALocalBlocks *local = &urma->local_ram_blocks;
    MigrationState *s = migrate_get_current();
    urma_reg_seg_flag_t flag = {
        .bs.token_policy = URMA_TOKEN_PLAIN_TEXT,
        .bs.cacheable = URMA_NON_CACHEABLE,
        .bs.reserved = 0
    };

    if (!urma->is_incoming) {
        flag.bs.access = URMA_ACCESS_LOCAL_ONLY;
    } else {
        flag.bs.access = URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC;
    }

    start_time = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);

    if (!urma->ram_discard_disabled) {
        ram_block_discard_disable(true);
        urma->ram_discard_disabled = true;
    }

    for (i = 0; i < local->nb_blocks; i++) {
        URMALocalBlock *block = &local->block[i];
        if (qemu_get_random_u32(&block->local_seg_token.token) < 0) {
            qemu_log("get segment random token failed, errno: %d\n", errno);
            goto err;
        }

        urma_seg_cfg_t seg_cfg = {
            .va = (uint64_t)block->local_host_addr,
            .len = block->length,
            .token_value = block->local_seg_token,
            .flag = flag,
            .user_ctx = (uintptr_t)NULL,
            .iova = 0
        };

        block->local_tseg = urma_register_seg_p(urma->urma_ctx, &seg_cfg);
        if (block->local_tseg == NULL) {
            qemu_log("URMA: Failed to register RAM block: %s, size: %ld\n", block->block_name, block->length);
            goto err;
        }
    }

    qemu_log("reigster all ram blocks success.\n");
    s->ram_reg_time = qemu_clock_get_ms(QEMU_CLOCK_REALTIME) - start_time;

    return 0;

err:
    qemu_urma_unreg_ram_blocks(urma);
    return -EINVAL;
}

static void qemu_urma_cleanup(URMAContext *urma)
{
    if (urma == NULL) {
        return;
    }

    qemu_urma_unreg_ram_blocks(urma);
    qemu_urma_free_blocks(urma);
    qemu_urma_cleanup_context(urma);

    urma_uninit_p();
    qemu_log("clean up urma info success.\n");
}

static int qemu_urma_init_all(URMAContext *urma, bool pin_all)
{
    int ret;

    urma->pin_all = pin_all;
    urma->nb_polling = 0;

    ret = urma_init_lib();
    if (ret) {
        goto err;
    }

    ret = qemu_urma_init_ram_blocks(urma);
    if (ret) {
        goto err;
    }

    qemu_log("prepare all urma info success.\n");
    return 0;
err:
    qemu_log("Get error during prepare urma info, ret: %d, errno: %d\n", ret, errno);
    qemu_urma_cleanup(urma);
    return ret;
}


static void pack_seg_jfr_info(seg_jfr_info_t *info, URMAContext *ctx, URMALocalBlock *block)
{
    (void)memset(info, 0, sizeof(seg_jfr_info_t));
    info->eid = ctx->urma_ctx->eid;
    info->uasid = ctx->urma_ctx->uasid;
    info->seg_va = block->local_tseg->seg.ubva.va;
    info->seg_len = block->local_tseg->seg.len;
    info->seg_flag = block->local_tseg->seg.attr.value;
    info->seg_token_id = block->local_tseg->seg.token_id;
    info->seg_token.token = block->local_seg_token.token;
    info->jfr_id = ctx->jfr->jfr_id;
    info->jfr_token.token = ctx->jfr_token.token;

}

static void unpack_seg_jfr_info(seg_jfr_info_t *info, URMAContext *ctx, URMALocalBlock *block)
{
    block->remote_seg.ubva.eid = info->eid;
    block->remote_seg.ubva.uasid = info->uasid;
    block->remote_seg.ubva.va = info->seg_va;
    block->remote_seg.len = info->seg_len;
    block->remote_seg.attr.value = info->seg_flag;
    block->remote_seg.token_id = info->seg_token_id;
    block->remote_seg_token.token = info->seg_token.token;
    ctx->remote_jfr_id = info->jfr_id;
    ctx->rjfr_token.token = info->jfr_token.token;

}

static urma_target_jetty_t *qemu_import_jfr(URMAContext *ctx)
{
    urma_rjfr_t remote_jfr = {
        .jfr_id = ctx->remote_jfr_id,
        .trans_mode = URMA_TM_RM,
        .tp_type = URMA_CTP,
    };
    urma_target_jetty_t *tjfr = urma_import_jfr_p(ctx->urma_ctx, &remote_jfr, &ctx->rjfr_token);
    if (tjfr == NULL) {
        qemu_log("Failed to do urma_import_jfr, errno: %d\n", errno);
        return NULL;
    }

    if (urma_advise_jfr_p(ctx->jfs, tjfr) != URMA_SUCCESS) {
        qemu_log("Failed to advise jfr, errno: %d\n", errno);
        (void)urma_unimport_jfr_p(tjfr);
        return NULL;
    }

    return tjfr;
}

static void qemu_urma_search_ram_block(URMAContext *urma,
                                       uintptr_t block_offset,
                                       uint64_t offset,
                                       uint64_t length,
                                       uint64_t *block_index,
                                       uint64_t *chunk_index)
{
    uint64_t current_addr = block_offset + offset;
    URMALocalBlock *block = g_hash_table_lookup(urma->blockmap,
                                                (void *) block_offset);
    assert(block);
    assert(current_addr >= block->offset);
    assert((current_addr + length) <= (block->offset + block->length));

    *block_index = block->index;
    *chunk_index = urma_ram_chunk_index(block->local_host_addr,
                                        block->local_host_addr + (current_addr - block->offset));
}

static inline int qemu_urma_buffer_mergable(URMAContext *urma,
                                            uint64_t offset, uint64_t len)
{
    URMALocalBlock *block;
    uint8_t *host_addr;
    uint8_t *chunk_end;

    if (urma->current_index < 0) {
        return 0;
    }

    if (urma->current_chunk < 0) {
        return 0;
    }

    block = &(urma->local_ram_blocks.block[urma->current_index]);
    host_addr = block->local_host_addr + (offset - block->offset);
    chunk_end = urma_ram_chunk_end(block, urma->current_chunk);

    if (urma->current_length == 0) {
        return 0;
    }

    /*
     * Only merge into chunk sequentially.
     */
    if (offset != (urma->current_addr + urma->current_length)) {
        return 0;
    }

    if (offset < block->offset) {
        return 0;
    }

    if ((offset + len) > (block->offset + block->length)) {
        return 0;
    }

    if ((host_addr + len) > chunk_end) {
        return 0;
    }

    return 1;
}

static int poll_jfc_wait(URMAContext *ctx, urma_cr_t *cr)
{
    int i, j = 0, cnt = 0;

    for (i = 0; i < URMA_MAX_POLL_TIME; i++) {
        if (ctx->nb_polling == 0) {
            return 0;
        }

        cnt = urma_poll_jfc_p(ctx->jfc, ctx->nb_polling, cr);
        if (cnt < 0) {
            goto err;
        } else if (cnt > 0) {
            for (j = 0; j < cnt; j++) {
                if (cr[j].status != URMA_CR_SUCCESS) {
                    goto err;
                }
            }
            ctx->nb_polling -= cnt;
        }

        usleep(1);
    }

err:
    qemu_log("urma_poll_jfc err: loop num: %d, cnt: %d, status: %d, nb_polling: %d, errno: %d\n",
             i, cnt, cr[j].status, ctx->nb_polling, errno);
    return -EINVAL;
}

int qemu_flush_urma_write(URMAContext *urma)
{
    if (!urma || !urma->cr) {
        qemu_log("enter qemu_flush_urma_write when the urma is uninitialized!\n");
        return -EINVAL;
    }

    if (poll_jfc_wait(urma, urma->cr) != 0) {
        qemu_log("Failed to poll jfc, errno: %d\n", errno);
        return -EINVAL;
    }

    return 0;
}

int qemu_urma_write_all(URMAContext *urma)
{
    int i;
    URMALocalBlocks *local = &urma->local_ram_blocks;
    uint64_t local_addr, remote_addr, offset, length;
    uint64_t chunk_length = 1UL << URMA_REG_CHUNK_SHIFT;
    urma_jfs_wr_flag_t flag = {
        .bs.complete_enable = 1
    };

    for (i = 0; i < local->nb_blocks; i++) {
        URMALocalBlock *block = &local->block[i];

        if (!block->is_ram_block) {
            continue;
        }

        for (offset = 0; offset < block->length; offset += chunk_length) {
            local_addr = (uint64_t)block->local_host_addr + offset;
            remote_addr = (uint64_t)block->remote_seg.ubva.va + offset;
            length = (block->length - offset) > chunk_length ? chunk_length : (block->length - offset);

            if (urma_write_p(urma->jfs, urma->tjfr, block->import_tseg, block->local_tseg,
                       remote_addr, local_addr, length,
                       flag, urma->rid) != URMA_SUCCESS) {
                qemu_log("Failed to do urma_write, block: %s, size: %zu, errno: %d\n",
                         block->block_name, (size_t)length, errno);
                return -EINVAL;
            }

            urma->nb_polling++;
            if (urma->nb_polling >= urma->max_jfs_depth) {
                if (qemu_flush_urma_write(urma) < 0) {
                    qemu_log("Failed to flush urma write, errno: %d\n", errno);
                    return -EINVAL;
                }
            }
        }
    }

    return 0;
}

static int qemu_urma_write_one(URMAContext *urma,
                               int current_index, uint64_t current_addr,
                               uint64_t length, bool force)
{
    uintptr_t local_addr, remote_addr, offset;
    URMALocalBlock *block = &(urma->local_ram_blocks.block[current_index]);
    urma_jfs_wr_t *wr, *bad_wr = NULL;
    urma_status_t ret;

    if (block->is_ram_block) {
        offset = current_addr - block->offset;
        local_addr = (uintptr_t)(block->local_host_addr + offset);
        remote_addr = (uintptr_t)(block->remote_seg.ubva.va + offset);

        if (urma->nr_wr_polling < 0 || urma->nr_wr_polling >= URMA_JFS_WR_LIST_LEN) {
            qemu_log("Invalid nr wr polling number: %d.\n", urma->nr_wr_polling);
            return -EINVAL;
        }

        urma->src_sge[urma->nr_wr_polling].addr = local_addr;
        urma->src_sge[urma->nr_wr_polling].len = length;
        urma->src_sge[urma->nr_wr_polling].tseg = block->local_tseg;

        urma->dst_sge[urma->nr_wr_polling].addr = remote_addr;
        urma->dst_sge[urma->nr_wr_polling].len = length;
        urma->dst_sge[urma->nr_wr_polling].tseg = block->import_tseg;

        wr = &urma->jfs_wr_list[urma->nr_wr_polling];
        wr->user_ctx = urma->rid;
        wr->tjetty = urma->tjfr;
        wr->next = NULL;

        if (urma->nr_wr_polling > 0) {
            urma->jfs_wr_list[urma->nr_wr_polling - 1].next = wr;
        }
        urma->nr_wr_polling++;

        if (force || urma->nr_wr_polling >= URMA_JFS_WR_LIST_LEN) {
            ret = urma_post_jfs_wr_p(urma->jfs, urma->jfs_wr_list, &bad_wr);
            if (ret != URMA_SUCCESS) {
                qemu_log("Failed to do urma_post_jfs_wr, block: %s, size: %zu, ret: %d, errno: %d\n",
                        block->block_name, (size_t)length, ret, errno);
                return -EINVAL;
            }

            urma->nb_polling += urma->nr_wr_polling;
            urma->nr_wr_polling = 0;
        }

        if (force || urma->nb_polling >= urma->max_jfs_depth) {
            if (qemu_flush_urma_write(urma) < 0) {
                qemu_log("Failed to flush urma write, errno: %d\n", errno);
                return -EINVAL;
            }
        }
    }

    stat64_add(&mig_stats.normal_pages, length / qemu_target_page_size());
    stat64_add(&mig_stats.urma_bytes, length);
    ram_transferred_add(length);

    return 0;
}

int qemu_urma_write_flush(URMAContext *urma, bool force)
{
    int ret;

    if (!urma->current_length) {
        return 0;
    }

    ret = qemu_urma_write_one(urma, urma->current_index, urma->current_addr,
                              urma->current_length, force);
    if (ret < 0) {
        return ret;
    }

    urma->nb_sent++;
    urma->current_length = 0;
    urma->current_addr = 0;

    return 0;
}

static int qemu_urma_write(URMAContext *urma,
                           uint64_t block_offset, uint64_t offset,
                           uint64_t len)
{
    uint64_t current_addr = block_offset + offset;
    uint64_t index = urma->current_index;
    uint64_t chunk = urma->current_chunk;
    int ret;

    /* If we cannot merge it, we flush the current buffer first. */
    if (!qemu_urma_buffer_mergable(urma, current_addr, len)) {
        ret = qemu_urma_write_flush(urma, false);
        if (ret) {
            return ret;
        }
        urma->current_length = 0;
        urma->current_addr = current_addr;
        qemu_urma_search_ram_block(urma, block_offset,
                                   offset, len, &index, &chunk);
        urma->current_index = index;
        urma->current_chunk = chunk;
    }

    /* merge it */
    urma->current_length += len;

    /* flush it if buffer is too large */
    if (urma->current_length >= URMA_CHUNK_MERGE_MAX) {
        return qemu_urma_write_flush(urma, true);
    }

    return 0;
}

static int qemu_urma_save_page(QEMUFile *f, ram_addr_t block_offset,
                               ram_addr_t offset, size_t size)
{
    MigrationState *s = migrate_get_current();
    URMAContext *urma = s->urma_ctx;
    int ret;

    if (!urma) {
        return -EINVAL;
    }

    if (size > 0) {
        /*
         * Add this page to the current 'chunk'. If the chunk
         * is full, or the page doesn't belong to the current chunk,
         * an actual urma write will occur and a new chunk will be formed.
         */
        ret = qemu_urma_write(urma, block_offset, offset, size);
        if (ret < 0) {
            qemu_log("urma write failed, size: %zu, ret: %d, errno: %d\n",
                (size_t)size, ret, errno);
            return ret;
        }
    }

    return RAM_SAVE_CONTROL_DELAYED;
}

static void qemu_urma_unimport(URMAContext *urma)
{
    int i;
    URMALocalBlocks *local_block = &urma->local_ram_blocks;

    for (i = 0; i < local_block->nb_blocks; i++) {
        URMALocalBlock *block = &local_block->block[i];
        if (block->import_tseg) {
            urma_unimport_seg_p(block->import_tseg);
            block->import_tseg = NULL;
        }
    }

    if (urma->tjfr) {
        urma_unimport_jfr_p(urma->tjfr);
        urma->tjfr = NULL;
    }

    qemu_log("unimport all blocks and jfr success.\n");
}

int qemu_urma_import(URMAContext *urma)
{
    int i;
    URMALocalBlocks *local_block = &urma->local_ram_blocks;
    urma_import_seg_flag_t flag = {
        .bs.cacheable = URMA_NON_CACHEABLE,
        .bs.access = URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC,
        .bs.mapping = URMA_SEG_NOMAP,
        .bs.reserved = 0
    };

    for (i = 0; i < local_block->nb_blocks; i++) {
        URMALocalBlock *block = &local_block->block[i];

        block->import_tseg = urma_import_seg_p(urma->urma_ctx, &block->remote_seg, &block->remote_seg_token, 0, flag);
        if (block->import_tseg == NULL) {
            qemu_log("Failed to import segment, block name: %s, size: %ld, errno: %d\n",
                block->block_name, block->length, errno);
            goto err;
        }
    }

    urma->tjfr = qemu_import_jfr(urma);
    if (urma->tjfr == NULL) {
        qemu_log("Failed to import jfr, errno: %d\n", errno);
        goto err;
    }

    qemu_log("import all blocks and jfr success.\n");
    return 0;

err:
    qemu_urma_unimport(urma);
    return -EINVAL;
}

int qemu_exchange_urma_info(QEMUFile *f, URMAContext *urma, bool server)
{
    int i;
    URMALocalBlocks *local_block = &urma->local_ram_blocks;
    seg_jfr_info_t local = {0}, remote = {0};
    MigrationState *s = migrate_get_current();
    int64_t start_time;

    start_time = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);

    qemu_log("start to exchange urma segment info.\n");

    for (i = 0; i < local_block->nb_blocks; i++) {
        URMALocalBlock *block = &local_block->block[i];

        if (server) {
            pack_seg_jfr_info(&local, urma, block);
            qemu_put_buffer(f, (uint8_t *)&local, sizeof(seg_jfr_info_t));
            if (qemu_fflush(f) < 0) {
                qemu_log("Failed to flush qemu file, errno: %d\n", errno);
                return -EINVAL;
            }
        } else {
            if (qemu_get_buffer(f, (uint8_t *)&remote, sizeof(seg_jfr_info_t)) != sizeof(seg_jfr_info_t)) {
                qemu_log("get urma info failed, block name: %s, errno: %d\n", block->block_name, errno);
                return -EINVAL;
            }
            unpack_seg_jfr_info(&remote, urma, block);
        }
    }

    s->urma_exchange_time = qemu_clock_get_ms(QEMU_CLOCK_REALTIME) - start_time;
    return 0;
}

void urma_start_outgoing_migration(void *opaque,
                                   SocketAddress *saddr,
                                   Error **errp)
{
    MigrationState *s = opaque;
    URMAContext *urma = NULL;
    int ret;
    int64_t start_time;

    start_time = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);

    ret = urma_dlfunc_init(errp);
    if (ret < 0) {
        goto err;
    }

    urma = qemu_urma_data_init(&saddr->u.inet);
    if (urma == NULL) {
        qemu_log("migration: qemu_urma_data_init failed\n");
        goto err;
    }

    ret = qemu_urma_init_all(urma, true);
    if (ret) {
        qemu_log("migration: qemu_urma_init_all failed, ret: %d\n", ret);
        goto err;
    }

    s->urma_init_time = qemu_clock_get_ms(QEMU_CLOCK_REALTIME) - start_time;
    s->urma_migration = true;
    s->urma_ctx = urma;
    socket_start_outgoing_migration(s, saddr, errp);

    qemu_log("migration: start urma_start_outgoing_migration\n");
    return;

err:
    error_setg(errp, "migration: urma start outgoing migration failed");
    qemu_urma_data_free(urma);
}

void urma_start_incoming_migration(SocketAddress *saddr,
                                   Error **errp)
{
    MigrationState *s = migrate_get_current();
    URMAContext *urma;
    int ret;

    urma = qemu_urma_data_init(&saddr->u.inet);
    if (urma == NULL) {
        qemu_log("migration: qemu_urma_data_init failed\n");
        goto err;
    }

    ret = urma_dlfunc_init(errp);
    if (ret < 0) {
        goto err;
    }

    urma->is_incoming = true;

    ret = qemu_urma_init_all(urma, true);
    if (ret) {
        qemu_log("migration: qemu_urma_init_all failed, ret: %d\n", ret);
        goto err;
    }

    s->urma_migration = true;
    s->urma_ctx = urma;
    socket_start_incoming_migration(saddr, errp);

    qemu_log("migration: start urma_start_incoming_migration\n");
    return;

err:
    error_setg(errp, "migration: urma start incoming migration failed");
    qemu_urma_data_free(urma);
}

void urma_migration_cleanup(void)
{
    MigrationState *s = migrate_get_current();

    if (s->urma_ctx == NULL) {
        return;
    }

    qemu_urma_unimport(s->urma_ctx);
    qemu_urma_cleanup(s->urma_ctx);
    qemu_urma_data_free(s->urma_ctx);
    s->urma_ctx = NULL;

    qemu_log("urma migration cleanup success.\n");
}

int urma_control_save_page(QEMUFile *f, ram_addr_t block_offset,
                           ram_addr_t offset, size_t size)
{
    int ret;

    ret = qemu_urma_save_page(f, block_offset, offset, size);

    if (ret != RAM_SAVE_CONTROL_DELAYED &&
        ret != RAM_SAVE_CONTROL_NOT_SUPP) {
        if (ret < 0) {
            qemu_file_set_error(f, ret);
        }
    }
    return ret;
}

void record_migration_log(MigrationState *s)
{
    qemu_log("qmp urma resource initialization and connection cost time: %ld(ms)\n", s->urma_init_time);
    qemu_log("qmp urma exchange info cost time: %ld(ms)\n", s->urma_exchange_time);
    qemu_log("qmp ram registration cost time: %ld(ms)\n", s->ram_reg_time);

    qemu_log("qmp notify cost time: %ld(ms)\n", s->notify_time);
    qemu_log("qmp bdrv cost time: %ld(ms)\n", s->bdrv_time);
    qemu_log("qmp precopy cost time: %ld(ms)\n", s->precopy_time);
    qemu_log("  > qmp cpu sync cost time: %ld(ms)\n", s->cpu_sync_time);
    qemu_log("  > qmp device migration cost time: %ld(ms)\n", s->dev_mig_time);
    qemu_log("  > qmp last memcpy cost time: %ld(ms)\n", s->last_memcpy_time);
    qemu_log("qmp downtime %ld(ms)\n", s->downtime);
    qemu_log("qmp setup time %ld(ms)\n", s->setup_time);
    qemu_log("qmp total time %ld(ms)\n", s->total_time);
}
