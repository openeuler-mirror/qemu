/*
 * vhost vdpa device iommufd backend
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All Rights Reserved.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include <sys/ioctl.h>
#include <linux/vhost.h>
#include "qapi/error.h"
#include "hw/virtio/vdpa-dev-iommufd.h"

static QLIST_HEAD(, VDPAIOMMUFDContainer) vdpa_container_list =
    QLIST_HEAD_INITIALIZER(vdpa_container_list);

static int vhost_vdpa_container_connect_iommufd(VDPAIOMMUFDContainer *container)
{
    IOMMUFDBackend *iommufd = container->iommufd;
    uint32_t ioas_id;
    Error *err = NULL;

    if (!iommufd) {
        return -1;
    }

    if (!iommufd_backend_connect(iommufd, &err)) {
        error_report_err(err);
        return -1;
    }

    if (!iommufd_backend_alloc_ioas(iommufd, &ioas_id, &err)) {
        error_report_err(err);
        iommufd_backend_disconnect(iommufd);
        return -1;
    }
    container->ioas_id = ioas_id;
    return 0;
}

static void vhost_vdpa_container_disconnect_iommufd(VDPAIOMMUFDContainer *container)
{
    IOMMUFDBackend *iommufd = container->iommufd;
    uint32_t ioas_id = container->ioas_id;

    if (!iommufd) {
        return;
    }

    iommufd_backend_free_id(iommufd, ioas_id);
    iommufd_backend_disconnect(iommufd);
}

static IOMMUFDHWPT *vhost_vdpa_find_hwpt(VDPAIOMMUFDContainer *container,
                                         VhostVdpaDevice *vdev)
{
    IOMMUFDHWPT *hwpt = NULL;
    VhostVdpaDevice *tmp = NULL;

    QLIST_FOREACH(hwpt, &container->hwpt_list, next) {
        QLIST_FOREACH(tmp, &hwpt->device_list, next) {
            if (tmp == vdev) {
                return hwpt;
            }
        }
    }

    return NULL;
}

static VDPAIOMMUFDContainer *vhost_vdpa_find_container(VhostVdpaDevice *vdev)
{
    VDPAIOMMUFDContainer *container = NULL;

    QLIST_FOREACH(container, &vdpa_container_list, next) {
        if (container->iommufd == vdev->iommufd) {
            return container;
        }
    }

    return NULL;
}

static VDPAIOMMUFDContainer *vhost_vdpa_create_container(VhostVdpaDevice *vdev)
{
    VDPAIOMMUFDContainer *container = NULL;

    container = g_new0(VDPAIOMMUFDContainer, 1);
    container->iommufd = vdev->iommufd;
    QLIST_INIT(&container->hwpt_list);

    QLIST_INSERT_HEAD(&vdpa_container_list, container, next);

    return container;
}

static void vhost_vdpa_destroy_container(VDPAIOMMUFDContainer *container)
{
    if (!container) {
        return;
    }

    container->iommufd = NULL;
    QLIST_SAFE_REMOVE(container, next);
    g_free(container);
}

static void vhost_vdpa_device_unbind_iommufd(VhostVdpaDevice *vdev)
{
    int ret;
    ret = ioctl(vdev->vhostfd, VHOST_VDPA_UNBIND_IOMMUFD, 0);
    if (ret) {
        qemu_log("vhost vdpa device unbind iommufd failed: %d, devid: %d\n",
                 ret, vdev->iommufd_devid);
    }
}

static int vhost_vdpa_device_bind_iommufd(VhostVdpaDevice *vdev)
{
    IOMMUFDBackend *iommufd = vdev->iommufd;
    struct vdpa_dev_bind_iommufd bind = {
        .iommufd = iommufd->fd,
        .out_devid = -1,
    };
    int ret;

    /* iommufd auto unbind when vdev->vhostfd close */
    ret = ioctl(vdev->vhostfd, VHOST_VDPA_BIND_IOMMUFD, &bind);
    if (ret) {
        qemu_log("vhost vdpa device bind iommufd failed: %d\n", ret);
        return ret;
    }
    vdev->iommufd_devid = bind.out_devid;
    return 0;
}

static int vhost_vdpa_container_attach_device(VDPAIOMMUFDContainer *container, VhostVdpaDevice *vdev)
{
    IOMMUFDBackend *iommufd = NULL;
    IOMMUFDHWPT *hwpt = NULL;
    Error *err = NULL;
    uint32_t pt_id;
    int ret;

    if (!container || !container->iommufd || container->iommufd != vdev->iommufd) {
        return -1;
    }

    iommufd = container->iommufd;

    /* try to find an available hwpt */
    QLIST_FOREACH(hwpt, &container->hwpt_list, next) {
        pt_id = hwpt->hwpt_id;
        ret = ioctl(vdev->vhostfd, VHOST_VDPA_ATTACH_IOMMUFD_PT, &pt_id);
        if (ret == 0) {
            QLIST_INSERT_HEAD(&hwpt->device_list, vdev, next);
            return 0;
        }
    }

    /* available hwpt not found in the container, create a new one */
    hwpt = g_new0(IOMMUFDHWPT, 1);
    QLIST_INIT(&hwpt->device_list);

    if (!iommufd_backend_alloc_hwpt(iommufd, vdev->iommufd_devid,
                                    container->ioas_id, 0, 0, 0, NULL,
                                    &pt_id, NULL, &err)) {
        error_report_err(err);
        ret = -1;
        goto free_mem;
    }

    hwpt->hwpt_id = pt_id;

    ret = ioctl(vdev->vhostfd, VHOST_VDPA_ATTACH_IOMMUFD_PT, &pt_id);
    if (ret) {
        qemu_log("vhost vdpa device attach iommufd pt failed: %d\n", ret);
        goto free_hwpt;
    }

    QLIST_INSERT_HEAD(&hwpt->device_list, vdev, next);
    QLIST_INSERT_HEAD(&container->hwpt_list, hwpt, next);

    return 0;

free_hwpt:
    iommufd_backend_free_id(iommufd, hwpt->hwpt_id);
free_mem:
    g_free(hwpt);
    return ret;
}

static void vhost_vdpa_container_detach_device(VDPAIOMMUFDContainer *container, VhostVdpaDevice *vdev)
{
    IOMMUFDBackend *iommufd = vdev->iommufd;
    IOMMUFDHWPT *hwpt = NULL;

    /* find the hwpt using by this device */
    hwpt = vhost_vdpa_find_hwpt(container, vdev);
    if (!hwpt) {
        return;
    }

    ioctl(vdev->vhostfd, VHOST_VDPA_DETACH_IOMMUFD_PT, &hwpt->hwpt_id);

    QLIST_SAFE_REMOVE(vdev, next);

    /* No device using this hwpt, free it */
    if (QLIST_EMPTY(&hwpt->device_list)) {
        iommufd_backend_free_id(iommufd, hwpt->hwpt_id);
        QLIST_SAFE_REMOVE(hwpt, next);
        g_free(hwpt);
    }
}

int vhost_vdpa_attach_container(VhostVdpaDevice *vdev)
{
    VDPAIOMMUFDContainer *container = NULL;
    IOMMUFDBackend *iommufd = vdev->iommufd;
    bool new_container = false;
    int ret = 0;

    if (!iommufd) {
        return 0;
    }

    container = vhost_vdpa_find_container(vdev);
    if (!container) {
        container = vhost_vdpa_create_container(vdev);
        if (!container) {
            qemu_log("vdpa create container failed\n");
            return -1;
        }
        ret = vhost_vdpa_container_connect_iommufd(container);
        if (ret) {
            qemu_log("vdpa container connect iommufd failed\n");
            goto destroy;
        }
        new_container = true;
    }

    ret = vhost_vdpa_device_bind_iommufd(vdev);
    if (ret) {
        qemu_log("vdpa device bind iommufd failed\n");
        goto disconnect;
    }

    ret = vhost_vdpa_container_attach_device(container, vdev);
    if (ret) {
        qemu_log("vdpa container attach device failed\n");
        goto unbind;
    }

    return 0;

unbind:
    vhost_vdpa_device_unbind_iommufd(vdev);
disconnect:
    if (!new_container) {
        return ret;
    }
    vhost_vdpa_container_disconnect_iommufd(container);
destroy:
    vhost_vdpa_destroy_container(container);

    return ret;
}

void vhost_vdpa_detach_container(VhostVdpaDevice *vdev)
{
    VDPAIOMMUFDContainer *container = NULL;
    IOMMUFDBackend *iommufd = vdev->iommufd;

    if (!iommufd) {
        return;
    }

    container = vhost_vdpa_find_container(vdev);
    if (!container) {
        return;
    }

    vhost_vdpa_container_detach_device(container, vdev);

    vhost_vdpa_device_unbind_iommufd(vdev);

    if (!QLIST_EMPTY(&container->hwpt_list)) {
        return;
    }
    /* No HWPT in this container, destroy it */
    vhost_vdpa_container_disconnect_iommufd(container);

    vhost_vdpa_destroy_container(container);
}