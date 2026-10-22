/*
 * SMBIOS Type 131 (OEM UB Info) handling
 *
 * Copyright (c) 2024 Huawei Technologies Co., Ltd
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/config-file.h"
#include "qemu/bswap.h"
#include "qemu/cutils.h"
#include "qemu/option.h"
#include "hw/firmware/smbios.h"
#include "smbios_build.h"

/* Type 131 instance storage */
struct type131_instance {
    uint8_t     flag;
    uint16_t    pod_id;
    uint8_t     slot_id;
    uint8_t     mesh_type;
    uint32_t    super_pod_id;
    uint16_t    served_index;
    const char *type_name;     /* "UB INFO" string */
    QTAILQ_ENTRY(type131_instance) next;
};
static QTAILQ_HEAD(, type131_instance) type131 = QTAILQ_HEAD_INITIALIZER(type131);

static const QemuOptDesc qemu_smbios_type131_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },{
        .name = "flag",
        .type = QEMU_OPT_NUMBER,
        .help = "Flag: 0xFF if BMC fetch failed, 0x0 on success",
    },{
        .name = "pod_id",
        .type = QEMU_OPT_NUMBER,
        .help = "Server interconnect network ID (ChassisID)",
    },{
        .name = "slot_id",
        .type = QEMU_OPT_NUMBER,
        .help = "Server ID within the network",
    },{
        .name = "mesh_type",
        .type = QEMU_OPT_NUMBER,
        .help = "Topology type (0=8P,1=1D-pod,2=2D-16P,3=YH,4=UBX,5=DY,0x80=1D,0x81=clos)",
    },{
        .name = "super_pod_id",
        .type = QEMU_OPT_NUMBER,
        .help = "Multi-Pod combined network ID (reserved)",
    },{
        .name = "served_index",
        .type = QEMU_OPT_NUMBER,
        .help = "Supernode global unique index",
    },{
        .name = "type_name",
        .type = QEMU_OPT_STRING,
        .help = "Type name string (typically \"UB INFO\")",
    },
    { /* end of list */ }
};

#define T131_BASE 0x8300   /* OEM area, above T127_BASE (0x7F00) */

/*
 * Append one type 131 structure, including its string set, to the SMBIOS
 * table stream.  This is the function based counterpart of the generic
 * table building macros in smbios_build.h, except that the strings
 * are copied and the reserved bytes are cleared with the bounds checked
 * QEMU helpers the secure coding rules require.
 *
 * Returns false if the table must be skipped, true otherwise.
 */
static bool type131_add_table(const struct type131_instance *src,
                              uint16_t handle)
{
    struct smbios_type_131 *t;
    size_t t_off;          /* table offset into smbios_tables */
    size_t term_cnt, t_size;
    int str_index = 0;
    int len;

    if (smbios_skip_table(SMBIOS_TYPE_131, true)) {
        return false;
    }

    t_off = smbios_tables_len;
    smbios_tables_len += sizeof(struct smbios_type_131);
    smbios_tables = g_realloc(smbios_tables, smbios_tables_len);
    t = (struct smbios_type_131 *)(smbios_tables + t_off);

    t->header.type = SMBIOS_TYPE_131;
    t->header.length = sizeof(struct smbios_type_131);
    t->header.handle = cpu_to_le16(handle);
    t->flag          = src->flag;
    t->pod_id        = cpu_to_le16(src->pod_id);
    t->slot_id       = src->slot_id;
    t->mesh_type     = src->mesh_type;
    t->super_pod_id  = cpu_to_le32(src->super_pod_id);
    t->served_index  = cpu_to_le16(src->served_index);
    /* fill the reserved area with zeros */
    strpadcpy((char *)t->reserved, sizeof(t->reserved), "", '\0');

    /* append the type name string, refreshing the table pointer after it */
    len = (src->type_name != NULL) ? strlen(src->type_name) + 1 : 0;
    if (len > 1) {
        smbios_tables = g_realloc(smbios_tables, smbios_tables_len + len);
        pstrcpy((char *)smbios_tables + smbios_tables_len, len,
                src->type_name);
        smbios_tables_len += len;
        t = (struct smbios_type_131 *)(smbios_tables + t_off);
        t->type_name_str = ++str_index;
    } else {
        t->type_name_str = 0;
    }

    /* add the '\0' string set terminator (two if no string was added) */
    term_cnt = (str_index == 0) ? 2 : 1;
    smbios_tables = g_realloc(smbios_tables, smbios_tables_len + term_cnt);
    strpadcpy((char *)smbios_tables + smbios_tables_len, (int)term_cnt,
              "", '\0');
    smbios_tables_len += term_cnt;

    /* update the SMBIOS maximum element size and element count */
    t_size = smbios_tables_len - t_off;
    if (t_size > smbios_table_max) {
        smbios_table_max = t_size;
    }
    smbios_table_cnt++;

    return true;
}

void smbios_build_type_131_table(void)
{
    unsigned instance = 0;
    struct type131_instance *t131;

    QTAILQ_FOREACH(t131, &type131, next) {
        if (!type131_add_table(t131, T131_BASE + instance)) {
            return;
        }
        instance++;
    }
}

void smbios_entry_add_type131(QemuOpts *opts, Error **errp)
{
    struct type131_instance *t131_i;

    if (!qemu_opts_validate(opts, qemu_smbios_type131_opts, errp)) {
        return;
    }
    t131_i = g_new0(struct type131_instance, 1);
    t131_i->flag         = qemu_opt_get_number(opts, "flag", 0xFF);
    t131_i->pod_id       = qemu_opt_get_number(opts, "pod_id", 0);
    t131_i->slot_id      = qemu_opt_get_number(opts, "slot_id", 0);
    t131_i->mesh_type    = qemu_opt_get_number(opts, "mesh_type", 0);
    t131_i->super_pod_id = qemu_opt_get_number(opts, "super_pod_id", 0);
    t131_i->served_index = qemu_opt_get_number(opts, "served_index", 0);
    t131_i->type_name    = qemu_opt_get(opts, "type_name");
    QTAILQ_INSERT_TAIL(&type131, t131_i, next);
}
