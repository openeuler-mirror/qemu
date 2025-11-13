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
                          MEMTXATTRS_UNSPECIFIED);
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
                             sizeof(uint32_t), MEMTXATTRS_UNSPECIFIED)) {
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
        g_free(entry);
    } else {
        qemu_log("cannot find dst_eid(0x%x) entry in kvtbl.\n", dst_eid);
    }
}

static void mcmdq_cmd_plbi_x_process(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    trace_mcmdq_cmd_plbi_x_process(mcmdq_idx, mcmdq_cmd_strings[CMD_TYPE(cmd)]);
}

static void mcmdq_cmd_tlbi_x_process(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    trace_mcmdq_cmd_tlbi_x_process(mcmdq_idx, mcmdq_cmd_strings[CMD_TYPE(cmd)]);
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
                         &result, sizeof(result), MEMTXATTRS_UNSPECIFIED)) {
        qemu_log("dma failed to wirte result(0x%x) to addr 0x%lx\n", result, addr);
        return;
    }

    qemu_log("mcmdq check pa continuity update result(0x%x) success.\n", result);
}

static void mcmdq_cmd_null(UMMUState *u, UMMUMcmdqCmd *cmd, uint8_t mcmdq_idx)
{
    uint64_t size;
    uint64_t addr;
    void *hva = NULL;
    ram_addr_t rb_offset;
    RAMBlock *rb = NULL;
    size_t rb_page_size = 0;

    if (CMD_NULL_SUBOP(cmd) != CMD_NULL_SUBOP_CHECK_PA_CONTINUITY) {
        qemu_log("current cannot process CMD_NULL subop %u.\n", CMD_NULL_SUBOP(cmd));
        return;
    }

    size = CMD_NULL_CHECK_PA_CONTI_SIZE(cmd);
    addr = CMD_NULL_CHECK_PA_CONTI_ADDR(cmd);
    hva = cpu_physical_memory_map(addr, &size, false);
    rb = qemu_ram_block_from_host(hva, false, &rb_offset);
    if (rb) {
        rb_page_size = qemu_ram_pagesize(rb);
    } else {
        qemu_log("failed to get ram block from host(%p)\n", hva);
    }

    trace_mcmdq_cmd_null(mcmdq_idx, addr, hva, size, rb_page_size);

#define PAGESZ_2M 0x200000
    if (rb_page_size < PAGESZ_2M) {
        mcmdq_check_pa_continuity_fill_result(&u->mcmdqs[mcmdq_idx], false);
    } else {
        mcmdq_check_pa_continuity_fill_result(&u->mcmdqs[mcmdq_idx], true);
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
    [CMD_CFGI_TECT]            = NULL,
    [CMD_CFGI_TECT_RANGE]      = NULL,
    [CMD_CFGI_TCT]             = NULL,
    [CMD_CFGI_TCT_ALL]         = NULL,
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
                          MEMTXATTRS_UNSPECIFIED);
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
                          MEMTXATTRS_UNSPECIFIED);
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
                         sizeof(tmp), MEMTXATTRS_UNSPECIFIED)) {
        qemu_log("dma failed to wirte cpl entry to addr 0x%lx\n", addr);
    }
}

static void ummu_process_mapt_cmd(UMMUState *u, MAPTCmdqBase *base, MAPTCmd *cmd, uint32_t ci)
{
    uint32_t type = MAPT_UCMD_TYPE(cmd);
    MAPTCmdCpl cpl;
    uint16_t tecte_tag;
    uint32_t tid;

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
            break;
        case MAPT_UCMD_TYPE_PLBI_USR_VA:
            qemu_log("start process mapt cmd: MAPT_UCMD_TYPE_PLBI_USR_VA.\n");
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

    return 0;
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
    ub_save_ummu_list(u);

    u->ummu_devs = g_hash_table_new_full(NULL, NULL, NULL, g_free);
    QLIST_INIT(&u->kvtbl);
    if (u->primary_bus) {
        ub_setup_iommu(u->primary_bus, &ummu_ops, u);
    } else {
        error_setg(errp, "UMMU is not attached to any UB bus!");
    }

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

static void ummu_base_unrealize(DeviceState *dev)
{
    UMMUState *u = UB_UMMU(dev);
    SysBusDevice *sysdev = SYS_BUS_DEVICE(dev);
    UMMUKVTblEntry *entry = NULL;
    UMMUKVTblEntry *next_entry = NULL;

    ub_remove_ummu_list(u);
    if (sysdev->parent_obj.id) {
        g_free(sysdev->parent_obj.id);
    }

    if (u->ummu_devs) {
        g_hash_table_remove_all(u->ummu_devs);
        g_hash_table_destroy(u->ummu_devs);
        u->ummu_devs = NULL;
    }

    QLIST_FOREACH_SAFE(entry, &u->kvtbl, list, next_entry) {
        QLIST_REMOVE(entry, list);
        g_free(entry);
    }
}

static void ummu_base_reset(DeviceState *dev)
{
    /* reset ummu relative struct later */
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

static void ummu_base_register_types(void)
{
    type_register_static(&ummu_base_info);
}
type_init(ummu_base_register_types)
