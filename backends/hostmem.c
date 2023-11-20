/*
 * QEMU Host Memory Backend
 *
 * Copyright (C) 2013-2014 Red Hat Inc
 *
 * Authors:
 *   Igor Mammedov <imammedo@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "sysemu/hostmem.h"
#include "hw/boards.h"
#include "qapi/error.h"
#include "qapi/qapi-builtin-visit.h"
#include "qapi/visitor.h"
#include "qemu/config-file.h"
#include "qom/object_interfaces.h"
#include "qemu/mmap-alloc.h"
#include "qemu/madvise.h"
#ifdef CONFIG_MBIND_PROPORTION
#include "qemu/option.h"
#include "sysemu/sysemu.h"
#include "qemu/log.h"
#include "qemu/units.h"
#endif

#ifdef CONFIG_NUMA
#include <numaif.h>
#include <numa.h>
QEMU_BUILD_BUG_ON(HOST_MEM_POLICY_DEFAULT != MPOL_DEFAULT);
/*
 * HOST_MEM_POLICY_PREFERRED may either translate to MPOL_PREFERRED or
 * MPOL_PREFERRED_MANY, see comments further below.
 */
QEMU_BUILD_BUG_ON(HOST_MEM_POLICY_PREFERRED != MPOL_PREFERRED);
QEMU_BUILD_BUG_ON(HOST_MEM_POLICY_BIND != MPOL_BIND);
QEMU_BUILD_BUG_ON(HOST_MEM_POLICY_INTERLEAVE != MPOL_INTERLEAVE);
#endif

#ifdef CONFIG_MBIND_PROPORTION
#define PROPORTION_MAX_NUM 11
#define PER_PROPORTION_MAX_LENGTH 32
#define PROPORTION_MAX_LENGTH 512
#endif

char *
host_memory_backend_get_name(HostMemoryBackend *backend)
{
    if (!backend->use_canonical_path) {
        return g_strdup(object_get_canonical_path_component(OBJECT(backend)));
    }

    return object_get_canonical_path(OBJECT(backend));
}

static void
host_memory_backend_get_size(Object *obj, Visitor *v, const char *name,
                             void *opaque, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    uint64_t value = backend->size;

    visit_type_size(v, name, &value, errp);
}

static void
host_memory_backend_set_size(Object *obj, Visitor *v, const char *name,
                             void *opaque, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    uint64_t value;

    if (host_memory_backend_mr_inited(backend)) {
        error_setg(errp, "cannot change property %s of %s ", name,
                   object_get_typename(obj));
        return;
    }

    if (!visit_type_size(v, name, &value, errp)) {
        return;
    }
    if (!value) {
        error_setg(errp,
                   "property '%s' of %s doesn't take value '%" PRIu64 "'",
                   name, object_get_typename(obj), value);
        return;
    }
    backend->size = value;
}

static void
host_memory_backend_get_host_nodes(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    uint16List *host_nodes = NULL;
    uint16List **tail = &host_nodes;
    unsigned long value;

    value = find_first_bit(backend->host_nodes, MAX_NODES);
    if (value == MAX_NODES) {
        goto ret;
    }

    QAPI_LIST_APPEND(tail, value);

    do {
        value = find_next_bit(backend->host_nodes, MAX_NODES, value + 1);
        if (value == MAX_NODES) {
            break;
        }

        QAPI_LIST_APPEND(tail, value);
    } while (true);

ret:
    visit_type_uint16List(v, name, &host_nodes, errp);
    qapi_free_uint16List(host_nodes);
}

static void
host_memory_backend_set_host_nodes(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
#ifdef CONFIG_NUMA
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    uint16List *l, *host_nodes = NULL;

    visit_type_uint16List(v, name, &host_nodes, errp);

    for (l = host_nodes; l; l = l->next) {
        if (l->value >= MAX_NODES) {
            error_setg(errp, "Invalid host-nodes value: %d", l->value);
            goto out;
        }
    }

    for (l = host_nodes; l; l = l->next) {
        bitmap_set(backend->host_nodes, l->value, 1);
    }

out:
    qapi_free_uint16List(host_nodes);
#else
    error_setg(errp, "NUMA node binding are not supported by this QEMU");
#endif
}

#ifdef CONFIG_MBIND_PROPORTION
static void
host_memory_backend_set_propertion(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
#ifdef CONFIG_NUMA
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    char *pro;
    visit_type_str(v, name, &pro, errp);
    backend->propertion = pro;
#else
    error_setg(errp, "NUMA node binding are not supported by this QEMU");
#endif
}
#endif

static int
host_memory_backend_get_policy(Object *obj, Error **errp G_GNUC_UNUSED)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    return backend->policy;
}

static void
host_memory_backend_set_policy(Object *obj, int policy, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    backend->policy = policy;

#ifndef CONFIG_NUMA
    if (policy != HOST_MEM_POLICY_DEFAULT) {
        error_setg(errp, "NUMA policies are not supported by this QEMU");
    }
#endif
}

static bool host_memory_backend_get_merge(Object *obj, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);

    return backend->merge;
}

static void host_memory_backend_set_merge(Object *obj, bool value, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);

    if (!host_memory_backend_mr_inited(backend)) {
        backend->merge = value;
        return;
    }

    if (value != backend->merge) {
        void *ptr = memory_region_get_ram_ptr(&backend->mr);
        uint64_t sz = memory_region_size(&backend->mr);

        qemu_madvise(ptr, sz,
                     value ? QEMU_MADV_MERGEABLE : QEMU_MADV_UNMERGEABLE);
        backend->merge = value;
    }
}

static bool host_memory_backend_get_dump(Object *obj, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);

    return backend->dump;
}

static void host_memory_backend_set_dump(Object *obj, bool value, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);

    if (!host_memory_backend_mr_inited(backend)) {
        backend->dump = value;
        return;
    }

    if (value != backend->dump) {
        void *ptr = memory_region_get_ram_ptr(&backend->mr);
        uint64_t sz = memory_region_size(&backend->mr);

        qemu_madvise(ptr, sz,
                     value ? QEMU_MADV_DODUMP : QEMU_MADV_DONTDUMP);
        backend->dump = value;
    }
}

static bool host_memory_backend_get_prealloc(Object *obj, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);

    return backend->prealloc;
}

static void host_memory_backend_set_prealloc(Object *obj, bool value,
                                             Error **errp)
{
    Error *local_err = NULL;
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);

    if (!backend->reserve && value) {
        error_setg(errp, "'prealloc=on' and 'reserve=off' are incompatible");
        return;
    }

    if (!host_memory_backend_mr_inited(backend)) {
        backend->prealloc = value;
        return;
    }

    if (value && !backend->prealloc) {
        int fd = memory_region_get_fd(&backend->mr);
        void *ptr = memory_region_get_ram_ptr(&backend->mr);
        uint64_t sz = memory_region_size(&backend->mr);

        qemu_prealloc_mem(fd, ptr, sz, backend->prealloc_threads,
                          backend->prealloc_context, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            return;
        }
        backend->prealloc = true;
    }
}

static void host_memory_backend_get_prealloc_threads(Object *obj, Visitor *v,
    const char *name, void *opaque, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    visit_type_uint32(v, name, &backend->prealloc_threads, errp);
}

static void host_memory_backend_set_prealloc_threads(Object *obj, Visitor *v,
    const char *name, void *opaque, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    uint32_t value;

    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }
    if (value <= 0) {
        error_setg(errp, "property '%s' of %s doesn't take value '%d'", name,
                   object_get_typename(obj), value);
        return;
    }
    backend->prealloc_threads = value;
}

static void host_memory_backend_init(Object *obj)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);
    MachineState *machine = MACHINE(qdev_get_machine());

    /* TODO: convert access to globals to compat properties */
    backend->merge = machine_mem_merge(machine);
    backend->dump = machine_dump_guest_core(machine);
    backend->reserve = true;
    backend->prealloc_threads = machine->smp.cpus;
}

static void host_memory_backend_post_init(Object *obj)
{
    object_apply_compat_props(obj);
}

bool host_memory_backend_mr_inited(HostMemoryBackend *backend)
{
    /*
     * NOTE: We forbid zero-length memory backend, so here zero means
     * "we haven't inited the backend memory region yet".
     */
    return memory_region_size(&backend->mr) != 0;
}

MemoryRegion *host_memory_backend_get_memory(HostMemoryBackend *backend)
{
    return host_memory_backend_mr_inited(backend) ? &backend->mr : NULL;
}

void host_memory_backend_set_mapped(HostMemoryBackend *backend, bool mapped)
{
    backend->is_mapped = mapped;
}

bool host_memory_backend_is_mapped(HostMemoryBackend *backend)
{
    return backend->is_mapped;
}

size_t host_memory_backend_pagesize(HostMemoryBackend *memdev)
{
    size_t pagesize = qemu_ram_pagesize(memdev->mr.ram_block);
    g_assert(pagesize >= qemu_real_host_page_size());
    return pagesize;
}

#ifdef CONFIG_MBIND_PROPORTION
static void mbind_by_proportions(void *ptr, const char *bind_proportions, uint64_t sz, Error **errp)
{
    char proportion_str[PROPORTION_MAX_LENGTH];
    char proportions[PROPORTION_MAX_NUM][PER_PROPORTION_MAX_LENGTH];
    int proportions_num, i;
    char *token;
    uint64_t size = 0;
    long mbind_ret = -1;
    uint64_t size_total = 0;

    if (strlen(bind_proportions) >= PROPORTION_MAX_LENGTH) {
        error_setg(errp, "Length of bind_proportions config is too long");
        return;
    }
    memcpy(proportion_str, bind_proportions, strlen(bind_proportions) + 1);
    proportions_num = 0;
    token = strtok(proportion_str, ":");
    while (token != NULL) {
        if (strlen(token) >= PER_PROPORTION_MAX_LENGTH) {
            error_setg(errp, "Length of bind_proportion token %zu is too long", strlen(token));
            return;
        }
        memcpy(proportions[proportions_num], token, strlen(token) + 1);
        proportions_num++;
        if (proportions_num >= PROPORTION_MAX_NUM) {
            error_setg(errp, "Invalid proportions number, max is %d", PROPORTION_MAX_NUM - 1);
            return;
        }
        token = strtok(NULL, ":");
    }

    for (i = 0; i < proportions_num; i++) {
        unsigned long tmp_lastbit, tmp_maxnode;
        char prop[PER_PROPORTION_MAX_LENGTH];
        char *end, *prop_token, *pos;
        long int node_id;
        long size_token;
        DECLARE_BITMAP(tmp_host_nodes, MAX_NODES + 1) = {0};

        ptr = (void*)((char *)ptr + size);
        if (strlen(proportions[i]) >= PER_PROPORTION_MAX_LENGTH) {
            error_setg(errp, "Length of bind_proportion token %zu is too long", strlen(proportions[i]));
            return;
        }
        memcpy(prop, proportions[i], strlen(proportions[i]) + 1);
        prop_token = strtok(prop, "-");
        if (prop_token == NULL) {
            error_setg(errp, "Invalid bind node and size config");
            return;
        }
        size_token = strtol(prop_token, &end, 10);
        if (*end != '\0') {
            error_setg(errp, "Invalid size format: '%s' (expected number)", prop_token);
            return;
        }
        size = size_token * MiB;
        size_total += size;
        prop_token = strtok(NULL, "-");
        if (prop_token == NULL) {
            error_setg(errp, "Missing node specification in token: %s", proportions[i]);
            return;
        }
        pos = strstr(prop_token, "node");
        if (pos == NULL) {
            error_setg(errp, "Invalid node info in token: %s", prop_token);
            return;
        }
        pos += strlen("node");
        node_id = strtol(pos, &end, 10);
        if (*end != '\0') {
            error_setg(errp, "Failed to convert node_id from string to number");
            return;
        }
        if (node_id < 0 || node_id > MAX_NODES) {
            error_setg(errp, "Invalid node id %ld", node_id);
            return;
        }
        bitmap_set(tmp_host_nodes, node_id, 1);
        tmp_lastbit = find_last_bit(tmp_host_nodes, MAX_NODES);
        tmp_maxnode = (tmp_lastbit + 1) % (MAX_NODES + 1);
        qemu_log("mbind args: addr %p, size  '%" PRIu64 "', node: %ld, \n", ptr, size, node_id);
        mbind_ret = mbind(ptr, size, MPOL_BIND, tmp_host_nodes, tmp_maxnode + 1,
            MPOL_MF_STRICT | MPOL_MF_MOVE);
        if (mbind_ret < 0) {
            error_setg_errno(errp, errno, "Failed to mbind address to host node");
            return;
        }
    }
    if (size_total != sz) {
        error_setg(errp, "Invalid propertion size config");
        return;
    }
    return;
}
#endif

static void
host_memory_backend_memory_complete(UserCreatable *uc, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(uc);
    HostMemoryBackendClass *bc = MEMORY_BACKEND_GET_CLASS(uc);
    Error *local_err = NULL;
    void *ptr;
    uint64_t sz;

    if (!bc->alloc) {
        return;
    }
    bc->alloc(backend, &local_err);
    if (local_err) {
        goto out;
    }

    ptr = memory_region_get_ram_ptr(&backend->mr);
    sz = memory_region_size(&backend->mr);

    if (backend->merge) {
        qemu_madvise(ptr, sz, QEMU_MADV_MERGEABLE);
    }
    if (!backend->dump) {
        qemu_madvise(ptr, sz, QEMU_MADV_DONTDUMP);
    }
#ifdef CONFIG_NUMA
    unsigned long lastbit = find_last_bit(backend->host_nodes, MAX_NODES);
    /* lastbit == MAX_NODES means maxnode = 0 */
    unsigned long maxnode = (lastbit + 1) % (MAX_NODES + 1);
    /* ensure policy won't be ignored in case memory is preallocated
     * before mbind(). note: MPOL_MF_STRICT is ignored on hugepages so
     * this doesn't catch hugepage case. */
    unsigned flags = MPOL_MF_STRICT | MPOL_MF_MOVE;
    int mode = backend->policy;
#ifdef CONFIG_MBIND_PROPORTION
    const char *proportion = backend->propertion;
    if (proportion != NULL) {
        mbind_by_proportions(ptr, proportion, sz, &local_err);
        if (local_err) {
            free(backend->propertion);
            goto out;
        }
        free(backend->propertion);
        goto prealloc;
    }
#endif

    /* check for invalid host-nodes and policies and give more verbose
     * error messages than mbind(). */
    if (maxnode && backend->policy == MPOL_DEFAULT) {
        error_setg(errp, "host-nodes must be empty for policy default,"
                   " or you should explicitly specify a policy other"
                   " than default");
        return;
    } else if (maxnode == 0 && backend->policy != MPOL_DEFAULT) {
        error_setg(errp, "host-nodes must be set for policy %s",
                   HostMemPolicy_str(backend->policy));
        return;
    }

    /* We can have up to MAX_NODES nodes, but we need to pass maxnode+1
     * as argument to mbind() due to an old Linux bug (feature?) which
     * cuts off the last specified node. This means backend->host_nodes
     * must have MAX_NODES+1 bits available.
     */
    assert(sizeof(backend->host_nodes) >=
           BITS_TO_LONGS(MAX_NODES + 1) * sizeof(unsigned long));
    assert(maxnode <= MAX_NODES);

#ifdef HAVE_NUMA_HAS_PREFERRED_MANY
    if (mode == MPOL_PREFERRED && numa_has_preferred_many() > 0) {
        /*
         * Replace with MPOL_PREFERRED_MANY otherwise the mbind() below
         * silently picks the first node.
         */
        mode = MPOL_PREFERRED_MANY;
    }
#endif

    if (maxnode &&
        mbind(ptr, sz, mode, backend->host_nodes, maxnode + 1, flags)) {
        if (backend->policy != MPOL_DEFAULT || errno != ENOSYS) {
            error_setg_errno(errp, errno,
                             "cannot bind memory to host NUMA nodes");
            return;
        }
    }
#endif
#ifdef CONFIG_MBIND_PROPORTION
prealloc:
#endif
    /* Preallocate memory after the NUMA policy has been instantiated.
     * This is necessary to guarantee memory is allocated with
     * specified NUMA policy in place.
     */
    if (backend->prealloc) {
        qemu_prealloc_mem(memory_region_get_fd(&backend->mr), ptr, sz,
                          backend->prealloc_threads,
                          backend->prealloc_context, &local_err);
        if (local_err) {
            goto out;
        }
    }
out:
    error_propagate(errp, local_err);
}

static bool
host_memory_backend_can_be_deleted(UserCreatable *uc)
{
    if (host_memory_backend_is_mapped(MEMORY_BACKEND(uc))) {
        return false;
    } else {
        return true;
    }
}

static bool host_memory_backend_get_share(Object *o, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(o);

    return backend->share;
}

static void host_memory_backend_set_share(Object *o, bool value, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(o);

    if (host_memory_backend_mr_inited(backend)) {
        error_setg(errp, "cannot change property value");
        return;
    }
    backend->share = value;
}

#ifdef CONFIG_LINUX
static bool host_memory_backend_get_reserve(Object *o, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(o);

    return backend->reserve;
}

static void host_memory_backend_set_reserve(Object *o, bool value, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(o);

    if (host_memory_backend_mr_inited(backend)) {
        error_setg(errp, "cannot change property value");
        return;
    }
    if (backend->prealloc && !value) {
        error_setg(errp, "'prealloc=on' and 'reserve=off' are incompatible");
        return;
    }
    backend->reserve = value;
}
#endif /* CONFIG_LINUX */

static bool
host_memory_backend_get_use_canonical_path(Object *obj, Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);

    return backend->use_canonical_path;
}

static void
host_memory_backend_set_use_canonical_path(Object *obj, bool value,
                                           Error **errp)
{
    HostMemoryBackend *backend = MEMORY_BACKEND(obj);

    backend->use_canonical_path = value;
}

static void
host_memory_backend_class_init(ObjectClass *oc, void *data)
{
    UserCreatableClass *ucc = USER_CREATABLE_CLASS(oc);

    ucc->complete = host_memory_backend_memory_complete;
    ucc->can_be_deleted = host_memory_backend_can_be_deleted;

    object_class_property_add_bool(oc, "merge",
        host_memory_backend_get_merge,
        host_memory_backend_set_merge);
    object_class_property_set_description(oc, "merge",
        "Mark memory as mergeable");
    object_class_property_add_bool(oc, "dump",
        host_memory_backend_get_dump,
        host_memory_backend_set_dump);
    object_class_property_set_description(oc, "dump",
        "Set to 'off' to exclude from core dump");
    object_class_property_add_bool(oc, "prealloc",
        host_memory_backend_get_prealloc,
        host_memory_backend_set_prealloc);
    object_class_property_set_description(oc, "prealloc",
        "Preallocate memory");
    object_class_property_add(oc, "prealloc-threads", "int",
        host_memory_backend_get_prealloc_threads,
        host_memory_backend_set_prealloc_threads,
        NULL, NULL);
    object_class_property_set_description(oc, "prealloc-threads",
        "Number of CPU threads to use for prealloc");
    object_class_property_add_link(oc, "prealloc-context",
        TYPE_THREAD_CONTEXT, offsetof(HostMemoryBackend, prealloc_context),
        object_property_allow_set_link, OBJ_PROP_LINK_STRONG);
    object_class_property_set_description(oc, "prealloc-context",
        "Context to use for creating CPU threads for preallocation");
    object_class_property_add(oc, "size", "int",
        host_memory_backend_get_size,
        host_memory_backend_set_size,
        NULL, NULL);
    object_class_property_set_description(oc, "size",
        "Size of the memory region (ex: 500M)");
    object_class_property_add(oc, "host-nodes", "int",
        host_memory_backend_get_host_nodes,
        host_memory_backend_set_host_nodes,
        NULL, NULL);
    object_class_property_set_description(oc, "host-nodes",
        "Binds memory to the list of NUMA host nodes");
#ifdef CONFIG_MBIND_PROPORTION
    object_class_property_add(oc, "host-nodes-propertion", "str",
        NULL,
        host_memory_backend_set_propertion,
        NULL, NULL);
    object_class_property_set_description(oc, "host-nodes-propertion",
            "Mark the memory bind to host node by propertion");
#endif
    object_class_property_add_enum(oc, "policy", "HostMemPolicy",
        &HostMemPolicy_lookup,
        host_memory_backend_get_policy,
        host_memory_backend_set_policy);
    object_class_property_set_description(oc, "policy",
        "Set the NUMA policy");
    object_class_property_add_bool(oc, "share",
        host_memory_backend_get_share, host_memory_backend_set_share);
    object_class_property_set_description(oc, "share",
        "Mark the memory as private to QEMU or shared");
#ifdef CONFIG_LINUX
    object_class_property_add_bool(oc, "reserve",
        host_memory_backend_get_reserve, host_memory_backend_set_reserve);
    object_class_property_set_description(oc, "reserve",
        "Reserve swap space (or huge pages) if applicable");
#endif /* CONFIG_LINUX */
    /*
     * Do not delete/rename option. This option must be considered stable
     * (as if it didn't have the 'x-' prefix including deprecation period) as
     * long as 4.0 and older machine types exists.
     * Option will be used by upper layers to override (disable) canonical path
     * for ramblock-id set by compat properties on old machine types ( <= 4.0),
     * to keep migration working when backend is used for main RAM with
     * -machine memory-backend= option (main RAM historically used prefix-less
     * ramblock-id).
     */
    object_class_property_add_bool(oc, "x-use-canonical-path-for-ramblock-id",
        host_memory_backend_get_use_canonical_path,
        host_memory_backend_set_use_canonical_path);
}

static const TypeInfo host_memory_backend_info = {
    .name = TYPE_MEMORY_BACKEND,
    .parent = TYPE_OBJECT,
    .abstract = true,
    .class_size = sizeof(HostMemoryBackendClass),
    .class_init = host_memory_backend_class_init,
    .instance_size = sizeof(HostMemoryBackend),
    .instance_init = host_memory_backend_init,
    .instance_post_init = host_memory_backend_post_init,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_USER_CREATABLE },
        { }
    }
};

static void register_types(void)
{
    type_register_static(&host_memory_backend_info);
}

type_init(register_types);
