ub_ss = ss.source_set()
ub_ss.add(files(
  'ub.c',
  'ub_ubc.c',
  'ub_config.c',
  'ub_acpi.c',
))
system_ss.add_all(when: 'CONFIG_HW_UB', if_true: ub_ss)
subdir('hisi')
