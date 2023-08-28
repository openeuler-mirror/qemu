/*
 * Acpi emulation on Loongarch system.
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

#ifndef HW_LS7A_H
#define HW_LS7A_H

#include "hw/hw.h"
#include "hw/isa/isa.h"
#include "hw/sysbus.h"
#include "hw/isa/apm.h"
#include "hw/pci/pci.h"
#include "hw/pci/pcie_host.h"
#include "hw/pci/pci_bridge.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/ls7a.h"
#include "hw/pci/pci_bus.h"

/* LS7A PCH Registers (Misc, Confreg) */
#define LS7A_PCH_REG_BASE 0x10000000UL
#define LS3A5K_LS7A_IOAPIC_REG_BASE (LS7A_PCH_REG_BASE)
#define LS7A_MISC_REG_BASE (LS7A_PCH_REG_BASE + 0x00080000)
#define LS7A_ACPI_REG_BASE (LS7A_MISC_REG_BASE + 0x00050000)

#define LOONGARCH_PCH_IRQ_BASE 64
#define LS7A_UART_IRQ (LOONGARCH_PCH_IRQ_BASE + 2)
#define LS7A_RTC_IRQ (LOONGARCH_PCH_IRQ_BASE + 3)
#define LS7A_SCI_IRQ (LOONGARCH_PCH_IRQ_BASE + 4)
#define LS7A_ACPI_IO_BASE 0x800
#define LS7A_ACPI_IO_SIZE 0x100
#define LS7A_PM_EVT_BLK (0x0C)     /* 4 bytes */
#define LS7A_PM_CNT_BLK (0x14)     /* 2 bytes */
#define LS7A_GPE0_STS_REG (0x28)   /* 4 bytes */
#define LS7A_GPE0_ENA_REG (0x2C)   /* 4 bytes */
#define LS7A_GPE0_RESET_REG (0x30) /* 4 bytes */
#define LS7A_PM_TMR_BLK (0x18)     /* 4 bytes */
#define LS7A_GPE0_LEN (8)
#define LS7A_RTC_REG_BASE (LS7A_MISC_REG_BASE + 0x00050100)
#define LS7A_RTC_LEN (0x100)

#define ACPI_IO_BASE (LS7A_ACPI_REG_BASE)
#define ACPI_GPE0_LEN (LS7A_GPE0_LEN)
#define ACPI_IO_SIZE (LS7A_ACPI_IO_SIZE)
#define ACPI_SCI_IRQ (LS7A_SCI_IRQ)

#define VIRT_PLATFORM_BUS_BASEADDRESS 0x16000000
#define VIRT_PLATFORM_BUS_SIZE 0x02000000
#define VIRT_PLATFORM_BUS_NUM_IRQS 2
#define VIRT_PLATFORM_BUS_IRQ (LOONGARCH_PCH_IRQ_BASE + 5)

#define LS3A5K_ISA_IO_BASE 0x18000000UL
#define LS_BIOS_BASE 0x1c000000
#define LS_BIOS_VAR_BASE 0x1c3a0000
#define LS_BIOS_SIZE (4 * 1024 * 1024)
#define LS_FDT_BASE 0x1c400000
#define LS_FDT_SIZE 0x00100000

#define FW_CFG_ADDR 0x1e020000
#define LS7A_REG_BASE 0x1FE00000
#define LS7A_UART_BASE 0x1fe001e0
#define LS7A_UART_LEN 0x8
#define SMP_GIPI_MAILBOX 0x1f000000ULL
#define CORE0_STATUS_OFF 0x000
#define CORE0_EN_OFF 0x004
#define CORE0_SET_OFF 0x008
#define CORE0_CLEAR_OFF 0x00c
#define CORE0_BUF_20 0x020
#define CORE0_BUF_28 0x028
#define CORE0_BUF_30 0x030
#define CORE0_BUF_38 0x038
#define CORE0_IPI_SEND 0x040
#define CORE0_MAIL_SEND 0x048
#define INT_ROUTER_REGS_BASE 0x1fe01400UL
#define INT_ROUTER_REGS_SIZE 0x100
#define INT_ROUTER_REGS_SYS_INT0 0x00
#define INT_ROUTER_REGS_SYS_INT1 0x01
#define INT_ROUTER_REGS_SYS_INT2 0x02
#define INT_ROUTER_REGS_SYS_INT3 0x03
#define INT_ROUTER_REGS_PCI_INT0 0x04
#define INT_ROUTER_REGS_PCI_INT1 0x05
#define INT_ROUTER_REGS_PCI_INT2 0x06
#define INT_ROUTER_REGS_PCI_INT3 0x07
#define INT_ROUTER_REGS_MATRIX_INT0 0x08
#define INT_ROUTER_REGS_MATRIX_INT1 0x09
#define INT_ROUTER_REGS_LPC_INT 0x0a
#define INT_ROUTER_REGS_MC0 0x0b
#define INT_ROUTER_REGS_MC1 0x0c
#define INT_ROUTER_REGS_BARRIER 0x0d
#define INT_ROUTER_REGS_THSENS_INT 0x0e
#define INT_ROUTER_REGS_PCI_PERR 0x0f
#define INT_ROUTER_REGS_HT0_INT0 0x10
#define INT_ROUTER_REGS_HT0_INT1 0x11
#define INT_ROUTER_REGS_HT0_INT2 0x12
#define INT_ROUTER_REGS_HT0_INT3 0x13
#define INT_ROUTER_REGS_HT0_INT4 0x14
#define INT_ROUTER_REGS_HT0_INT5 0x15
#define INT_ROUTER_REGS_HT0_INT6 0x16
#define INT_ROUTER_REGS_HT0_INT7 0x17
#define INT_ROUTER_REGS_HT1_INT0 0x18
#define INT_ROUTER_REGS_HT1_INT1 0x19
#define INT_ROUTER_REGS_HT1_INT2 0x1a
#define INT_ROUTER_REGS_HT1_INT3 0x1b
#define INT_ROUTER_REGS_HT1_INT4 0x1c
#define INT_ROUTER_REGS_HT1_INT5 0x1d
#define INT_ROUTER_REGS_HT1_INT6 0x1e
#define INT_ROUTER_REGS_HT1_INT7 0x1f
#define INT_ROUTER_REGS_ISR 0x20
#define INT_ROUTER_REGS_EN 0x24
#define INT_ROUTER_REGS_EN_SET 0x28
#define INT_ROUTER_REGS_EN_CLR 0x2c
#define INT_ROUTER_REGS_EDGE 0x38
#define INT_ROUTER_REGS_CORE0_INTISR 0x40
#define INT_ROUTER_REGS_CORE1_INTISR 0x48
#define INT_ROUTER_REGS_CORE2_INTISR 0x50
#define INT_ROUTER_REGS_CORE3_INTISR 0x58

#define LS_PCIECFG_BASE 0x20000000
#define LS_PCIECFG_SIZE 0x08000000
#define MSI_ADDR_LOW 0x2FF00000
#define MSI_ADDR_HI 0x0

#define PCIE_MEMORY_BASE 0x40000000
#define PCIE_MEMORY_SIZE 0x40000000

typedef struct LS7APCIState LS7APCIState;
typedef struct LS7APCIEHost {
    PCIExpressHost parent_obj;
    MemoryRegion io_ioport;
    MemoryRegion io_mmio;
    LS7APCIState *pci_dev;
} LS7APCIEHost;

struct LS7APCIState {
    PCIDevice dev;

    LS7APCIEHost *pciehost;

    /* LS7A registers */
    MemoryRegion iomem;
    LS7APCIPMRegs pm;
};

#define TYPE_LS7A_PCIE_HOST_BRIDGE "ls7a1000-pciehost"
#define LS7A_PCIE_HOST_BRIDGE(obj)                                            \
    OBJECT_CHECK(LS7APCIEHost, (obj), TYPE_LS7A_PCIE_HOST_BRIDGE)

#define TYPE_PCIE_LS7A "ls7a1000_pcie"
#define PCIE_LS7A(obj) OBJECT_CHECK(LS7APCIState, (obj), TYPE_PCIE_LS7A)

PCIBus *ls7a_init(MachineState *machine, qemu_irq *irq,
                  DeviceState **ls7a_dev);
LS7APCIState *get_ls7a_type(Object *obj);

#endif /* HW_LS7A_H */
