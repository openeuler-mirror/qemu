ub_ss = ss.source_set()
ub_ss.add(files(
  'ub.c',
  'ub_ubc.c',
  'ub_ummu.c',
  'ub_config.c',
  'ub_acpi.c',
  'ub_enum.c',
  'ub_common.c',
  'ub_usi.c',
  'ub_cna_mgmt.c',
  'ub_sec.c',
))
system_ss.add_all(when: 'CONFIG_HW_UB', if_true: ub_ss)
subdir('hisi')
