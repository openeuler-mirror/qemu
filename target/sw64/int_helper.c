#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "qemu/host-utils.h"
#include "exec/memattrs.h"

uint64_t helper_zapnot(uint64_t val, uint64_t mskb)
{
    uint64_t mask;

    mask = -(mskb & 0x01) & 0x00000000000000ffull;
    mask |= -(mskb & 0x02) & 0x000000000000ff00ull;
    mask |= -(mskb & 0x04) & 0x0000000000ff0000ull;
    mask |= -(mskb & 0x08) & 0x00000000ff000000ull;
    mask |= -(mskb & 0x10) & 0x000000ff00000000ull;
    mask |= -(mskb & 0x20) & 0x0000ff0000000000ull;
    mask |= -(mskb & 0x40) & 0x00ff000000000000ull;
    mask |= -(mskb & 0x80) & 0xff00000000000000ull;

    return val & mask;
}

uint64_t helper_zap(uint64_t val, uint64_t mask)
{
    return helper_zapnot(val, ~mask);
}

uint64_t helper_cmpgeb(uint64_t va, uint64_t vb)
{
    int i;
    uint64_t ret = 0;
    uint64_t tmp;
    for (i = 0; i < 64; i += 8) {
        tmp = ((va >> i) & 0xff) + (~(vb >> i) & 0xff) + 1;
        ret |= (tmp >> 8) << (i / 8);
    }
    return ret;
}

#ifndef CONFIG_USER_ONLY
static inline MemTxAttrs cpu_get_mem_attrs(CPUSW64State *env)
{
    return ((MemTxAttrs) { .secure = 1 });
}

static inline AddressSpace *cpu_addressspace(CPUState *cs, MemTxAttrs attrs)
{
    return cpu_get_address_space(cs, cpu_asidx_from_attrs(cs, attrs));
}

uint64_t sw64_ldw_phys(CPUState *cs, hwaddr addr)
{
    SW64CPU *cpu = SW64_CPU(cs);
    int32_t ret;
    CPUSW64State *env = &cpu->env;
    MemTxAttrs attrs = cpu_get_mem_attrs(env);
    AddressSpace *as = cpu_addressspace(cs, attrs);

    ret = (int32_t)address_space_ldl(as, addr, attrs, NULL);

    return (uint64_t)(int64_t)ret;
}

void sw64_stw_phys(CPUState *cs, hwaddr addr, uint64_t val)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    MemTxAttrs attrs = cpu_get_mem_attrs(env);
    AddressSpace *as = cpu_addressspace(cs, attrs);

    address_space_stl(as, addr, (uint32_t)val, attrs, NULL);
}

uint64_t sw64_ldl_phys(CPUState *cs, hwaddr addr)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    MemTxAttrs attrs = cpu_get_mem_attrs(env);
    AddressSpace *as = cpu_addressspace(cs, attrs);

    return address_space_ldq(as, addr, attrs, NULL);
}

void sw64_stl_phys(CPUState *cs, hwaddr addr, uint64_t val)
{
    SW64CPU *cpu = SW64_CPU(cs);
    CPUSW64State *env = &cpu->env;
    MemTxAttrs attrs = cpu_get_mem_attrs(env);
    AddressSpace *as = cpu_addressspace(cs, attrs);

    address_space_stq(as, addr, val, attrs, NULL);
}

uint64_t helper_pri_ldw(CPUSW64State *env, uint64_t hwaddr)
{
    CPUState *cs = CPU(sw64_env_get_cpu(env));
    return sw64_ldw_phys(cs, hwaddr);
}

void helper_pri_stw(CPUSW64State *env, uint64_t val, uint64_t hwaddr)
{
    CPUState *cs = CPU(sw64_env_get_cpu(env));
    sw64_stw_phys(cs, hwaddr, val);
}

uint64_t helper_pri_ldl(CPUSW64State *env, uint64_t hwaddr)
{
    CPUState *cs = CPU(sw64_env_get_cpu(env));
    return sw64_ldl_phys(cs, hwaddr);
}

void helper_pri_stl(CPUSW64State *env, uint64_t val, uint64_t hwaddr)
{
    CPUState *cs = CPU(sw64_env_get_cpu(env));
    sw64_stl_phys(cs, hwaddr, val);
}
#endif
