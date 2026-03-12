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

#ifndef UB_COMMON_H
#define UB_COMMON_H

#include "hw/ub/ub.h"
#include "hw/ub/ub_ubc.h"
#include "hw/ub/ub_bus.h"

/* You can use the following macro to execute a
 * repeated snippet of code
 */
#define CMD(macro, arg) macro(arg)
#define LOOP0(macro) CMD(macro, 0)
#define LOOP1(macro) LOOP0(macro)  CMD(macro, 1)
#define LOOP2(macro) LOOP1(macro)  CMD(macro, 2)
#define LOOP3(macro) LOOP2(macro)  CMD(macro, 3)
#define LOOP4(macro) LOOP3(macro)  CMD(macro, 4)
#define LOOP5(macro) LOOP4(macro)  CMD(macro, 5)
#define LOOP6(macro) LOOP5(macro)  CMD(macro, 6)
#define LOOP7(macro) LOOP6(macro)  CMD(macro, 7)
#define LOOP8(macro) LOOP7(macro)  CMD(macro, 8)
#define LOOP9(macro) LOOP8(macro)  CMD(macro, 9)
#define LOOP10(macro) LOOP9(macro)  CMD(macro, 10)
#define LOOP11(macro) LOOP10(macro)  CMD(macro, 11)
#define LOOP12(macro) LOOP11(macro)  CMD(macro, 12)
#define LOOP13(macro) LOOP12(macro)  CMD(macro, 13)
#define LOOP14(macro) LOOP13(macro)  CMD(macro, 14)
#define LOOP15(macro) LOOP14(macro)  CMD(macro, 15)
#define LOOP16(macro) LOOP15(macro)  CMD(macro, 16)
#define LOOP17(macro) LOOP16(macro)  CMD(macro, 17)
#define LOOP18(macro) LOOP17(macro)  CMD(macro, 18)
#define LOOP19(macro) LOOP18(macro)  CMD(macro, 19)
#define LOOP20(macro) LOOP19(macro)  CMD(macro, 20)
#define LOOP21(macro) LOOP20(macro)  CMD(macro, 21)
#define LOOP22(macro) LOOP21(macro)  CMD(macro, 22)
#define LOOP23(macro) LOOP22(macro)  CMD(macro, 23)
#define LOOP24(macro) LOOP23(macro)  CMD(macro, 24)
#define LOOP25(macro) LOOP24(macro)  CMD(macro, 25)
#define LOOP26(macro) LOOP25(macro)  CMD(macro, 26)
#define LOOP27(macro) LOOP26(macro)  CMD(macro, 27)
#define LOOP28(macro) LOOP27(macro)  CMD(macro, 28)
#define LOOP29(macro) LOOP28(macro)  CMD(macro, 29)
#define LOOP30(macro) LOOP29(macro)  CMD(macro, 30)
#define LOOP31(macro) LOOP30(macro)  CMD(macro, 31)
#define LOOP32(macro) LOOP31(macro)  CMD(macro, 32)
#define LOOP33(macro) LOOP32(macro)  CMD(macro, 33)
#define LOOP34(macro) LOOP33(macro)  CMD(macro, 34)
#define LOOP35(macro) LOOP34(macro)  CMD(macro, 35)
#define LOOP36(macro) LOOP35(macro)  CMD(macro, 36)
#define LOOP37(macro) LOOP36(macro)  CMD(macro, 37)
#define LOOP38(macro) LOOP37(macro)  CMD(macro, 38)
#define LOOP39(macro) LOOP38(macro)  CMD(macro, 39)
#define LOOP40(macro) LOOP39(macro)  CMD(macro, 40)
#define LOOP41(macro) LOOP40(macro)  CMD(macro, 41)
#define LOOP42(macro) LOOP41(macro)  CMD(macro, 42)
#define LOOP43(macro) LOOP42(macro)  CMD(macro, 43)
#define LOOP44(macro) LOOP43(macro)  CMD(macro, 44)
#define LOOP45(macro) LOOP44(macro)  CMD(macro, 45)
#define LOOP46(macro) LOOP45(macro)  CMD(macro, 46)
#define LOOP47(macro) LOOP46(macro)  CMD(macro, 47)
#define LOOP48(macro) LOOP47(macro)  CMD(macro, 48)
#define LOOP49(macro) LOOP48(macro)  CMD(macro, 49)
#define LOOP50(macro) LOOP49(macro)  CMD(macro, 50)
#define LOOP51(macro) LOOP50(macro)  CMD(macro, 51)
#define LOOP52(macro) LOOP51(macro)  CMD(macro, 52)
#define LOOP53(macro) LOOP52(macro)  CMD(macro, 53)
#define LOOP54(macro) LOOP53(macro)  CMD(macro, 54)
#define LOOP55(macro) LOOP54(macro)  CMD(macro, 55)
#define LOOP56(macro) LOOP55(macro)  CMD(macro, 56)
#define LOOP57(macro) LOOP56(macro)  CMD(macro, 57)
#define LOOP58(macro) LOOP57(macro)  CMD(macro, 58)
#define LOOP59(macro) LOOP58(macro)  CMD(macro, 59)
#define LOOP60(macro) LOOP59(macro)  CMD(macro, 60)
#define LOOP61(macro) LOOP60(macro)  CMD(macro, 61)
#define LOOP62(macro) LOOP61(macro)  CMD(macro, 62)
#define LOOP63(macro) LOOP62(macro)  CMD(macro, 63)
#define LOOP64(macro) LOOP63(macro)  CMD(macro, 64)
#define LOOP65(macro) LOOP64(macro)  CMD(macro, 65)
#define LOOP66(macro) LOOP65(macro)  CMD(macro, 66)
#define LOOP67(macro) LOOP66(macro)  CMD(macro, 67)
#define LOOP68(macro) LOOP67(macro)  CMD(macro, 68)
#define LOOP69(macro) LOOP68(macro)  CMD(macro, 69)
#define LOOP70(macro) LOOP69(macro)  CMD(macro, 70)
#define LOOP71(macro) LOOP70(macro)  CMD(macro, 71)
#define LOOP72(macro) LOOP71(macro)  CMD(macro, 72)
#define LOOP73(macro) LOOP72(macro)  CMD(macro, 73)
#define LOOP74(macro) LOOP73(macro)  CMD(macro, 74)
#define LOOP75(macro) LOOP74(macro)  CMD(macro, 75)
#define LOOP76(macro) LOOP75(macro)  CMD(macro, 76)
#define LOOP77(macro) LOOP76(macro)  CMD(macro, 77)
#define LOOP78(macro) LOOP77(macro)  CMD(macro, 78)
#define LOOP79(macro) LOOP78(macro)  CMD(macro, 79)
#define LOOP80(macro) LOOP79(macro)  CMD(macro, 80)
#define LOOP81(macro) LOOP80(macro)  CMD(macro, 81)
#define LOOP82(macro) LOOP81(macro)  CMD(macro, 82)
#define LOOP83(macro) LOOP82(macro)  CMD(macro, 83)
#define LOOP84(macro) LOOP83(macro)  CMD(macro, 84)
#define LOOP85(macro) LOOP84(macro)  CMD(macro, 85)
#define LOOP86(macro) LOOP85(macro)  CMD(macro, 86)
#define LOOP87(macro) LOOP86(macro)  CMD(macro, 87)
#define LOOP88(macro) LOOP87(macro)  CMD(macro, 88)
#define LOOP89(macro) LOOP88(macro)  CMD(macro, 89)
#define LOOP90(macro) LOOP89(macro)  CMD(macro, 90)
#define LOOP91(macro) LOOP90(macro)  CMD(macro, 91)
#define LOOP92(macro) LOOP91(macro)  CMD(macro, 92)
#define LOOP93(macro) LOOP92(macro)  CMD(macro, 93)
#define LOOP94(macro) LOOP93(macro)  CMD(macro, 94)
#define LOOP95(macro) LOOP94(macro)  CMD(macro, 95)
#define LOOP96(macro) LOOP95(macro)  CMD(macro, 96)
#define LOOP97(macro) LOOP96(macro)  CMD(macro, 97)
#define LOOP98(macro) LOOP97(macro)  CMD(macro, 98)
#define LOOP99(macro) LOOP98(macro)  CMD(macro, 99)
#define LOOP100(macro) LOOP99(macro)  CMD(macro, 100)
#define LOOP101(macro) LOOP100(macro)  CMD(macro, 101)
#define LOOP102(macro) LOOP101(macro)  CMD(macro, 102)
#define LOOP103(macro) LOOP102(macro)  CMD(macro, 103)
#define LOOP104(macro) LOOP103(macro)  CMD(macro, 104)
#define LOOP105(macro) LOOP104(macro)  CMD(macro, 105)
#define LOOP106(macro) LOOP105(macro)  CMD(macro, 106)
#define LOOP107(macro) LOOP106(macro)  CMD(macro, 107)
#define LOOP108(macro) LOOP107(macro)  CMD(macro, 108)
#define LOOP109(macro) LOOP108(macro)  CMD(macro, 109)
#define LOOP110(macro) LOOP109(macro)  CMD(macro, 110)
#define LOOP111(macro) LOOP110(macro)  CMD(macro, 111)
#define LOOP112(macro) LOOP111(macro)  CMD(macro, 112)
#define LOOP113(macro) LOOP112(macro)  CMD(macro, 113)
#define LOOP114(macro) LOOP113(macro)  CMD(macro, 114)
#define LOOP115(macro) LOOP114(macro)  CMD(macro, 115)
#define LOOP116(macro) LOOP115(macro)  CMD(macro, 116)
#define LOOP117(macro) LOOP116(macro)  CMD(macro, 117)
#define LOOP118(macro) LOOP117(macro)  CMD(macro, 118)
#define LOOP119(macro) LOOP118(macro)  CMD(macro, 119)
#define LOOP120(macro) LOOP119(macro)  CMD(macro, 120)
#define LOOP121(macro) LOOP120(macro)  CMD(macro, 121)
#define LOOP122(macro) LOOP121(macro)  CMD(macro, 122)
#define LOOP123(macro) LOOP122(macro)  CMD(macro, 123)
#define LOOP124(macro) LOOP123(macro)  CMD(macro, 124)
#define LOOP125(macro) LOOP124(macro)  CMD(macro, 125)
#define LOOP126(macro) LOOP125(macro)  CMD(macro, 126)
#define LOOP127(macro) LOOP126(macro)  CMD(macro, 127)
#define LOOP128(macro) LOOP127(macro)  CMD(macro, 128)
#define LOOP129(macro) LOOP128(macro)  CMD(macro, 129)
#define LOOP130(macro) LOOP129(macro)  CMD(macro, 130)
#define LOOP131(macro) LOOP130(macro)  CMD(macro, 131)
#define LOOP132(macro) LOOP131(macro)  CMD(macro, 132)
#define LOOP133(macro) LOOP132(macro)  CMD(macro, 133)
#define LOOP134(macro) LOOP133(macro)  CMD(macro, 134)
#define LOOP135(macro) LOOP134(macro)  CMD(macro, 135)
#define LOOP136(macro) LOOP135(macro)  CMD(macro, 136)
#define LOOP137(macro) LOOP136(macro)  CMD(macro, 137)
#define LOOP138(macro) LOOP137(macro)  CMD(macro, 138)
#define LOOP139(macro) LOOP138(macro)  CMD(macro, 139)
#define LOOP140(macro) LOOP139(macro)  CMD(macro, 140)
#define LOOP141(macro) LOOP140(macro)  CMD(macro, 141)
#define LOOP142(macro) LOOP141(macro)  CMD(macro, 142)
#define LOOP143(macro) LOOP142(macro)  CMD(macro, 143)
#define LOOP144(macro) LOOP143(macro)  CMD(macro, 144)
#define LOOP145(macro) LOOP144(macro)  CMD(macro, 145)
#define LOOP146(macro) LOOP145(macro)  CMD(macro, 146)
#define LOOP147(macro) LOOP146(macro)  CMD(macro, 147)
#define LOOP148(macro) LOOP147(macro)  CMD(macro, 148)
#define LOOP149(macro) LOOP148(macro)  CMD(macro, 149)
#define LOOP150(macro) LOOP149(macro)  CMD(macro, 150)
#define LOOP151(macro) LOOP150(macro)  CMD(macro, 151)
#define LOOP152(macro) LOOP151(macro)  CMD(macro, 152)
#define LOOP153(macro) LOOP152(macro)  CMD(macro, 153)
#define LOOP154(macro) LOOP153(macro)  CMD(macro, 154)
#define LOOP155(macro) LOOP154(macro)  CMD(macro, 155)
#define LOOP156(macro) LOOP155(macro)  CMD(macro, 156)
#define LOOP157(macro) LOOP156(macro)  CMD(macro, 157)
#define LOOP158(macro) LOOP157(macro)  CMD(macro, 158)
#define LOOP159(macro) LOOP158(macro)  CMD(macro, 159)
#define LOOP160(macro) LOOP159(macro)  CMD(macro, 160)
#define LOOP161(macro) LOOP160(macro)  CMD(macro, 161)
#define LOOP162(macro) LOOP161(macro)  CMD(macro, 162)
#define LOOP163(macro) LOOP162(macro)  CMD(macro, 163)
#define LOOP164(macro) LOOP163(macro)  CMD(macro, 164)
#define LOOP165(macro) LOOP164(macro)  CMD(macro, 165)
#define LOOP166(macro) LOOP165(macro)  CMD(macro, 166)
#define LOOP167(macro) LOOP166(macro)  CMD(macro, 167)
#define LOOP168(macro) LOOP167(macro)  CMD(macro, 168)
#define LOOP169(macro) LOOP168(macro)  CMD(macro, 169)
#define LOOP170(macro) LOOP169(macro)  CMD(macro, 170)
#define LOOP171(macro) LOOP170(macro)  CMD(macro, 171)
#define LOOP172(macro) LOOP171(macro)  CMD(macro, 172)
#define LOOP173(macro) LOOP172(macro)  CMD(macro, 173)
#define LOOP174(macro) LOOP173(macro)  CMD(macro, 174)
#define LOOP175(macro) LOOP174(macro)  CMD(macro, 175)
#define LOOP176(macro) LOOP175(macro)  CMD(macro, 176)
#define LOOP177(macro) LOOP176(macro)  CMD(macro, 177)
#define LOOP178(macro) LOOP177(macro)  CMD(macro, 178)
#define LOOP179(macro) LOOP178(macro)  CMD(macro, 179)
#define LOOP180(macro) LOOP179(macro)  CMD(macro, 180)
#define LOOP181(macro) LOOP180(macro)  CMD(macro, 181)
#define LOOP182(macro) LOOP181(macro)  CMD(macro, 182)
#define LOOP183(macro) LOOP182(macro)  CMD(macro, 183)
#define LOOP184(macro) LOOP183(macro)  CMD(macro, 184)
#define LOOP185(macro) LOOP184(macro)  CMD(macro, 185)
#define LOOP186(macro) LOOP185(macro)  CMD(macro, 186)
#define LOOP187(macro) LOOP186(macro)  CMD(macro, 187)
#define LOOP188(macro) LOOP187(macro)  CMD(macro, 188)
#define LOOP189(macro) LOOP188(macro)  CMD(macro, 189)
#define LOOP190(macro) LOOP189(macro)  CMD(macro, 190)
#define LOOP191(macro) LOOP190(macro)  CMD(macro, 191)
#define LOOP192(macro) LOOP191(macro)  CMD(macro, 192)
#define LOOP193(macro) LOOP192(macro)  CMD(macro, 193)
#define LOOP194(macro) LOOP193(macro)  CMD(macro, 194)
#define LOOP195(macro) LOOP194(macro)  CMD(macro, 195)
#define LOOP196(macro) LOOP195(macro)  CMD(macro, 196)
#define LOOP197(macro) LOOP196(macro)  CMD(macro, 197)
#define LOOP198(macro) LOOP197(macro)  CMD(macro, 198)
#define LOOP199(macro) LOOP198(macro)  CMD(macro, 199)
#define LOOP200(macro) LOOP199(macro)  CMD(macro, 200)
#define LOOP201(macro) LOOP200(macro)  CMD(macro, 201)
#define LOOP202(macro) LOOP201(macro)  CMD(macro, 202)
#define LOOP203(macro) LOOP202(macro)  CMD(macro, 203)
#define LOOP204(macro) LOOP203(macro)  CMD(macro, 204)
#define LOOP205(macro) LOOP204(macro)  CMD(macro, 205)
#define LOOP206(macro) LOOP205(macro)  CMD(macro, 206)
#define LOOP207(macro) LOOP206(macro)  CMD(macro, 207)
#define LOOP208(macro) LOOP207(macro)  CMD(macro, 208)
#define LOOP209(macro) LOOP208(macro)  CMD(macro, 209)
#define LOOP210(macro) LOOP209(macro)  CMD(macro, 210)
#define LOOP211(macro) LOOP210(macro)  CMD(macro, 211)
#define LOOP212(macro) LOOP211(macro)  CMD(macro, 212)
#define LOOP213(macro) LOOP212(macro)  CMD(macro, 213)
#define LOOP214(macro) LOOP213(macro)  CMD(macro, 214)
#define LOOP215(macro) LOOP214(macro)  CMD(macro, 215)
#define LOOP216(macro) LOOP215(macro)  CMD(macro, 216)
#define LOOP217(macro) LOOP216(macro)  CMD(macro, 217)
#define LOOP218(macro) LOOP217(macro)  CMD(macro, 218)
#define LOOP219(macro) LOOP218(macro)  CMD(macro, 219)
#define LOOP220(macro) LOOP219(macro)  CMD(macro, 220)
#define LOOP221(macro) LOOP220(macro)  CMD(macro, 221)
#define LOOP222(macro) LOOP221(macro)  CMD(macro, 222)
#define LOOP223(macro) LOOP222(macro)  CMD(macro, 223)
#define LOOP224(macro) LOOP223(macro)  CMD(macro, 224)
#define LOOP225(macro) LOOP224(macro)  CMD(macro, 225)
#define LOOP226(macro) LOOP225(macro)  CMD(macro, 226)
#define LOOP227(macro) LOOP226(macro)  CMD(macro, 227)
#define LOOP228(macro) LOOP227(macro)  CMD(macro, 228)
#define LOOP229(macro) LOOP228(macro)  CMD(macro, 229)
#define LOOP230(macro) LOOP229(macro)  CMD(macro, 230)
#define LOOP231(macro) LOOP230(macro)  CMD(macro, 231)
#define LOOP232(macro) LOOP231(macro)  CMD(macro, 232)
#define LOOP233(macro) LOOP232(macro)  CMD(macro, 233)
#define LOOP234(macro) LOOP233(macro)  CMD(macro, 234)
#define LOOP235(macro) LOOP234(macro)  CMD(macro, 235)
#define LOOP236(macro) LOOP235(macro)  CMD(macro, 236)
#define LOOP237(macro) LOOP236(macro)  CMD(macro, 237)
#define LOOP238(macro) LOOP237(macro)  CMD(macro, 238)
#define LOOP239(macro) LOOP238(macro)  CMD(macro, 239)
#define LOOP240(macro) LOOP239(macro)  CMD(macro, 240)
#define LOOP241(macro) LOOP240(macro)  CMD(macro, 241)
#define LOOP242(macro) LOOP241(macro)  CMD(macro, 242)
#define LOOP243(macro) LOOP242(macro)  CMD(macro, 243)
#define LOOP244(macro) LOOP243(macro)  CMD(macro, 244)
#define LOOP245(macro) LOOP244(macro)  CMD(macro, 245)
#define LOOP246(macro) LOOP245(macro)  CMD(macro, 246)
#define LOOP247(macro) LOOP246(macro)  CMD(macro, 247)
#define LOOP248(macro) LOOP247(macro)  CMD(macro, 248)
#define LOOP249(macro) LOOP248(macro)  CMD(macro, 249)
#define LOOP250(macro) LOOP249(macro)  CMD(macro, 250)
#define LOOP251(macro) LOOP250(macro)  CMD(macro, 251)
#define LOOP252(macro) LOOP251(macro)  CMD(macro, 252)
#define LOOP253(macro) LOOP252(macro)  CMD(macro, 253)
#define LOOP254(macro) LOOP253(macro)  CMD(macro, 254)
#define LOOP255(macro) LOOP254(macro)  CMD(macro, 255)
#define LOOP_HELPER(macro, n) LOOP##n(macro)
#define LOOP(macro, n) LOOP_HELPER(macro, n)

#define for_each_set_bit(bit, addr, size) \
    for ((bit) = find_first_bit((addr), (size));        \
         (bit) < (size);                    \
         (bit) = find_next_bit((addr), (size), (bit) + 1))

#define for_each_set_bit_from(bit, addr, size) \
    for ((bit) = find_next_bit((addr), (size), (bit));    \
         (bit) < (size);                    \
         (bit) = find_next_bit((addr), (size), (bit) + 1))

#define EID_HIGH(eid) (((eid) >> 12) & 0xff)
#define EID_LOW(eid) ((eid) & 0xfff)
#define EID_GEN(eid_h, eid_l) ((eid_h) << 12 | (eid_l))

#define UB_ALIGNMENT 64

/* Round number down to multiple */
#define ALIGN_DOWN(n, m) ((n) / (m) * (m))

/* Round number up to multiple */
#define ALIGN_UP(n, m) ALIGN_DOWN((n) + (m) - 1, (m))
#define GENMASK(h, l) \
    (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define BITS_PER_LONG_LONG 64
#define GENMASK_ULL(h, l) \
    (((~0ULL) - (1ULL << (l)) + 1) & \
      (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))
#define DASH_SZ 3
/* The caller is responsible for free memory. */
char *line_generator(uint8_t len);
enum UbMsgType {
    MSG_REQ = 0,
    MSG_RSP = 1
};

enum UbMsgCode {
    UB_MSG_CODE_RAS  =  0,
    UB_MSG_CODE_LINK =  1,
    UB_MSG_CODE_CFG  =  2,
    UB_MSG_CODE_VDM  =  3,
    UB_MSG_CODE_EXCH =  4,
    UB_MSG_CODE_SEC  =  5,
    UB_MSG_CODE_POOL =  6,
    UB_MSG_CODE_MAX  =  7
};

struct UbLinkHeader {
    uint32_t plen : 14;
    uint32_t rm : 2;
    uint32_t cfg : 4;
    uint32_t rsvd1 : 1;
    uint32_t vl : 4;
    uint32_t rsvd0 : 1;
    uint32_t crd_vl : 4;
    uint32_t ack : 1;
    uint32_t crd : 1;
};
#define UB_CLAN_LINK_CFG 6

struct ClanNetworkHeader {
    /* DW0 */
    uint32_t dcna : 16;
    uint32_t scna : 16;
    /* DW1 */
#define NTH_NLP_WITH_TPH 0
#define NTH_NLP_WITHOUT_TPH 1
    uint32_t nth_nlp : 3;
    uint32_t mgmt : 1;
    uint32_t sl : 4;
    uint32_t lb : 8;
    uint32_t cc : 16;
};

typedef struct MsgExtendedHeader {
    uint32_t plen : 12;
    uint32_t rsvd : 4;
    uint32_t rsp_status : 8;
    union {
        struct {
            uint8_t type : 1;
            uint8_t msg_code : 3;
            uint8_t sub_msg_code : 4;
        };
        uint8_t code;
    };
} MsgExtendedHeader;

typedef struct MsgPktHeader { /* TODO, check byte order */
    /* DW0 */
    struct UbLinkHeader ulh;
    /* DW1-DW2 */
    struct ClanNetworkHeader nth;
    /* DW3 */
    uint32_t seid_h : 8;
    uint32_t upi : 16;
#define CTPH_NLP_UPI_40BITS_UEID 2
    uint32_t ctph_nlp : 4; /* tp header */
    uint32_t pad : 2;
#define CTPH_OPCODE_NOT_CNP 0
    uint32_t tp_opcode : 2;
    /* DW4 */
    uint32_t deid : 20;
    uint32_t seid_l : 12;
    /* DW5 */
    uint32_t src_tassn : 16;
    uint32_t taver : 3;
    uint32_t tk_vld : 1;
    uint32_t udf : 4;
#define TAH_OPCODE_MSG 0x14
    uint32_t ta_opcode : 8;
    /* DW6 */
    uint32_t sjetty : 20;
    uint32_t sjt_type : 2;
    uint32_t rsv0 : 3;
    uint32_t retry : 1;
    uint32_t se : 1;
    uint32_t jetty_en : 1;
    uint32_t rsv1 : 1;
    uint32_t odr : 3;
    /* DW7 */
    struct MsgExtendedHeader msgetah;

    /* DW8~DW11 */
    char payload[0]; /* payload */
} MsgPktHeader;
#define MSG_PKT_HEADER_SIZE 32

uint32_t fill_rq(BusControllerState *s, void *rsp, uint32_t rsp_size);
uint32_t fill_cq(BusControllerState *s, HiMsgCqe *cqe);
void fill_rq_cq(BusControllerState *s, void *rsp, uint32_t rsp_size, HiMsgCqe *cqe);
/* get eid from sysfs, not found will return UINT32_MAX */
uint32_t sysfs_get_dev_number_by_guid(UbGuid *guid);
uint32_t sysfs_get_ub_device_bus_instance_eid(char *sysfsdev);
uint32_t sysfs_get_bus_instance_type_by_eid(uint32_t eid);
uint32_t sysfs_get_bus_instance_eid_by_guid(UbGuid *guid);
void ub_mem_dump(void *start, int size, const char *tag_fmt, ...) __attribute__((format(printf, 3, 4)));
int ub_hexdump(void *data, int offset, int len, char *buff, int buff_size);
bool ub_guid_is_none(UbGuid *guid);

#endif
