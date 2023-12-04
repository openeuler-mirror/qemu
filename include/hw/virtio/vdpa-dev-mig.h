/*
 * Vhost Vdpa Device Migration Header
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All Rights Reserved.
 */

#ifndef _VHOST_VDPA_MIGRATION_H
#define _VHOST_VDPA_MIGRATION_H

#include "hw/virtio/vdpa-dev.h"

enum {
    VDPA_DEVICE_START,
    VDPA_DEVICE_STOP,
    VDPA_DEVICE_PRE_START,
    VDPA_DEVICE_PRE_STOP,
    VDPA_DEVICE_CANCEL,
    VDPA_DEVICE_POST_START,
    VDPA_DEVICE_START_ASYNC,
    VDPA_DEVICE_STOP_ASYNC,
    VDPA_DEVICE_PRE_START_ASYNC,
    VDPA_DEVICE_QUERY_OP_STATE,
};

void vdpa_migration_register(VhostVdpaDevice *vdev);

void vdpa_migration_unregister(VhostVdpaDevice *vdev);

#endif /* _VHOST_VDPA_MIGRATION_H */
