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
#include <dlfcn.h>
#include "crypto/random.h"

int qemu_flush_urma_write(URMAContext *urma)
{
    /* TODO */
    return -EINVAL;
}

int qemu_urma_import(URMAContext *urma)
{
    /* TODO */
    return -EINVAL;
}

int qemu_exchange_urma_info(QEMUFile *f, URMAContext *urma, bool server)
{
    /* TODO */
    return -EINVAL;
}

void urma_start_outgoing_migration(void *opaque,
                                   SocketAddress *saddr,
                                   Error **errp)
{
    /* TODO */
    return;
}

void urma_start_incoming_migration(SocketAddress *saddr,
                                   Error **errp)
{
    /* TODO */
    return;
}

void urma_migration_cleanup(void)
{
    /* TODO */
    return;
}

int urma_control_save_page(QEMUFile *f, ram_addr_t block_offset,
                           ram_addr_t offset, size_t size)
{
    /* TODO */
    return RAM_SAVE_CONTROL_NOT_SUPP;
}

void record_migration_log(MigrationState *s)
{
    qemu_log("qmp urma resource initialization and connection cost time: %ld(ms)\n", s->urma_init_time);
    qemu_log("qmp urma exchange info cost time: %ld(ms)\n", s->urma_exchange_time);
    qemu_log("qmp ram registration cost time: %ld(ms)\n", s->ram_reg_time);
    qemu_log("qmp device migration cost time: %ld(ms)\n", s->dev_mig_time);
    qemu_log("qmp last memcpy cost time: %ld(ms)\n", s->last_memcpy_time);
    qemu_log("qmp downtime %ld(ms)\n", s->downtime);
    qemu_log("qmp setup time %ld(ms)\n", s->setup_time);
    qemu_log("qmp total time %ld(ms)\n", s->total_time);
}
