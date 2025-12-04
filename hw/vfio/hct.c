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
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>

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
#include "hw/virtio/vhost-vsock.h"
#include "migration/migration.h"
#include "migration/vmstate.h"
#include "migration/misc.h"

// ======================== g_id API ====================

#define HCT_DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))
#define HCT_BITMAP_SIZE(nr) HCT_DIV_ROUND_UP(nr, CHAR_BIT * sizeof(unsigned long))

static unsigned long g_id = 0;

enum {
    BITS_PER_WORD = sizeof(unsigned long) * CHAR_BIT
};
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b)  ((b) % BITS_PER_WORD)

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

/*
 * Each HCT and QEMU process allocates a unique GID through the shared memory hct_gid_bitmap.
 * The HCT process uses bits 0-1023 of the bitmap, while the QEMU process uses bits 1024-2047.
 * After a QEMU process allocates a bit_pos from the bitmap, it first locks the range of bytes
 * from bit_pos * 8 to (bit_pos + 1) * 8 in the shared memory hct_gid_locks. Then it calculates
 * the GID:
 *   bit_pos is incremented by 1 (since GID cannot be 0), then subtracted by 1024 (to correct
 *   for the starting offset of the bitmap), resulting in a number in the range of 1-1024.
 *   This number is then left-shifted by HCT_QEMU_GIDS_SHIFT_BITS (18) to obtain the final GID.
 */
#define HCT_GID_BITMAP_SHM_NAME   "hct_gid_bitmap"
#define HCT_GID_LOCK_FILE         "hct_gid_locks"

#define HCT_QEMU_GIDS_BITMAP_MAX_BIT    2048
#define HCT_QEMU_GIDS_BITMAP_MIN_BIT    1024
#define HCT_QEMU_GIDS_SHIFT_BITS        18
#define HCT_GIDS_PER_BLOCK              8
#define HCT_QEMU_GIDS_SHIFT_MASK        (~((1UL << HCT_QEMU_GIDS_SHIFT_BITS) - 1))

static void hct_clear_bit(unsigned long *bitmap, int n);
static uint32_t hct_get_bit(unsigned long *bitmap, int n);

/**
 * File-based bitmap structure for multi-process shared g_id allocation
 */
struct hct_gid_bitmap {
    char name[MAX_PATH];
    unsigned int len;
    unsigned long *bitmap;
    int shm_fd;
    int lock_fd;
};

/// Global bitmap instance for g_id management
struct hct_gid_bitmap *g_hct_gid_bitmap;

/**
 * @brief Allocate a new hct_gid_bitmap structure with shared memory storage
 * @details Creates shared memory if not exists, or opens existing one
 * @return pointer to allocated hct_gid_bitmap on success, NULL on failure
 */
static struct hct_gid_bitmap* hct_gid_bitmap_alloc(void);

/**
 * @brief Free hct_gid_bitmap structure and cleanup resources
 * @param bitmap pointer to hct_gid_bitmap to free
 */
static void hct_gid_bitmap_free(struct hct_gid_bitmap *bitmap);

/**
 * @brief Allocate a g_id from the 1024-bit bitmap, left-shift by 8 bits
 * @param bitmap pointer to hct_gid_bitmap structure
 * @param gid pointer to store allocated g_id
 * @return 0 on success, -EINVAL on failure
 */
static int hct_g_ids_alloc(struct hct_gid_bitmap *bitmap, unsigned long *gid);

/**
 * @brief Free a g_id and clear corresponding bit in bitmap
 * @param bitmap pointer to hct_gid_bitmap structure
 * @param gid g_id to free
 */
static void hct_g_ids_free(struct hct_gid_bitmap *bitmap, unsigned long gid);

/**
 * @brief Check if g_id is locked by trying to acquire exclusive lock on its file region
 * @param bitmap pointer to hct_gid_bitmap structure
 * @param gid g_id to check lock status
 * @return 0 if can lock (process dead), -EINVAL if cannot lock (process alive)
 */
static int hct_g_ids_lock_state_lock(struct hct_gid_bitmap *bitmap, unsigned long gid);

/**
 * @brief Walk through bitmap and check for abnormally exited processes
 * @details Clean up orphaned g_ids by checking file locks
 * @param bitmap pointer to hct_gid_bitmap structure
 */
static void hct_g_ids_lock_state_walk(struct hct_gid_bitmap *bitmap);

// ======================== g_id API end ====================


// ======================== HCT IPC API ====================

// HCT IPC TLV field type definitions
#define HCT_IPC_FIELD_COMMAND           1   /* command */
#define HCT_IPC_FIELD_CONTAINER_FD      2   /* container fd */
#define HCT_IPC_FIELD_GROUP_FD          3   /* group fd */
#define HCT_IPC_FIELD_DEVICE_FD         4   /* device fd */
#define HCT_IPC_FIELD_GROUP_INFO        5   /* group info */
#define HCT_IPC_FIELD_DEVICE_INFO       6   /* device info */
#define HCT_IPC_FIELD_DEVICE_NAMES      7   /* device names */
#define HCT_IPC_FIELD_VCCP_PATH         9   /* vccp device file path */
#define HCT_IPC_FIELD_VCCP_CONTENT      10  /* vccp device file content */
#define HCT_IPC_FIELD_ERROR_REASON      11  /* request failed reason */

// Error code definitions
#define HCT_SUCCESS             0    /* success */
#define HCT_ERROR_CONNECT       (-1) /* connect error */
#define HCT_ERROR_RECEIVE       (-2) /* receive error */
#define HCT_ERROR_INVALID_DATA  (-3) /* invalid data */

// Constants
#define HCT_DAEMON_PID_FILE        "/var/run/hctd.pid"  /* daemon pid file */
#define HCT_DAEMON_SOCK_PATH    "/var/run/hctd.sock"    /* daemon socket path */

#define PCI_ADDR_MAX            20   /* pci address max length */
#define DEVICE_NAME_MAX         32   /* device name max length */

// daemon client command type
enum hct_daemon_req_cmd {
    HCT_CMD_GET_ALL_DEVICES = 0x01,        /* libhct request: get all devices information */
    HCT_CMD_GET_DEVICE_BY_NAME  = 0x02,    /* qemu request: get device info via vccp file */
};

typedef struct hct_vccp_req {
    const char *path;
    const char *content;
} hct_vccp_req;

// TLV structure
typedef struct {
    uint16_t type;
    uint16_t length;
    void *value;
} hct_tlv_t;

// CCP device management structure
typedef struct {
    char pci_addr[PCI_ADDR_MAX];    /* PCI address (e.g., 0000:01:00.0) */
    int group_id;                   /* VFIO group ID */
    int group_fd;                   /* VFIO group FD */
    int device_fd;                  /* VFIO device FD */
    int group_index;                /* Index in group array of the group this device belongs to */
} hct_ccp_device_t;

// Group information structure
typedef struct {
    int group_id;
    int group_fd;
    int device_count;    /* Number of devices in this group */
} hct_group_info_t;

// Overall client device information container
typedef struct {
    int container_fd;           /* VFIO container file descriptor */
    hct_group_info_t *groups;   /* VFIO group information array */
    int group_count;            /* Number of groups */
    hct_ccp_device_t *devices;  /* VFIO device information array */
    int device_count;           /* Number of devices */
} hct_client_info_t;

// Internal constants for client implementation
#define MAX_TLV_BUFFER_SIZE 2048    /* max tlv buffer size */
#define MAX_FD_COUNT 128            /* max fd count */

// ======================== HCT IPC API end ====================

#define HCT_QEMU_VERSION             1
#define MAX_CCP_CNT                  48
#define DEF_CCP_CNT_MAX              16
#define MAX_HW_QUEUES                5
#define PAGE_SIZE                    4096
#define HCT_SHARED_MEMORY_SIZE       (PAGE_SIZE * MAX_CCP_CNT)
#define CCP_INDEX_BYTES              4
#define PATH_MAX                     4096
#define TYPE_HCT_DEV                 "hct"
#define PCI_HCT_DEV(obj)             OBJECT_CHECK(HCTDevState, (obj), TYPE_HCT_DEV)
#define HCT_MAX_PASID                (1 << 8)
#define HCT_MIGRATE_VERSION          1
#define HCT_QUEUE_NEED_INIT          0x01
#define HCT_DMA_MEM_SIZE_256K        (1ull << 18) /* 256K */
#define HCT_DMA_MEM_SIZE_8M          (1ull << 23) /* 8M */

#define PCI_VENDOR_ID_HYGON_CCP      0x1d94
#define PCI_DEVICE_ID_HYGON_CCP      0x1468

#define VFIO_DEVICE_CCP_SET_MODE     _IO(VFIO_TYPE, VFIO_BASE + 32)
#define VFIO_DEVICE_CCP_GET_MODE     _IO(VFIO_TYPE, VFIO_BASE + 33)

#define SHM_DIR                     "/dev/shm/"
#define HCT_GLOBAL_SHARE_SHM_NAME   "hct_global_share"
#define HCT_GLOBAL_SHARE_SHM_PATH SHM_DIR HCT_GLOBAL_SHARE_SHM_NAME

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

/* for migration */
#define HCT_MIG_PROTOCOL_VER         1
#define HCT_MIG_MSG_MAGIC            0x76484354
#define HCT_VMADDR_CID_HOST          2
#define HCT_VSOCK_PORT               12345
#define HCT_MIG_STATE_ONLINE         0x00
#define HCT_MIG_STATE_RESTRICTED     0x01
#define HCT_MIG_STATE_STOPPED        0x02
#define HCT_MIGRATION_START          0x01
#define HCT_CHECK_VM_READINESS       0x02
#define HCT_MIGRATION_DONE           0x03
#define HCT_MIG_MSG_ACK              0x01
#define HCT_MIG_MSG_ERR              0x02

static volatile struct hct_data {
    int init;
    int hct_fd;
    int hct_shm_fd;
    int vfio_container_fd;
    unsigned long pasid;
    unsigned long hct_shared_size;
    uint8_t *hct_shared_memory;
    uint8_t hct_version[VERSION_SIZE];
    uint8_t ccp_index[MAX_CCP_CNT];
    uint8_t ccp_cnt;
    uint8_t driver;
} hct_data;

typedef struct SharedDevice {
    PCIDevice dev;
    int ccp_idx;
    uint8_t *premap_memory;
    uint32_t premap_memory_size;
    uint8_t *share_memory;
    uint32_t share_memory_size;
    uint8_t *dma_memory;
    uint32_t dma_memory_size;
} SharedDevice;

typedef struct HctDevState {
    SharedDevice sdev;
    VFIODevice vdev;
    MemoryRegion mmio;
    MemoryRegion shared;
    AddressSpace dma_as;
    MemoryListener listener;
    NotifierWithReturn precopy_notifier;
    QEMUTimer *migrate_load_timer;
    uint64_t map_size[PCI_NUM_REGIONS];
    uint32_t guest_cid;
    uint32_t migrate_support;
    int client_fd;
    void *maps[PCI_NUM_REGIONS];
    char *ccp_dev_path;
    char *vsock_device;
    int container_fd;   /* vfio container fd */
    int group_fd;       /* vfio group fd */
    int group_id;       /* vfio group id */
    int lock_fd;        /* vccp flock fd for this device only */
    bool migrate_abort_err;
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

struct hct_vfio_shared_memory {
    uint32_t qemu_ver;
    uint64_t pasid;
    uint64_t gid;
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
    HCT_CCP_DRV_MOD_VFIO_PCI,
};


/* @brief vfio-pci mapping function */
static int vfio_hct_dma_map_vfio_pci(int container_fd, void *vaddr, uint64_t iova, uint64_t size);

/* @brief vfio-pci unmapping function */
static int vfio_hct_dma_unmap_vfio_pci(int container_fd, uint64_t iova, uint64_t size);

/* @brief vfio-pci init from daemon  function */
static int vfio_hct_init_from_daemon(HCTDevState *state);

/* @brief hct data uninit function */
static void hct_data_uninit(HCTDevState *state);

/* @brief hct get error string function */
static const char *hct_get_error_string(int error_code);

/* @brief hct find device by pci addr function */
static hct_ccp_device_t* hct_find_device_by_pci_addr(hct_client_info_t *client_info, const char *pci_addr);

/* @brief hct client cleanup function */
static void hct_client_cleanup(hct_client_info_t *client_info);

/* @brief hct client send cmd function */
static int hct_client_send_cmd(const char *socket_path, hct_client_info_t *client_info, enum hct_daemon_req_cmd cmd, void *req_data);

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

static int pasid_get_and_init(HCTDevState *state)
{
    int ret;

    g_hct_gid_bitmap = hct_gid_bitmap_alloc();
    if (!g_hct_gid_bitmap) {
        error_report("Failed to allocate hct_gid_bitmap");
        return -1;
    }

    /* check all gids to confirm if the corresponding qemu processes exist. */
    hct_g_ids_lock_state_walk(g_hct_gid_bitmap);

    ret = hct_g_ids_alloc(g_hct_gid_bitmap, &g_id);
    if (ret < 0) {
        error_report("Failed to allocate g_id, ret=%d", ret);
        return -1;
    }

    hct_data.pasid = g_id >> HCT_QEMU_GIDS_SHIFT_BITS;
    return 0;
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

    /* Release vccp file lock */
    if (state->lock_fd >= 0) {
        flock(state->lock_fd, LOCK_UN);
        close(state->lock_fd);
        state->lock_fd = -1;
    }

    if (hct_data.driver == HCT_CCP_DRV_MOD_VFIO_PCI) {
        state->container_fd = -1;
        state->group_fd = -1;
    }

    if (state->sdev.share_memory) {
        munmap(state->sdev.share_memory, state->sdev.share_memory_size);
    }
    if (state->sdev.dma_memory) {
        munmap(state->sdev.dma_memory, state->sdev.dma_memory_size);
    }
    if (state->sdev.premap_memory) {
        munmap(state->sdev.premap_memory, state->sdev.premap_memory_size);
    }

    memory_listener_unregister(&state->listener);
    hct_data.ccp_cnt--;

    if (hct_data.ccp_cnt == 0) {
        hct_data_uninit(state);
    }
}

static Property vfio_hct_properties[] = {
    DEFINE_PROP_STRING("sysfsdev", HCTDevState, vdev.sysfsdev),
    DEFINE_PROP_STRING("dev", HCTDevState, ccp_dev_path),
    DEFINE_PROP_STRING("vsock-device", HCTDevState, vsock_device),
    DEFINE_PROP_BOOL("migrate-abort-on-error", HCTDevState, migrate_abort_err, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void vfio_ccp_compute_needs_reset(VFIODevice *vdev)
{
    vdev->needs_reset = false;
}

struct VFIODeviceOps vfio_ccp_ops = {
    .vfio_compute_needs_reset = vfio_ccp_compute_needs_reset,
};

/* create BAR2 and BAR3 space for the virtual machine. */
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
            state->maps[i] = mmap(NULL, info->size,
                                 PROT_READ | PROT_WRITE, MAP_SHARED,
                                 state->vdev.fd, info->offset);
            if (state->maps[i] == MAP_FAILED) {
                ret = -errno;
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

    memory_region_init_io(&state->shared, OBJECT(state), &hct_mmio_ops,
                          state, "hct shared memory", HCT_DMA_MEM_SIZE_8M);
    memory_region_init_ram_device_ptr(&state->shared, OBJECT(state),
                                      "hct shared memory", HCT_DMA_MEM_SIZE_8M,
                                      (void *)state->sdev.share_memory);

    pci_register_bar(&state->sdev.dev, HCT_REG_BAR_IDX,
                     PCI_BASE_ADDRESS_SPACE_MEMORY, &state->mmio);
    pci_register_bar(&state->sdev.dev, HCT_SHARED_BAR_IDX,
                     PCI_BASE_ADDRESS_SPACE_MEMORY, &state->shared);

    address_space_init(&state->dma_as, &state->shared, "hct-dma-as");
    memory_listener_register(&state->listener, &state->dma_as);
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

static int hct_ccp_set_mode(HCTDevState *state)
{
    char fpath[PATH_MAX] = {0};
    uint32_t loops= 0;
    uint32_t max_loops = 10000;
    int fd;
    int ret = 0;

    if (state->vdev.fd <= 0) {
        error_report("fail to get device fd %d.", state->vdev.fd);
        return -1;
    }
    fd = state->vdev.fd;

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

    return 0;
}

static int hct_get_ccp_index(HCTDevState *state)
{
    char path[PATH_MAX] = {0};
    char vccp_content[256] = {0};
    FILE *fp = NULL;
    int mdev_used = 0, index = 0;

    if (hct_data.driver == HCT_CCP_DRV_MOD_CCP)
        if(hct_ccp_set_mode(state)) {
            return -1;
        }

    if (hct_data.driver == HCT_CCP_DRV_MOD_HCT) {
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

        state->sdev.ccp_idx = index;
    } else if (hct_data.driver == HCT_CCP_DRV_MOD_CCP || hct_data.driver == HCT_CCP_DRV_MOD_VFIO_PCI) {
        fp = fopen(state->ccp_dev_path, "r");
        if (!fp) {
            error_report("Failed to open vccp file %s to get index: %s", state->ccp_dev_path, strerror(errno));
            return -1;
        }
        if (fgets(vccp_content, sizeof(vccp_content), fp) == NULL) {
            error_report("Failed to read content from vccp file %s", state->ccp_dev_path);
            fclose(fp);
            return -1;
        }
        fclose(fp);

        if (sscanf(state->ccp_dev_path, "/dev/hct/vccp%*d_%d", &index) != 1) {
            error_report("Invalid vccp filename format for vfio-pci: %s", state->ccp_dev_path);
            return -1;
        }
        state->sdev.ccp_idx = index;
        if (hct_check_duplicated_index(index)) {
            return -1;
        }
    } else {
            error_report("Invalid driver mode %d, vccp path %s.\n", hct_data.driver, state->ccp_dev_path);
            return -1;
    }

    return 0;
}

static int hct_api_version_check(void)
{
    struct hct_dev_ctrl ctrl;
    int ret = 0;

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
    const char *name = HCT_GLOBAL_SHARE_SHM_NAME;
    size_t size = HCT_SHARED_MEMORY_SIZE;
    void *vaddr = NULL;
    int shm_fd = -1;
    mode_t oldmod;

    oldmod = umask(0);
    shm_fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    umask(oldmod);
    if (shm_fd < 0 && errno == EEXIST)
        shm_fd = shm_open(name, O_RDWR | O_CLOEXEC, 0666);
    if (shm_fd < 0) {
        error_report("Failed to open file %s, errno: %d.\n", name, errno);
        return -1;
    }

    if (ftruncate(shm_fd, size) != 0) {
        error_report("Failed to ftruncate file %s, errno: %d\n", name, errno);
        close(shm_fd);
        return -1;
    }

    vaddr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (vaddr == MAP_FAILED) {
        error_report("map hct shared memory fail\n");
        close(shm_fd);
        return -ENOMEM;
    }

    if (flock(shm_fd, LOCK_EX | LOCK_NB) == 0)
        memset(vaddr, 0, size);

    hct_data.hct_shm_fd = shm_fd;
    hct_data.hct_shared_size = size;
    hct_data.hct_shared_memory = vaddr;
    return flock(shm_fd, LOCK_SH);
}

static void hct_listener_region_add(MemoryListener *listener,
                                    MemoryRegionSection *section)
{
    struct hct_dev_ctrl ctrl;
    hwaddr iova;
    Int128 llend, llsize;
    void *vaddr;
    int ret = 0;

    iova = REAL_HOST_PAGE_ALIGN(section->offset_within_address_space);
    llend = int128_make64(section->offset_within_address_space);
    llend = int128_add(llend, section->size);
    llend = int128_add(llend, int128_exts64(qemu_real_host_page_mask()));

    if (int128_ge(int128_make64(iova), llend)) {
        return;
    }

    if (!section->mr->ram || !iova) {
        return;
    }

    vaddr = memory_region_get_ram_ptr(section->mr) +
            section->offset_within_region +
            (iova - section->offset_within_address_space);
    llsize = int128_sub(llend, int128_make64(iova));

    /* according to host running mode to select different DMA mapping mode */
    if (hct_data.driver == HCT_CCP_DRV_MOD_VFIO_PCI) {
        iova = iova | (hct_data.pasid << PASID_OFFSET);
        ret = vfio_hct_dma_map_vfio_pci(hct_data.vfio_container_fd, vaddr, iova, llsize);
        if (ret < 0) {
            error_report("VFIO_PCI_MAP_DMA: %d, iova=%lx", ret, iova);
        }
    } else {
        /* host running hct/ccp mdev mode: use hct/ccp module mapping */
        ctrl.op = HCT_SHARE_OP_DMA_MAP;
        ctrl.iova = iova | (hct_data.pasid << PASID_OFFSET);
        ctrl.vaddr = (uint64_t)vaddr;
        ctrl.size = llsize;
        ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
        if (ret < 0) {
            error_report("HCT_MAP_DMA: %d, iova=%lx", -errno, iova);
        }
    }
}

static void hct_listener_region_del(MemoryListener *listener,
                                    MemoryRegionSection *section)
{
    struct hct_dev_ctrl ctrl;
    hwaddr iova;
    Int128 llend, llsize;
    int ret = 0;

    iova = REAL_HOST_PAGE_ALIGN(section->offset_within_address_space);
    llend = int128_make64(section->offset_within_address_space);
    llend = int128_add(llend, section->size);
    llend = int128_add(llend, int128_exts64(qemu_real_host_page_mask()));

    if (int128_ge(int128_make64(iova), llend)) {
        return;
    }

    if (!section->mr->ram || !iova) {
        return;
    }

    llsize = int128_sub(llend, int128_make64(iova));

    /* according to host running mode to select different DMA unmapping mode */
    if (hct_data.driver == HCT_CCP_DRV_MOD_VFIO_PCI) {
        /* use vfio-pci directly unmapping */
        iova = iova | ((uint64_t)hct_data.pasid << PASID_OFFSET);
        ret = vfio_hct_dma_unmap_vfio_pci(hct_data.vfio_container_fd, iova, llsize);
        if (ret < 0) {
            error_report("VFIO_PCI_UNMAP_DMA: %d", ret);
        }
    } else {
        /* host running hct/ccp mdev mode: use hct/ccp module unmapping */
        ctrl.op = HCT_SHARE_OP_DMA_UNMAP;
        ctrl.iova = iova | (hct_data.pasid << PASID_OFFSET);
        ctrl.size = llsize;
        ret = ioctl(hct_data.hct_fd, HCT_SHARE_OP, &ctrl);
        if (ret < 0) {
            error_report("HCT_UNMAP_DMA: %d", -errno);
        }
    }
}

static int hct_listener_premap_memory_init(HCTDevState *state)
{
    void *vaddr = NULL;

    /* Apply for 8M virtual address space in advance. */
    vaddr = mmap(NULL, HCT_DMA_MEM_SIZE_8M,
                PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                -1, 0);
    if (vaddr == MAP_FAILED) {
        error_report("pre-mmap 8M memory fail, errno:%d.\n", errno);
        return -1;
    }

    state->sdev.premap_memory = vaddr;
    state->sdev.premap_memory_size = HCT_DMA_MEM_SIZE_8M;
    return 0;
}

static int hct_shared_page_memory_init(HCTDevState *state)
{
    size_t maddr;
    size_t offset;
    void *vaddr = NULL;

    maddr = (size_t)state->sdev.premap_memory;
    offset = state->sdev.ccp_idx * PAGE_SIZE;
    vaddr = mmap((void *)maddr, PAGE_SIZE,
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_FIXED,
                hct_data.hct_shm_fd, offset);
    if (vaddr == MAP_FAILED || vaddr != (void *)maddr) {
        error_report("mmap shared memory fail, errno:%d.\n", errno);
        return -1;
    }

    state->sdev.share_memory = vaddr;
    state->sdev.share_memory_size = PAGE_SIZE;
    return 0;
}

static int hct_vfio_shared_dma_memory_init(HCTDevState *state)
{
    struct hct_vfio_shared_memory *shr_mem = NULL;
    size_t maddr;
    size_t size;
    void *vaddr = NULL;

    maddr = (size_t)state->sdev.premap_memory;
    maddr += PAGE_SIZE;
    size = HCT_DMA_MEM_SIZE_8M - PAGE_SIZE;
    vaddr = mmap((void *)maddr, size,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                -1, 0);
    if (vaddr == MAP_FAILED || vaddr != (void *)maddr) {
        error_report("mmap dma memory fail, errno:%d.\n", errno);
        return -1;
    }

    maddr -= PAGE_SIZE;
    shr_mem = (void *)(maddr + HCT_DMA_MEM_SIZE_256K);
    shr_mem->qemu_ver = HCT_QEMU_VERSION;
    shr_mem->pasid = hct_data.pasid;
    shr_mem->gid = g_id;

    state->sdev.dma_memory = vaddr;
    state->sdev.dma_memory_size = size;
    return 0;
}

static int hct_listener_dma_memory_init(HCTDevState *state)
{
    if (hct_listener_premap_memory_init(state) != 0) {
        error_report("hct_shared_page_memory_init fail.\n");
        return -1;
    }

    if (hct_shared_page_memory_init(state) != 0) {
        error_report("hct_shared_page_memory_init fail.\n");
        munmap(state->sdev.premap_memory, state->sdev.premap_memory_size);
        return -1;
    }

    if (hct_vfio_shared_dma_memory_init(state) != 0) {
        error_report("hct_vfio_shared_dma_memory_init fail.\n");
        munmap(state->sdev.share_memory, state->sdev.share_memory_size);
        munmap(state->sdev.premap_memory, state->sdev.premap_memory_size);
        return -1;
    }

    state->listener.region_add = hct_listener_region_add;
    state->listener.region_del = hct_listener_region_del;
    return 0;
}

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

static int hct_get_vsock_guest_cid(HCTDevState *state)
{
    Object *dev = NULL;
    gchar *path = NULL;
    uint32_t cid;

    if (!state->vsock_device) {
        dev = object_resolve_path_type("", TYPE_VHOST_VSOCK, NULL);
        if (!dev) {
            error_report("get Object for %s failed.", TYPE_VHOST_VSOCK);
            return -1;
        }
    } else {
        path = g_strdup_printf("/machine/peripheral/%s", state->vsock_device);
        dev = object_resolve_path(path, NULL);
        g_free(path);
        if (!dev) {
            error_report("get Object for %s failed.", path);
            return -1;
        }
    }

    cid = object_property_get_uint(dev, "guest-cid", NULL);
    if (cid <= HCT_VMADDR_CID_HOST) {
        error_report("cid = %u, invalid.", cid);
        return -1;
    }

    state->guest_cid = cid;
    return 0;
}

static int hct_client_vsock_connect_op(HCTDevState *state)
{
    struct sockaddr_vm host_addr;
    int sock_fd;

    if (state->guest_cid <= HCT_VMADDR_CID_HOST) {
        error_report("state->guest_cid = %u, invalid.", state->guest_cid);
        return -1;
    }

    sock_fd = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket creation failed");
        return -1;
    }

    memset(&host_addr, 0, sizeof(host_addr));
    host_addr.svm_family = AF_VSOCK;
    host_addr.svm_cid = state->guest_cid;
    host_addr.svm_port = HCT_VSOCK_PORT;

    if (connect(sock_fd, (struct sockaddr*)&host_addr, sizeof(host_addr)) < 0) {
        perror("connect failed");
        close(sock_fd);
        return -EAGAIN;
    }

    state->client_fd = sock_fd;
    return 0;
}

static int hct_client_vsock_send_msg(int sock_fd, char *buf, size_t len)
{
    struct msghdr msg;
    struct iovec iov;

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = buf;
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    return sendmsg(sock_fd, &msg, 0);
}

static int hct_client_vsock_recv_msg(int sock_fd, char *buf, size_t len)
{
    struct msghdr msg;
    struct iovec iov;

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = buf;
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    return recvmsg(sock_fd, &msg, 0);
}

static int hct_client_send_msg(HCTDevState *state, char *buf, size_t len, int mloop)
{
    int loops = 0;
    int ret = -1;

    while ((ret = hct_client_vsock_connect_op(state)) != 0 && ret == -EAGAIN) {
        if (!mloop) {
            return -EAGAIN;
        }
        if (++loops > mloop) {
            error_report("loops = %d, connect failed.", loops);
            return -1;
        }
        usleep(20 * 1000);
    }

    if (hct_client_vsock_send_msg(state->client_fd, (char *)buf, len) < 0) {
        error_report("hct_client_vsock_send_msg failed.");
        goto exit;
    }

    memset((void *)buf, 0, len);
    if (hct_client_vsock_recv_msg(state->client_fd, (char *)buf, len) < 0) {
        error_report("hct_client_vsock_recv_msg failed.");
        goto exit;
    }

    ret = 0;

exit:
    close(state->client_fd);
    return ret;
}

static int hct_migrate_precopy_notifier(NotifierWithReturn *notifier, void *data)
{
    HCTDevState *state = container_of(notifier, HCTDevState, precopy_notifier);
    MigrationState *ms = migrate_get_current();
    PrecopyNotifyData *pnd = data;
    int msg[16];
    int MAX_CONNECT_LOOPS = 10;
    int MAX_CHECK_LOOPS = 20;
    int loops = 0;
    int ret = -1;

    if (pnd->reason != PRECOPY_NOTIFY_SETUP)
        return 0;

    bql_unlock();

    /* [0]:magic [1]:version [2]:op [3]:sync_state */
    msg[0] = HCT_MIG_MSG_MAGIC;
    msg[1] = HCT_MIG_PROTOCOL_VER;
    msg[2] = HCT_MIGRATION_START;
    msg[3] = 0;
    ret = hct_client_send_msg(state, (char *)msg, sizeof(msg), MAX_CONNECT_LOOPS);
    if (ret != 0 || msg[0] != HCT_MIG_MSG_MAGIC) {
        /* Perform live migration addording to a regular virtual machine. */
        error_report("ret:%d msg[0]:0x%x, please install a newer hct.ko"
                     " for the virtual machine.", ret, msg[0]);
        state->migrate_support = 0;
        ret = 0;
        goto exit;
    } else if (msg[3] != HCT_MIG_MSG_ACK) {
        /* We believe that the virtual machine is not ready,
         * so terminate the live migration.
         */
        if (state->migrate_abort_err) {
            error_setg(pnd->errp, "%s[%u] msg[3]:0x%02x invalid, notifier fail.\n",
                                  __func__, __LINE__, msg[3]);
            ms->state = MIGRATION_STATUS_CANCELLED;
            goto exit;
        } else {
            error_report("%s[%u] msg[3]:0x%02x invalid, notifier fail.\n",
                         __func__, __LINE__, msg[3]);
            state->migrate_support = 0;
            ret = 0;
            goto exit;
        }
    }

    while (++loops <= MAX_CHECK_LOOPS) {
        msg[0] = HCT_MIG_MSG_MAGIC;
        msg[1] = HCT_MIG_PROTOCOL_VER;
        msg[2] = HCT_CHECK_VM_READINESS;
        msg[3] = 0;
        ret = hct_client_send_msg(state, (char *)msg, sizeof(msg), MAX_CONNECT_LOOPS);
        if (ret != 0 || msg[0] != HCT_MIG_MSG_MAGIC) {
            if (state->migrate_abort_err) {
                error_setg(pnd->errp, "%s[%u] ret:%d msg[0]:0x%x invalid, notifier fail.\n",
                                      __func__, __LINE__, ret, msg[0]);
                ms->state = MIGRATION_STATUS_CANCELLED;
                goto exit;
            } else {
                error_report("%s[%u] ret:%d msg[0]:0x%x invalid, notifier fail.\n",
                             __func__, __LINE__, ret, msg[0]);
                state->migrate_support = 0;
                ret = 0;
                goto exit;
            }
        } else if (msg[3] == HCT_MIG_STATE_STOPPED) {
            break;
        }
        sleep(1);
    }
    if (loops > MAX_CHECK_LOOPS) {
        if (state->migrate_abort_err) {
            error_setg(pnd->errp, "%s[%u] loops:%d > MAX_CHECK_LOOPS:%d,"
                                  " the live migration will be canceled.\n",
                                  __func__, __LINE__, loops, MAX_CHECK_LOOPS);
            ms->state = MIGRATION_STATUS_CANCELLED;
            goto exit;
        } else {
            error_report("%s[%u] loops:%d > MAX_CHECK_LOOPS:%d, hct live migration fail.\n",
                         __func__, __LINE__, loops, MAX_CHECK_LOOPS);
            state->migrate_support = 0;
            ret = 0;
            goto exit;
        }
    }

    info_report("%s: recieved HCT_MIG_MSG_ACK.", __func__);
    ret = 0;

exit:
    bql_lock();
    return ret;
}

static void hct_client_connect_timer_cb(void *opaque)
{
    HCTDevState *state = opaque;
    int msg[16];
    int MAX_LOOP_TIMES = 10;
    static int loops = 0;
    int ret = 0;

    /* [0]:magic [1]:version [2]:op [3]:sync_state */
    msg[0] = HCT_MIG_MSG_MAGIC;
    msg[1] = HCT_MIG_PROTOCOL_VER;
    msg[2] = HCT_MIGRATION_DONE;
    msg[3] = 0;
    ret = hct_client_send_msg(state, (char *)msg, sizeof(msg), 0);
    if (ret == -EAGAIN) {
        if (++loops > MAX_LOOP_TIMES) {
            error_report("%s: loops = %d, connect failed.", __func__, loops);
            goto exit;
        }
        timer_mod(state->migrate_load_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                            NANOSECONDS_PER_SECOND * 2); /* 2s */
        return;
    }

    if (ret != 0 || msg[0] != HCT_MIG_MSG_MAGIC || msg[3] != HCT_MIG_MSG_ACK) {
        error_report("ret:%d msg[0]:0x%x msg[3]:0x%x, invalid.\n", ret, msg[0], msg[3]);
        goto exit;
    }
    info_report("%s: recieved HCT_MIG_MSG_ACK.", __func__);

exit:
    timer_free(state->migrate_load_timer);
    state->migrate_load_timer = NULL;
    close(state->client_fd);
    loops = 0;
}

static int hct_dev_post_load(void *opaque, int version_id)
{
    HCTDevState *state = opaque;

    if (!state->migrate_support)
        return 0;

    /* When there are multiple ccp devices, each device will
     * execute the post_load function once.
     * We only hope that the first device can set the timer.
     */
    state->migrate_support = 0;
    state->migrate_load_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                        hct_client_connect_timer_cb, state);
    timer_mod(state->migrate_load_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                        NANOSECONDS_PER_SECOND * 5); /* 5s */

    return 0;
}

static void hct_data_uninit(HCTDevState *state)
{
    if (hct_data.hct_fd) {
        qemu_close(hct_data.hct_fd);
        hct_data.hct_fd = 0;
    }

    if (hct_data.driver == HCT_CCP_DRV_MOD_HCT) {
        if (state->vdev.fd) {
            qemu_close(state->vdev.fd);
            state->vdev.fd = 0;
        }
    }

    if (hct_data.hct_shm_fd) {
        close(hct_data.hct_shm_fd);
        hct_data.hct_shm_fd = 0;
    }
    if (hct_data.pasid) {
        hct_data.pasid = 0;
    }

    if (g_id) {
        hct_g_ids_free(g_hct_gid_bitmap, g_id);
        g_id = 0;
    }

    if (hct_data.hct_shared_memory) {
        munmap((void *)hct_data.hct_shared_memory, hct_data.hct_shared_size);
        hct_data.hct_shared_memory = NULL;
    }

    precopy_remove_notifier(&state->precopy_notifier);

    hct_data.init = 0;
    hct_data.driver = HCT_CCP_DRV_MOD_UNINIT;
}

static int hct_data_init(HCTDevState *state)
{

    const char *hct_shr_name = NULL;
    int ret = 0;

    if (hct_data.init == 0) {
        /*
         * Check driver type based on parameters.
         * sysfsdev: mdev mode (hct or ccp)
         * dev(ccp_dev_path): vfio-pci or ccp mode (via vccp files)
         */
        if (state->ccp_dev_path) {
            FILE *fp = fopen(state->ccp_dev_path, "r");
            if (fp) {
                int type_char = fgetc(fp);
                fclose(fp);
                if (type_char == 'v') {
                    hct_data.driver = HCT_CCP_DRV_MOD_VFIO_PCI;
                } else if (type_char == 'c') {
                    hct_data.driver = HCT_CCP_DRV_MOD_CCP;
                } else {
                    error_report("hct: invalid vccp file content in %s", state->ccp_dev_path);
                    return -EINVAL;
                }
            } else {
                error_report("hct: cannot open vccp file %s", state->ccp_dev_path);
                return -EIO;
            }
         } else {
             /* Default to legacy mdev mode check if no params given */
             ret = hct_get_used_driver_walk(PCI_DRV_HCT_DIR);
             if (ret == 0) {
                 hct_data.driver = HCT_CCP_DRV_MOD_HCT;
                 hct_shr_name = HCT_SHARE_DEV;
                 hct_data.hct_fd = qemu_open_old(hct_shr_name, O_RDWR);
                 if (hct_data.hct_fd < 0) {
                     error_report("fail to open %s, errno %d.", hct_shr_name, errno);
                     goto out;
                 }

                 /* The hct.ko version number needs not to be less than 0.2. */
                 ret = hct_api_version_check();
                 if (ret) {
                     goto out;
                 }
                 /* close fd for ioctl in kernel module and open the real share memory file below. */
                 qemu_close(hct_data.hct_fd);

            } else {
                /* This case is now handled by the unified logic below, assuming vccp path */
                error_report("hct: sysfsdev is only supported hct driver mode.");
            }
         }
    }
    if (hct_data.driver == HCT_CCP_DRV_MOD_VFIO_PCI || hct_data.driver == HCT_CCP_DRV_MOD_CCP) {
        /* host running vfio-pci/ccp mode: get device information from daemon via vccp file */
        ret = vfio_hct_init_from_daemon(state);
        if (ret < 0) {
            error_report("Failed to initialize device info %d", ret);
            return ret;
        }
    }
    if (hct_data.init == 0) {
        /* assign a page to the virtual BAR3 of each CCP. */
        ret = hct_shared_memory_init();
        if (ret)
            goto out;

        /* Open fd for ioctl */
        if (hct_data.driver == HCT_CCP_DRV_MOD_HCT) {
            hct_data.hct_fd = qemu_open_old(HCT_SHARE_DEV, O_RDWR);
            if (hct_data.hct_fd < 0) {
                error_report("fail to open %s, errno %d.", HCT_SHARE_DEV, errno);
                goto unmap_shared_memory_exit;
            }
        } else if (hct_data.driver == HCT_CCP_DRV_MOD_CCP) {
            hct_data.hct_fd = qemu_open_old(CCP_SHARE_DEV, O_RDWR);
            if (hct_data.hct_fd < 0) {
                error_report("fail to open %s, errno %d.", CCP_SHARE_DEV, errno);
                goto unmap_shared_memory_exit;
            }
        }

        /* assign a unique pasid to each virtual machine. */
        ret = pasid_get_and_init(state);
        if (ret < 0)
            goto unmap_shared_memory_exit;

        ret = hct_get_vsock_guest_cid(state);
        if (ret < 0)
            error_report("get the guest_cid of vsock device fail.");

        state->precopy_notifier.notify = hct_migrate_precopy_notifier;
        precopy_add_notifier(&state->precopy_notifier);
        state->migrate_load_timer = NULL;
        state->migrate_support = 1;

        hct_data.init = 1;
    }

    return hct_get_ccp_index(state);

unmap_shared_memory_exit:
    munmap((void *)hct_data.hct_shared_memory, hct_data.hct_shared_size);

out:
    return ret;
}

#define VFIO_GET_REGION_ADDR(x) ((uint64_t)(x) << 40ULL)

/* @brief set bus master to avoid ccp stuck in vfio-pci mode */
static int pci_vfio_set_bus_master(int dev_fd)
{
    uint16_t reg = 0;
    int ret = 0;

    ret = pread(dev_fd, &reg, sizeof(reg),
            VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
            PCI_COMMAND);
    if (ret != sizeof(reg)) {
        error_report("Cannot read command from PCI config space!\n");
        return -1;
    }

    reg |= PCI_COMMAND_MASTER;

    ret = pwrite(dev_fd, &reg, sizeof(reg),
            VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
            PCI_COMMAND);

    if (ret != sizeof(reg)) {
        error_report("Cannot write command to PCI config space!\n");
        return -1;
    }

    return 0;
}

/* When device is loaded */
static void vfio_hct_realize(PCIDevice *pci_dev, Error **errp)
{
    int ret = 0;
    char *mdevid;
    Error *err = NULL;
    HCTDevState *state = PCI_HCT_DEV(pci_dev);

    state->lock_fd = -1;

    /*
     * In vfio-pci/ccp mode, lock this device's vccp file to prevent
     * multiple QEMU instances from using the same vccp group.
     */
    if (state->ccp_dev_path && !state->vdev.sysfsdev) {
        state->lock_fd = open(state->ccp_dev_path, O_RDONLY | O_CLOEXEC);
        if (state->lock_fd < 0) {
            error_setg(errp, "hct: cannot open vccp file %s: %s",
                      state->ccp_dev_path, strerror(errno));
            return;
        }
        if (flock(state->lock_fd, LOCK_EX | LOCK_NB) != 0) {
            error_setg(errp, "hct: cannot lock vccp file %s, another QEMU may be using this vccp group.",
                      state->ccp_dev_path);
            close(state->lock_fd);
            state->lock_fd = -1;
            return;
        }
    }

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
    } else if(hct_data.driver == HCT_CCP_DRV_MOD_VFIO_PCI) {
        pci_vfio_set_bus_master(state->vdev.fd);
    }

    ret = hct_listener_dma_memory_init(state);
    if (ret < 0) {
        error_setg(errp, "hct listener alloc dma memory fail.");
        goto detach_device_out;
    }

    ret = vfio_hct_region_mmap(state);
    if (ret < 0) {
        error_setg(errp, "hct vfio region mmap failed.");
        goto put_dma_mem_out;
    }

    return;

put_dma_mem_out:
    if (state->sdev.share_memory)
        munmap(state->sdev.share_memory, state->sdev.share_memory_size);
    if (state->sdev.dma_memory)
        munmap(state->sdev.dma_memory, state->sdev.dma_memory_size);
    if (state->sdev.premap_memory)
        munmap(state->sdev.premap_memory, state->sdev.premap_memory_size);

detach_device_out:
    if (hct_data.driver == HCT_CCP_DRV_MOD_HCT)
        vfio_hct_detach_device(state);

data_uninit_out:
    hct_data_uninit(state);

out:
    return;
}

static const VMStateDescription vfio_hct_vmstate = {
    .name = "vfio-hct-dev",
    .version_id = HCT_MIGRATE_VERSION,
    .minimum_version_id = HCT_MIGRATE_VERSION,
    .post_load = hct_dev_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(migrate_support, HCTDevState),
        VMSTATE_END_OF_LIST()
    }
};

static void hct_dev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pdc = PCI_DEVICE_CLASS(klass);

    dc->desc = "HCT Device";
    dc->vmsd = &vfio_hct_vmstate;
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

/* @brief vfio-pci mode DMA mapping function */
static int vfio_hct_dma_map_vfio_pci(int container_fd, void *vaddr,  uint64_t iova, uint64_t size)
{
    struct vfio_iommu_type1_dma_map dma_map = { 0 };
    int ret = 0;

    if (container_fd < 0) {
        error_report("Invalid container fd for vfio-pci mapping");
        return -1;
    }

    if (!vaddr || !size) {
        error_report("Invalid parameters for vfio-pci mapping");
        return -1;
    }

    dma_map.argsz = sizeof(dma_map);
    dma_map.vaddr = (uint64_t)vaddr;
    dma_map.size = size;
    dma_map.iova = (uint64_t)iova;
    dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

    ret = ioctl(container_fd, VFIO_IOMMU_MAP_DMA, &dma_map);
    if (ret) {
        if (errno == EEXIST) {
            error_report("Memory segment is already mapped in vfio-pci mode, container_fd=%d, iova=%lx, size=%lx", container_fd, iova, size);
            ret = 0;
        } else {
            error_report("Cannot set up DMA remapping in vfio-pci mode, error %i (%s)",
                        errno, strerror(errno));
        }
    }

    return ret;
}

static int vfio_hct_dma_unmap_vfio_pci(int container_fd, uint64_t iova, uint64_t size)
{
    struct vfio_iommu_type1_dma_unmap dma_unmap = { 0 };
    int ret = 0;

    if (container_fd < 0) {
        error_report("Invalid container fd for vfio-pci unmapping");
        return -1;
    }

    if (!iova || !size) {
        error_report("Invalid parameters for vfio-pci unmapping, iova %lu, size %lu\n", iova, size);
        return -1;
    }

    dma_unmap.argsz = sizeof(dma_unmap);
    dma_unmap.size = size;
    dma_unmap.iova = (uint64_t)iova;

    ret = ioctl(container_fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
    if (ret < 0) {
        error_report("Cannot unmap DMA in vfio-pci mode, error %i (%s)",
                    errno, strerror(errno));
    }

    return ret;
}

/* @brief get vfio file descriptor from daemon */
static int vfio_hct_init_from_daemon(HCTDevState *state)
{
    hct_client_info_t client_info;
    hct_vccp_req req_data;
    hct_ccp_device_t *device_info = NULL;
    hct_group_info_t *group_info = NULL;
    char vccp_content[256] = {0};
    char bdf[PCI_ADDR_MAX] = {0};
    FILE *fp = NULL;
    int ret = 0;
    char type_char = 'c';

    fp = fopen(state->ccp_dev_path, "r");
    if (!fp) {
        error_report("Failed to open vccp file %s: %s", state->ccp_dev_path, strerror(errno));
        return -EINVAL;
    }

    if (fgets(vccp_content, sizeof(vccp_content), fp) == NULL) {
        error_report("Failed to read content from vccp file %s", state->ccp_dev_path);
        fclose(fp);
        return -EINVAL;
    }
    fclose(fp);

    vccp_content[strcspn(vccp_content, "\n")] = '\0';

    if (sscanf(vccp_content, "%c", &type_char) != 1) {
        error_report("Invalid vccp file format %s", state->ccp_dev_path);
        return -EINVAL;
    }

    if (type_char == 'v') {
        if (sscanf(vccp_content, "v %*d %*d %15s", bdf) != 1) {
            error_report("Invalid vfio-pci vccp file %s", state->ccp_dev_path);
            return -EINVAL;
        }
    }

    req_data.path = state->ccp_dev_path;
    req_data.content = vccp_content;
    ret = hct_client_send_cmd(HCT_DAEMON_SOCK_PATH, &client_info, HCT_CMD_GET_DEVICE_BY_NAME, &req_data);
    if (ret != HCT_SUCCESS) {
        error_report("Failed to send command: %s", hct_get_error_string(ret));
        return ret;
    }

    /* find specified CCP device */
    if (hct_data.driver == HCT_CCP_DRV_MOD_CCP) {
        if (client_info.device_count != 1 || !client_info.devices) {
            error_report("CCP mode: Expected 1 device, but received %d",
                         client_info.device_count);
            hct_client_cleanup(&client_info);
            return -ENODEV;
        }
        device_info = &client_info.devices[0];

        state->vdev.fd = device_info->device_fd;
    } else { /* VFIO-PCI mode */
        device_info = hct_find_device_by_pci_addr(&client_info, bdf);
        if (!device_info) {
            error_report("Device %s not found", bdf);
            hct_client_cleanup(&client_info);
            return -ENODEV;
        }
        /* get corresponding group information */
        group_info = &client_info.groups[device_info->group_index];

        /* use returned file descriptor */
        state->container_fd = client_info.container_fd;
        state->group_fd = group_info->group_fd;
        state->vdev.fd = device_info->device_fd;
        state->group_id = group_info->group_id;
        hct_data.vfio_container_fd = state->container_fd;
    }


    /* note: do not call hct_client_cleanup, because we need to keep FD open */
    /* only clean up dynamic allocated memory, do not close FD */
    if (client_info.groups) {
        free(client_info.groups);
    }
    if (client_info.devices) {
        free(client_info.devices);
    }

    return 0;
}

/**
 * @brief Parse single TLV record
 * @param buffer The buffer to parse
 * @param buffer_len The length of the buffer
 * @param offset The offset to parse
 * @param tlv The TLV to parse
 * @return 0 on success, -1 on failure
 */
static int hct_parse_tlv(const char *buffer, size_t buffer_len, size_t *offset, hct_tlv_t *tlv)
{
    if (*offset + sizeof(uint16_t) * 2 > buffer_len) {
        return -1;
    }

    memcpy(&tlv->type, buffer + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);

    memcpy(&tlv->length, buffer + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);

    if (*offset + tlv->length > buffer_len) {
        return -1;
    }

    if (tlv->length > 0) {
        tlv->value = malloc(tlv->length);
        if (!tlv->value) {
            return -1;
        }
        memcpy(tlv->value, buffer + *offset, tlv->length);
        *offset += tlv->length;
    } else {
        tlv->value = NULL;
    }

    return 0;
}

/**
 * @brief Add a TLV to the buffer
 * @param buffer The buffer to add the TLV to
 * @param current_len The current length of the buffer
 * @param max_len The maximum length of the buffer
 * @param type The type of the TLV
 * @param value The value of the TLV
 * @param length The length of the TLV
 * @return 0 on success, -1 on failure
 */
static int hct_add_tlv_to_buffer(char *buffer, size_t *current_len, size_t max_len, uint16_t type, const void *value, uint16_t length)
{
    if (*current_len + sizeof(type) + sizeof(length) + length > max_len) {
        error_report("Buffer overflow in TLV add\n");
        return -1;
    }

    memcpy(buffer + *current_len, &type, sizeof(type));
    *current_len += sizeof(type);

    memcpy(buffer + *current_len, &length, sizeof(length));
    *current_len += sizeof(length);

    if (length > 0 && value) {
        memcpy(buffer + *current_len, value, length);
        *current_len += length;
    }

    return 0;
}

/**
 * @brief Send command to daemon
 * @param sock The socket to send to
 * @param cmd The command to send
 * @param device_names Array of device names (for HCT_CMD_GET_DEVICE_BY_NAME)
 * @param device_count Number of device names
 * @return 0 on success, -1 on failure
 */
static int hct_send_command(int sock, enum hct_daemon_req_cmd cmd, void *req_data)
{
    char buffer[2048] = {0};
    size_t buffer_len = 0;
    struct hct_vccp_req *vccp_req = NULL;

    /* add command TLV */
    if (hct_add_tlv_to_buffer(buffer, &buffer_len, sizeof(buffer),
                              HCT_IPC_FIELD_COMMAND, &cmd, sizeof(cmd)) < 0) {
        error_report("Failed to add command TLV\n");
        return -1;
    }

    /* add device names TLV if present */
    if (cmd == HCT_CMD_GET_DEVICE_BY_NAME) {
        vccp_req = req_data;
        if (hct_add_tlv_to_buffer(buffer, &buffer_len, sizeof(buffer),
                                  HCT_IPC_FIELD_VCCP_PATH, vccp_req->path,
                                  strlen(vccp_req->path) + 1) < 0) {
            error_report("Failed to add vccp path TLV\n");
            return -1;
        }
        if (hct_add_tlv_to_buffer(buffer, &buffer_len, sizeof(buffer),
                                  HCT_IPC_FIELD_VCCP_CONTENT, vccp_req->content,
                                  strlen(vccp_req->content) + 1) < 0) {
            error_report("Failed to add vccp content TLV\n");
            return -1;
        }
    }

    /* send command */
    if (send(sock, buffer, buffer_len, 0) < 0) {
        error_report("Failed to send command: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * @brief Send command request to HCT daemon and get response
 * @param socket_path The path to the socket
 * @param client_info The client information
 * @param cmd The command to send
 * @param device_names Array of device names (for HCT_CMD_GET_DEVICE_BY_NAME)
 * @param device_count Number of device names
 * @return 0 on success, -1 on failure
 */
static int hct_client_send_cmd(const char *socket_path, hct_client_info_t *client_info,
                               enum hct_daemon_req_cmd cmd, void *req_data)
{
    char cmsgbuf[CMSG_SPACE(sizeof(int) * MAX_FD_COUNT)] = {0};
    char buffer[MAX_TLV_BUFFER_SIZE];
    char error_reason[256] = {0};
    int fds[MAX_FD_COUNT] = {0};
    struct sockaddr_un addr;
    struct msghdr msg;
    struct iovec iov;
    hct_tlv_t tlv;
    struct cmsghdr *cmsg = NULL;
    int *fdptr = NULL;
    ssize_t received = 0;
    size_t offset = 0;
    size_t fd_size = 0;
    int current_group_index = 0;
    int fd_index = 0;
    int sock = 0;
    int has_pending_group_info = 0;
    int has_pending_device_info = 0;
    int device_index = 0;

    /* Temporary variables to hold info before FD */
    struct {
        int group_id;
        int device_count;
    } pending_group_info = {0};
    struct {
        char pci_addr[16];
        int group_id;
    } pending_device_info = {0};


    if (!socket_path || !client_info) {
        return HCT_ERROR_INVALID_DATA;
    }

    memset(client_info, 0, sizeof(hct_client_info_t));
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        error_report("Failed to create socket: %s\n", strerror(errno));
        return HCT_ERROR_CONNECT;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        error_report("Failed to connect to %s: %s\n", socket_path, strerror(errno));
        close(sock);
        return HCT_ERROR_CONNECT;
    }

    if (hct_send_command(sock, cmd, req_data) < 0) {
        error_report("[HCT] Failed to send command");
        close(sock);
        return HCT_ERROR_CONNECT;
    }

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = buffer;
    iov.iov_len = sizeof(buffer);

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    received = recvmsg(sock, &msg, 0);
    if (received <= 0) {
        error_report("Failed to receive message: %s\n", strerror(errno));
        close(sock);
        return HCT_ERROR_RECEIVE;
    }

    /* Extract file descriptors from control message */
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            fd_size = cmsg->cmsg_len - CMSG_LEN(0);
            fdptr = (int *)CMSG_DATA(cmsg);
            memcpy(fds, fdptr, fd_size);
        }
    }

    offset = 0;
    fd_index = 0;
    current_group_index = -1;

    while (offset < received) {
        memset(&tlv, 0, sizeof(tlv));
        if (hct_parse_tlv(buffer, received, &offset, &tlv) < 0) {
            error_report("[HCT] Failed to parse TLV data");
            hct_client_cleanup(client_info);
            close(sock);
            return HCT_ERROR_INVALID_DATA;
        }

        /* Process different TLV types according to order */
        switch (tlv.type) {
            case HCT_IPC_FIELD_CONTAINER_FD:
                client_info->container_fd = fds[fd_index];
                fd_index++;
                break;

            case HCT_IPC_FIELD_GROUP_INFO:
                /* Store group info for next group FD */
                if (tlv.length == sizeof(pending_group_info)) {
                    memcpy(&pending_group_info, tlv.value, sizeof(pending_group_info));
                    has_pending_group_info = 1;
                } else {
                    error_report("Invalid group info size\n");
                }
                break;

            case HCT_IPC_FIELD_GROUP_FD:
                if (!has_pending_group_info) {
                    error_report("Received group FD without group info");
                    break;
                }

                /* New group detected, allocate space */
                current_group_index++;
                client_info->group_count = current_group_index + 1;

                if (!client_info->groups) {
                    client_info->groups = malloc(sizeof(hct_group_info_t) * MAX_CCP_CNT);
                    if (!client_info->groups) {
                        error_report("Failed to allocate memory for groups");
                        hct_client_cleanup(client_info);
                        close(sock);
                        return HCT_ERROR_INVALID_DATA;
                    }
                }

                /* Initialize new group with real group_id */
                memset(&client_info->groups[current_group_index], 0, sizeof(hct_group_info_t));
                client_info->groups[current_group_index].group_id = pending_group_info.group_id;
                client_info->groups[current_group_index].group_fd = fds[fd_index];
                client_info->groups[current_group_index].device_count = 0;

                fd_index++;
                has_pending_group_info = 0; /* Clear pending info */
                break;

            case HCT_IPC_FIELD_DEVICE_INFO:
                /* Store device info for next device FD */
                if (tlv.length == sizeof(pending_device_info)) {
                    memcpy(&pending_device_info, tlv.value, sizeof(pending_device_info));
                    has_pending_device_info = 1;
                } else {
                    error_report("Invalid device info size\n");
                }
                break;

            case HCT_IPC_FIELD_DEVICE_FD:
                /* In CCP mode, we only receive DEVICE_FD, no preceding INFO TLVs */
                if (hct_data.driver == HCT_CCP_DRV_MOD_VFIO_PCI) {
                    if (!has_pending_device_info) {
                        error_report("VFIO mode: Received device FD without device info");
                        break;
                    }
                    if (current_group_index < 0) {
                        error_report("Received device FD without group context");
                        break;
                    }
                }

                if (!client_info->devices) {
                    client_info->devices = malloc(sizeof(hct_ccp_device_t) * MAX_CCP_CNT);
                    if (!client_info->devices) {
                        error_report("Failed to allocate memory for devices");
                        hct_client_cleanup(client_info);
                        close(sock);
                        return HCT_ERROR_INVALID_DATA;
                    }
                }

                client_info->device_count++;
                /* Initialize new device with real pci_addr */
                device_index = client_info->device_count - 1;
                memset(&client_info->devices[device_index], 0, sizeof(hct_ccp_device_t));
                client_info->devices[device_index].device_fd = fds[fd_index];

                if (hct_data.driver != HCT_CCP_DRV_MOD_CCP) {
                    client_info->devices[device_index].group_index = current_group_index;

                    /* Use real pci_addr from device info */
                    strncpy(client_info->devices[device_index].pci_addr,
                            pending_device_info.pci_addr,
                            sizeof(client_info->devices[device_index].pci_addr) - 1);
                    client_info->devices[device_index].pci_addr[
                        sizeof(client_info->devices[device_index].pci_addr) - 1] = '\0';

                    /* Update group device count */
                    client_info->groups[current_group_index].device_count++;
                }

                fd_index++;
                has_pending_device_info = 0; /* Clear pending info */
                break;
            case HCT_IPC_FIELD_ERROR_REASON:
                error_report("IPC return error");
                break;

            default:
                error_report("Unknown TLV type: %d", tlv.type);
                break;
        }

        if (tlv.value) {
            free(tlv.value);
        }
    }

    if (error_reason[0] != '\0') {
        error_report("Rejected request");
        hct_client_cleanup(client_info);
        close(sock);
        return HCT_ERROR_INVALID_DATA;
    }

    close(sock);

    return HCT_SUCCESS;
}

/**
 * @brief Cleanup HCT client resources
 * @param client_info The client information
 */
static void hct_client_cleanup(hct_client_info_t *client_info)
{
    if (!client_info) {
        return;
    }

    if (client_info->devices) {
        free(client_info->devices);
        client_info->devices = NULL;
    }

    if (client_info->groups) {
        free(client_info->groups);
        client_info->groups = NULL;
    }

    if (client_info->container_fd >= 0) {
        client_info->container_fd = -1;
    }

    client_info->device_count = 0;
    client_info->group_count = 0;
}

/**
 * @brief Get error description string
 * @param error_code The error code
 * @return The error description string
 */
static const char *hct_get_error_string(int error_code)
{
    switch (error_code) {
        case HCT_SUCCESS:
            return "Success";
        case HCT_ERROR_CONNECT:
            return "Failed to connect";
        case HCT_ERROR_RECEIVE:
            return "Failed to receive data";
        case HCT_ERROR_INVALID_DATA:
            return "Invalid data received";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Find device by PCI address
 * @param client_info The client information
 * @param pci_addr The PCI address of the device
 * @return The device information
 */
static hct_ccp_device_t* hct_find_device_by_pci_addr(hct_client_info_t *client_info, const char *pci_addr)
{
    int i = 0;

    if (!client_info || !pci_addr || !client_info->devices) {
        return NULL;
    }

    for (i = 0; i < client_info->device_count; i++) {
        if (strcmp(client_info->devices[i].pci_addr, pci_addr) == 0) {
            return &client_info->devices[i];
        }
    }

    return NULL;
}

/**
 * @brief Allocate a global bitmap instance for g_id management
 * @return The allocated bitmap instance, or NULL on failure
 */
static struct hct_gid_bitmap* hct_gid_bitmap_alloc(void)
{
    struct hct_gid_bitmap *gid_bitmap = NULL;
    mode_t oldmod = 0;
    size_t total_size = 0;
    int shm_fd = -1;
    int lock_fd = -1;

    gid_bitmap = (struct hct_gid_bitmap *)calloc(1, sizeof(struct hct_gid_bitmap));
    if (!gid_bitmap) {
        error_report("Failed to allocate hct_gid_bitmap structure\n");
        return NULL;
    }

    strncpy(gid_bitmap->name, HCT_GID_BITMAP_SHM_NAME, MAX_PATH - 1);
    gid_bitmap->name[MAX_PATH - 1] = '\0';
    gid_bitmap->shm_fd = -1;
    gid_bitmap->lock_fd = -1;

    total_size = HCT_BITMAP_SIZE(HCT_QEMU_GIDS_BITMAP_MAX_BIT) * sizeof(unsigned long);
    gid_bitmap->len = total_size;

    oldmod = umask(0);
    shm_fd = shm_open(HCT_GID_BITMAP_SHM_NAME, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0666);
    umask(oldmod);

    if (shm_fd < 0 && errno == EEXIST) {
        // Shared memory already exists, try to open it
        shm_fd = shm_open(HCT_GID_BITMAP_SHM_NAME, O_RDWR | O_CLOEXEC, 0666);
    }

    if (shm_fd < 0) {
        error_report("Failed to create/open shared memory %s, errno %d\n", HCT_GID_BITMAP_SHM_NAME, errno);
        goto cleanup;
    }

    gid_bitmap->shm_fd = shm_fd;
    if (ftruncate(shm_fd, total_size) != 0) {
        error_report("Failed to set shared memory size, errno %d\n", errno);
        goto cleanup;
    }

    gid_bitmap->bitmap = (unsigned long *)mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                MAP_SHARED, shm_fd, 0);
    if (gid_bitmap->bitmap == MAP_FAILED) {
        error_report("Failed to mmap shared memory, errno %d\n", errno);
        goto cleanup;
    }

    lock_fd = shm_open(HCT_GID_LOCK_FILE, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (lock_fd < 0) {
        if (errno == EEXIST) {
            lock_fd = shm_open(HCT_GID_LOCK_FILE, O_RDWR, 0);
            if (lock_fd == -1) {
                error_report("Failed to shm_open lock file %s, errno %d\n", HCT_GID_LOCK_FILE, errno);
                goto cleanup;
            }
        } else {
            error_report("Failed to shm_open lock file %s, errno %d\n", HCT_GID_LOCK_FILE, errno);
            goto cleanup;
        }
    } else {
        if (ftruncate(lock_fd, HCT_QEMU_GIDS_BITMAP_MAX_BIT * 8) != 0) {
            error_report("Failed to ftruncate lock shm %s, errno %d\n", HCT_GID_LOCK_FILE, errno);
            goto cleanup;
        }
        if (fchmod(lock_fd, 0666) == -1) {
            error_report("fchmod failed\n");
        }
    }

    gid_bitmap->lock_fd = lock_fd;
    g_hct_gid_bitmap = gid_bitmap;

    return gid_bitmap;

cleanup:
    hct_gid_bitmap_free(gid_bitmap);
    return NULL;
}

/**
 * @brief Free a global bitmap instance for g_id management
 * @param bitmap The bitmap instance to free
 */
static void hct_gid_bitmap_free(struct hct_gid_bitmap *bitmap)
{
    if (!bitmap)
        return;

    if (bitmap->bitmap && bitmap->bitmap != MAP_FAILED) {
        munmap(bitmap->bitmap, bitmap->len);
    }

    if (bitmap->shm_fd >= 0) {
        close(bitmap->shm_fd);
    }

    if (bitmap->lock_fd >= 0) {
        close(bitmap->lock_fd);
    }

    if (g_hct_gid_bitmap == bitmap) {
        g_hct_gid_bitmap = NULL;
    }

    free(bitmap);
}

/**
 * @brief Allocate a g_id from the 1024-bit bitmap, left-shift by 8 bits
 * @param bitmap The bitmap instance to allocate from
 * @return The allocated g_id, or -1 if no g_id is available
 */
static inline int _hct_gid_bitmap_try_alloc(struct hct_gid_bitmap *bitmap) {
    unsigned long bit_pos = 0;
    unsigned long old_val = 0, new_val = 0;
    volatile unsigned long *word_ptr = NULL;
    unsigned long mask = 0;

    for (bit_pos = HCT_QEMU_GIDS_BITMAP_MIN_BIT; bit_pos < HCT_QEMU_GIDS_BITMAP_MAX_BIT; bit_pos++) {
        if (!hct_get_bit(bitmap->bitmap, bit_pos)) {
            word_ptr = &bitmap->bitmap[WORD_OFFSET(bit_pos)];
            mask = 1UL << BIT_OFFSET(bit_pos);
            old_val = *word_ptr;
            if (!(old_val & mask)) {
                new_val = old_val | mask;
                if (__atomic_compare_exchange_n(word_ptr, &old_val, new_val,
                    false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                    return bit_pos;
                }
            }
        }
    }
    return -1;
}

/**
 * @brief Allocate a g_id from the 1024-bit bitmap, left-shift by 8 bits
 * @param bitmap The bitmap instance to allocate from
 * @param gid The allocated g_id
 * @return 0 on success, -1 if no g_id is available
 */
static int hct_g_ids_alloc(struct hct_gid_bitmap *bitmap, unsigned long *gid)
{
    unsigned long id = 0;
    int bit_pos = -1;
    int try_count = 0;
    int max_try = 2;
    int lock_ret = 0;

    if (!bitmap || !bitmap->bitmap || !gid) {
        error_report("Invalid parameters\n");
        return -EINVAL;
    }

    while (try_count < max_try) {
        bit_pos = _hct_gid_bitmap_try_alloc(bitmap);
        if (bit_pos >= 0) {
            id = bit_pos + 1 - 1024; // g_id can't be 0
            *gid = id << HCT_QEMU_GIDS_SHIFT_BITS;
            lock_ret = hct_g_ids_lock_state_lock(bitmap, *gid);
            if (lock_ret == 0) {
                return 0;
            } else {
                continue;
            }
        } else if (try_count == 0) {
            // try to clean up orphaned g_ids
            error_report("try to clean up orphaned g_ids\n");
            hct_g_ids_lock_state_walk(bitmap);
        }
        try_count++;
    }

    error_report("No available g_id in bitmap or all locks busy after cleanup\n");
    return -EINVAL;
}

/**
 * @brief Free a g_id from the 1024-bit bitmap, left-shift by 8 bits
 * @param bitmap The bitmap instance to free from
 * @param gid The g_id to free
 */
static void hct_g_ids_free(struct hct_gid_bitmap *bitmap, unsigned long gid)
{
    unsigned long id = 0;
    unsigned long bit_pos = 0;
    off_t offset = 0;
    struct flock lock;

    if (!bitmap || !bitmap->bitmap) {
        error_report("Invalid bitmap parameters\n");
        return;
    }

    if (gid == 0) {
        error_report("Invalid g_id=0\n");
        return;
    }

    // Extract original id from g_id
    id = gid >> HCT_QEMU_GIDS_SHIFT_BITS;
    if (id == 0 || id > HCT_QEMU_GIDS_BITMAP_MAX_BIT) {
        error_report("Invalid g_id=0x%lx, extracted id=%lu\n", gid, id);
        return;
    }

    bit_pos = id - 1 + 1024;  // Convert back to bit position

    offset = bit_pos * HCT_GIDS_PER_BLOCK;
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = HCT_GIDS_PER_BLOCK;
    if (bitmap->lock_fd >= 0)
        fcntl(bitmap->lock_fd, F_SETLK, &lock);

    hct_clear_bit(bitmap->bitmap, bit_pos);
}

/**
 * @brief Lock a g_id
 * @param bitmap The bitmap instance to lock
 * @param gid The g_id to lock
 * @return 0 on success, -1 if the g_id is not locked
 */
static int hct_g_ids_lock_state_lock(struct hct_gid_bitmap *bitmap, unsigned long gid)
{
    struct flock lock;
    unsigned long id = 0;
    off_t offset = 0;
    int ret = 0;

    if (!bitmap || !bitmap->bitmap || bitmap->lock_fd < 0) {
        error_report("Invalid bitmap\n");
        return -EINVAL;
    }

    if (gid == 0) {
        error_report("Invalid g_id=0\n");
        return -EINVAL;
    }

    // Extract original id from g_id
    id = gid >> HCT_QEMU_GIDS_SHIFT_BITS;
    if (id == 0 || id > HCT_QEMU_GIDS_BITMAP_MAX_BIT) {
        error_report("Invalid g_id=0x%lx, extracted id=%lu\n", gid, id);
        return -EINVAL;
    }

    // Calculate file offset for this g_id's lock region
    offset = (id + 1024 - 1) * HCT_GIDS_PER_BLOCK;

    // Set up exclusive lock
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = HCT_GIDS_PER_BLOCK;

    // Try to acquire lock (non-blocking)
    if (fcntl(bitmap->lock_fd, F_SETLK, &lock) == 0) {
        ret = 0;
    } else {
        error_report("Failed to lock g_id=0x%lx, offset%lx, errno=%d\n", gid, offset, errno);
        ret = -EINVAL;
    }

    return ret;
}

/**
 * @brief Clean up queue states for a specific gid
 * @param gid: the gid to clean up
 */
static void hct_ccp_gid_queue_status_cleanup(unsigned int gid)
{
    uint32_t *ccp_queue_state = NULL;
    int i = 0;
    int j = 0;

    if (!hct_data.hct_shared_memory) {
        error_report("hct_data.hct_shared_memory is NULL, please check.\n");
        return;
    }

    for (i = 0; i < MAX_CCP_CNT; i++) {
        ccp_queue_state = (uint32_t *)(hct_data.hct_shared_memory + i * PAGE_SIZE);
        for (j = 0; j < MAX_HW_QUEUES; j++) {
            if (!ccp_queue_state[j] || ccp_queue_state[j] == HCT_QUEUE_NEED_INIT)
                continue;
            if ((ccp_queue_state[j] & HCT_QEMU_GIDS_SHIFT_MASK) == gid)
                ccp_queue_state[j] = HCT_QUEUE_NEED_INIT;
        }
    }
}

/**
 * @brief Walk the bitmap to detect orphaned g_ids
 * @param bitmap The bitmap instance to walk
 */
static void hct_g_ids_lock_state_walk(struct hct_gid_bitmap *bitmap)
{
    struct flock lock;
    unsigned long bit_pos = 0;
    unsigned long gid = 0;
    off_t offset = 0;

    if (!bitmap || !bitmap->bitmap || bitmap->lock_fd < 0) {
        error_report("Invalid bitmap parameters\n");
        return;
    }

    // Walk through all allocated bits in bitmap
    for (bit_pos = HCT_QEMU_GIDS_BITMAP_MIN_BIT;
                bit_pos < HCT_QEMU_GIDS_BITMAP_MAX_BIT; bit_pos++) {
        if (!hct_get_bit(bitmap->bitmap, bit_pos))
            continue;

        gid = (bit_pos - HCT_QEMU_GIDS_BITMAP_MIN_BIT + 1) << HCT_QEMU_GIDS_SHIFT_BITS;
        offset = bit_pos * HCT_GIDS_PER_BLOCK;

        // Try to set exclusive lock for this g_id
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = offset;
        lock.l_len = HCT_GIDS_PER_BLOCK;
        if (fcntl(bitmap->lock_fd, F_SETLK, &lock) == -1)
            continue;

        hct_ccp_gid_queue_status_cleanup(gid);

        lock.l_type = F_UNLCK;
        fcntl(bitmap->lock_fd, F_SETLK, &lock);
        hct_clear_bit(bitmap->bitmap, bit_pos);
    }
}

static void hct_clear_bit(unsigned long *bitmap, int n)
{
    __atomic_fetch_and(&bitmap[WORD_OFFSET(n)], ~(1UL << BIT_OFFSET(n)), __ATOMIC_RELEASE);
}

static uint32_t hct_get_bit(unsigned long *bitmap, int n)
{
    return ((bitmap[WORD_OFFSET(n)] & (0x1UL << BIT_OFFSET(n))) != 0);
}
