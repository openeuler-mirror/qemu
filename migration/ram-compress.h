/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 * Copyright (c) 2011-2015 Red Hat Inc
 *
 * Authors:
 *  Juan Quintela <quintela@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef QEMU_MIGRATION_COMPRESS_H
#define QEMU_MIGRATION_COMPRESS_H

#ifdef CONFIG_ZSTD
#include <zstd.h>
#include <zstd_errors.h>
#endif
#include "qemu-file.h"
#include "qapi/qapi-types-migration.h"

enum CompressResult {
    RES_NONE = 0,
    RES_ZEROPAGE = 1,
    RES_COMPRESS = 2
};
typedef enum CompressResult CompressResult;

struct DecompressParam {
    bool done;
    bool quit;
    QemuMutex mutex;
    QemuCond cond;
    void *des;
    uint8_t *compbuf;
    int len;

    /* for zlib compression */
    z_stream stream;
#ifdef CONFIG_ZSTD
    ZSTD_DStream *zstd_ds;
    ZSTD_inBuffer in;
    ZSTD_outBuffer out;
#endif
};
typedef struct DecompressParam DecompressParam;

struct CompressParam {
    bool done;
    bool quit;
    bool trigger;
    CompressResult result;
    QEMUFile *file;
    QemuMutex mutex;
    QemuCond cond;
    RAMBlock *block;
    ram_addr_t offset;

    /* internally used fields */
    uint8_t *originbuf;

    /* for zlib compression */
    z_stream stream;

#ifdef CONFIG_ZSTD
    ZSTD_CStream *zstd_cs;
    ZSTD_inBuffer in;
    ZSTD_outBuffer out;
#endif
};
typedef struct CompressParam CompressParam;

typedef struct {
    int (*save_setup)(CompressParam *param);
    void (*save_cleanup)(CompressParam *param);
    ssize_t (*compress_data)(CompressParam *param, size_t size);
} MigrationCompressOps;

typedef struct {
    int (*load_setup)(DecompressParam *param);
    void (*load_cleanup)(DecompressParam *param);
    int (*decompress_data)(DecompressParam *param, uint8_t *dest, size_t size);
    int (*check_len)(int len);
} MigrationDecompressOps;

void compress_threads_save_cleanup(void);
int compress_threads_save_setup(void);

bool compress_page_with_multi_thread(RAMBlock *block, ram_addr_t offset,
                                      int (send_queued_data(CompressParam *)));

int wait_for_decompress_done(void);
void compress_threads_load_cleanup(void);
int compress_threads_load_setup(QEMUFile *f);
void decompress_data_with_multi_threads(QEMUFile *f, void *host, int len);

void populate_compress(MigrationInfo *info);
uint64_t compress_ram_pages(void);
void update_compress_thread_counts(const CompressParam *param, int bytes_xmit);
void compress_update_rates(uint64_t page_count);
int compress_send_queued_data(CompressParam *param);
void compress_flush_data(void);

#endif
