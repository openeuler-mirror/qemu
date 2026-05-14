/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
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

#ifndef QEMU_MIGRATION_URMA_H
#define QEMU_MIGRATION_URMA_H

#include "qemu/sockets.h"
#include "exec/memory.h"

#include <ub/umdk/urma/urma_api.h>
#include <ub/umdk/urma/urma_ubagg.h>

typedef struct QEMUFile QEMUFile;

#define URMA_SO_PATH "liburma.so.0"
#define URMA_TOKEN_LEN 32
#define URMA_JFS_WR_LIST_LEN 256

typedef struct QEMU_PACKED URMADestBlock {
    uint64_t remote_host_addr;
    uint64_t offset;
    uint64_t length;
    uint32_t remote_rkey;
    uint32_t padding;
} URMADestBlock;

typedef struct URMALocalBlock {
    char                *block_name;
    uint8_t             *local_host_addr; /* local virtual address */
    uint64_t             remote_host_addr; /* remote virtual address */
    uint64_t             offset;
    uint64_t             length;
    int                  index;           /* which block are we */
    unsigned int         src_index;       /* (Only used on dest) */
    bool                 is_ram_block;
    int                  nb_chunks;
    unsigned long       *transit_bitmap;
    unsigned long       *unregister_bitmap;

    urma_target_seg_t   *local_tseg;      /* tseg for non-chunk-level registration */
    urma_token_t         local_seg_token;
    urma_seg_t           remote_seg;      /* remote seg for non-chunk-level registration */
    urma_token_t         remote_seg_token;
    urma_target_seg_t   *import_tseg;     /* Imported target segment for read/write/atomic */
} URMALocalBlock;

typedef struct URMALocalBlocks {
    int             nb_blocks;
    bool            init;             /* main memory init complete */
    URMALocalBlock *block;
} URMALocalBlocks;


typedef struct URMAContext {
    char *host;
    int port;
    int id;

    int is_incoming;

    /* number of outstanding writes */
    int nb_sent;

    /* number of polling writes */
    int nb_polling;

    /* store info about current buffer so that we can
       merge it with future sends */
    uint64_t current_addr;
    uint64_t current_length;
    /* index of ram block the current buffer belongs to */
    int current_index;
    /* index of the chunk in the current ram block */
    int current_chunk;

    bool pin_all;

    bool ram_discard_disabled;

    GHashTable *blockmap;

    /*
     * Description of ram blocks used throughout the code.
     */
    URMALocalBlocks local_ram_blocks;


    URMADestBlock  *dest_blocks;

    /* urma info */
    urma_context_t *urma_ctx;
    urma_device_attr_t dev_attr;

    urma_jfce_t *jfce;
    urma_jfc_t *jfc;
    urma_jfs_t *jfs;
    urma_jfr_t *jfr;
    urma_cr_t *cr;
    uint64_t rid;
    urma_token_t jfr_token;
    bool event_mode;
    int max_jfs_depth;

    urma_jfs_wr_t jfs_wr_list[URMA_JFS_WR_LIST_LEN];
    urma_sge_t src_sge[URMA_JFS_WR_LIST_LEN];
    urma_sge_t dst_sge[URMA_JFS_WR_LIST_LEN];
    int nr_wr_polling;

    int client_sockfd;
    int listen_fd;

    urma_jfr_id_t remote_jfr_id;
    urma_token_t rjfr_token;
    urma_target_jetty_t *tjfr;
} URMAContext;

typedef struct seg_jfr_info_t {
    /* Common */
    urma_eid_t eid;
    uint32_t uasid;
    /* segment */
    uint64_t seg_va;
    uint64_t seg_len;
    uint32_t seg_flag;
    uint32_t seg_token_id;
    urma_token_t seg_token;
    /* jfr */
    urma_jfr_id_t jfr_id;
    urma_token_t jfr_token;
} __attribute__((packed)) seg_jfr_info_t;


void urma_start_outgoing_migration(void *opaque, SocketAddress *saddr,
                                   Error **errp);
void urma_start_incoming_migration(SocketAddress *saddr, Error **errp);
int urma_control_save_page(QEMUFile *f, ram_addr_t block_offset,
                           ram_addr_t offset, size_t size);
int qemu_flush_urma_write(URMAContext *urma);
int qemu_exchange_urma_info(QEMUFile *f, URMAContext *urma, bool server);
int qemu_urma_import(URMAContext *urma);
void urma_migration_cleanup(void);
void record_migration_log(MigrationState *s);
int qemu_urma_write_all(URMAContext *urma);
int qemu_urma_reg_whole_ram_blocks(URMAContext *urma);
int qemu_urma_write_flush(URMAContext *urma, bool force);
int qemu_urma_init_context(URMAContext *ctx);
int qemu_urma_prepare_incoming(QEMUFile *f);

#endif
