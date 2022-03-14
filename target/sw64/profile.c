#include "translate.h"

const char *insn_opc[535] = {
    "sys_call", "call", "ret", "jmp", "br", "bsr", "memb", "imemb",
    "wmemb", "rtc", "rcid", "halt", "rd_f", "wr_f", "rtid",
    "csrws", "csrwc", "pri_rcsr", "pri_wcsr", "pri_ret", "lldw", "lldl",
    "ldw_inc", "ldl_inc", "ldw_dec", "ldl_dec", "ldw_set", "ldl_set", "lstw",
    "lstl", "ldw_nc", "ldl_nc", "ldd_nc", "stw_nc", "stl_nc", "std_nc",
    "ldwe", "ldse", "ldde", "vlds", "vldd", "vsts", "vstd",
    "fimovs", "fimovd", "addw", "subw", "s4addw", "s4subw", "s8addw",
    "s8subw", "addl", "subl", "s4addl", "s4subl", "s8addl", "s8subl",
    "mulw", "divw", "udivw", "remw", "uremw", "mull", "mulh",
    "divl", "udivl", "reml", "ureml", "addpi", "addpis", "cmpeq",
    "cmplt", "cmple", "cmpult", "cmpule", "sbt", "cbt", "and",
    "bic", "bis", "ornot", "xor", "eqv", "inslb", "inslh",
    "inslw", "insll", "inshb", "inshh", "inshw", "inshl", "slll",
    "srll", "sral", "roll", "sllw", "srlw", "sraw", "rolw",
    "extlb", "extlh", "extlw", "extll", "exthb", "exthh", "exthw",
    "exthl", "ctpop", "ctlz", "cttz", "revbh", "revbw", "revbl",
    "casw", "casl", "masklb", "masklh", "masklw", "maskll", "maskhb",
    "maskhh", "maskhw", "maskhl", "zap", "zapnot", "sextb", "sexth",
    "seleq", "selge", "selgt", "selle", "sellt", "selne", "sellbc",
    "sellbs", "addwi", "subwi", "s4addwi", "s4subwi", "s8addwi", "s8subwi",
    "addli", "subli", "s4addli", "s4subli", "s8addli", "s8subli", "mulwi",
    "divwi", "udivwi", "remwi", "uremwi", "mulli", "mulhi", "divli",
    "udivli", "remli", "uremli", "addpii", "addpisi", "cmpeqi", "cmplti",
    "cmplei", "cmpulti", "cmpulei", "sbti", "cbti", "andi", "bici",
    "bisi", "ornoti", "xori", "eqvi", "inslbi", "inslhi", "inslwi",
    "inslli", "inshbi", "inshhi", "inshwi", "inshli", "sllli", "srlli",
    "srali", "rolli", "sllwi", "srlwi", "srawi", "rolwi", "extlbi",
    "extlhi", "extlwi", "extlli", "exthbi", "exthhi", "exthwi", "exthli",
    "ctpopi", "ctlzi", "cttzi", "revbhi", "revbwi", "revbli", "caswi",
    "casli", "masklbi", "masklhi", "masklwi", "masklli", "maskhbi", "maskhhi",
    "maskhwi", "maskhli", "zapi", "zapnoti", "sextbi", "sexthi", "cmpgebi",
    "seleqi", "selgei", "selgti", "sellei", "sellti", "selnei", "sellbci",
    "sellbsi", "vlogzz", "fadds", "faddd", "fsubs", "fsubd", "fmuls",
    "fmuld", "fdivs", "fdivd", "fsqrts", "fsqrtd", "fcmpeq", "fcmple",
    "fcmplt", "fcmpun", "fcvtsd", "fcvtds", "fcvtdl_g", "fcvtdl_p", "fcvtdl_z",
    "fcvtdl_n", "fcvtdl", "fcvtwl", "fcvtlw", "fcvtls", "fcvtld", "fcpys",
    "fcpyse", "fcpysn", "ifmovs", "ifmovd", "rfpcr", "wfpcr", "setfpec0",
    "setfpec1", "setfpec2", "setfpec3", "frecs", "frecd", "fris", "fris_g",
    "fris_p", "fris_z", "fris_n", "frid", "frid_g", "frid_p", "frid_z",
    "frid_n", "fmas", "fmad", "fmss", "fmsd", "fnmas", "fnmad",
    "fnmss", "fnmsd", "fseleq", "fselne", "fsellt", "fselle", "fselgt",
    "fselge", "vaddw", "vaddwi", "vsubw", "vsubwi", "vcmpgew", "vcmpgewi",
    "vcmpeqw", "vcmpeqwi", "vcmplew", "vcmplewi", "vcmpltw", "vcmpltwi", "vcmpulew",
    "vcmpulewi", "vcmpultw", "vcmpultwi", "vsllw", "vsllwi", "vsrlw", "vsrlwi",
    "vsraw", "vsrawi", "vrolw", "vrolwi", "sllow", "sllowi", "srlow",
    "srlowi", "vaddl", "vaddli", "vsubl", "vsubli", "vsllb", "vsllbi",
    "vsrlb", "vsrlbi", "vsrab", "vsrabi", "vrolb", "vrolbi", "vsllh",
    "vsllhi", "vsrlh", "vsrlhi", "vsrah", "vsrahi", "vrolh", "vrolhi",
    "ctpopow", "ctlzow", "vslll", "vsllli", "vsrll", "vsrlli", "vsral",
    "vsrali", "vroll", "vrolli", "vmaxb", "vminb", "vucaddw", "vucaddwi",
    "vucsubw", "vucsubwi", "vucaddh", "vucaddhi", "vucsubh", "vucsubhi", "vucaddb",
    "vucaddbi", "vucsubb", "vucsubbi", "sraow", "sraowi", "vsumw", "vsuml",
    "vsm4r", "vbinvw", "vcmpueqb", "vcmpugtb", "vcmpugtbi", "vsm3msw", "vmaxh",
    "vminh", "vmaxw", "vminw", "vmaxl", "vminl", "vumaxb", "vuminb",
    "vumaxh", "vuminh", "vumaxw", "vuminw", "vumaxl", "vuminl", "vsm4key",
    "vadds", "vaddd", "vsubs", "vsubd", "vmuls", "vmuld", "vdivs",
    "vdivd", "vsqrts", "vsqrtd", "vfcmpeq", "vfcmple", "vfcmplt", "vfcmpun",
    "vcpys", "vcpyse", "vcpysn", "vsums", "vsumd", "vfcvtsd", "vfcvtds",
    "vfcvtls", "vfcvtld", "vfcvtdl", "vfcvtdl_g", "vfcvtdl_p", "vfcvtdl_z", "vfcvtdl_n",
    "vfris", "vfris_g", "vfris_p", "vfris_z", "vfris_n", "vfrid", "vfrid_g",
    "vfrid_p", "vfrid_z", "vfrid_n", "vfrecs", "vfrecd", "vmaxs", "vmins",
    "vmaxd", "vmind", "vmas", "vmad", "vmss", "vmsd", "vnmas",
    "vnmad", "vnmss", "vnmsd", "vfseleq", "vfsellt", "vfselle", "vseleqw",
    "vseleqwi", "vsellbcw", "vsellbcwi", "vselltw", "vselltwi", "vsellew", "vsellewi",
    "vinsw", "vinsf", "vextw", "vextf", "vcpyw", "vcpyf", "vconw",
    "vshfw", "vcons", "vcond", "vinsb", "vinsh", "vinsectlh", "vinsectlw",
    "vinsectll", "vinsectlb", "vshfq", "vshfqb", "vcpyb", "vcpyh", "vsm3r",
    "vfcvtsh", "vfcvths", "vldw_u", "vstw_u", "vlds_u", "vsts_u", "vldd_u",
    "vstd_u", "vstw_ul", "vstw_uh", "vsts_ul", "vsts_uh", "vstd_ul", "vstd_uh",
    "vldd_nc", "vstd_nc", "lbr", "ldbu_a", "ldhu_a", "ldw_a", "ldl_a",
    "flds_a", "fldd_a", "stbu_a", "sthu_a", "stw_a", "stl_a", "fsts_a",
    "fstd_a", "dpfhr", "dpfhw", "ldbu", "ldhu", "ldw", "ldl",
    "ldl_u", "pri_ldl", "pri_ldw", "flds", "fldd", "stb", "sth",
    "stw", "stl", "stl_u", "pri_stl", "pri_stw", "fsts", "fstd",
    "beq", "bne", "blt", "ble", "bgt", "bge", "blbc",
    "blbs", "fbeq", "fbne", "fblt", "fble", "fbgt", "fbge",
    "ldih", "ldi", };

void insn_profile(DisasContext *ctx, uint32_t insn)
{
    int32_t disp16, disp26 __attribute__((unused));
    uint8_t opc;
    uint16_t fn3, fn4, fn6, fn8, fn11;
    TCGv count;
    int index, offs;

    opc = extract32(insn, 26, 6);

    fn3 = extract32(insn, 10, 3);
    fn6 = extract32(insn, 10, 6);
    fn4 = extract32(insn, 12, 4);
    fn8 = extract32(insn, 5, 8);
    fn11 = extract32(insn, 5, 11);

    disp16 = sextract32(insn, 0, 16);
    disp26 = sextract32(insn, 0, 26);

    index = 0;
    switch (opc) {
    case 0x00:
        /* SYS_CALL */
        index = SYS_CALL;
        break;
    case 0x01:
        /* CALL */
        index = CALL;
        break;
    case 0x02:
        /* RET */
        index = RET;
	break;
    case 0x03:
        /* JMP */
        index = JMP;
        break;
    case 0x04:
        /* BR */
        index = BR;
        break;
    case 0x05:
        /* BSR */
        index = BSR;
        break;
    case 0x06:
        switch (disp16) {
        case 0x0000:
            /* MEMB */
            index = MEMB;
            break;
        case 0x0001:
            /* IMEMB */
            index = IMEMB;
            break;
        case 0x0002:
            /* WMEMB */
            index = WMEMB;
            break;
        case 0x0020:
            /* RTC */
            index = RTC;
            break;
        case 0x0040:
            /* RCID */
            index = RCID;
            break;
        case 0x0080:
            /* HALT */
            index = HALT;
            break;
        case 0x1000:
            /* RD_F */
            index = RD_F;
            break;
        case 0x1020:
            /* WR_F */
            index = WR_F;
            break;
        case 0x1040:
            /* RTID */
            index = RTID;
            break;
        default:
            if ((disp16 & 0xFF00) == 0xFC00) {
                /* CSRWS */
                index = CSRWS;
                break;
            }
            if ((disp16 & 0xFF00) == 0xFD00) {
                /* CSRWC */
                index = CSRWC;
                break;
            }
            if ((disp16 & 0xFF00) == 0xFE00) {
                /* PRI_RCSR */
                index = PRI_RCSR;
                break;
            }
            if ((disp16 & 0xFF00) == 0xFF00) {
                /* PRI_WCSR */
                index = PRI_WCSR;
                break;
            }
            goto do_invalid;
        }
        break;
    case 0x07:
        /* PRI_RET */
        index = PRI_RET;
        break;
    case 0x08:
        switch (fn4) {
        case 0x0:
            /* LLDW */
            index = LLDW;
            break;
        case 0x1:
            /* LLDL */
            index = LLDL;
            break;
        case 0x2:
            /* LDW_INC */
            index = LDW_INC;
            break;
        case 0x3:
            /* LDL_INC */
            index = LDL_INC;
            break;
        case 0x4:
            /* LDW_DEC */
            index = LDW_DEC;
            break;
        case 0x5:
            /* LDL_DEC */
            index = LDL_DEC;
            break;
        case 0x6:
            /* LDW_SET */
            index = LDW_SET;
            break;
        case 0x7:
            /* LDL_SET */
            index = LDL_SET;
            break;
        case 0x8:
            /* LSTW */
            index = LSTW;
            break;
        case 0x9:
            /* LSTL */
            index = LSTL;
            break;
        case 0xa:
            /* LDW_NC */
            index = LDW_NC;
            break;
        case 0xb:
            /* LDL_NC */
            index = LDL_NC;
            break;
        case 0xc:
            /* LDD_NC */
            index = LDD_NC;
            break;
        case 0xd:
            /* STW_NC */
            index = STW_NC;
            break;
        case 0xe:
            /* STL_NC */
            index = STL_NC;
            break;
        case 0xf:
            /* STD_NC */
            index = STD_NC;
            break;
        default:
            goto do_invalid;
        }
        break;
    case 0x9:
        /* LDWE */
        index = LDWE;
        break;
    case 0x0a:
        /* LDSE */
        index = LDSE;
        break;
    case 0x0b:
        /* LDDE */
        index = LDDE;
        break;
    case 0x0c:
        /* VLDS */
        index = VLDS;
        break;
    case 0x0d:
        /* VLDD */
        index = VLDD;
        break;
    case 0x0e:
        /* VSTS */
        index = VSTS;
        break;
    case 0x0f:
        /* VSTD */
        index = VSTD;
        break;
    case 0x10:
        if (fn11 == 0x70) {
            /* FIMOVS */
            index = FIMOVS;
        } else if (fn11 == 0x78) {
            /* FIMOVD */
            index = FIMOVD;
        } else {
            switch (fn11 & 0xff) {
            case 0x00:
                /* ADDW */
                index = ADDW;
                break;
            case 0x01:
                /* SUBW */
                index = SUBW;
                break;
            case 0x02:
                /* S4ADDW */
                index = S4ADDW;
                break;
            case 0x03:
                /* S4SUBW */
                index = S4SUBW;
                break;
            case 0x04:
                /* S8ADDW */
                index = S8ADDW;
                break;
            case 0x05:
                /* S8SUBW */
                index = S8SUBW;
                break;

            case 0x08:
                /* ADDL */
                index = ADDL;
                break;
            case 0x09:
                /* SUBL */
                index = SUBL;
                break;
            case 0x0a:
                /* S4ADDL */
                index = S4ADDL;
                break;
            case 0x0b:
                /* S4SUBL */
                index = S4SUBL;
                break;
            case 0x0c:
                /* S8ADDL */
                index = S8ADDL;
                break;
            case 0x0d:
                /* S8SUBL */
                index = S8SUBL;
                break;
            case 0x10:
                /* MULW */
                index = MULW;
                break;
            case 0x11:
                /* DIVW */
                index = DIVW;
                break;
            case 0x12:
                /* UDIVW */
                index = UDIVW;
                break;
            case 0x13:
                /* REMW */
                index = REMW;
                break;
            case 0x14:
                /* UREMW */
                index = UREMW;
                break;
            case 0x18:
                /* MULL */
                index = MULL;
                break;
            case 0x19:
                /* MULH */
                index = MULH;
                break;
            case 0x1A:
                /* DIVL */
                index = DIVL;
                break;
            case 0x1B:
                /* UDIVL */
                index = UDIVL;
                break;
            case 0x1C:
                /* REML */
                index = REML;
                break;
            case 0x1D:
                /* UREML */
                index = UREML;
                break;
            case 0x1E:
                /* ADDPI */
                index = ADDPI;
                break;
            case 0x1F:
                /* ADDPIS */
                index = ADDPIS;
                break;
            case 0x28:
                /* CMPEQ */
                index = CMPEQ;
                break;
            case 0x29:
                /* CMPLT */
                index = CMPLT;
                break;
            case 0x2a:
                /* CMPLE */
                index = CMPLE;
                break;
            case 0x2b:
                /* CMPULT */
                index = CMPULT;
                break;
            case 0x2c:
                /* CMPULE */
                index = CMPULE;
                break;
            case 0x2D:
                /* SBT */
                index = SBT;
                break;
            case 0x2E:
                /* CBT */
                index = CBT;
                break;
            case 0x38:
                /* AND */
                index = AND;
                break;
            case 0x39:
                /* BIC */
                index = BIC;
                break;
            case 0x3a:
                /* BIS */
                index = BIS;
                break;
            case 0x3b:
                /* ORNOT */
                index = ORNOT;
                break;
            case 0x3c:
                /* XOR */
                index = XOR;
                break;
            case 0x3d:
                /* EQV */
                index = EQV;
                break;
            case 0x40:
                /* INSLB */
                index = INSLB;
                break;
            case 0x41:
                /* INSLH */
                index = INSLH;
                break;
            case 0x42:
                /* INSLW */
                index = INSLW;
                break;
            case 0x43:
                /* INSLL */
                index = INSLL;
                break;
            case 0x44:
                /* INSHB */
                index = INSHB;
                break;
            case 0x45:
                /* INSHH */
                index = INSHH;
                break;
            case 0x46:
                /* INSHW */
                index = INSHW;
                break;
            case 0x47:
                /* INSHL */
                index = INSHL;
                break;
            case 0x48:
                /* SLLL */
                index = SLLL;
                break;
            case 0x49:
                /* SRLL */
                index = SRLL;
                break;
            case 0x4a:
                /* SRAL */
                index = SRAL;
                break;
            case 0x4B:
                /* ROLL */
                index = ROLL;
                break;
            case 0x4C:
                /* SLLW */
                index = SLLW;
                break;
            case 0x4D:
                /* SRLW */
                index = SRLW;
                break;
            case 0x4E:
                /* SRAW */
                index = SRAW;
                break;
            case 0x4F:
                /* ROLW */
                index = ROLW;
                break;
            case 0x50:
                /* EXTLB */
                index = EXTLB;
                break;
            case 0x51:
                /* EXTLH */
                index = EXTLH;
                break;
            case 0x52:
                /* EXTLW */
                index = EXTLW;
                break;
            case 0x53:
                /* EXTLL */
                index = EXTLL;
                break;
            case 0x54:
                /* EXTHB */
                index = EXTHB;
                break;
            case 0x55:
                /* EXTHH */
                index = EXTHH;
                break;
            case 0x56:
                /* EXTHW */
                index = EXTHW;
                break;
            case 0x57:
                /* EXTHL */
                index = EXTHL;
                break;
            case 0x58:
                /* CTPOP */
                index = CTPOP;
                break;
            case 0x59:
                /* CTLZ */
                index = CTLZ;
                break;
            case 0x5a:
                /* CTTZ */
                index = CTTZ;
                break;
            case 0x5B:
                /* REVBH */
                index = REVBH;
                break;
            case 0x5C:
                /* REVBW */
                index = REVBW;
                break;
            case 0x5D:
                /* REVBL */
                index = REVBL;
                break;
            case 0x5E:
                /* CASW */
                index = CASW;
                break;
            case 0x5F:
                /* CASL */
                index = CASL;
                break;
            case 0x60:
                /* MASKLB */
                index = MASKLB;
                break;
            case 0x61:
                /* MASKLH */
                index = MASKLH;
                break;
            case 0x62:
                /* MASKLW */
                index = MASKLW;
                break;
            case 0x63:
                /* MASKLL */
                index = MASKLL;
                break;
            case 0x64:
                /* MASKHB */
                index = MASKHB;
                break;
            case 0x65:
                /* MASKHH */
                index = MASKHH;
                break;
            case 0x66:
                /* MASKHW */
                index = MASKHW;
                break;
            case 0x67:
                /* MASKHL */
                index = MASKHL;
                break;
            case 0x68:
                /* ZAP */
                index = ZAP;
                break;
            case 0x69:
                /* ZAPNOT */
                index = ZAPNOT;
                break;
            case 0x6a:
                /* SEXTB */
                index = SEXTB;
                break;
            case 0x6b:
                /* SEXTH */
                index = SEXTH;
                break;
            case 0x6c:
                /* CMPGEB*/
                break;
            default:
                break;
            }
        }
        break;
    case 0x11:
        switch (fn3) {
        case 0x0:
            /* SELEQ */
            index = SELEQ;
            break;
        case 0x1:
            /* SELGE */
            index = SELGE;
            break;
        case 0x2:
            /* SELGT */
            index = SELGT;
            break;
        case 0x3:
            /* SELLE */
            index = SELLE;
            break;
        case 0x4:
            /* SELLT */
            index = SELLT;
            break;
        case 0x5:
            /* SELNE */
            index = SELNE;
            break;
        case 0x6:
            /* SELLBC */
            index = SELLBC;
            break;
        case 0x7:
            /* SELLBS */
            index = SELLBS;
            break;
        default:
            break;
        }
        break;
    case 0x12:
        switch (fn8 & 0xff) {
        case 0x00:
            /* ADDWI */
            index = ADDWI;
            break;
        case 0x01:
            /* SUBWI */
            index = SUBWI;
            break;
        case 0x02:
            /* S4ADDWI */
            index = S4ADDWI;
            break;
        case 0x03:
            /* S4SUBWI */
            index = S4SUBWI;
            break;
        case 0x04:
            /* S8ADDWI */
            index = S8ADDWI;
            break;
        case 0x05:
            /* S8SUBWI */
            index = S8SUBWI;
            break;

        case 0x08:
            /* ADDLI */
            index = ADDLI;
            break;
        case 0x09:
            /* SUBLI */
            index = SUBLI;
            break;
        case 0x0a:
            /* S4ADDLI */
            index = S4ADDLI;
            break;
        case 0x0b:
            /* S4SUBLI */
            index = S4SUBLI;
            break;
        case 0x0c:
            /* S8ADDLI */
            index = S8ADDLI;
            break;
        case 0x0d:
            /* S8SUBLI */
            index = S8SUBLI;
            break;
        case 0x10:
            /* MULWI */
            index = MULWI;
            break;
        case 0x11:
            /* DIVWI */
            index = DIVWI;
            break;
        case 0x12:
            /* UDIVWI */
            index = UDIVWI;
            break;
        case 0x13:
            /* REMWI */
            index = REMWI;
            break;
        case 0x14:
            /* UREMWI */
            index = UREMWI;
            break;
        case 0x18:
            /* MULLI */
            index = MULLI;
            break;
        case 0x19:
            /* MULHI */
            index = MULHI;
            break;
        case 0x1A:
            /* DIVLI */
            index = DIVLI;
            break;
        case 0x1B:
            /* UDIVLI */
            index = UDIVLI;
            break;
        case 0x1C:
            /* REMLI */
            index = REMLI;
            break;
        case 0x1D:
            /* UREMLI */
            index = UREMLI;
            break;
        case 0x1E:
            /* ADDPII */
            index = ADDPII;
            break;
        case 0x1F:
            /* ADDPISI */
            index = ADDPISI;
            break;
        case 0x28:
            /* CMPEQI */
            index = CMPEQI;
            break;
        case 0x29:
            /* CMPLTI */
            index = CMPLTI;
            break;
        case 0x2a:
            /* CMPLEI */
            index = CMPLEI;
            break;
        case 0x2b:
            /* CMPULTI */
            index = CMPULTI;
            break;
        case 0x2c:
            /* CMPULEI */
            index = CMPULEI;
            break;
        case 0x2D:
            /* SBTI */
            index = SBTI;
            break;
        case 0x2E:
            /* CBTI */
            index = CBTI;
            break;
        case 0x38:
            /* ANDI */
            index = ANDI;
            break;
        case 0x39:
            /* BICI */
            index = BICI;
            break;
        case 0x3a:
            /* BISI */
            index = BISI;
            break;
        case 0x3b:
            /* ORNOTI */
            index = ORNOTI;
            break;
        case 0x3c:
            /* XORI */
            index = XORI;
            break;
        case 0x3d:
            /* EQVI */
            index = EQVI;
            break;
        case 0x40:
            /* INSLBI */
            index = INSLBI;
            break;
        case 0x41:
            /* INSLHI */
            index = INSLHI;
            break;
        case 0x42:
            /* INSLWI */
            index = INSLWI;
            break;
        case 0x43:
            /* INSLLI */
            index = INSLLI;
            break;
        case 0x44:
            /* INSHBI */
            index = INSHBI;
            break;
        case 0x45:
            /* INSHHI */
            index = INSHHI;
            break;
        case 0x46:
            /* INSHWI */
            index = INSHWI;
            break;
        case 0x47:
            /* INSHLI */
            index = INSHLI;
            break;
        case 0x48:
            /* SLLLI */
            index = SLLLI;
            break;
        case 0x49:
            /* SRLLI */
            index = SRLLI;
            break;
        case 0x4a:
            /* SRALI */
            index = SRALI;
            break;
        case 0x4B:
            /* ROLLI */
            index = ROLLI;
            break;
        case 0x4C:
            /* SLLWI */
            index = SLLWI;
            break;
        case 0x4D:
            /* SRLWI */
            index = SRLWI;
            break;
        case 0x4E:
            /* SRAWI */
            index = SRAWI;
            break;
        case 0x4F:
            /* ROLWI */
            index = ROLWI;
            break;
        case 0x50:
            /* EXTLBI */
            index = EXTLBI;
            break;
        case 0x51:
            /* EXTLHI */
            index = EXTLHI;
            break;
        case 0x52:
            /* EXTLWI */
            index = EXTLWI;
            break;
        case 0x53:
            /* EXTLLI */
            index = EXTLLI;
            break;
        case 0x54:
            /* EXTHBI */
            index = EXTHBI;
            break;
        case 0x55:
            /* EXTHHI */
            index = EXTHHI;
            break;
        case 0x56:
            /* EXTHWI */
            index = EXTHWI;
            break;
        case 0x57:
            /* EXTHLI */
            index = EXTHLI;
            break;
        case 0x58:
            /* CTPOPI */
            index = CTPOPI;
            break;
        case 0x59:
            /* CTLZI */
            index = CTLZI;
            break;
        case 0x5a:
            /* CTTZI */
            index = CTTZI;
            break;
        case 0x5B:
            /* REVBHI */
            index = REVBHI;
            break;
        case 0x5C:
            /* REVBWI */
            index = REVBWI;
            break;
        case 0x5D:
            /* REVBLI */
            index = REVBLI;
            break;
        case 0x5E:
            /* CASWI */
            index = CASWI;
            break;
        case 0x5F:
            /* CASLI */
            index = CASLI;
            break;
        case 0x60:
            /* MASKLBI */
            index = MASKLBI;
            break;
        case 0x61:
            /* MASKLHI */
            index = MASKLHI;
            break;
        case 0x62:
            /* MASKLWI */
            index = MASKLWI;
            break;
        case 0x63:
            /* MASKLLI */
            index = MASKLLI;
            break;
        case 0x64:
            /* MASKHBI */
            index = MASKHBI;
            break;
        case 0x65:
            /* MASKHHI */
            index = MASKHHI;
            break;
        case 0x66:
            /* MASKHWI */
            index = MASKHWI;
            break;
        case 0x67:
            /* MASKHLI */
            index = MASKHLI;
            break;
        case 0x68:
            /* ZAPI */
            index = ZAPI;
            break;
        case 0x69:
            /* ZAPNOTI */
            index = ZAPNOTI;
            break;
        case 0x6a:
            /* SEXTBI */
            index = SEXTBI;
            break;
        case 0x6b:
            /* SEXTHI */
            index = SEXTHI;
            break;
        case 0x6c:
            /* CMPGEBI */
            index = CMPGEBI;
            break;
        default:
            break;
        }
        break;
    case 0x13:
        switch (fn3) {
        case 0x0:
            /* SELEQI */
            index = SELEQI;
            break;
        case 0x1:
            /* SELGEI */
            index = SELGEI;
            break;
        case 0x2:
            /* SELGTI */
            index = SELGTI;
            break;
        case 0x3:
            /* SELLEI */
            index = SELLEI;
            break;
        case 0x4:
            /* SELLTI */
            index = SELLTI;
            break;
        case 0x5:
            /* SELNEI */
            index = SELNEI;
            break;
        case 0x6:
            /* SELLBCI */
            index = SELLBCI;
            break;
        case 0x7:
            /* SELLBSI */
            index = SELLBSI;
            break;
        default:
            break;
        }
        break;
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
        /* VLOGZZ */
        index = VLOGZZ;
        break;
    case 0x18:
        switch (fn8) {
        case 0x00:
            /* FADDS */
            index = FADDS;
            break;
        case 0x01:
            /* FADDD */
            index = FADDD;
            break;
        case 0x02:
            /* FSUBS */
            index = FSUBS;
            break;
        case 0x03:
            /* FSUBD */
            index = FSUBD;
            break;
        case 0x4:
            /* FMULS */
            index = FMULS;
            break;
        case 0x05:
            /* FMULD */
            index = FMULD;
            break;
        case 0x06:
            /* FDIVS */
            index = FDIVS;
            break;
        case 0x07:
            /* FDIVD */
            index = FDIVD;
            break;
        case 0x08:
            /* FSQRTS */
            index = FSQRTS;
            break;
        case 0x09:
            /* FSQRTD */
            index = FSQRTD;
            break;
        case 0x10:
            /* FCMPEQ */
            index = FCMPEQ;
            break;
        case 0x11:
            /* FCMPLE */
            index = FCMPLE;
            break;
        case 0x12:
            /* FCMPLT */
            index = FCMPLT;
            break;
        case 0x13:
            /* FCMPUN */
            index = FCMPUN;
            break;
        case 0x20:
            /* FCVTSD */
            index = FCVTSD;
            break;
        case 0x21:
            /* FCVTDS */
            index = FCVTDS;
            break;
        case 0x22:
            /* FCVTDL_G */
            index = FCVTDL_G;
            break;
        case 0x23:
            /* FCVTDL_P */
            index = FCVTDL_P;
            break;
        case 0x24:
            /* FCVTDL_Z */
            index = FCVTDL_Z;
            break;
        case 0x25:
            /* FCVTDL_N */
            index = FCVTDL_N;
            break;
        case 0x27:
            /* FCVTDL */
            index = FCVTDL;
            break;
        case 0x28:
            /* FCVTWL */
            index = FCVTWL;
            break;
        case 0x29:
            /* FCVTLW */
            index = FCVTLW;
            break;
        case 0x2d:
            /* FCVTLS */
            index = FCVTLS;
            break;
        case 0x2f:
            /* FCVTLD */
            index = FCVTLD;
            break;
        case 0x30:
            /* FCPYS */
            index = FCPYS;
            break;
        case 0x31:
            /* FCPYSE */
            index = FCPYSE;
            break;
        case 0x32:
            /* FCPYSN */
            index = FCPYSN;
            break;
        case 0x40:
            /* IFMOVS */
            index = IFMOVS;
            break;
        case 0x41:
            /* IFMOVD */
            index = IFMOVD;
            break;
        case 0x50:
            /* RFPCR */
            index = RFPCR;
            break;
        case 0x51:
            /* WFPCR */
            index = WFPCR;
            break;
        case 0x54:
            /* SETFPEC0 */
            index = SETFPEC0;
            break;
        case 0x55:
            /* SETFPEC1 */
            index = SETFPEC1;
            break;
        case 0x56:
            /* SETFPEC2 */
            index = SETFPEC2;
            break;
        case 0x57:
            /* SETFPEC3 */
            index = SETFPEC3;
            break;
        case 0x58:
            /* FRECS */
            index = FRECS;
            break;
        case 0x59:
            /* FRECD */
            index = FRECD;
            break;
        case 0x5A:
            /* FRIS */
            index = FRIS;
            break;
        case 0x5B:
            /* FRIS_G */
            index = FRIS_G;
            break;
        case 0x5C:
            /* FRIS_P */
            index = FRIS_P;
            break;
        case 0x5D:
            /* FRIS_Z */
            index = FRIS_Z;
            break;
        case 0x5F:
            /* FRIS_N */
            index = FRIS_N;
            break;
        case 0x60:
            /* FRID */
            index = FRID;
            break;
        case 0x61:
            /* FRID_G */
            index = FRID_G;
            break;
        case 0x62:
            /* FRID_P */
            index = FRID_P;
            break;
        case 0x63:
            /* FRID_Z */
            index = FRID_Z;
            break;
        case 0x64:
            /* FRID_N */
            index = FRID_N;
            break;
        default:
            break;
        }
        break;
    case 0x19:
        switch (fn6) {
        case 0x00:
            /* FMAS */
            index = FMAS;
            break;
        case 0x01:
            /* FMAD */
            index = FMAD;
            break;
        case 0x02:
            /* FMSS */
            index = FMSS;
            break;
        case 0x03:
            /* FMSD */
            index = FMSD;
            break;
        case 0x04:
            /* FNMAS */
            index = FNMAS;
            break;
        case 0x05:
            /* FNMAD */
            index = FNMAD;
            break;
        case 0x06:
            /* FNMSS */
            index = FNMSS;
            break;
        case 0x07:
            /* FNMSD */
            index = FNMSD;
            break;
        case 0x10:
            /* FSELEQ */
            index = FSELEQ;
            break;
        case 0x11:
            /* FSELNE */
            index = FSELNE;
            break;
        case 0x12:
            /* FSELLT */
            index = FSELLT;
            break;
        case 0x13:
            /* FSELLE */
            index = FSELLE;
            break;
        case 0x14:
            /* FSELGT */
            index = FSELGT;
            break;
        case 0x15:
            /* FSELGE */
            index = FSELGE;
            break;
        default:
            break;
        }
        break;
    case 0x1A:
        switch (fn8) {
        case 0x00:
            /* VADDW */
            index = VADDW;
            break;
        case 0x20:
            /* VADDWI */
            index = VADDWI;
            break;
        case 0x01:
            /* VSUBW */
            index = VSUBW;
            break;
        case 0x21:
            /* VSUBWI */
            index = VSUBWI;
            break;
        case 0x02:
            /* VCMPGEW */
            index = VCMPGEW;
            break;
        case 0x22:
            /* VCMPGEWI */
            index = VCMPGEWI;
            break;
        case 0x03:
            /* VCMPEQW */
            index = VCMPEQW;
            break;
        case 0x23:
            /* VCMPEQWI */
            index = VCMPEQWI;
            break;
        case 0x04:
            /* VCMPLEW */
            index = VCMPLEW;
            break;
        case 0x24:
            /* VCMPLEWI */
            index = VCMPLEWI;
            break;
        case 0x05:
            /* VCMPLTW */
            index = VCMPLTW;
            break;
        case 0x25:
            /* VCMPLTWI */
            index = VCMPLTWI;
            break;
        case 0x06:
            /* VCMPULEW */
            index = VCMPULEW;
            break;
        case 0x26:
            /* VCMPULEWI */
            index = VCMPULEWI;
            break;
        case 0x07:
            /* VCMPULTW */
            index = VCMPULTW;
            break;
        case 0x27:
            /* VCMPULTWI */
            index = VCMPULTWI;
            break;
        case 0x08:
            /* VSLLW */
            index = VSLLW;
            break;
        case 0x28:
            /* VSLLWI */
            index = VSLLWI;
            break;
        case 0x09:
            /* VSRLW */
            index = VSRLW;
            break;
        case 0x29:
            /* VSRLWI */
            index = VSRLWI;
            break;
        case 0x0A:
            /* VSRAW */
            index = VSRAW;
            break;
        case 0x2A:
            /* VSRAWI */
            index = VSRAWI;
            break;
        case 0x0B:
            /* VROLW */
            index = VROLW;
            break;
        case 0x2B:
            /* VROLWI */
            index = VROLWI;
            break;
        case 0x0C:
            /* SLLOW */
            index = SLLOW;
            break;
        case 0x2C:
            /* SLLOWI */
            index = SLLOWI;
            break;
        case 0x0D:
            /* SRLOW */
            index = SRLOW;
            break;
        case 0x2D:
            /* SRLOWI */
            index = SRLOWI;
            break;
        case 0x0E:
            /* VADDL */
            index = VADDL;
            break;
        case 0x2E:
            /* VADDLI */
            index = VADDLI;
            break;
        case 0x0F:
            /* VSUBL */
            index = VSUBL;
            break;
        case 0x2F:
            /* VSUBLI */
            index = VSUBLI;
            break;
        case 0x10:
            /* VSLLB */
            index = VSLLB;
            break;
        case 0x30:
            /* VSLLBI */
            index = VSLLBI;
            break;
        case 0x11:
            /* VSRLB */
            index = VSRLB;
            break;
        case 0x31:
            /* VSRLBI */
            index = VSRLBI;
            break;
        case 0x12:
            /* VSRAB */
            index = VSRAB;
            break;
        case 0x32:
            /* VSRABI */
            index = VSRABI;
            break;
        case 0x13:
            /* VROLB */
            index = VROLB;
            break;
        case 0x33:
            /* VROLBI */
            index = VROLBI;
            break;
        case 0x14:
            /* VSLLH */
            index = VSLLH;
            break;
        case 0x34:
            /* VSLLHI */
            index = VSLLHI;
            break;
        case 0x15:
            /* VSRLH */
            index = VSRLH;
            break;
        case 0x35:
            /* VSRLHI */
            index = VSRLHI;
            break;
        case 0x16:
            /* VSRAH */
            index = VSRAH;
            break;
        case 0x36:
            /* VSRAHI */
            index = VSRAHI;
            break;
        case 0x17:
            /* VROLH */
            index = VROLH;
            break;
        case 0x37:
            /* VROLHI */
            index = VROLHI;
            break;
        case 0x18:
            /* CTPOPOW */
            index = CTPOPOW;
            break;
        case 0x19:
            /* CTLZOW */
            index = CTLZOW;
            break;
        case 0x1A:
            /* VSLLL */
            index = VSLLL;
            break;
        case 0x3A:
            /* VSLLLI */
            index = VSLLLI;
            break;
        case 0x1B:
            /* VSRLL */
            index = VSRLL;
            break;
        case 0x3B:
            /* VSRLLI */
            index = VSRLLI;
            break;
        case 0x1C:
            /* VSRAL */
            index = VSRAL;
            break;
        case 0x3C:
            /* VSRALI */
            index = VSRALI;
            break;
        case 0x1D:
            /* VROLL */
            index = VROLL;
            break;
        case 0x3D:
            /* VROLLI */
            index = VROLLI;
            break;
        case 0x1E:
            /* VMAXB */
            index = VMAXB;
            break;
        case 0x1F:
            /* VMINB */
            index = VMINB;
            break;
        case 0x40:
            /* VUCADDW */
            index = VUCADDW;
            break;
        case 0x60:
            /* VUCADDWI */
            index = VUCADDWI;
            break;
        case 0x41:
            /* VUCSUBW */
            index = VUCSUBW;
            break;
        case 0x61:
            /* VUCSUBWI */
            index = VUCSUBWI;
            break;
        case 0x42:
            /* VUCADDH */
            index = VUCADDH;
            break;
        case 0x62:
            /* VUCADDHI */
            index = VUCADDHI;
            break;
        case 0x43:
            /* VUCSUBH */
            index = VUCSUBH;
            break;
        case 0x63:
            /* VUCSUBHI */
            index = VUCSUBHI;
            break;
        case 0x44:
            /* VUCADDB */
            index = VUCADDB;
            break;
        case 0x64:
            /* VUCADDBI */
            index = VUCADDBI;
            break;
        case 0x45:
            /* VUCSUBB */
            index = VUCSUBB;
            break;
        case 0x65:
            /* VUCSUBBI */
            index = VUCSUBBI;
            break;
        case 0x46:
            /* SRAOW */
            index = SRAOW;
            break;
        case 0x66:
            /* SRAOWI */
            index = SRAOWI;
            break;
        case 0x47:
            /* VSUMW */
            index = VSUMW;
            break;
        case 0x48:
            /* VSUML */
            index = VSUML;
            break;
        case 0x49:
            /* VSM4R */
            index = VSM4R;
            break;
        case 0x4A:
            /* VBINVW */
            index = VBINVW;
            break;
        case 0x4B:
            /* VCMPUEQB */
            index = VCMPUEQB;
            break;
        case 0x6B:
            /* VCMPUEQBI*/
            break;
        case 0x4C:
            /* VCMPUGTB */
            index = VCMPUGTB;
            break;
        case 0x6C:
            /* VCMPUGTBI */
            index = VCMPUGTBI;
            break;
        case 0x4D:
            /* VSM3MSW */
            index = VSM3MSW;
            break;
        case 0x50:
            /* VMAXH */
            index = VMAXH;
            break;
        case 0x51:
            /* VMINH */
            index = VMINH;
            break;
        case 0x52:
            /* VMAXW */
            index = VMAXW;
            break;
        case 0x53:
            /* VMINW */
            index = VMINW;
            break;
        case 0x54:
            /* VMAXL */
            index = VMAXL;
            break;
        case 0x55:
            /* VMINL */
            index = VMINL;
            break;
        case 0x56:
            /* VUMAXB */
            index = VUMAXB;
            break;
        case 0x57:
            /* VUMINB */
            index = VUMINB;
            break;
        case 0x58:
            /* VUMAXH */
            index = VUMAXH;
            break;
        case 0x59:
            /* VUMINH */
            index = VUMINH;
            break;
        case 0x5A:
            /* VUMAXW */
            index = VUMAXW;
            break;
        case 0x5B:
            /* VUMINW */
            index = VUMINW;
            break;
        case 0x5C:
            /* VUMAXL */
            index = VUMAXL;
            break;
        case 0x5D:
            /* VUMINL */
            index = VUMINL;
            break;
        case 0x68:
            /* VSM4KEY */
            index = VSM4KEY;
            break;
        case 0x80:
            /* VADDS */
            index = VADDS;
            break;
        case 0x81:
            /* VADDD */
            index = VADDD;
            break;
        case 0x82:
            /* VSUBS */
            index = VSUBS;
            break;
        case 0x83:
            /* VSUBD */
            index = VSUBD;
            break;
        case 0x84:
            /* VMULS */
            index = VMULS;
            break;
        case 0x85:
            /* VMULD */
            index = VMULD;
            break;
        case 0x86:
            /* VDIVS */
            index = VDIVS;
            break;
        case 0x87:
            /* VDIVD */
            index = VDIVD;
            break;
        case 0x88:
            /* VSQRTS */
            index = VSQRTS;
            break;
        case 0x89:
            /* VSQRTD */
            index = VSQRTD;
            break;
        case 0x8C:
            /* VFCMPEQ */
            index = VFCMPEQ;
            break;
        case 0x8D:
            /* VFCMPLE */
            index = VFCMPLE;
            break;
        case 0x8E:
            /* VFCMPLT */
            index = VFCMPLT;
            break;
        case 0x8F:
            /* VFCMPUN */
            index = VFCMPUN;
            break;
        case 0x90:
            /* VCPYS */
            index = VCPYS;
            break;
        case 0x91:
            /* VCPYSE */
            index = VCPYSE;
            break;
        case 0x92:
            /* VCPYSN */
            index = VCPYSN;
            break;
        case 0x93:
            /* VSUMS */
            index = VSUMS;
            break;
        case 0x94:
            /* VSUMD */
            index = VSUMD;
            break;
        case 0x95:
            /* VFCVTSD */
            index = VFCVTSD;
            break;
        case 0x96:
            /* VFCVTDS */
            index = VFCVTDS;
            break;
        case 0x99:
            /* VFCVTLS */
            index = VFCVTLS;
            break;
        case 0x9A:
            /* VFCVTLD */
            index = VFCVTLD;
            break;
        case 0x9B:
            /* VFCVTDL */
            index = VFCVTDL;
            break;
        case 0x9C:
            /* VFCVTDL_G */
            index = VFCVTDL_G;
            break;
        case 0x9D:
            /* VFCVTDL_P */
            index = VFCVTDL_P;
            break;
        case 0x9E:
            /* VFCVTDL_Z */
            index = VFCVTDL_Z;
            break;
        case 0x9F:
            /* VFCVTDL_N */
            index = VFCVTDL_N;
            break;
        case 0xA0:
            /* VFRIS */
            index = VFRIS;
            break;
        case 0xA1:
            /* VFRIS_G */
            index = VFRIS_G;
            break;
        case 0xA2:
            /* VFRIS_P */
            index = VFRIS_P;
            break;
        case 0xA3:
            /* VFRIS_Z */
            index = VFRIS_Z;
            break;
        case 0xA4:
            /* VFRIS_N */
            index = VFRIS_N;
            break;
        case 0xA5:
            /* VFRID */
            index = VFRID;
            break;
        case 0xA6:
            /* VFRID_G */
            index = VFRID_G;
            break;
        case 0xA7:
            /* VFRID_P */
            index = VFRID_P;
            break;
        case 0xA8:
            /* VFRID_Z */
            index = VFRID_Z;
            break;
        case 0xA9:
            /* VFRID_N */
            index = VFRID_N;
            break;
        case 0xAA:
            /* VFRECS */
            index = VFRECS;
            break;
        case 0xAB:
            /* VFRECD */
            index = VFRECD;
            break;
        case 0xAC:
            /* VMAXS */
            index = VMAXS;
            break;
        case 0xAD:
            /* VMINS */
            index = VMINS;
            break;
        case 0xAE:
            /* VMAXD */
            index = VMAXD;
            break;
        case 0xAF:
            /* VMIND */
            index = VMIND;
            break;
        default:
            break;
        }
        break;
    case 0x1B:
        switch (fn6) {
        case 0x00:
            /* VMAS */
            index = VMAS;
            break;
        case 0x01:
            /* VMAD */
            index = VMAD;
            break;
        case 0x02:
            /* VMSS */
            index = VMSS;
            break;
        case 0x03:
            /* VMSD */
            index = VMSD;
            break;
        case 0x04:
            /* VNMAS */
            index = VNMAS;
            break;
        case 0x05:
            /* VNMAD */
            index = VNMAD;
            break;
        case 0x06:
            /* VNMSS */
            index = VNMSS;
            break;
        case 0x07:
            /* VNMSD */
            index = VNMSD;
            break;
        case 0x10:
            /* VFSELEQ */
            index = VFSELEQ;
            break;
        case 0x12:
            /* VFSELLT */
            index = VFSELLT;
            break;
        case 0x13:
            /* VFSELLE */
            index = VFSELLE;
            break;
        case 0x18:
            /* VSELEQW */
            index = VSELEQW;
            break;
        case 0x38:
            /* VSELEQWI */
            index = VSELEQWI;
            break;
        case 0x19:
            /* VSELLBCW */
            index = VSELLBCW;
            break;
        case 0x39:
            /* VSELLBCWI */
            index = VSELLBCWI;
            break;
        case 0x1A:
            /* VSELLTW */
            index = VSELLTW;
            break;
        case 0x3A:
            /* VSELLTWI */
            index = VSELLTWI;
            break;
        case 0x1B:
            /* VSELLEW */
            index = VSELLEW;
            break;
        case 0x3B:
            /* VSELLEWI */
            index = VSELLEWI;
            break;
        case 0x20:
            /* VINSW */
            index = VINSW;
            break;
        case 0x21:
            /* VINSF */
            index = VINSF;
            break;
        case 0x22:
            /* VEXTW */
            index = VEXTW;
            break;
        case 0x23:
            /* VEXTF */
            index = VEXTF;
            break;
        case 0x24:
            /* VCPYW */
            index = VCPYW;
            break;
        case 0x25:
            /* VCPYF */
            index = VCPYF;
            break;
        case 0x26:
            /* VCONW */
            index = VCONW;
            break;
        case 0x27:
            /* VSHFW */
            index = VSHFW;
            break;
        case 0x28:
            /* VCONS */
            index = VCONS;
            break;
        case 0x29:
            /* VCOND */
            index = VCOND;
            break;
        case 0x2A:
            /* VINSB */
            index = VINSB;
            break;
        case 0x2B:
            /* VINSH */
            index = VINSH;
            break;
        case 0x2C:
            /* VINSECTLH */
            index = VINSECTLH;
            break;
        case 0x2D:
            /* VINSECTLW */
            index = VINSECTLW;
            break;
        case 0x2E:
            /* VINSECTLL */
            index = VINSECTLL;
            break;
        case 0x2F:
            /* VINSECTLB */
            index = VINSECTLB;
            break;
        case 0x30:
            /* VSHFQ */
            index = VSHFQ;
            break;
        case 0x31:
            /* VSHFQB */
            index = VSHFQB;
            break;
        case 0x32:
            /* VCPYB */
            index = VCPYB;
            break;
        case 0x33:
            /* VCPYH */
            index = VCPYH;
            break;
        case 0x34:
            /* VSM3R */
            index = VSM3R;
            break;
        case 0x35:
            /* VFCVTSH */
            index = VFCVTSH;
            break;
        case 0x36:
            /* VFCVTHS */
            index = VFCVTHS;
            break;
        default:
            break;
        }
        break;
    case 0x1C:
        switch (fn4) {
        case 0x0:
            /* VLDW_U */
            index = VLDW_U;
            break;
        case 0x1:
            /* VSTW_U */
            index = VSTW_U;
            break;
        case 0x2:
            /* VLDS_U */
            index = VLDS_U;
            break;
        case 0x3:
            /* VSTS_U */
            index = VSTS_U;
            break;
        case 0x4:
            /* VLDD_U */
            index = VLDD_U;
            break;
        case 0x5:
            /* VSTD_U */
            index = VSTD_U;
            break;
        case 0x8:
            /* VSTW_UL */
            index = VSTW_UL;
            break;
        case 0x9:
            /* VSTW_UH */
            index = VSTW_UH;
            break;
        case 0xa:
            /* VSTS_UL */
            index = VSTS_UL;
            break;
        case 0xb:
            /* VSTS_UH */
            index = VSTS_UH;
            break;
        case 0xc:
            /* VSTD_UL */
            index = VSTD_UL;
            break;
        case 0xd:
            /* VSTD_UH */
            index = VSTD_UH;
            break;
        case 0xe:
            /* VLDD_NC */
            index = VLDD_NC;
            break;
        case 0xf:
            /* VSTD_NC */
            index = VSTD_NC;
            break;
        default:
            break;
        }
        break;
    case 0x1D:
        /* LBR */
        index = LBR;
        break;
    case 0x1E:
        switch (fn4) {
        case 0x0:
            /* LDBU_A */
            index = LDBU_A;
            break;
        case 0x1:
            /* LDHU_A */
            index = LDHU_A;
            break;
        case 0x2:
            /* LDW_A */
            index = LDW_A;
            break;
        case 0x3:
            /* LDL_A */
            index = LDL_A;
            break;
        case 0x4:
            /* FLDS_A */
            index = FLDS_A;
            break;
        case 0x5:
            /* FLDD_A */
            index = FLDD_A;
            break;
        case 0x6:
            /* STBU_A */
            index = STBU_A;
            break;
        case 0x7:
            /* STHU_A */
            index = STHU_A;
            break;
        case 0x8:
            /* STW_A */
            index = STW_A;
            break;
        case 0x9:
            /* STL_A */
            index = STL_A;
            break;
        case 0xA:
            /* FSTS_A */
            index = FSTS_A;
            break;
        case 0xB:
            /* FSTD_A */
            index = FSTD_A;
            break;
        case 0xE:
            /* DPFHR */
            index = DPFHR;
            break;
        case 0xF:
            /* DPFHW */
            index = DPFHW;
            break;
        default:
            break;
        }
        break;
    case 0x20:
        /* LDBU */
        index = LDBU;
        break;
    case 0x21:
        /* LDHU */
        index = LDHU;
        break;
    case 0x22:
        /* LDW */
        index = LDW;
        break;
    case 0x23:
        /* LDL */
        index = LDL;
        break;
    case 0x24:
        /* LDL_U */
        index = LDL_U;
        break;
    case 0x25:
        if ((insn >> 12) & 1) {
            /* PRI_LDL */
            index = PRI_LDL;
        } else {
            /* PRI_LDW */
            index = PRI_LDW;
        }
        break;
    case 0x26:
        /* FLDS */
        index = FLDS;
        break;
    case 0x27:
        /* FLDD */
        index = FLDD;
        break;
    case 0x28:
        /* STB */
        index = STB;
        break;
    case 0x29:
        /* STH */
        index = STH;
        break;
    case 0x2a:
        /* STW */
        index = STW;
        break;
    case 0x2b:
        /* STL */
        index = STL;
        break;
    case 0x2c:
        /* STL_U */
        index = STL_U;
        break;
    case 0x2d:
        if ((insn >> 12) & 1) {
            /* PRI_STL */
            index = PRI_STL;
        } else {
            /* PRI_STW */
            index = PRI_STW;
        }
        break;
    case 0x2e:
        /* FSTS */
        index = FSTS;
        break;
    case 0x2f:
        /* FSTD */
        index = FSTD;
        break;
    case 0x30:
        /* BEQ */
        index = BEQ;
        break;
    case 0x31:
        /* BNE */
        index = BNE;
        break;
    case 0x32:
        /* BLT */
        index = BLT;
        break;
    case 0x33:
        /* BLE */
        index = BLE;
        break;
    case 0x34:
        /* BGT */
        index = BGT;
        break;
    case 0x35:
        /* BGE */
        index = BGE;
        break;
    case 0x36:
        /* BLBC */
        index = BLBC;
        break;
    case 0x37:
        /* BLBS */
        index = BLBS;
        break;
    case 0x38:
        /* FBEQ */
        index = FBEQ;
        break;
    case 0x39:
        /* FBNE */
        index = FBNE;
        break;
    case 0x3a:
        /* FBLT */
        index = FBLT;
        break;
    case 0x3b:
        /* FBLE */
        index = FBLE;
        break;
    case 0x3c:
        /* FBGT */
        index = FBGT;
        break;
    case 0x3d:
        /* FBGE */
        index = FBGE;
        break;
    case 0x3f:
        /* LDIH */
        index = LDIH;
        break;
    case 0x3e:
        /* LDI */
        index = LDI;
        break;
    default:
do_invalid:
            break;
    }
    count = tcg_temp_new();
    offs = offsetof(CPUSW64State, insn_count[index]);
    tcg_gen_ld_i64(count, cpu_env, offs);
    tcg_gen_addi_i64(count, count, 1);
    tcg_gen_st_i64(count, cpu_env, offs);
    tcg_temp_free(count);
}
