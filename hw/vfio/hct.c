/*
 * vfio based mediated ccp(hct) assignment support
 *
 * Copyright 2023 HYGON Corp.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

#include <linux/vfio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>

#include "qemu/osdep.h"
#include "qemu/queue.h"
#include "qemu/main-loop.h"
#include "qemu/log.h"
#include "trace.h"
#include "hw/pci/pci.h"
#include "hw/vfio/pci.h"
#include "qemu/range.h"
#include "sysemu/kvm.h"
#include "hw/pci/msi.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"

#define MAX_CCP_CNT                  48
#define DEF_CCP_CNT_MAX              16
#define PAGE_SIZE                    4096
#define HCT_SHARED_MEMORY_SIZE       (PAGE_SIZE * MAX_CCP_CNT)
#define CCP_INDEX_BYTES              4
#define PATH_MAX                     4096
#define TYPE_HCT_DEV                 "hct"
#define PCI_HCT_DEV(obj)             OBJECT_CHECK(HCTDevState, (obj), TYPE_HCT_DEV)
#define HCT_MAX_PASID                (1 << 8)

#define PCI_VENDOR_ID_HYGON_CCP      0x1d94
#define PCI_DEVICE_ID_HYGON_CCP      0x1468

#define VFIO_DEVICE_CCP_SET_MODE     _IO(VFIO_TYPE, VFIO_BASE + 32)
#define VFIO_DEVICE_CCP_GET_MODE     _IO(VFIO_TYPE, VFIO_BASE + 33)

#define HCT_SHARE_DEV                "/dev/hct_share"
#define CCP_SHARE_DEV                "/dev/ccp_share"
#define PCI_DRV_HCT_DIR              "/sys/bus/pci/drivers/hct"
#define PCI_DRV_CCP_DIR              "/sys/bus/pci/drivers/ccp"

#define DEF_VERSION_STRING           "0.1"
#define HCT_VERSION_STR_02           "0.2"
#define HCT_VERSION_STR_05           "0.5"
#define HCT_VERSION_STR_06           "0.6"
#define VERSION_SIZE                 16

#define HCT_SHARE_IOC_TYPE           'C'
#define HCT_SHARE_OP_TYPE            0x01
#define HCT_SHARE_OP                 _IOWR(HCT_SHARE_IOC_TYPE, \
                                           HCT_SHARE_OP_TYPE,  \
                                           struct hct_dev_ctrl)
#define HCT_SHARE_OP_DMA_MAP         0x01
#define HCT_SHARE_OP_GET_ID          0x03
#define HCT_SHARE_OP_GET_PASID       0x04
#define HCT_SHARE_OP_DMA_UNMAP       0x05
#define HCT_SHARE_OP_GET_VERSION     0x06

/* BARS */
#define HCT_REG_BAR_IDX              2
#define HCT_SHARED_BAR_IDX           3
#define HCT_PASID_BAR_IDX            4

#define PASID_OFFSET                 40
#define HCT_PASID_MEM_GID_OFFSET     1024

static volatile struct hct_data {
    int init;
    int hct_fd;
    unsigned long pasid;
    unsigned long hct_shared_size;
    uint8_t *pasid_memory;
    uint8_t *hct_shared_memory;
    uint8_t hct_version[VERSION_SIZE];
    uint8_t ccp_index[MAX_CCP_CNT];
    uint8_t ccp_cnt;
    uint8_t driver;
} hct_data;

typedef struct SharedDevice {
    PCIDevice dev;
    int shared_memory_offset;
} SharedDevice;

typedef struct HctDevState {
    SharedDevice sdev;
    VFIODevice vdev;
    MemoryRegion mmio;
    MemoryRegion shared;
    MemoryRegion pasid;
    uint64_t map_size[PCI_NUM_REGIONS];
    void *maps[PCI_NUM_REGIONS];
    char *ccp_dev_path;
} HCTDevState;

struct hct_dev_ctrl {
    unsigned char op;
    unsigned char rsvd[3];
    union {
        unsigned char version[VERSION_SIZE];
        unsigned int id;
        unsigned int pasid;
        struct {
            unsigned long vaddr;
            unsigned long iova;
            unsigned long size;
        };
    };
};

enum ccp_dev_used_mode {
    _KERNEL_SPACE_USED = 0,
    _USER_SPACE_USED,
};

enum MDEV_USED_TYPE {
    MDEV_USED_FOR_HOST,
    MDEV_USED_FOR_VM,
    MDEV_USED_UNDEF
};

enum hct_ccp_driver_mode_type {
    HCT_CCP_DRV_MOD_UNINIT = 0,
    HCT_CCP_DRV_MOD_HCT,
    HCT_CCP_DRV_MOD_CCP,
};

static int hct_get_sysfs_value(const char *path, int *val)
{
    FILE *fp = NULL;
    char buf[CCP_INDEX_BYTES] = {0};
    unsigned long v;

    fp = fopen(path, "r");
    if (!fp) {
        error_report("fail to open %s, errno %d.\n", path, errno);
        return -EINVAL;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        return -EINVAL;
    }

    if (1 != sscanf(buf, "%lu", &v)) {
        fclose(fp);
        return -EINVAL;
    }

    *val = (int)v;

    fclose(fp);
    return 0;
}

/*
 * the memory layout of pasid_memory is as follows:
 * offset -- 0              1024                            4096
 * a page -- |pasid(8B) --- |gid(8B) ---                    |
 */
static int pasid_get_and_init(HCTDevState *state)
{
    void *base = (void *)hct_data.pasid_memory;
    struct hct_dev_ctrl ctrl;
    unsigned long *gid = NULL;
    int ret = 0;

    ctrl.op = HCT_SHARE_OP_GET_PASID;
    ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
    if (ret < 0) {
        ret = -errno;
        error_report("get pasid fail, errno: %d.", errno);
        goto out;
    }

    hct_data.pasid = (unsigned long)ctrl.pasid;
    *(unsigned long *)base = (unsigned long)ctrl.pasid;

    ctrl.op = HCT_SHARE_OP_GET_ID;
    ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
    if (ret < 0) {
        ret = -errno;
        error_report("get gid fail, errno: %d", errno);
        goto out;
    }

    gid = (unsigned long *)((unsigned long)base + HCT_PASID_MEM_GID_OFFSET);
    *(unsigned long *)gid = (unsigned long)ctrl.id;

out:
    return ret;
}

static const MemoryRegionOps hct_mmio_ops = {
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid =
        {
            .min_access_size = 4,
            .max_access_size = 4,
        },
};

static void vfio_hct_detach_device(HCTDevState *state)
{
    vfio_detach_device(&state->vdev);
}

static void vfio_hct_exit(PCIDevice *dev)
{
    HCTDevState *state = PCI_HCT_DEV(dev);

    if (hct_data.driver == HCT_CCP_DRV_MOD_HCT)
        vfio_hct_detach_device(state);

    if (hct_data.hct_fd) {
        qemu_close(hct_data.hct_fd);
        hct_data.hct_fd = 0;
    }
    if (state->vdev.fd) {
        qemu_close(state->vdev.fd);
        state->vdev.fd = 0;
    }
}

static Property vfio_hct_properties[] = {
    DEFINE_PROP_STRING("sysfsdev", HCTDevState, vdev.sysfsdev),
    DEFINE_PROP_STRING("path", HCTDevState, ccp_dev_path),
    DEFINE_PROP_END_OF_LIST(),
};

static void vfio_ccp_compute_needs_reset(VFIODevice *vdev)
{
    vdev->needs_reset = false;
}

struct VFIODeviceOps vfio_ccp_ops = {
    .vfio_compute_needs_reset = vfio_ccp_compute_needs_reset,
};

/* create BAR2, BAR3 and BAR4 space for the virtual machine. */
static int vfio_hct_region_mmap(HCTDevState *state)
{
    int ret;
    int i;
    struct vfio_region_info *info;

    for (i = 0; i < PCI_ROM_SLOT; i++) {
        ret = vfio_get_region_info(&state->vdev, i, &info);
        if (ret)
            goto out;

        if (info->size) {
            state->maps[i] = mmap(NULL, info->size, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, state->vdev.fd, info->offset);
            if (state->maps[i] == MAP_FAILED) {
                ret = -errno;
                g_free(info);
                error_report("vfio mmap fail\n");
                goto out;
            }
            state->map_size[i] = info->size;
        }
        g_free(info);
    }

    memory_region_init_io(&state->mmio, OBJECT(state), &hct_mmio_ops,
                          state, "hct mmio", state->map_size[HCT_REG_BAR_IDX]);
    memory_region_init_ram_device_ptr(&state->mmio, OBJECT(state),
                                      "hct mmio", state->map_size[HCT_REG_BAR_IDX],
                                      state->maps[HCT_REG_BAR_IDX]);

    memory_region_init_io(&state->shared, OBJECT(state), &hct_mmio_ops, state,
                          "hct shared memory", PAGE_SIZE);
    memory_region_init_ram_device_ptr(
        &state->shared, OBJECT(state), "hct shared memory", PAGE_SIZE,
        (void *)hct_data.hct_shared_memory +
            state->sdev.shared_memory_offset * PAGE_SIZE);

    memory_region_init_io(&state->pasid, OBJECT(state), &hct_mmio_ops, state,
                          "hct pasid", PAGE_SIZE);
    memory_region_init_ram_device_ptr(&state->pasid, OBJECT(state), "hct pasid",
                                      PAGE_SIZE, hct_data.pasid_memory);

    pci_register_bar(&state->sdev.dev, HCT_REG_BAR_IDX,
                     PCI_BASE_ADDRESS_SPACE_MEMORY, &state->mmio);
    pci_register_bar(&state->sdev.dev, HCT_SHARED_BAR_IDX,
                     PCI_BASE_ADDRESS_SPACE_MEMORY, &state->shared);
    pci_register_bar(&state->sdev.dev, HCT_PASID_BAR_IDX,
                     PCI_BASE_ADDRESS_SPACE_MEMORY, &state->pasid);
out:
    return ret;
}

static int hct_check_duplicated_index(int index)
{
    int cnt;
    for (cnt = 0; cnt < hct_data.ccp_cnt; cnt++) {
        if (hct_data.ccp_index[cnt] == index) {
            error_report("many mdev shouldn't be mapped to one ccp in a "
                         "virtual machine!\n");
            return -1;
        }
    }

    hct_data.ccp_index[hct_data.ccp_cnt++] = index;
    return 0;
}

static int hct_ccp_dev_get_index(HCTDevState *state)
{
    char fpath[PATH_MAX] = {0};
    char *ptr = NULL;
    uint32_t loops= 0;
    uint32_t max_loops = 10000;
    int ccp_idx;
    int fd;
    int ret;

    if (!state->ccp_dev_path) {
        error_report("state->ccp_dev_path is NULL.");
        return -1;
    }

    ptr = strstr(state->ccp_dev_path, "ccp");
    if (!ptr)
        return -1;

    ccp_idx = atoi(ptr + strlen("ccp"));
    if (hct_check_duplicated_index(ccp_idx))
        return -1;

    fd = qemu_open_old(state->ccp_dev_path, O_RDWR);
    if (fd < 0) {
        error_report("fail to open %s, errno %d.", fpath, errno);
        return -1;
    }

    while ((ret = ioctl(fd, VFIO_DEVICE_CCP_SET_MODE, _USER_SPACE_USED)) < 0
                                        && errno == EAGAIN) {
        if (++loops > max_loops) {
            error_report("loops = %u, configure user mode fail.\n", loops);
            break;
        }
        usleep(10);
    }
    if (ret < 0) {
        error_report("configure user mode for %s fail, errno %d", fpath, errno);
        close(fd);
        return -1;
    }

    state->vdev.fd = fd;
    state->sdev.shared_memory_offset = ccp_idx;
    return 0;
}

static int hct_get_ccp_index(HCTDevState *state)
{
    char path[PATH_MAX] = {0};
    int mdev_used, index;

    if (hct_data.driver == HCT_CCP_DRV_MOD_CCP)
        return hct_ccp_dev_get_index(state);

    if (!state->vdev.sysfsdev) {
        error_report("state->vdev.sysfsdev is NULL.");
        return -1;
    }

    if (memcmp((void *)hct_data.hct_version, HCT_VERSION_STR_06,
                                 sizeof(HCT_VERSION_STR_06)) >= 0) {
        snprintf(path, PATH_MAX, "%s/vendor/use", state->vdev.sysfsdev);
        if (hct_get_sysfs_value(path, &mdev_used)) {
            error_report("get %s sysfs value fail.\n", path);
            return -1;
        } else if (mdev_used != MDEV_USED_FOR_VM) {
            error_report("The value of file node(%s) is %d, should be MDEV_USED_FOR_VM(%d), pls check.\n",
			    path, mdev_used, MDEV_USED_FOR_VM);
            return -1;
        }
    }

    snprintf(path, PATH_MAX, "%s/vendor/id", state->vdev.sysfsdev);
    if (hct_get_sysfs_value(path, &index)) {
        error_report("get %s sysfs value fail.\n", path);
        return -1;
    }

    if (hct_check_duplicated_index(index))
        return -1;

    state->sdev.shared_memory_offset = index;
    return 0;
}

static int hct_api_version_check(void)
{
    struct hct_dev_ctrl ctrl;
    int ret;

    ctrl.op = HCT_SHARE_OP_GET_VERSION;
    memcpy(ctrl.version, DEF_VERSION_STRING, sizeof(DEF_VERSION_STRING));
    ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
    if (ret < 0) {
        error_report("ret %d, errno %d: fail to get hct.ko version.\n", ret, errno);
        return -1;
    } else if (memcmp(ctrl.version,  HCT_VERSION_STR_02, sizeof(HCT_VERSION_STR_02)) < 0) {
        error_report("The hct.ko version is %s, please upgrade to version %s or higher.\n",
                      ctrl.version, HCT_VERSION_STR_02);
        return -1;
    }

    memcpy((void *)hct_data.hct_version, (void *)ctrl.version, sizeof(hct_data.hct_version));
    if (memcmp(ctrl.version,  HCT_VERSION_STR_05, sizeof(HCT_VERSION_STR_05)) < 0)
        hct_data.hct_shared_size = PAGE_SIZE * DEF_CCP_CNT_MAX;
    else
        hct_data.hct_shared_size = HCT_SHARED_MEMORY_SIZE;

    return 0;
}

static int hct_shared_memory_init(void)
{
    int ret = 0;

    hct_data.hct_shared_memory = mmap(NULL, hct_data.hct_shared_size,
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     hct_data.hct_fd, 0);
    if (hct_data.hct_shared_memory == MAP_FAILED) {
        ret = -errno;
        error_report("map hct shared memory fail\n");
        goto out;
    }

out:
    return ret;
}

static void hct_listener_region_add(MemoryListener *listener,
                                    MemoryRegionSection *section)
{
    struct hct_dev_ctrl ctrl;
    hwaddr iova;
    Int128 llend, llsize;
    void *vaddr;
    int ret;

    iova = REAL_HOST_PAGE_ALIGN(section->offset_within_address_space);
    llend = int128_make64(section->offset_within_address_space);
    llend = int128_add(llend, section->size);
    llend = int128_add(llend, int128_exts64(qemu_real_host_page_mask()));

    if (int128_ge(int128_make64(iova), llend)) {
        return;
    }

    if (!section->mr->ram) {
        return;
    }

    vaddr = memory_region_get_ram_ptr(section->mr) +
            section->offset_within_region +
            (iova - section->offset_within_address_space);
    llsize = int128_sub(llend, int128_make64(iova));

    ctrl.op = HCT_SHARE_OP_DMA_MAP;
    ctrl.iova = iova | (hct_data.pasid << PASID_OFFSET);
    ctrl.vaddr = (uint64_t)vaddr;
    ctrl.size = llsize;
    ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
    if (ret < 0)
        error_report("VFIO_MAP_DMA: %d, iova=%lx", -errno, iova);
}

static void hct_listener_region_del(MemoryListener *listener,
                                    MemoryRegionSection *section)
{
    struct hct_dev_ctrl ctrl;
    hwaddr iova;
    Int128 llend, llsize;
    int ret;

    iova = REAL_HOST_PAGE_ALIGN(section->offset_within_address_space);
    llend = int128_make64(section->offset_within_address_space);
    llend = int128_add(llend, section->size);
    llend = int128_add(llend, int128_exts64(qemu_real_host_page_mask()));

    if (int128_ge(int128_make64(iova), llend)) {
        return;
    }

    if (!section->mr->ram) {
        return;
    }

    llsize = int128_sub(llend, int128_make64(iova));

    ctrl.op = HCT_SHARE_OP_DMA_UNMAP;
    ctrl.iova = iova | (hct_data.pasid << PASID_OFFSET);
    ctrl.size = llsize;
    ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
    if (ret < 0)
        error_report("VFIO_UNMAP_DMA: %d", -errno);
}

static MemoryListener hct_memory_listener = {
    .region_add = hct_listener_region_add,
    .region_del = hct_listener_region_del,
};

static int hct_get_used_driver_walk(const char *path)
{
    const char filter[] = "0000:*";
    struct dirent *e = NULL;
    DIR *dir = NULL;
    int ret = -EINVAL;

    dir = opendir(path);
    if (dir == NULL)
        return -1;

    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.')
            continue;

        if (fnmatch(filter, e->d_name, 0) == 0) {
            ret = 0;
            break;
        }
    }

    closedir(dir);
    return ret;
}

static void hct_data_uninit(HCTDevState *state)
{
    if (hct_data.hct_fd) {
        qemu_close(hct_data.hct_fd);
        hct_data.hct_fd = 0;
    }

    if (state->vdev.fd) {
        qemu_close(state->vdev.fd);
        state->vdev.fd = 0;
    }

    if (hct_data.pasid) {
        hct_data.pasid = 0;
    }

    if (hct_data.pasid_memory) {
        munmap(hct_data.pasid_memory, PAGE_SIZE);
        hct_data.pasid_memory = NULL;
    }

    if (hct_data.hct_shared_memory) {
        munmap((void *)hct_data.hct_shared_memory, hct_data.hct_shared_size);
        hct_data.hct_shared_memory = NULL;
    }

    memory_listener_unregister(&hct_memory_listener);
}

static int hct_data_init(HCTDevState *state)
{
    const char *hct_shr_name = NULL;
    int ret;

    if (hct_data.init == 0) {

        ret = hct_get_used_driver_walk(PCI_DRV_HCT_DIR);
        if (ret == 0) {
            hct_data.driver = HCT_CCP_DRV_MOD_HCT;
            hct_shr_name = HCT_SHARE_DEV;
        } else {
            hct_data.driver = HCT_CCP_DRV_MOD_CCP;
            hct_shr_name = CCP_SHARE_DEV;
        }

        hct_data.hct_fd = qemu_open_old(hct_shr_name, O_RDWR);
        if (hct_data.hct_fd < 0) {
            error_report("fail to open %s, errno %d.", hct_shr_name, errno);
            ret = -errno;
            goto out;
        }

        /* The hct.ko version number needs not to be less than 0.2. */
        ret = hct_api_version_check();
        if (ret)
            goto out;

        /* assign a page to the virtual BAR3 of each CCP. */
        ret = hct_shared_memory_init();
        if (ret)
            goto out;

        hct_data.pasid_memory = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (hct_data.pasid_memory < 0)
            goto unmap_shared_memory_exit;

        /* assign a unique pasid to each virtual machine. */
        ret = pasid_get_and_init(state);
        if (ret < 0)
            goto unmap_pasid_memory_exit;

        /* perform DMA_MAP and DMA_UNMAP operations on all memories of the
         * virtual machine. */
        memory_listener_register(&hct_memory_listener, &address_space_memory);

        hct_data.init = 1;
    }

    return hct_get_ccp_index(state);

unmap_pasid_memory_exit:
    munmap(hct_data.pasid_memory, PAGE_SIZE);

unmap_shared_memory_exit:
    munmap((void *)hct_data.hct_shared_memory, hct_data.hct_shared_size);

out:
    return ret;
}

/* When device is loaded */
static void vfio_hct_realize(PCIDevice *pci_dev, Error **errp)
{
    int ret;
    char *mdevid;
    Error *err = NULL;
    HCTDevState *state = PCI_HCT_DEV(pci_dev);

    ret = hct_data_init(state);
    if (ret < 0) {
        error_setg(errp, "hct data initialization failed.");
        goto out;
    }

    if (hct_data.driver == HCT_CCP_DRV_MOD_HCT) {
        mdevid =  g_path_get_basename(state->vdev.sysfsdev);
        state->vdev.name = g_strdup_printf("%s", mdevid);

        ret = vfio_attach_device(state->vdev.name, &state->vdev,
                    pci_device_iommu_address_space(pci_dev), &err);
        if (ret) {
            error_setg(errp, "attach device failed, name = %s.", state->vdev.name);
            g_free(state->vdev.name);
            goto data_uninit_out;
        }

        state->vdev.ops = &vfio_ccp_ops;
        state->vdev.dev = &state->sdev.dev.qdev;
        g_free(state->vdev.name);
    }

    ret = vfio_hct_region_mmap(state);
    if (ret < 0) {
        error_setg(errp, "hct vfio region mmap failed.");
        goto detach_device_out;
    }

    return;

detach_device_out:
    if (hct_data.driver == HCT_CCP_DRV_MOD_HCT)
        vfio_hct_detach_device(state);

data_uninit_out:
    hct_data_uninit(state);

out:
    return;
}

static void hct_dev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pdc = PCI_DEVICE_CLASS(klass);

    dc->desc = "HCT Device";
    device_class_set_props(dc, vfio_hct_properties);

    pdc->realize = vfio_hct_realize;
    pdc->exit = vfio_hct_exit;
    pdc->vendor_id = PCI_VENDOR_ID_HYGON_CCP;
    pdc->device_id = PCI_DEVICE_ID_HYGON_CCP;
    pdc->class_id = PCI_CLASS_CRYPT_OTHER;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);

    return;
}

static const TypeInfo pci_hct_info = {
    .name = TYPE_HCT_DEV,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(HCTDevState),
    .class_init = hct_dev_class_init,
    .interfaces =
        (InterfaceInfo[]){
            {INTERFACE_CONVENTIONAL_PCI_DEVICE},
            {},
        },
};

static void hct_register_types(void) {
    type_register_static(&pci_hct_info);
}

type_init(hct_register_types);
