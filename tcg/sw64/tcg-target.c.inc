/*
 * Initial TCG Implementation for sw_64
 *
 */

#include "../tcg-pool.c.inc"
#include "qemu/bitops.h"

/* We're going to re-use TCGType in setting of the SF bit, which controls
   the size of the operation performed.  If we know the values match, it
   makes things much cleaner.  */
QEMU_BUILD_BUG_ON(TCG_TYPE_I32 != 0 || TCG_TYPE_I64 != 1);
static const tcg_insn_unit *tb_ret_addr;

#ifdef CONFIG_DEBUG_TCG
static const char * const tcg_target_reg_names[TCG_TARGET_NB_REGS] = {
    "X0", "X1", "X2", "X3", "X4", "X5", "X6", "X7",
    "X8", "X9", "X10", "X11", "X12", "X13", "X14", "fp",
    "X16", "X17", "X18", "X19", "X20", "X21", "X22", "X23",
    "X24", "X25", "X26", "X27", "X28", "X29", "Xsp", "X31",

    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
    "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
};
#endif /* CONFIG_DEBUG_TCG */

static const int tcg_target_reg_alloc_order[] = {
    /* TCG_REG_X9 qemu saved for AREG0*/
    TCG_REG_X10, TCG_REG_X11, TCG_REG_X12, TCG_REG_X13, TCG_REG_X14,

    TCG_REG_X0, TCG_REG_X1, TCG_REG_X2, TCG_REG_X3, TCG_REG_X4,
    TCG_REG_X5, TCG_REG_X6, TCG_REG_X7, TCG_REG_X8,

    TCG_REG_X22, TCG_REG_X23, TCG_REG_X24, /*TCG_REG_X25, TCG_REG_X26, TCG_REG_X27, */

    /* TCG_REG_SP=TCG_REG_X15 saved for system*/
    TCG_REG_X16, TCG_REG_X17, TCG_REG_X18, TCG_REG_X19, TCG_REG_X20, TCG_REG_X21, TCG_REG_X28, /* TCG_REG_X29, TCG_REG_X30, TCG_REG_X31 */

    /* TCG_REG_TMP=TCG_REG_X27 reserved as temporary register */
    /* TCG_REG_TMP2=TCG_REG_X25 reserved as temporary register */
    /* TCG_REG_RA=TCG_REG_X26 reserved as temporary */
    /* TCG_REG_GP=TCG_REG_X29 gp saved for system*/
    /* TCG_REG_SP=TCG_REG_X30 sp saved for system*/
    /* TCG_REG_ZERO=TCG_REG_X31 zero saved for system*/

    TCG_REG_F2, TCG_REG_F3, TCG_REG_F4, TCG_REG_F5, TCG_REG_F6, TCG_REG_F7, TCG_REG_F8, TCG_REG_F9,  /* f2-f9 saved registers */
    /* TCG_VEC_TMP=TCG_REG_F10, TCG_VEC_TMP2=TCG_REG_F11, are saved as temporary */
    TCG_REG_F12, TCG_REG_F13, TCG_REG_F14, TCG_REG_F15,  /* f10-f15 temporary registers */

    TCG_REG_F22, TCG_REG_F23, TCG_REG_F24, TCG_REG_F25, TCG_REG_F26, TCG_REG_F27, TCG_REG_F28, TCG_REG_F29, TCG_REG_F30,  /* f22-f30 temporary registers */
    /* TCG_REG_F31, zero saved for system */

    TCG_REG_F16, TCG_REG_F17, TCG_REG_F18, TCG_REG_F19, TCG_REG_F20, TCG_REG_F21,  /* input args */

    TCG_REG_F0, TCG_REG_F1, /*output args */
};

static const int tcg_target_call_iarg_regs[6] = {
    TCG_REG_X16, TCG_REG_X17, TCG_REG_X18, TCG_REG_X19, TCG_REG_X20, TCG_REG_X21,
};
static const int tcg_target_call_oarg_regs[1] = {
    TCG_REG_X0,
};

#define TCG_REG_TMP TCG_REG_X27
#define TCG_REG_TMP2 TCG_REG_X25
#define TCG_FLOAT_TMP TCG_REG_F10
#define TCG_FLOAT_TMP2 TCG_REG_F11

#define ALL_GENERAL_REGS  0xffffffffu
#define ALL_QLDST_REGS   ALL_GENERAL_REGS
#define PUSH_SIZE  ((15-9+1+1) * 8)
#define FRAME_SIZE \
    ((PUSH_SIZE \
      + TCG_STATIC_CALL_ARGS_SIZE \
      + CPU_TEMP_BUF_NLONGS * sizeof(long) \
      + TCG_TARGET_STACK_ALIGN - 1) \
     & ~(TCG_TARGET_STACK_ALIGN - 1))

/* We encode the format of the insn into the beginning of the name, so that
   we can have the preprocessor help "typecheck" the insn vs the output
   function.  We don't have nice names for the formats, so we use the section
   number of the architecture reference manual in which the instruction
   group is described.  */
#define OPC_OP(x)     ((x & 0x3f) << 26)
#define OPC_FUNC(x)   ((x & 0xff) << 5)
#define OPC_FUNC_COMPLEX(x)   ((x & 0xff) << 10)
typedef enum {
    OPC_NOP       =0X43ff075f,
    OPC_SYS_CALL  =OPC_OP(0x00),
    OPC_CALL      =OPC_OP(0x01),
    OPC_RET       =OPC_OP(0x02),
    OPC_JMP       =OPC_OP(0x03),
    OPC_BR        =OPC_OP(0x04),
    OPC_BSR       =OPC_OP(0x05),
    OPC_PRI_RET   =OPC_OP(0x07),
    OPC_LDWE      =OPC_OP(0x09),
    OPC_LDSE      =OPC_OP(0x0A),
    OPC_LDDE      =OPC_OP(0x0B),
    OPC_VLDS      =OPC_OP(0x0C),
    OPC_VLDD      =OPC_OP(0x0D),
    OPC_VSTS      =OPC_OP(0x0E),
    OPC_VSTD      =OPC_OP(0x0F),

    OPC_LDBU      =OPC_OP(0x20),
    OPC_LDHU      =OPC_OP(0x21),
    OPC_LDW       =OPC_OP(0x22),
    OPC_LDL       =OPC_OP(0x23),
    OPC_LDL_U     =OPC_OP(0x24),
    OPC_FLDS      =OPC_OP(0X26),
    OPC_PRI_LD    =OPC_OP(0x25),
    OPC_FLDD      =OPC_OP(0X27),
    OPC_STB       =OPC_OP(0X28),
    OPC_STH       =OPC_OP(0x29),
    OPC_STW       =OPC_OP(0x2a),
    OPC_STL       =OPC_OP(0x2B),
    OPC_STL_U     =OPC_OP(0x2C),
    OPC_PRI_ST    =OPC_OP(0x2D),
    OPC_FSTS      =OPC_OP(0x2E),
    OPC_FSTD      =OPC_OP(0x2F),

    OPC_BEQ       =OPC_OP(0x30),
    OPC_BNE       =OPC_OP(0x31),
    OPC_BLT       =OPC_OP(0x32),
    OPC_BLE       =OPC_OP(0x33),
    OPC_BGT       =OPC_OP(0x34),
    OPC_BGE       =OPC_OP(0x35),
    OPC_BLBC      =OPC_OP(0x36),
    OPC_BLBS      =OPC_OP(0x37),

    OPC_FBEQ      =OPC_OP(0x38),
    OPC_FBNE      =OPC_OP(0x39),
    OPC_FBLT      =OPC_OP(0x3A),
    OPC_FBLE      =OPC_OP(0x3B),
    OPC_FBGT      =OPC_OP(0x3C),
    OPC_FBGE      =OPC_OP(0x3D),
    OPC_LDI       =OPC_OP(0x3E),
    OPC_LDIH      =OPC_OP(0x3F),

    OPC_ADDW        =(OPC_OP(0x10) | OPC_FUNC(0x0)),
    OPC_ADDW_I      =(OPC_OP(0x12) | OPC_FUNC(0x0)),
    OPC_SUBW        =(OPC_OP(0x10) | OPC_FUNC(0x1)),
    OPC_SUBW_I      =(OPC_OP(0x12) | OPC_FUNC(0x1)),
    OPC_S4ADDW      =(OPC_OP(0x10) | OPC_FUNC(0x02)),
    OPC_S4ADDW_I    =(OPC_OP(0x12) | OPC_FUNC(0x02)),
    OPC_S4SUBW      =(OPC_OP(0x10) | OPC_FUNC(0x03)),
    OPC_S4SUBW_I    =(OPC_OP(0x12) | OPC_FUNC(0x03)),

    OPC_S8ADDW      =(OPC_OP(0x10) | OPC_FUNC(0x04)),
    OPC_S8ADDW_I    =(OPC_OP(0x12) | OPC_FUNC(0x04)),
    OPC_S8SUBW      =(OPC_OP(0x10) | OPC_FUNC(0x05)),
    OPC_S8SUBW_I    =(OPC_OP(0x12) | OPC_FUNC(0x05)),

    OPC_ADDL        =(OPC_OP(0x10) | OPC_FUNC(0x8)),
    OPC_ADDL_I      =(OPC_OP(0x12) | OPC_FUNC(0x8)),
    OPC_SUBL        =(OPC_OP(0x10) | OPC_FUNC(0x9)),
    OPC_SUBL_I      =(OPC_OP(0x12) | OPC_FUNC(0x9)),

    OPC_S4ADDL      =(OPC_OP(0x10) | OPC_FUNC(0xA)),
    OPC_S4ADDL_I    =(OPC_OP(0x12) | OPC_FUNC(0xA)),
    OPC_S4SUBL      =(OPC_OP(0x10) | OPC_FUNC(0xB)),
    OPC_S4SUBL_I    =(OPC_OP(0x12) | OPC_FUNC(0xB)),

    OPC_S8ADDL      =(OPC_OP(0x10) | OPC_FUNC(0xC)),
    OPC_S8ADDL_I    =(OPC_OP(0x12) | OPC_FUNC(0xC)),
    OPC_S8SUBL      =(OPC_OP(0x10) | OPC_FUNC(0xD)),
    OPC_S8SUBL_I    =(OPC_OP(0x12) | OPC_FUNC(0xD)),

    OPC_MULW        =(OPC_OP(0x10) | OPC_FUNC(0x10)),
    OPC_MULW_I      =(OPC_OP(0x12) | OPC_FUNC(0x10)),
    OPC_MULL        =(OPC_OP(0x10) | OPC_FUNC(0x18)),
    OPC_MULL_I      =(OPC_OP(0x12) | OPC_FUNC(0x18)),

    OPC_UMULH       =(OPC_OP(0x10) | OPC_FUNC(0x19)),
    OPC_UMULH_I     =(OPC_OP(0x12) | OPC_FUNC(0x19)),

    OPC_CTPOP       =(OPC_OP(0x10) | OPC_FUNC(0x58)),
    OPC_CTLZ        =(OPC_OP(0x10) | OPC_FUNC(0x59)),
    OPC_CTTZ        =(OPC_OP(0x10) | OPC_FUNC(0x5A)),

    OPC_ZAP         =(OPC_OP(0x10) | OPC_FUNC(0x68)),
    OPC_ZAP_I       =(OPC_OP(0x12) | OPC_FUNC(0x68)),
    OPC_ZAPNOT      =(OPC_OP(0x10) | OPC_FUNC(0x69)),
    OPC_ZAPNOT_I    =(OPC_OP(0x12) | OPC_FUNC(0x69)),

    OPC_SEXTB       =(OPC_OP(0x10) | OPC_FUNC(0x6A)),
    OPC_SEXTB_I     =(OPC_OP(0x12) | OPC_FUNC(0x6A)),
    OPC_SEXTH       =(OPC_OP(0x10) | OPC_FUNC(0x6B)),
    OPC_SEXTH_I     =(OPC_OP(0x12) | OPC_FUNC(0x6B)),

    OPC_CMPEQ       =(OPC_OP(0x10) | OPC_FUNC(0x28)),
    OPC_CMPEQ_I     =(OPC_OP(0x12) | OPC_FUNC(0x28)),

    OPC_CMPLT       =(OPC_OP(0x10) | OPC_FUNC(0x29)),
    OPC_CMPLT_I     =(OPC_OP(0x12) | OPC_FUNC(0x29)),
    OPC_CMPLE       =(OPC_OP(0x10) | OPC_FUNC(0x2A)),
    OPC_CMPLE_I     =(OPC_OP(0x12) | OPC_FUNC(0x2A)),

    OPC_CMPULT      =(OPC_OP(0x10) | OPC_FUNC(0x2B)),
    OPC_CMPULT_I    =(OPC_OP(0x12) | OPC_FUNC(0x2B)),
    OPC_CMPULE      =(OPC_OP(0x10) | OPC_FUNC(0x2C)),
    OPC_CMPULE_I    =(OPC_OP(0x12) | OPC_FUNC(0x2C)),

    OPC_AND         =(OPC_OP(0x10) | OPC_FUNC(0x38)),
    OPC_BIC         =(OPC_OP(0x10) | OPC_FUNC(0x39)),
    OPC_BIS         =(OPC_OP(0x10) | OPC_FUNC(0x3A)),
    OPC_ORNOT       =(OPC_OP(0x10) | OPC_FUNC(0x3B)),
    OPC_XOR         =(OPC_OP(0x10) | OPC_FUNC(0x3C)),
    OPC_EQV         =(OPC_OP(0x10) | OPC_FUNC(0x3D)),

    OPC_AND_I       =(OPC_OP(0x12) | OPC_FUNC(0x38)),
    OPC_BIC_I       =(OPC_OP(0x12) | OPC_FUNC(0x39)),
    OPC_BIS_I       =(OPC_OP(0x12) | OPC_FUNC(0x3A)),
    OPC_ORNOT_I     =(OPC_OP(0x12) | OPC_FUNC(0x3B)),
    OPC_XOR_I       =(OPC_OP(0x12) | OPC_FUNC(0x3C)),
    OPC_EQV_I       =(OPC_OP(0x12) | OPC_FUNC(0x3D)),

    OPC_SLL         =(OPC_OP(0x10) | OPC_FUNC(0x48)),
    OPC_SRL         =(OPC_OP(0x10) | OPC_FUNC(0x49)),
    OPC_SRA         =(OPC_OP(0x10) | OPC_FUNC(0x4A)),
    OPC_SLL_I       =(OPC_OP(0x12) | OPC_FUNC(0x48)),
    OPC_SRL_I       =(OPC_OP(0x12) | OPC_FUNC(0x49)),
    OPC_SRA_I       =(OPC_OP(0x12) | OPC_FUNC(0x4A)),

    OPC_SELEQ       =(OPC_OP(0x11) | OPC_FUNC_COMPLEX(0x00)),
    OPC_SELGE       =(OPC_OP(0x11) | OPC_FUNC_COMPLEX(0x01)),
    OPC_SELGT       =(OPC_OP(0x11) | OPC_FUNC_COMPLEX(0x02)),
    OPC_SELLE       =(OPC_OP(0x11) | OPC_FUNC_COMPLEX(0x03)),
    OPC_SELLT       =(OPC_OP(0x11) | OPC_FUNC_COMPLEX(0x04)),
    OPC_SELNE       =(OPC_OP(0x11) | OPC_FUNC_COMPLEX(0x05)),
    OPC_SELLBC      =(OPC_OP(0x11) | OPC_FUNC_COMPLEX(0x06)),
    OPC_SELLBS      =(OPC_OP(0x11) | OPC_FUNC_COMPLEX(0x07)),
    OPC_SELEQ_I     =(OPC_OP(0x13) | OPC_FUNC_COMPLEX(0x00)),
    OPC_SELGE_I     =(OPC_OP(0x13) | OPC_FUNC_COMPLEX(0x01)),
    OPC_SELGT_I     =(OPC_OP(0x13) | OPC_FUNC_COMPLEX(0x02)),
    OPC_SELLE_I     =(OPC_OP(0x13) | OPC_FUNC_COMPLEX(0x03)),
    OPC_SELLT_I     =(OPC_OP(0x13) | OPC_FUNC_COMPLEX(0x04)),
    OPC_SELNE_I     =(OPC_OP(0x13) | OPC_FUNC_COMPLEX(0x05)),
    OPC_SELLBC_I    =(OPC_OP(0x13) | OPC_FUNC_COMPLEX(0x06)),
    OPC_SELLBS_I    =(OPC_OP(0x13) | OPC_FUNC_COMPLEX(0x07)),

    OPC_INS0B       =(OPC_OP(0x10) | OPC_FUNC(0x40)),
    OPC_INS1B       =(OPC_OP(0x10) | OPC_FUNC(0x41)),
    OPC_INS2B       =(OPC_OP(0x10) | OPC_FUNC(0x42)),
    OPC_INS3B       =(OPC_OP(0x10) | OPC_FUNC(0x43)),
    OPC_INS4B       =(OPC_OP(0x10) | OPC_FUNC(0x44)),
    OPC_INS5B       =(OPC_OP(0x10) | OPC_FUNC(0x45)),
    OPC_INS6B       =(OPC_OP(0x10) | OPC_FUNC(0x46)),
    OPC_INS7B       =(OPC_OP(0x10) | OPC_FUNC(0x47)),
    OPC_INS0B_I     =(OPC_OP(0x12) | OPC_FUNC(0x40)),
    OPC_INS1B_I     =(OPC_OP(0x12) | OPC_FUNC(0x41)),
    OPC_INS2B_I     =(OPC_OP(0x12) | OPC_FUNC(0x42)),
    OPC_INS3B_I     =(OPC_OP(0x12) | OPC_FUNC(0x43)),
    OPC_INS4B_I     =(OPC_OP(0x12) | OPC_FUNC(0x44)),
    OPC_INS5B_I     =(OPC_OP(0x12) | OPC_FUNC(0x45)),
    OPC_INS6B_I     =(OPC_OP(0x12) | OPC_FUNC(0x46)),
    OPC_INS7B_I     =(OPC_OP(0x12) | OPC_FUNC(0x47)),
          
    OPC_EXT0B       =(OPC_OP(0x10) | OPC_FUNC(0x50)),
    OPC_EXT1B       =(OPC_OP(0x10) | OPC_FUNC(0x51)),
    OPC_EXT2B       =(OPC_OP(0x10) | OPC_FUNC(0x52)),
    OPC_EXT3B       =(OPC_OP(0x10) | OPC_FUNC(0x53)),
    OPC_EXT4B       =(OPC_OP(0x10) | OPC_FUNC(0x54)),
    OPC_EXT5B       =(OPC_OP(0x10) | OPC_FUNC(0x55)),
    OPC_EXT6B       =(OPC_OP(0x10) | OPC_FUNC(0x56)),
    OPC_EXT7B       =(OPC_OP(0x10) | OPC_FUNC(0x57)),
    OPC_EXT0B_I     =(OPC_OP(0x12) | OPC_FUNC(0x50)),
    OPC_EXT1B_I     =(OPC_OP(0x12) | OPC_FUNC(0x51)),
    OPC_EXT2B_I     =(OPC_OP(0x12) | OPC_FUNC(0x52)),
    OPC_EXT3B_I     =(OPC_OP(0x12) | OPC_FUNC(0x53)),
    OPC_EXT4B_I     =(OPC_OP(0x12) | OPC_FUNC(0x54)),
    OPC_EXT5B_I     =(OPC_OP(0x12) | OPC_FUNC(0x55)),
    OPC_EXT6B_I     =(OPC_OP(0x12) | OPC_FUNC(0x56)),
    OPC_EXT7B_I     =(OPC_OP(0x12) | OPC_FUNC(0x57)),
    
    OPC_MASK0B      =(OPC_OP(0x10) | OPC_FUNC(0x60)),
    OPC_MASK1B      =(OPC_OP(0x10) | OPC_FUNC(0x61)),
    OPC_MASK2B      =(OPC_OP(0x10) | OPC_FUNC(0x62)),
    OPC_MASK3B      =(OPC_OP(0x10) | OPC_FUNC(0x63)),
    OPC_MASK4B      =(OPC_OP(0x10) | OPC_FUNC(0x64)),
    OPC_MASK5B      =(OPC_OP(0x10) | OPC_FUNC(0x65)),
    OPC_MASK6B      =(OPC_OP(0x10) | OPC_FUNC(0x66)),
    OPC_MASK7B      =(OPC_OP(0x10) | OPC_FUNC(0x67)),
    OPC_MASK0B_I    =(OPC_OP(0x12) | OPC_FUNC(0x60)),
    OPC_MASK1B_I    =(OPC_OP(0x12) | OPC_FUNC(0x61)),
    OPC_MASK2B_I    =(OPC_OP(0x12) | OPC_FUNC(0x62)),
    OPC_MASK3B_I    =(OPC_OP(0x12) | OPC_FUNC(0x63)),
    OPC_MASK4B_I    =(OPC_OP(0x12) | OPC_FUNC(0x64)),
    OPC_MASK5B_I    =(OPC_OP(0x12) | OPC_FUNC(0x65)),
    OPC_MASK6B_I    =(OPC_OP(0x12) | OPC_FUNC(0x66)),
    OPC_MASK7B_I    =(OPC_OP(0x12) | OPC_FUNC(0x67)),
    
    OPC_CNPGEB      =(OPC_OP(0x10) | OPC_FUNC(0x6C)),
    OPC_CNPGEB_I    =(OPC_OP(0x12) | OPC_FUNC(0x6C)),

    OPC_MEMB        =(OPC_OP(0x06) | OPC_FUNC(0x0)),
    OPC_RTC         =(OPC_OP(0x06) | OPC_FUNC(0x20)),

    /*float insn*/
    OPC_RFPCR = (OPC_OP(0x18) | OPC_FUNC(0x50)),
    OPC_WFPCR = (OPC_OP(0x18) | OPC_FUNC(0x51)),
    OPC_SETFPEC0 = (OPC_OP(0x18) | OPC_FUNC(0x54)),
    OPC_SETFPEC1 = (OPC_OP(0x18) | OPC_FUNC(0x55)),
    OPC_SETFPEC2 = (OPC_OP(0x18) | OPC_FUNC(0x56)),
    OPC_SETFPEC3 = (OPC_OP(0x18) | OPC_FUNC(0x57)),


    OPC_IFMOVS = (OPC_OP(0x18) | OPC_FUNC(0x40)),
    OPC_IFMOVD = (OPC_OP(0x18) | OPC_FUNC(0x41)),
    OPC_FIMOVS = (OPC_OP(0x10) | OPC_FUNC(0x70)),
    OPC_FIMOVD = (OPC_OP(0x10) | OPC_FUNC(0x78)),
    
    /*translate S--D*/
    /*translate S/D--Long*/
    OPC_FCVTSD = (OPC_OP(0x18) | OPC_FUNC(0x20)),
    OPC_FCVTDS = (OPC_OP(0x18) | OPC_FUNC(0x21)),
    OPC_FCVTDL_G = (OPC_OP(0x18) | OPC_FUNC(0x22)),
    OPC_FCVTDL_P = (OPC_OP(0x18) | OPC_FUNC(0x23)),
    OPC_FCVTDL_Z = (OPC_OP(0x18) | OPC_FUNC(0x24)),
    OPC_FCVTDL_N = (OPC_OP(0x18) | OPC_FUNC(0x25)),
    OPC_FCVTDL = (OPC_OP(0x18) | OPC_FUNC(0x27)),
    OPC_FCVTLS = (OPC_OP(0x18) | OPC_FUNC(0x2D)),
    OPC_FCVTLD = (OPC_OP(0x18) | OPC_FUNC(0x2F)),


    OPC_FADDS = (OPC_OP(0x18) | OPC_FUNC(0x00)),
    OPC_FADDD = (OPC_OP(0x18) | OPC_FUNC(0x01)),
    OPC_FSUBS = (OPC_OP(0x18) | OPC_FUNC(0x02)),
    OPC_FSUBD = (OPC_OP(0x18) | OPC_FUNC(0x03)),
    OPC_FMULS = (OPC_OP(0x18) | OPC_FUNC(0x04)),
    OPC_FMULD = (OPC_OP(0x18) | OPC_FUNC(0x05)),
    OPC_FDIVS = (OPC_OP(0x18) | OPC_FUNC(0x06)),
    OPC_FDIVD = (OPC_OP(0x18) | OPC_FUNC(0x07)),
    OPC_FSQRTS = (OPC_OP(0x18) | OPC_FUNC(0x08)),
    OPC_FSQRTD = (OPC_OP(0x18) | OPC_FUNC(0x09)),
}SW_64Insn;

static void tcg_out_insn_br(TCGContext *s, SW_64Insn insn, TCGReg rd, intptr_t imm64);
static void tcg_out_insn_ldst(TCGContext *s, SW_64Insn insn, TCGReg rd, TCGReg rn, intptr_t imm16);
static void tcg_out_insn_simpleReg(TCGContext *s, SW_64Insn insn, TCGReg rd, TCGReg rn, TCGReg rm);
static void tcg_out_insn_simple(TCGContext *s, SW_64Insn insn_Imm, SW_64Insn insn_Reg, TCGReg rd, TCGReg rn, intptr_t imm64);
static void tcg_out_insn_simpleImm(TCGContext *s, SW_64Insn insn_Imm, TCGReg rd, TCGReg rn, intptr_t imm64);
static void tcg_out_insn_bitImm(TCGContext *s, SW_64Insn insn_Imm, TCGReg rd, TCGReg rn, unsigned long imm64);
static void tcg_out_insn_bit(TCGContext *s, SW_64Insn insn_Imm, SW_64Insn insn_Reg, TCGReg rd, TCGReg rn, unsigned long imm64);
static void tcg_out_insn_complexReg(TCGContext *s, SW_64Insn insn, TCGReg cond, TCGReg rd, TCGReg rn, TCGReg rm);
static void tcg_out_movcond(TCGContext *s, TCGCond cond, TCGReg ret,TCGReg a1,TCGReg a2, bool const_b, TCGReg v1, TCGReg v2);
static bool reloc_pc21(tcg_insn_unit *src_rw, const tcg_insn_unit *target);
static inline uint32_t tcg_in32(TCGContext *s);
static void tcg_out_movr(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn);
static void tcg_out_ldst(TCGContext *s, SW_64Insn insn, TCGReg rd, TCGReg rn, intptr_t offset, bool sign);
static void tcg_out_cond_cmp(TCGContext *s, TCGCond cond, TCGReg ret,  TCGArg a, TCGArg b, bool const_b);
static void tcg_out_addsubi(TCGContext *s, int ext, TCGReg rd, TCGReg rn, int64_t aimm);
static inline void tcg_out_extr(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, TCGReg rm, unsigned int m);
static inline void tcg_out_rotl_Reg(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, TCGReg rm);
static inline void tcg_out_rotr_Reg(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, TCGReg rm);
static inline void tcg_out_rotl_Imm(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, unsigned int m);
static inline void tcg_out_rotr_Imm(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, unsigned int m);
static void tcg_out_cltz(TCGContext *s, SW_64Insn opc_clz, TCGType ext, TCGReg rd, TCGReg rn, TCGArg b, bool const_b);
static inline void tcg_out_bswap16u(TCGContext *s, TCGReg rd, TCGReg rn);
static inline void tcg_out_bswap16s(TCGContext *s, TCGReg rd, TCGReg rn);
static inline void tcg_out_bswap32u(TCGContext *s, TCGReg rd, TCGReg rn);
static inline void tcg_out_bswap32s(TCGContext *s, TCGReg rd, TCGReg rn);
static inline void tcg_out_bswap64(TCGContext *s, TCGReg rd, TCGReg rn);
static void tcg_out_qemu_st(TCGContext *s, TCGReg data_reg, TCGReg addr_reg, MemOpIdx oi);
static void tcg_out_qemu_ld(TCGContext *s, TCGReg data_reg, TCGReg addr_reg, MemOpIdx oi, TCGType ext);
static void tcg_out_setcond(TCGContext *s, TCGCond cond, TCGReg ret, TCGReg arg1, TCGReg arg2);
static void tcg_out_extract(TCGContext *s, TCGReg rd, TCGReg rn, int pos, int len);
static void tcg_out_dep(TCGContext *s, TCGReg rd, TCGReg rn, int pos, int len);
static void tcg_out_mulsh64(TCGContext *s,  TCGReg rd, TCGReg rn, TCGReg rm);

#define tcg_out_insn_jump tcg_out_insn_ldst
#define tcg_out_insn_bitReg tcg_out_insn_simpleReg

static void tcg_target_init(TCGContext *s)
{
    tcg_target_available_regs[TCG_TYPE_I32] = 0xffffffffu;
    tcg_target_available_regs[TCG_TYPE_I64] = 0xffffffffu;
    tcg_target_available_regs[TCG_TYPE_V64] = 0xffffffff00000000ull;
    tcg_target_available_regs[TCG_TYPE_V128] = 0xffffffff00000000ull;
    tcg_target_call_clobber_regs = -1ull;
    
    //sw_64 callee saved x9-x15
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_X9);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_X10);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_X11);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_X12);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_X13);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_X14);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_X15);
    
    //sw_64 callee saved f2~f9
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_F2);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_F3);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_F4);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_F5);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_F6);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_F7);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_F8);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_F9);

    s->reserved_regs = 0;
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_SP);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_FP);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_TMP);     //TCG_REG_X27
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_TMP2);    //TCG_REG_X25
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_RA);      //TCG_REG_X26
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_X29);     /*sw_64 platform register */
    tcg_regset_set_reg(s->reserved_regs, TCG_FLOAT_TMP);   /*sw_64 platform register */
    tcg_regset_set_reg(s->reserved_regs, TCG_FLOAT_TMP2);  /*sw_64 platform register */
}


#ifndef CONFIG_SOFTMMU
    #define USE_GUEST_BASE  guest_base != 0
    #define TCG_REG_GUEST_BASE TCG_REG_X14
#endif


#define zeroExt 0
#define sigExt  1


static void tcg_target_qemu_prologue(TCGContext *s)
{
    TCGReg r;
    int ofs;
    
    /* allocate space for all saved registers */
    /* subl $sp,PUSH_SIZE,$sp */
    tcg_out_insn_simple(s, OPC_SUBL_I, OPC_SUBL, TCG_REG_SP, TCG_REG_SP, PUSH_SIZE);
    
    /* Push (FP, LR)  */
    /* stl $fp,0($sp) */
    tcg_out_insn_ldst(s, OPC_STL, TCG_REG_FP, TCG_REG_SP, 0);
    /* stl $26,8($sp) */
    tcg_out_insn_ldst(s, OPC_STL, TCG_REG_RA, TCG_REG_SP, 8);


    /* Set up frame pointer for canonical unwinding.  */
    /* TCG_REG_FP=TCG_REG_SP */
    tcg_out_movr(s, TCG_TYPE_I64, TCG_REG_FP, TCG_REG_SP);

    /* Store callee-preserved regs x9..x14.  */
    for (r = TCG_REG_X9; r <= TCG_REG_X14; r += 1){
        ofs = (r - TCG_REG_X9 + 2) * 8;
        tcg_out_insn_ldst(s, OPC_STL, r, TCG_REG_SP, ofs);
    }

    /* Make stack space for TCG locals.  */
    /* subl $sp,FRAME_SIZE-PUSH_SIZE,$sp */
    tcg_out_insn_simple(s, OPC_SUBL_I, OPC_SUBL, TCG_REG_SP, TCG_REG_SP, FRAME_SIZE - PUSH_SIZE);

    /* Inform TCG about how to find TCG locals with register, offset, size.  */
    tcg_set_frame(s, TCG_REG_SP, TCG_STATIC_CALL_ARGS_SIZE,
                  CPU_TEMP_BUF_NLONGS * sizeof(long));

#if !defined(CONFIG_SOFTMMU)
    if (USE_GUEST_BASE) {
        tcg_out_movi(s, TCG_TYPE_PTR, TCG_REG_GUEST_BASE, guest_base);
        tcg_regset_set_reg(s->reserved_regs, TCG_REG_GUEST_BASE);
    }
#endif
    
    /* TCG_AREG0=tcg_target_call_iarg_regs[0], on sw, we mov $16 to $9 */
    tcg_out_mov(s, TCG_TYPE_I64, TCG_AREG0, tcg_target_call_iarg_regs[0]);
    tcg_out_insn_jump(s, OPC_JMP, TCG_REG_ZERO, tcg_target_call_iarg_regs[1], 0);

    /*
     * Return path for goto_ptr. Set return value to 0, a-la exit_tb,
     * and fall through to the rest of the epilogue.
     */
    tcg_code_gen_epilogue = tcg_splitwx_to_rx(s->code_ptr);
    tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_X0, 0);

    /* TB epilogue */
    tb_ret_addr = tcg_splitwx_to_rx(s->code_ptr);

    /* Remove TCG locals stack space.  */
    /* addl $sp,FRAME_SIZE-PUSH_SIZE,$sp */
    tcg_out_insn_simple(s, OPC_ADDL_I, OPC_ADDL, TCG_REG_SP, TCG_REG_SP, FRAME_SIZE - PUSH_SIZE);

    /* Restore registers x9..x14.  */
    for (r = TCG_REG_X9; r <= TCG_REG_X14; r += 1) {
        int ofs = (r - TCG_REG_X9 + 2) * 8;
        tcg_out_insn_ldst(s, OPC_LDL, r, TCG_REG_SP, ofs);
    }

    
    /* Pop (FP, LR) */
    /* ldl $fp,0($sp) */
    tcg_out_insn_ldst(s, OPC_LDL, TCG_REG_FP, TCG_REG_SP, 0);
    /* ldl $26,8($sp) */
    tcg_out_insn_ldst(s, OPC_LDL, TCG_REG_RA, TCG_REG_SP, 8);
    
    /* restore SP to previous frame. */
    /* addl $sp,PUSH_SIZE,$sp */
    tcg_out_insn_simple(s, OPC_ADDL_I, OPC_ADDL, TCG_REG_SP, TCG_REG_SP, PUSH_SIZE);
    
    tcg_out_insn_jump(s, OPC_RET, TCG_REG_ZERO, TCG_REG_RA, 0);
}

static bool tcg_out_mov(TCGContext *s, TCGType type, TCGReg ret, TCGReg arg)
{
    if (ret == arg) {
        return true;
    }
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        if (ret < 32 && arg < 32) {
            tcg_out_movr(s, type, ret, arg);
            break;
        } else if (ret < 32) {
            break;
        } else if (arg < 32) {
            break;
        }
        /* FALLTHRU */
    default:
        g_assert_not_reached();
    }
    return true;
}


static TCGConstraintSetIndex tcg_target_op_def(TCGOpcode op)
{
    switch (op) {
    case INDEX_op_goto_ptr:
        return C_O0_I1(r);

    case INDEX_op_ld8u_i32:
    case INDEX_op_ld8s_i32:
    case INDEX_op_ld16u_i32:
    case INDEX_op_ld16s_i32:
    case INDEX_op_ld_i32:
    case INDEX_op_ld8u_i64:
    case INDEX_op_ld8s_i64:
    case INDEX_op_ld16u_i64:
    case INDEX_op_ld16s_i64:
    case INDEX_op_ld32u_i64:
    case INDEX_op_ld32s_i64:
    case INDEX_op_ld_i64:
    case INDEX_op_neg_i32:
    case INDEX_op_neg_i64:
    case INDEX_op_not_i32:
    case INDEX_op_not_i64:
    case INDEX_op_bswap16_i32:
    case INDEX_op_bswap32_i32:
    case INDEX_op_bswap16_i64:
    case INDEX_op_bswap32_i64:
    case INDEX_op_bswap64_i64:
    case INDEX_op_ext8s_i32:
    case INDEX_op_ext16s_i32:
    case INDEX_op_ext8u_i32:
    case INDEX_op_ext16u_i32:
    case INDEX_op_ext8s_i64:
    case INDEX_op_ext16s_i64:
    case INDEX_op_ext32s_i64:
    case INDEX_op_ext8u_i64:
    case INDEX_op_ext16u_i64:
    case INDEX_op_ext32u_i64:
    case INDEX_op_ext_i32_i64:
    case INDEX_op_extu_i32_i64:
    case INDEX_op_extract_i32:
    case INDEX_op_extract_i64:
    case INDEX_op_sextract_i32:
    case INDEX_op_sextract_i64:
        return C_O1_I1(r, r);

    case INDEX_op_st8_i32:
    case INDEX_op_st16_i32:
    case INDEX_op_st_i32:
    case INDEX_op_st8_i64:
    case INDEX_op_st16_i64:
    case INDEX_op_st32_i64:
    case INDEX_op_st_i64:
        return C_O0_I2(rZ, r);

    case INDEX_op_add_i32:
    case INDEX_op_add_i64:
    case INDEX_op_sub_i32:
    case INDEX_op_sub_i64:
        return C_O1_I2(r, r, rU);//rA

    case INDEX_op_setcond_i32:
    case INDEX_op_setcond_i64:
        return C_O1_I2(r, r, rU);//compare,rA

    case INDEX_op_mul_i32:
    case INDEX_op_mul_i64:
    case INDEX_op_div_i32:
    case INDEX_op_div_i64:
    case INDEX_op_divu_i32:
    case INDEX_op_divu_i64:
    case INDEX_op_rem_i32:
    case INDEX_op_rem_i64:
    case INDEX_op_remu_i32:
    case INDEX_op_remu_i64:
    case INDEX_op_muluh_i64:
    case INDEX_op_mulsh_i64:
        return C_O1_I2(r, r, r);

    case INDEX_op_and_i32:
    case INDEX_op_and_i64:
    case INDEX_op_or_i32:
    case INDEX_op_or_i64:
    case INDEX_op_xor_i32:
    case INDEX_op_xor_i64:
    case INDEX_op_andc_i32:
    case INDEX_op_andc_i64:
    case INDEX_op_orc_i32:
    case INDEX_op_orc_i64:
    case INDEX_op_eqv_i32:
    case INDEX_op_eqv_i64:
        return C_O1_I2(r, r, rU);//rL

    case INDEX_op_shl_i32:
    case INDEX_op_shr_i32:
    case INDEX_op_sar_i32:
    case INDEX_op_rotl_i32:
    case INDEX_op_rotr_i32:
    case INDEX_op_shl_i64:
    case INDEX_op_shr_i64:
    case INDEX_op_sar_i64:
    case INDEX_op_rotl_i64:
    case INDEX_op_rotr_i64:
        return C_O1_I2(r, r, ri);

    case INDEX_op_clz_i32:
    case INDEX_op_clz_i64:
        return C_O1_I2(r, r, r); //rAL 

    case INDEX_op_ctz_i32:
    case INDEX_op_ctz_i64:
        return C_O1_I2(r, r, r);//rAL

    case INDEX_op_brcond_i32:
    case INDEX_op_brcond_i64:
        return C_O0_I2(r, rU);//rA

    case INDEX_op_movcond_i32:
    case INDEX_op_movcond_i64:
        return C_O1_I4(r, r, rU, rZ, rZ);//rA->rU

    case INDEX_op_qemu_ld_i32:
    case INDEX_op_qemu_ld_i64:
        return C_O1_I1(r, l);

    case INDEX_op_qemu_st_i32:
    case INDEX_op_qemu_st_i64:
        return C_O0_I2(lZ, l);

    case INDEX_op_deposit_i32:
    case INDEX_op_deposit_i64:
        return C_O1_I2(r, 0, rZ);

    case INDEX_op_extract2_i32:
    case INDEX_op_extract2_i64:
        return C_O1_I2(r, rZ, rZ);

    case INDEX_op_add2_i32:
    case INDEX_op_add2_i64:
    case INDEX_op_sub2_i32:
    case INDEX_op_sub2_i64:
        return C_O2_I4(r, r, rZ, rZ, rA, rMZ);

    case INDEX_op_add_vec:
    case INDEX_op_sub_vec:
    case INDEX_op_mul_vec:
    case INDEX_op_xor_vec:
    case INDEX_op_ssadd_vec:
    case INDEX_op_sssub_vec:
    case INDEX_op_usadd_vec:
    case INDEX_op_ussub_vec:
    case INDEX_op_smax_vec:
    case INDEX_op_smin_vec:
    case INDEX_op_umax_vec:
    case INDEX_op_umin_vec:
    case INDEX_op_shlv_vec:
    case INDEX_op_shrv_vec:
    case INDEX_op_sarv_vec:
        return C_O1_I2(w, w, w);
    case INDEX_op_not_vec:
    case INDEX_op_neg_vec:
    case INDEX_op_abs_vec:
    case INDEX_op_shli_vec:
    case INDEX_op_shri_vec:
    case INDEX_op_sari_vec:
        return C_O1_I1(w, w);
    case INDEX_op_ld_vec:
    case INDEX_op_dupm_vec:
        return C_O1_I1(w, r);
    case INDEX_op_st_vec:
        return C_O0_I2(w, r);
    case INDEX_op_dup_vec:
        return C_O1_I1(w, wr);
    case INDEX_op_or_vec:
    case INDEX_op_andc_vec:
        return C_O1_I2(w, w, wO);
    case INDEX_op_and_vec:
    case INDEX_op_orc_vec:
        return C_O1_I2(w, w, wN);
    case INDEX_op_cmp_vec:
        return C_O1_I2(w, w, wZ);
    case INDEX_op_bitsel_vec:
        return C_O1_I3(w, w, w, w);

    default:
        g_assert_not_reached();
    }
}


static void tcg_out_nop_fill(tcg_insn_unit *p, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        p[i] = OPC_NOP;
    }
}

/* SW instruction format of syscall
 * insn = opcode[31,26]:Function[25,0],
 */

/* SW instruction format of br(alias jump)
 * insn = opcode[31,26]:Rd[25,21]:disp[20,0],
 */
static void tcg_out_insn_br(TCGContext *s, SW_64Insn insn, TCGReg rd, intptr_t imm64)
{
    tcg_debug_assert(imm64 <= 0xfffff && imm64 >= -0x100000);
    tcg_out32(s, insn | (rd & 0x1f) << 21 | (imm64 & 0x1fffff));
}


/* SW instruction format of (load and store)
 * insn = opcode[31,26]:rd[25,21]:rn[20,16]:disp[15,0]
 */
static void tcg_out_insn_ldst(TCGContext *s, SW_64Insn insn, TCGReg rd, TCGReg rn, intptr_t imm16)
{
    tcg_debug_assert(imm16 <= 0x7fff && imm16 >= -0x8000);
    tcg_out32(s, insn | (rd & 0x1f) << 21 | (rn & 0x1f) << 16 | (imm16 & 0xffff));
}


/* SW instruction format of simple operator for Register
 * insn = opcode[31,26]:rn(ra)[25,21]:rn(rb)[20,16]:Zeors[15,13]:function[12,5]:rd(rc)[4,0]
 */
static void tcg_out_insn_simpleReg(TCGContext *s, SW_64Insn insn, TCGReg rd, TCGReg rn, TCGReg rm)
{
    tcg_out32(s, insn | (rn & 0x1f) << 21 | (rm & 0x1f) << 16 | (rd & 0x1f));
}

/* SW instruction format of simple operator for imm
 * insn = opcode[31,26]:rn(ra)[25,21]:disp[20,13]:function[12,5]:rd(rc)[4,0]
 */
static void tcg_out_insn_simple(TCGContext *s, SW_64Insn insn_Imm, SW_64Insn insn_Reg, TCGReg rd, TCGReg rn, intptr_t imm64)
{
    if(imm64 <= 0x7f && imm64 >= -0x80) {
        tcg_out32(s, insn_Imm | (rn & 0x1f) << 21 | (imm64 & 0xff) << 13 | (rd & 0x1f));
    }
    else {
    	tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_TMP, imm64);
	tcg_out_insn_simpleReg(s, insn_Reg, rd, rn, TCG_REG_TMP);
    }
}


static void tcg_out_insn_simpleImm(TCGContext *s, SW_64Insn insn_Imm, TCGReg rd, TCGReg rn, intptr_t imm64)
{
    tcg_debug_assert(imm64 <= 0x7f && imm64 >= -0x80);
    tcg_out32(s, insn_Imm | (rn & 0x1f) << 21 | (imm64 & 0xff) << 13 | (rd & 0x1f));

}

static void tcg_out_insn_bitImm(TCGContext *s, SW_64Insn insn_Imm, TCGReg rd, TCGReg rn, unsigned long imm64)
{
    tcg_debug_assert(imm64 <= 255);
    tcg_out32(s, insn_Imm | (rn & 0x1f) << 21 | (imm64 & 0xff) << 13  | (rd & 0x1f));
}
/* sw bit operation: and bis etc */
static void tcg_out_insn_bit(TCGContext *s, SW_64Insn insn_Imm, SW_64Insn insn_Reg, TCGReg rd, TCGReg rn, unsigned long imm64)
{
    if (imm64 <= 255) {
        tcg_out32(s, insn_Imm | (rn & 0x1f) << 21 | (imm64 & 0xff) << 13  | (rd & 0x1f));
    }
    else {
        tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_TMP, imm64);
        tcg_out_insn_bitReg(s, insn_Reg, rd, rn, TCG_REG_TMP);
    }
}

/* SW instruction format of complex operator
 * insn = opcode[31,26]:rd[25,21]:rn[20,16],function[15,10]:rm[9,5]:rx[4,0]
 */
static void tcg_out_insn_complexReg(TCGContext *s, SW_64Insn insn, TCGReg cond, TCGReg rd, TCGReg rn, TCGReg rm)
{
    tcg_out32(s, insn | (cond & 0x1f) << 21 | (rn & 0x1f) << 16 | (rm & 0x1f) << 5 | (rd & 0x1f));
}

static bool reloc_pc21(tcg_insn_unit *src_rw, const tcg_insn_unit *target)
{
    const tcg_insn_unit *src_rx = tcg_splitwx_to_rx(src_rw);
    ptrdiff_t offset = target - (src_rx + 1) ;

    if (offset == sextract64(offset, 0, 21)) {
        /* read instruction, mask away previous PC_REL21 parameter contents,
           set the proper offset, then write back the instruction. */
        *src_rw = deposit32(*src_rw, 0, 21, offset);
        return true;
    }
    return false;
}

/* sw*/
static bool patch_reloc(tcg_insn_unit *code_ptr, int type, intptr_t value, intptr_t addend)
{
    tcg_debug_assert(addend == 0);
    switch (type) {
    case R_SW_64_BRADDR:
        return reloc_pc21(code_ptr, (const tcg_insn_unit *)value);
    default:
        g_assert_not_reached();
    }
}

static inline uint32_t tcg_in32(TCGContext *s)
{
    uint32_t v = *(uint32_t *)s->code_ptr;
    return v;
}

/*SW Register to register move using ADDL*/
static void tcg_out_movr(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn)
{
    tcg_out_insn_simpleReg(s, OPC_BIS, rd, rn, TCG_REG_ZERO);
    if (ext == TCG_TYPE_I32){
        tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, rd, rd, 0xf);
    }
}

/*sw
 *put imm into rd
 */
static void tcg_out_movi(TCGContext *s, TCGType type, TCGReg rd, tcg_target_long orig)
{
    long l0, l1, l2=0, l3=0, extra=0;
    tcg_target_long val = orig;
    TCGReg rs = TCG_REG_ZERO;

    if (type == TCG_TYPE_I32)
        val = (int32_t)val;

    l0 = (int16_t)val;
    val = (val - l0) >> 16;
    l1 = (int16_t)val;

    if (orig >> 31 == -1 || orig >> 31 == 0) {
        if (l1 < 0 && orig >= 0) {
            extra = 0x4000;
            l1 = (int16_t)(val - 0x4000);
        }
    } else {
        val = (val - l1) >> 16;
        l2 = (int16_t)val;
        val = (val - l2) >> 16;
        l3 = (int16_t)val;

        if (l3) {
            tcg_out_insn_ldst(s, OPC_LDIH, rd, rs, l3);
            rs = rd;
        }
        if (l2) {
            tcg_out_insn_ldst(s, OPC_LDI, rd, rs, l2);
            rs = rd;
        }
        if (l3 || l2)
            tcg_out_insn_simpleImm(s, OPC_SLL_I, rd, rd, 32);
    }

    if (l1) {
        tcg_out_insn_ldst(s, OPC_LDIH, rd, rs, l1);
        rs = rd;
    }
    
    if (extra) {
        tcg_out_insn_ldst(s, OPC_LDIH, rd, rs, extra);
        rs = rd;
    }

    tcg_out_insn_ldst(s, OPC_LDI, rd, rs, l0);
}


/*sw
* memory <=> Reg in (B H W L) bytes
*/
static void tcg_out_ldst(TCGContext *s, SW_64Insn insn, TCGReg rd, TCGReg rn, intptr_t offset, bool sign)
{
    int16_t lo = offset;
    if (offset != lo) {
        tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_TMP, offset - lo);
        if (rn != TCG_REG_ZERO) {
            tcg_out_insn_simpleReg(s, OPC_ADDL, TCG_REG_TMP, TCG_REG_TMP, rn);
        }
    tcg_out_insn_ldst(s, insn, rd, TCG_REG_TMP, lo);
    }
    else {
       tcg_out_insn_ldst(s, insn, rd, rn, lo);
    }

    switch (insn) {
        case OPC_LDBU:
             if (sign)
                 tcg_out_insn_simpleReg(s, OPC_SEXTB, rd, TCG_REG_ZERO, rd);  //for micro-op:INDEX_op_ld8s_i32/64,set rd[63,8]=1
             break;
        case OPC_LDHU:
             if (sign)
                 tcg_out_insn_simpleReg(s, OPC_SEXTH, rd, TCG_REG_ZERO, rd);  //for micro-op:INDEX_op_ld16s_i32/64,set rd[63,16]=1
             break;
        case OPC_LDW:
             if (!sign)
                 tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, rd, rd, 0xf);  //for micro-op:INDEX_op_ld32u_i32/64,set rd[63,32]=0
             break;
        default:
             break;
	}
}

/* TCG_REG_TMP stores result_of_condition_compare */
static void tcg_out_cond_cmp(TCGContext *s, TCGCond cond, TCGReg ret, TCGArg a, TCGArg b, bool const_b)
{
    if (const_b) {
        switch(cond) {
            case TCG_COND_ALWAYS:
	    case TCG_COND_NEVER:
	        break;
            case TCG_COND_EQ:
	    case TCG_COND_NE:
                tcg_out_insn_simple(s, OPC_CMPEQ_I, OPC_CMPEQ, ret, a, b);
                break;
            case TCG_COND_LT:
            case TCG_COND_GE:
                tcg_out_insn_simple(s, OPC_CMPLT_I, OPC_CMPLT, ret, a, b);
                break;
            case TCG_COND_LE:
            case TCG_COND_GT:
                tcg_out_insn_simple(s, OPC_CMPLE_I, OPC_CMPLE, ret, a, b);
                break;
            case TCG_COND_LTU:
            case TCG_COND_GEU:
                tcg_out_insn_simple(s, OPC_CMPULT_I, OPC_CMPULT, ret, a, b);
                break;
            case TCG_COND_LEU:
	    case TCG_COND_GTU:
                tcg_out_insn_simple(s, OPC_CMPULE_I, OPC_CMPULE, ret, a, b);
                break;
	    }//cond
	}//if (const_b)
    else {
	    switch(cond) {
	        case TCG_COND_ALWAYS:
	        case TCG_COND_NEVER:
                        break;
	        case TCG_COND_EQ:
	        case TCG_COND_NE:
	  	        tcg_out_insn_simpleReg(s, OPC_CMPEQ, ret, a, b);
		        break;
	        case TCG_COND_LT:
	        case TCG_COND_GE:
		        tcg_out_insn_simpleReg(s, OPC_CMPLT, ret, a, b);
		        break;
	        case TCG_COND_LE:
	        case TCG_COND_GT:
	 	        tcg_out_insn_simpleReg(s, OPC_CMPLE, ret, a, b);
	 	        break;
	        case TCG_COND_LTU:
	        case TCG_COND_GEU:
	  	        tcg_out_insn_simpleReg(s, OPC_CMPULT, ret, a, b);
		        break;
	        case TCG_COND_LEU:
	        case TCG_COND_GTU:
	 	        tcg_out_insn_simpleReg(s, OPC_CMPULE, ret, a, b);
		        break;
	    }//cond
    }//else
    switch(cond) {
        case TCG_COND_ALWAYS:
        case TCG_COND_NEVER:
        case TCG_COND_EQ:
        case TCG_COND_LT:
        case TCG_COND_LE:
        case TCG_COND_LTU:
        case TCG_COND_LEU:
            break;
	case TCG_COND_NE:
        case TCG_COND_GE:
        case TCG_COND_GT:
        case TCG_COND_GEU:
        case TCG_COND_GTU:
            tcg_out_insn_bitImm(s, OPC_XOR_I, ret, ret, 0x1);
	    break;
    }
}

/* sw
 * step1 tcg_out_cmp() ,"eq" and "ne" in the same case with the same insn;
 * store compare result by TCG_REG_TMP, for step2;
 * step2: jump address with compare result. in last "switch" section, we diff qe/ne by different case with different insn.
 */
static void tcg_out_brcond(TCGContext *s, TCGType ext, TCGCond cond, TCGArg a, TCGArg b, bool b_const, TCGLabel *l)
{
    intptr_t offset;
    bool need_cmp;

    if (b_const && b == 0 && (cond == TCG_COND_EQ || cond == TCG_COND_NE)) {
        need_cmp = false;
    } else {
        need_cmp = true;
        tcg_out_cond_cmp(s, cond, TCG_REG_TMP, a, b, b_const);
    }

    if (!l->has_value) {
        tcg_out_reloc(s, s->code_ptr, R_SW_64_BRADDR, l, 0);
	offset=0;   //offset = tcg_in32(s) >> 5; br $31, 0, do not jump here!
    } else {
        offset = tcg_pcrel_diff(s, l->u.value_ptr) ;
	offset = offset >> 2;
        tcg_debug_assert(offset == sextract64(offset, 0, 21));
    }

    if (need_cmp) {
        tcg_out_insn_br(s, OPC_BGT, TCG_REG_TMP, offset); //a cond b,jmp
    } else if (cond == TCG_COND_EQ) {
        tcg_out_insn_br(s, OPC_BEQ, a, offset);
    } else {
        tcg_out_insn_br(s, OPC_BNE, a, offset);
    }
}

/*sw
 * contact with "tcg-target-con-str.h"
 */
#define TCG_CT_CONST_ZERO 0x100
#define TCG_CT_CONST_LONG 0x200
#define TCG_CT_CONST_MONE 0x400
#define TCG_CT_CONST_ORRI 0x800
#define TCG_CT_CONST_WORD 0X1000
#define TCG_CT_CONST_U8 0x2000
#define TCG_CT_CONST_S8 0X4000

#define ALL_GENERAL_REGS  0xffffffffu
#define ALL_VECTOR_REGS   0xffffffff00000000ull


#ifdef CONFIG_SOFTMMU
/*sw #define ALL_QLDST_REGS */
#else
    #define ALL_QLDST_REGS   ALL_GENERAL_REGS
#endif

/* sw test if a constant matches the constraint */
static bool tcg_target_const_match(int64_t val, TCGType type, int ct)
{
    if (ct & TCG_CT_CONST) {
        return 1;
    }
    if (type == TCG_TYPE_I32) {
        val = (int32_t)val;
    }
    if ((ct & TCG_CT_CONST_U8) && 0 <= val && val <= 255) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_LONG)) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_MONE)) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_ORRI)) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_WORD)) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_ZERO) && val == 0) {
        return 1;
    }
    return 0;
}

static void tcg_out_ld(TCGContext *s, TCGType type, TCGReg rd, TCGReg rn, intptr_t ofs)
{
    switch (type) {
    case TCG_TYPE_I32:
        tcg_out_ldst(s, OPC_LDW, rd, rn, ofs, sigExt);
        break;
    case TCG_TYPE_I64:
        tcg_out_ldst(s, OPC_LDL, rd, rn, ofs, sigExt);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_st(TCGContext *s, TCGType type, TCGReg rd, TCGReg rn, intptr_t ofs)
{
    switch (type) {
    case TCG_TYPE_I32:
    	tcg_out_insn_ldst(s, OPC_STW, rd, rn, ofs);
        break;
    case TCG_TYPE_I64:
    	tcg_out_insn_ldst(s, OPC_STL, rd, rn, ofs);
        break;
    default:
        g_assert_not_reached();
    }
}

static inline bool tcg_out_sti(TCGContext *s, TCGType type, TCGArg val, TCGReg base, intptr_t ofs)
{
    if (type <= TCG_TYPE_I64 && val == 0) {
	tcg_out_st(s,  type, TCG_REG_ZERO, base, ofs);
        return true;
    }
    return false;
}

static void tcg_out_addsubi(TCGContext *s, int ext, TCGReg rd, TCGReg rn, int64_t imm64)
{
    if (imm64 >= 0) {
        if(0 <=imm64 && imm64 <= 255) {
	    /* we use tcg_out_insn_bitImm because imm64 is between 0~255 */
            tcg_out_insn_bitImm(s, OPC_ADDL_I, rd, rn, imm64);
        }//aimm>0  && aimm == sextract64(aim, 0, 8)
	else {
            tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_TMP, imm64);
            tcg_out_insn_simpleReg(s, OPC_ADDL, rd, rn, TCG_REG_TMP);
	}//aimm>0  && aimm != sextract64(aim, 0, 8)
    } else {
        if(0 < -imm64 && -imm64 <= 255) {
	    /* we use tcg_out_insn_bitImm because -imm64 is between 0~255 */
            tcg_out_insn_bitImm(s, OPC_SUBL_I, rd, rn, -imm64);
        }//aimm<0  && aimm == sextract64(aim, 0, 8)
	else {
            tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_TMP, -imm64);
            tcg_out_insn_simpleReg(s, OPC_SUBL, rd, rn, TCG_REG_TMP);
	    }//aimm<0  && aimm != sextract64(aim, 0, 8)
    }
}

static void tcg_out_goto(TCGContext *s, const tcg_insn_unit *target)
{
    ptrdiff_t offset = tcg_pcrel_diff(s, target) >> 2;
    tcg_debug_assert(offset == sextract64(offset, 0, 21));
    tcg_out_insn_br(s, OPC_BR, TCG_REG_ZERO, offset);
}

static void tcg_out_goto_long(TCGContext *s, const tcg_insn_unit *target)
{
    ptrdiff_t offset = tcg_pcrel_diff(s, target) >> 2;
    if (0 <= offset && offset <= 0x1fffff) {
        tcg_out_insn_br(s, OPC_BR, TCG_REG_ZERO, offset);
    } else {
        tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_TMP, (intptr_t)target);
        tcg_out_insn_jump(s, OPC_JMP, TCG_REG_ZERO, TCG_REG_TMP, 0);
    }
}


/*sw
* call subroutine
*/
static void tcg_out_call(TCGContext *s, const tcg_insn_unit *target)
{
    ptrdiff_t offset = tcg_pcrel_diff(s, target) >> 2;
    if (offset == sextract64(offset, 0, 21)) {
        tcg_out_insn_br(s, OPC_BSR, TCG_REG_RA, offset);
    } else {
        tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_TMP, (intptr_t)target);
        tcg_out_insn_jump(s, OPC_CALL, TCG_REG_RA, TCG_REG_TMP, 0);
    }
}

void tb_target_set_jmp_target(uintptr_t tc_ptr, uintptr_t jmp_rx, uintptr_t jmp_rw, uintptr_t addr)
{
    tcg_debug_assert(0);
    //sw not support
}

static inline void tcg_out_goto_label(TCGContext *s, TCGLabel *l)
{
    if (!l->has_value) {
        tcg_out_reloc(s, s->code_ptr, R_SW_64_BRADDR, l, 0);
        tcg_out_insn_br(s, OPC_BR, TCG_REG_ZERO, 0);
    } else {
        tcg_out_goto(s, l->u.value_ptr);
    }
}

/* sw  
 *  resut: rd=rn(64,64-m]:rm(64-m,0]
 * 1: rn(m,0]--->TCG_REG_TMP(64,64-m]
 * 2: rm(64,64-m]--->rm(64-m,0]
 * 3: rd=TCG_REG_TMP(64,64-m]:rm(64-m,0]
 */
static inline void tcg_out_extr(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, TCGReg rm, unsigned int m)
{
	int bits = ext ? 64 : 32;
	int max = bits - 1;
	tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_REG_TMP, rn, bits - (m & max));
	tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_REG_TMP2, rm, (m & max));
	tcg_out_insn_bitReg(s, OPC_BIS, rd, TCG_REG_TMP, TCG_REG_TMP2);
}

/* sw 
 * loop right shift
 */
static inline void tcg_out_rotr_Imm(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, unsigned int m)
{
    int bits = ext ? 64 : 32;
    int max = bits - 1;
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_REG_TMP, rn, bits - (m & max));
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_REG_TMP2, rn, (m & max));
    tcg_out_insn_bitReg(s, OPC_BIS, rd, TCG_REG_TMP, TCG_REG_TMP2);
}

/* sw loop right shift
 */
static inline void tcg_out_rotr_Reg(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, TCGReg rm)
{
    int bits = ext ? 64 : 32;
    //get TCG_REG_TMP=64-[rm]
    tcg_out_insn_simpleImm(s, OPC_SUBL_I, TCG_REG_TMP, rm, bits);
    tcg_out_insn_bitReg(s, OPC_ORNOT, TCG_REG_TMP, TCG_REG_ZERO, TCG_REG_TMP);

    tcg_out_insn_bitReg(s, OPC_SLL, TCG_REG_TMP2, rn, TCG_REG_TMP);  //get rn right part to TCG_REG_TMP
    tcg_out_insn_bitReg(s, OPC_SRL, TCG_REG_TMP, rn, rm);   //get rn left part to TCG_REG_TMP
    tcg_out_insn_bitReg(s, OPC_BIS, rd, TCG_REG_TMP, TCG_REG_TMP2);
}

/* sw 
 * loop left shift
 */
static inline void tcg_out_rotl_Imm(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, unsigned int m)
{
    int bits = ext ? 64 : 32;
    int max = bits - 1;

    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_REG_TMP, rn, bits -(m & max));
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_REG_TMP2, rn, (m & max));  //get rn left part to TCG_REG_TMP
    tcg_out_insn_bitReg(s, OPC_BIS, rd, TCG_REG_TMP, TCG_REG_TMP2);   //get rn right part to left
}


/* sw loop left shift
 */
static inline void tcg_out_rotl_Reg(TCGContext *s, TCGType ext, TCGReg rd, TCGReg rn, TCGReg rm)
{
    int bits = ext ? 64 : 32;
    tcg_out_insn_simpleImm(s, OPC_SUBL_I, TCG_REG_TMP, rm, bits); //rm = 64-rm
    tcg_out_insn_bitReg(s, OPC_ORNOT, TCG_REG_TMP, TCG_REG_ZERO, TCG_REG_TMP);

    tcg_out_insn_bitReg(s, OPC_SRL, TCG_REG_TMP2, rn, TCG_REG_TMP);  //get rn left part to TCG_REG_TMP
    tcg_out_insn_bitReg(s, OPC_SLL, TCG_REG_TMP, rn, rm);   //get rn right part to left
    tcg_out_insn_bitReg(s, OPC_BIS, rd, TCG_REG_TMP, TCG_REG_TMP2);
}



static void tcg_out_op(TCGContext *s, TCGOpcode opc, const TCGArg args[TCG_MAX_OP_ARGS], const int const_args[TCG_MAX_OP_ARGS])
{
    /* 99% of the time, we can signal the use of extension registers
       by looking to see if the opcode handles 64-bit data.  */
    TCGType ext = (tcg_op_defs[opc].flags & TCG_OPF_64BIT) != 0; 
    /* Hoist the loads of the most common arguments.  */
    TCGArg a0 = args[0];
    TCGArg a1 = args[1];
    TCGArg a2 = args[2];
    int c2 = const_args[2];

    /* Some operands are defined with "rZ" constraint, a register or
       the zero register.  These need not actually test args[I] == 0.  */
    #define REG0(I)  (const_args[I] ? TCG_REG_ZERO : (TCGReg)args[I])

    switch (opc) {
    case INDEX_op_exit_tb:
        /* Reuse the zeroing that exists for goto_ptr.  */
        if (a0 == 0) {
            tcg_out_goto_long(s, tcg_code_gen_epilogue);
        } else {
            tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_X0, a0);
            tcg_out_goto_long(s, tb_ret_addr);
        }
        break;

    case INDEX_op_goto_tb:
        if (s->tb_jmp_insn_offset != NULL) {
            /* TCG_TARGET_HAS_direct_jump */
            tcg_debug_assert(0);
            /* not support here */
        } else {
            /* !TCG_TARGET_HAS_direct_jump */
            tcg_debug_assert(s->tb_jmp_target_addr != NULL);
	    tcg_out_ld(s, TCG_TYPE_PTR, TCG_REG_TMP, TCG_REG_ZERO, (uintptr_t)(s->tb_jmp_target_addr + a0));
        }
        tcg_out_insn_jump(s, OPC_JMP, TCG_REG_ZERO, TCG_REG_TMP, 0);
        set_jmp_reset_offset(s, a0);
        break;

    case INDEX_op_goto_ptr:
        tcg_out_insn_jump(s, OPC_JMP, TCG_REG_ZERO, a0, 0);
        break;

    case INDEX_op_br:
        tcg_out_goto_label(s, arg_label(a0));
        break;

    case INDEX_op_ld8u_i32:
    case INDEX_op_ld8u_i64:
        tcg_out_ldst(s, OPC_LDBU, a0, a1, a2, 0);
        break;
    case INDEX_op_ld8s_i32:
    case INDEX_op_ld8s_i64:
        tcg_out_ldst(s, OPC_LDBU, a0, a1, a2, 1);
        break;
    case INDEX_op_ld16u_i32:
    case INDEX_op_ld16u_i64:
        tcg_out_ldst(s, OPC_LDHU, a0, a1, a2, 0);
        break;
    case INDEX_op_ld16s_i32:
    case INDEX_op_ld16s_i64:
        tcg_out_ldst(s, OPC_LDHU, a0, a1, a2, 1);
        break;
    case INDEX_op_ld_i32:
        tcg_out_ldst(s, OPC_LDW, a0, a1, a2, 1);
	break;
    case INDEX_op_ld32u_i64:
        tcg_out_ldst(s, OPC_LDW, a0, a1, a2, 0);
        break;
    case INDEX_op_ld32s_i64:
        tcg_out_ldst(s, OPC_LDW, a0, a1, a2, 1);
        break;
    case INDEX_op_ld_i64:
        tcg_out_ldst(s, OPC_LDL, a0, a1, a2, 1);
        break;
    case INDEX_op_st8_i32:
    case INDEX_op_st8_i64:
        tcg_out_ldst(s, OPC_STB, a0, a1, a2, 0);
        break;
    case INDEX_op_st16_i32:
    case INDEX_op_st16_i64:
        tcg_out_ldst(s, OPC_STH, a0, a1, a2, 0);
        break;
    case INDEX_op_st_i32:
    case INDEX_op_st32_i64:
        tcg_out_ldst(s, OPC_STW, a0, a1, a2, 0);
        break;
    case INDEX_op_st_i64:
        tcg_out_ldst(s, OPC_STL, a0, a1, a2, 0);
        break;

    case INDEX_op_add_i32:
        a2 = (int32_t)a2;
        if (c2) {
            tcg_out_addsubi(s, ext, a0, a1, a2);
        } else {
            tcg_out_insn_simpleReg(s, OPC_ADDL, a0, a1, a2);
        }
        break;
    case INDEX_op_add_i64:
        if (c2) {
            tcg_out_addsubi(s, ext, a0, a1, a2);
        } else {
            tcg_out_insn_simpleReg(s, OPC_ADDL, a0, a1, a2);
        }
        break;

    case INDEX_op_sub_i32:
        a2 = (int32_t)a2;
        if (c2) {
            tcg_out_addsubi(s, ext, a0, a1, -a2);
        } else {
            tcg_out_insn_simpleReg(s, OPC_SUBL, a0, a1, a2);
        }
        break;
    case INDEX_op_sub_i64:
        if (c2) {
            tcg_out_addsubi(s, ext, a0, a1, -a2);
        } else {
            tcg_out_insn_simpleReg(s, OPC_SUBL, a0, a1, a2);
        }
        break;

    case INDEX_op_neg_i64:
    case INDEX_op_neg_i32:
        tcg_out_insn_bitReg(s, OPC_SUBL, a0, TCG_REG_ZERO, a1);
        break;

    case INDEX_op_and_i32:
        a2 = (int32_t)a2;
        if (c2) {
            tcg_out_insn_bit(s, OPC_AND_I, OPC_AND, a0, a1, a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_AND, a0, a1, a2);
        }
        break;
    case INDEX_op_and_i64:
        if (c2) {
            tcg_out_insn_bit(s, OPC_AND_I, OPC_AND, a0, a1, a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_AND, a0, a1, a2);
        }
        break;
    case INDEX_op_andc_i32:
        a2 = (int32_t)a2;
        tcg_debug_assert(0);
        if (c2) {
            tcg_out_insn_bit(s, OPC_AND_I, OPC_AND, a0, a1, ~a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_BIC, a0, a1, a2);
        }
        break;
    case INDEX_op_andc_i64:
        tcg_debug_assert(0);
        if (c2) {
            tcg_out_insn_bit(s, OPC_AND_I, OPC_AND, a0, a1, ~a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_BIC, a0, a1, a2);
        }
        break;

    case INDEX_op_or_i32:
        a2 = (int32_t)a2;
        if (c2) {
            tcg_out_insn_bit(s, OPC_BIS_I, OPC_BIS, a0, a1, a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_BIS, a0, a1, a2);
        }
        break;
    case INDEX_op_or_i64:
        if (c2) {
            tcg_out_insn_bit(s, OPC_BIS_I, OPC_BIS, a0, a1, a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_BIS, a0, a1, a2);
        }
        break;

    case INDEX_op_orc_i32:
        a2 = (int32_t)a2;
        tcg_debug_assert(0);
        if (c2) {
            tcg_out_insn_bit(s, OPC_BIS_I, OPC_BIS, a0, a1, ~a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_ORNOT, a0, a1, a2);
        }
        break;
    case INDEX_op_orc_i64:
        tcg_debug_assert(0);
        if (c2) {
            tcg_out_insn_bit(s, OPC_BIS_I, OPC_BIS, a0, a1, ~a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_ORNOT, a0, a1, a2);
        }
        break;

    case INDEX_op_xor_i32:
        a2 = (int32_t)a2;
        if (c2) {
            tcg_out_insn_bit(s, OPC_XOR_I, OPC_XOR, a0, a1, a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_XOR, a0, a1, a2);
        }
        break;
    case INDEX_op_xor_i64:
        if (c2) {
            tcg_out_insn_bit(s, OPC_XOR_I, OPC_XOR, a0, a1, a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_XOR, a0, a1, a2);
        }
        break;

    case INDEX_op_eqv_i32:
        a2 = (int32_t)a2;
        tcg_debug_assert(0);
        if (c2) {
            tcg_out_insn_bit(s, OPC_XOR_I, OPC_XOR, a0, a1, ~a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_EQV, a0, a1, a2);
        }
        break;

    case INDEX_op_eqv_i64:
        tcg_debug_assert(0);
        if (c2) {
            tcg_out_insn_bit(s, OPC_XOR_I, OPC_XOR, a0, a1, ~a2);
        } else {
            tcg_out_insn_bitReg(s, OPC_EQV, a0, a1, a2);
        }
        break;

    case INDEX_op_not_i64:
    case INDEX_op_not_i32:
    	tcg_out_insn_bitReg(s, OPC_ORNOT, a0, TCG_REG_ZERO, a1);
        break;

    case INDEX_op_mul_i64:
    case INDEX_op_mul_i32:
    	tcg_out_insn_simpleReg(s, OPC_MULL, a0, a1, a2);
        break;

    case INDEX_op_div_i64: /* a0=a1/a2 singed divide*/
    case INDEX_op_div_i32:
        tcg_debug_assert(0);
        break;
    case INDEX_op_divu_i64: /* a0=a1/a2 unsigned divide */
    case INDEX_op_divu_i32:
        tcg_debug_assert(0);
        break;

    case INDEX_op_rem_i64:  /* if a1=17,a2=4, 17/4=4...1, a0=1 */
    case INDEX_op_rem_i32:
        tcg_debug_assert(0);
        break;
    case INDEX_op_remu_i64:
    case INDEX_op_remu_i32:
        tcg_debug_assert(0);
        break;

    case INDEX_op_shl_i64:
    case INDEX_op_shl_i32: /* sw logical left*/
        if (c2) {
            int bits = ext ? 64 : 32;
            int max = bits - 1;
            tcg_out_insn_bitImm(s, OPC_SLL_I, a0, a1, a2&max);
        } else {
            tcg_out_insn_bitReg(s, OPC_SLL, a0, a1, a2);
        }
        break;

    case INDEX_op_shr_i64:
    case INDEX_op_shr_i32: /* sw logical right */
        if (c2) {
            int bits = ext ? 64 : 32;
            int max = bits - 1;
            tcg_out_insn_bitImm(s, OPC_SRL_I, a0, a1, a2&max);
        } else {
            tcg_out_insn_bitReg(s, OPC_SRL, a0, a1, a2);
        }
        break;

    case INDEX_op_sar_i64:
    case INDEX_op_sar_i32: /* sw arithmetic right*/
        if (c2) {
            int bits = ext ? 64 : 32;
            int max = bits - 1;
            tcg_out_insn_bitImm(s, OPC_SRA_I, a0, a1, a2&max);
        } else {
            tcg_out_insn_bitReg(s, OPC_SRA, a0, a1, a2);
        }
        break;

    case INDEX_op_rotr_i64:
    case INDEX_op_rotr_i32: /* loop shift */
        if (c2) {/* loop right shift a2*/
            tcg_out_rotr_Imm(s, ext, a0, a1, a2);
        } else {
            tcg_out_rotr_Reg(s, ext, a0, a1, a2);
        }
        break;

    case INDEX_op_rotl_i64:
    case INDEX_op_rotl_i32: /* loop shift */
        if (c2) {/* loop left shift a2*/
            tcg_out_rotl_Imm(s, ext, a0, a1, a2);
        } else {
            tcg_out_rotl_Reg(s, ext, a0, a1, a2);
        }
        break;

    case INDEX_op_clz_i64: /* counting leading zero numbers */
    case INDEX_op_clz_i32:
        tcg_out_cltz(s, OPC_CTLZ, ext, a0, a1, a2, c2);
        break;
    case INDEX_op_ctz_i64: /* counting tailing zero numbers */
    case INDEX_op_ctz_i32:
        tcg_out_cltz(s, OPC_CTTZ, ext, a0, a1, a2, c2);
        break;

    case INDEX_op_brcond_i32:
        a1 = (int32_t)a1;
        tcg_out_brcond(s, ext, a2, a0, a1, const_args[1], arg_label(args[3]));
        break;

    case INDEX_op_brcond_i64:
        tcg_out_brcond(s, ext, a2, a0, a1, const_args[1], arg_label(args[3]));
        break;

    case INDEX_op_setcond_i32:
        a2 = (int32_t)a2;
        tcg_out_setcond(s, args[3], a0, a1, a2);
        break;

    case INDEX_op_setcond_i64:
        tcg_out_setcond(s, args[3], a0, a1, a2);
        break;

    case INDEX_op_movcond_i32:
        a2 = (int32_t)a2;
        tcg_out_movcond(s, args[5], a0, a1, a2, c2, REG0(3), REG0(4));
        break;

        /* FALLTHRU */
    case INDEX_op_movcond_i64:
        tcg_out_movcond(s, args[5], a0, a1, a2, c2, REG0(3), REG0(4));
        break;

    case INDEX_op_qemu_ld_i32:
    case INDEX_op_qemu_ld_i64:
        tcg_out_qemu_ld(s, a0, a1, a2, ext);
        break;
    case INDEX_op_qemu_st_i32:
    case INDEX_op_qemu_st_i64:
        tcg_out_qemu_st(s, REG0(0), a1, a2);
        break;

    case INDEX_op_bswap64_i64: /* 0x123456789abcdef--->0xefcdab8967452301 */
        tcg_debug_assert(0);
        tcg_out_bswap64(s, a0, a1);
        break;
    case INDEX_op_bswap32_i64:  /* 0x123456789abcdef--->0x67452301efcdab89 */
        tcg_debug_assert(0);
        tcg_out_bswap32u(s, a0, a1);
        break;
    case INDEX_op_bswap32_i32:
        tcg_debug_assert(0);
        break;
    case INDEX_op_bswap16_i64:  /* 0x123456789abcdef--->0x23016745ab89efcd */
    case INDEX_op_bswap16_i32:
        tcg_debug_assert(0);
        break;

    case INDEX_op_ext8s_i64:
    case INDEX_op_ext8s_i32:
	tcg_out_insn_simpleReg(s, OPC_SEXTB, a0, TCG_REG_ZERO, a1);
        break;
    case INDEX_op_ext16s_i64:
    case INDEX_op_ext16s_i32:
        tcg_out_insn_simpleReg(s, OPC_SEXTH, a0, TCG_REG_ZERO, a1);
        break;
    case INDEX_op_ext_i32_i64:
    case INDEX_op_ext32s_i64:
	tcg_out_insn_simpleReg(s, OPC_ADDW, a0, TCG_REG_ZERO, a1);
        break;
    case INDEX_op_ext8u_i64:
    case INDEX_op_ext8u_i32:
	tcg_out_insn_simpleImm(s, OPC_EXT0B_I, a0, a1, 0x0);
        break;
    case INDEX_op_ext16u_i64:
    case INDEX_op_ext16u_i32:
	tcg_out_insn_simpleImm(s, OPC_EXT1B_I, a0, a1, 0x0);
        break;
    case INDEX_op_extu_i32_i64:
    case INDEX_op_ext32u_i64:
        tcg_out_movr(s, TCG_TYPE_I32, a0, a1);
        break;

    case INDEX_op_deposit_i64:
    case INDEX_op_deposit_i32:
        tcg_out_dep(s, a0, a2, args[3], args[4]);
        break;

    case INDEX_op_extract_i64:
    case INDEX_op_extract_i32:
        tcg_out_extract(s, a0, a1, a2, args[3]);
        break;

    case INDEX_op_sextract_i64:
    case INDEX_op_sextract_i32:
        tcg_debug_assert(0);
        break;

    case INDEX_op_extract2_i64:
    case INDEX_op_extract2_i32: /* extract REG0(2) right args[3] bit to REG0(1) left ,save to a0*/
        tcg_debug_assert(0);
        break;

    case INDEX_op_add2_i32:
        tcg_debug_assert(0);
        break;
    case INDEX_op_add2_i64:
        tcg_debug_assert(0);
        break;
    case INDEX_op_sub2_i32:
        tcg_debug_assert(0);
        break;
    case INDEX_op_sub2_i64:
        tcg_debug_assert(0);
        break;

    case INDEX_op_muluh_i64:
        tcg_out_insn_simpleReg(s, OPC_UMULH, a0, a1, a2);
        break;
    case INDEX_op_mulsh_i64: /* sw not support */
        tcg_out_mulsh64(s, a0, a1, a2);
        break;

    case INDEX_op_mb:
        break;

    case INDEX_op_mov_i32:  /* Always emitted via tcg_out_mov.  */
    case INDEX_op_mov_i64:
    case INDEX_op_call:     /* Always emitted via tcg_out_call.  */
    default:
        g_assert_not_reached();
    }

#undef REG0
}



/*sw
* counting heading/tailing zero numbers
*/
static void tcg_out_cltz(TCGContext *s, SW_64Insn opc_clz, TCGType ext, TCGReg rd,
                         TCGReg rn, TCGArg b, bool const_b)
{
   /* cond1. b is a const, and b=64 or b=32 */
    if (const_b && b == (ext ? 64 : 32)) {
	/* count rn zero numbers, and writes to rd */
        tcg_out_insn_simpleReg(s, opc_clz, rd, TCG_REG_ZERO, rn);
    }else {
        /* TCG_REG_TMP= counting rn heading/tailing zero numbers */
        tcg_out_insn_simpleReg(s, opc_clz, TCG_REG_TMP, TCG_REG_ZERO, rn);
	
	if (const_b) {
	    if (b == -1) {
                /* cond2. b is const and b=-1 */
		/* if rn != 0 , rd= counting rn heading/tailing zero numbers, else rd = 0xffffffffffffffff*/
                tcg_out_insn_bitReg(s, OPC_ORNOT, TCG_REG_TMP2, TCG_REG_ZERO, TCG_REG_ZERO);
                tcg_out_insn_complexReg(s, OPC_SELNE, rn, rd, TCG_REG_TMP, TCG_REG_TMP2);
	    }
	    else if (b == 0) {
                /* cond3. b is const and b=0 */
               /* if rn != 0 , rd=counting rn heading/tailing zero numbers , else rd = TCG_REG_ZERO */
               tcg_out_insn_complexReg(s, OPC_SELNE, rn, rd, TCG_REG_TMP, TCG_REG_ZERO);
	    } else {
                /* cond4. b is const */
               tcg_out_movi(s, TCG_TYPE_I64, TCG_REG_TMP2, b);
               /* if rn != 0 , rd=counting rn heading/tailing zero numbers , else mov b to rd */
               tcg_out_insn_complexReg(s, OPC_SELNE, rn, rd, TCG_REG_TMP, TCG_REG_TMP2);
            }
	}
	else {
	    /* if b is register */
               tcg_out_insn_complexReg(s, OPC_SELNE, rn, rd, TCG_REG_TMP, b);
	}
    }
}
			 
/*sw
 * unsigned 16bit,  ab->ba
 */
static inline void tcg_out_bswap16u(TCGContext *s, TCGReg rd, TCGReg rn)
{
    TCGReg TCG_TMP0 = rn;
    TCGReg TCG_TMP1 = rd;
    /*t1=00b0*/
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP1, TCG_TMP0, 8);
    /*t1=(0000)000a*/
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP0, TCG_TMP0, 8);
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP0, TCG_TMP0, 0x1);
    /*t1=ooba*/
    tcg_out_insn_simpleReg(s, OPC_BIS, TCG_TMP1, TCG_TMP1, TCG_TMP0);
}

/*sw
 * signed 16bit,  ab->ssba
 */
static inline void tcg_out_bswap16s(TCGContext *s, TCGReg rd, TCGReg rn)
{
    TCGReg TCG_TMP0 = rn;
    TCGReg TCG_TMP1 = TCG_REG_TMP;
    TCGReg TCG_TMP2 = rn;
    /*t1=(ssss)ssb0*/
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP1, TCG_TMP0, 8);
    tcg_out_insn_simpleImm(s, OPC_ZAP_I, TCG_TMP1, TCG_TMP1, 0x2);
    tcg_out_insn_simpleReg(s, OPC_SEXTH, TCG_TMP1, TCG_REG_ZERO, TCG_TMP1);
    /*t2=(0000)000a*/
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP2, TCG_TMP0, 8);
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP2, TCG_TMP2, 0x1);
    /*t2=(ssss)ssba*/
    tcg_out_insn_simpleReg(s, OPC_BIS, TCG_TMP1, TCG_TMP1, TCG_TMP2);
}


/*sw
 * signed 32bit, abcd -> ssdcba
 */
static inline void tcg_out_bswap32s(TCGContext *s, TCGReg rd, TCGReg rn)
{   
    TCGReg TCG_TMP0 = rn;
    TCGReg TCG_TMP3 = rd;
    TCGReg TCG_TMP1 = TCG_REG_TMP;
    TCGReg TCG_TMP2 = TCG_REG_TMP2;
    /*swap32 -- 32-bit swap.  a0 = abcd.*/

    /* t3 = (ssss)d000 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP3, TCG_TMP0, 24);
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP3, TCG_TMP3, 0x0f);
    tcg_out_insn_simpleReg(s, OPC_SEXTB, TCG_TMP1, TCG_REG_ZERO, TCG_TMP0);
    tcg_out_insn_simpleImm(s, OPC_ZAP_I, TCG_TMP1, TCG_TMP1, 0x0f);
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);

    /* t1 = 000a */
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP1, TCG_TMP0, 24);
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP1, TCG_TMP1, 0x1);

    /* t2 = 00c0 */
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP2, TCG_TMP0, 0x2);

    /* t3 = (ssss)d00a */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);

    /* t1 = 0abc */
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP1, TCG_TMP0, 8);
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP1, TCG_TMP1, 0x7);

    /* t2 = 0c00 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP2, TCG_TMP2, 8);
    /* t1 = 00b0 */
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP1, TCG_TMP1, 0x2);
    /* t3 = (ssss)dc0a */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP2);
    /* t3 = (ssss)dcba -- delay slot */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);
}

/*sw
 * unsigned 32bit, abcd->dcba
 */
static void tcg_out_bswap32u(TCGContext *s, TCGReg rd, TCGReg rn)
{
    TCGReg TCG_TMP0 = rn;
    TCGReg TCG_TMP3 = rd;
    TCGReg TCG_TMP1 = TCG_REG_TMP;
    TCGReg TCG_TMP2 = TCG_REG_TMP2;

    /*bswap32u -- unsigned 32-bit swap.  a0 = ....abcd.*/
    /* t1 = (0000)000d */
    tcg_out_insn_bitImm(s, OPC_AND_I, TCG_TMP1, TCG_TMP0, 0xff);
    /* t3 = 000a */
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP3, TCG_TMP0, 24);
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP3, TCG_TMP3, 0x1);
    /* t1 = (0000)d000 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP1, TCG_TMP1, 24);
    /* t2 = 00c0 */
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP2, TCG_TMP0, 0x2);
    /* t3 = d00a */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);
    /* t1 = 0abc */
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP1, TCG_TMP0, 8);
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP1, TCG_TMP1, 0x7);
    /* t2 = 0c00 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP2, TCG_TMP2, 8);
    /* t1 = 00b0 */
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP1, TCG_TMP1, 0x2);
    /* t3 = dc0a */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP2);
    /* t3 = dcba -- delay slot */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);
}



/*sw
 * swap 64bit, abcdefgh->hgfedcba
 */
static void tcg_out_bswap64(TCGContext *s, TCGReg rd, TCGReg rn)
{
    
    TCGReg TCG_TMP0 = rn;
    TCGReg TCG_TMP3 = rd;
    TCGReg TCG_TMP1 = TCG_REG_TMP;
    TCGReg TCG_TMP2 = TCG_REG_TMP2;
	
     /* bswap64 -- 64-bit swap.  a0 = abcdefgh*/
	
    /* t3 = h0000000 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP3, TCG_TMP0, 56);
    /* t1 = 0000000a */
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP1, TCG_TMP0, 56);
    /* t2 = 000000g0 */
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP2, TCG_TMP0, 0x2);
    /* t3 = h000000a */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);
    /* t1 = 00000abc */
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP1, TCG_TMP0, 40);
    /* t2 = 0g000000 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP2, TCG_TMP2, 40);
    /* t1 = 000000b0 */
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP1, TCG_TMP1, 0x2);
    /* t3 = hg00000a */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP2);
    /* t2 = 0000abcd */
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP2, TCG_TMP0, 32);
    /* t3 = hg0000ba */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);
    /* t1 = 000000c0 */
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP1, TCG_TMP2, 0x2);
    /* t2 = 0000000d */
    tcg_out_insn_bitImm(s, OPC_AND_I, TCG_TMP2, TCG_TMP2, 0xff);
     /* t1 = 00000c00 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP1, TCG_TMP1, 8);
    /* t2 = 0000d000 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP2, TCG_TMP2, 24);
    /* t3 = hg000cba */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);
    /* t1 = 00abcdef */
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_TMP1, TCG_TMP0, 16);
    /* t3 = hg00dcba */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP2);
    /* t2 = 0000000f */
    tcg_out_insn_bitImm(s, OPC_AND_I, TCG_TMP2, TCG_TMP1, 0xff);
    /* t1 = 000000e0 */
    tcg_out_insn_simpleImm(s, OPC_ZAPNOT_I, TCG_TMP1, TCG_TMP1, 0x2);
    /* t2 = 00f00000 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP2, TCG_TMP2, 40);
    /* t1 = 000e0000 */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_TMP1, TCG_TMP1, 24);
    /* t3 = hgf0dcba */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP2);
    /* t3 = hgfedcba -- delay slot */
    tcg_out_insn_bitReg(s, OPC_BIS, TCG_TMP3, TCG_TMP3, TCG_TMP1);

}

static void tcg_out_qemu_ld(TCGContext *s, TCGReg data_reg, TCGReg addr_reg, MemOpIdx oi, TCGType ext)
{
#ifndef CONFIG_SOFTMMU
    MemOp memop = get_memop(oi);
    const TCGType otype = TCG_TYPE_I64;

    if (USE_GUEST_BASE) {
	tcg_out_insn_simpleReg(s, OPC_ADDL, TCG_REG_GUEST_BASE, TCG_REG_GUEST_BASE, addr_reg);
        tcg_out_qemu_ld_direct(s, memop, data_reg, TCG_REG_GUEST_BASE, otype, 0);
    } else {
        tcg_out_qemu_ld_direct(s, memop, data_reg, addr_reg, TCG_TYPE_I64, 0);
    }
#endif /* CONFIG_SOFTMMU */

}

static void tcg_out_qemu_st(TCGContext *s, TCGReg data_reg, TCGReg addr_reg,
                            MemOpIdx oi)
{
#ifndef CONFIG_SOFTMMU
    MemOp memop = get_memop(oi);
    const TCGType otype = TCG_TYPE_I64;

    if (USE_GUEST_BASE) {
	tcg_out_insn_simpleReg(s, OPC_ADDL, TCG_REG_GUEST_BASE, TCG_REG_GUEST_BASE, addr_reg);
        tcg_out_qemu_st_direct(s, memop, data_reg, TCG_REG_GUEST_BASE, otype, 0);
    } else {
        tcg_out_qemu_st_direct(s, memop, data_reg, addr_reg, TCG_TYPE_I64, 0);
    }
#endif /* CONFIG_SOFTMMU */
}


/*sw
 * if cond is successful, ret=1, otherwise ret = 0
 */
static void tcg_out_setcond(TCGContext *s, TCGCond cond, TCGReg ret,
                            TCGReg arg1, TCGReg arg2)
{
    switch(cond) {
        case TCG_COND_EQ:
        case TCG_COND_LT:
        case TCG_COND_LE:
        case TCG_COND_LTU:
        case TCG_COND_LEU:
	case TCG_COND_NE:
        case TCG_COND_GE:
        case TCG_COND_GT:
        case TCG_COND_GEU:
        case TCG_COND_GTU:
            tcg_out_cond_cmp(s, cond, ret, arg1, arg2, 0);
            break;
	default:
            tcg_abort();
            break;
     }
}
/*sw
 * cond(a1,a2), yes:v1->ret, no:v2->ret
 */
static void tcg_out_movcond(TCGContext *s, TCGCond cond, TCGReg ret,
                            TCGReg a1, TCGReg a2, bool const_b, TCGReg v1, TCGReg v2)
{
    tcg_out_cond_cmp(s, cond, TCG_REG_TMP, a1, a2, const_b);
    tcg_out_insn_complexReg(s, OPC_SELLBS, TCG_REG_TMP, ret, v1, v2);
}



/*sw
 * extract rn[lsb, lsb+len-1] ->  rd[0, len-1]
 */
static void tcg_out_extract(TCGContext *s, TCGReg rd, TCGReg rn, int lsb, int len)
{
    //get 000..111..0000
    tcg_out_insn_bitReg(s, OPC_ORNOT, TCG_REG_TMP, TCG_REG_ZERO, TCG_REG_ZERO);
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_REG_TMP, TCG_REG_TMP, 64 - len);
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_REG_TMP, TCG_REG_TMP,  lsb);
    /* get rn[lsb, lsb+len-1]-->rd[lsb, lsb+len-1] */
    tcg_out_insn_bitReg(s, OPC_AND, rd, rn, TCG_REG_TMP);

    /* rd[lsb, lsb+len-1] --> rd[0, len-1] */
    tcg_out_insn_bitImm(s, OPC_SRL_I, rd, rd, lsb);
}


/*sw
 * depos: rd = rd[63:msb+1]:rn[msb,lsb]:rd[lsb-1,0]
 * len = msb -lsb + 1
 */
static void tcg_out_dep(TCGContext *s,  TCGReg rd, TCGReg rn, int lsb, int len)
{

    //get 000..111..0000
    tcg_out_insn_bitReg(s, OPC_ORNOT, TCG_REG_TMP, TCG_REG_ZERO, TCG_REG_ZERO);
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_REG_TMP, TCG_REG_TMP, 64 - len);
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_REG_TMP, TCG_REG_TMP, lsb);
    
    /* TCG_REG_TMP2 = rn[msb,lsb] */
    tcg_out_insn_bitImm(s, OPC_SLL_I, TCG_REG_TMP2, rn, 64-len);
    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_REG_TMP2, TCG_REG_TMP2, 64-len-lsb);

    /* clear rd[msb,lsb] */
    tcg_out_insn_bitReg(s, OPC_BIC, rd, rd, TCG_REG_TMP);
    /* rd = rd[63:msb+1]:rn[msb,lsb]:rd[lsb-1,0] */
    tcg_out_insn_bitReg(s, OPC_BIS, rd, rd, TCG_REG_TMP2);
}

/*sw
 * get val_s64(rn) * val_s64(rm) -> res_128
 * res[127:64] -> rd
 * warn:maybe rd=rn or rm
 */
static void tcg_out_mulsh64(TCGContext *s,  TCGReg rd, TCGReg rn, TCGReg rm)
{
    tcg_out_insn_simpleReg(s, OPC_UMULH, TCG_REG_TMP, rn, rm);

    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_REG_TMP2,  rn, 63);
    tcg_out_insn_complexReg(s, OPC_SELEQ, TCG_REG_TMP2, TCG_REG_TMP2, TCG_REG_ZERO, rm);
    tcg_out_insn_simpleReg(s, OPC_SUBL, TCG_REG_TMP, TCG_REG_TMP, TCG_REG_TMP2);

    tcg_out_insn_bitImm(s, OPC_SRL_I, TCG_REG_TMP2, rm, 63);
    tcg_out_insn_complexReg(s, OPC_SELEQ, TCG_REG_TMP2, TCG_REG_TMP2, TCG_REG_ZERO, rn);
    tcg_out_insn_simpleReg(s, OPC_SUBL, rd, TCG_REG_TMP, TCG_REG_TMP2);
}

typedef struct {
    DebugFrameHeader h;
    uint8_t fde_def_cfa[4];
    uint8_t fde_reg_ofs[8 * 2];
} DebugFrame;

#define ELF_HOST_MACHINE EM_SW_64
/* GDB doesn't appear to require proper setting of ELF_HOST_FLAGS,
   which is good because they're really quite complicated for SW_64.  */

static const DebugFrame debug_frame = {
    .h.cie.len = sizeof(DebugFrameCIE) - 4, /* length after .len member */
    .h.cie.id = -1,
    .h.cie.version = 1,
    .h.cie.code_align = 1,
    .h.cie.data_align = -(TCG_TARGET_REG_BITS / 8) & 0x7f, /* sleb128 */
    .h.cie.return_column = TCG_REG_RA,

    /* Total FDE size does not include the "len" member.  */
    .h.fde.len = sizeof(DebugFrame) - offsetof(DebugFrame, h.fde.cie_offset),

    .fde_def_cfa = {
        12, TCG_REG_SP,                 /* DW_CFA_def_cfa sp, ... */
        (FRAME_SIZE & 0x7f) | 0x80,     /* ... uleb128 FRAME_SIZE */
        (FRAME_SIZE >> 7)
    },
    .fde_reg_ofs = {
        0x80 + 14, 1,                   /* DW_CFA_offset,  */
        0x80 + 13, 2,                   /* DW_CFA_offset,  */
        0x80 + 12, 3,                   /* DW_CFA_offset,  */
        0x80 + 11, 4,                   /* DW_CFA_offset,  */
        0x80 + 10, 5,                   /* DW_CFA_offset,  */
        0x80 +  9, 6,                   /* DW_CFA_offset,  */
        0x80 + 26, 7,                   /* DW_CFA_offset, ra, -24 */
        0x80 + 15, 8,                   /* DW_CFA_offset, fp,  -8 */
    }
};

void tcg_register_jit(const void *buf, size_t buf_size)
{
    tcg_register_jit_int(buf, buf_size, &debug_frame, sizeof(debug_frame));
}
