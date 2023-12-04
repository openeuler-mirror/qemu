/*
 * Vhost Vdpa Device Migration Header
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All Rights Reserved.
 */

#ifndef _VHOST_VDPA_MIGRATION_H
#define _VHOST_VDPA_MIGRATION_H

#include "hw/virtio/vdpa-dev.h"

void vdpa_migration_register(VhostVdpaDevice *vdev);

void vdpa_migration_unregister(VhostVdpaDevice *vdev);

#endif /* _VHOST_VDPA_MIGRATION_H */
