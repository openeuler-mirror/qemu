loongarch_ss = ss.source_set()
loongarch_ss.add(files('larch_3a.c'), fdt)
loongarch_ss.add(files(
  'larch_int.c',
  'larch_hotplug.c',
  'ls7a_nb.c',
  'ioapic.c',
  'acpi-build.c',
  'ipi.c',
  'apic.c',
  'iocsr.c',
  'sysbus-fdt.c',
))

hw_arch += {'loongarch64': loongarch_ss}
