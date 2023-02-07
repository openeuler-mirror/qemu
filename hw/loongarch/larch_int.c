/*
 * QEMU LOONGARCH interrupt support
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
#include "qemu/main-loop.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/loongarch/cpudevs.h"
#include "cpu.h"
#include "sysemu/kvm.h"
#include "kvm_larch.h"
#ifdef CONFIG_KVM
#include <linux/kvm.h>
#endif

static void cpu_irq_request(void *opaque, int irq, int level)
{
    LOONGARCHCPU *cpu = opaque;
    CPULOONGARCHState *env = &cpu->env;
    CPUState *cs = CPU(cpu);
    bool locked = false;

    if (irq < 0 || irq > 13) {
        return;
    }

    /* Make sure locking works even if BQL is already held by the caller */
    if (!qemu_mutex_iothread_locked()) {
        locked = true;
        qemu_mutex_lock_iothread();
    }

    if (level) {
        env->CSR_ESTAT |= 1 << irq;
    } else {
        env->CSR_ESTAT &= ~(1 << irq);
    }

    if (kvm_enabled()) {
        if (irq == 2) {
            kvm_loongarch_set_interrupt(cpu, irq, level);
        } else if (irq == 3) {
            kvm_loongarch_set_interrupt(cpu, irq, level);
        } else if (irq == 12) {
            kvm_loongarch_set_ipi_interrupt(cpu, irq, level);
        }
    }

    if (env->CSR_ESTAT & CSR_ESTAT_IPMASK) {
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
    }

    if (locked) {
        qemu_mutex_unlock_iothread();
    }
}

void cpu_init_irq(LOONGARCHCPU *cpu)
{
    CPULOONGARCHState *env = &cpu->env;
    qemu_irq *qi;
    int i;

    qi = qemu_allocate_irqs(cpu_irq_request, loongarch_env_get_cpu(env),
                            N_IRQS);
    for (i = 0; i < N_IRQS; i++) {
        env->irq[i] = qi[i];
    }
}
