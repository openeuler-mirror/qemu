/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
 */

#include <sys/ioctl.h>
#include <linux/vhost.h>
#include "qemu/osdep.h"
#include "migration/misc.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "hw/virtio/vhost.h"
#include "hw/virtio/vdpa-dev.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-access.h"
#include "migration/register.h"
#include "migration/migration.h"
#include "qemu-common.h"
#include "sysemu/runstate.h"
#include "qemu/error-report.h"
#include "hw/virtio/vdpa-dev-mig.h"
#include "migration/qemu-file-types.h"
#include "qemu/main-loop.h"

/*
 * Flags used as delimiter:
 * 0xffffffff => MSB 32-bit all 1s
 * 0xef10     => emulated (virtual) function IO
 * 0x0000     => 16-bits reserved for flags
 */
#define VDPA_MIG_FLAG_END_OF_STATE      (0xffffffffef100001ULL)
#define VDPA_MIG_FLAG_DEV_CONFIG_STATE  (0xffffffffef100002ULL)
#define VDPA_MIG_FLAG_DEV_SETUP_STATE   (0xffffffffef100003ULL)

static int vhost_vdpa_call(struct vhost_dev *dev, unsigned long int request,
                           void *arg)
{
    struct vhost_vdpa *v = dev->opaque;
    int fd = v->device_fd;

    if (dev->vhost_ops->backend_type != VHOST_BACKEND_TYPE_VDPA) {
        error_report("backend type isn't VDPA. Operation not permitted!\n");
        return -EPERM;
    }

    return ioctl(fd, request, arg);
}

static int vhost_vdpa_set_mig_state(struct vhost_dev *dev, uint8_t state)
{
    return vhost_vdpa_call(dev, VHOST_VDPA_SET_MIG_STATE, &state);
}

static int vhost_vdpa_dev_buffer_size(struct vhost_dev *dev, uint32_t *size)
{
    return vhost_vdpa_call(dev, VHOST_GET_DEV_BUFFER_SIZE, size);
}

static int vhost_vdpa_dev_buffer_save(struct vhost_dev *dev, QEMUFile *f)
{
    struct vhost_vdpa_config *config;
    unsigned long config_size = offsetof(struct vhost_vdpa_config, buf);
    uint32_t buffer_size = 0;
    int ret;

    ret = vhost_vdpa_dev_buffer_size(dev, &buffer_size);
    if (ret) {
        error_report("get dev buffer size failed: %d\n", ret);
        return ret;
    }

    qemu_put_be32(f, buffer_size);

    config = g_malloc(buffer_size + config_size);
    config->off = 0;
    config->len = buffer_size;

    ret = vhost_vdpa_call(dev, VHOST_GET_DEV_BUFFER, config);
    if (ret) {
        error_report("get dev buffer failed: %d\n", ret);
        goto free;
    }

    qemu_put_buffer(f, config->buf, buffer_size);
free:
    g_free(config);

    return ret;
}

static int vhost_vdpa_dev_buffer_load(struct vhost_dev *dev, QEMUFile *f)
{
    struct vhost_vdpa_config *config;
    unsigned long config_size = offsetof(struct vhost_vdpa_config, buf);
    uint32_t buffer_size, recv_size;
    int ret;

    buffer_size = qemu_get_be32(f);

    config = g_malloc(buffer_size + config_size);
    config->off = 0;
    config->len = buffer_size;

    recv_size = qemu_get_buffer(f, config->buf, buffer_size);
    if (recv_size != buffer_size) {
        error_report("read dev mig buffer failed, buffer_size: %u, "
                     "recv_size: %u\n", buffer_size, recv_size);
        ret = -EINVAL;
        goto free;
    }

    ret = vhost_vdpa_call(dev, VHOST_SET_DEV_BUFFER, config);
    if (ret) {
        error_report("set dev buffer failed: %d\n", ret);
    }

free:
    g_free(config);

    return ret;
}

static int vhost_vdpa_device_suspend(VhostVdpaDevice *vdpa)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(vdpa);

    if (!vdpa->started || vdpa->suspended) {
        return 0;
    }

    vdpa->suspended = true;

    return vhost_dev_suspend(&vdpa->dev, vdev, false);
}

static int vhost_vdpa_device_resume(VhostVdpaDevice *vdpa)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(vdpa);
    MigrationIncomingState *mis = migration_incoming_get_current();
    int ret;

    if (!vdpa->started ||
        (!vdpa->suspended && mis->state != RUN_STATE_RESTORE_VM)) {
        return 0;
    }

    ret = vhost_dev_resume(&vdpa->dev, vdev, false);
    if (ret < 0) {
        return ret;
    }

    vdpa->suspended = false;
    return ret;
}

static void vdpa_dev_migration_handle_incoming_bh(void *opaque)
{
    struct vhost_dev *hdev = opaque;
    int ret;

    /* Post start device, unsupport rollback if failed! */
    ret = vhost_vdpa_set_mig_state(hdev, VDPA_DEVICE_POST_START);
    if (ret) {
        error_report("Failed to set state: POST_START\n");
    }
}

static void vdpa_dev_vmstate_change(void *opaque, bool running, RunState state)
{
    VhostVdpaDevice *vdpa = VHOST_VDPA_DEVICE(opaque);
    struct vhost_dev *hdev = &vdpa->dev;
    int ret;
    MigrationState *ms = migrate_get_current();
    MigrationIncomingState *mis = migration_incoming_get_current();

    if (!running) {
        if (state == RUN_STATE_FINISH_MIGRATE || state == RUN_STATE_PAUSED) {
            ret = vhost_vdpa_device_suspend(vdpa);
            if (ret) {
                error_report("suspend vdpa device failed: %d\n", ret);
                if (ms->migration_thread_running) {
                    migrate_fd_cancel(ms);
                }
            }
        }
    } else {
        if (vdpa->suspended) {
            ret = vhost_vdpa_device_resume(vdpa);
            if (ret) {
                error_report("vhost vdpa device resume failed: %d\n", ret);
            }
        }

        if (mis->state == RUN_STATE_RESTORE_VM) {
            ret = vhost_vdpa_call(hdev, VHOST_VDPA_RESUME, NULL);
            if (ret) {
                error_report("migration dest resume device failed: %d\n", ret);
                exit(EXIT_FAILURE);
            }
            /* post resume */
            mis->bh = qemu_bh_new(vdpa_dev_migration_handle_incoming_bh,
                                  hdev);
            qemu_bh_schedule(mis->bh);
        }
    }
}

static int vdpa_save_setup(QEMUFile *f, void *opaque)
{
    qemu_put_be64(f, VDPA_MIG_FLAG_DEV_SETUP_STATE);
    qemu_put_be64(f, VDPA_MIG_FLAG_END_OF_STATE);

    return qemu_file_get_error(f);
}

static int vdpa_save_complete_precopy(QEMUFile *f, void *opaque)
{
    VhostVdpaDevice *vdev = VHOST_VDPA_DEVICE(opaque);
    struct vhost_dev *hdev = &vdev->dev;
    int ret;

    qemu_put_be64(f, VDPA_MIG_FLAG_DEV_CONFIG_STATE);
    qemu_put_be16(f, (uint16_t)vdev->suspended);
    if (vdev->suspended) {
        ret = vhost_vdpa_dev_buffer_save(hdev, f);
        if (ret) {
            error_report("Save vdpa device buffer failed: %d\n", ret);
            return ret;
        }
    }
    qemu_put_be64(f, VDPA_MIG_FLAG_END_OF_STATE);

    return qemu_file_get_error(f);
}

static int vdpa_load_state(QEMUFile *f, void *opaque, int version_id)
{
    VhostVdpaDevice *vdev = VHOST_VDPA_DEVICE(opaque);
    struct vhost_dev *hdev = &vdev->dev;

    int ret;
    uint64_t data;
    uint16_t suspended;

    data = qemu_get_be64(f);
    while (data != VDPA_MIG_FLAG_END_OF_STATE) {
        if (data == VDPA_MIG_FLAG_DEV_SETUP_STATE) {
            data = qemu_get_be64(f);
            if (data == VDPA_MIG_FLAG_END_OF_STATE) {
                return 0;
            } else {
                error_report("SETUP STATE: EOS not found 0x%lx\n", data);
                return -EINVAL;
            }
        } else if (data == VDPA_MIG_FLAG_DEV_CONFIG_STATE) {
            suspended = qemu_get_be16(f);
            if (suspended) {
                ret = vhost_vdpa_dev_buffer_load(hdev, f);
                if (ret) {
                    error_report("fail to restore device buffer.\n");
                    return ret;
                }
            }
        }

        ret = qemu_file_get_error(f);
        if (ret) {
            error_report("qemu file error: %d\n", ret);
            return ret;
        }
        data = qemu_get_be64(f);
    }

    return 0;
}

static int vdpa_load_setup(QEMUFile *f, void *opaque)
{
    VhostVdpaDevice *v = VHOST_VDPA_DEVICE(opaque);
    struct vhost_dev *hdev = &v->dev;
    int ret = 0;

    ret = vhost_vdpa_set_mig_state(hdev, VDPA_DEVICE_PRE_START);
    if (ret) {
        error_report("pre start device failed: %d\n", ret);
        goto out;
    }

    return qemu_file_get_error(f);
out:
    return ret;
}

static SaveVMHandlers savevm_vdpa_handlers = {
    .save_setup = vdpa_save_setup,
    .save_live_complete_precopy = vdpa_save_complete_precopy,
    .load_state = vdpa_load_state,
    .load_setup = vdpa_load_setup,
};

static void vdpa_migration_state_notifier(Notifier *notifier, void *data)
{
    MigrationState *s = data;
    VhostVdpaDevice *vdev = container_of(notifier,
                                         VhostVdpaDevice,
                                         migration_state);
    struct vhost_dev *hdev = &vdev->dev;
    int ret;

    switch (s->state) {
    case MIGRATION_STATUS_CANCELLING:
    case MIGRATION_STATUS_CANCELLED:
    case MIGRATION_STATUS_FAILED:
        ret = vhost_vdpa_set_mig_state(hdev, VDPA_DEVICE_CANCEL);
        if (ret) {
            error_report("Failed to set state CANCEL\n");
        }

        break;
    case MIGRATION_STATUS_COMPLETED:
    default:
        break;
    }
}

void vdpa_migration_register(VhostVdpaDevice *vdev)
{
    vdev->vmstate = qdev_add_vm_change_state_handler(DEVICE(vdev),
                                                     vdpa_dev_vmstate_change,
                                                     DEVICE(vdev));
    register_savevm_live("vdpa", -1, 1,
                         &savevm_vdpa_handlers, DEVICE(vdev));
    vdev->migration_state.notify = vdpa_migration_state_notifier;
    add_migration_state_change_notifier(&vdev->migration_state);
}

void vdpa_migration_unregister(VhostVdpaDevice *vdev)
{
    remove_migration_state_change_notifier(&vdev->migration_state);
    unregister_savevm(NULL, "vdpa", DEVICE(vdev));
    qemu_del_vm_change_state_handler(vdev->vmstate);
}
