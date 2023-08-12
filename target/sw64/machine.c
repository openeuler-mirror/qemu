#include "qemu/osdep.h"
#include "qemu-common.h"
#include "cpu.h"
#include "migration/vmstate.h"
#include "migration/cpu.h"

VMStateDescription vmstate_sw64_cpu = {
    .name = "cpu",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
#ifdef CONFIG_KVM
	VMSTATE_UINTTL_ARRAY(k_regs, SW64CPU, 158),
	VMSTATE_UINTTL_ARRAY(k_vcb, SW64CPU, 48),
#endif
	VMSTATE_END_OF_LIST()
    }
};
