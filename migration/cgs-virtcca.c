/*
 * QEMU add virtcca cvm live migration feature.
 * 
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2027. All rights reserved.
 *
 * Authors:
 *      Zhu Yifan <zhuyifan30@huawei.com>
 *      He JingXian <hejingxian@huawei.com>
 *      Pan HengChang <panhengchang@huawei.com>
 *      Liu Hao <liuhao365@h-partners.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 * 
 */

#include "qemu/osdep.h"
#include "qemu-file.h"
#include "cgs.h"
#include "target/arm/kvm_arm.h"
#include "migration/misc.h"
#include "qemu/error-report.h"
#include "hw/boards.h"

/* MBMD, gpa_list and 2 pages of mac_list */
#define MULTIFD_EXTRA_IOV_NUM 4

/* Bytes of the MBMD for memory page, calculated from the spec */
#define VIRTCCA_MBMD_MEM_BYTES 48

/* qemufile header mbmd msg */
#define KVM_VIRTCCA_MIG_MBMD_TYPE_IMMUTABLE_STATE   0
#define KVM_VIRTCCA_MIG_MBMD_TYPE_VCPU_STATE        2
#define KVM_VIRTCCA_MIG_MBMD_TYPE_MEMORY_STATE      16
#define KVM_VIRTCCA_MIG_MBMD_TYPE_EPOCH_TOKEN       32
#define KVM_VIRTCCA_MIG_MBMD_TYPE_ABORT_TOKEN       33

#define GPA_LIST_OP_EXPORT 1
#define GPA_LIST_OP_CANCEL 2
#define GPA_LIST_OP_CHECK_ZERO_PAGE 3

#define CVM_MIG_F_CONTINUE 0x1

#define VIRTCCA_SYSFS_MIG_CHECK_SRC  "/sys/kernel/tmm/migration/mig_check_src"
#define VIRTCCA_SYSFS_MIG_CHECK_DST  "/sys/kernel/tmm/migration/mig_check_dst"
#define UINT64_LEN  20
#define MB_SHIFT 20
#define SYSFS_RESULT_LEN 10

struct virtcca_bind_info {
    int16_t version;
    bool premig_done;
};

struct virtcca_dst_host_info {
    char dst_ip[16];
    uint16_t dst_port;
    uint8_t version;
};

typedef struct virtCCAMigHdr {
    uint16_t flags;
    uint16_t buf_list_num;
} virtCCAMigHdr;

typedef union GpaListEntry {
    uint64_t val;
    struct {
        uint64_t level : 2;
        uint64_t pending : 1;
        uint64_t reserved_0 : 4;
        uint64_t l2_map : 3;
#define GPA_LIST_ENTRY_MIG_TYPE_4KB 0
        uint64_t mig_type : 2;
        uint64_t gfn : 40;
/* every ipa operation flags including nop, export, cancel */
        uint64_t operation : 2;
        uint64_t reserved_1 : 2;
        uint64_t status : 5;
        uint64_t reserved_2 : 3;
    };
} GpaListEntry;


virtCCAMigState virtCCA_mig;

static int virtcca_mig_stream_ioctl(virtCCAMigStream *stream, int cmd_id,
                                    __u32 metadata, void *data)
{
    struct kvm_virtcca_mig_cmd cmd;
    int ret;

    memset(&cmd, 0x0, sizeof(cmd));

    cmd.id = cmd_id;
    cmd.flags = metadata;
    cmd.data = (__u64)(unsigned long)data;

    ret = kvm_device_ioctl(stream->fd, KVM_CVM_MIG_IOCTL, &cmd);
    if (ret) {
        error_report("Failed to send migration cmd %d to the driver: %s",
                     cmd_id, strerror(ret));
    }

    return ret;
}

static uint64_t virtcca_mig_put_mig_hdr(QEMUFile *f, uint64_t num, uint16_t flags)
{
    virtCCAMigHdr hdr = {
        .flags = flags,
        .buf_list_num = (uint16_t)num,
    };

    qemu_put_buffer(f, (uint8_t *)&hdr, sizeof(hdr));

    return sizeof(hdr);
}

static inline uint64_t virtcca_mig_stream_get_mbmd_bytes(virtCCAMigStream *stream)
{
    /*
     * The first 2 bytes in MBMD buffer tells the overall size of the mbmd
     */
    uint16_t bytes = *(uint16_t *)stream->mbmd;

    return (uint64_t)bytes;
}

static uint8_t virtcca_mig_stream_get_mbmd_type(virtCCAMigStream *stream)
{
    /* MB_TYPE at byte offset 6, virtcca temporarily reuse this structure */
    return *((uint8_t *)stream->mbmd + 6);
}

int append_number_to_string(char *result, size_t *current_len, uint64_t number, uint64_t node_mask)
{
    char buffer[UINT64_LEN + 1];
    size_t buffer_len = 0;

    if (!result || !current_len) {
        error_report("Invalid pointer");
        return -1;
    }

    int copy_buffer = snprintf(buffer, sizeof(buffer), "%lu-%lu", number, node_mask);
    if (copy_buffer < 0 || (size_t)copy_buffer >= sizeof(buffer)) {
        error_report("Message truncated or error formatting number: %lu-%lu", number, node_mask);
        return -1;
    }

    buffer_len = strlen(buffer);
    if (*current_len > 0) {
        result[*current_len] = ' ';
        *current_len += 1;
    }

    strcpy(result + *current_len, buffer);
    *current_len += buffer_len;
    return 0;
}

int write_numa_info_to_sysfs(struct kvm_numa_info *numa_info, bool is_src)
{
    const char *path = is_src ? VIRTCCA_SYSFS_MIG_CHECK_SRC : VIRTCCA_SYSFS_MIG_CHECK_DST;
    struct kvm_numa_node *numa_node;
    char *numa_str = NULL;
    size_t current_len = 0;
    size_t estimated_size = numa_info->numa_cnt * (UINT64_LEN + 1) * 2;
    int ret = 0;
    struct stat buffer;
    FILE *sysfs_file = NULL;

    if (stat(path, &buffer) != 0) {
        info_report("Unable to get tmm driver, skip check.");
        return 0;
    }

    numa_str = (char *)g_malloc0(estimated_size);
    for (int idx = 0; idx < numa_info->numa_cnt; idx++) {
        numa_node = &(numa_info->numa_nodes[idx]);
        if (append_number_to_string(numa_str, &current_len,
            (numa_node->ipa_size >> MB_SHIFT), numa_node->host_numa_nodes[0])) {
            ret = -EINVAL;
            goto out;
        }
    }

    sysfs_file = fopen(path, "w");
    if (!sysfs_file) {
        error_report("Failed to open sysfs file");
        ret = -EIO;
        goto out;
    }

    if (fprintf(sysfs_file, "%s\n", numa_str) < 0) {
        error_report("Failed to write to sysfs file");
        ret = -EIO;
        goto out;
    }

    ret = 0;
out:
    if (sysfs_file)
        fclose(sysfs_file);
    if (numa_str)
        free(numa_str);

    return ret;
}

int get_migration_result_from_sysfs(bool is_src)
{
    const char *path = is_src ? VIRTCCA_SYSFS_MIG_CHECK_SRC : VIRTCCA_SYSFS_MIG_CHECK_DST;
    int ret = 0;
    struct stat buffer;
    char buf[SYSFS_RESULT_LEN];
    FILE *sysfs_file = NULL;

    if (stat(path, &buffer) != 0) {
        info_report("Unable to get tmm driver, skip check.");
        return 0;
    }

    sysfs_file = fopen(path, "r");
    if (!sysfs_file) {
        error_report("Failed to open sysfs file");
        return -1;
    }

    if (fgets(buf, sizeof(buf), sysfs_file) == NULL) {
        error_report("Error reading sysfs file");
        ret = -1;
        goto out;
    }

    if (strncmp(buf, "1", 1) == 0) {
        ret = 1;
        info_report("Migration check succeeded");
    } else {
        ret = -1;
        info_report("Migration check failed");
    }

out:
    if (sysfs_file)
        fclose(sysfs_file);
    return ret;
}

int virtcca_check_mig_mem(bool is_src)
{
    MachineState *ms = (MachineState *)qdev_get_machine();
    struct kvm_numa_info *numa_info = NULL;
    int ret = 0;

    numa_info = g_malloc0(sizeof(struct kvm_numa_info));
    if (ms->numa_state != NULL && ms->numa_state->num_nodes > 0) {
        numa_info->numa_cnt = ms->numa_state->num_nodes;
        for (int64_t i = 0; i < ms->numa_state->num_nodes && i < MAX_NUMA_NODE; i++) {
            numa_info->numa_nodes[i].numa_id = i;
            numa_info->numa_nodes[i].ipa_size = ms->numa_state->nodes[i].node_mem;
            numa_info->numa_nodes[i].host_numa_nodes[0] = ms->numa_state->nodes[i].node_memdev->host_nodes[0];
        }
    } else {
        numa_info->numa_cnt = 1;
        numa_info->numa_nodes[0].numa_id = 0;
        numa_info->numa_nodes[0].ipa_size = ms->ram_size;
        memset(numa_info->numa_nodes[0].host_numa_nodes, 0, MAX_NODES / BITS_PER_LONG * sizeof(uint64_t));
    }

    ret = write_numa_info_to_sysfs(numa_info, is_src);
    if (ret < 0) {
        goto out;
    }
    ret = get_migration_result_from_sysfs(is_src);

out:
    if (numa_info)
        g_free(numa_info);
    return ret;
}

static int virtcca_mig_savevm_state_start(QEMUFile *f)
{
    info_report("Entering virtcca_mig_savevm_state_start");
    virtCCAMigStream *stream = &virtCCA_mig.streams[0];
    uint64_t mbmd_bytes, buf_list_bytes, exported_num = 0;
    int ret;

    /* Export mbmd and buf_list */
    ret = virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_EXPORT_STATE_IMMUTABLE,
                                   0, &exported_num);
    if (ret) {
        error_report("Failed to export immutable states: %s", strerror(ret));
        return ret;
    }

    mbmd_bytes = virtcca_mig_stream_get_mbmd_bytes(stream);
    buf_list_bytes = exported_num * TARGET_PAGE_SIZE;

    virtcca_mig_put_mig_hdr(f, exported_num, 0);
    qemu_put_buffer(f, (uint8_t *)stream->mbmd, mbmd_bytes);
    qemu_put_buffer(f, (uint8_t *)stream->buf_list, buf_list_bytes);

    return 0;
}

static long virtcca_mig_save_epoch(QEMUFile *f, bool in_order_done)
{
    virtCCAMigStream *stream = &virtCCA_mig.streams[0];
    uint64_t flags = in_order_done ? VIRTCCA_MIG_EXPORT_TRACK_F_IN_ORDER_DONE : 0;
    long virtcca_hdr_bytes, mbmd_bytes;
    int ret;

    ret = virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_EXPORT_TRACK, 0, &flags);
    if (ret) {
        return ret;
    }

    mbmd_bytes = virtcca_mig_stream_get_mbmd_bytes(stream);

    /* Epoch only has mbmd data */
    virtcca_hdr_bytes = virtcca_mig_put_mig_hdr(f, 0, 0);
    qemu_put_buffer(f, (uint8_t *)stream->mbmd, mbmd_bytes);

    return virtcca_hdr_bytes + mbmd_bytes;
}

static long virtcca_mig_savevm_state_ram_start_epoch(QEMUFile *f)
{
    return virtcca_mig_save_epoch(f, false);
}

static void virtcca_mig_gpa_list_setup(union GpaListEntry *gpa_list, hwaddr *gpa,
                                       uint64_t gpa_num, int operation)
{
    int i;

    for (i = 0; i < gpa_num; i++) {
        gpa_list[i].val = 0;
        gpa_list[i].gfn = gpa[i] >> TARGET_PAGE_BITS;
        gpa_list[i].mig_type = GPA_LIST_ENTRY_MIG_TYPE_4KB;
        gpa_list[i].operation = operation;
    }
}

bool virtcca_is_zero_page(uint32_t channel_id, hwaddr cgs_private_gpa, size_t len)
{
    int ret;
    bool is_zero_page;
    virtCCAMigStream *stream = &virtCCA_mig.streams[channel_id];

    virtcca_mig_gpa_list_setup((GpaListEntry *)stream->gpa_list,
                           &cgs_private_gpa, 1, GPA_LIST_OP_CHECK_ZERO_PAGE);

    ret = virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_IS_ZERO_PAGE, 0, &is_zero_page);
    if (ret) {
        error_report("%s: failed: failed to check zero page %d", __func__, ret);
        return false;
    }

    return is_zero_page;
}

int virtcca_import_zero_page(uint32_t channel_id, void *host)
{
    int ret = 0;
    virtCCAMigStream *stream = &virtCCA_mig.streams[channel_id];
    hwaddr cgs_private_gpa;

    ret = kvm_physical_memory_addr_from_host(kvm_state, host,
                                             &cgs_private_gpa);
    if (!ret) {
        return 0;
    }

    ret = virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_IMPORT_ZERO_PAGE, 0, (void *)cgs_private_gpa);
    if (ret) {
        error_report("%s: failed: failed to import zero page %d", __func__, ret);
        return -1;
    }

    return ret;
}

static long virtcca_mig_save_ram(QEMUFile *f, virtCCAMigStream *stream)
{
    uint64_t num = 1;
    uint64_t hdr_bytes, mbmd_bytes, gpa_list_bytes,
             buf_list_bytes, mac_list_bytes;
    int ret;

    /* Export mbmd, buf list, mac list and gpa list */
    ret = virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_EXPORT_MEM, 0, &num);
    if (ret) {
        return ret;
    }

    mbmd_bytes = virtcca_mig_stream_get_mbmd_bytes(stream);
    buf_list_bytes = TARGET_PAGE_SIZE;
    mac_list_bytes = sizeof(Int128);
    gpa_list_bytes = sizeof(GpaListEntry);

    hdr_bytes = virtcca_mig_put_mig_hdr(f, 1, 0);
    qemu_put_buffer(f, (uint8_t *)stream->mbmd, mbmd_bytes);
    qemu_put_buffer(f, (uint8_t *)stream->buf_list, buf_list_bytes);
    qemu_put_buffer(f, (uint8_t *)stream->gpa_list, gpa_list_bytes);
    qemu_put_buffer(f, (uint8_t *)stream->mac_list, mac_list_bytes);

    return hdr_bytes + mbmd_bytes + gpa_list_bytes +
           buf_list_bytes + mac_list_bytes;
}

static long virtcca_mig_savevm_state_ram(QEMUFile *f, uint32_t channel_id,
                                         hwaddr gpa)
{
    virtCCAMigStream *stream = &virtCCA_mig.streams[channel_id];

    virtcca_mig_gpa_list_setup((GpaListEntry *)stream->gpa_list,
                               &gpa, 1, GPA_LIST_OP_EXPORT);
    return virtcca_mig_save_ram(f, stream);
}

static int virtcca_mig_savevm_state_pause(void)
{
    virtCCAMigStream *stream = &virtCCA_mig.streams[0];

    return virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_EXPORT_PAUSE, 0, 0);
}

static int virtcca_mig_save_one_tec(QEMUFile *f, virtCCAMigStream *stream)
{
    uint64_t mbmd_bytes, buf_list_bytes, exported_num = 0;
    int ret;

    ret = virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_EXPORT_STATE_TEC, 0,
                               &exported_num);
    if (ret) {
        return ret;
    }

    mbmd_bytes = virtcca_mig_stream_get_mbmd_bytes(stream);
    buf_list_bytes = exported_num * TARGET_PAGE_SIZE;
    /* Ask the destination to continue to load the next vCPU states */
    virtcca_mig_put_mig_hdr(f, exported_num, CVM_MIG_F_CONTINUE);

    qemu_put_buffer(f, (uint8_t *)stream->mbmd, mbmd_bytes);
    qemu_put_buffer(f, (uint8_t *)stream->buf_list, buf_list_bytes);

    return 0;
}

static int virtcca_mig_save_tecs(QEMUFile *f, virtCCAMigStream *stream)
{
    CPUState *cpu;
    int ret;

    CPU_FOREACH(cpu) {
        ret = virtcca_mig_save_one_tec(f, stream);
        if (ret) {
            return ret;
        }
    }

    return 0;
}

static int virtcca_mig_savevm_state_end(QEMUFile *f)
{
    virtCCAMigStream *stream = &virtCCA_mig.streams[0];
    int ret;

    ret = virtcca_mig_save_tecs(f, stream);
    if (ret) {
        return ret;
    }

    ret = virtcca_mig_save_epoch(f, true);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static bool virtcca_migvm_agent_attest(bool is_src, const char *dst_ip, uint16_t dst_port)
{
    struct virtcca_dst_host_info info;
    struct kvm_virtcca_mig_cmd cmd;
    int ret = 0;

    memset(&info, 0, sizeof(struct virtcca_dst_host_info));
    cmd.flags = 0;
    info.version = KVM_CVM_MIGVM_VERSION;
    cmd.data = (__u64)(unsigned long)&info;
    if (is_src) {
        if (dst_ip == NULL) {
            error_report("migration dst ip is NULL");
            return false;
        }

        if (strlen(dst_ip) >= sizeof(info.dst_ip)) {
            error_report("migration dst ip too long");
            return false;
        }
        strncpy(info.dst_ip, dst_ip, sizeof(info.dst_ip) - 1);

        info.dst_ip[sizeof(info.dst_ip) - 1] = '\0';
        info.dst_port = dst_port;
        cmd.id = KVM_CVM_MIGCVM_ATTEST;
    } else {
        cmd.id = KVM_CVM_MIGCVM_ATTEST_DST;
    }

    ret = kvm_vm_ioctl(kvm_state, KVM_CVM_MIG_IOCTL, &cmd);
    if (ret) {
        error_report("Failed to virtcca_migvm_agent_attest: %d", ret);
        return false;
    }

    return true;
}

static bool virtcca_premig_is_done(bool is_src)
{
    struct virtcca_bind_info info;
    struct kvm_virtcca_mig_cmd cmd;
    int ret;

    if(!is_src) {
        return true;
    }
    cmd.id = KVM_CVM_GET_BIND_STATUS;
    cmd.flags = 0;
    memset(&info, 0, sizeof(struct virtcca_bind_info));
    info.version = KVM_CVM_MIGVM_VERSION;
    cmd.data = (__u64)(unsigned long)&info;

    ret = kvm_vm_ioctl(kvm_state, KVM_CVM_MIG_IOCTL, &cmd);
    if (ret) {
        error_report("Failed to get the migration info: %d", ret);
        return false;
    }

    return !!info.premig_done;
}

/* check the mig */
static bool virtcca_mig_is_ready(bool is_src, const char *dst_ip, uint16_t dst_port)
{
    int ret;
    ret = virtcca_check_mig_mem(is_src);
    if (ret < 0) {
        error_report("Failed to migrate cvm, secure memory is insufficient.");
        return false;
    }

    if(virtcca_migvm_agent_attest(is_src, dst_ip, dst_port))
        return virtcca_premig_is_done(is_src);
    return false;
}

static int virtcca_get_mig_info(void)
{
    int ret;
    virtCCAMigStream *stream = &virtCCA_mig.streams[0];
    virtCCAMigInfo virtca_mig_info;

    ret = virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_GET_MIG_INFO, 0, &virtca_mig_info);
    if (ret) {
        error_report("virtcca_get_mig_info failed!");
    }

    virtCCA_mig.swiotlb_start = virtca_mig_info.swiotlb_start;
    virtCCA_mig.swiotlb_end = virtca_mig_info.swiotlb_end;
    return ret;
}

/* Enable the ko creation */
static int virtcca_mig_stream_create(virtCCAMigStream *stream)
{
    info_report("Entering and calling virtcca_mig_stream_create");
    int ret;

    ret = kvm_create_device(kvm_state, KVM_DEV_TYPE_VIRTCCA_MIG_STREAM, false);
    if (ret < 0) {
        error_report("Failed to create virtcca mig stream due to %s", strerror(errno));
        return ret;
    }
    stream->fd = ret;

    return 0;
}

/* Set up the stream buffer data and create the mig ko */
static int virtcca_mig_do_stream_setup(virtCCAMigStream *stream, uint32_t nr_pages)
{
    int ret;
    struct kvm_dev_virtcca_mig_attr virtcca_mig_attr;
    struct kvm_device_attr attr = {
        .group = KVM_DEV_VIRTCCA_MIG_ATTR,
        .addr = (uint64_t)&virtcca_mig_attr,
        .attr = sizeof(struct kvm_dev_virtcca_mig_attr),
    };
    size_t map_size;
    off_t map_offset;

    ret = virtcca_mig_stream_create(stream);
    if (ret) {
        return ret;
    }

    /*
     * Tell the virtCCA_mig driver the number of pages to add to buffer list for
     * private page export/import.
     */
    virtcca_mig_attr.buf_list_pages = nr_pages;
    virtcca_mig_attr.version = KVM_DEV_VIRTCCA_MIG_ATTR_VERSION;
    if (kvm_device_ioctl(stream->fd, KVM_SET_DEVICE_ATTR, &attr) < 0) {
        return -EIO;
    }

    /* check the set is ok */
    memset(&virtcca_mig_attr, 0, sizeof(struct kvm_dev_virtcca_mig_attr));
    virtcca_mig_attr.version = KVM_DEV_VIRTCCA_MIG_ATTR_VERSION;
    if (kvm_device_ioctl(stream->fd, KVM_GET_DEVICE_ATTR, &attr) < 0) {
        return -EIO;
    }

    /* four metadata map offset and size setup */
    map_offset = VIRTCCA_MIG_STREAM_MBMD_MAP_OFFSET;
    map_size = (VIRTCCA_MIG_STREAM_GPA_LIST_MAP_OFFSET -
                VIRTCCA_MIG_STREAM_MBMD_MAP_OFFSET) * TARGET_PAGE_SIZE;
    stream->mbmd = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        stream->fd, map_offset);
    if (stream->mbmd == MAP_FAILED) {
        ret = -errno;
        error_report("Failed to map mbmd due to %s", strerror(ret));
        return ret;
    }

    map_offset = VIRTCCA_MIG_STREAM_GPA_LIST_MAP_OFFSET * TARGET_PAGE_SIZE;
    map_size = (VIRTCCA_MIG_STREAM_MAC_LIST_MAP_OFFSET -
                VIRTCCA_MIG_STREAM_GPA_LIST_MAP_OFFSET) * TARGET_PAGE_SIZE;
    stream->gpa_list = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                            stream->fd, map_offset);
    if (stream->gpa_list == MAP_FAILED) {
        ret = -errno;
        error_report("Failed to map gpa list due to %s", strerror(ret));
        return ret;
    }

    map_offset = VIRTCCA_MIG_STREAM_MAC_LIST_MAP_OFFSET * TARGET_PAGE_SIZE;
    map_size = (VIRTCCA_MIG_STREAM_BUF_LIST_MAP_OFFSET -
                VIRTCCA_MIG_STREAM_MAC_LIST_MAP_OFFSET) * TARGET_PAGE_SIZE;
    stream->mac_list = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                            stream->fd, map_offset);
    if (stream->mac_list == MAP_FAILED) {
        ret = -errno;
        error_report("Failed to map mac list due to %s", strerror(ret));
        return ret;
    }

    map_offset = VIRTCCA_MIG_STREAM_BUF_LIST_MAP_OFFSET * TARGET_PAGE_SIZE;
    map_size = virtcca_mig_attr.buf_list_pages * TARGET_PAGE_SIZE;
    stream->buf_list = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                            stream->fd, map_offset);
    if (stream->buf_list == MAP_FAILED) {
        ret = -errno;
        error_report("Failed to map buf list due to %s", strerror(ret));
        return ret;
    }

    return 0;
}

/* after the cgs savevm setup, enter the virtcca stream setup procedure */
static int virtcca_mig_stream_setup(uint32_t nr_channels, uint32_t nr_pages)
{
    info_report("Entering and calling virtcca_mig_stream_setup");
    virtCCAMigStream *stream;
    int i, ret;

    virtCCA_mig.streams = g_malloc0(sizeof(struct virtCCAMigStream) * nr_channels);

    for (i = 0; i < nr_channels; i++) {
        stream = &virtCCA_mig.streams[i];
        ret = virtcca_mig_do_stream_setup(stream, nr_pages);
        if (!ret) {
            virtCCA_mig.nr_streams++;
        } else {
            return ret;
        }
    }

    virtcca_get_mig_info();
    return 0;
}

static void virtcca_mig_stream_cleanup(virtCCAMigStream *stream)
{
    info_report("Entering and calling virtcca_mig_stream_cleanup");
    struct kvm_dev_virtcca_mig_attr virtcca_mig_attr;
    struct kvm_device_attr attr = {
        .group = KVM_DEV_VIRTCCA_MIG_ATTR, /* add the ko clean attr */
        .addr = (uint64_t)&virtcca_mig_attr,
        .attr = sizeof(struct kvm_dev_virtcca_mig_attr),
    };
    size_t unmap_size;
    int ret;

    memset(&virtcca_mig_attr, 0, sizeof(struct kvm_dev_virtcca_mig_attr));
    ret = kvm_device_ioctl(stream->fd, KVM_GET_DEVICE_ATTR, &attr);
    if (ret < 0) {
        error_report("virtcca mig cleanup failed: %s", strerror(ret));
        return;
    }

    unmap_size = (VIRTCCA_MIG_STREAM_GPA_LIST_MAP_OFFSET -
                  VIRTCCA_MIG_STREAM_MBMD_MAP_OFFSET) * TARGET_PAGE_SIZE;
    munmap(stream->mbmd, unmap_size);

    unmap_size = (VIRTCCA_MIG_STREAM_MAC_LIST_MAP_OFFSET -
                  VIRTCCA_MIG_STREAM_GPA_LIST_MAP_OFFSET) * TARGET_PAGE_SIZE;
    munmap(stream->gpa_list, unmap_size);

    unmap_size = (VIRTCCA_MIG_STREAM_BUF_LIST_MAP_OFFSET -
                  VIRTCCA_MIG_STREAM_MAC_LIST_MAP_OFFSET) * TARGET_PAGE_SIZE;
    munmap(stream->mac_list, unmap_size);

    unmap_size = virtcca_mig_attr.buf_list_pages * TARGET_PAGE_SIZE;
    munmap(stream->buf_list, unmap_size);
    close(stream->fd);
}

static void virtcca_mig_cleanup(void)
{
    int i;

    for (i = 0; i < virtCCA_mig.nr_streams; i++) {
        virtcca_mig_stream_cleanup(&virtCCA_mig.streams[i]);
    }

    virtCCA_mig.nr_streams = 0;

    g_free(virtCCA_mig.streams);
    virtCCA_mig.streams = NULL;
}

static void virtcca_mig_loadvm_state_cleanup(void)
{
    virtCCAMigStream *stream = &virtCCA_mig.streams[0];

    virtcca_mig_stream_ioctl(stream, KVM_CVM_MIG_IMPORT_END, 0, 0);
    virtcca_mig_cleanup();
}

static int virtcca_mig_savevm_state_abort(void)
{
    int ret;

    struct kvm_virtcca_mig_cmd cmd;
    cmd.id = KVM_CVM_MIG_EXPORT_ABORT;
    cmd.flags = 0;
    cmd.data = 0;
    ret = kvm_vm_ioctl(kvm_state, KVM_CVM_MIG_IOCTL, &cmd);

    if (ret) {
        error_report("%s: failed: failed to abort %d", __func__, ret);
    }

    return ret;
}


static int virtcca_mig_loadvm_state(QEMUFile *f, uint32_t channel_id)
{
    virtCCAMigStream *stream = &virtCCA_mig.streams[channel_id];
    uint64_t mbmd_bytes, buf_list_bytes, mac_list_bytes, gpa_list_bytes;
    uint64_t buf_list_num = 0;
    bool should_continue = true;
    uint8_t mbmd_type;
    int ret, cmd_id;
    virtCCAMigHdr hdr;

    while (should_continue) {
        if (should_continue && qemu_peek_le16(f, sizeof(hdr)) == 0) {
            continue;
        }
        qemu_get_buffer(f, (uint8_t *)&hdr, sizeof(hdr));
        mbmd_bytes = qemu_peek_le16(f, 0);
        qemu_get_buffer(f, (uint8_t *)stream->mbmd, mbmd_bytes);
        mbmd_type = virtcca_mig_stream_get_mbmd_type(stream);

        buf_list_num = hdr.buf_list_num;
        buf_list_bytes = buf_list_num * TARGET_PAGE_SIZE;
        if (buf_list_num) {
            qemu_get_buffer(f, (uint8_t *)stream->buf_list, buf_list_bytes);
        }

        switch (mbmd_type) {
        case KVM_VIRTCCA_MIG_MBMD_TYPE_IMMUTABLE_STATE:
            cmd_id = KVM_CVM_MIG_IMPORT_STATE_IMMUTABLE;
            break;
        case KVM_VIRTCCA_MIG_MBMD_TYPE_MEMORY_STATE:
            cmd_id = KVM_CVM_MIG_IMPORT_MEM;
            mac_list_bytes = buf_list_num * sizeof(Int128);
            gpa_list_bytes = buf_list_num * sizeof(GpaListEntry);
            qemu_get_buffer(f, (uint8_t *)stream->gpa_list, gpa_list_bytes);
            qemu_get_buffer(f, (uint8_t *)stream->mac_list, mac_list_bytes);
            break;
        case KVM_VIRTCCA_MIG_MBMD_TYPE_EPOCH_TOKEN:
            cmd_id = KVM_CVM_MIG_IMPORT_TRACK;
            break;
        case KVM_VIRTCCA_MIG_MBMD_TYPE_VCPU_STATE:
            cmd_id = KVM_CVM_MIG_IMPORT_STATE_TEC;
            break;
        default:
            error_report("%s: unsupported mb_type %d", __func__, mbmd_type);
            return -1;
        }

        ret = virtcca_mig_stream_ioctl(stream, cmd_id, 0, &buf_list_num);

        if (cmd_id == KVM_CVM_MIG_IMPORT_STATE_IMMUTABLE) {
            virtcca_get_mig_info();
        }

        if (ret) {
            ret = -1;
            if (buf_list_num != 0) {
                error_report("%s: buf_list_num=%lx", __func__, buf_list_num);
            }
            break;
        }
        should_continue = hdr.flags & CVM_MIG_F_CONTINUE;
    }

    return ret;
}

static int virtcca_mig_create_tec(QEMUFile *f)
{
    return tmm_create_tec();
}

void vircca_mig_init(CgsMig *cgs_mig)
{
    cgs_mig->is_ready = virtcca_mig_is_ready;
    cgs_mig->savevm_state_setup = virtcca_mig_stream_setup;
    cgs_mig->savevm_state_start = virtcca_mig_savevm_state_start;
    cgs_mig->savevm_state_ram_start_epoch =
                        virtcca_mig_savevm_state_ram_start_epoch;
    cgs_mig->savevm_state_ram = virtcca_mig_savevm_state_ram;
    cgs_mig->savevm_state_pause = virtcca_mig_savevm_state_pause;
    cgs_mig->savevm_state_end = virtcca_mig_savevm_state_end;
    cgs_mig->savevm_state_cleanup = virtcca_mig_cleanup;
    cgs_mig->savevm_state_abort = virtcca_mig_savevm_state_abort;
    cgs_mig->loadvm_state_setup = virtcca_mig_stream_setup;
    cgs_mig->loadvm_state = virtcca_mig_loadvm_state;
    cgs_mig->loadvm_create_tec = virtcca_mig_create_tec;
    cgs_mig->loadvm_state_cleanup = virtcca_mig_loadvm_state_cleanup;
}
