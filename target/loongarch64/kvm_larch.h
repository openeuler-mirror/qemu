/*
 * KVM/LOONGARCH: LOONGARCH specific KVM APIs
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

#ifndef KVM_LOONGARCH_H
#define KVM_LOONGARCH_H

/**
 * kvm_loongarch_reset_vcpu:
 * @cpu: LOONGARCHCPU
 *
 * Called at reset time to set kernel registers to their initial values.
 */
void kvm_loongarch_reset_vcpu(LOONGARCHCPU *cpu);

int kvm_loongarch_set_interrupt(LOONGARCHCPU *cpu, int irq, int level);
int kvm_loongarch_set_ipi_interrupt(LOONGARCHCPU *cpu, int irq, int level);

int kvm_loongarch_put_pvtime(LOONGARCHCPU *cpu);
int kvm_loongarch_get_pvtime(LOONGARCHCPU *cpu);

#ifndef KVM_INTERRUPT_SET
#define KVM_INTERRUPT_SET -1
#endif

#ifndef KVM_INTERRUPT_UNSET
#define KVM_INTERRUPT_UNSET -2
#endif

#ifndef KVM_INTERRUPT_SET_LEVEL
#define KVM_INTERRUPT_SET_LEVEL -3
#endif

#endif /* KVM_LOONGARCH_H */
