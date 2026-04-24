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
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/arm/virt.h"
#include "hw/qdev-properties.h"
#include "hw/ub/ub.h"
#include "hw/ub/hisi/ummu.h"
#include "hw/ub/ub_bus.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_ummu.h"
#include "hw/ub/ub_config.h"
#include "hw/ub/hisi/ubc.h"
#include "migration/vmstate.h"
#include "ub_ummu_internal.h"
#include "sysemu/dma.h"
#include "hw/arm/mmu-translate-common.h"
#include "hw/ub/ub_ubc.h"
#include "qemu/error-report.h"
#include "trace.h"
#include "sysemu/iommufd.h"

static const char *const mcmdq_cmd_strings[MCMDQ_CMD_MAX] = {
    [CMD_SYNC]                   = "CMD_SYNC",
    [CMD_STALL_RESUME]           = "CMD_STALL_RESUME",
    [CMD_PREFET_CFG]             = "CMD_PREFET_CFG",
    [CMD_CFGI_TECT]              = "CMD_CFGI_TECT",
    [CMD_CFGI_TECT_RANGE]        = "CMD_CFGI_TECT_RANGE",
    [CMD_CFGI_TCT]               = "CMD_CFGI_TCT",
    [CMD_CFGI_TCT_ALL]           = "CMD_CFGI_TCT_ALL",
    [CMD_CFGI_VMS_PIDM]          = "CMD_CFGI_VMS_PIDM",
    [CMD_PLBI_OS_EID]            = "CMD_PLBI_OS_EID",
    [CMD_PLBI_OS_EIDTID]         = "CMD_PLBI_OS_EIDTID",
    [CMD_PLBI_OS_VA]             = "CMD_PLBI_OS_VA",
    [CMD_TLBI_OS_ALL]            = "CMD_TLBI_OS_ALL",
    [CMD_TLBI_OS_TID]            = "CMD_TLBI_OS_TID",
    [CMD_TLBI_OS_VA]             = "CMD_TLBI_OS_VA",
    [CMD_TLBI_OS_VAA]            = "CMD_TLBI_OS_VAA",
    [CMD_TLBI_HYP_ALL]           = "CMD_TLBI_HYP_ALL",
    [CMD_TLBI_HYP_TID]           = "CMD_TLBI_HYP_TID",
    [CMD_TLBI_HYP_VA]            = "CMD_TLBI_HYP_VA",
    [CMD_TLBI_HYP_VAA]           = "CMD_TLBI_HYP_VAA",
    [CMD_TLBI_S1S2_VMALL]        = "CMD_TLBI_S1S2_VMALL",
    [CMD_TLBI_S2_IPA]            = "CMD_TLBI_S2_IPA",
    [CMD_TLBI_NS_OS_ALL]         = "CMD_TLBI_NS_OS_ALL",
    [CMD_RESUME]                 = "CMD_RESUME",
    [CMD_CREATE_KVTBL]           = "CMD_CREATE_KVTBL",
    [CMD_DELETE_KVTBL]           = "CMD_DELETE_KVTBL",
    [CMD_TLBI_OS_ALL_U]          = "CMD_TLBI_OS_ALL_U",
    [CMD_TLBI_OS_ASID_U]         = "CMD_TLBI_OS_ASID_U",
    [CMD_TLBI_OS_VA_U]           = "CMD_TLBI_OS_VA_U",
    [CMD_TLBI_OS_VAA_U]          = "CMD_TLBI_OS_VAA_U",
    [CMD_TLBI_HYP_ASID_U]        = "CMD_TLBI_HYP_ASID_U",
    [CMD_TLBI_HYP_VA_U]          = "CMD_TLBI_HYP_VA_U",
    [CMD_TLBI_S1S2_VMALL_U]      = "CMD_TLBI_S1S2_VMALL_U",
    [CMD_TLBI_S2_IPA_U]          = "CMD_TLBI_S2_IPA_U",
};

static const char *const ummu_event_type_strings[EVT_MAX] = {
    [EVT_NONE] = "EVT_NONE",
    [EVT_UT] = "EVT_UT",
    [EVT_BAD_DSTEID] = "EVT_BAD_DSTEID",
    [EVT_TECT_FETCH] = "EVT_TECT_FETCH",
    [EVT_BAD_TECT] = "EVT_BAD_TECT",
    [EVT_RESERVE_0] = "EVT_RESERVE_0",
    [EVT_BAD_TOKENID] = "EVT_BAD_TOKENID",
    [EVT_TCT_FETCH] = "EVT_TCT_FETCH",
    [EVT_BAD_TCT] = "EVT_BAD_TCT",
    [EVT_A_PTW_EABT] = "EVT_A_PTW_EABT",
    [EVT_A_TRANSLATION] = "EVT_A_TRANSLATION",
    [EVT_A_ADDR_SIZE] = "EVT_A_ADDR_SIZE",
    [EVT_ACCESS] = "EVT_ACCESS",
    [EVT_A_PERMISSION] = "EVT_A_PERMISSION",
    [EVT_TBU_CONFLICT] = "EVT_TBU_CONFLICT",
    [EVT_CFG_CONFLICT] = "EVT_CFG_CONFLICT",
    [EVT_VMS_FETCH] = "EVT_VMS_FETCH",
    [EVT_P_PTW_EABT] = "EVT_P_PTW_EABT",
    [EVT_P_CFG_ERROR] = "EVT_P_CFG_ERROR",
    [EVT_P_PERMISSION] = "EVT_P_PERMISSION",
    [EVT_RESERVE_1] = "EVT_RESERVE_1",
    [EVT_EBIT_DENY] = "EVT_EBIT_DENY",
    [EVT_CREATE_DSTEID_TECT_RELATION_RESULT] = "EVT_CREATE_DSTEID_TECT_RELATION_RESULT",
    [EVT_DELETE_DSTEID_TECT_RELATION_RESULT] = "EVT_DELETE_DSTEID_TECT_RELATION_RESULT"
};

QLIST_HEAD(, UMMUState) ub_umms;
UMMUState *ummu_find_by_bus_num(uint8_t bus_num)
{
    UMMUState *ummu;
    QLIST_FOREACH(ummu, &ub_umms, node) {
        if (ummu->bus_num == bus_num) {
            return ummu;
        }
    }
    return NULL;
}

static void ummu_cr0_process_task(UMMUState *u)
{
    u->ctrl0_ack = u->ctrl[0];
}

static uint64_t ummu_mcmdq_reg_readl(UMMUState *u, hwaddr offset)
{
    uint8_t mcmdq_idx;
    uint64_t val = UINT64_MAX;

    mcmdq_idx = (uint8_t)(offset & MCMDQ_IDX_MASK) >> __bf_shf(MCMDQ_IDX_MASK);
    if (mcmdq_idx >= UMMU_MAX_MCMDQS) {
        qemu_log("invalid idx %u, offset is 0x%lx\n", mcmdq_idx, offset);
        return val;
    }

    switch (offset & MCMDQ_BASE_ADDR_MASK) {
        case MCMDQ_PROD_BASE_ADDR:
            val = u->mcmdqs[mcmdq_idx].queue.prod;
            break;
        case MCMDQ_CONS_BASE_ADDR:
            val = u->mcmdqs[mcmdq_idx].queue.cons;
            break;
        default:
            qemu_log("ummu cannot handle 32-bit mcmdq reg read access at 0x%lx\n", offset);
            break;
    }

    return val;
}

static int ummu_mapt_get_cmdq_base(UMMUState *u, dma_addr_t base_addr, uint32_t qid, MAPTCmdqBase *base)
{
    int ret, i;
    dma_addr_t addr = base_addr + qid * MAPT_CMDQ_CTXT_BASE_BYTES;

    ret = dma_memory_read(&address_space_memory, addr, base, sizeof(*base),
                          MEMTXATTRS_MEMORY);
    if (ret != MEMTX_OK) {
        qemu_log("Cannot fetch mapt cmdq ctx at address=0x%lx\n", addr);
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(base->word); i++) {
        le32_to_cpus(&base->word[i]);
    }

    return 0;
}

static int ummu_mapt_update_cmdq_base(UMMUState *u, dma_addr_t base_addr, uint32_t qid, MAPTCmdqBase *base)
{
    int i;
    dma_addr_t addr = base_addr + qid * MAPT_CMDQ_CTXT_BASE_BYTES;

    for (i = 0; i < ARRAY_SIZE(base->word); i++, addr += sizeof(uint32_t)) {
        uint32_t tmp = cpu_to_le32(base->word[i]);
        if (dma_memory_write(&address_space_memory, addr, &tmp,
                             sizeof(uint32_t), MEMTXATTRS_MEMORY)) {
            qemu_log("dma failed to write to addr 0x%lx\n", addr);
            return -1;
        }
    }

    return 0;
}

static uint64_t ummu_mapt_ctrlr_page_read_process(UMMUState *u, hwaddr offset)
{
    MAPTCmdqBase base;
    uint32_t qid = ummu_mapt_cmdq_get_qid(u, offset);
    dma_addr_t addr = MAPT_CMDQ_CTXT_BASE_ADDR(u->mapt_cmdq_ctxt_base);
    int ret;
    uint64_t val = UINT64_MAX;

    if (!addr) {
        /* mapt ctrlr page not init, return default val 0 */
        return 0;
    }

    ret = ummu_mapt_get_cmdq_base(u, addr, qid, &base);
    if (ret) {
        qemu_log("failed to get mapt cmdq base.\n");
        return val;
    }

    switch (offset & UCMDQ_UCPLQ_CI_PI_MASK) {
        case UCMDQ_PI:
            val = ummu_mapt_cmdq_base_get_ucmdq_pi(&base);
            break;
        case UCMDQ_CI:
            val = ummu_mapt_cmdq_base_get_ucmdq_ci(&base);
            break;
        case UCPLQ_PI:
            val = ummu_mapt_cmdq_base_get_ucplq_pi(&base);
            break;
        case UCPLQ_CI:
            val = ummu_mapt_cmdq_base_get_ucplq_ci(&base);
            break;
        default:
            qemu_log("cannot process addr(0x%lx) mpat ctrlr page read.\n", offset);
            return val;
    }

    return val;
}

static uint64_t ummu_reg_readw(UMMUState *u, hwaddr offset)
{
    uint64_t val = UINT64_MAX;

    switch (offset) {
        case A_UCMDQ_PI_START_REG...A_UCPLQ_CI_END_REG:
            val = ummu_mapt_ctrlr_page_read_process(u, offset);
            break;
        default:
            qemu_log("ummu cannot handle 16-bit read access at: 0x%lx\n", offset);
            break;
    }

    return val;
}

static uint64_t ummu_reg_readl(UMMUState *u, hwaddr offset)
{
    uint64_t val = UINT64_MAX;

    switch (offset) {
        case A_CAP0...A_CAP6:
            val = u->cap[(offset - A_CAP0) / 4];
            break;
        case A_IIDR:
            val = u->iidr;
            break;
        case A_AIDR:
            val = u->aidr;
            break;
        case A_CTRL0:
            val = u->ctrl[0];
            break;
        case A_CTRL0_ACK:
            val = u->ctrl0_ack;
            break;
        case A_CTRL1:
            val = u->ctrl[1];
            break;
        case A_CTRL2:
            val = u->ctrl[2];
            break;
        case A_CTRL3:
            val = u->ctrl[3];
            break;
        case A_TECT_BASE_CFG:
            val = u->tect_base_cfg;
            break;
        case A_MCMD_QUE_BASE...A_MCMD_QUE_LASTEST_CI:
            val = ummu_mcmdq_reg_readl(u, offset);
            break;
        case A_EVENT_QUE_PI:
            val = u->eventq.queue.prod;
            break;
        case A_EVENT_QUE_CI:
            val = u->eventq.queue.cons;
            break;
        case A_EVENT_QUE_USI_DATA:
            val = u->eventq.usi_data;
            break;
        case A_EVENT_QUE_USI_ATTR:
            val = u->eventq.usi_attr;
            break;
        case A_GLB_INT_EN:
            val = 0;
            /* glb err interrupt bit enabled int bit 0 */
            if (ummu_glb_err_int_en(u)) {
                val |= 0x1;
            }

            /* event que interrupt bit enabled in bit 1 */
            if (ummu_event_que_int_en(u)) {
                val |= (1 << 1);
            }
            break;
        case A_GLB_ERR:
            val = u->glb_err.glb_err;
            break;
        case A_GLB_ERR_RESP:
            val = u->glb_err.glb_err_resp;
            break;
        case A_GLB_ERR_INT_USI_DATA:
            val = u->glb_err.usi_data;
            break;
        case A_GLB_ERR_INT_USI_ATTR:
            val = u->glb_err.usi_attr;
            break;
        case A_RELEASE_UM_QUEUE_ID:
            val = u->release_um_queue_id;
            break;
        case A_RELEASE_UM_QUEUE:
            val = u->release_um_queue;
            break;
        case A_UCMDQ_PI_START_REG...A_UCPLQ_CI_END_REG:
            val = ummu_mapt_ctrlr_page_read_process(u, offset);
            break;
        case A_UMCMD_PAGE_SEL:
            val = u->ucmdq_page_sel;
            break;
        case A_UMMU_USER_CONFIG0...A_UMMU_USER_CONFIG11:
        case A_UMMU_MEM_USI_DATA:
        case A_UMMU_MEM_USI_ATTR:
        case A_UMMU_INT_MASK:
        case A_UMMU_DSTEID_CAM_TABLE_BASE_CFG:
            /* do nothing, reg return val 0 */
            val = 0;
            break;
        default:
            qemu_log("ummu cannot handle 32-bit read access at 0x%lx\n", offset);
            break;
    }

    return val;
}

static uint64_t ummu_mcmdq_reg_readll(UMMUState *u, hwaddr offset)
{
    uint8_t mcmdq_idx;
    uint64_t val = UINT64_MAX;

    mcmdq_idx = (uint8_t)(offset & MCMDQ_IDX_MASK) >> __bf_shf(MCMDQ_IDX_MASK);
    if (mcmdq_idx >= UMMU_MAX_MCMDQS) {
        qemu_log("invalid idx %u, offset is 0x%lx\n", mcmdq_idx, offset);
        return val;
    }

    switch (offset & MCMDQ_BASE_ADDR_MASK) {
        case A_MCMD_QUE_BASE:
            val = u->mcmdqs[mcmdq_idx].queue.base;
            break;
        default:
            qemu_log("ummu cannot handle 64-bit mcmdq reg read access at 0x%lx\n", offset);
            break;
    }

    return val;
}

static uint64_t ummu_reg_readll(UMMUState *u, hwaddr offset)
{
    uint64_t val = UINT64_MAX;

    switch (offset) {
        case A_TECT_BASE0:
            val = u->tect_base;
            break;
        case A_MCMD_QUE_BASE...A_MCMD_QUE_LASTEST_CI:
            val = ummu_mcmdq_reg_readll(u, offset);
            break;
        case A_EVENT_QUE_BASE0:
            val = u->eventq.queue.base;
            break;
        case A_EVENT_QUE_USI_ADDR0:
            val = u->eventq.usi_addr;
            break;
        case A_GLB_ERR_INT_USI_ADDR0:
            val = u->glb_err.usi_addr;
            break;
        case A_MAPT_CMDQ_CTXT_BADDR0:
            val = u->mapt_cmdq_ctxt_base;
            break;
        case A_UMMU_MEM_USI_ADDR0:
            /* do nothing, reg return val 0 */
            val = 0;
            break;
        default:
            qemu_log("ummu cannot handle 64-bit read access at 0x%lx\n", offset);
            break;
    }

    return val;
}

static uint64_t ummu_reg_read(void *opaque, hwaddr offset, unsigned size)
{
    UMMUState *u = opaque;
    uint64_t val = UINT64_MAX;

    switch (size) {
        case 2:
            val = ummu_reg_readw(u, offset);
            break;
        case 4:
            val = ummu_reg_readl(u, offset);
            break;
        case 8:
            val = ummu_reg_readll(u, offset);
            break;
        default:
            break;
    }

    return val;
}

static void mcmdq_cmd_sync_usi_irq(uint64_t addr, uint32_t data)
{
    cpu_physical_memory_rw(addr, &data, sizeof(uint32_t), true);
}

static void mcmdq_cmd_sync_sev_irq(void)
{
    qemu_log("cannot support CMD_SYNC SEV event.\n");
}

static void ummu_glb_usi_notify(UMMUState *u, UMMUUSIVectorType type)
{
    USIMessage msg;

    if (type == UMMU_USI_VECTOR_GERROR) {
        msg = ummu_get_gerror_usi_message(u);
    } else {
        msg = ummu_get_eventq_usi_message(u);
    }

    usi_send_message(&msg, UMMU_INTERRUPT_ID, NULL);
}

static void mcmdq_cmd_sync_handler(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    uint32_t cm = CMD_SYNC_CM(cmd);

    trace_mcmdq_cmd_sync_handler(mcmdq_idx, CMD_SYNC_USI_ADDR(cmd), CMD_SYNC_USI_DATA(cmd));
    if (cm & CMD_SYNC_CM_USI) {
        mcmdq_cmd_sync_usi_irq(CMD_SYNC_USI_ADDR(cmd), CMD_SYNC_USI_DATA(cmd));
    } else if (cm & CMD_SYNC_CM_SEV) {
        mcmdq_cmd_sync_sev_irq();
    }
}

static void mcmdq_cmd_create_kvtbl(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    UMMUKVTblEntry *entry = NULL;
    uint32_t dst_eid = CMD_CREATE_KVTBL_DEST_EID(cmd);
    uint32_t tecte_tag = CMD_CREATE_KVTBL_TECTE_TAG(cmd);

    trace_mcmdq_cmd_create_kvtbl(mcmdq_idx, dst_eid, tecte_tag);

    if (u->kvtbl_entrys >= UMMU_KVTBL_ENTRY_MAX_NUM) {
        qemu_log("kvtbl_entrys reach max value %u\n", u->kvtbl_entrys);
        return;
    }

    QLIST_FOREACH(entry, &u->kvtbl, list) {
        if (entry->dst_eid == dst_eid) {
            qemu_log("update kvtlb dst_eid(0x%x) tecte_tag from 0x%x to 0x%x\n",
                     dst_eid, entry->tecte_tag, tecte_tag);
            entry->tecte_tag = tecte_tag;
            return;
        }
    }

    entry = g_malloc(sizeof(UMMUKVTblEntry));
    if (!entry) {
        qemu_log("failed to malloc for kvtbl entry for dst_eid(0x%x)\n", dst_eid);
        return;
    }

    entry->dst_eid = dst_eid;
    entry->tecte_tag = tecte_tag;
    QLIST_INSERT_HEAD(&u->kvtbl, entry, list);
    u->kvtbl_entrys++;
}

static void mcmdq_cmd_delete_kvtbl(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    UMMUKVTblEntry *entry = NULL;
    uint32_t dst_eid = CMD_DELETE_KVTBL_DEST_EID(cmd);

    trace_mcmdq_cmd_delete_kvtbl(mcmdq_idx, dst_eid);

    QLIST_FOREACH(entry, &u->kvtbl, list) {
        if (entry->dst_eid == dst_eid) {
            break;
        }
    }

    if (entry) {
        QLIST_REMOVE(entry, list);
        u->kvtbl_entrys--;
        g_free(entry);
    } else {
        qemu_log("cannot find dst_eid(0x%x) entry in kvtbl.\n", dst_eid);
    }
}

static gboolean ummu_invalid_tecte(gpointer key, gpointer value, gpointer user_data)
{
    UMMUDevice *ummu_dev = (UMMUDevice *)key;
    UMMUTransCfg *cfg = (UMMUTransCfg *)value;
    UMMUTecteRange *range = (UMMUTecteRange *)user_data;

    if (range->invalid_all ||
        (cfg->tecte_tag >= range->start && cfg->tecte_tag <= range->end)) {
        qemu_log("ummu start invalidate udev(%s) cached config.\n", ummu_dev->udev->qdev.id);
        return true;
    }

    return false;
}

static void ummu_invalid_single_tecte(UMMUState *u, uint32_t tecte_tag)
{
    UMMUTecteRange tecte_range = { .invalid_all = false, };

    trace_ummu_invalid_single_tecte(tecte_tag);
    tecte_range.start = tecte_tag;
    tecte_range.end = tecte_tag;
    g_hash_table_foreach_remove(u->configs, ummu_invalid_tecte, &tecte_range);
}

static void ummu_uninstall_nested_tecte(gpointer key, gpointer value, gpointer opaque)
{
    UMMUDevice *ummu_dev = (UMMUDevice *)value;

    ummu_dev_uninstall_nested_tecte(ummu_dev);
}

/* V | ST_MODE(.CONFIG) | TCRC_SEL(.STRW) */
#define INSTALL_TECTE0_WORD0_MASK (GENMASK(0, 0) | GENMASK(1, 3) | GENMASK(22, 21))
#define INSTALL_TECTE0_WORD1_MASK 0
/* TCT_MAXNUM(.S1CDMax) |  TCT_PTR[31:6](.S1ContextPtr) */
#define INSTALL_TECTE1_WORD0_MASK (GENMASK(4, 0) | GENMASK(31, 6))
/* TCT_PTR[51:32](.S1ContextPtr) | TCT_FMT(.S1Fmt) | TCT_STALL_EN(.S1STALLD) |
 * TCT_Ptr_MD0(.S1CIR) | TCT_Ptr_MD1(.S1COR) | TCT_Ptr_MSD(.S1CSH) */
#define INSTALL_TECTE1_WORD1_MASK (GENMASK(19, 0)  | \
                                   GENMASK(21, 20) | \
                                   GENMASK(24, 24) | \
                                   GENMASK(27, 26) | \
                                   GENMASK(29, 28) | \
                                   GENMASK(31, 30))

static void ummu_install_nested_tecte(gpointer key, gpointer value, gpointer opaque)
{
    UMMUDevice *ummu_dev = (UMMUDevice *)value;
    TECTE *tecte = (TECTE *)opaque;
    struct iommu_hwpt_ummu iommu_config = {};
    int ret;

    if (ummu_dev->udev->dev_type != UB_TYPE_DEVICE &&
        ummu_dev->udev->dev_type != UB_TYPE_IDEVICE) {
        return;
    }

    if (!ummu_dev->vdev && ummu_dev->idev && ummu_dev->viommu) {
        UMMUVdev *vdev = g_new0(UMMUVdev, 1);
        /* default use eid as virt_id */
        vdev->core = iommufd_backend_alloc_vdev(ummu_dev->idev, ummu_dev->viommu->core, ummu_dev->udev->eid);
        if (!vdev->core) {
            error_report("failed to allocate a vDEVICE");
            g_free(vdev);
            return;
        }
        ummu_dev->vdev = vdev;
    }

    iommu_config.tecte[0] = (uint64_t)tecte->word[0] & INSTALL_TECTE0_WORD0_MASK;
    iommu_config.tecte[0] |= ((uint64_t)tecte->word[1] & INSTALL_TECTE0_WORD1_MASK) << 32;
    iommu_config.tecte[1] = (uint64_t)tecte->word[2] & INSTALL_TECTE1_WORD0_MASK;
    iommu_config.tecte[1] |= ((uint64_t)tecte->word[3] & INSTALL_TECTE1_WORD1_MASK) << 32;
    trace_ummu_install_nested_tecte(iommu_config.tecte[0], iommu_config.tecte[1]);
    ret = ummu_dev_install_nested_tecte(ummu_dev, IOMMU_HWPT_DATA_UMMU,
                                        sizeof(iommu_config), &iommu_config);
    if (ret && ret != -ENOENT) {
        error_report("Unable to alloc Stage-1 HW Page Table: %d", ret);
    } else if (ret == 0) {
        qemu_log("install nested tecte success.\n");
    }
}

static int ummu_find_tecte(UMMUState *ummu, uint32_t tecte_tag, TECTE *tecte);
static void ummu_config_tecte(UMMUState *u, uint32_t tecte_tag)
{
    TECTE tecte;
    int ret;

    ret = ummu_find_tecte(u, tecte_tag, &tecte);
    if (ret) {
        qemu_log("failed to find tecte\n");
        return;
    }

    trace_ummu_config_tecte(TECTE_VALID(&tecte), TECTE_ST_MODE(&tecte));
    if (!TECTE_VALID(&tecte) || TECTE_ST_MODE(&tecte) != TECTE_ST_MODE_S1) {
        g_hash_table_foreach(u->ummu_devs, ummu_uninstall_nested_tecte, NULL);
        return;
    }

    if (u->tecte_tag_num >= UMMU_TECTE_TAG_MAX_NUM) {
        qemu_log("unexpect tecte tag num over %u\n", UMMU_TECTE_TAG_MAX_NUM);
        return;
    }
    g_hash_table_foreach(u->ummu_devs, ummu_install_nested_tecte, &tecte);
    u->tecte_tag_cache[u->tecte_tag_num++] = tecte_tag;
}

static void ummu_invalidate_cache(UMMUState *u, UMMUMcmdqCmd *cmd);
static void mcmdq_cmd_cfgi_tect_handler(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    uint32_t tecte_tag = CMD_TECTE_TAG(cmd);

    trace_mcmdq_cmd_cfgi_tect_handler(mcmdq_idx, tecte_tag);

    ummu_invalid_single_tecte(u, tecte_tag);
    ummu_config_tecte(u, tecte_tag);
    ummu_invalidate_cache(u, cmd);
}

static void mcmdq_cmd_cfgi_tect_range_handler(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    uint32_t tecte_tag = CMD_TECTE_TAG(cmd);
    uint8_t range = CMD_TECTE_RANGE(cmd);
    uint32_t mask;
    int i;
    UMMUTecteRange tecte_range = { .invalid_all = false, };

    trace_mcmdq_cmd_cfgi_tect_range_handler(mcmdq_idx, tecte_tag, range);

    if (CMD_TECTE_RANGE_INVILID_ALL(range)) {
        tecte_range.invalid_all = true;
    } else {
        mask = (1ULL << (range + 1)) - 1;
        tecte_range.start = tecte_tag & ~mask;
        tecte_range.end  = tecte_range.start + mask;
    }

    g_hash_table_foreach_remove(u->configs, ummu_invalid_tecte, &tecte_range);
    ummu_invalidate_cache(u, cmd);

    if (tecte_range.invalid_all && u->tecte_tag_num > 0) {
        u->tecte_tag_num = 0;
        g_hash_table_foreach(u->ummu_devs, ummu_uninstall_nested_tecte, NULL);
        return;
    }

    if (!tecte_range.invalid_all) {
        for (i = tecte_range.start; i <= tecte_range.end; i++) {
            ummu_config_tecte(u, i);
        }
    }

}

static void mcmdq_cmd_cfgi_tct_handler(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    uint32_t tecte_tag = CMD_TECTE_TAG(cmd);

    trace_mcmdq_cmd_cfgi_tct_handler(mcmdq_idx, tecte_tag);

    ummu_invalid_single_tecte(u, tecte_tag);
    ummu_invalidate_cache(u, cmd);
}

static void mcmdq_cmd_cfgi_tct_all_handler(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    trace_mcmdq_cmd_cfgi_tct_all_handler(mcmdq_idx);

    /* cfgi_tct & cfgi_tct_all process is the same */
    mcmdq_cmd_cfgi_tct_handler(u, cmd, mcmdq_idx);
}

static void ummu_viommu_invalidate_cache(IOMMUFDViommu *viommu, uint32_t type, UMMUMcmdqCmd *cmd)
{
    int ret;
    uint32_t tecte_tag = CMD_TECTE_TAG(cmd);
    uint32_t ncmds = 1;

    if (!viommu) {
        return;
    }

    ret = iommufd_viommu_invalidate_cache(viommu->iommufd, viommu->viommu_id,
                                          type, sizeof(*cmd), &ncmds, cmd);
    if (ret) {
        qemu_log("failed to invalidate cache for ummu, tecte_tag = %u, ret = %d\n", tecte_tag, ret);
    }
}

static void ummu_invalidate_cache(UMMUState *u, UMMUMcmdqCmd *cmd)
{
    IOMMUFDViommu *viommu = NULL;
    UMMUDevice *ummu_dev = NULL;

    if (!u->viommu) {
        return;
    }

    ummu_dev = QLIST_FIRST(&u->viommu->device_list);
    if (!ummu_dev || !ummu_dev->vdev) {
        return;
    }

    viommu = u->viommu->core;
    ummu_viommu_invalidate_cache(viommu, IOMMU_VIOMMU_INVALIDATE_DATA_UMMU, cmd);
}

static void mcmdq_cmd_plbi_x_process(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    trace_mcmdq_cmd_plbi_x_process(mcmdq_idx, mcmdq_cmd_strings[CMD_TYPE(cmd)]);
    ummu_invalidate_cache(u, cmd);
}

static void mcmdq_cmd_tlbi_x_process(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    trace_mcmdq_cmd_tlbi_x_process(mcmdq_idx, mcmdq_cmd_strings[CMD_TYPE(cmd)]);
    ummu_invalidate_cache(u, cmd);
}

static void mcmdq_check_pa_continuity_fill_result(UMMUMcmdQueue *mcmdq, bool continuity)
{
    uint8_t result = 0;
    dma_addr_t addr;

    result |= UMMU_RUN_IN_VM_FLAG;
    if (continuity) {
        result |= PA_CONTINUITY;
    } else {
        result |= PA_NOT_CONTINUITY;
    }

#define CHECK_PA_CONTINUITY_RESULT_OFFSET 0x2
    addr = MCMD_QUE_BASE_ADDR(&mcmdq->queue) +
           MCMD_QUE_RD_IDX(&mcmdq->queue) * mcmdq->queue.entry_size;
    if (dma_memory_write(&address_space_memory, addr + CHECK_PA_CONTINUITY_RESULT_OFFSET,
                         &result, sizeof(result), MEMTXATTRS_MEMORY)) {
        qemu_log("dma failed to wirte result(0x%x) to addr 0x%lx\n", result, addr);
        return;
    }

    qemu_log("mcmdq check pa continuity update result(0x%x) success.\n", result);
}

static void mcmdq_cmd_pa_continuity(UMMUState *u, UMMUMcmdqCmd *cmd,
                                     uint8_t mcmdq_idx)
{
    uint64_t size;
    uint64_t addr;
    void *hva = NULL;
    ram_addr_t rb_offset;
    RAMBlock *rb = NULL;
    size_t rb_page_size = 0;
#define PAGESZ_4K 0x1000
    uint64_t map_size = PAGESZ_4K;

    size = CMD_NULL_CHECK_PA_CONTI_SIZE(cmd);
    addr = CMD_NULL_CHECK_PA_CONTI_ADDR(cmd);
    hva = cpu_physical_memory_map(addr, &map_size, false);
    rb = qemu_ram_block_from_host(hva, false, &rb_offset);
    if (rb) {
        rb_page_size = qemu_ram_pagesize(rb);
    } else {
        qemu_log("failed to get ram block from host(%p) map_size(%" PRIu64 ")\n", hva, map_size);
    }
    cpu_physical_memory_unmap(hva, map_size, false, 0);

    trace_mcmdq_cmd_null(mcmdq_idx, addr, hva, size, rb_page_size, map_size);

#define PAGESZ_2M 0x200000
    if (rb_page_size < PAGESZ_2M) {
        mcmdq_check_pa_continuity_fill_result(&u->mcmdqs[mcmdq_idx], false);
    } else {
        mcmdq_check_pa_continuity_fill_result(&u->mcmdqs[mcmdq_idx], true);
    }
}

#ifdef CONFIG_UBMEM_VMMU
static void mcmdq_check_ubmem_vmmu_support(UMMUMcmdQueue *mcmdq)
{
    dma_addr_t addr;
    uint8_t result = UBMEM_UMMU_NOT_SUPPORT;

    addr = MCMD_QUE_BASE_ADDR(&mcmdq->queue) +
           MCMD_QUE_RD_IDX(&mcmdq->queue) * mcmdq->queue.entry_size;
#define CHECK_UBMEM_VMMU_SUPPORT_RESULT_OFFSET 0x2
    if (dma_memory_write(&address_space_memory,
                         addr + CHECK_UBMEM_VMMU_SUPPORT_RESULT_OFFSET,
                         &result, sizeof(result), MEMTXATTRS_MEMORY)) {
        qemu_log("dma failed to wirte ubmem vmmu support result(0x%x) "
                 "to addr 0x%lx\n", result, addr);
        return;
    }
    qemu_log("mcmdq check ubmem vmmu support update result(0x%x) "
             "success.\n", result);
}
#endif

static void mcmdq_cmd_null(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{

    switch (CMD_NULL_SUBOP(cmd)) {
    case CMD_NULL_SUBOP_CHECK_PA_CONTINUITY:
        mcmdq_cmd_pa_continuity(u, cmd, mcmdq_idx);
        break;
#ifdef CONFIG_UBMEM_VMMU
    case CMD_NULL_SUBOP_CHECK_UBMEM_VMMU_SUPPORT:
        mcmdq_check_ubmem_vmmu_support(&u->mcmdqs[mcmdq_idx]);
        break;
#endif
    default:
        qemu_log("ummu cannot handle null cmd subop 0x%x\n",
                    CMD_NULL_SUBOP(cmd));
        return;
    }
}

static void mcmdq_cmd_prefet_cfg(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    /* do nothing */
}

static void (*mcmdq_cmd_handlers[])(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx) = {
    [CMD_SYNC]                 = mcmdq_cmd_sync_handler,
    [CMD_STALL_RESUME]         = NULL,
    [CMD_PREFET_CFG]           = mcmdq_cmd_prefet_cfg,
    [CMD_CFGI_TECT]            = mcmdq_cmd_cfgi_tect_handler,
    [CMD_CFGI_TECT_RANGE]      = mcmdq_cmd_cfgi_tect_range_handler,
    [CMD_CFGI_TCT]             = mcmdq_cmd_cfgi_tct_handler,
    [CMD_CFGI_TCT_ALL]         = mcmdq_cmd_cfgi_tct_all_handler,
    [CMD_CFGI_VMS_PIDM]        = NULL,
    [CMD_PLBI_OS_EID]          = mcmdq_cmd_plbi_x_process,
    [CMD_PLBI_OS_EIDTID]       = mcmdq_cmd_plbi_x_process,
    [CMD_PLBI_OS_VA]           = mcmdq_cmd_plbi_x_process,
    [CMD_TLBI_OS_ALL]          = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_OS_TID]          = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_OS_VA]           = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_OS_VAA]          = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_HYP_ALL]         = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_HYP_TID]         = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_HYP_VA]          = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_HYP_VAA]         = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_S1S2_VMALL]      = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_S2_IPA]          = mcmdq_cmd_tlbi_x_process,
    [CMD_TLBI_NS_OS_ALL]       = mcmdq_cmd_tlbi_x_process,
    [CMD_RESUME]               = NULL,
    [CMD_CREATE_KVTBL]         = mcmdq_cmd_create_kvtbl,
    [CMD_DELETE_KVTBL]         = mcmdq_cmd_delete_kvtbl,
    [CMD_NULL]                 = mcmdq_cmd_null,
    [CMD_TLBI_OS_ALL_U]        = NULL,
    [CMD_TLBI_OS_ASID_U]       = NULL,
    [CMD_TLBI_OS_VA_U]         = NULL,
    [CMD_TLBI_OS_VAA_U]        = NULL,
    [CMD_TLBI_HYP_ASID_U]      = NULL,
    [CMD_TLBI_HYP_VA_U]        = NULL,
    [CMD_TLBI_S1S2_VMALL_U]    = NULL,
    [CMD_TLBI_S2_IPA_U]        = NULL,
};

static MemTxResult ummu_cmdq_fetch_cmd(UMMUMcmdQueue *mcmdq, UMMUMcmdqCmd *cmd)
{
    uint64_t addr, mcmdq_base_addr;
    MemTxResult ret;
    int i;

    mcmdq_base_addr = MCMD_QUE_BASE_ADDR(&mcmdq->queue);
    addr = mcmdq_base_addr + MCMD_QUE_RD_IDX(&mcmdq->queue) * mcmdq->queue.entry_size;
    ret = dma_memory_read(&address_space_memory, addr, cmd, sizeof(UMMUMcmdqCmd),
                          MEMTXATTRS_MEMORY);
    if (ret != MEMTX_OK) {
        qemu_log("addr 0x%lx failed to fectch mcmdq cmd\n", addr);
        return ret;
    }

    for (i = 0; i < ARRAY_SIZE(cmd->word); i++) {
        le32_to_cpus(&cmd->word[i]);
    }

    return ret;
}

static void mcmdq_process_task(UMMUState *u, uint8_t mcmdq_idx)
{
    UMMUMcmdQueue *mcmdq = &u->mcmdqs[mcmdq_idx];
    UMMUMcmdqCmd cmd;
    UmmuMcmdqCmdType cmd_type;

    if (!ummu_mcmdq_enabled(mcmdq)) {
        ummu_mcmdq_disable_resp(mcmdq);
        return;
    }

    while (!ummu_mcmdq_empty(mcmdq)) {
        if (ummu_cmdq_fetch_cmd(mcmdq, &cmd) != MEMTX_OK) {
            /* eventq generate later */
            break;
        }

        cmd_type = CMD_TYPE(&cmd);
        if (cmd_type >= MCMDQ_CMD_MAX) {
            /* eventq generate later */
            break;
        }

        if (mcmdq_cmd_handlers[cmd_type]) {
            trace_mcmdq_process_task(mcmdq_idx, mcmdq_cmd_strings[cmd_type]);
            mcmdq_cmd_handlers[cmd_type](u, &cmd, mcmdq_idx);
        } else {
            qemu_log("current cannot process mcmdq cmd: %s.\n", mcmdq_cmd_strings[cmd_type]);
        }

        ummu_mcmdq_cons_incr(mcmdq);
    }

    ummu_mcmdq_enable_resp(mcmdq);
}

static void ummu_mcmdq_reg_writel(UMMUState *u, hwaddr offset, uint64_t data)
{
    uint8_t mcmdq_idx;
    UMMUMcmdQueue *q = NULL;

    mcmdq_idx = (uint8_t)(offset & MCMDQ_IDX_MASK) >> __bf_shf(MCMDQ_IDX_MASK);
    if (mcmdq_idx >= UMMU_MAX_MCMDQS) {
        qemu_log("invalid idx %u, offset is 0x%lx\n", mcmdq_idx, offset);
        return;
    }

    switch (offset & MCMDQ_BASE_ADDR_MASK) {
        case MCMDQ_PROD_BASE_ADDR:
            update_reg32_by_wmask(&u->mcmdqs[mcmdq_idx].queue.prod, data, UMMU_MCMDQ_PI_WMASK);
            mcmdq_process_task(u, mcmdq_idx);
            break;
        case MCMDQ_CONS_BASE_ADDR:
            update_reg32_by_wmask(&u->mcmdqs[mcmdq_idx].queue.cons, data, UMMU_MCMDQ_CI_WMASK);
            break;
        default:
            qemu_log("ummu cannot handle 32-bit mcmdq reg write access at 0x%lx\n", offset);
            break;
    }

    q = &u->mcmdqs[mcmdq_idx];
    trace_ummu_mcmdq_reg_writel(mcmdq_idx, MCMD_QUE_WD_IDX(&q->queue), MCMD_QUE_RD_IDX(&q->queue));
}

static void ummu_glb_int_disable(UMMUState *u, UMMUUSIVectorType type)
{
    qemu_log("start disable glb int\n");

    if (u->usi_virq[type] < 0) {
        return;
    }

    kvm_irqchip_release_virq(kvm_state, u->usi_virq[type]);
    u->usi_virq[type] = -1;
}

static void ummu_glb_int_enable(UMMUState *u, UMMUUSIVectorType type)
{
    KVMRouteChange route_change;
    USIMessage msg;
    uint32_t interrupt_id = UMMU_INTERRUPT_ID;

    if (!kvm_usi_via_irqfd_enabled()) {
        qemu_log("kvm usi via irqfd disabled.\n");
        return;
    }

    if (type == UMMU_USI_VECTOR_EVETQ) {
        msg = ummu_get_eventq_usi_message(u);
    } else {
        msg = ummu_get_gerror_usi_message(u);
    }

    route_change = kvm_irqchip_begin_route_changes(kvm_state);
    u->usi_virq[type] = kvm_irqchip_add_usi_route(&route_change, msg, interrupt_id, NULL);
    trace_ummu_glb_int_enable(type, u->usi_virq[type]);
    if (u->usi_virq[type] < 0) {
        qemu_log("kvm irqchip failed to add usi route.\n");
        return;
    }
    kvm_irqchip_commit_route_changes(&route_change);
}

static void ummu_handle_glb_int_enable_update(UMMUState *u, UMMUUSIVectorType type,
                                              bool was_enabled, bool is_enabled)
{
    if (was_enabled && !is_enabled) {
        ummu_glb_int_disable(u, type);
    } else if (!was_enabled && is_enabled) {
        ummu_glb_int_enable(u, type);
    }
}

static void ummu_glb_int_en_process(UMMUState *u, uint64_t data)
{
    bool gerror_was_enabled, eventq_was_enabled;
    bool gerror_is_enabled, eventq_is_enabled;

    /* process eventq interrupt update */
    eventq_was_enabled = ummu_event_que_int_en(u);
    ummu_set_event_que_int_en(u, data);
    eventq_is_enabled = ummu_event_que_int_en(u);
    ummu_handle_glb_int_enable_update(u, UMMU_USI_VECTOR_EVETQ,
                                      eventq_was_enabled, eventq_is_enabled);

    /* process glb_err interrupt update */
    gerror_was_enabled = ummu_glb_err_int_en(u);
    ummu_set_glb_err_int_en(u, data);
    gerror_is_enabled = ummu_glb_err_int_en(u);
    ummu_handle_glb_int_enable_update(u, UMMU_USI_VECTOR_GERROR,
                                      gerror_was_enabled, gerror_is_enabled);
}

static MemTxResult ummu_mapt_cmdq_fetch_cmd(MAPTCmdqBase *base, MAPTCmd *cmd)
{
    dma_addr_t base_addr = MAPT_UCMDQ_BASE_ADDR(base);
    dma_addr_t addr = base_addr + MAPT_UCMDQ_CI(base) * sizeof(*cmd);
    int ret, i;

    ret = dma_memory_read(&address_space_memory, addr, cmd, sizeof(*cmd),
                          MEMTXATTRS_MEMORY);
    if (ret != MEMTX_OK) {
        qemu_log("addr 0x%lx failed to fectch mapt ucmdq cmd.\n", addr);
        return ret;
    }

    for (i = 0; i < ARRAY_SIZE(cmd->word); i++) {
        le32_to_cpus(&cmd->word[i]);
    }

    return ret;
}

static void ummu_mapt_cplq_add_entry(MAPTCmdqBase *base, MAPTCmdCpl *cpl)
{
    dma_addr_t base_addr = MAPT_UCPLQ_BASE_ADDR(base);
    dma_addr_t addr = base_addr + MAPT_UCPLQ_PI(base) * sizeof(*cpl);
    uint32_t tmp = cpu_to_le32(*(uint32_t *)cpl);

    if (dma_memory_write(&address_space_memory, addr, &tmp,
                         sizeof(tmp), MEMTXATTRS_MEMORY)) {
        qemu_log("dma failed to wirte cpl entry to addr 0x%lx\n", addr);
    }
}

static void ummu_process_mapt_cmd(UMMUState *u, MAPTCmdqBase *base, MAPTCmd *cmd, uint32_t ci)
{
    uint32_t type = MAPT_UCMD_TYPE(cmd);
    MAPTCmdCpl cpl;
    UMMUMcmdqCmd mcmd_cmd = { 0 };
    uint16_t tecte_tag;
    uint32_t tid;

    mcmd_cmd.word[0] = CMD_PLBI_OS_EID;
    /* default set cpl staus invalid */
    ummu_mapt_ucplq_set_cpl(&cpl, MAPT_UCPL_STATUS_INVALID, 0);
    tecte_tag = ummu_mapt_cmdq_base_get_tecte_tag(base);
    tid = ummu_mapt_cmdq_base_get_token_id(base);
    qemu_log("tid: %u, tecte_tag: %u\n", tid, tecte_tag);
    switch (type) {
        case MAPT_UCMD_TYPE_PSYNC:
            qemu_log("start process mapt cmd: MAPT_UCMD_TYPE_PSYNC.\n");
            ummu_mapt_ucplq_set_cpl(&cpl, MAPT_UCPL_STATUS_PSYNC_SUCCESS, ci);
            break;
        case MAPT_UCMD_TYPE_PLBI_USR_ALL:
            qemu_log("start process mapt cmd: MAPT_UCMD_TYPE_PLBI_USR_ALL.\n");
            ummu_mcmdq_construct_plbi_os_eidtid(&mcmd_cmd, tid, tecte_tag);
            ummu_invalidate_cache(u, &mcmd_cmd);
            break;
        case MAPT_UCMD_TYPE_PLBI_USR_VA:
            qemu_log("start process mapt cmd: MAPT_UCMD_TYPE_PLBI_USR_VA.\n");
            ummu_plib_usr_va_to_pibi_os_va(cmd, &mcmd_cmd, tid, tecte_tag);
            ummu_invalidate_cache(u, &mcmd_cmd);
            break;
        default:
            qemu_log("unknown mapt cmd type: 0x%x\n", type);
            ummu_mapt_ucplq_set_cpl(&cpl, MAPT_UCPL_STATUS_TYPE_ERROR, ci);
            break;
    }

    if (cpl.cpl_status == MAPT_UCPL_STATUS_INVALID) {
        return;
    }

    if (ummu_mapt_ucplq_full(base)) {
        qemu_log("mapt ucplq full, failed to add cpl entry.\n");
        return;
    }
    ummu_mapt_cplq_add_entry(base, &cpl);
    ummu_mapt_ucqlq_prod_incr(base);
    qemu_log("mapt cplq add entry success, cplpi: %u, cplci: %u.\n",
             MAPT_UCPLQ_PI(base),  MAPT_UCPLQ_CI(base));
}

static void ummu_process_mapt_cmdq(UMMUState *u, MAPTCmdqBase *base)
{
    MAPTCmd cmd;
    int ret;

    while (!ummu_mapt_ucmdq_empty(base)) {
        ret = ummu_mapt_cmdq_fetch_cmd(base, &cmd);
        if (ret) {
            qemu_log("failed to fetch matp cmdq cmd.\n");
            return;
        }
        ummu_process_mapt_cmd(u, base, &cmd, MAPT_UCMDQ_CI(base));
        ummu_mapt_ucmdq_cons_incr(base);
    }
    qemu_log("after cmdq process, log2size: %u, cmdpi: %u, cmdci: %u, cplpi: %u, cplci: %u\n",
             MAPT_UCMDQ_LOG2SIZE(base), MAPT_UCMDQ_PI(base), MAPT_UCMDQ_CI(base),
             MAPT_UCPLQ_PI(base), MAPT_UCPLQ_CI(base));
}

static void ummu_mapt_ctrlr_page_write_process(UMMUState *u, hwaddr offset, uint64_t data)
{
    MAPTCmdqBase base;
    uint32_t qid = ummu_mapt_cmdq_get_qid(u, offset);
    dma_addr_t addr = MAPT_CMDQ_CTXT_BASE_ADDR(u->mapt_cmdq_ctxt_base);
    int ret;

    qemu_log("qid: %u, mapt_ctxt_base: 0x%lx\n", qid, addr);
    ret = ummu_mapt_get_cmdq_base(u, addr, qid, &base);
    if (ret) {
        qemu_log("failed to get mapt cmdq base.\n");
        return;
    }

    switch (offset & UCMDQ_UCPLQ_CI_PI_MASK) {
        case UCMDQ_PI:
            ummu_mapt_cmdq_base_update_ucmdq_pi(&base, (uint16_t)data);
            ummu_process_mapt_cmdq(u, &base);
            break;
        case UCMDQ_CI:
            ummu_mapt_cmdq_base_update_ucmdq_ci(&base, (uint16_t)data);
            break;
        case UCPLQ_PI:
            ummu_mapt_cmdq_base_update_ucplq_pi(&base, (uint16_t)data);
            break;
        case UCPLQ_CI:
            ummu_mapt_cmdq_base_update_ucplq_ci(&base, (uint16_t)data);
            break;
        default:
            qemu_log("cannot process addr(0x%lx) mpat ctrlr page write.\n", offset);
            return;
    }

    ret = ummu_mapt_update_cmdq_base(u, addr, qid, &base);
    if (ret) {
        qemu_log("failed to update mapt cmdq ctx.\n");
        return;
    }
}

static void ummu_reg_writew(UMMUState *u, hwaddr offset, uint64_t data)
{
    switch (offset) {
        case A_UCMDQ_PI_START_REG...A_UCPLQ_CI_END_REG:
            ummu_mapt_ctrlr_page_write_process(u, offset, data);
            break;
        default:
            qemu_log("ummu cannot handle 16-bit write access at: 0x%lx\n", offset);
            break;
    }
}

static int ummu_mapt_process_release_um_queue(UMMUState *u)
{
    MAPTCmdqBase base;
    uint32_t qid = u->release_um_queue_id;
    dma_addr_t addr = MAPT_CMDQ_CTXT_BASE_ADDR(u->mapt_cmdq_ctxt_base);

    memset(&base, 0, sizeof(base));
    if (ummu_mapt_update_cmdq_base(u, addr, qid, &base)) {
        qemu_log("failed to release um queue(qid: %u)\n", qid);
        return -1;
    }

    qemu_log("release um queue(qid: %u) success.\n", qid);
    return 0;
}

static void ummu_reg_writel(UMMUState *u, hwaddr offset, uint64_t data)
{
    switch (offset) {
        case A_CTRL0:
            update_reg32_by_wmask(&u->ctrl[0], data, UMMU_CTRL0_WMASK);
            ummu_cr0_process_task(u);
            break;
        case A_CTRL1:
            update_reg32_by_wmask(&u->ctrl[1], data, UMMU_CTRL1_WMASK);
            break;
        case A_CTRL2:
            update_reg32_by_wmask(&u->ctrl[2], data, UMMU_CTRL2_WMASK);
            break;
        case A_CTRL3:
            update_reg32_by_wmask(&u->ctrl[3], data, UMMU_CTRL3_WMASK);
            break;
        case A_TECT_BASE_CFG:
            update_reg32_by_wmask(&u->tect_base_cfg, data, UMMU_TECT_BASE_CFG_WMASK);
            break;
        case A_MCMD_QUE_BASE...A_MCMD_QUE_LASTEST_CI:
            ummu_mcmdq_reg_writel(u, offset, data);
            break;
        case A_EVENT_QUE_PI:
            update_reg32_by_wmask(&u->eventq.queue.prod, data, UMMU_EVENTQ_PI_WMASK);
            break;
        case A_EVENT_QUE_CI:
            update_reg32_by_wmask(&u->eventq.queue.cons, data, UMMU_EVENTQ_CI_WMASK);
            break;
        case A_EVENT_QUE_USI_DATA:
            update_reg32_by_wmask(&u->eventq.usi_data, data, UMMU_EVENT_QUE_USI_DATA_WMASK);
            break;
        case A_EVENT_QUE_USI_ATTR:
            update_reg32_by_wmask(&u->eventq.usi_attr, data, UMMU_EVENTQ_USI_ATTR_WMASK);
            break;
        case A_GLB_ERR_INT_USI_DATA:
            update_reg32_by_wmask(&u->glb_err.usi_data, data, UMMU_GLB_ERR_INT_USI_DATA_WMASK);
            break;
        case A_GLB_ERR_INT_USI_ATTR:
            update_reg32_by_wmask(&u->glb_err.usi_attr, data, UMMU_GLB_ERR_INT_USI_ATTR_WMASK);
            break;
        case A_GLB_INT_EN:
            ummu_glb_int_en_process(u, data);
            break;
        case A_GLB_ERR_RESP:
             update_reg32_by_wmask(&u->glb_err.glb_err_resp, data, UMMU_GLB_ERR_RESP_WMASK);
             break;
        case A_RELEASE_UM_QUEUE:
            /* release_um_queue reg set 1 to release um_queue */
            if ((data & RELEASE_UM_QUEUE_WMASK) != 1) {
                break;
            }
            if (ummu_mapt_process_release_um_queue(u)) {
                u->release_um_queue = 1;
                break;
            }
            /* release success, set release_um_queue reg to 0, means release success */
            u->release_um_queue = 0;
            break;
        case A_RELEASE_UM_QUEUE_ID:
            update_reg32_by_wmask(&u->release_um_queue_id, data, RELEASE_UM_QUEUE_ID_WMASK);
            break;
        case A_UCMDQ_PI_START_REG...A_UCPLQ_CI_END_REG:
            ummu_mapt_ctrlr_page_write_process(u, offset, data);
            break;
        case A_UMCMD_PAGE_SEL:
            qemu_log("ucmdq set page sel to %s\n",
                     data == MAPT_CMDQ_CTRLR_PAGE_SIZE_4K ? "4K" : "64K");
            update_reg32_by_wmask(&u->ucmdq_page_sel, data, UMCMD_PAGE_SEL_WMASK);
            break;
        case A_DSTEID_KV_TABLE_BASE_CFG:
        case A_UMMU_DSTEID_KV_TABLE_HASH_CFG0:
        case A_UMMU_DSTEID_KV_TABLE_HASH_CFG1:
        case A_UMMU_USER_CONFIG0...A_UMMU_USER_CONFIG11:
        case A_UMMU_MEM_USI_DATA:
        case A_UMMU_MEM_USI_ATTR:
        case A_UMMU_INT_MASK:
        case A_UMMU_DSTEID_CAM_TABLE_BASE_CFG:
            /* do nothing */
            break;
        default:
            qemu_log("ummu cannot handle 32-bit write access at 0x%lx\n", offset);
            break;
    }
}

static void ummu_mcmdq_reg_writell(UMMUState *u, hwaddr offset, uint64_t data)
{
    uint8_t mcmdq_idx;

    mcmdq_idx = (uint8_t)(offset & MCMDQ_IDX_MASK) >> __bf_shf(MCMDQ_IDX_MASK);
    if (mcmdq_idx >= UMMU_MAX_MCMDQS) {
        qemu_log("invalid idx %u, offset is 0x%lx\n", mcmdq_idx, offset);
        return;
    }

    switch (offset & MCMDQ_BASE_ADDR_MASK) {
        case A_MCMD_QUE_BASE:
            update_reg64_by_wmask(&u->mcmdqs[mcmdq_idx].queue.base, data, UMMU_MCMDQ_BASE_WMASK);
            u->mcmdqs[mcmdq_idx].queue.log2size = MCMD_QUE_LOG2SIZE(data);
            trace_ummu_mcmdq_base_reg_writell(mcmdq_idx, u->mcmdqs[mcmdq_idx].queue.base,
                                              u->mcmdqs[mcmdq_idx].queue.log2size);
            break;
        default:
            qemu_log("ummu cannot handle 64-bit mcmdq reg write access at 0x%lx\n", offset);
            break;
    }
}

static void ummu_reg_writell(UMMUState *u, hwaddr offset, uint64_t data)
{
    switch (offset) {
        case A_TECT_BASE0:
            update_reg64_by_wmask(&u->tect_base, data, UMMU_TECT_BASE_WMASK);
            break;
        case A_MCMD_QUE_BASE...A_MCMD_QUE_LASTEST_CI:
            ummu_mcmdq_reg_writell(u, offset, data);
            break;
        case A_EVENT_QUE_BASE0:
            update_reg64_by_wmask(&u->eventq.queue.base, data, UMMU_EVENTQ_BASE_WMASK);
            u->eventq.queue.log2size = EVENT_QUE_LOG2SIZE(data);
            trace_ummu_eventq_req_writell(u->eventq.queue.base, u->eventq.queue.log2size);
            break;
        case A_EVENT_QUE_USI_ADDR0:
            update_reg64_by_wmask(&u->eventq.usi_addr, data, UMMU_EVENTQ_USI_ADDR_WMASK);
            trace_ummu_eventq_usi_reg_writell(data);
            break;
        case A_GLB_ERR_INT_USI_ADDR0:
            update_reg64_by_wmask(&u->glb_err.usi_addr, data, UMMU_GLB_ERR_INT_USI_ADDR_WMASK);
            trace_ummu_glberr_usi_reg_writell(data);
            break;
        case A_MAPT_CMDQ_CTXT_BADDR0:
            update_reg64_by_wmask(&u->mapt_cmdq_ctxt_base, data, MAPT_CMDQ_CTXT_BADDR_WMASK);
            trace_ummu_mapt_ctx_base_reg_writell(u->mapt_cmdq_ctxt_base);
            break;
        case A_DSTEID_KV_TABLE_BASE0:
        case A_UMMU_DSTEID_CAM_TABLE_BASE0:
        case A_UMMU_MEM_USI_ADDR0:
            /* do nothing */
            break;
        default:
            qemu_log("ummu cannot handle 64-bit write access at 0x%lx\n", offset);
            break;
    }
}

static void ummu_reg_write(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    UMMUState *u = opaque;

    switch (size) {
        case 2:
            ummu_reg_writew(u, offset, data);
            break;
        case 4:
            ummu_reg_writel(u, offset, data);
            break;
        case 8:
            ummu_reg_writell(u, offset, data);
            break;
        default:
            qemu_log("cann't process ummu reg write for size: %u\n", size);
            break;
    }
}

static const MemoryRegionOps ummu_reg_ops = {
    .read = ummu_reg_read,
    .write = ummu_reg_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 2,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 2,
        .max_access_size = 8,
    },
};

static int ummu_iommu_get_info(UMMUDevice *ummu_dev, uint32_t *data_type,
                               uint32_t data_len, void *data)
{
    if (!ummu_dev || !ummu_dev->idev) {
        return -ENOENT;
    }

    return iommufd_device_get_info(ummu_dev->idev, data_type, data_len, data);
}

static int ummu_init_hw_regs(UMMUDevice *ummu_dev)
{
    struct iommu_hw_info_ummu *info = &ummu_dev->info;
    uint32_t data_type;
    int ret;

    ret = ummu_iommu_get_info(ummu_dev, &data_type, sizeof(*info), info);
    if (ret) {
        error_report("Failed to get UMMU device info");
        return ret;
    }

    if (data_type != IOMMU_HW_INFO_TYPE_UMMU) {
        error_report("Wrong data type (%u)!, expect (%u)", data_type, IOMMU_HW_INFO_TYPE_UMMU);
        return -ENOENT;
    }

    return 0;
}

static void ummu_registers_init(UMMUState *u)
{
    int i;

    memset(u->cap, 0, sizeof(u->cap));
    /* cap 0 init */
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, DSTEID_SIZE,          0x10);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, TOKENID_SIZE,         0x14);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, ATTR_PERMS_OVR,       0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, ATTR_TYPES_OVR,       0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, S2_ATTR_TYPE,         0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, TCT_LEVEL,            0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, TECT_MODE,            0x1);
    u->cap[0] = FIELD_DP32(u->cap[0], CAP0, TECT_LEVEL,           0x1);
    /* cap 1 init */
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, EVENTQ_SIZE,          0x13);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, EVENTQ_NUMB,          0x0);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, EVENTQ_SUPPORT,       0x1);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, MCMDQ_SIZE,           0xF);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, MCMDQ_NUMB,           0x3);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, MCMDQ_SUPPORT,        0x1);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, EVENT_GEN,            0x1);
    u->cap[1] = FIELD_DP32(u->cap[1], CAP1, STALL_MAX,            0x80);
    /* cap 2 init */
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, VMID_TLBI,            0x0);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, TLB_BOARDCAST,        0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, RANGE_TLBI,           0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, OA_SIZE,              0x5);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, GRAN4K_T,             0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, GRAN16K_T,            0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, GRAN64K_T,            0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, VA_EXTEND,            0x0);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, S2_TRANS,             0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, S1_TRANS,             0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, SMALL_TRANS,          0x1);
    u->cap[2] = FIELD_DP32(u->cap[2], CAP2, TRANS_FORM,           0x2);
    /* cap 3 init */
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, HIER_ATTR_DISABLE,    0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, S2_EXEC_NEVER_CTRL,   0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, BBM_LEVEL,            0x2);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, COHERENT_ACCESS,      0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, TTENDIAN_MODE,        0x0);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, MTM_SUPPORT,          0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, HTTU_SUPPORT,         0x2);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, HYP_S1CONTEXT,        0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, USI_SUPPORT,          0x1);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, STALL_MODEL,          0x0);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, TERM_MODEL,           0x0);
    u->cap[3] = FIELD_DP32(u->cap[3], CAP3, SATI_MAX,             0x1);
    /* cap 4 init */
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UCMDQ_UCPLQ_NUMB,     0x10);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UCMDQ_SIZE,           0xF);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UCPLQ_SIZE,           0xF);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UIEQ_SIZE,            0xF);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UIEQ_NUMB,            0x5);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, UIEQ_SUPPORT,         0x1);
    u->cap[4] = FIELD_DP32(u->cap[4], CAP4, PPLB_SUPPORT,         0x0);

    /* cap 5 init */
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, MAPT_SUPPORT,         0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, MAPT_MODE,            0x3);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, GRAN2M_P,             0x0);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, GRAN4K_P,             0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, TOKENVAL_CHK,         0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, TOKENVAL_CHK_MODE,    0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, RANGE_PLBI,           0x1);
    u->cap[5] = FIELD_DP32(u->cap[5], CAP5, PLB_BORDCAST,         0x0);
    /* cap 6 init */
    u->cap[6] = FIELD_DP32(u->cap[6], CAP6, MTM_ID_MAX,           0x00FF);
    u->cap[6] = FIELD_DP32(u->cap[6], CAP6, MTM_GP_MAX,           0x03);

    /* ctrlr init */
    memset(u->ctrl, 0, sizeof(u->ctrl));
    u->ctrl[1] = FIELD_DP32(u->ctrl[1], CTRL1, TECT_MODE_SEL,     0x1);

    /* tect init */
    u->tect_base = 0;
    u->tect_base_cfg = 0;

    /* mcmdq init */
    for (i = 0; i < UMMU_MAX_MCMDQS; i++) {
        u->mcmdqs[i].queue.base = 0;
        u->mcmdqs[i].queue.prod = 0;
        u->mcmdqs[i].queue.cons = 0;
        u->mcmdqs[i].queue.entry_size = sizeof(UMMUMcmdqCmd);
    }

    /* eventq init */
    memset(&u->eventq, 0, sizeof(u->eventq));

    /* glb err init */
    memset(&u->glb_err, 0, sizeof(u->glb_err));

    /* evt queue init */
    u->eventq.queue.base = 0;
    u->eventq.queue.prod = 0;
    u->eventq.queue.cons = 0;
    u->eventq.queue.entry_size = sizeof(UMMUEvent);

    /* mapt cmdq ctxt base addr init */
    u->mapt_cmdq_ctxt_base = 0;

    /* umcmdq default page set to 4K */
    u->ucmdq_page_sel = MAPT_CMDQ_CTRLR_PAGE_SIZE_4K;
}

int ummu_associating_with_ubc(BusControllerState *ubc)
{
    UMMUState *ummu;
    unsigned int bus_num;

    if (1 != sscanf(ubc->bus->qbus.name, "ubus.%u", &bus_num)) {
        qemu_log("failed to get bus num %s\n",
                 ubc->bus->qbus.name);
        return -1;
    }
    ummu = ummu_find_by_bus_num(bus_num);
    if (!ummu) {
        qemu_log("failed to get ummu %u\n", bus_num);
        return -1;
    }
    return 0;
}

static UMMUDevice *ummu_get_udev(UBBus *bus, UMMUState *u, uint32_t eid)
{
    UMMUDevice *ummu_dev = NULL;
    UBDevice *udev = NULL;
    char *name = NULL;

    udev = ub_find_device_by_eid(bus, eid);
    ummu_dev = g_hash_table_lookup(u->ummu_devs, udev);
    if (ummu_dev) {
        return ummu_dev;
    }

    /* will be freed when remove from hash table */
    ummu_dev = g_new0(UMMUDevice, 1);
    ummu_dev->ummu = u;
    ummu_dev->udev = udev;

    name = g_strdup_printf("%s-0x%x", u->mrtypename, eid);
    memory_region_init_iommu(&ummu_dev->iommu, sizeof(ummu_dev->iommu), u->mrtypename,
                             OBJECT(u), name, UINT64_MAX);
    address_space_init(&ummu_dev->as_sysmem, &u->root, name);
    address_space_init(&ummu_dev->as, MEMORY_REGION(&ummu_dev->iommu), name);
    g_free(name);
    g_hash_table_insert(u->ummu_devs, udev, ummu_dev);

    return ummu_dev;
}

static AddressSpace *ummu_find_add_as(UBBus *bus, void *opaque, uint32_t eid)
{
    UMMUState *u = opaque;
    UMMUDevice *ummu_dev = ummu_get_udev(bus, u, eid);

    if (u->nested && !ummu_dev->s1_hwpt) {
        return &ummu_dev->as_sysmem;
    }

    return &ummu_dev->as;
}

static bool ummu_is_nested(void *opaque)
{
    UMMUState *u = opaque;

    return u->nested;
}

static bool ummu_dev_attach_viommu(UMMUDevice *udev,
                                   HostIOMMUDeviceIOMMUFD *idev, Error **errp)
{
    UMMUState *u = udev->ummu;
    UMMUS2Hwpt *s2_hwpt = NULL;
    UMMUViommu *viommu = NULL;
    uint32_t s2_hwpt_id;

    if (u->viommu) {
        return host_iommu_device_iommufd_attach_hwpt(
            idev, u->viommu->s2_hwpt->hwpt_id, errp);
    }

    if (!iommufd_backend_alloc_hwpt(idev->iommufd, idev->devid, idev->ioas_id,
                                    IOMMU_HWPT_ALLOC_NEST_PARENT,
                                    IOMMU_HWPT_DATA_NONE, 0, NULL,
                                    &s2_hwpt_id, NULL, errp)) {
        error_setg(errp, "failed to allocate an S2 hwpt");
        return false;
    }

    if (!host_iommu_device_iommufd_attach_hwpt(idev, s2_hwpt_id, errp)) {
        error_setg(errp, "failed to attach stage-2 HW pagetable");
        goto free_s2_hwpt;
    }

    viommu = g_new0(UMMUViommu, 1);
    viommu->core = iommufd_backend_alloc_viommu(idev->iommufd, idev->devid,
                                                IOMMU_VIOMMU_TYPE_UMMU,
                                                s2_hwpt_id);
    if (!viommu->core) {
        error_setg(errp, "failed to allocate a viommu");
        goto free_viommu;
    }

    s2_hwpt = g_new0(UMMUS2Hwpt, 1);
    s2_hwpt->iommufd = idev->iommufd;
    s2_hwpt->hwpt_id = s2_hwpt_id;
    s2_hwpt->ioas_id = idev->ioas_id;
    qemu_log("alloc hwpt for s2 success, hwpt id is %u\n", s2_hwpt_id);

    viommu->iommufd = idev->iommufd;
    viommu->s2_hwpt = s2_hwpt;

    u->viommu = viommu;
    return true;

free_viommu:
    g_free(viommu);
    host_iommu_device_iommufd_attach_hwpt(idev, udev->idev->ioas_id, errp);
free_s2_hwpt:
    iommufd_backend_free_id(idev->iommufd, s2_hwpt_id);

    return false;
}

static bool ummu_dev_set_iommu_dev(UBBus *bus, void *opaque, uint32_t eid,
                                   HostIOMMUDevice *hiod, Error **errp)
{
    HostIOMMUDeviceIOMMUFD *idev = HOST_IOMMU_DEVICE_IOMMUFD(hiod);
    UMMUState *u = opaque;
    UMMUDevice *ummu_dev = NULL;

    if (!u->nested) {
        error_setg(errp, "set iommu dev expcet ummu is nested mode\n");
        return false;
    }

    if (!idev) {
        error_setg(errp, "unexpect idev is NULL\n");
        return false;
    }

    ummu_dev = ummu_get_udev(bus, u, eid);
    if (!ummu_dev) {
        error_setg(errp, "failed to get ummu dev by eid 0x%x\n", eid);
        return false;
    }

    if (ummu_dev->idev) {
        if (ummu_dev->idev != idev) {
            error_setg(errp, "udev(%s) exist idev conflict new config idev\n", ummu_dev->udev->name);
            return false;
        } else {
            return true;
        }
    }

    if (!ummu_dev_attach_viommu(ummu_dev, idev, errp)) {
        error_report("Unable to attach viommu");
        return false;
    }

    ummu_dev->idev = idev;
    ummu_dev->viommu = u->viommu;
    QLIST_INSERT_HEAD(&u->viommu->device_list, ummu_dev, next);

    u->set_custom_config(u);

    return true;
}

static void ummu_dev_unset_iommu_dev(UBBus *bus, void *opaque, uint32_t eid)
{
    UMMUDevice *ummu_dev;
    UMMUViommu *viommu = NULL;
    UMMUVdev *vdev = NULL;
    UMMUState *u = opaque;
    UBDevice *udev = NULL;

    if (!u->nested) {
        return;
    }

    udev = ub_find_device_by_eid(bus, eid);
    ummu_dev = g_hash_table_lookup(u->ummu_devs, udev);
    if (!ummu_dev) {
        return;
    }

    if (!host_iommu_device_iommufd_attach_hwpt(ummu_dev->idev,
                                               ummu_dev->idev->ioas_id, NULL)) {
        error_report("Unable to attach dev to the default HW pagetable");
    }

    vdev = ummu_dev->vdev;
    viommu = ummu_dev->viommu;

    ummu_dev->idev = NULL;
    ummu_dev->viommu = NULL;
    QLIST_REMOVE(ummu_dev, next);

    if (vdev) {
        iommufd_backend_free_id(viommu->iommufd, vdev->core->vdev_id);
        g_free(vdev->core);
        g_free(vdev);
    }

    if (QLIST_EMPTY(&viommu->device_list)) {
        iommufd_backend_free_id(viommu->iommufd, viommu->core->viommu_id);
        g_free(viommu->core);
        iommufd_backend_free_id(viommu->iommufd, viommu->s2_hwpt->hwpt_id);
        g_free(viommu->s2_hwpt);
        g_free(viommu);
        u->viommu = NULL;
    }
}

static const UBIOMMUOps ummu_ops = {
    .get_address_space = ummu_find_add_as,
    .ummu_is_nested = ummu_is_nested,
    .set_iommu_device = ummu_dev_set_iommu_dev,
    .unset_iommu_device = ummu_dev_unset_iommu_dev,
};

static void ub_save_ummu_list(UMMUState *u)
{
    QLIST_INSERT_HEAD(&ub_umms, u, node);
}

static void ub_remove_ummu_list(UMMUState *u)
{
    QLIST_REMOVE(u, node);
}

static void ummu_set_custom_config(UMMUState *u)
{
    uint32_t val;
    UMMUDevice *ummu_dev = NULL;

    if (!u->viommu) {
        qemu_log("Failed to get host ummu info, viommu is NULL\n");
        return;
    }

    ummu_dev = QLIST_FIRST(&u->viommu->device_list);
    if (!ummu_dev) {
        qemu_log("Failed to get host ummu info, ummu_dev is NULL\n");
        return;
    }

    if((ummu_dev->info.iidr && ummu_dev->info.aidr) || !ummu_init_hw_regs(ummu_dev)) {
        val = FIELD_EX32(ummu_dev->info.iidr, IIDR, PROD_REVISION);
        u->iidr = FIELD_DP32(u->iidr, IIDR, PROD_REVISION, val);
        qemu_log("IIDR, PROD_REVISION:%u\n", val);
        val = FIELD_EX32(ummu_dev->info.iidr, IIDR, PROD_VARIANT);
        u->iidr = FIELD_DP32(u->iidr, IIDR, PROD_VARIANT, val);
        qemu_log("IIDR, PROD_VARIANT:%u\n", val);
        val = FIELD_EX32(ummu_dev->info.iidr, IIDR, PROD_ID);
        u->iidr = FIELD_DP32(u->iidr, IIDR, PROD_ID, val);
        qemu_log("IIDR, PROD_ID:%u\n", val);
        val = FIELD_EX32(ummu_dev->info.aidr, AIDR, ARCH_MINOR_REV);
        u->aidr = FIELD_DP32(u->aidr, AIDR, ARCH_MINOR_REV, val);
        qemu_log("AIDR, ARCH_MINOR_REV:%u\n", val);
        val = FIELD_EX32(ummu_dev->info.aidr, AIDR, ARCH_MAJOR_REV);
        u->aidr = FIELD_DP32(u->aidr, AIDR, ARCH_MAJOR_REV, val);
        qemu_log("AIDR, ARCH_MAJOR_REV:%u\n", val);
    } else {
        qemu_log("Failed to get host ummu info\n");
    }
}

static void ummu_base_realize(DeviceState *dev, Error **errp)
{
    static uint8_t NO = 0;
    UMMUState *u = UB_UMMU(dev);
    SysBusDevice *sysdev = SYS_BUS_DEVICE(dev);

    u->bus_num = NO;
    sysdev->parent_obj.id = g_strdup_printf("ummu.%u", NO++);

    memory_region_init_io(&u->ummu_reg_mem, OBJECT(u), &ummu_reg_ops,
                          u, TYPE_UB_UMMU, u->ummu_reg_size);
    sysbus_init_mmio(sysdev, &u->ummu_reg_mem);

    memset(u->usi_virq, -1, sizeof(u->usi_virq));
    ummu_registers_init(u);
    u->set_custom_config = ummu_set_custom_config;
    ub_save_ummu_list(u);

    u->ummu_devs = g_hash_table_new_full(NULL, NULL, NULL, g_free);
    u->configs = g_hash_table_new_full(NULL, NULL, NULL, g_free);
    QLIST_INIT(&u->kvtbl);
    u->kvtbl_entrys = 0;
    if (u->primary_bus) {
        ub_setup_iommu(u->primary_bus, &ummu_ops, u);
    } else {
        error_setg(errp, "UMMU is not attached to any UB bus!");
    }

    u->tecte_tag_num = 0;
    u->mrtypename = TYPE_UMMU_IOMMU_MEMORY_REGION;
    if (u->nested) {
        memory_region_init(&u->stage2, OBJECT(u), "stage2", UINT64_MAX);
        memory_region_init_alias(&u->sysmem, OBJECT(u),
                                 "ummu-sysmem", get_system_memory(), 0,
                                 memory_region_size(get_system_memory()));
        memory_region_add_subregion(&u->stage2, 0, &u->sysmem);

        memory_region_init(&u->root, OBJECT(u), "ummu-root", UINT64_MAX);
        memory_region_add_subregion(&u->root, 0, &u->stage2);
    }
}

static void ummu_kvtbl_reset(UMMUState *u)
{
    UMMUKVTblEntry *entry = NULL;
    UMMUKVTblEntry *next_entry = NULL;

    QLIST_FOREACH_SAFE(entry, &u->kvtbl, list, next_entry) {
        QLIST_REMOVE(entry, list);
        u->kvtbl_entrys--;
        g_free(entry);
    }
}

static void ummu_base_unrealize(DeviceState *dev)
{
    UMMUState *u = UB_UMMU(dev);
    SysBusDevice *sysdev = SYS_BUS_DEVICE(dev);


    ub_remove_ummu_list(u);
    if (sysdev->parent_obj.id) {
        g_free(sysdev->parent_obj.id);
    }

    if (u->ummu_devs) {
        g_hash_table_remove_all(u->ummu_devs);
        g_hash_table_destroy(u->ummu_devs);
        u->ummu_devs = NULL;
    }

    if (u->configs) {
        g_hash_table_remove_all(u->configs);
        g_hash_table_destroy(u->configs);
        u->configs = NULL;
    }

    ummu_kvtbl_reset(u);
}

static void ummu_base_reset(DeviceState *dev)
{
    UMMUState *u = UB_UMMU(dev);

    ummu_kvtbl_reset(u);
}

static Property ummu_dev_properties[] = {
    DEFINE_PROP_UINT64("ub-ummu-reg-size", UMMUState,
                       ummu_reg_size, 0),
    DEFINE_PROP_LINK("primary-bus", UMMUState, primary_bus,
                     TYPE_UB_BUS, UBBus *),
    DEFINE_PROP_BOOL("nested", UMMUState, nested, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void ummu_base_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, ummu_dev_properties);
    dc->realize = ummu_base_realize;
    dc->unrealize = ummu_base_unrealize;
    dc->reset = ummu_base_reset;
}

static const TypeInfo ummu_base_info = {
    .name          = TYPE_UB_UMMU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(UMMUState),
    .class_data    = NULL,
    .class_size    = sizeof(UMMUBaseClass),
    .class_init    = ummu_base_class_init,
};

static int ummu_get_tecte(UMMUState *ummu, dma_addr_t addr, TECTE *tecte)
{
    int ret, i;

    ret = dma_memory_read(&address_space_memory, addr, tecte, sizeof(*tecte),
                          MEMTXATTRS_MEMORY);
    if (ret != MEMTX_OK) {
        qemu_log("Cannot fetch tecte at address=0x%lx\n", addr);
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(tecte->word); i++) {
        le32_to_cpus(&tecte->word[i]);
    }

    return 0;
}

static uint32_t ummu_get_tecte_tag_by_dest_eid(UMMUState *u, uint32_t dst_eid)
{
    UMMUKVTblEntry *entry = NULL;

    QLIST_FOREACH(entry, &u->kvtbl, list) {
        if (entry->dst_eid == dst_eid) {
            break;
        }
    }

    if (!entry) {
        qemu_log("cannot find tecte_tag by dst_eid 0x%x\n", dst_eid);
        return UINT32_MAX;
    }
    qemu_log("success get tecte_tag(0x%x) by dst_eid(0x%x)\n", entry->tecte_tag, dst_eid);

    return entry->tecte_tag;
}

static int ummu_find_tecte(UMMUState *ummu, uint32_t tecte_tag, TECTE *tecte)
{
    dma_addr_t tect_base_addr = TECT_BASE_ADDR(ummu->tect_base);
    dma_addr_t tecte_addr;
    int ret;
    int i;

    if (ummu_tect_fmt_2level(ummu)) {
        int l1_tecte_offset, l2_tecte_offset;
        uint32_t split;
        dma_addr_t l1ptr, l2ptr;
        TECTEDesc l1_tecte_desc;

        split = ummu_tect_split(ummu);
        l1_tecte_offset = tecte_tag >> split;
        l2_tecte_offset = tecte_tag & ((1 << split) - 1);
        l1ptr = (dma_addr_t)(tect_base_addr + l1_tecte_offset * sizeof(l1_tecte_desc));

        ret = dma_memory_read(&address_space_memory, l1ptr, &l1_tecte_desc,
                              sizeof(l1_tecte_desc), MEMTXATTRS_MEMORY);
        if (ret != MEMTX_OK) {
            qemu_log("dma read failed for tecte level1 desc.\n");
            return -EINVAL;
        }

        for (i = 0; i < ARRAY_SIZE(l1_tecte_desc.word); i++) {
            le32_to_cpus(&l1_tecte_desc.word[i]);
        }

        if (TECT_DESC_V(&l1_tecte_desc) == 0) {
            qemu_log("tecte desc is invalid\n");
            return -EINVAL;
        }

        l2ptr = TECT_L2TECTE_PTR(&l1_tecte_desc);
        tecte_addr = l2ptr + l2_tecte_offset * sizeof(*tecte);
    } else {
        qemu_log("liner table process not support\n");
        return -EINVAL;
    }

    if (ummu_get_tecte(ummu, tecte_addr, tecte)) {
        qemu_log("failed to get tecte.\n");
        return -EINVAL;
    }

    return 0;
}

static int ummu_decode_tecte(UMMUState *ummu, UMMUTransCfg *cfg,
                             TECTE *tecte, UMMUEventInfo *event)
{
    if (TECTE_VALID(tecte) == 0) {
        qemu_log("fetched tecte is invalid\n");
        return -EINVAL;
    }

    cfg->tct_ptr = TECTE_TCT_PTR(tecte);
    cfg->tct_num = TECTE_TCT_NUM(tecte);
    cfg->tct_fmt = TECTE_TCT_FMT(tecte);

    qemu_log("tct_ptr: 0x%lx, tct_num: %lu, fmt: %lu\n",
             cfg->tct_ptr, cfg->tct_num, cfg->tct_fmt);
    return 0;
}

static int ummu_get_tcte(UMMUState *ummu, dma_addr_t addr,
                         TCTE *tcte, uint32_t tid)
{
    int ret, i;
    uint64_t *_tcte;

    ret = dma_memory_read(&address_space_memory, addr, tcte, sizeof(*tcte),
                          MEMTXATTRS_MEMORY);
    if (ret != MEMTX_OK) {
        qemu_log("Cannot fetch tcte at address=0x%lx\n", addr);
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(tcte->word); i++) {
        le32_to_cpus(&tcte->word[i]);
    }

    _tcte = (uint64_t *)tcte;
    qemu_log("fetch tcte(%u): <0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx>\n",
             tid, _tcte[0], _tcte[1], _tcte[2], _tcte[3], _tcte[4], _tcte[5], _tcte[6], _tcte[7]);
    return 0;
}

static int ummu_find_tcte(UMMUState *ummu, UMMUTransCfg *cfg, uint32_t tid,
                          TCTE *tcte, UMMUEventInfo *event)
{
    int l1idx, l2idx;
    dma_addr_t tct_lv1_addr, tcte_addr;
    TCTEDesc tct_desc;
    int ret, i;

    if (cfg->tct_num == 0 || tid >= TCTE_MAX_NUM(cfg->tct_num)) {
        event->type = EVT_BAD_TOKENID;
        return -EINVAL;
    }

    if (TCT_FMT_LINEAR == cfg->tct_fmt || TCT_FMT_LVL2_4K == cfg->tct_fmt) {
        event->type = EVT_TCT_FETCH;
        qemu_log("current dont support TCT_FMT_LINEAR&TCT_FMT_LVL2_4K.\n");
        return -EINVAL;
    }

    l1idx = tid >> TCT_SPLIT_64K;
    tct_lv1_addr = cfg->tct_ptr + l1idx * sizeof(tct_desc);
    ret = dma_memory_read(&address_space_memory, tct_lv1_addr, &tct_desc, sizeof(tct_desc),
                          MEMTXATTRS_MEMORY);
    if (ret != MEMTX_OK) {
        event->type = EVT_TCT_FETCH;
        qemu_log("failed to dma read tct lv1 entry.\n");
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(tct_desc.word); i++) {
        le32_to_cpus(&tct_desc.word[i]);
    }

    qemu_log("l1idx: %d, tct_l1_addr: 0x%lx, tct_desc: 0x%lx, tcte_ptr: 0x%llx, l1tcte_v: %u\n",
             l1idx, tct_lv1_addr, *(uint64_t *)&tct_desc, TCT_L2TCTE_PTR(&tct_desc), TCT_L1TCTE_V(&tct_desc));

    if (TCT_L1TCTE_V(&tct_desc) == 0) {
        event->type = EVT_BAD_TOKENID;
        qemu_log("l2tcte is invalid\n");
        return -EINVAL;
    }

    l2idx = tid & (TCT_L2_ENTRIES - 1);
    tcte_addr = TCT_L2TCTE_PTR(&tct_desc) + l2idx * sizeof(*tcte);
    qemu_log("l2idx: %d, tcte_addr: 0x%lx\n", l2idx, tcte_addr);
    ret = ummu_get_tcte(ummu, tcte_addr, tcte, tid);
    if (ret) {
        event->type = EVT_TCT_FETCH;
        qemu_log("failed to get tcte, ret = %d\n", ret);
        return ret;
    }

    return 0;
}

static int ummu_decode_tcte(UMMUState *ummu, UMMUTransCfg *cfg,
                            TCTE *tcte, UMMUEventInfo *event)
{
    uint32_t tct_v = TCTE_TCT_V(tcte);

    if (tct_v == 0) {
        qemu_log("fetched tcte invalid\n");
        event->type = EVT_BAD_TCT;
        return -1;
    }

    cfg->tct_ttba = TCTE_TTBA(tcte);
    cfg->tct_sz = TCTE_SZ(tcte);
    cfg->tct_tgs = tgs2granule(TCTE_TGS(tcte));
    qemu_log("tcte_tbba: 0x%lx, sz: %u, tgs: %u, tct_v: %u\n",
             cfg->tct_ttba, cfg->tct_sz, cfg->tct_tgs, tct_v);
    return 0;
}

static int ummu_tect_parse_sparse_table(UMMUDevice *ummu_dev, UMMUTransCfg *cfg,
                                        uint32_t dest_eid, UMMUEventInfo *event)
{
    UMMUState *ummu = ummu_dev->ummu;
    int ret;
    TECTE tecte;
    TCTE tcte;
    uint32_t tecte_tag;
    uint32_t tid = ub_dev_get_token_id(ummu_dev->udev);

    tecte_tag = ummu_get_tecte_tag_by_dest_eid(ummu, dest_eid);
    if (tecte_tag == UINT32_MAX) {
        qemu_log("failed to get tecte tag by dest_eid(%u).\n", dest_eid);
        event->type = EVT_BAD_DSTEID;
        goto failed;
    }

    ret = ummu_find_tecte(ummu, tecte_tag, &tecte);
    if (ret) {
        event->type = EVT_TECT_FETCH;
        qemu_log("failed to find tecte: %d\n", ret);
        goto failed;
    }

    ret = ummu_decode_tecte(ummu, cfg, &tecte, event);
    if (ret) {
        event->type = EVT_BAD_TECT;
        qemu_log("failed to decode tecte.\n");
        goto failed;
    }

    qemu_log("get udev(%s %s) tid(%u)\n",
             ummu_dev->udev->name, ummu_dev->udev->qdev.id, tid);
    ret = ummu_find_tcte(ummu, cfg, tid, &tcte, event);
    if (ret) {
        qemu_log("failed to find tecte.\n");
        goto failed;
    }

    ret = ummu_decode_tcte(ummu, cfg, &tcte, event);
    if (ret) {
        qemu_log("failed to decode tecte.\n");
        goto failed;
    }
    cfg->tecte_tag = tecte_tag;
    cfg->tid = tid;

    return 0;

failed:
     event->tid = tid;
     event->tecte_tag = tecte_tag;
     return -EINVAL;
}

static int ummu_decode_config(UMMUDevice *ummu_dev, UMMUTransCfg *cfg, UMMUEventInfo *event)
{
    uint32_t dest_eid = ub_dev_get_ueid(ummu_dev->udev);

    qemu_log("ummu decode config dest_eid is %u.\n", dest_eid);
    if (ummu_tect_mode_sparse_table(ummu_dev->ummu)) {
        return ummu_tect_parse_sparse_table(ummu_dev, cfg, dest_eid, event);
    }

    event->type = EVT_TECT_FETCH;
    event->tecte_tag = ummu_get_tecte_tag_by_dest_eid(ummu_dev->ummu, dest_eid);

    qemu_log("current not support process linear table.\n");
    return -1;
}

static UMMUTransCfg *ummu_get_config(UMMUDevice *ummu_dev, UMMUEventInfo *event)
{
    UMMUState *ummu = ummu_dev->ummu;
    UMMUTransCfg *cfg = NULL;

    cfg = g_hash_table_lookup(ummu->configs, ummu_dev);
    if (cfg) {
        return cfg;
    }

    /* cfg will be freed when removed from hash table */
    cfg = g_new0(UMMUTransCfg, 1);
    if (!ummu_decode_config(ummu_dev, cfg, event)) {
        g_hash_table_insert(ummu->configs, ummu_dev, cfg);
    } else {
        g_free(cfg);
        cfg = NULL;
    }

    return cfg;
}

static int get_pte(dma_addr_t baseaddr, uint32_t index, uint64_t *pte)
{
    int ret;
    dma_addr_t addr = baseaddr + index * sizeof(*pte);

    ret = ldq_le_dma(&address_space_memory, addr, pte, MEMTXATTRS_UNSPECIFIED);
    if (ret) {
        qemu_log("failed to get dma data for addr 0x%lx\n", addr);
        return -EINVAL;
    }

    return 0;
}

static void ummu_ptw_64_s1(UMMUTransCfg *cfg, dma_addr_t iova, IOMMUTLBEntry *entry, UMMUPTWEventInfo *ptw_info)
{
    dma_addr_t baseaddr, indexmask;
    uint32_t granule_sz, stride, level, inputsize;

    granule_sz = cfg->tct_tgs;
    stride = VMSA_STRIDE(granule_sz);
    if (granule_sz == 0 || stride == 0) {
        qemu_log("ummu ptw 64 s1 failed: granule_sz = %u, stride = %u\n", granule_sz, stride);
        goto error;
    }
    inputsize = 64 - cfg->tct_sz;
    level = 4 - (inputsize - 4) / stride;
    indexmask = VMSA_IDXMSK(inputsize, stride, level);
    baseaddr = extract64(cfg->tct_ttba, 0, 48);
    baseaddr &= ~indexmask;

    qemu_log("stride: %u, inputsize: %u, level: %u, baseaddr: 0x%lx\n",
             stride, inputsize, level, baseaddr);
    while (level < VMSA_LEVELS) {
        uint64_t subpage_size = 1ULL << level_shift(level, granule_sz);
        uint64_t mask = subpage_size - 1;
        uint64_t pte, gpa;
        uint32_t offset = iova_level_offset(iova, inputsize, level, granule_sz);

        if (get_pte(baseaddr, offset, &pte)) {
            goto error;
        }

        if (is_invalid_pte(pte) || is_reserved_pte(pte, level)) {
            qemu_log("invalid or reserved pte.\n");
            break;
        }

        if (is_table_pte(pte, level)) {
            baseaddr = get_table_pte_address(pte, granule_sz);
            level++;
            continue;
        } else if (is_page_pte(pte, level)) {
            gpa = get_page_pte_address(pte, granule_sz);
        } else {
            uint64_t block_size;
            gpa = get_block_pte_address(pte, level, granule_sz, &block_size);
        }

        entry->translated_addr = gpa;
        entry->iova = iova & ~mask;
        entry->addr_mask = mask;

        return;
    }

error:
    ptw_info->type = UMMU_PTW_ERR_TRANSLATION;
    return;
}

static void ummu_ptw(UMMUTransCfg *cfg, dma_addr_t iova, IOMMUTLBEntry *entry, UMMUPTWEventInfo *ptw_info)
{
    ummu_ptw_64_s1(cfg, iova, entry, ptw_info);
}

static MemTxResult eventq_write(UMMUEventQueue *q, UMMUEvent *evt_in)
{
    dma_addr_t base_addr, addr;
    MemTxResult ret;
    UMMUEvent evt = *evt_in;
    int i;

    for (i = 0; i < ARRAY_SIZE(evt.word); i++) {
        cpu_to_le32s(&evt.word[i]);
    }

    base_addr = EVENT_QUE_BASE_ADDR(&q->queue);
    addr = base_addr + EVENT_QUE_WR_IDX(&q->queue) * q->queue.entry_size;
    ret = dma_memory_write(&address_space_memory, addr, &evt, sizeof(UMMUEvent),
                           MEMTXATTRS_MEMORY);
    if (ret != MEMTX_OK) {
        return ret;
    }

    ummu_eventq_prod_incr(q);
    qemu_log("eventq: addr(0x%lx), prod(%u), cons(%u)\n", addr,
             EVENT_QUE_WR_IDX(&q->queue), EVENT_QUE_RD_IDX(&q->queue));
    return MEMTX_OK;
}

static MemTxResult ummu_write_eventq(UMMUState *u, UMMUEvent *evt)
{
    UMMUEventQueue *queue = &u->eventq;
    MemTxResult r;

    if (!ummu_eventq_enabled(u)) {
        return MEMTX_ERROR;
    }

    if (ummu_eventq_full(queue)) {
        qemu_log("ummu eventq full, eventq write failed.\n");
        return MEMTX_ERROR;
    }

    r = eventq_write(queue, evt);
    if (r != MEMTX_OK) {
        return r;
    }

    if (!ummu_eventq_empty(queue)) {
        ummu_glb_usi_notify(u, UMMU_USI_VECTOR_EVETQ);
    }

    return MEMTX_OK;
}

static void ummu_record_event(UMMUState *u, UMMUEventInfo *info)
{
    UMMUEvent evt = {};
    MemTxResult r;

    if (!ummu_eventq_enabled(u)) {
        qemu_log("ummu eventq disabled.\n");
        return;
    }

    /* need set more EVT info for different event later */
    EVT_SET_TYPE(&evt, info->type);
    EVT_SET_TECTE_TAG(&evt, info->tecte_tag);
    EVT_SET_TID(&evt, info->tid);

    qemu_log("report event %s: tecte_tag %u tid %u\n",
              ummu_event_type_strings[info->type], info->tecte_tag, info->tid);

    r = ummu_write_eventq(u, &evt);
    if (r != MEMTX_OK) {
        qemu_log("ummu failed to write eventq.\n");
        /* trigger glb err irq later */
    }
}

static IOMMUTLBEntry ummu_translate(IOMMUMemoryRegion *mr, hwaddr addr,
                                    IOMMUAccessFlags flag, int iommu_idx)
{
    UMMUDevice *ummu_dev = container_of(mr, UMMUDevice, iommu);
    UMMUTransCfg *cfg = NULL;
    IOMMUTLBEntry entry = {
        .target_as = &address_space_memory,
        .iova = addr,
        .translated_addr = addr,
        .addr_mask = ~(hwaddr)0,
        .perm = IOMMU_RW,
    };
    UMMUEventInfo event = {
        .type = EVT_NONE
    };
    UMMUPTWEventInfo ptw_info = {
        .type = UMMU_PTW_ERR_NONE
    };

    cfg = ummu_get_config(ummu_dev, &event);
    if (!cfg) {
        qemu_log("failed to get ummu config.\n");
        goto epilogue;
    }

    /* need support cache TLB entry later */
    ummu_ptw(cfg, addr, &entry, &ptw_info);
    if (ptw_info.type == UMMU_PTW_ERR_NONE) {
        goto epilogue;
    }

    event.tecte_tag = cfg->tecte_tag;
    event.tid = cfg->tid;
    switch (ptw_info.type)
    {
    case UMMU_PTW_ERR_TRANSLATION:
        event.type = EVT_A_TRANSLATION;
        break;
    case UMMU_PTW_ERR_PERMISSION:
        event.type = EVT_A_PERMISSION;
        break;
    default:
        break;
    }

epilogue:
    qemu_log("ummu_translate: addr(0x%lx), translated_addr(0x%lx)\n", addr, entry.translated_addr);

    if (event.type != EVT_NONE) {
        ummu_record_event(ummu_dev->ummu, &event);
    }

    return entry;
}

static int ummu_notify_flag_changed(IOMMUMemoryRegion *iommu,
                                    IOMMUNotifierFlag old,
                                    IOMMUNotifierFlag new,
                                    Error **errp)
{
    qemu_log("ummu_notify_flag_changed\n");
    return 0;
}

void ummu_dev_uninstall_nested_tecte(UMMUDevice *ummu_dev)
{
    HostIOMMUDeviceIOMMUFD *idev = ummu_dev->idev;
    UMMUS1Hwpt *s1_hwpt = ummu_dev->s1_hwpt;
    uint32_t hwpt_id;
    UMMUVdev *vdev = NULL;

    if (!s1_hwpt || !ummu_dev->viommu) {
        return;
    }

    hwpt_id = ummu_dev->viommu->s2_hwpt->hwpt_id;
    if (!host_iommu_device_iommufd_attach_hwpt(idev, hwpt_id, NULL)) {
        error_report("Unable to attach dev to stage-2 HW pagetable");
        return;
    }

    qemu_log("uninstall s1 hwpt(%u) success\n", s1_hwpt->hwpt_id);
    iommufd_backend_free_id(idev->iommufd, s1_hwpt->hwpt_id);
    ummu_dev->s1_hwpt = NULL;
    g_free(s1_hwpt);

    vdev = ummu_dev->vdev;
    if (vdev) {
        iommufd_backend_free_id(ummu_dev->viommu->iommufd, vdev->core->vdev_id);
        g_free(vdev->core);
        g_free(vdev);
    }
    ummu_dev->vdev = NULL;
}

int ummu_dev_install_nested_tecte(UMMUDevice *ummu_dev, uint32_t data_type,
                                  uint32_t data_len, void *data)
{
    UMMUViommu *viommu = ummu_dev->viommu;
    UMMUS1Hwpt *s1_hwpt = ummu_dev->s1_hwpt;
    HostIOMMUDeviceIOMMUFD *idev = ummu_dev->idev;
    uint64_t *tecte = (uint64_t *)data;

    if (!idev || !viommu) {
        return -ENOENT;
    }

    if (s1_hwpt) {
        return 0;
    }

    s1_hwpt = g_new0(UMMUS1Hwpt, 1);
    if (!s1_hwpt) {
        return -ENOMEM;
    }

    s1_hwpt->ummu = ummu_dev->ummu;
    s1_hwpt->viommu = viommu;
    s1_hwpt->iommufd = idev->iommufd;

    if (tecte) {
        trace_ummu_dev_install_nested_tecte(tecte[0], tecte[1]);
    }

    if (!iommufd_backend_alloc_hwpt(idev->iommufd, idev->devid,
                                    viommu->core->viommu_id, 0, data_type,
                                    data_len, data, &s1_hwpt->hwpt_id, NULL, NULL)) {
        goto free;
    }

    if (!host_iommu_device_iommufd_attach_hwpt(idev, s1_hwpt->hwpt_id, NULL)) {
        goto free_hwpt;
    }

    ummu_dev->s1_hwpt = s1_hwpt;

    return 0;
free_hwpt:
    iommufd_backend_free_id(idev->iommufd, s1_hwpt->hwpt_id);
free:
    ummu_dev->s1_hwpt = NULL;
    g_free(s1_hwpt);

    return -EINVAL;
}

static void ummu_iommu_memory_region_class_init(ObjectClass *klass, void *data)
{
    IOMMUMemoryRegionClass *imrc = IOMMU_MEMORY_REGION_CLASS(klass);

    imrc->translate = ummu_translate;
    imrc->notify_flag_changed = ummu_notify_flag_changed;
}

static const TypeInfo ummu_iommu_memory_region_info = {
    .parent = TYPE_IOMMU_MEMORY_REGION,
    .name = TYPE_UMMU_IOMMU_MEMORY_REGION,
    .class_init = ummu_iommu_memory_region_class_init,
};

static void ummu_base_register_types(void)
{
    type_register_static(&ummu_base_info);
    type_register_static(&ummu_iommu_memory_region_info);
}

type_init(ummu_base_register_types)
