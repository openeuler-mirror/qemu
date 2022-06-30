/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: support vm live migration with vfio-pci device
 */

#include <sys/ioctl.h>
#include <linux/vfio.h>
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/cutils.h"

#include "sysemu/runstate.h"
#include "hw/vfio/vfio-common.h"
#include "cpu.h"
#include "migration/vmstate.h"
#include "migration/qemu-file.h"
#include "migration/register.h"
#include "migration/blocker.h"
#include "migration/misc.h"
#include "qapi/error.h"
#include "exec/ramlist.h"
#include "exec/ram_addr.h"
#include "pci.h"
#include "trace.h"
#include "hw/hw.h"
#include "migration/migration.h"

#define VFIO_MIG_FLAG_END_OF_STATE      (0xffffffffef100001ULL)
#define VFIO_MIG_FLAG_DEV_CONFIG_STATE  (0xffffffffef100002ULL)
#define VFIO_MIG_FLAG_DEV_SETUP_STATE   (0xffffffffef100003ULL)
#define VFIO_MIG_FLAG_DEV_DATA_STATE    (0xffffffffef100004ULL)
#define VFIO_MIG_FLAG_MEM_CHECK         (0xffffffffef100005ULL)

#define LONG_LONG_LEN       8
#define INT_LEN             4
#define SHORT_LEN           2

#define BSD_CHECKSUM_RIGHT_SHIFT    1
#define BSD_CHECKSUM_LEFT_SHIFT     15

static int64_t g_bytes_transferred;

void migration_memory_check(void)
{
    MemoryRegion *root_mr = address_space_memory.root;
    MemoryRegion *sub_mr = NULL;
    hwaddr addr, start;
    uint64_t size;
    uint16_t sum = 0;
    uint8_t val;
    bool found = false;

    QTAILQ_FOREACH(sub_mr, &root_mr->subregions, subregions_link) {
        if (!strcmp(sub_mr->name, "mach-virt.ram")) {
            found = true;
            break;
        }
    }

    if (!found) {
        error_report("can't find mach-virt.ram in address_space_memory");
        return;
    }

    start = sub_mr->addr;
    size = int128_get64(sub_mr->size);

    for (addr = start; addr < (start + size); addr++) {
        val = address_space_ldub(&address_space_memory, addr,
                                 MEMTXATTRS_UNSPECIFIED, NULL);
        sum = (sum >> BSD_CHECKSUM_RIGHT_SHIFT) |
              (sum << BSD_CHECKSUM_LEFT_SHIFT);
        sum += val;
    }

    info_report("vm memory check: start=%lu, size=%lu, verify code=%05d",
                start, size, sum);
}

static inline int vfio_mig_access(const VFIODevice *vbasedev,
                                  void *val, int count,
                                  off_t off, bool iswrite)
{
    int ret;

    ret = iswrite ? pwrite(vbasedev->fd, val, count, off) :
                    pread(vbasedev->fd, val, count, off);
    if (ret < count) {
        error_report("vfio device %s vfio_mig_%s %d byte: failed at offset 0x%"
                     HWADDR_PRIx", errno: %d", vbasedev->name,
                     iswrite ? "write" : "read",
                     count, off, errno);
        return (ret < 0) ? ret : -EINVAL;
    }

    return 0;
}

static int vfio_mig_rw(const VFIODevice *vbasedev,
                       __u8 *buf, size_t count,
                       off_t off, bool iswrite)
{
    int ret;
    int done = 0;
    __u8 *tbuf = buf;

    while (count) {
        int bytes;

        if (count >= LONG_LONG_LEN && !(off % LONG_LONG_LEN)) {
            bytes = LONG_LONG_LEN;
        } else if (count >= INT_LEN && !(off % INT_LEN)) {
            bytes = INT_LEN;
        } else if (count >= SHORT_LEN && !(off % SHORT_LEN)) {
            bytes = SHORT_LEN;
        } else {
            bytes = 1;
        }

        ret = vfio_mig_access(vbasedev, tbuf, bytes, off, iswrite);
        if (ret != 0) {
            return ret;
        }

        count -= bytes;
        done += bytes;
        off += bytes;
        tbuf += bytes;
    }

    return done;
}

#define VFIO_MIG_STRUCT_OFFSET(f) offsetof(struct vfio_device_migration_info, f)

static int vfio_mig_read(const VFIODevice *vbasedev,
                         void *buf, size_t count, off_t off)
{
    return vfio_mig_rw(vbasedev, (__u8 *)buf, count, off, false);
}

static int vfio_mig_write(const VFIODevice *vbasedev,
                          void *buf, size_t count, off_t off)
{
    return vfio_mig_rw(vbasedev, (__u8 *)buf, count, off, true);
}

static int vfio_migration_set_state(VFIODevice *vbasedev,
                                    uint32_t mask, uint32_t value)
{
    VFIOMigration *migration = vbasedev->migration;
    VFIORegion *region = &migration->region;
    off_t dev_state_off = region->fd_offset +
                          VFIO_MIG_STRUCT_OFFSET(device_state);
    uint32_t device_state;
    int ret;

    ret = vfio_mig_read(vbasedev, &device_state,
                        sizeof(device_state), dev_state_off);
    if (ret < 0) {
        return ret;
    }

    device_state = (device_state & mask) | value;
    
    if (!VFIO_DEVICE_STATE_VALID(device_state)) {
        return -EINVAL;
    }

    ret = vfio_mig_write(vbasedev, &device_state,
                         sizeof(device_state), dev_state_off);
    if (ret < 0) {
        int rret;
        rret = vfio_mig_read(vbasedev, &device_state,
                             sizeof(device_state), dev_state_off);
        if (rret < 0)
            return rret;

        if (VFIO_DEVICE_STATE_IS_ERROR(device_state)) {
            error_report("vfio device %s is in error state 0x%x\n",
                         vbasedev->name, device_state);
            return -EIO;
        }
        return ret;
    }

    migration->device_state = device_state;

    return 0;
}

static int vfio_save_setup(QEMUFile *f, void *opaque)
{
    VFIODevice *vbasedev = opaque;
    VFIOMigration *migration = vbasedev->migration;
    MigrationState *ms = migrate_get_current();
    int ret;

    qemu_put_be64(f, VFIO_MIG_FLAG_DEV_SETUP_STATE);

    if (migration->region.mmaps) {
        /*
         * Calling vfio_region_mmap() from migration thread. Memory API called
         * from this function require locking the iothread when called from
         * outside the main loop thread.
         */
        qemu_mutex_lock_iothread();
        ret = vfio_region_mmap(&migration->region);
        qemu_mutex_unlock_iothread();
        if (ret != 0) {
            error_report("%s: Failed to mmap VFIO migration region, ret=%d",
                         vbasedev->name, ret);
            error_report("%s: Falling back to slow path", vbasedev->name);
        }
    }

    ret = vfio_migration_set_state(vbasedev,
                                   VFIO_DEVICE_STATE_MASK,
                                   VFIO_DEVICE_STATE_SAVING);
    if (ret != 0) {
        error_report("%s: Failed to set state SAVING", vbasedev->name);
        return ret;
    }

    /* migrate the version info of migration driver to dst. */
    qemu_put_be32(f, migration->version_id);

    qemu_put_be64(f, ms->parameters.memory_check == 1 ?
                     VFIO_MIG_FLAG_MEM_CHECK : VFIO_MIG_FLAG_END_OF_STATE);

    ret = qemu_file_get_error(f);
    if (ret != 0) {
        return ret;
    }

    return ret;
}

static int vfio_save_device_config_state(QEMUFile *f, void *opaque)
{
    VFIODevice *vbasedev = opaque;

    qemu_put_be64(f, VFIO_MIG_FLAG_DEV_CONFIG_STATE);
    if (vbasedev->ops && vbasedev->ops->vfio_save_config) {
        vbasedev->ops->vfio_save_config(vbasedev, f);
    }

    qemu_put_be64(f, VFIO_MIG_FLAG_END_OF_STATE);

    return qemu_file_get_error(f);
}

static int vfio_update_pending(VFIODevice *vbasedev)
{
    VFIOMigration *migration = vbasedev->migration;
    VFIORegion *region = &migration->region;
    uint64_t pending_bytes = 0;
    int ret;

    ret = vfio_mig_read(vbasedev, &pending_bytes, sizeof(pending_bytes),
                        region->fd_offset + VFIO_MIG_STRUCT_OFFSET(pending_bytes));
    if (ret < 0) {
        migration->pending_bytes = 0;
        return ret;
    }

    migration->pending_bytes = pending_bytes;

    return 0;
}

static void *get_data_section_size(VFIORegion *region, uint64_t data_offset,
                                   uint64_t data_size, uint64_t *size)
{
    void *ptr = NULL;
    uint64_t limit = 0;
    int i;

    if (!region->mmaps) {
        if (size) {
            *size = MIN(data_size, region->size - data_offset);
        }
        return ptr;
    }

    for (i = 0; i < region->nr_mmaps; i++) {
        VFIOMmap *map = region->mmaps + i;

        if ((data_size >= map->offset) && 
            (data_offset < map->offset + map->size)) {
            /* check if data_offset is within sparse mmap areas */
            ptr = map->mmap + data_offset - map->offset;
            if (size) {
                *size = MIN(data_size, map->offset + map->size - data_offset);
            }
            break;
        } else if ((data_offset < map->offset) &&
                   (!limit || limit > map->offset)) {
            /*
             * data_offset is not within sparse mmap areas, find size of
             * non-mapped area. Check through all list since region->mmaps list
             * is not sorted.
             */
            limit = map->offset;
        }
    }

    if (!ptr && size) {
        *size = limit ? MIN(data_size, limit - data_offset) : data_size;
    }

    return ptr;
}

static int vfio_save_buffer(QEMUFile *f, VFIODevice *vbasedev, uint64_t *size)
{
    VFIOMigration *migration = vbasedev->migration;
    VFIORegion *region = &migration->region;
    uint64_t data_offset = 0;
    uint64_t data_size = 0;
    uint64_t sz;
    int ret;

    ret = vfio_mig_read(vbasedev, &data_offset, sizeof(data_offset),
                        region->fd_offset + VFIO_MIG_STRUCT_OFFSET(data_offset));
    if (ret < 0) {
        return ret;
    }

    ret = vfio_mig_read(vbasedev, &data_size, sizeof(data_size),
                        region->fd_offset + VFIO_MIG_STRUCT_OFFSET(data_size));
    if (ret < 0) {
        return ret;
    }
    qemu_put_be64(f, data_size);
    sz = data_size;

    while (sz) {
        void *buf;
        uint64_t sec_size;
        bool buf_allocated = false;

        buf = get_data_section_size(region, data_offset, sz, &sec_size);
        if (!buf) {
            /* fall slow path */
            if (sec_size == 0 || (buf = g_try_malloc(sec_size)) == NULL) {
                error_report("Error allocating buffer");
                return -ENOMEM;
            }
            buf_allocated = true;

            ret = vfio_mig_read(vbasedev, buf, sec_size,
                                region->fd_offset + data_offset);
            if (ret < 0) {
                g_free(buf);
                return ret;
            }
        }

        qemu_put_buffer(f, buf, sec_size);
        if (buf_allocated) {
            g_free(buf);
        }
        sz -= sec_size;
        data_offset += sec_size;
    }

    ret = qemu_file_get_error(f);
    if (!ret && size) {
        *size = data_size;
    }

    g_bytes_transferred += data_size;

    return ret;
}

static int vfio_save_complete_precopy(QEMUFile *f, void *opaque)
{
    VFIODevice *vbasedev = opaque;
    VFIOMigration *migration = vbasedev->migration;
    uint64_t data_size;
    int ret;

    ret = vfio_migration_set_state(vbasedev,
                                   ~VFIO_DEVICE_STATE_RUNNING,
                                   VFIO_DEVICE_STATE_SAVING);
    if (ret != 0) {
        error_report("%s: Failed to set state STOP and SAVING", vbasedev->name);
        return ret;
    }

    ret = vfio_save_device_config_state(f, opaque);
    if (ret != 0) {
        return ret;
    }

    ret = vfio_update_pending(vbasedev);
    if (ret != 0) {
        return ret;
    }

    while (migration->pending_bytes > 0) {
        qemu_put_be64(f, VFIO_MIG_FLAG_DEV_DATA_STATE);
        ret = vfio_save_buffer(f, vbasedev, &data_size);
        if (ret < 0) {
            error_report("%s: Failed to save buffer", vbasedev->name);
            return ret;
        }

        if (data_size == 0) {
            break;
        }
        
        ret = vfio_update_pending(vbasedev);
        if (ret != 0) {
            return ret;
        }
    }

    qemu_put_be64(f, VFIO_MIG_FLAG_END_OF_STATE);

    ret = qemu_file_get_error(f);
    if (ret != 0) {
        return ret;
    }

    ret = vfio_migration_set_state(vbasedev, ~VFIO_DEVICE_STATE_SAVING, 0);
    if (ret != 0) {
        error_report("%s: Failed to set state STOPPED", vbasedev->name);
    }

    return ret;
}

static void vfio_save_cleanup(void *opaque)
{
    VFIODevice *vbasedev = opaque;
    VFIOMigration *migration = vbasedev->migration;

    if (migration->region.mmaps) {
        vfio_region_unmap(&migration->region);
    }
}

static int vfio_load_setup(QEMUFile *f, void *opaque)
{
    VFIODevice *vbasedev = opaque;
    VFIOMigration *migration = vbasedev->migration;
    int ret;

    if (migration->region.mmaps) {
        ret = vfio_region_mmap(&migration->region);
        if (ret != 0) {
            error_report("%s: Failed to mmap VFIO migration region %d, ret=%d",
                         vbasedev->name, migration->region.nr, ret);
            error_report("%s: Falling back to slow path", vbasedev->name);
        }
    }
    
    ret = vfio_migration_set_state(vbasedev,
                                   ~VFIO_DEVICE_STATE_MASK,
                                   VFIO_DEVICE_STATE_RESUMING);
    if (ret != 0) {
        error_report("%s: Failed to set state RESUMING", vbasedev->name);
        if (migration->region.mmaps) {
            vfio_region_unmap(&migration->region);
        }
    }

    return ret;
}

static int vfio_load_device_config_state(QEMUFile *f, void *opaque)
{
    VFIODevice *vbasedev = opaque;
    int ret;

    if (vbasedev->ops && vbasedev->ops->vfio_load_config) {
        ret = vbasedev->ops->vfio_load_config(vbasedev, f);
        if (ret != 0) {
            error_report("%s: Failed to load device config space",
                         vbasedev->name);
            return ret;
        }
    }

    return qemu_file_get_error(f);
}

static int vfio_load_state_data(QEMUFile *f, VFIODevice *vbasedev,
                                uint64_t data_offset, uint64_t size)
{
    VFIORegion *region = &vbasedev->migration->region;
    uint64_t sec_size;
    void *buf = NULL;
    bool buf_alloc = false;
    int ret;

    while (size) {
        buf_alloc = false;
        buf = get_data_section_size(region, data_offset, size, &sec_size);
        if (!buf) {
            if (sec_size == 0 || (buf = g_try_malloc(sec_size)) == NULL) {
                error_report("Error allocating buffer");
                return -ENOMEM;
            }
            buf_alloc = true;
        }

        qemu_get_buffer(f, buf, sec_size);

        if (buf_alloc) {
            ret = vfio_mig_write(vbasedev, buf, sec_size,
                                 region->fd_offset + data_offset);
            g_free(buf);
            if (ret < 0) {
                return ret;
            }
        }

        size -= sec_size;
        data_offset += sec_size;
    }

    return 0;
}

static int vfio_load_buffer(QEMUFile *f, VFIODevice *vbasedev, uint64_t data_size)
{
    VFIORegion *region = &vbasedev->migration->region;
    uint64_t data_offset = 0;
    uint64_t size, report_size;
    int ret;

    do {
        ret = vfio_mig_read(vbasedev, &data_offset, sizeof(data_offset),
                            region->fd_offset + VFIO_MIG_STRUCT_OFFSET(data_offset));
        if (ret < 0) {
            return ret;
        }
        if (data_offset + data_size > region->size) {
            report_size = size = region->size - data_offset;
            data_size -= size;
        } else {
            report_size = size = data_size;
            data_size = 0;
        }

        ret = vfio_load_state_data(f, vbasedev, data_offset, size);
        if (ret != 0) {
            return ret;
        }

        ret = vfio_mig_write(vbasedev, &report_size, sizeof(report_size),
                             region->fd_offset + VFIO_MIG_STRUCT_OFFSET(data_size));
        if (ret < 0) {
            return ret;
        }
    } while (data_size);

    return 0;
}

static int migration_driver_version_check(VFIODevice *vbasedev,
                                          uint32_t src_version)
{
    VFIOMigration *migration = vbasedev->migration;

    if (src_version > migration->version_id) {
        error_report("Can't support live migration from higher to lower "
                     "migration driver, src version = %u, dst version = %u.",
                     src_version, migration->version_id);
        return -EINVAL;
    }

    return 0;
}

static int vfio_load_setup_check(QEMUFile *f, VFIODevice *vbasedev)
{
    uint32_t src_version;
    uint64_t flag;
    MigrationState *ms = migrate_get_current();

    src_version = qemu_get_be32(f);
    flag = qemu_get_be64(f);
    if (flag != VFIO_MIG_FLAG_MEM_CHECK && flag != VFIO_MIG_FLAG_END_OF_STATE) {
        error_report("Unknown flag 0x%lx in load setup stage", flag);
        return -EINVAL;
    }

    if (flag == VFIO_MIG_FLAG_MEM_CHECK) {
        ms->parameters.memory_check = 1;
    }

    return migration_driver_version_check(vbasedev, src_version);
}

static int vfio_load_state(QEMUFile *f, void *opaque, int version_id)
{
    VFIODevice *vbasedev = opaque;
    int ret = 0;
    uint64_t data;

    data = qemu_get_be64(f);
    while (data != VFIO_MIG_FLAG_END_OF_STATE) {
        switch (data) {
            case VFIO_MIG_FLAG_DEV_SETUP_STATE: {
                return vfio_load_setup_check(f, vbasedev);
                break;
            }
            case VFIO_MIG_FLAG_DEV_CONFIG_STATE: {
                ret = vfio_load_device_config_state(f, opaque);
                if (ret != 0) {
                    return ret;
                }
                data = qemu_get_be64(f);
                if (data != VFIO_MIG_FLAG_END_OF_STATE) {
                    error_report("%s: Failed loading device config space, "
                                 "end flag incorrect 0x%"PRIx64,
                                 vbasedev->name, data);
                    return -EINVAL;
                }
                break;
            }
            case VFIO_MIG_FLAG_DEV_DATA_STATE: {
                uint64_t data_size = qemu_get_be64(f);
                if (data_size &&
                    (ret = vfio_load_buffer(f, vbasedev, data_size)) < 0) {
                    return ret;
                }
                break;
            }
            default:
                error_report("%s: Unknown tag 0x%"PRIx64, vbasedev->name, data);
                return -EINVAL;
        }

        data = qemu_get_be64(f);
        ret = qemu_file_get_error(f);
        if (ret != 0) {
            return ret;
        }
    }

    return ret;
}

static int vfio_load_cleanup(void *opaque)
{
    VFIODevice *vbasedev = opaque;
    VFIOMigration *migration = vbasedev->migration;

    if (migration->region.mmaps) {
        vfio_region_unmap(&migration->region);
    }
    return 0;
}

static SaveVMHandlers g_savevm_vfio_handlers = {
    .save_setup = vfio_save_setup,
    /* save_live_pending reserved */
    /* save_live_iterate reserved */
    .save_live_complete_precopy = vfio_save_complete_precopy,
    .save_cleanup = vfio_save_cleanup,
    .load_setup = vfio_load_setup,
    .load_state = vfio_load_state,
    .load_cleanup = vfio_load_cleanup,
};

static int vfio_migration_cancel(VFIODevice *vbasedev)
{
    VFIOMigration *migration = vbasedev->migration;
    VFIORegion *region = &migration->region;
    VFIOCmd cmd = VFIO_DEVICE_MIGRATION_CANCEL;
    int ret;

    ret = vfio_mig_write(vbasedev, &cmd, sizeof(cmd),
                         region->fd_offset + VFIO_MIG_STRUCT_OFFSET(device_cmd));
    return (ret < 0) ? ret : 0;
}

static void vfio_migration_state_notifier(Notifier *notifier, void *data)
{
    MigrationState *s = data;
    VFIOMigration *migration = container_of(notifier,
                                            VFIOMigration,
                                            migration_state);
    VFIODevice *vbasedev = migration->vbasedev;
    int ret;

    switch (s->state) {
        case MIGRATION_STATUS_CANCELLING:
        case MIGRATION_STATUS_CANCELLED:
        case MIGRATION_STATUS_FAILED:
            g_bytes_transferred = 0;
            ret = vfio_migration_cancel(vbasedev);
            if (ret != 0) {
                error_report("%s: Failed to notify the vifio migration cancel, "
                             "ret = %d.", vbasedev->name, ret);
                return;
            }

            ret = vfio_migration_set_state(vbasedev, 
                                           ~(VFIO_DEVICE_STATE_SAVING | VFIO_DEVICE_STATE_RESUMING),
                                           VFIO_DEVICE_STATE_RUNNING);
            if (ret != 0) {
                error_report("%s: Failed to set state RUNNING", vbasedev->name);
            }
            break;
        default:
            break;
    }
}

static void vfio_migration_exit(VFIODevice *vbasedev)
{
    VFIOMigration *migration = vbasedev->migration;

    vfio_region_exit(&migration->region);
    vfio_region_finalize(&migration->region);
    g_free(vbasedev->migration);
    vbasedev->migration = NULL;
}

static int vfio_get_migration_version(VFIODevice *vbasedev)
{
    VFIOMigration *migration = vbasedev->migration;
    VFIORegion *region = &migration->region;
    uint32_t version_id;
    int ret;

    ret = vfio_mig_read(vbasedev, &version_id, sizeof(version_id),
                        region->fd_offset + VFIO_MIG_STRUCT_OFFSET(version_id));
    if (ret < 0) {
        return ret;
    }

    migration->version_id = version_id;
    return 0;
}

static int vfio_migration_init(VFIODevice *vbasedev,
                               struct vfio_region_info *info)
{
    int ret;
    Object *obj = NULL;
    VFIOMigration *migration = NULL;
    g_autofree char *path = NULL;
    g_autofree char *oid = NULL;
    char id[256] = "";

    if (!vbasedev->ops->vfio_get_object) {
        error_report("vfio device=%s can't find 'vfio_get_object' ops\n",
                     vbasedev->name);
        return -EINVAL;
    }

    obj = vbasedev->ops->vfio_get_object(vbasedev);
    if (!obj) {
        return -EINVAL;
    }

    vbasedev->migration = g_new0(VFIOMigration, 1);
    ret = vfio_region_setup(obj, vbasedev,
                            &vbasedev->migration->region,
                            info->index, "migration");
    if (ret != 0) {
        error_report("%s: Failed to setup VFIO migration region %d, ret=%d",
                     vbasedev->name, info->index, ret);
        goto err;
    }

    if (!vbasedev->migration->region.size) {
        error_report("%s: Invalid zero-sized VFIO migration region %d",
                     vbasedev->name, info->index);
        ret = -EINVAL;
        goto err;
    }

    ret = vfio_get_migration_version(vbasedev);
    if (ret != 0) {
        error_report("%s: Failed to get migration driver version: %d",
                     vbasedev->name, ret);
        goto err;
    }

    migration = vbasedev->migration;
    migration->vbasedev = vbasedev;

    oid = vmstate_if_get_id(VMSTATE_IF(DEVICE(obj)));
    if (oid) {
        path = g_strdup_printf("%s/vfio", oid);
    } else {
        path = g_strdup("vfio");
    }
    strpadcpy(id, sizeof(id), path, '\0');

    register_savevm_live(id, VMSTATE_INSTANCE_ID_ANY, 1,
                         &g_savevm_vfio_handlers, vbasedev);
    migration->migration_state.notify = vfio_migration_state_notifier;
    add_migration_state_change_notifier(&migration->migration_state);

    migration->vm_running = runstate_is_running() ? 1 : 0;
    return 0;

err:
    vfio_migration_exit(vbasedev);
    return ret;
}

int vfio_migration_probe(VFIODevice *vbasedev, Error **errp)
{
    struct vfio_region_info *info = NULL;
    Error *local_err = NULL;
    int ret = -ENOTSUP;

    if (!vbasedev->enable_migration) {
        error_setg(&vbasedev->migration_blocker,
                   "Current Disable the VFIO device migration");
        return ret;
    }

    ret = vfio_get_dev_region_info(vbasedev, VFIO_REGION_TYPE_MIGRATION,
                                   VFIO_REGION_SUBTYPE_MIGRATION, &info);
    if (ret != 0) {
        error_report("vfio device=%s find migration region from vfio failed\n",
                     vbasedev->name);
        goto add_blocker;
    }

    ret = vfio_migration_init(vbasedev, info);
    if (ret != 0) {
        goto add_blocker;
    }

    g_free(info);
    return 0;

add_blocker:
    error_setg(&vbasedev->migration_blocker,
               "VFIO device=%s doesn't support migration",
               vbasedev->name);
    g_free(info);

    migrate_add_blocker(vbasedev->migration_blocker, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        error_free(vbasedev->migration_blocker);
        vbasedev->migration_blocker = NULL;
    }

    return ret;
}

void vfio_migration_finalize(VFIODevice *vbasedev)
{
    Object *obj = NULL;
    g_autofree char *path = NULL;
    g_autofree char *oid = NULL;
    char id[256] = "";

    if (vbasedev->migration) {
        VFIOMigration *migration = vbasedev->migration;

        remove_migration_state_change_notifier(&migration->migration_state);
        vfio_migration_exit(vbasedev);
    }

    if (vbasedev->migration_blocker) {
        migrate_del_blocker(vbasedev->migration_blocker);
        error_free(vbasedev->migration_blocker);
        vbasedev->migration_blocker = NULL;
    }

    obj = vbasedev->ops->vfio_get_object(vbasedev);
    if (!obj)
        return;
    oid = vmstate_if_get_id(VMSTATE_IF(DEVICE(obj)));
    path = oid ? g_strdup_printf("%s/vfio", oid) : g_strdup("vfio");
    strpadcpy(id, sizeof(id), path, '\0');
    unregister_savevm(NULL, id, vbasedev);
}

int vfio_device_disable(VFIODevice *vbasedev)
{
    if (vbasedev->migration_blocker ||
        (vbasedev->migration->device_state & VFIO_DEVICE_STATE_RUNNING) == 0) {
        return 0;
    }

    return vfio_migration_set_state(vbasedev, ~VFIO_DEVICE_STATE_RUNNING, 0);
}

int vfio_device_enable(VFIODevice *vbasedev)
{
    /*
     * There is no need to change vfio device state when vm is paused.
     */
    if (vbasedev->migration_blocker ||
        (vbasedev->migration->device_state & VFIO_DEVICE_STATE_RUNNING)) {
        return 0;
    }

    return vfio_migration_set_state(vbasedev, ~VFIO_DEVICE_STATE_MASK,
                                    VFIO_DEVICE_STATE_RUNNING);
}

int set_all_vfio_device(bool is_enable)
{
    VFIOGroup *group;
    VFIODevice *vbasedev;
    int ret = 0;
    int rret;

    QLIST_FOREACH(group, &vfio_group_list, next) {
        QLIST_FOREACH(vbasedev, &group->device_list, next) {
            rret = is_enable ?
                   vfio_device_enable(vbasedev) : vfio_device_disable(vbasedev);
            ret = rret ? rret : ret;
        }
    }

    return ret;
}