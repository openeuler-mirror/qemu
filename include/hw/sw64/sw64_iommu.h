/*
 * Copyright (C) 2021-2025 Wuxi Institute of Advanced Technology
 * Written by Lu Feifei
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_SW64_IOMMU_H
#define HW_SW64_IOMMU_H

#include "hw/sysbus.h"
#include "hw/pci/pci.h"

#define TYPE_SW64_IOMMU_MEMORY_REGION "sw64-iommu-memory-region"
#define SW_IOMMU_ENTRY_VALID	((1UL) << 63)
#define SW_IOMMU_LEVEL1_OFFSET	0x1ff
#define SW_IOMMU_LEVEL2_OFFSET  0x3ff
#define SW_IOMMU_ENABLE		3
#define SW_IOMMU_GRN		((0UL) << 4)
#define SWVT_PCI_BUS_MAX	256

typedef struct SW64IOMMUClass SW64IOMMUClass;
typedef struct SW64IOMMUState SW64IOMMUState;
typedef struct SWVTAddressSpace SWVTAddressSpace;
typedef struct SW64DTIOTLBKey SW64DTIOTLBKey;
typedef struct SW64PTIOTLBKey SW64PTIOTLBKey;
typedef struct SW64DTIOTLBEntry SW64DTIOTLBEntry;
typedef struct SWVTBus SWVTBus;

struct SW64DTIOTLBEntry {
    uint16_t source_id;
    unsigned long ptbase_addr;
};

struct SW64DTIOTLBKey {
    uint16_t source_id;
};

struct SW64PTIOTLBKey {
    uint16_t source_id;
    dma_addr_t iova;
};

struct SWVTAddressSpace {
    PCIBus *bus;
    uint8_t devfn;
    AddressSpace as;
    IOMMUMemoryRegion iommu;
    MemoryRegion root;
    MemoryRegion msi;      /* Interrupt region: 0xfeeXXXXX */
    SW64IOMMUState *iommu_state;
    QLIST_ENTRY(SWVTAddressSpace) next;
    /* Superset of notifier flags that this address space has */
    IOMMUNotifierFlag notifier_flags;
};

struct SWVTBus {
    PCIBus* bus;                /* A reference to the bus to provide translation for */
    SWVTAddressSpace *dev_as[0]; /* A table of SWVTAddressSpace objects indexed by devfn */
};

struct SW64IOMMUState {
    SysBusDevice busdev;
    dma_addr_t dtbr;			/* Current root table pointer */
    GHashTable *dtiotlb;		/* IOTLB for device table */
    GHashTable *ptiotlb;		/* IOTLB for page table */

    GHashTable *swvtbus_as_by_busptr;
    /* list of registered notifiers */
    QLIST_HEAD(, SWVTAddressSpace) swvt_as_with_notifiers;

    PCIBus *pci_bus;
    QemuMutex iommu_lock;
};

struct SW64IOMMUClass {
    SysBusDeviceClass parent;
    DeviceRealize realize;
};

#define TYPE_SW64_IOMMU   "sw64-iommu"
#define SW64_IOMMU(obj) \
    OBJECT_CHECK(SW64IOMMUState, (obj), TYPE_SW64_IOMMU)
#define SW64_IOMMU_CLASS(klass)                              \
    OBJECT_CLASS_CHECK(SW64IOMMUClass, (klass), TYPE_SW64_IOMMU)
#define SW64_IOMMU_GET_CLASS(obj) \
     OBJECT_GET_CLASS(SW64IOMMUClass, (obj), TYPE_SW64_IOMMU)
extern void sw64_vt_iommu_init(PCIBus *b);
extern void swvt_address_space_invalidate_iova(SW64IOMMUState *s, unsigned long val);
extern void swvt_address_space_unmap_iova(SW64IOMMUState *s, unsigned long val);
extern void swvt_address_space_map_iova(SW64IOMMUState *s, unsigned long val);
extern SWVTAddressSpace *iommu_find_add_as(SW64IOMMUState *s, PCIBus *bus, int devfn);
extern MemTxResult msi_write(void *opaque, hwaddr addr, uint64_t value, unsigned size,
                              MemTxAttrs attrs);
#endif
