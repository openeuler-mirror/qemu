/*
 * QEMU sw64 IOMMU emulation
 *
 * Copyright (c) 2021 Lu Feifei
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/sw64/sw64_iommu.h"
#include "sysemu/kvm.h"

#define IOMMU_PAGE_SHIFT		13
#define IOMMU_PAGE_SIZE_8K		(1ULL << IOMMU_PAGE_SHIFT)
#define IOMMU_PAGE_MASK_8K		(~(IOMMU_PAGE_SIZE_8K - 1))
#define IOMMU_IOVA_SHIFT		16
#define SW64IOMMU_PTIOTLB_MAX_SIZE	256

static MemTxResult swvt_msi_read(void *opaque, hwaddr addr,
                                 uint64_t *data, unsigned size, MemTxAttrs attrs)
{
    return MEMTX_OK;
}

static MemTxResult swvt_msi_write(void *opaque, hwaddr addr,
                                      uint64_t value, unsigned size,
                                      MemTxAttrs attrs)
{
    MemTxResult ret;

    ret = msi_write(opaque, addr, value, size, attrs);

    return ret;
}

static const MemoryRegionOps swvt_msi_ops = {
    .read_with_attrs = swvt_msi_read,
    .write_with_attrs = swvt_msi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

SWVTAddressSpace *iommu_find_add_as(SW64IOMMUState *s, PCIBus *bus, int devfn)
{
    uintptr_t key = (uintptr_t)bus;
    SWVTBus *swvt_bus = g_hash_table_lookup(s->swvtbus_as_by_busptr, &key);
    SWVTAddressSpace *swvt_dev_as;
    char name[128];

    if (!swvt_bus) {
         uintptr_t *new_key = g_malloc(sizeof(*new_key));
         *new_key = (uintptr_t)bus;
         /* No corresponding free() */
         swvt_bus = g_malloc0(sizeof(SWVTBus) + sizeof(SWVTAddressSpace *) * \
                             PCI_DEVFN_MAX);
         swvt_bus->bus = bus;
         g_hash_table_insert(s->swvtbus_as_by_busptr, new_key, swvt_bus);
     }
     swvt_dev_as = swvt_bus->dev_as[devfn];
     if (!swvt_dev_as) {
         snprintf(name, sizeof(name), "sw64_iommu_devfn_%d", devfn);
         swvt_bus->dev_as[devfn] = swvt_dev_as = g_malloc0(sizeof(SWVTAddressSpace));

         swvt_dev_as->bus = bus;
         swvt_dev_as->devfn = (uint8_t)devfn;
         swvt_dev_as->iommu_state = s;

         memory_region_init_iommu(&swvt_dev_as->iommu, sizeof(swvt_dev_as->iommu),
                                  TYPE_SW64_IOMMU_MEMORY_REGION, OBJECT(s),
                                  "sw64_iommu_dmar",
                                  1UL << 32);
         memory_region_init_io(&swvt_dev_as->msi, OBJECT(s),
                                &swvt_msi_ops, s, "sw_msi", 1 * 1024 * 1024);
         memory_region_init(&swvt_dev_as->root, OBJECT(s),
                            "swvt_root", UINT64_MAX);
         memory_region_add_subregion_overlap(&swvt_dev_as->root,
                                              0x8000fee00000ULL,
                                              &swvt_dev_as->msi, 64);
         address_space_init(&swvt_dev_as->as, &swvt_dev_as->root, name);
         memory_region_add_subregion_overlap(&swvt_dev_as->root, 0,
                                              MEMORY_REGION(&swvt_dev_as->iommu),
                                              1);
     }

     memory_region_set_enabled(MEMORY_REGION(&swvt_dev_as->iommu), true);

     return swvt_dev_as;
}

/**
 * get_pte - Get the content of a page table entry located at
 * @base_addr[@index]
 */
static int get_pte(dma_addr_t baseaddr,  uint64_t *pte)
{
    int ret;

    /* TODO: guarantee 64-bit single-copy atomicity */
    ret = dma_memory_read(&address_space_memory, baseaddr,
                          (uint8_t *)pte, sizeof(*pte), MEMTXATTRS_UNSPECIFIED);

    if (ret != MEMTX_OK)
        return -EINVAL;

    return 0;
}

static bool swvt_do_iommu_translate(SWVTAddressSpace *swvt_as, PCIBus *bus,
                                    uint8_t devfn, hwaddr addr, IOMMUTLBEntry *entry)
{
    SW64IOMMUState *s = swvt_as->iommu_state;
    uint8_t bus_num = pci_bus_num(bus);
    unsigned long dtbbaseaddr, dtbbasecond;
    unsigned long pdebaseaddr, ptebaseaddr;
    unsigned long pte;
    uint16_t source_id;
    SW64DTIOTLBEntry *dtcached_entry = NULL;
    SW64DTIOTLBKey dtkey, *new_key;

    dtcached_entry = g_hash_table_lookup(s->dtiotlb, &dtkey);

    if (unlikely(!dtcached_entry)) {
        dtbbaseaddr = s->dtbr + (bus_num << 3);

        if (get_pte(dtbbaseaddr, &pte))
            goto error;

        dtbbasecond = (pte & (~(SW_IOMMU_ENTRY_VALID))) + (devfn << 3);
        if (get_pte(dtbbasecond, &pte))
            goto error;

	source_id = ((bus_num & 0xffUL) << 8) | (devfn & 0xffUL);
        dtcached_entry = g_new0(SW64DTIOTLBEntry, 1);
        dtcached_entry->ptbase_addr = pte & (~(SW_IOMMU_ENTRY_VALID));
        dtcached_entry->source_id = source_id;

        new_key = g_new0(SW64DTIOTLBKey, 1);
        new_key->source_id = source_id;

        g_hash_table_insert(s->dtiotlb, new_key, dtcached_entry);
    }

    pdebaseaddr = dtcached_entry->ptbase_addr;
    pdebaseaddr += ((addr >> 23) & SW_IOMMU_LEVEL1_OFFSET) << 3;

    if (get_pte(pdebaseaddr, &pte))
        goto error;

    ptebaseaddr = pte & (~(SW_IOMMU_ENTRY_VALID));
    ptebaseaddr += ((addr >> IOMMU_PAGE_SHIFT) & SW_IOMMU_LEVEL2_OFFSET) << 3;

    if (get_pte(ptebaseaddr, &pte))
        goto error;

    pte &= ~(SW_IOMMU_ENTRY_VALID | SW_IOMMU_GRN | SW_IOMMU_ENABLE);
    entry->translated_addr = pte;
    entry->addr_mask = IOMMU_PAGE_SIZE_8K - 1;

    return 0;

error:
    entry->perm = IOMMU_NONE;
    return -EINVAL;
}

static void swvt_ptiotlb_inv_all(SW64IOMMUState *s)
{
    g_hash_table_remove_all(s->ptiotlb);
}

static IOMMUTLBEntry *swvt_lookup_ptiotlb(SW64IOMMUState *s, uint16_t source_id,
                                hwaddr addr)
{
    SW64PTIOTLBKey ptkey;
    IOMMUTLBEntry *entry = NULL;

    ptkey.source_id = source_id;
    ptkey.iova = addr;

    entry = g_hash_table_lookup(s->ptiotlb, &ptkey);

    return entry;
}

static IOMMUTLBEntry sw64_translate_iommu(IOMMUMemoryRegion *iommu, hwaddr addr,
                                       IOMMUAccessFlags flag, int iommu_idx)
{
    SWVTAddressSpace *swvt_as = container_of(iommu, SWVTAddressSpace, iommu);
    SW64IOMMUState *s = swvt_as->iommu_state;
    IOMMUTLBEntry *cached_entry = NULL;
    IOMMUTLBEntry entry = {
         .target_as = &address_space_memory,
         .iova = addr,
         .translated_addr = addr,
         .addr_mask = ~(hwaddr)0,
         .perm = IOMMU_NONE,
    };
    uint8_t bus_num = pci_bus_num(swvt_as->bus);
    uint16_t source_id;
    SW64PTIOTLBKey *new_ptkey;
    hwaddr aligned_addr;

    source_id = ((bus_num & 0xffUL) << 8) | (swvt_as->devfn & 0xffUL);

    qemu_mutex_lock(&s->iommu_lock);

    aligned_addr = addr & IOMMU_PAGE_MASK_8K;

    cached_entry = swvt_lookup_ptiotlb(s, source_id, aligned_addr);

    if (cached_entry)
        goto out;

    if (g_hash_table_size(s->ptiotlb) >= SW64IOMMU_PTIOTLB_MAX_SIZE) {
        swvt_ptiotlb_inv_all(s);
    }

    cached_entry = g_new0(IOMMUTLBEntry, 1);

    if (swvt_do_iommu_translate(swvt_as, swvt_as->bus, swvt_as->devfn,
                                 addr, cached_entry)) {
        g_free(cached_entry);
        qemu_mutex_unlock(&s->iommu_lock);
        printf("%s: detected translation failure "
                           "(busnum=%d, devfn=%#x, iova=%#lx.\n",
                           __func__, pci_bus_num(swvt_as->bus), swvt_as->devfn,
                           entry.iova);
        entry.iova = 0;
        entry.translated_addr = 0;
        entry.addr_mask = 0;
        entry.perm = IOMMU_NONE;

        return entry;
    } else {
        new_ptkey = g_new0(SW64PTIOTLBKey, 1);
        new_ptkey->source_id = source_id;
        new_ptkey->iova = aligned_addr;
        g_hash_table_insert(s->ptiotlb, new_ptkey, cached_entry);
    }

out:
    qemu_mutex_unlock(&s->iommu_lock);
    entry.perm = flag;
    entry.translated_addr = cached_entry->translated_addr +
                                     (addr & (IOMMU_PAGE_SIZE_8K - 1));
    entry.addr_mask = cached_entry->addr_mask;

    return entry;
}

static void swvt_ptiotlb_inv_iova(SW64IOMMUState *s, uint16_t source_id, dma_addr_t iova)
{
    SW64PTIOTLBKey key = {.source_id = source_id, .iova = iova};

    qemu_mutex_lock(&s->iommu_lock);
    g_hash_table_remove(s->ptiotlb, &key);
    qemu_mutex_unlock(&s->iommu_lock);
}

void swvt_address_space_unmap_iova(SW64IOMMUState *s, unsigned long val)
{
    SWVTAddressSpace *swvt_as;
    IOMMUNotifier *n;
    uint16_t source_id;
    dma_addr_t iova;
    IOMMUTLBEvent event;

    source_id = val & 0xffff;
    iova = (val >> IOMMU_IOVA_SHIFT) << IOMMU_PAGE_SHIFT;

    swvt_ptiotlb_inv_iova(s, source_id, iova);

    QLIST_FOREACH(swvt_as, &s->swvt_as_with_notifiers, next) {
        uint8_t bus_num = pci_bus_num(swvt_as->bus);
        uint16_t as_sourceid = ((bus_num & 0xffUL) << 8) | (swvt_as->devfn & 0xffUL);

        if (as_sourceid == source_id) {
            IOMMU_NOTIFIER_FOREACH(n, &swvt_as->iommu) {
		event.type = IOMMU_NOTIFIER_UNMAP;
                event.entry.target_as = &address_space_memory;
                event.entry.iova = iova & IOMMU_PAGE_MASK_8K;
                event.entry.translated_addr = 0;
                event.entry.perm = IOMMU_NONE;
		event.entry.addr_mask = IOMMU_PAGE_SIZE_8K - 1;

                memory_region_notify_iommu(&swvt_as->iommu, 0, event);
            }
        }
    }
}

/* Unmap the whole range in the notifier's scope. */
static void swvt_address_space_unmap(SWVTAddressSpace *as, IOMMUNotifier *n)
{
    IOMMUTLBEvent event;
    hwaddr size;
    hwaddr start = n->start;
    hwaddr end = n->end;

    assert(start <= end);
    size = end - start;

    event.entry.target_as = &address_space_memory;
    /* Adjust iova for the size */
    event.entry.iova = n->start & ~(size - 1);
    /* This field is meaningless for unmap */
    event.entry.translated_addr = 0;
    event.entry.perm = IOMMU_NONE;
    event.entry.addr_mask = size - 1;

    memory_region_notify_iommu_one(n, &event);
}

void swvt_address_space_map_iova(SW64IOMMUState *s, unsigned long val)
{
    SWVTAddressSpace *swvt_as;
    IOMMUNotifier *n;
    uint16_t source_id;
    dma_addr_t iova;
    IOMMUTLBEvent event;
    int ret;

    source_id = val & 0xffff;
    iova = (val >> IOMMU_IOVA_SHIFT) << IOMMU_PAGE_SHIFT;

    swvt_ptiotlb_inv_iova(s, source_id, iova);

    QLIST_FOREACH(swvt_as, &s->swvt_as_with_notifiers, next) {
        uint8_t bus_num = pci_bus_num(swvt_as->bus);
        uint16_t as_sourceid = ((bus_num & 0xffUL) << 8) | (swvt_as->devfn & 0xffUL);

        if (as_sourceid == source_id) {
            IOMMU_NOTIFIER_FOREACH(n, &swvt_as->iommu) {
		event.type = IOMMU_NOTIFIER_UNMAP;
                event.entry.target_as = &address_space_memory;
                event.entry.iova = iova & IOMMU_PAGE_MASK_8K;
                event.entry.perm = IOMMU_RW;

                ret = swvt_do_iommu_translate(swvt_as, swvt_as->bus,
				              swvt_as->devfn, iova, &event.entry);
                if (ret)
                    goto out;

                memory_region_notify_iommu(&swvt_as->iommu, 0, event);
            }
        }
    }
out:
    return;
}

void swvt_address_space_invalidate_iova(SW64IOMMUState *s, unsigned long val)
{
    int map_flag;

    map_flag = val >> 36;

    if (map_flag)
        swvt_address_space_map_iova(s, val & 0xfffffffff);
    else
        swvt_address_space_unmap_iova(s, val);

    return;
}

static AddressSpace *sw64_dma_iommu(PCIBus *bus, void *opaque, int devfn)
{
    SW64IOMMUState *s = opaque;
    SWVTAddressSpace *swvt_as;

    assert(0 <= devfn && devfn < PCI_DEVFN_MAX);

    swvt_as = iommu_find_add_as(s, bus, devfn);
    return &swvt_as->as;
}

static uint64_t piu0_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t ret = 0;
    switch (addr) {
    default:
        break;
    }
    return ret;
}

static void piu0_write(void *opaque, hwaddr addr, uint64_t val,
                         unsigned size)
{
    SW64IOMMUState *s = (SW64IOMMUState *)opaque;

    switch (addr) {
    case 0xb000:
    /* DTBaseAddr */
        s->dtbr = val;
        break;
    case 0xb280:
    /* PTLB_FlushVAddr */
        swvt_address_space_invalidate_iova(s, val);
        break;
    default:
        break;
    }
}

const MemoryRegionOps core3_pci_piu0_ops = {
    .read = piu0_read,
    .write = piu0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
	.min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

void sw64_vt_iommu_init(PCIBus *b)
{
    DeviceState *dev_iommu;
    SW64IOMMUState *s;
    MemoryRegion *io_piu0 = g_new(MemoryRegion, 1);

    dev_iommu = qdev_new(TYPE_SW64_IOMMU);
    s = SW64_IOMMU(dev_iommu);

    s->pci_bus = b;
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev_iommu), &error_fatal);

    pci_setup_iommu(b, sw64_dma_iommu, dev_iommu);

    memory_region_init_io(io_piu0, OBJECT(s), &core3_pci_piu0_ops, s,
                          "pci0-piu0-io", 4 * 1024 * 1024);
    memory_region_add_subregion(get_system_memory(), 0x880200000000ULL,
                                 io_piu0);
}

static int swvt_iommu_notify_flag_changed(IOMMUMemoryRegion *iommu,
                                           IOMMUNotifierFlag old,
                                           IOMMUNotifierFlag new,
					   Error **errp)
{
    SWVTAddressSpace *swvt_as = container_of(iommu, SWVTAddressSpace, iommu);
    SW64IOMMUState *s = swvt_as->iommu_state;

    /* Update per-address-space notifier flags */
    swvt_as->notifier_flags = new;

    if (new & IOMMU_NOTIFIER_DEVIOTLB_UNMAP) {
        error_setg(errp, "swvt does not support dev-iotlb yet");
        return -EINVAL;
    }

    if (old == IOMMU_NOTIFIER_NONE) {
        QLIST_INSERT_HEAD(&s->swvt_as_with_notifiers, swvt_as, next);
    } else if (new == IOMMU_NOTIFIER_NONE) {
        QLIST_REMOVE(swvt_as, next);
    }
    return 0;
}

static void swvt_iommu_replay(IOMMUMemoryRegion *iommu_mr, IOMMUNotifier *n)
{
    SWVTAddressSpace *swvt_as = container_of(iommu_mr, SWVTAddressSpace, iommu);

    /*
     * The replay can be triggered by either a invalidation or a newly
     * created entry. No matter what, we release existing mappings
     * (it means flushing caches for UNMAP-only registers).
     */
    swvt_address_space_unmap(swvt_as, n);
}

/* GHashTable functions */
static gboolean swvt_uint64_equal(gconstpointer v1, gconstpointer v2)
{
    return *((const uint64_t *)v1) == *((const uint64_t *)v2);
}

static guint swvt_uint64_hash(gconstpointer v)
{
    return (guint)*(const uint64_t *)v;
}

static void iommu_realize(DeviceState *d, Error **errp)
{
    SW64IOMMUState *s = SW64_IOMMU(d);

    QLIST_INIT(&s->swvt_as_with_notifiers);
    qemu_mutex_init(&s->iommu_lock);

    s->dtiotlb = g_hash_table_new_full(swvt_uint64_hash, swvt_uint64_equal,
				     g_free, g_free);
    s->ptiotlb = g_hash_table_new_full(swvt_uint64_hash, swvt_uint64_equal,
                                      g_free, g_free);

    s->swvtbus_as_by_busptr = g_hash_table_new(NULL, NULL);
}

static void iommu_reset(DeviceState *d)
{
}

static void sw64_iommu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = iommu_reset;
    dc->realize = iommu_realize;
}

static void sw64_iommu_memory_region_class_init(ObjectClass *klass, void *data)
{
    IOMMUMemoryRegionClass *imrc = IOMMU_MEMORY_REGION_CLASS(klass);

    imrc->translate = sw64_translate_iommu;
    imrc->notify_flag_changed = swvt_iommu_notify_flag_changed;
    imrc->replay = swvt_iommu_replay;
}

static const TypeInfo sw64_iommu_info = {
    .name          = TYPE_SW64_IOMMU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SW64IOMMUState),
    .class_init    = sw64_iommu_class_init,
    .class_size    = sizeof(SW64IOMMUClass),
};

static const TypeInfo sw64_iommu_memory_region_info = {
    .parent = TYPE_IOMMU_MEMORY_REGION,
    .name = TYPE_SW64_IOMMU_MEMORY_REGION,
    .class_init = sw64_iommu_memory_region_class_init,
};

static void sw64_iommu_register_types(void)
{
    type_register_static(&sw64_iommu_info);
    type_register_static(&sw64_iommu_memory_region_info);
}

type_init(sw64_iommu_register_types)
