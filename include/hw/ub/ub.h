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

#ifndef UB_H
#define UB_H
#include <linux/vfio.h>
#include "qemu/typedefs.h"
#include "exec/memory.h"
#include "sysemu/host_iommu_device.h"

#define BYTE_SIZE 1
#define WORD_SIZE 2
#define DWORD_SIZE 4

#define UINT16_MASK 0x0000FFFF

#define UB_DEV_NAME_LEN 64
#define UB_NUM_REGIONS (VFIO_UB_NUM_REGIONS - 1) /* Exclude the config region */
#define UB_SUPPORT_MIN_EID 1
#define UB_SUPPORT_MAX_EID 0xFFFFF
#define UB_GUID_BASE_CODE_MASK 0x00FF

typedef struct UBIORegion {
    uint64_t addr; /* current UB mapping address. -1 means not mapped */
#define UB_ER_UNMAPPED (~(uint64_t)0)
    uint64_t size;
    MemoryRegion *memory;
    MemoryRegion *address_space;
} UBIORegion;

#define GUID_STR_EXAMPLE "e0fc-a120-0-2-000000-0000000000000000" \
        "(Vendor-DeviceId-Version-Type-Rsv-SequenceNumber)"
typedef struct __attribute__ ((__packed__)) UbGuid {
    unsigned long seq_num : 64;
    unsigned long rsv : 24;
    unsigned int type : 4;
    unsigned int version : 4;
    unsigned int device_id : 16;
    unsigned int vendor : 16;
} UbGuid;
bool ub_guid_initialized(UbGuid *guid);
#define UB_DEV_GUID_STRING_LENGTH   37
void ub_device_get_str_from_guid(UbGuid *guid, char *guid_str, uint32_t str_len);
bool ub_device_get_guid_from_str(UbGuid *guid, char *guid_str);

typedef struct UBHostDeviceAddress {
    UbGuid guid;
} UBHostDeviceAddress;

enum UbGUIDType {
    UB_GUID_TYPE_UNINIT = -1,
    UB_GUID_TYPE_BUS_INSTANCE = 0x0,
    UB_GUID_TYPE_BUS_CONTROLLER = 0x1,
    UB_GUID_TYPE_IBUS_CONTROLLER = 0x2,
    UB_GUID_TYPE_SWITCH = 0x3,
    UB_GUID_TYPE_ISWITCH = 0x4,
};

enum UbGUIDBaseCode {
    UB_GUID_BASE_INSTANCE = 0,
    UB_GUID_BASE_SWITCH = 4,
};

enum UbDeviceType {
    UB_TYPE_UNINIT = -1,
    UB_TYPE_BUS_INSTANCE,
    UB_TYPE_DEVICE,
    UB_TYPE_IDEVICE,
    UB_TYPE_SWITCH,
    UB_TYPE_ISWITCH,
    UB_TYPE_IBUS_CONTROLLER,
};

static inline const char *ub_dev_get_type_str(enum UbDeviceType type)
{
    switch (type) {
    case UB_TYPE_UNINIT:
        return "type_uninit";
    case UB_TYPE_BUS_INSTANCE:
        return "type_businstance";
    case UB_TYPE_DEVICE:
        return "type_device";
    case UB_TYPE_IDEVICE:
        return "type_idevice";
    case UB_TYPE_SWITCH:
        return "type_switch";
    case UB_TYPE_ISWITCH:
        return "type_iswitch";
    case UB_TYPE_IBUS_CONTROLLER:
        return "type_ibus_controller";
    default:
        return "type_unknown";
    }
}

/*
 * the reserved address space in config space supports a maximum of 4094 ports,
 * current ubus driver support max 256 ports.
 * */
#define UB_DEV_MAX_NUM_OF_PORT                  256
#define UB_DEV_CONFIG_SPACE_PORT_SIZE           0x40000UL     // 256KiB
#define UB_DEV_NUM_OF_CFG                       0x2UL
#define UB_DEV_CONFIG_SPACE_CFG_SIZE            0x40000UL     // 256KiB
#define UB_DEV_CONFIG_SPACE_ROUTE_TABLE_SIZE    0x40000000UL  // 1GiB
#define UB_DEV_CONFIG_SPACE_ROUTE_TABLE_START   0x3C0000000UL  // 1GiB
#define UB_DEV_CONFIG_SPACE_TOTAL_SIZE \
    (UB_DEV_CONFIG_SPACE_ROUTE_TABLE_START + UB_DEV_CONFIG_SPACE_ROUTE_TABLE_SIZE) // according to frontend code

#define UB_DEV_ID_LEN 64
typedef struct NeighborInfo {
    union {
        char neighbor_id[UB_DEV_ID_LEN];
        UBDevice *neighbor_dev;
    };
    uint32_t local_port_idx;
    uint32_t neighbor_port_idx;
} NeighborInfo;

typedef struct UbPortInfo {
    uint32_t port_num;
    char *neighbors_cmd;
    NeighborInfo *neighbors;
    bool port_info_exist;
} UbPortInfo;

typedef int UBConfigReadFunc(UBDevice *dev, uint64_t offset,
                              uint32_t *val, uint32_t dw_mask);
typedef int UBConfigWriteFunc(UBDevice *dev, uint64_t offset,
                               uint32_t *val, uint32_t dw_mask);
typedef int (*USIVectorUseNotifier)(UBDevice *udev, uint16_t vector, USIMessage msg);
typedef void (*USIVectorReleaseNotifier)(UBDevice *udev, uint16_t vector);
typedef void (*USIVectorPollNotifier)(UBDevice *dev, uint16_t vector_start, uint16_t vector_end);

struct UBDevice {
    DeviceState qdev;
    /* UB config space */
    uint8_t *config;
    /* UB config space right mask */
    uint8_t *wmask;
    uint8_t *w1cmask;
    enum UbDeviceType dev_type;
    char name[UB_DEV_NAME_LEN];
    uint32_t eid;
    uint32_t bus_instance_eid;
    uint32_t cna;
    uint32_t ue_idx;
    uint32_t rst_cnt;
    bool host_dev;
    UbGuid guid;
    UbPortInfo port;
    UBIORegion io_regions[UB_NUM_REGIONS];
    UBConfigReadFunc *config_read;
    UBConfigWriteFunc *config_write;
    int (* bus_instance_verify)(UBDevice *dev, Error **errp);

    /* usi entries */
    uint16_t usi_entries_nr;
    uint16_t usi_addr_table_nr;
    /* Space to store usi vec table & addr table & pending bit array */
    uint8_t *usi_vec_table;
    uint8_t *usi_addr_table;
    uint8_t *usi_pend_table;
    /* MemoryRegion container for usi vec table & addr table & pending bit array */
    MemoryRegion usi_vec_table_mmio;
    MemoryRegion usi_addr_table_mmio;
    MemoryRegion usi_pend_table_mmio;
    /* USI notifiers */
    USIVectorUseNotifier usi_vector_use_notifier;
    USIVectorReleaseNotifier usi_vector_release_notifier;
    USIVectorPollNotifier usi_vector_poll_notifier;

    QLIST_ENTRY(UBDevice) node;
};

typedef void UBUnregisterFunc(UBDevice *dev);

typedef struct UBDeviceClass {
    DeviceClass parent_class;

    void (*realize)(UBDevice *dev, Error **errp);
    UBUnregisterFunc *exit;
    UBConfigReadFunc *config_read;
    UBConfigWriteFunc *config_write;
} UBDeviceClass;

#define TYPE_UB_DEVICE "ub-device"
DECLARE_OBJ_CHECKERS(UBDevice, UBDeviceClass,
                     UB_DEVICE, TYPE_UB_DEVICE)

typedef struct UBIOMMUOps {
    /**
     * @get_address_space: get the address space for a set of devices
     * on a UB bus.
     *
     * Mandatory callback which returns a pointer to an #AddressSpace
     *
     * @bus: the #UBBus being accessed.
     *
     * @opaque: the data passed to ub_setup_iommu().
     *
     * @eid: ub device eid
     */
    AddressSpace * (*get_address_space)(UBBus *bus, void *opaque, uint32_t eid);
    bool (*set_iommu_device)(UBBus *bus, void *opaque, uint32_t eid,
                            HostIOMMUDevice *dev, Error **errp);
    void (*unset_iommu_device)(UBBus *bus, void *opaque, uint32_t eid);
    bool (*ummu_is_nested)(void *opaque);
} UBIOMMUOps;

static inline void ub_set_byte(uint8_t *config, uint8_t val)
{
    *config = val;
}

static inline uint8_t ub_get_byte(const uint8_t *config)
{
    return *config;
}

static inline void ub_set_word(uint8_t *config, uint16_t val)
{
    stw_le_p(config, val);
}

static inline uint16_t ub_get_word(const uint8_t *config)
{
    return lduw_le_p(config);
}

static inline void ub_set_long(uint8_t *config, uint32_t val)
{
    stl_le_p(config, val);
}

static inline uint32_t ub_get_long(const uint8_t *config)
{
    return ldl_le_p(config);
}

static inline void ub_set_quad(uint8_t *config, uint64_t val)
{
    stq_le_p(config, val);
}

static inline uint64_t ub_get_quad(const uint8_t *config)
{
    return ldq_le_p(config);
}

int ub_default_read_config(UBDevice *dev, uint64_t offset,
                            uint32_t *val, uint32_t dw_mask);
int ub_default_write_config(UBDevice *dev, uint64_t offset,
                             uint32_t *val, uint32_t dw_mask);
UBDevice *ub_find_device_by_guid(UbGuid *guid);
int ub_dev_finally_setup(Error **errp);
static inline uint64_t ub_config_size(void)
{
    return UB_DEV_CONFIG_SPACE_TOTAL_SIZE;
}
AddressSpace *ub_device_iommu_address_space(UBDevice *dev);
int ub_device_set_iommu_device(UBDevice *dev, HostIOMMUDevice *hoid, Error **errp);
void ub_device_unset_iommu_device(UBDevice *dev);
bool ub_device_check_ummu_is_nested(UBDevice *dev);
UBDevice *ub_find_device_by_id(const char *id);
void ub_register_ers(UBDevice *dev, uint8_t region_num,
                      MemoryRegion *memory);
uint32_t ub_interrupt_id(UBDevice *udev);
void ub_setup_iommu(UBBus *bus, const UBIOMMUOps *ops, void *opaque);
uint32_t ub_dev_get_token_id(UBDevice *udev);
uint32_t ub_dev_get_ueid(UBDevice *udev);
enum UbDeviceType ub_dev_get_type(UBDevice *udev);
int ub_dev_dump_config(const char *id, uint64_t offset, uint64_t len,
                       char *buff, int buff_size);
void ub_dev_dump_ers(const char *id, uint8_t idx, uint64_t offset, uint64_t len,
                      char *buff, int buff_size);
int ub_dev_get_detail(Monitor *mon, const char *id);
#endif
