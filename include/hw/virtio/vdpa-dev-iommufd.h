/*
 * vhost vDPA device support iommufd header
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All Rights Reserved.
 */

#ifndef _VHOST_VDPA_IOMMUFD_H
#define _VHOST_VDPA_IOMMUFD_H

#include "hw/virtio/vdpa-dev.h"

/*
 * A HW pagetable is called an iommu_domain inside the kernel.
 * This user object allows directly creating an inspecting the
 * domains. Domains that have kernel owned page tables will be
 * associated with an iommufd_ioas that provides the IOVA to
 * PFN map.
 */
typedef struct IOMMUFDHWPT {
    uint32_t hwpt_id;
    QLIST_HEAD(, VhostVdpaDevice) device_list;
    QLIST_ENTRY(IOMMUFDHWPT) next;
} IOMMUFDHWPT;

typedef struct VDPAIOMMUFDContainer {
    struct IOMMUFDBackend *iommufd;
    uint32_t ioas_id;
    QLIST_HEAD(, IOMMUFDHWPT) hwpt_list;
    QLIST_ENTRY(VDPAIOMMUFDContainer) next;
} VDPAIOMMUFDContainer;

struct vdpa_dev_bind_iommufd {
    __s32 iommufd;
    __u32 out_devid;
};

int vhost_vdpa_attach_container(VhostVdpaDevice *vdev);
void vhost_vdpa_detach_container(VhostVdpaDevice *vdev);

#endif /* _VHOST_VDPA_IOMMUFD_H */
