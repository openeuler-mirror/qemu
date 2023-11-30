/*
 * hygon psp device emulation
 *
 * Copyright 2024 HYGON Corp.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

#include "qemu/osdep.h"
#include "qemu/compiler.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "migration/vmstate.h"
#include "hw/qdev-properties.h"
#include "sysemu/runstate.h"
#include <sys/ioctl.h>

#define TYPE_PSP_DEV "psp"
OBJECT_DECLARE_SIMPLE_TYPE(PSPDevState, PSP_DEV)

struct PSPDevState {
    /* Private */
    DeviceState pdev;

    /* Public */
    Notifier shutdown_notifier;
    int dev_fd;
    bool enabled;

    /**
     * vid is used to identify a virtual machine in qemu.
     * When a virtual machine accesses a tkm key,
     * the TKM module uses different key spaces based on different vids.
    */
    uint32_t vid;
};

#define PSP_DEV_PATH "/dev/hygon_psp_config"
#define HYGON_PSP_IOC_TYPE      'H'
#define PSP_IOC_MUTEX_ENABLE    _IOWR(HYGON_PSP_IOC_TYPE, 1, NULL)
#define PSP_IOC_MUTEX_DISABLE   _IOWR(HYGON_PSP_IOC_TYPE, 2, NULL)
#define PSP_IOC_VPSP_OPT        _IOWR(HYGON_PSP_IOC_TYPE, 3, NULL)

enum VPSP_DEV_CTRL_OPCODE {
    VPSP_OP_VID_ADD,
    VPSP_OP_VID_DEL,
};

struct psp_dev_ctrl {
    unsigned char op;
    union {
        unsigned int vid;
        unsigned char reserved[128];
    } data;
};

static void psp_dev_destroy(PSPDevState *state)
{
    struct psp_dev_ctrl ctrl = { 0 };
    if (state && state->dev_fd) {
        if (state->enabled) {
            ctrl.op = VPSP_OP_VID_DEL;
            if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
                error_report("VPSP_OP_VID_DEL: %d", -errno);
            } else {
                state->enabled = false;
            }
        }
        qemu_close(state->dev_fd);
        state->dev_fd = 0;
    }
}

/**
 * Guest OS performs shut down operations through 'shutdown' and 'powerdown' event.
 * The 'powerdown' event will also trigger 'shutdown' in the end,
 * so only attention to the 'shutdown' event.
 *
 * When Guest OS trigger 'reboot' or 'reset' event, to do nothing.
*/
static void psp_dev_shutdown_notify(Notifier *notifier, void *data)
{
    PSPDevState *state = container_of(notifier, PSPDevState, shutdown_notifier);
    psp_dev_destroy(state);
}

static void psp_dev_realize(DeviceState *dev, Error **errp)
{
    struct psp_dev_ctrl ctrl = { 0 };
    PSPDevState *state = PSP_DEV(dev);

    state->dev_fd = qemu_open_old(PSP_DEV_PATH, O_RDWR);
    if (state->dev_fd < 0) {
        error_setg(errp, "fail to open %s, errno %d.", PSP_DEV_PATH, errno);
        goto end;
    }

    ctrl.op = VPSP_OP_VID_ADD;
    ctrl.data.vid = state->vid;
    if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
        error_setg(errp, "psp_dev_realize VPSP_OP_VID_ADD vid %d, return %d", ctrl.data.vid, -errno);
        goto end;
    }

    state->enabled = true;
    state->shutdown_notifier.notify = psp_dev_shutdown_notify;
    qemu_register_shutdown_notifier(&state->shutdown_notifier);
end:
    return;
}

static struct Property psp_dev_properties[] = {
    DEFINE_PROP_UINT32("vid", PSPDevState, vid, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void psp_dev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "PSP Device";
    dc->realize = psp_dev_realize;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    device_class_set_props(dc, psp_dev_properties);
}

static const TypeInfo psp_dev_info = {
    .name = TYPE_PSP_DEV,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(PSPDevState),
    .class_init = psp_dev_class_init,
};

static void psp_dev_register_types(void)
{
    type_register_static(&psp_dev_info);
}

type_init(psp_dev_register_types)
