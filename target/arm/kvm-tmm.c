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
#include "qom/object_interfaces.h"
#include "sysemu/kvm.h"
#include "sysemu/runstate.h"
#include "hw/loader.h"

#define TYPE_TMM_GUEST "tmm-guest"
OBJECT_DECLARE_SIMPLE_TYPE(TmmGuest, TMM_GUEST)

#define TMM_PAGE_SIZE qemu_real_host_page_size
#define TMM_MAX_PMU_CTRS    0x20 
#define TMM_MAX_CFG      5 

struct TmmGuest {
    ConfidentialGuestSupport parent_obj;
    GSList *ram_regions;
    TmmGuestMeasurementAlgo measurement_algo;
    uint32_t sve_vl;
    uint32_t num_pmu_cntrs;
};

typedef struct {
    hwaddr base1;
    hwaddr len1;
    hwaddr base2;
    hwaddr len2;
    bool populate;
} TmmRamRegion;

static TmmGuest *tmm_guest;
 
bool kvm_arm_tmm_enabled(void)
{
    return !!tmm_guest;
}
 
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
        case KVM_CAP_ARM_TMM_CFG_PMU:
            if (!guest->num_pmu_cntrs) {
                return 0;
            }
            args.num_pmu_cntrs = guest->num_pmu_cntrs;
            cfg_str = "PMU";
            break;
        default:
            g_assert_not_reached();
    }
 
    ret = kvm_vm_enable_cap(kvm_state, KVM_CAP_ARM_TMM, 0,
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

    ret = kvm_vm_enable_cap(kvm_state, KVM_CAP_ARM_TMM, 0,
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
    int ret = kvm_vm_enable_cap(kvm_state, KVM_CAP_ARM_TMM, 0,
                                KVM_CAP_ARM_TMM_CREATE_RD);
    if (ret) {
        error_setg_errno(errp, -ret, "TMM: failed to create tmm Descriptor");
    }
    return ret;
}

static void tmm_vm_state_change(void *opaque, bool running, RunState state)
{
    int ret;
    CPUState *cs;

    if (!running) {
        return;
    }

    g_slist_foreach(tmm_guest->ram_regions, tmm_populate_region, NULL);
    g_slist_free_full(g_steal_pointer(&tmm_guest->ram_regions), g_free);

    CPU_FOREACH(cs) {
        ret = kvm_arm_vcpu_finalize(cs, KVM_ARM_VCPU_TEC);
        if (ret) {
            error_report("TMM: failed to finalize vCPU: %s", strerror(-ret));
            exit(1);
        }
    }

    ret = kvm_vm_enable_cap(kvm_state, KVM_CAP_ARM_TMM, 0,
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
 
    if (!kvm_check_extension(kvm_state, KVM_CAP_ARM_TMM)) {
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
    object_class_property_add(oc, "num-pmu-counters", "uint32",
                              tmm_get_num_pmu_cntrs, tmm_set_num_pmu_cntrs,
                              NULL, NULL);
    object_class_property_set_description(oc, "num-pmu-counters",
            "Number of PMU counters");
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
