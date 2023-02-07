loongarch_user_ss = ss.source_set()
loongarch_softmmu_ss = ss.source_set()
loongarch_ss = ss.source_set()
loongarch_ss.add(files(
  'cpu.c',
  'fpu.c',
  'gdbstub.c',
))

gen = [
  decodetree.process('insn.decode', extra_args: [ '--decode', 'decode_insn',
                                                  '--insnwidth', '32' ])
]

loongarch_ss.add(gen)
loongarch_ss.add(when: 'CONFIG_TCG', if_true: files(
   'helper.c',
   'translate.c',
   'op_helper.c',
   'fpu_helper.c',
   'tlb_helper.c',
   'csr_helper.c',
))

loongarch_softmmu_ss.add(when: 'CONFIG_SOFTMMU', if_true: files(
   'machine.c',
   'stabletimer.c',
   'arch_dump.c',
))

loongarch_softmmu_ss.add(when: 'CONFIG_KVM', if_true: files('kvm.c'))

target_arch += {'loongarch64': loongarch_ss}
target_softmmu_arch += {'loongarch64': loongarch_softmmu_ss}
target_user_arch += {'loongarch64': loongarch_user_ss}
