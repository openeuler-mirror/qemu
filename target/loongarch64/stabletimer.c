/*
 * QEMU LOONGARCH timer support
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

#include "qemu/osdep.h"
#include "hw/loongarch/cpudevs.h"
#include "qemu/timer.h"
#include "sysemu/kvm.h"
#include "internal.h"
#include "hw/irq.h"

#ifdef DEBUG_TIMER
#define debug_timer(fmt, args...)                                             \
    printf("%s(%d)-%s -> " #fmt "\n", __FILE__, __LINE__, __func__, ##args);
#else
#define debug_timer(fmt, args...)
#endif

#define TIMER_PERIOD 10 /* 10 ns period for 100 Mhz frequency */
#define STABLETIMER_TICK_MASK 0xfffffffffffcUL
#define STABLETIMER_ENABLE 0x1UL
#define STABLETIMER_PERIOD 0x2UL

/* return random value in [low, high] */
uint32_t cpu_loongarch_get_random_ls3a5k_tlb(uint32_t low, uint32_t high)
{
    static uint32_t seed = 5;
    static uint32_t prev_idx;
    uint32_t idx;
    uint32_t nb_rand_tlb = high - low + 1;

    do {
        seed = 1103515245 * seed + 12345;
        idx = (seed >> 16) % nb_rand_tlb + low;
    } while (idx == prev_idx);
    prev_idx = idx;

    return idx;
}

/* LOONGARCH timer */
uint64_t cpu_loongarch_get_stable_counter(CPULOONGARCHState *env)
{
    return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) / TIMER_PERIOD;
}

uint64_t cpu_loongarch_get_stable_timer_ticks(CPULOONGARCHState *env)
{
    uint64_t now, expire;

    now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    expire = timer_expire_time_ns(env->timer);

    return (expire - now) / TIMER_PERIOD;
}

void cpu_loongarch_store_stable_timer_config(CPULOONGARCHState *env,
                                             uint64_t value)
{
    uint64_t now, next;

    env->CSR_TCFG = value;
    if (value & STABLETIMER_ENABLE) {
        now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        next = now + (value & STABLETIMER_TICK_MASK) * TIMER_PERIOD;
        timer_mod(env->timer, next);
    }
    debug_timer("0x%lx 0x%lx now 0x%lx, next 0x%lx", value, env->CSR_TCFG, now,
                next);
}

static void loongarch_stable_timer_cb(void *opaque)
{
    CPULOONGARCHState *env;
    uint64_t now, next;

    env = opaque;
    debug_timer();
    if (env->CSR_TCFG & STABLETIMER_PERIOD) {
        now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        next = now + (env->CSR_TCFG & STABLETIMER_TICK_MASK) * TIMER_PERIOD;
        timer_mod(env->timer, next);
    } else {
        env->CSR_TCFG &= ~STABLETIMER_ENABLE;
    }

    qemu_irq_raise(env->irq[IRQ_TIMER]);
}

void cpu_loongarch_clock_init(LOONGARCHCPU *cpu)
{
    CPULOONGARCHState *env = &cpu->env;

    /*
     * If we're in KVM mode, don't create the periodic timer, that is handled
     * in kernel.
     */
    if (!kvm_enabled()) {
        env->timer =
            timer_new_ns(QEMU_CLOCK_VIRTUAL, &loongarch_stable_timer_cb, env);
    }
}
