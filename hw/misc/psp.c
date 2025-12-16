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
#include "exec/ramblock.h"
#include "hw/i386/e820_memory_layout.h"
#include "migration/migration.h"
#include "migration/misc.h"
#include <sys/ioctl.h>

#define TYPE_PSP_DEV "psp"
OBJECT_DECLARE_SIMPLE_TYPE(PSPDevState, PSP_DEV)

#define VPSP_MIGRATE_VERSION                1
#define HUGEPAGE_SIZE                       (1024*1024*2)

static int vpsp_dev_pre_save(void *opaque);
static int vpsp_dev_post_load(void *opaque, int version_id);

struct PSPDevState {
    /* Private */
    DeviceState pdev;

    /* Public */
    Notifier shutdown_notifier;
    NotifierWithReturn precopy_notifier;

    int dev_fd;
    bool enabled;

    /**
     * vid is used to identify a virtual machine in qemu.
     * When a virtual machine accesses a tkm key,
     * the TKM module uses different key spaces based on different vids.
    */
    uint32_t vid;
    /* pinned hugepage numbers */
    int hp_num;

    uint32_t img_len;
    uint8_t *key_img;
    uint32_t ctx_len;
    uint8_t *cmd_ctx;
};

static const VMStateDescription vmstate_vpsp_dev = {
    .name = "vpsp-dev",
    .version_id = VPSP_MIGRATE_VERSION,
    .minimum_version_id = VPSP_MIGRATE_VERSION,
    .pre_save = vpsp_dev_pre_save,
    .post_load = vpsp_dev_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(img_len, PSPDevState),
        VMSTATE_VBUFFER_ALLOC_UINT32(key_img,
                                     PSPDevState, 0, 0,
                                     img_len),
        VMSTATE_UINT32(ctx_len, PSPDevState),
        VMSTATE_VBUFFER_ALLOC_UINT32(cmd_ctx,
                                     PSPDevState, 0, 0,
                                     ctx_len),
        VMSTATE_END_OF_LIST()
    }
};

#define PSP_DEV_PATH "/dev/hygon_psp_config"
#define HYGON_PSP_IOC_TYPE      'H'
#define PSP_IOC_MUTEX_ENABLE    _IOWR(HYGON_PSP_IOC_TYPE, 1, NULL)
#define PSP_IOC_MUTEX_DISABLE   _IOWR(HYGON_PSP_IOC_TYPE, 2, NULL)
#define PSP_IOC_VPSP_OPT        _IOWR(HYGON_PSP_IOC_TYPE, 3, NULL)
#define PSP_IOC_PIN_USER_PAGE   _IOWR(HYGON_PSP_IOC_TYPE, 4, NULL)
#define PSP_IOC_UNPIN_USER_PAGE _IOWR(HYGON_PSP_IOC_TYPE, 5, NULL)

enum VPSP_DEV_CTRL_OPCODE {
    VPSP_OP_VID_ADD,
    VPSP_OP_VID_DEL,
    VPSP_OP_SET_DEFAULT_VID_PERMISSION,
    VPSP_OP_GET_DEFAULT_VID_PERMISSION,
    VPSP_OP_SET_GPA,
    VPSP_OP_BACKUP_KEY,
    VPSP_OP_RESTORE_KEY,
    VPSP_OP_BACKUP_CTX,
    VPSP_OP_RESTORE_CTX,
};

typedef struct key_img_ctl {
    unsigned int img_len;
    void *key_img_ptr;
} __attribute__ ((packed)) key_img_ctl_t;

typedef struct cmd_ctx_ctl {
    unsigned int buffer_len;
    void *cmd_ctx_ptr;
} __attribute__ ((packed)) cmd_ctx_ctl_t;

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
        key_img_ctl_t key_img_ctl;
        cmd_ctx_ctl_t cmd_ctx_ctl;
        unsigned char reserved[128];
    } __attribute__ ((packed)) data;
};

static int vpsp_dev_backup_key_img(struct PSPDevState *state)
{
    int ret = 0;
    uint32_t img_buffer_len = 0;
    struct psp_dev_ctrl ctrl = { 0 };

    if (state && state->dev_fd) {
        if (state->enabled && state->vid) {
            ctrl.op = VPSP_OP_BACKUP_KEY;

            // get actual key image buffer length
            ctrl.data.key_img_ctl.img_len = 0;
            if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
                error_report("ioctl VPSP_OP_BACKUP_KEY: %d", -errno);
                return -1;
            }

            img_buffer_len = ctrl.data.key_img_ctl.img_len;
            // no key images need to migrate
            if (unlikely(img_buffer_len == 0))
                return 0;

            // free last key images
            if (unlikely(state->key_img))
                g_free(state->key_img);

            state->key_img = g_malloc0(img_buffer_len);
            if (!state->key_img) {
                error_report("g_malloc0 failed: %d", -errno);
                return -1;
            }

            // get key images backup buffer
            ctrl.data.key_img_ctl.img_len = img_buffer_len;
            ctrl.data.key_img_ctl.key_img_ptr = state->key_img;
            ret = ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl);
            if (ret < 0) {
                error_report("ioctl VPSP_OP_BACKUP_KEY: %d", -errno);
                return -1;
            }

            state->img_len = ctrl.data.key_img_ctl.img_len;
        }
    }

    return 0;
}

static int vpsp_dev_restore_key_img(struct PSPDevState *state)
{
    int ret = 0;
    struct psp_dev_ctrl ctrl = { 0 };

    if (state && state->dev_fd) {
        if (state->enabled && state->vid && state->img_len) {
            if (!state->key_img) {
                error_report("PSPDevState load invalid, key_img is null\n");
                return -1;
            }

            ctrl.op = VPSP_OP_RESTORE_KEY;
            ctrl.data.key_img_ctl.img_len = state->img_len;
            ctrl.data.key_img_ctl.key_img_ptr = state->key_img;
            if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
                error_report("ioctl VPSP_OP_RESTORE_KEY: %d", -errno);
                return -1;
            }

            // release key_img buffer, for migrate again
            g_free(state->key_img);
            state->key_img = NULL;
            state->img_len = 0;
        }
    }

    return ret;
}

static int vpsp_dev_backup_cmd_ctx(struct PSPDevState *state)
{
    int ret = 0;
    uint32_t buffer_len = 0;
    struct psp_dev_ctrl ctrl = { 0 };

    if (state && state->dev_fd) {
        if (state->enabled && state->vid) {
            ctrl.op = VPSP_OP_BACKUP_CTX;

            // get actual cmd context serialization buffer length
            ctrl.data.cmd_ctx_ctl.buffer_len = 0;
            if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
                error_report("ioctl VPSP_OP_BACKUP_CTX: %d", -errno);
                return -1;
            }

            buffer_len = ctrl.data.cmd_ctx_ctl.buffer_len;
            // no cmd ctx need to migrate
            if (buffer_len == 0)
                return 0;

            // free last cmd_ctx buffer
            if (unlikely(state->cmd_ctx))
                g_free(state->cmd_ctx);

            state->cmd_ctx = g_malloc0(buffer_len);
            if (!state->cmd_ctx) {
                error_report("g_malloc0 failed: %d", -errno);
                return -1;
            }

            ctrl.data.cmd_ctx_ctl.buffer_len = buffer_len;
            ctrl.data.cmd_ctx_ctl.cmd_ctx_ptr = state->cmd_ctx;
            ret = ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl);
            if (ret < 0) {
                error_report("ioctl VPSP_OP_BACKUP_CTX: %d", -errno);
                return -1;
            }
            state->ctx_len = ctrl.data.cmd_ctx_ctl.buffer_len;
        }
    }

    return 0;
}

static int vpsp_dev_restore_cmd_ctx(struct PSPDevState *state)
{
    struct psp_dev_ctrl ctrl = { 0 };

    if (state && state->dev_fd) {
        if (state->enabled && state->vid && state->ctx_len) {
            if (!state->cmd_ctx) {
                error_report("PSPDevState load invalid, cmd_ctx is null\n");
                return -1;
            }

            ctrl.op = VPSP_OP_RESTORE_CTX;
            ctrl.data.cmd_ctx_ctl.buffer_len = state->ctx_len;
            ctrl.data.cmd_ctx_ctl.cmd_ctx_ptr = state->cmd_ctx;

            if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
                error_report("ioctl PSP_IOC_VPSP_OPT: %d", -errno);
                return -1;
            }

            // release cmd ctx buffer, for migrate again
            g_free(state->cmd_ctx);
            state->cmd_ctx = NULL;
            state->ctx_len = 0;
        }
    }

    return 0;
}

static int vpsp_dev_pre_save(void *opaque)
{
    int ret = 0;
    struct PSPDevState *state = opaque;

    /**
     * Back up the key image in the final stage
     * to ensure the key image is up-to-date
     */
    ret = vpsp_dev_backup_key_img(opaque);

    if (ret && state->key_img) {
        g_free(state->key_img);
        state->key_img = NULL;
        state->img_len = 0;
    }
    return ret;
}

static int vpsp_dev_post_load(void *opaque, int version_id)
{
    int ret = 0;

    /**
     * During load, there are no sequencing requirements
     * between restore of the key image and cmd_ctx
     */
    ret = vpsp_dev_restore_cmd_ctx(opaque);
    if (ret)
        return ret;

    ret = vpsp_dev_restore_key_img(opaque);
    if (ret)
        return ret;

    return ret;
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

static int precopy_state_notifier(NotifierWithReturn *notifier, void *data)
{
    int ret = 0, i;
    PrecopyNotifyData *pnd = data;
    char mr_name[128] = {0};
    MemoryRegion *find_mr = NULL;
    PSPDevState *state = container_of(notifier, PSPDevState, precopy_notifier);

    if (pnd->reason != PRECOPY_NOTIFY_COMPLETE)
        goto end;

    /**
     * The host kernel will then check each cmd_ctx
     * to confirm all cmd_ctx are completed.
     */
    ret = vpsp_dev_backup_cmd_ctx(state);
    if (ret)
        goto end;

    for (i = 0 ; i < state->hp_num; ++i) {
        sprintf(mr_name, "mem2-%d", i);
        find_mr = find_memory_region_by_name(get_system_memory(), mr_name);
        if (!find_mr) {
            error_report("fail to find memory region by name %s.", mr_name);
            ret = -ENOMEM;
            goto end;
        }

        /* ensure mem2 memoryregion is migrated during downtime */
        memory_region_set_dirty(find_mr, 0, HUGEPAGE_SIZE);
    }

end:
    if (ret && state->cmd_ctx) {
        g_free(state->cmd_ctx);
        state->cmd_ctx = NULL;
        state->ctx_len = 0;
    }
    return ret;
}

static int pin_user_hugepage(int fd, uint64_t vaddr)
{
    int ret;

    ret = ioctl(fd, PSP_IOC_PIN_USER_PAGE, vaddr);
    /* 22: Invalid argument, some old kernel doesn't support this ioctl command */
    if (ret != 0 && errno == EINVAL) {
        ret = 0;
    }
    return ret;
}

static int unpin_user_hugepage(int fd, uint64_t vaddr)
{
    int ret;

    ret = ioctl(fd, PSP_IOC_UNPIN_USER_PAGE, vaddr);
    /* 22: Invalid argument, some old kernel doesn't support this ioctl command */
    if (ret != 0 && errno == EINVAL) {
        ret = 0;
    }
    return ret;
}

static int pin_psp_user_hugepages(struct PSPDevState *state, MemoryRegion *root)
{
    int ret = 0;
    char mr_name[128] = {0};
    int i, pinned_num;
    MemoryRegion *find_mr = NULL;

    for (i = 0 ; i < state->hp_num; ++i) {
        sprintf(mr_name, "mem2-%d", i);
        find_mr = find_memory_region_by_name(root, mr_name);
        if (!find_mr) {
            error_report("fail to find memory region by name %s.", mr_name);
            ret = -ENOMEM;
            goto end;
        }

        ret = pin_user_hugepage(state->dev_fd, (uint64_t)find_mr->ram_block->host);
        if (ret) {
            error_report("fail to pin_user_hugepage, ret: %d.", ret);
            goto end;
        }
    }
end:
    if (ret) {
        pinned_num = i;
        for (i = 0 ; i < pinned_num; ++i) {
            sprintf(mr_name, "mem2-%d", i);
            find_mr = find_memory_region_by_name(root, mr_name);
            if (!find_mr) {
                continue;
            }
            unpin_user_hugepage(state->dev_fd, (uint64_t)find_mr->ram_block->host);
        }

    }
    return ret;
}

static int unpin_psp_user_hugepages(struct PSPDevState *state, MemoryRegion *root)
{
    int ret = 0;
    char mr_name[128] = {0};
    int i;
    MemoryRegion *find_mr = NULL;

    for (i = 0 ; i < state->hp_num; ++i) {
        sprintf(mr_name, "mem2-%d", i);
        find_mr = find_memory_region_by_name(root, mr_name);
        if (!find_mr) {
            continue;
        }

        ret = unpin_user_hugepage(state->dev_fd, (uint64_t)find_mr->ram_block->host);
        if (ret) {
            error_report("fail to unpin_user_hugepage, ret: %d.", ret);
            goto end;
        }
    }
end:
    return ret;
}

static void psp_dev_destroy(PSPDevState *state)
{
    struct psp_dev_ctrl ctrl = { 0 };
    if (state && state->dev_fd) {
        if (state->enabled) {
            ctrl.op = VPSP_OP_VID_DEL;
            if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
                error_report("VPSP_OP_VID_DEL: %d", -errno);
            }

            /* Unpin hugepage memory */
            if (unpin_psp_user_hugepages(state, get_system_memory())) {
                error_report("unpin_psp_user_hugepages failed");
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

    state->hp_num = i;

    if (ram2_start != ram2_end) {
        ctrl.op = VPSP_OP_SET_GPA;
        ctrl.data.gpa.gpa_start = ram2_start;
        ctrl.data.gpa.gpa_end = ram2_end;
        if (ioctl(state->dev_fd, PSP_IOC_VPSP_OPT, &ctrl) < 0) {
            error_setg(errp, "psp_dev_realize VPSP_OP_SET_GPA (start 0x%lx, end 0x%lx), return %d",
                        ram2_start, ram2_end, -errno);
            goto del_vid;
        }

        /* Pin hugepage memory */
        if(pin_psp_user_hugepages(state, root_mr)) {
            error_setg(errp, "pin_psp_user_hugepages failed.");
            goto del_vid;
        }
    }

    state->enabled = true;
    state->shutdown_notifier.notify = psp_dev_shutdown_notify;
    qemu_register_shutdown_notifier(&state->shutdown_notifier);

    state->precopy_notifier.notify = precopy_state_notifier;
    precopy_add_notifier(&state->precopy_notifier);

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
    dc->vmsd = &vmstate_vpsp_dev;

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
