/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"
#include "hw/arm/virt.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/ub/ub_common.h"
#include "hw/ub/ubus_instance.h"
#include "sysemu/dma.h"

/* tmp for vfio-ub run with stub, remove later */

uint32_t fill_rq(BusControllerState *s, void *rsp, uint32_t rsp_size)
{
    uint32_t ci = ub_get_long(s->msgq_reg + RQ_CI);
    uint32_t pi = ub_get_long(s->msgq_reg + RQ_PI);
    uint32_t pi_new;
    uint32_t depth = s->msgq.rq_depth;
    uint32_t remain;
    hwaddr dst_rqe;
    uint32_t require;

    if (!s->msgq.rq_base_addr_gpa) {
        qemu_log("rq_base_addr_gpa is NULL\n");
        return UINT32_MAX;
    }

    if (ci >= depth || pi >= depth) {
        qemu_log("Invalid RQ indices: ci=%u pi=%u depth=%u\n", ci, pi, depth);
        return UINT32_MAX;
    }

    require = DIV_ROUND_UP(rsp_size, HI_MSG_RQE_SIZE);
    remain = depth - (pi + depth - ci) % depth;
    if (remain < require + 1) {
        qemu_log("RQ is full! require: %u, remain %u, depth %u, ci %u, pi %u\n",
                 require + 1, remain, depth, ci, pi);
        return UINT32_MAX;
    }

    dst_rqe = (uint64_t)((uint8_t *)s->msgq.rq_base_addr_gpa + pi * HI_MSG_RQE_SIZE);
    dma_memory_write(&address_space_memory, dst_rqe, rsp, rsp_size,
                     MEMTXATTRS_MEMORY);
    pi_new = (pi + require) % depth;
    ub_set_long(s->msgq_reg + RQ_PI, pi_new);
    return pi;
}

uint32_t fill_cq(BusControllerState *s, HiMsgCqe *cqe)
{
    uint32_t ci = ub_get_long(s->msgq_reg + CQ_CI);
    uint32_t pi = ub_get_long(s->msgq_reg + CQ_PI);
    uint32_t depth = s->msgq.cq_depth;
    uint32_t remain;
    hwaddr dst_cqe;

    if (ci >= depth || pi >= depth) {
        qemu_log("Invalid CQ indices: ci=%u pi=%u depth=%u\n", ci, pi, depth);
        return UINT32_MAX;
    }

    if (!s->msgq.cq_base_addr_gpa) {
        qemu_log("sq_base_addr_gpa is NULL\n");
        return UINT32_MAX;
    }

    remain = depth - (pi + depth - ci) % depth;
    if (remain <= 1) {
        qemu_log("CQ is full! depth=%u ci=%u pi=%u\n", depth, ci, pi);
        return UINT32_MAX;
    }

    dst_cqe = (uint64_t)((HiMsgCqe *)s->msgq.cq_base_addr_gpa + pi);
    dma_memory_write(&address_space_memory, dst_cqe, cqe,
                     sizeof(HiMsgCqe), MEMTXATTRS_MEMORY);
    ub_set_long(s->msgq_reg + CQ_PI, ++pi % depth);

    return pi;
}

/*
 * Fill RQ and CQ atomically with single lock acquisition.
 * This ensures RQ write and CQ write are atomic together.
 */
void fill_rq_cq(BusControllerState *s, void *rsp, uint32_t rsp_size, HiMsgCqe *cqe)
{
    uint32_t rq_pi;

    pthread_spin_lock(&s->rq_cq_lock);

    rq_pi = fill_rq(s, rsp, rsp_size);
    if (rq_pi == UINT32_MAX) {
        qemu_log("fill rq failed!\n");
        pthread_spin_unlock(&s->rq_cq_lock);
        return;
    }
    cqe->rq_pi = rq_pi;
    (void)fill_cq(s, cqe);

    pthread_spin_unlock(&s->rq_cq_lock);
}

#define UB_MEM_DUMP_MAX_STR_LEN 4096
#define UB_MEM_DUMP_COLUMN 4
#define UB_MEM_DUMP_WIDTH 36
#define UB_MEM_DUMP_MAX_BYTES 2048
#define UB_HEXDUMP_TITLE "   ↓0x0      ↓0x4      ↓0x8     ↓0xC\n"
int ub_hexdump(void *data, int offset, int len, char *buff, int buff_size)
{
    size_t l = 0;
    size_t tmp;
    int dw = len / sizeof(uint32_t) + !!(len % sizeof(uint32_t));
    int dw_round_up = ROUND_UP(dw, UB_MEM_DUMP_COLUMN);
    int i;
    void *real_data = data + offset;
    char str_addr[64] = {0};
    int width;
    bool last_line_all_0 = 1;
    int cnt_line_all_0 = 0;
    size_t last_line_cnt_character = 0;
    g_autofree char *line = line_generator(UB_MEM_DUMP_WIDTH);
    g_autofree char *line_head = g_strdup_printf("┌%s┐", line);
    g_autofree char *line_tail = g_strdup_printf("└%s┘", line);
    g_autofree char *line_zero = g_strdup_printf("%-*s",
                                                 UB_MEM_DUMP_WIDTH + 3, "│");

    if (!line || !line_head || !line_tail || !line_zero) {
        qemu_log("failed to alloc mem %p %p %p %p\n",
                 line, line_head, line_tail, line_zero);
        return -1;
    }

    if (buff_size < strlen(line_head) + strlen(line_tail) +
        strlen(UB_HEXDUMP_TITLE) + dw_round_up * 8) {
        qemu_log("buff too small %d %d %ld\n",
                 buff_size, len, strlen(line_head) + strlen(line_tail) +
                 strlen(UB_HEXDUMP_TITLE) + dw_round_up * 8);
        return -1;
    }
    snprintf(str_addr, sizeof(str_addr), "0x%x", offset + len);
    width = (int)strlen(str_addr);
    l += snprintf(buff + l, buff_size - l, "\n%*s%s",
                  width, " ", UB_HEXDUMP_TITLE);
    l += snprintf(buff + l, buff_size - l, "%*s%s",
                  width, " ", line_head);
    for (i = 0; i < dw_round_up; i++) {
        if (i >= dw) {
            l += snprintf(buff + l, buff_size - l, " %8s", " ");
        } else {
            if ((i % UB_MEM_DUMP_COLUMN) != 0) {
                tmp = snprintf(buff + l, buff_size - l, " %.8x",
                               *((uint32_t *)real_data + i));
                l += tmp;
                last_line_all_0 &= !(*((uint32_t *)real_data + i));
                last_line_cnt_character += tmp;
            } else {
                if (last_line_all_0 && last_line_cnt_character) {
                    cnt_line_all_0++;
                    if (cnt_line_all_0 == 2) {
                        l -= last_line_cnt_character;
                        l += snprintf(buff + l, buff_size - l,
                                      "│\n%*s%s", width, "...",
                                      line_zero);
                    } else if (cnt_line_all_0 > 2) {
                        l -= last_line_cnt_character;
                    }
                } else {
                    cnt_line_all_0 = 0;
                }
                snprintf(str_addr, sizeof(str_addr), "0x%lx",
                         offset + i * sizeof(uint32_t));
                tmp = snprintf(buff + l, buff_size - l, "%s\n%*s│ %.8x",
                               i == 0 ? "" : "│", width, str_addr,
                               *((uint32_t *)real_data + i));
                l += tmp;
                last_line_all_0 = !(*((uint32_t *)real_data + i));
                last_line_cnt_character = tmp;
            }
        }
    }
    l += snprintf(buff + l, buff_size - l, "│\n%*s%s\n",
                  width, " ", line_tail);
    return 0;
}

void ub_mem_dump(void *start, int size, const char *tag_fmt, ...)
{
    va_list ap;
    char str[UB_MEM_DUMP_MAX_STR_LEN] = {0};
    size_t l;

    /* get mem tag info */
    va_start(ap, tag_fmt);
    l = vsnprintf(str, sizeof(str), tag_fmt, ap);
    va_end(ap);

    if (size > UB_MEM_DUMP_MAX_BYTES) {
        qemu_log("%s execeed max len %d\n",
                 str, UB_MEM_DUMP_MAX_BYTES);
        return;
    }

    if (ub_hexdump(start, 0, size, str + l, sizeof(str) - l) < 0) {
        qemu_log("failed to dump memory. %s\n", str);
        return;
    }
    qemu_log("%s", str);
}

/* get interrupt_id from sysfs, not found will return UINT32_MAX */
#define MAX_BUF_LENGTH 1024
uint32_t sysfs_get_dev_number_by_guid(UbGuid *guid)
{
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    uint32_t id = UINT32_MAX;
    const char *ub_sysfs_devices = "/sys/bus/ub/devices";
    struct dirent *entry;
    DIR *dir = NULL;

    dir = opendir(ub_sysfs_devices);
    if (!dir) {
        qemu_log("failed to opendir %s\n", ub_sysfs_devices);
        return UINT32_MAX;
    }
    ub_device_get_str_from_guid(guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);

    while ((entry = readdir(dir)) != NULL) {
        char file_path[MAX_BUF_LENGTH] = {0};   /* guid file path */
        char guid_buffer[MAX_BUF_LENGTH] = {0}; /* guid that read from file */
        FILE *file = NULL;
        size_t bytes_read;

        /* skip the stumbling blocks */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(file_path, sizeof(file_path), "%s/%s/guid",
                 ub_sysfs_devices, entry->d_name);
        file = fopen(file_path, "r");
        if (file == NULL) {
            qemu_log("failed to open %s\n", file_path);
            closedir(dir);
            return UINT32_MAX;
        }

        bytes_read = fread(guid_buffer, 1, MAX_BUF_LENGTH - 1, file);
        fclose(file);
        guid_buffer[bytes_read] = '\0';
        /* discard annoying line breaks */
        if (bytes_read > 0 && guid_buffer[bytes_read - 1] == '\n') {
            guid_buffer[bytes_read - 1] = '\0';
        }

        /* check if it's a long-awaited true love */
        if (strcmp(guid_buffer, guid_str) == 0) {
            sscanf(entry->d_name, "%x", &id);
            closedir(dir);
            return id;
        }
    }
    closedir(dir);
    return id;
}

uint32_t sysfs_get_ub_device_bus_instance_eid(char *sysfsdev)
{
    FILE *f = NULL;
    char *path = NULL;
    char bus_instance_eid_buf[MAX_BUF_LENGTH] = {0};
    uint32_t eid = UINT32_MAX;

    path = g_strdup_printf("%s/instance", sysfsdev);
    f = fopen(path, "r");
    if (!f) {
        qemu_log("failed to open file:%s\n", path);
        g_free(path);
        return eid;
    }

    if (fgets(bus_instance_eid_buf, MAX_BUF_LENGTH, f) != NULL) {
        sscanf(bus_instance_eid_buf, "%x", &eid);
        qemu_log("sysfs(%s) get bus instance eid: 0x%x.\n", sysfsdev, eid);
    }

    if (eid == UINT32_MAX) {
        qemu_log("cannot get bus instance eid: %s.\n", sysfsdev);
    }

    fclose(f);
    g_free(path);
    return eid;
}

uint32_t sysfs_get_bus_instance_eid_by_guid(UbGuid *guid)
{
    FILE *file = NULL;
    char guid_str[UB_DEV_GUID_STRING_LENGTH + 1] = {0};
    uint32_t eid = UINT32_MAX;
    char line[MAX_BUF_LENGTH] = {0};
    bool found = false;

    ub_device_get_str_from_guid(guid, guid_str, UB_DEV_GUID_STRING_LENGTH + 1);
    file = fopen("/sys/bus/ub/instance", "r");
    if (file == NULL) {
        qemu_log("failed to open /sys/bus/ub/instance\n");
        return eid;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, guid_str) != NULL) {
            found = true;
            break;
        }
    }
    fclose(file);

    if (found) {
        /*                           0       1       2       3
         * /sys/bus/ub/instance: guid:xxx type:xxx eid:xxx upi:xxx
         */
        char **eid_str = g_strsplit(line, " ", 4);
        if (eid_str && eid_str[2]) {
            sscanf(eid_str[2], "eid:%05x", &eid);
            qemu_log("find ubus instance eid 0x%x by guid %s\n", eid, guid_str);
        }
        g_strfreev(eid_str);
    }

    if (eid == UINT32_MAX) {
        qemu_log("can not find instance eid by guid %s.\n", guid_str);
    }

    return eid;
}

uint32_t sysfs_get_bus_instance_type_by_eid(uint32_t eid)
{
    FILE *file = NULL;
    char *eid_str = NULL;
    char line[MAX_BUF_LENGTH] = {0};
    int bus_instance_type = UBUS_INSTANCE_UNKNOW;
    bool found = false;

    file = fopen("/sys/bus/ub/instance", "r");
    if (file == NULL) {
        qemu_log("failed to open /sys/bus/ub/instance\n");
        return bus_instance_type;
    }

    eid_str = g_strdup_printf("eid:%05x", eid);
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, eid_str) != NULL) {
            found = true;
            break;
        }
    }
    g_free(eid_str);
    fclose(file);

    if (found) {
        /*                           0       1       2       3
         * /sys/bus/ub/instance: guid:xxx type:xxx eid:xxx upi:xxx
         */
        char **type = g_strsplit(line, " ", 4);
        if (type && type[1]) {
            sscanf(type[1] + strlen("type:"), "%d", &bus_instance_type);
            qemu_log("bus instance eid(0x%x) type is %d.\n", eid, bus_instance_type);
        }
        g_strfreev(type);
    }

    if (bus_instance_type == UBUS_INSTANCE_UNKNOW) {
        qemu_log("can not get bus instance type by eid: 0x%x\n", eid);
    }

    return bus_instance_type;
}

bool ub_guid_is_none(UbGuid *guid)
{
    if (guid->seq_num == 0 &&
        guid->device_id == 0 && guid->version == 0 &&
        guid->type == 0 && guid->vendor == 0) {
        return true;
    }

    return false;
}

/* The caller is responsible for free memory. */
char *line_generator(uint8_t len)
{
    char *line = NULL;
    int i, j;
    if (!len) {
        qemu_log("invalid len %d", len);
        return NULL;
    }

    line = g_malloc0(len * DASH_SZ + 1);
    if (!line) {
        qemu_log("failed to alloc mem %d", len * DASH_SZ + 1);
        return NULL;
    }
    for (i = 0, j = 0; i < len; i++) {
        line[j++] = '\xE2';
        line[j++] = '\x80';
        line[j++] = '\x94';
    }
    return line;
}
