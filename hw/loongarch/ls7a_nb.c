/*
 * Loongarch 7A1000 north bridge support
 *
 * Copyright (c) 2023 Loongarch Technology
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"

#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/pci/pci.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/pcie_host.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "qapi/error.h"
#include "hw/loongarch/cpudevs.h"
#include "hw/acpi/ls7a.h"
#include "hw/i386/pc.h"
#include "hw/isa/isa.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "hw/loongarch/bios.h"
#include "hw/loader.h"
#include "elf.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci/pci_bus.h"
#include "linux/kvm.h"
#include "sysemu/kvm.h"
#include "sysemu/runstate.h"
#include "sysemu/reset.h"
#include "migration/vmstate.h"
#include "hw/loongarch/larch.h"
#include "hw/loongarch/ls7a.h"

#undef DEBUG_LS7A

#ifdef DEBUG_LS7A
#define DPRINTF(fmt, ...) fprintf(stderr, "%s: " fmt, __func__, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

static void ls7a_reset(void *opaque)
{
    uint64_t wmask;
    wmask = ~(-1);

    PCIDevice *dev = opaque;
    pci_set_word(dev->config + PCI_VENDOR_ID, 0x0014);
    pci_set_word(dev->wmask + PCI_VENDOR_ID, wmask & 0xffff);
    pci_set_word(dev->cmask + PCI_VENDOR_ID, 0xffff);
    pci_set_word(dev->config + PCI_DEVICE_ID, 0x7a00);
    pci_set_word(dev->wmask + PCI_DEVICE_ID, wmask & 0xffff);
    pci_set_word(dev->cmask + PCI_DEVICE_ID, 0xffff);
    pci_set_word(dev->config + 0x4, 0x0000);
    pci_set_word(dev->config + PCI_STATUS, 0x0010);
    pci_set_word(dev->wmask + PCI_STATUS, wmask & 0xffff);
    pci_set_word(dev->cmask + PCI_STATUS, 0xffff);
    pci_set_byte(dev->config + PCI_REVISION_ID, 0x0);
    pci_set_byte(dev->wmask + PCI_REVISION_ID, wmask & 0xff);
    pci_set_byte(dev->cmask + PCI_REVISION_ID, 0xff);
    pci_set_byte(dev->config + 0x9, 0x00);
    pci_set_byte(dev->wmask + 0x9, wmask & 0xff);
    pci_set_byte(dev->cmask + 0x9, 0xff);
    pci_set_byte(dev->config + 0xa, 0x00);
    pci_set_byte(dev->wmask + 0xa, wmask & 0xff);
    pci_set_byte(dev->cmask + 0xa, 0xff);
    pci_set_byte(dev->config + 0xb, 0x06);
    pci_set_byte(dev->wmask + 0xb, wmask & 0xff);
    pci_set_byte(dev->cmask + 0xb, 0xff);
    pci_set_byte(dev->config + 0xc, 0x00);
    pci_set_byte(dev->wmask + 0xc, wmask & 0xff);
    pci_set_byte(dev->cmask + 0xc, 0xff);
    pci_set_byte(dev->config + 0xe, 0x80);
    pci_set_byte(dev->wmask + 0xe, wmask & 0xff);
    pci_set_byte(dev->cmask + 0xe, 0xff);
}

static const VMStateDescription vmstate_ls7a_pcie = {
    .name = "LS7A_PCIE",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){ VMSTATE_PCI_DEVICE(dev, LS7APCIState),
                                VMSTATE_STRUCT(pm, LS7APCIState, 0,
                                               vmstate_ls7a_pm, LS7APCIPMRegs),
                                VMSTATE_END_OF_LIST() }
};

static PCIINTxRoute ls7a_route_intx_pin_to_irq(void *opaque, int pin)
{
    PCIINTxRoute route;

    route.irq = pin;
    route.mode = PCI_INTX_ENABLED;
    return route;
}

static int pci_ls7a_map_irq(PCIDevice *d, int irq_num)
{
    int irq;

    irq = 16 + ((PCI_SLOT(d->devfn) * 4 + irq_num) & 0xf);
    return irq;
}

static void pci_ls7a_set_irq(void *opaque, int irq_num, int level)
{
    qemu_irq *pic = opaque;
    DPRINTF("------ %s irq %d %d\n", __func__, irq_num, level);
    qemu_set_irq(pic[irq_num], level);
}

static void ls7a_pcie_realize(PCIDevice *dev, Error **errp)
{
    LS7APCIState *s = PCIE_LS7A(dev);
    /* Ls7a North Bridge, built on FPGA, VENDOR_ID/DEVICE_ID are "undefined" */
    pci_config_set_prog_interface(dev->config, 0x00);

    /* set the default value of north bridge pci config */
    qemu_register_reset(ls7a_reset, s);
}

static AddressSpace *ls7a_pci_dma_iommu(PCIBus *bus, void *opaque, int devfn)
{
    return &address_space_memory;
}

static PCIBus *pci_ls7a_init(MachineState *machine, DeviceState *dev,
                             qemu_irq *pic)
{
    LoongarchMachineState *lsms = LoongarchMACHINE(machine);
    LoongarchMachineClass *lsmc = LoongarchMACHINE_GET_CLASS(lsms);
    LS7APCIEHost *pciehost = LS7A_PCIE_HOST_BRIDGE(dev);
    PCIExpressHost *e;
    SysBusDevice *sysbus;
    PCIHostState *phb;
    MemoryRegion *mmio_alias;

    e = PCIE_HOST_BRIDGE(dev);
    sysbus = SYS_BUS_DEVICE(e);
    phb = PCI_HOST_BRIDGE(e);

    sysbus_init_mmio(sysbus, &e->mmio);

    memory_region_init(&pciehost->io_mmio, OBJECT(pciehost),
                       "pciehost-mmio", UINT64_MAX);
    sysbus_init_mmio(sysbus, &pciehost->io_mmio);
    mmio_alias = g_new0(MemoryRegion, 1);
    memory_region_init_alias(mmio_alias, OBJECT(dev), "pcie-mmio",
                             &pciehost->io_mmio, PCIE_MEMORY_BASE,
                             PCIE_MEMORY_SIZE);
    memory_region_add_subregion(get_system_memory(),
                                PCIE_MEMORY_BASE, mmio_alias);

    memory_region_init(&pciehost->io_ioport, OBJECT(pciehost),
                       "pciehost-ioport", LS_ISA_IO_SIZE);
    sysbus_init_mmio(sysbus, &pciehost->io_ioport);

    sysbus_mmio_map(sysbus, 2, LS3A5K_ISA_IO_BASE);


    phb->bus = pci_register_root_bus(dev, "pcie.0", pci_ls7a_set_irq,
                                     pci_ls7a_map_irq, pic,
                                     &pciehost->io_mmio, &pciehost->io_ioport,
                                     (1 << 3), 128, TYPE_PCIE_BUS);
    /*update pcie config memory*/
    pcie_host_mmcfg_update(e, true, lsmc->pciecfg_base, LS_PCIECFG_SIZE);

    pci_bus_set_route_irq_fn(phb->bus, ls7a_route_intx_pin_to_irq);

    return phb->bus;
}

PCIBus *ls7a_init(MachineState *machine, qemu_irq *pic, DeviceState **ls7a_dev)
{
    DeviceState *dev;
    PCIHostState *phb;
    LS7APCIState *pbs;
    PCIDevice *pcid;
    PCIBus *pci_bus;
    PCIExpressHost *e;

    /*1. init the HT PCI CFG*/
    DPRINTF("------ %d\n", __LINE__);
    dev = qdev_new(TYPE_LS7A_PCIE_HOST_BRIDGE);
    e = PCIE_HOST_BRIDGE(dev);
    phb = PCI_HOST_BRIDGE(e);

    DPRINTF("------ %d\n", __LINE__);
    pci_bus = pci_ls7a_init(machine, dev, pic);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    phb->bus = pci_bus;
    /* set the pcihost pointer after rs780_pcihost_initfn is called */
    DPRINTF("------ %d\n", __LINE__);
    pcid = pci_new(PCI_DEVFN(0, 0), TYPE_PCIE_LS7A);
    pbs = PCIE_LS7A(pcid);
    pbs->pciehost = LS7A_PCIE_HOST_BRIDGE(dev);
    pbs->pciehost->pci_dev = pbs;

    if (ls7a_dev) {
        *ls7a_dev = DEVICE(pcid);
    }

    pci_realize_and_unref(pcid, phb->bus, &error_fatal);

    /* IOMMU */
    pci_setup_iommu(phb->bus, ls7a_pci_dma_iommu, NULL);

    ls7a_pm_init(&pbs->pm, pic);
    DPRINTF("------ %d\n", __LINE__);
    /*3. init the north bridge VGA,not do now*/
    return pci_bus;
}

LS7APCIState *get_ls7a_type(Object *obj)
{
    LS7APCIState *pbs;

    pbs = PCIE_LS7A(obj);
    return pbs;
}

static void ls7a_pcie_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    HotplugHandlerClass *hc = HOTPLUG_HANDLER_CLASS(klass);
    AcpiDeviceIfClass *adevc = ACPI_DEVICE_IF_CLASS(klass);

    k->realize = ls7a_pcie_realize;
    k->vendor_id = 0x0014;
    k->device_id = 0x7a00;
    k->revision = 0x00;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    dc->desc = "LS7A1000 PCIE Host bridge";
    dc->vmsd = &vmstate_ls7a_pcie;
    /*
     * PCI-facing part of the host bridge, not usable without the
     * host-facing part, which can't be device_add'ed, yet.
     */
    dc->user_creatable = false;
    hc->plug = ls7a_pm_device_plug_cb;
    hc->unplug_request = ls7a_pm_device_unplug_request_cb;
    hc->unplug = ls7a_pm_device_unplug_cb;
    adevc->ospm_status = ls7a_pm_ospm_status;
    adevc->send_event = ls7a_send_gpe;
    adevc->madt_cpu = ls7a_madt_cpu_entry;
}

static void ls7a_pci_add_properties(LS7APCIState *ls7a)
{
    ls7a_pm_add_properties(OBJECT(ls7a), &ls7a->pm, NULL);
}

static void ls7a_pci_initfn(Object *obj)
{
    LS7APCIState *ls7a = get_ls7a_type(obj);

    ls7a_pci_add_properties(ls7a);
}

static const TypeInfo ls7a_pcie_device_info = {
    .name = TYPE_PCIE_LS7A,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(LS7APCIState),
    .class_init = ls7a_pcie_class_init,
    .instance_init = ls7a_pci_initfn,
    .interfaces =
        (InterfaceInfo[]){
            { TYPE_HOTPLUG_HANDLER },
            { TYPE_ACPI_DEVICE_IF },
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            {},
        },
};

static void ls7a_pciehost_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    k->parent_class.fw_name = "pci";
}

static const TypeInfo ls7a_pciehost_info = {
    .name = TYPE_LS7A_PCIE_HOST_BRIDGE,
    .parent = TYPE_PCIE_HOST_BRIDGE,
    .instance_size = sizeof(LS7APCIEHost),
    .class_init = ls7a_pciehost_class_init,
};

static void ls7a_register_types(void)
{
    type_register_static(&ls7a_pciehost_info);
    type_register_static(&ls7a_pcie_device_info);
}

type_init(ls7a_register_types)
