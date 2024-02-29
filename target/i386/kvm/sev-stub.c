#include "qemu/osdep.h"
#include "sev.h"

bool sev_kvm_has_msr_ghcb;

int sev_remove_shared_regions_list(unsigned long gfn_start,
                                   unsigned long gfn_end)
{
    return 0;
}

int sev_add_shared_regions_list(unsigned long gfn_start, unsigned long gfn_end)
{
    return 0;
}

void sev_del_migrate_blocker(void)
{
}
