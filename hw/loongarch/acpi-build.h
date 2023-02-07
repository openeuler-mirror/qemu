/*
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

#ifndef HW_LARCH_ACPI_BUILD_H
#define HW_LARCH_ACPI_BUILD_H

#define EFI_ACPI_OEM_ID "LARCH"
#define EFI_ACPI_OEM_TABLE_ID "LARCH" /* OEM table id 8 bytes long */
#define EFI_ACPI_OEM_REVISION 0x00000002
#define EFI_ACPI_CREATOR_ID "LINUX"
#define EFI_ACPI_CREATOR_REVISION 0x01000013

#define ACPI_COMPATIBLE_1_0 0
#define ACPI_COMPATIBLE_2_0 1

void loongarch_acpi_setup(void);

#endif
