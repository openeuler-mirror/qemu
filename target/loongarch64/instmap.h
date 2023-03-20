/*
 * Loongarch emulation for qemu: instruction opcode
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

#ifndef TARGET_LARCH_INSTMAP_H
#define TARGET_LARCH_INSTMAP_H

enum {
    /* fix opcodes */
    OPC_LARCH_CLO_W = (0x000004 << 10),
    OPC_LARCH_CLZ_W = (0x000005 << 10),
    OPC_LARCH_CLO_D = (0x000008 << 10),
    OPC_LARCH_CLZ_D = (0x000009 << 10),
    OPC_LARCH_REVB_2H = (0x00000C << 10),
    OPC_LARCH_REVB_4H = (0x00000D << 10),
    OPC_LARCH_REVH_D = (0x000011 << 10),
    OPC_LARCH_BREV_4B = (0x000012 << 10),
    OPC_LARCH_BREV_8B = (0x000013 << 10),
    OPC_LARCH_EXT_WH = (0x000016 << 10),
    OPC_LARCH_EXT_WB = (0x000017 << 10),

    OPC_LARCH_ADD_W = (0x00020 << 15),
    OPC_LARCH_ADD_D = (0x00021 << 15),
    OPC_LARCH_SUB_W = (0x00022 << 15),
    OPC_LARCH_SUB_D = (0x00023 << 15),
    OPC_LARCH_SLT = (0x00024 << 15),
    OPC_LARCH_SLTU = (0x00025 << 15),
    OPC_LARCH_MASKEQZ = (0x00026 << 15),
    OPC_LARCH_MASKNEZ = (0x00027 << 15),
    OPC_LARCH_NOR = (0x00028 << 15),
    OPC_LARCH_AND = (0x00029 << 15),
    OPC_LARCH_OR = (0x0002A << 15),
    OPC_LARCH_XOR = (0x0002B << 15),
    OPC_LARCH_SLL_W = (0x0002E << 15),
    OPC_LARCH_SRL_W = (0x0002F << 15),
    OPC_LARCH_SRA_W = (0x00030 << 15),
    OPC_LARCH_SLL_D = (0x00031 << 15),
    OPC_LARCH_SRL_D = (0x00032 << 15),
    OPC_LARCH_SRA_D = (0x00033 << 15),
    OPC_LARCH_ROTR_W = (0x00036 << 15),
    OPC_LARCH_ROTR_D = (0x00037 << 15),
    OPC_LARCH_MUL_W = (0x00038 << 15),
    OPC_LARCH_MULH_W = (0x00039 << 15),
    OPC_LARCH_MULH_WU = (0x0003A << 15),
    OPC_LARCH_MUL_D = (0x0003B << 15),
    OPC_LARCH_MULH_D = (0x0003C << 15),
    OPC_LARCH_MULH_DU = (0x0003D << 15),
    OPC_LARCH_DIV_W = (0x00040 << 15),
    OPC_LARCH_MOD_W = (0x00041 << 15),
    OPC_LARCH_DIV_WU = (0x00042 << 15),
    OPC_LARCH_MOD_WU = (0x00043 << 15),
    OPC_LARCH_DIV_D = (0x00044 << 15),
    OPC_LARCH_MOD_D = (0x00045 << 15),
    OPC_LARCH_DIV_DU = (0x00046 << 15),
    OPC_LARCH_MOD_DU = (0x00047 << 15),
    OPC_LARCH_SRLI_W = (0x00089 << 15),
    OPC_LARCH_SRAI_W = (0x00091 << 15),
    OPC_LARCH_ROTRI_W = (0x00099 << 15),

    OPC_LARCH_ALSL_W = (0x0002 << 17),
    OPC_LARCH_ALSL_D = (0x0016 << 17),

    OPC_LARCH_TRINS_W = (0x003 << 21) | (0x0 << 15),
    OPC_LARCH_TRPICK_W = (0x003 << 21) | (0x1 << 15),
};

enum {
    /* float opcodes */
    OPC_LARCH_FABS_S = (0x004501 << 10),
    OPC_LARCH_FABS_D = (0x004502 << 10),
    OPC_LARCH_FNEG_S = (0x004505 << 10),
    OPC_LARCH_FNEG_D = (0x004506 << 10),
    OPC_LARCH_FCLASS_S = (0x00450D << 10),
    OPC_LARCH_FCLASS_D = (0x00450E << 10),
    OPC_LARCH_FSQRT_S = (0x004511 << 10),
    OPC_LARCH_FSQRT_D = (0x004512 << 10),
    OPC_LARCH_FRECIP_S = (0x004515 << 10),
    OPC_LARCH_FRECIP_D = (0x004516 << 10),
    OPC_LARCH_FRSQRT_S = (0x004519 << 10),
    OPC_LARCH_FRSQRT_D = (0x00451A << 10),
    OPC_LARCH_FMOV_S = (0x004525 << 10),
    OPC_LARCH_FMOV_D = (0x004526 << 10),
    OPC_LARCH_GR2FR_W = (0x004529 << 10),
    OPC_LARCH_GR2FR_D = (0x00452A << 10),
    OPC_LARCH_GR2FRH_W = (0x00452B << 10),
    OPC_LARCH_FR2GR_S = (0x00452D << 10),
    OPC_LARCH_FR2GR_D = (0x00452E << 10),
    OPC_LARCH_FRH2GR_S = (0x00452F << 10),

    OPC_LARCH_FCVT_S_D = (0x004646 << 10),
    OPC_LARCH_FCVT_D_S = (0x004649 << 10),
    OPC_LARCH_FTINTRM_W_S = (0x004681 << 10),
    OPC_LARCH_FTINTRM_W_D = (0x004682 << 10),
    OPC_LARCH_FTINTRM_L_S = (0x004689 << 10),
    OPC_LARCH_FTINTRM_L_D = (0x00468A << 10),
    OPC_LARCH_FTINTRP_W_S = (0x004691 << 10),
    OPC_LARCH_FTINTRP_W_D = (0x004692 << 10),
    OPC_LARCH_FTINTRP_L_S = (0x004699 << 10),
    OPC_LARCH_FTINTRP_L_D = (0x00469A << 10),
    OPC_LARCH_FTINTRZ_W_S = (0x0046A1 << 10),
    OPC_LARCH_FTINTRZ_W_D = (0x0046A2 << 10),
    OPC_LARCH_FTINTRZ_L_S = (0x0046A9 << 10),
    OPC_LARCH_FTINTRZ_L_D = (0x0046AA << 10),
    OPC_LARCH_FTINTRNE_W_S = (0x0046B1 << 10),
    OPC_LARCH_FTINTRNE_W_D = (0x0046B2 << 10),
    OPC_LARCH_FTINTRNE_L_S = (0x0046B9 << 10),
    OPC_LARCH_FTINTRNE_L_D = (0x0046BA << 10),
    OPC_LARCH_FTINT_W_S = (0x0046C1 << 10),
    OPC_LARCH_FTINT_W_D = (0x0046C2 << 10),
    OPC_LARCH_FTINT_L_S = (0x0046C9 << 10),
    OPC_LARCH_FTINT_L_D = (0x0046CA << 10),
    OPC_LARCH_FFINT_S_W = (0x004744 << 10),
    OPC_LARCH_FFINT_S_L = (0x004746 << 10),
    OPC_LARCH_FFINT_D_W = (0x004748 << 10),
    OPC_LARCH_FFINT_D_L = (0x00474A << 10),
    OPC_LARCH_FRINT_S = (0x004791 << 10),
    OPC_LARCH_FRINT_D = (0x004792 << 10),

    OPC_LARCH_FADD_S = (0x00201 << 15),
    OPC_LARCH_FADD_D = (0x00202 << 15),
    OPC_LARCH_FSUB_S = (0x00205 << 15),
    OPC_LARCH_FSUB_D = (0x00206 << 15),
    OPC_LARCH_FMUL_S = (0x00209 << 15),
    OPC_LARCH_FMUL_D = (0x0020A << 15),
    OPC_LARCH_FDIV_S = (0x0020D << 15),
    OPC_LARCH_FDIV_D = (0x0020E << 15),
    OPC_LARCH_FMAX_S = (0x00211 << 15),
    OPC_LARCH_FMAX_D = (0x00212 << 15),
    OPC_LARCH_FMIN_S = (0x00215 << 15),
    OPC_LARCH_FMIN_D = (0x00216 << 15),
    OPC_LARCH_FMAXA_S = (0x00219 << 15),
    OPC_LARCH_FMAXA_D = (0x0021A << 15),
    OPC_LARCH_FMINA_S = (0x0021D << 15),
    OPC_LARCH_FMINA_D = (0x0021E << 15),
};

enum {
    /* 12 bit immediate opcodes */
    OPC_LARCH_SLTI = (0x008 << 22),
    OPC_LARCH_SLTIU = (0x009 << 22),
    OPC_LARCH_ADDI_W = (0x00A << 22),
    OPC_LARCH_ADDI_D = (0x00B << 22),
    OPC_LARCH_ANDI = (0x00D << 22),
    OPC_LARCH_ORI = (0x00E << 22),
    OPC_LARCH_XORI = (0x00F << 22),
};

enum {
    /* load/store opcodes */
    OPC_LARCH_FLDX_S = (0x07060 << 15),
    OPC_LARCH_FLDX_D = (0x07068 << 15),
    OPC_LARCH_FSTX_S = (0x07070 << 15),
    OPC_LARCH_FSTX_D = (0x07078 << 15),
    OPC_LARCH_FLDGT_S = (0x070E8 << 15),
    OPC_LARCH_FLDGT_D = (0x070E9 << 15),
    OPC_LARCH_FLDLE_S = (0x070EA << 15),
    OPC_LARCH_FLDLE_D = (0x070EB << 15),
    OPC_LARCH_FSTGT_S = (0x070EC << 15),
    OPC_LARCH_FSTGT_D = (0x070ED << 15),
    OPC_LARCH_FSTLE_S = (0x070EE << 15),
    OPC_LARCH_FSTLE_D = (0x070EF << 15),

    OPC_LARCH_LD_B = (0x0A0 << 22),
    OPC_LARCH_LD_H = (0x0A1 << 22),
    OPC_LARCH_LD_W = (0x0A2 << 22),
    OPC_LARCH_LD_D = (0x0A3 << 22),
    OPC_LARCH_ST_B = (0x0A4 << 22),
    OPC_LARCH_ST_H = (0x0A5 << 22),
    OPC_LARCH_ST_W = (0x0A6 << 22),
    OPC_LARCH_ST_D = (0x0A7 << 22),
    OPC_LARCH_LD_BU = (0x0A8 << 22),
    OPC_LARCH_LD_HU = (0x0A9 << 22),
    OPC_LARCH_LD_WU = (0x0AA << 22),
    OPC_LARCH_FLD_S = (0x0AC << 22),
    OPC_LARCH_FST_S = (0x0AD << 22),
    OPC_LARCH_FLD_D = (0x0AE << 22),
    OPC_LARCH_FST_D = (0x0AF << 22),

    OPC_LARCH_LL_W = (0x20 << 24),
    OPC_LARCH_SC_W = (0x21 << 24),
    OPC_LARCH_LL_D = (0x22 << 24),
    OPC_LARCH_SC_D = (0x23 << 24),
    OPC_LARCH_LDPTR_W = (0x24 << 24),
    OPC_LARCH_STPTR_W = (0x25 << 24),
    OPC_LARCH_LDPTR_D = (0x26 << 24),
    OPC_LARCH_STPTR_D = (0x27 << 24),
};

enum {
    /* jump opcodes */
    OPC_LARCH_BEQZ = (0x10 << 26),
    OPC_LARCH_BNEZ = (0x11 << 26),
    OPC_LARCH_B = (0x14 << 26),
    OPC_LARCH_BEQ = (0x16 << 26),
    OPC_LARCH_BNE = (0x17 << 26),
    OPC_LARCH_BLT = (0x18 << 26),
    OPC_LARCH_BGE = (0x19 << 26),
    OPC_LARCH_BLTU = (0x1A << 26),
    OPC_LARCH_BGEU = (0x1B << 26),
};

#endif
