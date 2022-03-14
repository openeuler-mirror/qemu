sw64_ss = ss.source_set()

sw64_ss.add(files('sw64_iommu.c'))

sw64_ss.add(when: 'CONFIG_CORE3', if_true: files(
  'core3.c',
  'core3_board.c',
))

hw_arch += {'sw64': sw64_ss}
