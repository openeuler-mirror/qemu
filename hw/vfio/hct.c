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

#define MAX_CCP_CNT                  16
#define PAGE_SIZE                    4096
#define HCT_SHARED_MEMORY_SIZE       (PAGE_SIZE * MAX_CCP_CNT)
#define CCP_INDEX_BYTES              4
#define PATH_MAX                     4096
#define TYPE_HCT_DEV                 "hct"
#define PCI_HCT_DEV(obj)             OBJECT_CHECK(HCTDevState, (obj), TYPE_HCT_DEV)
#define HCT_MMIO_SIZE                (1 << 20)
#define HCT_MAX_PASID                (1 << 8)

#define PCI_VENDOR_ID_HYGON_CCP      0x1d94
#define PCI_DEVICE_ID_HYGON_CCP      0x1468

#define HCT_SHARE_DEV                "/dev/hct_share"

#define HCT_VERSION_STRING           "0.2"
#define DEF_VERSION_STRING           "0.1"
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

static volatile struct hct_data {
    int init;
    int hct_fd;
    unsigned long pasid;
    uint8_t *pasid_memory;
    uint8_t *hct_shared_memory;
    uint8_t ccp_index[MAX_CCP_CNT];
    uint8_t ccp_cnt;
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
    void *maps[PCI_NUM_REGIONS];
} HCTDevState;

struct hct_dev_ctrl {
    unsigned char op;
    unsigned char rsvd[3];
    union {
        unsigned char version[VERSION_SIZE];
        struct {
            unsigned long vaddr;
            unsigned long iova;
            unsigned long size;
        };
        unsigned int id;
    };
};

static int pasid_get_and_init(HCTDevState *state)
{
    struct hct_dev_ctrl ctrl;
    int ret;

    ctrl.op = HCT_SHARE_OP_GET_PASID;
    ctrl.id = -1;
    ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
    if (ret < 0) {
        ret = -errno;
        error_report("GET_PASID fail: %d", -errno);
        goto out;
    }

    *hct_data.pasid_memory = ctrl.id;
    hct_data.pasid = ctrl.id;

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
    g_free(state->vdev.name);
}

static void vfio_hct_exit(PCIDevice *dev)
{
    HCTDevState *state = PCI_HCT_DEV(dev);

    vfio_hct_detach_device(state);

    if (hct_data.hct_fd) {
        qemu_close(hct_data.hct_fd);
        hct_data.hct_fd = 0;
    }
}

static Property vfio_hct_properties[] = {
    DEFINE_PROP_STRING("sysfsdev", HCTDevState, vdev.sysfsdev),
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
        }
        g_free(info);
    }

    memory_region_init_io(&state->mmio, OBJECT(state), &hct_mmio_ops, state,
                          "hct mmio", HCT_MMIO_SIZE);
    memory_region_init_ram_device_ptr(&state->mmio, OBJECT(state), "hct mmio",
                                      HCT_MMIO_SIZE,
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

static int hct_get_ccp_index(HCTDevState *state)
{
    char path[PATH_MAX];
    char buf[CCP_INDEX_BYTES];
    int fd;
    int ret;
    int ccp_index;

    snprintf(path, PATH_MAX, "%s/vendor/id", state->vdev.sysfsdev);
    fd = qemu_open_old(path, O_RDONLY);
    if (fd < 0) {
        error_report("open %s fail\n", path);
        return -errno;
    }

    ret = read(fd, buf, sizeof(buf));
    if (ret < 0) {
        ret = -errno;
        error_report("read %s fail\n", path);
        goto out;
    }

    if (1 != sscanf(buf, "%d", &ccp_index)) {
        ret = -errno;
        error_report("format addr %s fail\n", buf);
        goto out;
    }

    if (!hct_check_duplicated_index(ccp_index)) {
        state->sdev.shared_memory_offset = ccp_index;
    } else {
        ret = -1;
    }

out:
    qemu_close(fd);
    return ret;
}

static int hct_api_version_check(void)
{
    struct hct_dev_ctrl ctrl;
    int ret;

    ctrl.op = HCT_SHARE_OP_GET_VERSION;
    memcpy(ctrl.version, DEF_VERSION_STRING, sizeof(DEF_VERSION_STRING));
    ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
    if (ret < 0) {
        error_report("ret %d, errno %d: fail to get hct.ko version, please "
                     "update hct.ko to version 0.2.\n",
                     ret, errno);
        return -1;
    } else if (memcmp(ctrl.version, HCT_VERSION_STRING,
                      sizeof(HCT_VERSION_STRING)) < 0) {
        error_report("The API version %s is larger than hct.ko version %s, "
                     "please update hct.ko to version 0.2\n",
                     HCT_VERSION_STRING, ctrl.version);
        return -1;
    }

    return 0;
}

static int hct_shared_memory_init(void)
{
    int ret = 0;

    hct_data.hct_shared_memory =
        mmap(NULL, HCT_SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
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

static void hct_data_uninit(HCTDevState *state)
{
    if (hct_data.hct_fd) {
        qemu_close(hct_data.hct_fd);
        hct_data.hct_fd = 0;
    }

    if (hct_data.pasid) {
        hct_data.pasid = 0;
    }

    if (hct_data.pasid_memory) {
        munmap(hct_data.pasid_memory, PAGE_SIZE);
        hct_data.pasid_memory = NULL;
    }

    if (hct_data.hct_shared_memory) {
        munmap((void *)hct_data.hct_shared_memory, HCT_SHARED_MEMORY_SIZE);
        hct_data.hct_shared_memory = NULL;
    }

    memory_listener_unregister(&hct_memory_listener);
}

static int hct_data_init(HCTDevState *state)
{
    int ret;

    if (hct_data.init == 0) {

        hct_data.hct_fd = qemu_open_old(HCT_SHARE_DEV, O_RDWR);
        if (hct_data.hct_fd < 0) {
            error_report("fail to open %s, errno %d.", HCT_SHARE_DEV, errno);
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
    munmap((void *)hct_data.hct_shared_memory, HCT_SHARED_MEMORY_SIZE);

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

    /* parsing mdev device name from startup scripts */
    mdevid = g_path_get_basename(state->vdev.sysfsdev);
    state->vdev.name = g_strdup_printf("%s", mdevid);

    ret = hct_data_init(state);
    if (ret < 0) {
        g_free(state->vdev.name);
        goto out;
    }

    ret = vfio_attach_device(state->vdev.name, &state->vdev,
                             pci_device_iommu_address_space(pci_dev), &err);

    if (ret) {
        error_report("attach device failed, name = %s", state->vdev.name);
        goto data_uninit_out;
    }

    state->vdev.ops = &vfio_ccp_ops;
    state->vdev.dev = &state->sdev.dev.qdev;

    ret = vfio_hct_region_mmap(state);
    if (ret < 0)
        goto detach_device_out;

    return;

detach_device_out:
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
