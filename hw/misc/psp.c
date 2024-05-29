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
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "hw/i386/e820_memory_layout.h"
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
    VPSP_OP_SET_DEFAULT_VID_PERMISSION,
    VPSP_OP_GET_DEFAULT_VID_PERMISSION,
    VPSP_OP_SET_GPA,
};

struct psp_dev_ctrl {
    unsigned char op;
    unsigned char resv[3];
    union {
        unsigned int vid;
        // Set or check the permissions for the default VID
        unsigned int def_vid_perm;
        struct {
            uint64_t gpa_start;
            uint64_t gpa_end;
        } gpa;
        unsigned char reserved[128];
    } __attribute__ ((packed)) data;
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

static MemoryRegion *find_memory_region_by_name(MemoryRegion *root, const char *name) {
    MemoryRegion *subregion;
    MemoryRegion *result;

    if (strcmp(root->name, name) == 0)
        return root;

    QTAILQ_FOREACH(subregion, &root->subregions, subregions_link) {
        result = find_memory_region_by_name(subregion, name);
        if (result) {
            return result;
        }
    }

    return NULL;
}

static void psp_dev_realize(DeviceState *dev, Error **errp)
{
    int i;
    char mr_name[128] = {0};
    struct psp_dev_ctrl ctrl = { 0 };
    PSPDevState *state = PSP_DEV(dev);
    MemoryRegion *root_mr = get_system_memory();
    MemoryRegion *find_mr = NULL;
    uint64_t ram2_start = 0, ram2_end = 0;

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

    for (i = 0 ;; ++i) {
        sprintf(mr_name, "mem2-%d", i);
        find_mr = find_memory_region_by_name(root_mr, mr_name);
        if (!find_mr)
            break;

        if (!ram2_start)
            ram2_start = find_mr->addr;
        ram2_end = find_mr->addr + find_mr->size - 1;
    }

    if (ram2_start != ram2_end) {
        ctrl.op = VPSP_OP_SET_GPA;
        ctrl.data.gpa.gpa_start = ram2_start;
        ctrl.data.gpa.gpa_end = ram2_end;
        if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
            error_setg(errp, "psp_dev_realize VPSP_OP_SET_GPA (start 0x%lx, end 0x%lx), return %d",
                        ram2_start, ram2_end, -errno);
            goto del_vid;
        }
    }

    state->enabled = true;
    state->shutdown_notifier.notify = psp_dev_shutdown_notify;
    qemu_register_shutdown_notifier(&state->shutdown_notifier);

    return;
del_vid:
    ctrl.op = VPSP_OP_VID_DEL;
    ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl);
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
