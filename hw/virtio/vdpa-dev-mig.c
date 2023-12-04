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

static int vhost_vdpa_device_suspend(VhostVdpaDevice *vdpa)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(vdpa);
    BusState *qbus = BUS(qdev_get_parent_bus(DEVICE(vdev)));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(qbus);
    int ret;

    if (!vdpa->started) {
        return -EFAULT;
    }

    if (!k->set_guest_notifiers) {
        return -EFAULT;
    }

    vdpa->started = false;

    ret = vhost_dev_suspend(&vdpa->dev, vdev, false);
    if (ret) {
        goto suspend_fail;
    }

    ret = k->set_guest_notifiers(qbus->parent, vdpa->dev.nvqs, false);
    if (ret < 0) {
        error_report("vhost guest notifier cleanup failed: %d\n", ret);
        goto set_guest_notifiers_fail;
    }

    vhost_dev_disable_notifiers(&vdpa->dev, vdev);
    return ret;

set_guest_notifiers_fail:
    ret = k->set_guest_notifiers(qbus->parent, vdpa->dev.nvqs, true);
    if (ret) {
        error_report("vhost guest notifier restore failed: %d\n", ret);
    }

suspend_fail:
    vdpa->started = true;
    return ret;
}

static int vhost_vdpa_device_resume(VhostVdpaDevice *vdpa)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(vdpa);
    BusState *qbus = BUS(qdev_get_parent_bus(DEVICE(vdev)));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(qbus);
    int i, ret;

    if (!k->set_guest_notifiers) {
        error_report("binding does not support guest notifiers\n");
        return -ENOSYS;
    }

    ret = vhost_dev_enable_notifiers(&vdpa->dev, vdev);
    if (ret < 0) {
        error_report("Error enabling host notifiers: %d\n", ret);
        return ret;
    }

    ret = k->set_guest_notifiers(qbus->parent, vdpa->dev.nvqs, true);
    if (ret < 0) {
        error_report("Error binding guest notifier: %d\n", ret);
        goto err_host_notifiers;
    }

    vdpa->dev.acked_features = vdev->guest_features;

    ret = vhost_dev_resume(&vdpa->dev, vdev, false);
    if (ret < 0) {
        error_report("Error starting vhost: %d\n", ret);
        goto err_guest_notifiers;
    }
    vdpa->started = true;

    /*
     * guest_notifier_mask/pending not used yet, so just unmask
     * everything here. virtio-pci will do the right thing by
     * enabling/disabling irqfd.
     */
    for (i = 0; i < vdpa->dev.nvqs; i++) {
        vhost_virtqueue_mask(&vdpa->dev, vdev, i, false);
    }

    return ret;

err_guest_notifiers:
    k->set_guest_notifiers(qbus->parent, vdpa->dev.nvqs, false);
err_host_notifiers:
    vhost_dev_disable_notifiers(&vdpa->dev, vdev);
    return ret;
}

static void vdpa_dev_vmstate_change(void *opaque, bool running, RunState state)
{
    VhostVdpaDevice *vdpa = VHOST_VDPA_DEVICE(opaque);
    struct vhost_dev *hdev = &vdpa->dev;
    int ret;
    MigrationState *ms = migrate_get_current();
    MigrationIncomingState *mis = migration_incoming_get_current();

    if (!running) {
        if (ms->state == RUN_STATE_PAUSED) {
            ret = vhost_vdpa_device_suspend(vdpa);
            if (ret) {
                error_report("suspend vdpa device failed: %d\n", ret);
                if (ms->migration_thread_running) {
                    migrate_fd_cancel(ms);
                }
            }
        }
    } else {
        if (ms->state == RUN_STATE_RESTORE_VM) {
            ret = vhost_vdpa_device_resume(vdpa);
            if (ret) {
                error_report("migration dest resume device failed, abort!\n");
                exit(EXIT_FAILURE);
            }
        }

        if (mis->state == RUN_STATE_RESTORE_VM) {
            vhost_vdpa_call(hdev, VHOST_VDPA_RESUME, NULL);
        }
    }
}

void vdpa_migration_register(VhostVdpaDevice *vdev)
{
    vdev->vmstate = qdev_add_vm_change_state_handler(DEVICE(vdev),
                                                     vdpa_dev_vmstate_change,
                                                     DEVICE(vdev));
}

void vdpa_migration_unregister(VhostVdpaDevice *vdev)
{
    qemu_del_vm_change_state_handler(vdev->vmstate);
}
