/*
 * QEMU add virtcca cvm feature.
 * 
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 * 
 */

#include "qemu/osdep.h"
#include "exec/confidential-guest-support.h"
#include "hw/boards.h"
#include "hw/core/cpu.h"
#include "kvm_arm.h"
#include "migration/blocker.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-misc-target.h"
#include "qemu/osdep.h"
#include "qom/object.h"
#include "qom/object_interfaces.h"
#include "sysemu/kvm.h"
#include "sysemu/runstate.h"
#include "hw/loader.h"
#include "linux-headers/asm-arm64/kvm.h"
#include <unistd.h>
#ifdef CONFIG_VIRTCCA_MIGRATION
#include "migration/cgs.h"
#endif
#define TYPE_TMM_GUEST "tmm-guest"
OBJECT_DECLARE_SIMPLE_TYPE(TmmGuest, TMM_GUEST)

#define TMM_PAGE_SIZE qemu_real_host_page_size()
#define TMM_MAX_PMU_CTRS    0x20
#ifdef CONFIG_VIRTCCA_MIGRATION
#define TMM_MAX_CFG      8
#else
#define TMM_MAX_CFG      6
#endif
#define TMM_MEMORY_INFO_SYSFS "/sys/kernel/tmm/memory_info"

typedef struct {
    uint32_t kae_vf_num;
    hwaddr sec_addr[KVM_ARM_TMM_MAX_KAE_VF_NUM];
    hwaddr hpre_addr[KVM_ARM_TMM_MAX_KAE_VF_NUM];
} KaeDeviceInfo;

/* add the migration cap */
typedef struct {
    uint32_t mig_enable;
} MigrationCap;

struct TmmGuest {
    ConfidentialGuestSupport parent_obj;
    GSList *ram_regions;
    TmmGuestMeasurementAlgo measurement_algo;
    uint32_t sve_vl;
    uint32_t num_pmu_cntrs;
    KaeDeviceInfo kae_device_info;
    MigrationCap migration_cap;
    TmmMigVmCap migvm_cap;
};

typedef struct {
    hwaddr base1;
    hwaddr len1;
    hwaddr base2;
    hwaddr len2;
    bool populate;
} TmmRamRegion;

static TmmGuest *tmm_guest;
bool virtcca_mig_migcvm_allowed = false;

static int tmm_configure_one(TmmGuest *guest, uint32_t cfg, Error **errp)
{
    int ret = 1;
    const char *cfg_str;
    struct kvm_cap_arm_tmm_config_item args = {
        .cfg = cfg,
    };
 
    switch (cfg) {
    case KVM_CAP_ARM_TMM_CFG_RPV:
        return 0;
    case KVM_CAP_ARM_TMM_CFG_HASH_ALGO:
        switch (guest->measurement_algo) {
        case TMM_GUEST_MEASUREMENT_ALGO_DEFAULT:
             return 0;
        case TMM_GUEST_MEASUREMENT_ALGO_SHA256:
            args.hash_algo = KVM_CAP_ARM_TMM_MEASUREMENT_ALGO_SHA256;
            break;
        case TMM_GUEST_MEASUREMENT_ALGO_SHA512:
            args.hash_algo = KVM_CAP_ARM_TMM_MEASUREMENT_ALGO_SHA512;
            break;
        default:
            g_assert_not_reached();
        }
        cfg_str = "hash algorithm";
        break;
        case KVM_CAP_ARM_TMM_CFG_SVE:
            if (!guest->sve_vl) {
                return 0;
            }
            args.sve_vq = guest->sve_vl / 128;
            cfg_str = "SVE";
            break;
        case KVM_CAP_ARM_TMM_CFG_DBG:
            return 0;
        case KVM_CAP_ARM_TMM_CFG_MIG:
            if (!guest->migration_cap.mig_enable) {
                info_report("\n Qemu-KVM:\n\tMigration disabled\n\n");
                return 0;
            }
            args.mig_src = !runstate_check(RUN_STATE_INMIGRATE);
            if (args.mig_src) {
                info_report("\n  Migration Version: Dev(Live).\n \
                    WARNING: you are using Live Migration Version of virtCCA, this is src.\n\n");
            } else {
                info_report("\n  Migration Version: Dev(Live).\n \
                    WARNING: you are using Live Migration Version of virtCCA, this is dest.\n\n");
            }
            args.mig_enable = guest->migration_cap.mig_enable ? 1 : 0;
            cfg_str = "Migration";
            break;
        case KVM_CAP_ARM_TMM_CFG_MIG_CVM:
            if (!guest->migvm_cap) {
                return 0;
            }
            switch (guest->migvm_cap) {
            case KVM_CAP_ARM_TMM_MIGVM_DEFAULT:
                args.migration_migvm_cap = KVM_CAP_ARM_TMM_MIGVM_DEFAULT;
                break;
            case KVM_CAP_ARM_TMM_MIGVM_ENABLE:
                args.migration_migvm_cap = KVM_CAP_ARM_TMM_MIGVM_ENABLE;
                info_report("Migration Version: the migvm is enabled \n\n");
                virtcca_mig_migcvm_allowed = true;
                break;
            default:
                g_assert_not_reached();
            }
            cfg_str = "migvm enabled";
            break;
        case KVM_CAP_ARM_TMM_CFG_PMU:
            if (!guest->num_pmu_cntrs) {
                return 0;
            }
            args.num_pmu_cntrs = guest->num_pmu_cntrs;
            cfg_str = "PMU";
            break;
        case KVM_CAP_ARM_TMM_CFG_KAE:
            if (!guest->kae_device_info.kae_vf_num) {
                return 0;
            }
            args.kae_vf_num= guest->kae_device_info.kae_vf_num;
            for (int i = 0; i < guest->kae_device_info.kae_vf_num; i++) {
                args.sec_addr[i] = guest->kae_device_info.sec_addr[i];
                args.hpre_addr[i] = guest->kae_device_info.hpre_addr[i];
            }
            cfg_str = "KAE";
            break;
        default:
            g_assert_not_reached();
    }
 
    ret = kvm_vm_enable_cap(kvm_state, KVM_CAP_ARM_RME, 0,
                            KVM_CAP_ARM_TMM_CONFIG_CVM, (intptr_t)&args);
    if (ret) {
        error_setg_errno(errp, -ret, "TMM: failed to configure %s", cfg_str);
    }

    return ret;
}

static gint tmm_compare_ram_regions(gconstpointer a, gconstpointer b)
{
    const TmmRamRegion *ra = a;
    const TmmRamRegion *rb = b;

    g_assert(ra->base1 != rb->base1);
    return ra->base1 < rb->base1 ? -1 : 1;
}

void tmm_add_ram_region(hwaddr base1, hwaddr len1, hwaddr base2, hwaddr len2, bool populate)
{
    TmmRamRegion *region;

    region = g_new0(TmmRamRegion, 1);
    region->base1 = QEMU_ALIGN_DOWN(base1, TMM_PAGE_SIZE);
    region->len1 = QEMU_ALIGN_UP(len1, TMM_PAGE_SIZE);
    region->base2 = QEMU_ALIGN_DOWN(base2, TMM_PAGE_SIZE);
    region->len2 = QEMU_ALIGN_UP(len2, TMM_PAGE_SIZE);
    region->populate = populate;

    tmm_guest->ram_regions = g_slist_insert_sorted(tmm_guest->ram_regions,
                                                   region, tmm_compare_ram_regions);
}

static void tmm_populate_region(gpointer data, gpointer unused)
{
    int ret;
    const TmmRamRegion *region = data;
    struct kvm_cap_arm_tmm_populate_region_args populate_args = {
        .populate_ipa_base1 = region->base1,
        .populate_ipa_size1 = region->len1,
        .populate_ipa_base2 = region->base2,
        .populate_ipa_size2 = region->len2,
        .flags = KVM_ARM_TMM_POPULATE_FLAGS_MEASURE,
    };

    if (!region->populate) {
        return;
    }

    ret = kvm_vm_enable_cap(kvm_state, KVM_CAP_ARM_RME, 0,
                            KVM_CAP_ARM_TMM_POPULATE_CVM,
                            (intptr_t)&populate_args);
    if (ret) {
        error_report("TMM: failed to populate cvm region (0x%"HWADDR_PRIx", 0x%"HWADDR_PRIx", 0x%"HWADDR_PRIx", 0x%"HWADDR_PRIx"): %s",
                     region->base1, region->len1, region->base2, region->len2, strerror(-ret));
        exit(1);
    }
}

static int tmm_create_rd(Error **errp)
{
    int ret = kvm_vm_enable_cap(kvm_state, KVM_CAP_ARM_RME, 0,
                                KVM_CAP_ARM_TMM_CREATE_RD);
    if (ret) {
        error_setg_errno(errp, -ret, "TMM: failed to create tmm Descriptor");
    }
    return ret;
}

int tmm_create_tec(void)
{
    CPUState *cs;
    int ret = 0;

    CPU_FOREACH(cs) {
        ret = kvm_arm_vcpu_finalize(cs, KVM_ARM_VCPU_REC);
        if (ret) {
            error_report("TMM: failed to finalize vCPU: %s", strerror(-ret));
            return ret;
        }
    }
    return ret;
}

#ifdef CONFIG_VIRTCCA_MIGRATION
static int virtcca_save_migvm_cid(uint64_t cid)
{
    info_report("calling virtcca_binding_with_migcvm_pid");
    struct kvm_virtcca_mig_cmd cmd;
    struct mig_cvm guest_mig_cvm_info;
    int ret;

    cmd.id = KVM_CVM_MIGCVM_SET_CID;
    cmd.flags = 0;
    guest_mig_cvm_info.version = KVM_CVM_MIGVM_VERSION;
    guest_mig_cvm_info.migvm_cid = cid; /* vsock cid of migvm */
    cmd.data = (__u64)(unsigned long)&guest_mig_cvm_info;

    ret = kvm_vm_ioctl(kvm_state, KVM_CVM_MIG_IOCTL, &cmd);
    if (ret) {
        error_report("failed to bind migcvm: %d", ret);
    }

    return ret;
}

typedef struct search_cid_ctx {
    const char *target_type;
    Object *result;
} search_cid_ctx_t;

static int recursive_search_cb(Object *obj, void *opaque)
{
    search_cid_ctx_t *ctx = (search_cid_ctx_t *)opaque;
    const char *obj_type = object_get_typename(obj);

    if (!ctx->result && strcmp(obj_type, ctx->target_type) == 0) {
        ctx->result = obj;
        return 1;
    }
    object_child_foreach(obj, recursive_search_cb, ctx);
    return 0;
}

static Object *find_vsock_backend(Object *vsock_obj)
{
    Error *err = NULL;
    Object *backend = object_property_get_link(vsock_obj, "vhost-vsock-device", &err);

    if (!err && backend) {
        return backend;
    }
    error_free(err);

    search_cid_ctx_t ctx = {
        .target_type = "vhost-vsock-device",
        .result = NULL,
    };
    object_child_foreach(vsock_obj, recursive_search_cb, &ctx);
    return ctx.result;
}

static Object *find_vsock_device(Object *root)
{
    const char *vsock_types[] = {
        "vhost-vsock-pci",
        "virtio-vsock-pci",
        NULL
    };

    for (int i = 0; vsock_types[i]; i++) {
        search_cid_ctx_t ctx = {
            .target_type = vsock_types[i],
            .result = NULL,
        };

        object_child_foreach(root, recursive_search_cb, &ctx);

        if (ctx.result) {
            info_report("Found VSOCK device of type '%s'", vsock_types[i]);
            return ctx.result;
        }
    }

    return NULL;
}

static uint64_t parse_migcvm_cid(void)
{
    Error *err = NULL;
    uint64_t cid = 0;
    info_report("calling parse_migcvm_cid");

    Object *machine = object_resolve_path("/machine", NULL);
    if (!machine) {
        error_report("Failed to find /machine object");
        return 0;
    }

    Object *vsock_obj = find_vsock_device(machine);
    if (!vsock_obj) {
        error_report("No VSOCK PCI device found");
        return 0;
    }

    Object *backend = find_vsock_backend(vsock_obj);
    if (!backend) {
        error_report("No vhost-vsock-device backend found");
        return 0;
    }

    cid = object_property_get_uint(backend, "guest-cid", &err);
    if (err) {
        error_report_err(err);
        return 0;
    }
    info_report("Detected guest-cid: %" PRIu64, cid);
    return cid;
}

void virtcca_migvm_save_cid(void)
{
    uint64_t cid = 0;

    cid = parse_migcvm_cid();
    if (!cid) {
        error_report("Failed to parse migcvm cid");
        exit(1);
    }

    if (virtcca_save_migvm_cid(cid)) {
        error_report("Failed to save migcvm cid");
        exit(1);
    }
}
#endif
static void tmm_vm_state_change(void *opaque, bool running, RunState state)
{
    int ret;

    if (!running) {
        return;
    }

    g_slist_foreach(tmm_guest->ram_regions, tmm_populate_region, NULL);
    g_slist_free_full(g_steal_pointer(&tmm_guest->ram_regions), g_free);

    if (tmm_create_tec()) {
        exit(1);
    }

    ret = kvm_vm_enable_cap(kvm_state, KVM_CAP_ARM_RME, 0,
                            KVM_CAP_ARM_TMM_ACTIVATE_CVM);
    if (ret) {
        error_report("TMM: failed to activate cvm: %s", strerror(-ret));
        exit(1);
    }
}

int kvm_arm_tmm_init(ConfidentialGuestSupport *cgs, Error **errp)
{
    int ret;
    int cfg;
 
    if (!tmm_guest) {
        return -ENODEV;
    }
 
    if (!kvm_check_extension(kvm_state, KVM_CAP_ARM_RME)) {
        error_setg(errp, "KVM does not support TMM");
        return -ENODEV;
    }
 
    for (cfg = 0; cfg < TMM_MAX_CFG; cfg++) {
        ret = tmm_configure_one(tmm_guest, cfg, &error_abort);
        if (ret) {
            return ret;
        }
    }
 
    ret = tmm_create_rd(&error_abort);
    if (ret) {
        return ret;
    }
#ifdef CONFIG_VIRTCCA_MIGRATION
    if (runstate_check(RUN_STATE_INMIGRATE)) {
        ret = !cgs_mig_is_ready(false, NULL, 0);
    }

    if (ret) {
        error_setg(errp, "cgs mig required, but not ready");
        return ret;
    }
#endif
    qemu_add_vm_change_state_handler(tmm_vm_state_change, NULL);
    return 0;
}
 
static void tmm_get_sve_vl(Object *obj, Visitor *v, const char *name,
                           void *opaque, Error **errp)
{
    TmmGuest *guest = TMM_GUEST(obj);
 
    visit_type_uint32(v, name, &guest->sve_vl, errp);
}
 
static void tmm_set_sve_vl(Object *obj, Visitor *v, const char *name,
                           void *opaque, Error **errp)
{
    TmmGuest *guest = TMM_GUEST(obj);
    uint32_t value;
 
    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }
 
    if (value & 0x7f || value >= ARM_MAX_VQ * 128) {
        error_setg(errp, "invalid SVE vector length");
        return;
    }
 
    guest->sve_vl = value;
}

static void tmm_get_num_pmu_cntrs(Object *obj, Visitor *v, const char *name,
                                  void *opaque, Error **errp)
{
    TmmGuest *guest = TMM_GUEST(obj);

    visit_type_uint32(v, name, &guest->num_pmu_cntrs, errp);
}

static void tmm_set_num_pmu_cntrs(Object *obj, Visitor *v, const char *name,
                                  void *opaque, Error **errp)
{
    TmmGuest *guest = TMM_GUEST(obj);
    uint32_t value;

    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }

    if (value >= TMM_MAX_PMU_CTRS) {
        error_setg(errp, "invalid number of PMU counters");
        return;
    }

    guest->num_pmu_cntrs = value;
}

static int tmm_get_measurement_algo(Object *obj, Error **errp G_GNUC_UNUSED)
{
    TmmGuest *guest = TMM_GUEST(obj);

    return guest->measurement_algo;
}

static void tmm_set_measurement_algo(Object *obj, int algo, Error **errp G_GNUC_UNUSED)
{
    TmmGuest *guest = TMM_GUEST(obj);

    guest->measurement_algo = algo;
}

static void tmm_get_kae_vf_num(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    TmmGuest *guest = TMM_GUEST(obj);

    visit_type_uint32(v, name, &guest->kae_device_info.kae_vf_num, errp);
}

static void tmm_set_kae_vf_num(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    TmmGuest *guest = TMM_GUEST(obj);
    uint32_t value;

    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }

    if (value > KVM_ARM_TMM_MAX_KAE_VF_NUM) {
        error_setg(errp, "invalid number of kae vfs");
        return;
    }

    guest->kae_device_info.kae_vf_num = value;
}

int tmm_get_kae_num(void)
{
    return tmm_guest->kae_device_info.kae_vf_num;
}

void tmm_set_sec_addr(hwaddr base, int num)
{
    tmm_guest->kae_device_info.sec_addr[num] = base;
}

void tmm_set_hpre_addr(hwaddr base, int num)
{
    tmm_guest->kae_device_info.hpre_addr[num] = base;
}

#ifdef CONFIG_VIRTCCA_MIGRATION
/* get the mig ability config */
static void tmm_get_mig_cap(Object *obj, Visitor *v, const char *name,
                            void *opaque, Error **errp)
{
    TmmGuest *guest = TMM_GUEST(obj);

    visit_type_uint32(v, name, &guest->migration_cap.mig_enable, errp);
}

/* enable mig cap into qemu */
static void tmm_set_mig_cap(Object *obj, Visitor *v, const char *name,
                            void *opaque, Error **errp)
{
    TmmGuest *guest = TMM_GUEST(obj);
    uint32_t value;

    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }

    guest->migration_cap.mig_enable = value;
}

static int tmm_get_migvm_algo(Object *obj, Error **errp G_GNUC_UNUSED)
{
    TmmGuest *guest = TMM_GUEST(obj);

    return guest->migvm_cap;
}

static void tmm_set_migvm_algo(Object *obj, int algo, Error **errp G_GNUC_UNUSED)
{
    TmmGuest *guest = TMM_GUEST(obj);

    guest->migvm_cap = algo;
}
#endif

static void tmm_guest_class_init(ObjectClass *oc, void *data)
{
    object_class_property_add_enum(oc, "measurement-algo",
                                   "TmmGuestMeasurementAlgo",
                                   &TmmGuestMeasurementAlgo_lookup,
                                   tmm_get_measurement_algo,
                                   tmm_set_measurement_algo);
    object_class_property_set_description(oc, "measurement-algo",
                                          "cvm measurement algorithm ('sha256', 'sha512')"); 
    /*
     * This is not ideal. Normally SVE parameters are given to -cpu, but the
     * cvm parameters are needed much earlier than CPU initialization. We also
     * don't have a way to discover what is supported at the moment, the idea is
     * that the user knows exactly what hardware it is running on because these
     * parameters are part of the measurement and play in the attestation.
     */
    object_class_property_add(oc, "sve-vector-length", "uint32", tmm_get_sve_vl,
                              tmm_set_sve_vl, NULL, NULL);
    object_class_property_set_description(oc, "sve-vector-length",
            "SVE vector length. 0 disables SVE (the default)");
#ifdef CONFIG_VIRTCCA_MIGRATION
    /* Add the migration enable func */
    object_class_property_add(oc, "virtcca-migration-cap", "uint32", tmm_get_mig_cap,
                              tmm_set_mig_cap, NULL, NULL);
    object_class_property_set_description(oc, "virtcca-migration-cap",
            "Config of virtcca migration. 0 disables mig (the default)");
    object_class_property_add_enum(oc, "migvm-cap", "TmmMigVmCap", &TmmMigVmCap_lookup,
                                   tmm_get_migvm_algo, tmm_set_migvm_algo);
    object_class_property_set_description(oc, "migvm-cap",
            "Config of migCVM of virtcca migration. Options are ('default', 'migvm')");
#endif
    object_class_property_add(oc, "num-pmu-counters", "uint32",
                              tmm_get_num_pmu_cntrs, tmm_set_num_pmu_cntrs,
                              NULL, NULL);
    object_class_property_set_description(oc, "num-pmu-counters",
            "Number of PMU counters");
    object_class_property_add(oc, "kae", "uint32", tmm_get_kae_vf_num,
                              tmm_set_kae_vf_num, NULL, NULL);
    object_class_property_set_description(oc, "kae",
            "Number of KAE virtual functions. 0 disables KAE (the default)");
}
 
static void tmm_guest_instance_init(Object *obj)
{
    if (tmm_guest) {
        error_report("a single instance of TmmGuest is supported");
        exit(1);
    }
    tmm_guest = TMM_GUEST(obj);
}
 
static const TypeInfo tmm_guest_info = {
    .parent = TYPE_CONFIDENTIAL_GUEST_SUPPORT,
    .name = TYPE_TMM_GUEST,
    .instance_size = sizeof(struct TmmGuest),
    .instance_init = tmm_guest_instance_init,
    .class_init = tmm_guest_class_init,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_USER_CREATABLE },
        { }
    }
};
 
static void tmm_register_types(void)
{
    type_register_static(&tmm_guest_info);
}
type_init(tmm_register_types);

static VirtccaCapability *virtcca_get_capabilities(Error **errp)
{
    VirtccaCapability *cap = NULL;
    uint64_t tmi_version = 0;
    int rc = 0;

    if (kvm_ioctl(kvm_state, KVM_GET_TMI_VERSION, &tmi_version) < 0) {
        error_setg(errp, "VIRTCCA is not enabled in KVM");
        return NULL;
    }

    rc = access(TMM_MEMORY_INFO_SYSFS, R_OK);
    if (rc < 0) {
        error_setg_errno(errp, errno, "VIRTCCA: Failed to read %s",
                    TMM_MEMORY_INFO_SYSFS);
        return NULL;
    }

    cap = g_new0(VirtccaCapability, 1);

    cap->enabled = true;

    return cap;
}

VirtccaCapability *qmp_query_virtcca_capabilities(Error **errp)
{
    return virtcca_get_capabilities(errp);
}