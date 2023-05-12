Name: qemu
Version: 4.1.0
Release: 77
Epoch: 10
Summary: QEMU is a generic and open source machine emulator and virtualizer
License: GPLv2 and BSD and MIT and CC-BY-SA-4.0
URL: http://www.qemu.org
Source0: https://www.qemu.org/download/%{name}-%{version}%{?rcstr}.tar.xz
Source1: 80-kvm.rules
Source2: 99-qemu-guest-agent.rules
Source3: bridge.conf

Patch0001: pl011-reset-read-FIFO-when-UARTTIMSC-0-UARTICR-0xfff.patch
Patch0002: pl031-support-rtc-timer-property-for-pl031.patch
Patch0003: vhost-cancel-migration-when-vhost-user-restarted.patch
Patch0004: qcow2-fix-memory-leak-in-qcow2_read_extensions.patch
Patch0005: bios-tables-test-prepare-to-change-ARM-virt-ACPI-DSDT.patch
Patch0006: hw-arm-expose-host-CPU-frequency-info-to-guest.patch
Patch0007: smbios-Add-missing-member-of-type-4-for-smbios-3.0.patch
Patch0008: tests-bios-tables-test-disable-this-testcase.patch
Patch0009: hw-arm-virt-Introduce-cpu-topology-support.patch
Patch0010: hw-arm64-add-vcpu-cache-info-support.patch
Patch0011: xhci-Fix-memory-leak-in-xhci_address_slot.patch
Patch0012: xhci-Fix-memory-leak-in-xhci_kick_epctx.patch
Patch0013: ehci-fix-queue-dev-null-ptr-dereference.patch
Patch0014: util-async-hold-AioContext-ref-to-prevent-use-after-free.patch
Patch0015: vhost-user-scsi-prevent-using-uninitialized-vqs.patch
Patch0016: cpu-add-Kunpeng-920-cpu-support.patch
Patch0017: cpu-parse-feature-to-avoid-failure.patch
Patch0018: cpu-add-Cortex-A72-processor-kvm-target-support.patch
Patch0019: pcie-disable-the-PCI_EXP_LINKSTA_DLLA-cap.patch
Patch0020: vnc-fix-memory-leak-when-vnc-disconnect.patch
Patch0021: linux-headers-update-against-KVM-ARM-Fix-256-vcpus.patch
Patch0022: intc-arm_gic-Support-IRQ-injection-for-more-than-256.patch
Patch0023: ARM-KVM-Check-KVM_CAP_ARM_IRQ_LINE_LAYOUT_2-for-smp.patch
Patch0024: 9pfs-local-Fix-possible-memory-leak-in-local_link.patch
Patch0025: scsi-disk-define-props-in-scsi_block_disk-to-avoid-memleaks.patch
Patch0026: arm-translate-a64-fix-uninitialized-variable-warning.patch
Patch0027: nbd-fix-uninitialized-variable-warning.patch
Patch0028: xhci-Fix-memory-leak-in-xhci_kick_epctx-when-poweroff.patch
Patch0029: block-fix-memleaks-in-bdrv_refresh_filename.patch
Patch0030: iscsi-Cap-block-count-from-GET-LBA-STATUS-CVE-2020-1.patch
Patch0031: slirp-tcp_emu-Fix-oob-access.patch
Patch0032: slirp-use-correct-size-while-emulating-IRC-commands.patch
Patch0033: slirp-use-correct-size-while-emulating-commands.patch
Patch0034: slirp-util-add-slirp_fmt-helpers.patch
Patch0035: slirp-tcp_emu-fix-unsafe-snprintf-usages.patch
Patch0036: block-iscsi-use-MIN-between-mx_sb_len-and-sb_len_wr.patch
Patch0037: monitor-fix-memory-leak-in-monitor_fdset_dup_fd_find.patch
Patch0038: memory-Align-MemoryRegionSections-fields.patch
Patch0039: memory-Provide-an-equality-function-for-MemoryRegion.patch
Patch0040: vhost-Fix-memory-region-section-comparison.patch
Patch0041: file-posix-Handle-undetectable-alignment.patch
Patch0042: block-backup-fix-max_transfer-handling-for-copy_rang.patch
Patch0043: block-backup-fix-backup_cow_with_offload-for-last-cl.patch
Patch0044: qcow2-Limit-total-allocation-range-to-INT_MAX.patch
Patch0045: mirror-Do-not-dereference-invalid-pointers.patch
Patch0046: COLO-compare-Fix-incorrect-if-logic.patch
Patch0047: qcow2-bitmap-Fix-uint64_t-left-shift-overflow.patch
Patch0048: pcie-Add-pcie-root-port-fast-plug-unplug-feature.patch
Patch0049: pcie-Compat-with-devices-which-do-not-support-Link-W.patch
Patch0050: aio-wait-delegate-polling-of-main-AioContext-if-BQL-not-held.patch
Patch0051: async-use-explicit-memory-barriers.patch
Patch0052: dma-helpers-ensure-AIO-callback-is-invoked-after-can.patch
Patch0053: Revert-ide-ahci-Check-for-ECANCELED-in-aio-callbacks.patch
Patch0054: pc-Don-t-make-die-id-mandatory-unless-necessary.patch
Patch0055: block-file-posix-Reduce-xfsctl-use.patch
Patch0056: pr-manager-Fix-invalid-g_free-crash-bug.patch
Patch0057: x86-do-not-advertise-die-id-in-query-hotpluggbale-cp.patch
Patch0058: vpc-Return-0-from-vpc_co_create-on-success.patch
Patch0059: target-arm-Free-TCG-temps-in-trans_VMOV_64_sp.patch
Patch0060: target-arm-Don-t-abort-on-M-profile-exception-return.patch
Patch0061: libvhost-user-fix-SLAVE_SEND_FD-handling.patch
Patch0062: qcow2-Fix-the-calculation-of-the-maximum-L2-cache-si.patch
Patch0063: block-nfs-tear-down-aio-before-nfs_close.patch
Patch0064: blockjob-update-nodes-head-while-removing-all-bdrv.patch
Patch0065: block-qcow2-Fix-corruption-introduced-by-commit-8ac0.patch
Patch0066: coroutine-Add-qemu_co_mutex_assert_locked.patch
Patch0067: qcow2-Fix-corruption-bug-in-qcow2_detect_metadata_pr.patch
Patch0068: hw-arm-boot.c-Set-NSACR.-CP11-CP10-for-NS-kernel-boo.patch
Patch0069: make-release-pull-in-edk2-submodules-so-we-can-build.patch
Patch0070: roms-Makefile.edk2-don-t-pull-in-submodules-when-bui.patch
Patch0071: block-snapshot-Restrict-set-of-snapshot-nodes.patch
Patch0072: vhost-user-save-features-if-the-char-dev-is-closed.patch
Patch0073: hw-core-loader-Fix-possible-crash-in-rom_copy.patch
Patch0074: ui-Fix-hanging-up-Cocoa-display-on-macOS-10.15-Catal.patch
Patch0075: virtio-new-post_load-hook.patch
Patch0076: virtio-net-prevent-offloads-reset-on-migration.patch
Patch0077: util-hbitmap-strict-hbitmap_reset.patch
Patch0078: hbitmap-handle-set-reset-with-zero-length.patch
Patch0079: target-arm-Allow-reading-flags-from-FPSCR-for-M-prof.patch
Patch0080: scsi-lsi-exit-infinite-loop-while-executing-script-C.patch
Patch0081: virtio-blk-Cancel-the-pending-BH-when-the-dataplane-.patch
Patch0082: qcow2-Fix-QCOW2_COMPRESSED_SECTOR_MASK.patch
Patch0083: util-iov-introduce-qemu_iovec_init_extended.patch
Patch0084: util-iov-improve-qemu_iovec_is_zero.patch
Patch0085: block-io-refactor-padding.patch
Patch0086: block-Make-wait-mark-serialising-requests-public.patch
Patch0087: block-Add-bdrv_co_get_self_request.patch
Patch0088: block-file-posix-Let-post-EOF-fallocate-serialize.patch
Patch0089: block-posix-Always-allocate-the-first-block.patch
Patch0090: block-create-Do-not-abort-if-a-block-driver-is-not-a.patch
Patch0091: mirror-Keep-mirror_top_bs-drained-after-dropping-per.patch
Patch0092: target-arm-kvm-trivial-Clean-up-header-documentation.patch
Patch0093: target-arm-kvm64-kvm64-cpus-have-timer-registers.patch
Patch0094: target-arm-kvm-Implement-virtual-time-adjustment.patch
Patch0095: target-arm-cpu-Add-the-kvm-no-adjvtime-CPU-property.patch
Patch0096: hw-acpi-Make-ACPI-IO-address-space-configurable.patch
Patch0097: hw-acpi-Do-not-create-memory-hotplug-method-when-han.patch
Patch0098: hw-acpi-Add-ACPI-Generic-Event-Device-Support.patch
Patch0099: hw-arm-virt-Add-memory-hotplug-framework.patch
Patch0100: hw-arm-virt-Enable-device-memory-cold-hot-plug-with-.patch
Patch0101: hw-arm-virt-acpi-build-Add-PC-DIMM-in-SRAT.patch
Patch0102: hw-arm-Factor-out-powerdown-notifier-from-GPIO.patch
Patch0103: hw-arm-Use-GED-for-system_powerdown-event.patch
Patch0104: docs-specs-Add-ACPI-GED-documentation.patch
Patch0105: tests-Update-ACPI-tables-list-for-upcoming-arm-virt-.patch
Patch0106: tests-acpi-add-empty-files.patch
Patch0107: tests-allow-empty-expected-files.patch
Patch0108: tests-Add-bios-tests-to-arm-virt.patch
Patch0109: tests-document-how-to-update-acpi-tables.patch
Patch0110: hw-arm-virt-Simplify-by-moving-the-gic-in-the-machin.patch
Patch0111: bugfix-Use-gicr_typer-in-arm_gicv3_icc_reset.patch
Patch0112: Typo-Correct-the-name-of-CPU-hotplug-memory-region.patch
Patch0113: acpi-madt-Factor-out-the-building-of-MADT-GICC-struc.patch
Patch0114: acpi-ged-Add-virt_madt_cpu_entry-to-madt_cpu-hook.patch
Patch0115: arm-virt-acpi-Factor-out-CPPC-building-from-DSDT-CPU.patch
Patch0116: acpi-cpu-Prepare-build_cpus_aml-for-arm-virt.patch
Patch0117: acpi-ged-Extend-ACPI-GED-to-support-CPU-hotplug.patch
Patch0118: arm-cpu-assign-arm_get_arch_id-handler-to-get_arch_i.patch
Patch0119: arm-virt-Attach-ACPI-CPU-hotplug-support-to-virt.patch
Patch0120: arm-virt-Add-CPU-hotplug-framework.patch
Patch0121: arm-virt-Add-CPU-topology-support.patch
Patch0122: test-numa-Adjust-aarch64-numa-test.patch
Patch0123: hw-arm-virt-Factor-out-some-CPU-init-codes-to-pre_pl.patch
Patch0124: hw-arm-boot-Add-manually-register-and-trigger-of-CPU.patch
Patch0125: arm-virt-gic-Construct-irqs-connection-from-create_g.patch
Patch0126: intc-gicv3_common-Factor-out-arm_gicv3_common_cpu_re.patch
Patch0127: intc-gicv3_cpuif-Factor-out-gicv3_init_one_cpuif.patch
Patch0128: intc-kvm_gicv3-Factor-out-kvm_arm_gicv3_cpu_realize.patch
Patch0129: hw-intc-gicv3-Add-CPU-hotplug-realize-hook.patch
Patch0130: accel-kvm-Add-pre-park-vCPU-support.patch
Patch0131: intc-gicv3-Add-pre-sizing-capability-to-GICv3.patch
Patch0132: acpi-madt-Add-pre-sizing-capability-to-MADT-GICC-str.patch
Patch0133: arm-virt-Add-cpu_hotplug_enabled-field.patch
Patch0134: arm-virt-acpi-Extend-cpufreq-to-support-max_cpus.patch
Patch0135: arm-virt-Pre-sizing-MADT-GICC-PPTT-GICv3-and-Pre-par.patch
Patch0136: arm-virt-Add-some-sanity-checks-in-cpu_pre_plug-hook.patch
Patch0137: arm-virt-Start-up-CPU-hot-plug.patch
Patch0138: migration-always-initialise-ram_counters-for-a-new-m.patch
Patch0139: migration-add-qemu_file_update_transfer-interface.patch
Patch0140: migration-add-speed-limit-for-multifd-migration.patch
Patch0141: migration-update-ram_counters-for-multifd-sync-packe.patch
Patch0142: migration-Make-global-sem_sync-semaphore-by-channel.patch
Patch0143: migration-multifd-fix-nullptr-access-in-terminating-m.patch
Patch0144: migration-Maybe-VM-is-paused-when-migration-is-cance.patch
Patch0145: migration-multifd-fix-potential-wrong-acception-orde.patch
Patch0146: migration-multifd-fix-destroyed-mutex-access-in-term.patch
Patch0147: migration-multifd-fix-nullptr-access-in-multifd_send.patch
Patch0148: vtimer-compat-cross-version-migration-from-v4.0.1.patch
Patch0149: migration-ram-Do-error_free-after-migrate_set_error-.patch
Patch0150: migration-ram-fix-memleaks-in-multifd_new_send_chann.patch
Patch0151: migration-rdma-fix-a-memleak-on-error-path-in-rdma_s.patch
Patch0152: arm-virt-Support-CPU-cold-plug.patch
Patch0153: ide-Fix-incorrect-handling-of-some-PRDTs-in-ide_dma_.patch
Patch0154: ati-vga-Fix-checks-in-ati_2d_blt-to-avoid-crash.patch
Patch0155: slirp-tftp-restrict-relative-path-access.patch
Patch0156: slirp-ip_reass-Fix-use-after-free.patch
Patch0157: bt-use-size_t-type-for-length-parameters-instead-of-.patch
Patch0158: log-Add-some-logs-on-VM-runtime-path.patch
Patch0159: Revert-vtimer-compat-cross-version-migration-from-v4.patch
Patch0160: ARM64-record-vtimer-tick-when-cpu-is-stopped.patch
Patch0161: hw-arm-virt-add-missing-compat-for-kvm-no-adjvtime.patch
Patch0162: migration-Compat-virtual-timer-adjust-for-v4.0.1-and.patch
Patch0163: vtimer-Drop-vtimer-virtual-timer-adjust.patch
Patch0164: target-arm-Add-the-kvm_adjvtime-vcpu-property-for-Co.patch
Patch0165: target-arm-Fix-PAuth-sbox-functions.patch
Patch0166: es1370-check-total-frame-count-against-current-frame.patch
Patch0167: exec-set-map-length-to-zero-when-returning-NULL.patch
Patch0168: ati-vga-check-mm_index-before-recursive-call-CVE-202.patch
Patch0169: megasas-use-unsigned-type-for-reply_queue_head-and-c.patch
Patch0170: megasas-avoid-NULL-pointer-dereference.patch
Patch0171: megasas-use-unsigned-type-for-positive-numeric-field.patch
Patch0172: hw-scsi-megasas-Fix-possible-out-of-bounds-array-acc.patch
Patch0173: hw-arm-acpi-enable-SHPC-native-hot-plug.patch
Patch0174: hw-usb-core-fix-buffer-overflow.patch
Patch0175: slirp-drop-bogus-IPv6-messages.patch
Patch0176: hw-sd-sdhci-Fix-DMA-Transfer-Block-Size-field.patch
Patch0177: hw-xhci-check-return-value-of-usb_packet_map.patch
Patch0178: hw-net-xgmac-Fix-buffer-overflow-in-xgmac_enet_send.patch
Patch0179: hw-net-net_tx_pkt-fix-assertion-failure-in-net_tx_pk.patch
Patch0180: sm501-Convert-printf-abort-to-qemu_log_mask.patch
Patch0181: sm501-Shorten-long-variable-names-in-sm501_2d_operat.patch
Patch0182: sm501-Use-BIT-x-macro-to-shorten-constant.patch
Patch0183: sm501-Clean-up-local-variables-in-sm501_2d_operation.patch
Patch0184: sm501-Replace-hand-written-implementation-with-pixma.patch
Patch0185: pci-check-bus-pointer-before-dereference.patch
Patch0186: hw-ide-check-null-block-before-_cancel_dma_sync.patch
Patch0187: target-arm-Add-isar_feature-tests-for-PAN-ATS1E1.patch
Patch0188: target-arm-Add-ID_AA64MMFR2_EL1.patch
Patch0189: target-arm-Add-and-use-FIELD-definitions-for-ID_AA64.patch
Patch0190: target-arm-Use-FIELD-macros-for-clearing-ID_DFR0-PER.patch
Patch0191: target-arm-Define-an-aa32_pmu_8_1-isar-feature-test-.patch
Patch0192: target-arm-Add-_aa64_-and-_any_-versions-of-pmu_8_1-.patch
Patch0193: target-arm-Stop-assuming-DBGDIDR-always-exists.patch
Patch0194: target-arm-Move-DBGDIDR-into-ARMISARegisters.patch
Patch0195: target-arm-Enable-ARMv8.2-ATS1E1-in-cpu-max.patch
Patch0196: target-arm-Test-correct-register-in-aa32_pan-and-aa3.patch
Patch0197: target-arm-Read-debug-related-ID-registers-from-KVM.patch
Patch0198: target-arm-monitor-Introduce-qmp_query_cpu_model_exp.patch
Patch0199: target-arm-monitor-query-cpu-model-expansion-crashed.patch
Patch0200: target-arm-convert-isar-regs-to-array.patch
Patch0201: target-arm-parse-cpu-feature-related-options.patch
Patch0202: target-arm-register-CPU-features-for-property.patch
Patch0203: target-arm-Allow-ID-registers-to-synchronize-to-KVM.patch
Patch0204: target-arm-introduce-CPU-feature-dependency-mechanis.patch
Patch0205: target-arm-introduce-KVM_CAP_ARM_CPU_FEATURE.patch
Patch0206: target-arm-Add-CPU-features-to-query-cpu-model-expan.patch
Patch0207: target-arm-Update-ID-fields.patch
Patch0208: target-arm-Add-more-CPU-features.patch
Patch0209: target-arm-ignore-evtstrm-and-cpuid-CPU-features.patch
Patch0210: target-arm-Update-the-ID-registers-of-Kunpeng-920.patch
Patch0211: target-arm-only-set-ID_PFR1_EL1.GIC-for-AArch32-gues.patch
Patch0212: target-arm-clear-EL2-and-EL3-only-when-kvm-is-not-en.patch
Patch0213: ati-check-x-y-display-parameter-values.patch
Patch0214: net-remove-an-assert-call-in-eth_get_gso_type.patch
Patch0215: json-Fix-a-memleak-in-parse_pair.patch
Patch0216: tests-Disalbe-filemonitor-testcase.patch
Patch0217: hostmem-Fix-up-free-host_nodes-list-right-after-visi.patch
Patch0218: slirp-check-pkt_len-before-reading-protocol-header.patch
Patch0219: hw-usb-hcd-ohci-check-for-processed-TD-before-retire.patch
Patch0220: hw-ehci-check-return-value-of-usb_packet_map.patch
Patch0221: hw-usb-hcd-ohci-check-len-and-frame_number-variables.patch
Patch0222: hw-net-e1000e-advance-desc_offset-in-case-of-null-de.patch
Patch0223: target-arm-Fix-write-redundant-values-to-kvm.patch
Patch0224: memory-clamp-cached-translation-in-case-it-points-to.patch
Patch0225: ati-use-vga_read_byte-in-ati_cursor_define.patch
Patch0226: sd-sdhci-assert-data_count-is-within-fifo_buffer.patch
Patch0227: msix-add-valid.accepts-methods-to-check-address.patch
Patch0228: ide-atapi-check-io_buffer_index-in-ide_atapi_cmd_rep.patch
Patch0229: net-vmxnet3-validate-configuration-values-during-act.patch
Patch0230: scsi-bus-Refactor-the-code-that-retries-requests.patch
Patch0231: scsi-disk-Add-support-for-retry-on-errors.patch
Patch0232: qapi-block-core-Add-retry-option-for-error-action.patch
Patch0233: block-backend-Introduce-retry-timer.patch
Patch0234: block-backend-Add-device-specific-retry-callback.patch
Patch0235: block-backend-Enable-retry-action-on-errors.patch
Patch0236: block-backend-Add-timeout-support-for-retry.patch
Patch0237: block-Add-error-retry-param-setting.patch
Patch0238: virtio-blk-Refactor-the-code-that-processes-queued-r.patch
Patch0239: virtio-blk-On-restart-process-queued-requests-in-the.patch
Patch0240: virtio_blk-Add-support-for-retry-on-errors.patch
Patch0241: block-backend-Stop-retrying-when-draining.patch
Patch0242: block-Add-sanity-check-when-setting-retry-parameters.patch
Patch0243: build-smt-processor-structure-to-support-smt-topolog.patch
Patch0244: Bugfix-hw-acpi-Use-max_cpus-instead-of-cpus-when-bui.patch
Patch0245: migration-dirtyrate-setup-up-query-dirtyrate-framwor.patch
Patch0246: migration-dirtyrate-add-DirtyRateStatus-to-denote-ca.patch
Patch0247: migration-dirtyrate-Add-RamblockDirtyInfo-to-store-s.patch
Patch0248: migration-dirtyrate-Add-dirtyrate-statistics-series-.patch
Patch0249: migration-dirtyrate-move-RAMBLOCK_FOREACH_MIGRATABLE.patch
Patch0250: migration-dirtyrate-Record-hash-results-for-each-sam.patch
Patch0251: migration-dirtyrate-Compare-page-hash-results-for-re.patch
Patch0252: migration-dirtyrate-skip-sampling-ramblock-with-size.patch
Patch0253: migration-dirtyrate-Implement-set_sample_page_period.patch
Patch0254: migration-dirtyrate-Implement-calculate_dirtyrate-fu.patch
Patch0255: migration-dirtyrate-Implement-qmp_cal_dirty_rate-qmp.patch
Patch0256: migration-dirtyrate-Add-trace_calls-to-make-it-easie.patch
Patch0257: migration-dirtyrate-record-start_time-and-calc_time-.patch
Patch0258: migration-dirtyrate-present-dirty-rate-only-when-que.patch
Patch0259: migration-dirtyrate-simplify-includes-in-dirtyrate.c.patch
Patch0260: migration-tls-save-hostname-into-MigrationState.patch
Patch0261: migration-tls-extract-migration_tls_client_create-fo.patch
Patch0262: migration-tls-add-tls_hostname-into-MultiFDSendParam.patch
Patch0263: migration-tls-extract-cleanup-function-for-common-us.patch
Patch0264: migration-tls-add-support-for-multifd-tls-handshake.patch
Patch0265: migration-tls-add-trace-points-for-multifd-tls.patch
Patch0266: qemu-file-Don-t-do-IO-after-shutdown.patch
Patch0267: multifd-Make-sure-that-we-don-t-do-any-IO-after-an-e.patch
Patch0268: migration-Don-t-send-data-if-we-have-stopped.patch
Patch0269: migration-Create-migration_is_running.patch
Patch0270: migration-fix-COLO-broken-caused-by-a-previous-commi.patch
Patch0271: migration-multifd-fix-hangup-with-TLS-Multifd-due-to.patch
Patch0272: multifd-tls-fix-memoryleak-of-the-QIOChannelSocket-o.patch
Patch0273: migration-fix-memory-leak-in-qmp_migrate_set_paramet.patch
Patch0274: migration-tls-fix-inverted-semantics-in-multifd_chan.patch
Patch0275: migration-tls-add-error-handling-in-multifd_tls_hand.patch
Patch0276: arm-cpu-Fixed-function-undefined-error-at-compile-ti.patch
Patch0277: scsi-mptsas-dequeue-request-object-in-case-of-an-err.patch
Patch0278: hw-sd-sdhci-Don-t-transfer-any-data-when-command-tim.patch
Patch0279: hw-sd-sdhci-Don-t-write-to-SDHC_SYSAD-register-when-.patch
Patch0280: hw-sd-sdhci-Correctly-set-the-controller-status-for-.patch
Patch0281: hw-sd-sdhci-Limit-block-size-only-when-SDHC_BLKSIZE-.patch
Patch0282: hw-sd-sdhci-Reset-the-data-pointer-of-s-fifo_buffer-.patch
Patch0283: net-introduce-qemu_receive_packet.patch
Patch0284: e1000-switch-to-use-qemu_receive_packet-for-loopback.patch
Patch0285: dp8393x-switch-to-use-qemu_receive_packet-for-loopba.patch
Patch0286: sungem-switch-to-use-qemu_receive_packet-for-loopbac.patch
Patch0287: tx_pkt-switch-to-use-qemu_receive_packet_iov-for-loo.patch
Patch0288: rtl8139-switch-to-use-qemu_receive_packet-for-loopba.patch
Patch0289: pcnet-switch-to-use-qemu_receive_packet-for-loopback.patch
Patch0290: cadence_gem-switch-to-use-qemu_receive_packet-for-lo.patch
Patch0291: lan9118-switch-to-use-qemu_receive_packet-for-loopba.patch
Patch0292: hw-pci-host-add-pci-intack-write-method.patch
Patch0293: pci-host-add-pcie-msi-read-method.patch
Patch0294: vfio-add-quirk-device-write-method.patch
Patch0295: prep-add-ppc-parity-write-method.patch
Patch0296: nvram-add-nrf51_soc-flash-read-method.patch
Patch0297: spapr_pci-add-spapr-msi-read-method.patch
Patch0298: tz-ppc-add-dummy-read-write-methods.patch
Patch0299: imx7-ccm-add-digprog-mmio-write-method.patch
Patch0300: bugfix-fix-Uninitialized-Free-Vulnerability.patch
Patch0301: 9pfs-Fully-restart-unreclaim-loop-CVE-2021-20181.patch
Patch0302: vhost-user-gpu-fix-resource-leak-in-vg_resource_crea.patch
Patch0303: vhost-user-gpu-fix-memory-leak-in-vg_resource_attach.patch
Patch0304: vhost-user-gpu-fix-memory-leak-while-calling-vg_reso.patch
Patch0305: vhost-user-gpu-fix-memory-leak-in-virgl_cmd_resource.patch
Patch0306: vhost-user-gpu-fix-memory-leak-in-virgl_resource_att.patch
Patch0307: vhost-user-gpu-fix-memory-disclosure-in-virgl_cmd_ge.patch
Patch0308: vhost-user-gpu-fix-OOB-write-in-virgl_cmd_get_capset.patch
Patch0309: ide-ahci-add-check-to-avoid-null-dereference-CVE-201.patch
Patch0310: hw-intc-arm_gic-Fix-interrupt-ID-in-GICD_SGIR-regist.patch
Patch0311: usb-limit-combined-packets-to-1-MiB-CVE-2021-3527.patch
Patch0312: hw-net-rocker_of_dpa-fix-double-free-bug-of-rocker-d.patch
Patch0313: x86-Intel-AVX512_BF16-feature-enabling.patch
Patch0314: i386-Add-MSR-feature-bit-for-MDS-NO.patch
Patch0315: i386-Add-macro-for-stibp.patch
Patch0316: i386-Add-new-CPU-model-Cooperlake.patch
Patch0317: target-i386-Add-new-bit-definitions-of-MSR_IA32_ARCH.patch
Patch0318: target-i386-Add-missed-security-features-to-Cooperla.patch
Patch0319: target-i386-add-PSCHANGE_NO-bit-for-the-ARCH_CAPABIL.patch
Patch0320: target-i386-Export-TAA_NO-bit-to-guests.patch
Patch0321: usbredir-fix-free-call.patch
Patch0322: hw-arm-virt-Init-PMU-for-hotplugged-vCPU.patch
Patch0323: uas-add-stream-number-sanity-checks.patch
Patch0324: virtio-net-fix-use-after-unmap-free-for-sg.patch
Patch0325: Add-mtod_check.patch
Patch0326: bootp-limit-vendor-specific-area-to-input-packet-mem.patch
Patch0327: bootp-check-bootp_input-buffer-size.patch
Patch0328: upd6-check-udp6_input-buffer-size.patch
Patch0329: tftp-check-tftp_input-buffer-size.patch
Patch0330: tftp-introduce-a-header-structure.patch
Patch0331: fix-cve-2020-35504.patch
Patch0332: fix-cve-2020-35505.patch
Patch0333: virtio-balloon-apply-upstream-patch.patch
Patch0334: add-Phytium-s-CPU-models-FT-2000-and-Tengyun-S2500.patch
Patch0335: hw-scsi-scsi-disk-MODE_PAGE_ALLS-not-allowed-in-MODE.patch
Patch0336: hw-rdma-Fix-possible-mremap-overflow-in-the-pvrdma-d.patch
Patch0337: pvrdma-Ensure-correct-input-on-ring-init-CVE-2021-36.patch
Patch0338: pvrdma-Fix-the-ring-init-error-flow-CVE-2021-3608.patch
Patch0339: vhost-vsock-detach-the-virqueue-element-in-case-of-e.patch
Patch0340: virtio-net-fix-map-leaking-on-error-during-receive.patch
Patch0341: hw-block-fdc-Extract-blk_create_empty_drive.patch
Patch0342: hw-block-fdc-Kludge-missing-floppy-drive-to-fix-CVE-.patch
Patch0343: tests-fdc-test-Add-a-regression-test-for-CVE-2021-20.patch
Patch0344: display-qxl-render-fix-race-condition-in-qxl_cursor-.patch
Patch0345: ui-cursor-fix-integer-overflow-in-cursor_alloc-CVE-2.patch
Patch0346: hw-intc-arm_gicv3_dist-Rename-64-bit-accessors-with-.patch
Patch0347: hw-intc-arm_gicv3-Replace-mis-used-MEMTX_-constants-.patch
Patch0348: hw-intc-arm_gicv3-Check-for-MEMTX_OK-instead-of-MEMT.patch
Patch0349: net-colo-compare.c-Check-that-colo-compare-is-active.patch
Patch0350: e1000-fail-early-for-evil-descriptor.patch
Patch0351: e1000-fix-tx-re-entrancy-problem.patch
Patch0352: hw-sd-sdcard-Restrict-Class-6-commands-to-SCSD-cards.patch
Patch0353: hw-sd-sdcard-Simplify-realize-a-bit.patch
Patch0354: hw-sd-sdcard-Do-not-allow-invalid-SD-card-sizes.patch
Patch0355: hw-sd-sdcard-Update-coding-style-to-make-checkpatch..patch
Patch0356: hw-sd-sdcard-Do-not-switch-to-ReceivingData-if-addre.patch
Patch0357: scsi-qemu-pr-helper-Fix-out-of-bounds-access-to-trnp.patch
Patch0358: curses-Fixes-curses-compiling-errors.patch
Patch0359: net-dump.c-Suppress-spurious-compiler-warning.patch
Patch0360: tests-Replace-deprecated-ASN1-code.patch
Patch0361: hw-block-fdc-Prevent-end-of-track-overrun-CVE-2021-3.patch
Patch0362: softmmu-Always-initialize-xlat-in-address_space_tran.patch
Patch0363: hw-scsi-lsi53c895a-Do-not-abort-when-DMA-requested-a.patch
Patch0364: scsi-lsi53c895a-fix-use-after-free-in-lsi_do_msgout-.patch
Patch0365: scsi-lsi53c895a-really-fix-use-after-free-in-lsi_do_.patch
Patch0366: hw-usb-hcd-xhci-Fix-unbounded-loop-in-xhci_ring_chai.patch
Patch0367: hw-display-ati_2d-Fix-buffer-overflow-in-ati_2d_blt-.patch
Patch0368: hw-display-qxl-Have-qxl_log_command-Return-early-if-.patch
Patch0369: hw-display-qxl-Document-qxl_phys2virt.patch
Patch0370: hw-display-qxl-Pass-requested-buffer-size-to-qxl_phy.patch
Patch0371: hw-display-qxl-Avoid-buffer-overrun-in-qxl_phys2virt.patch
Patch0372: hw-display-qxl-Assert-memory-slot-fits-in-preallocat.patch
Patch0373: hw-pvrdma-Protect-against-buggy-or-malicious-guest-d.patch

BuildRequires: flex
BuildRequires: bison
BuildRequires: texinfo
BuildRequires: perl-podlators
BuildRequires: kernel
BuildRequires: chrpath
BuildRequires: gettext
BuildRequires: python-sphinx

BuildRequires: zlib-devel
BuildRequires: gtk3-devel
BuildRequires: gnutls-devel
BuildRequires: numactl-devel
BuildRequires: device-mapper-multipath-devel
BuildRequires: rdma-core-devel
BuildRequires: libcap-devel
BuildRequires: libcap-ng-devel
BuildRequires: cyrus-sasl-devel
BuildRequires: libaio-devel
BuildRequires: usbredir-devel >= 0.5.2
BuildRequires: libseccomp-devel >= 2.3.0
BuildRequires: systemd-devel
BuildRequires: libiscsi-devel
BuildRequires: snappy-devel
BuildRequires: lzo-devel
BuildRequires: ncurses-devel
BuildRequires: libattr-devel
BuildRequires: libcurl-devel
BuildRequires: libjpeg-devel
BuildRequires: libpng-devel
BuildRequires: brlapi-devel
BuildRequires: pixman-devel
BuildRequires: libusbx-devel
BuildRequires: bzip2-devel
BuildRequires: libepoxy-devel
BuildRequires: libtasn1-devel
BuildRequires: libxml2-devel
BuildRequires: libudev-devel
BuildRequires: pam-devel
BuildRequires: perl-Test-Harness
BuildRequires: python3-devel
BuildRequires: librbd-devel
BuildRequires: krb5-devel
BuildRequires: libssh-devel
BuildRequires: glib2
%ifarch aarch64
BuildRequires: libfdt-devel
BuildRequires: virglrenderer-devel
%endif

# for upgrade from qemu-kvm
Provides: qemu-kvm
Obsoletes: qemu-kvm < 10:4.1.0

Requires(post): /usr/bin/getent
Requires(post): /usr/sbin/groupadd
Requires(post): /usr/sbin/useradd
Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units
Requires(postun): qemu-block-iscsi
Requires(postun): qemu-block-curl

%description
QEMU is a FAST! processor emulator using dynamic translation to achieve good emulation speed.

QEMU has two operating modes:

   Full system emulation. In this mode, QEMU emulates a full system (for example a PC),
   including one or several processors and various peripherals. It can be used to launch
   different Operating Systems without rebooting the PC or to debug system code.

   User mode emulation. In this mode, QEMU can launch processes compiled for one CPU on another CPU.
   It can be used to launch the Wine Windows API emulator (https://www.winehq.org) or to ease
   cross-compilation and cross-debugging.
You can refer to https://www.qemu.org for more infortmation.

%package guest-agent
Summary: QEMU guest agent
Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units
%description guest-agent
This package provides an agent to run inside guests, which communicates
with the host over a virtio-serial channel named "org.qemu.guest_agent.0"
Please refer to https://wiki.qemu.org/Features/GuestAgent for more information.

%package help
Summary: Documents for qemu
Buildarch: noarch
%description help
This package provides documents for qemu related man help and information.

%package  img
Summary: QEMU command line tool for manipulating disk images
%description img
This package provides a command line tool for manipulating disk images

%package  block-rbd
Summary: Qemu-block-rbd
%description block-rbd
This package provides RBD support for Qemu

%package  block-ssh
Summary: Qemu-block-ssh
%description block-ssh
This package provides block-ssh support for Qemu

%package  block-iscsi
Summary: Qemu-block-iscsi
%description block-iscsi
This package provides block-iscsi support for Qemu

%package  block-curl
Summary: Qemu-block-curl
%description block-curl
This package provides block-curl support for Qemu

%ifarch %{ix86} x86_64
%package seabios
Summary: QEMU seabios
%description seabios
This package include bios-256k.bin and bios.bin of seabios
%endif

%prep
%setup -q -n qemu-%{version}%{?rcstr}
%autopatch -p1

%build
%ifarch x86_64
buildarch="x86_64-softmmu"
%endif
%ifarch aarch64
buildarch="aarch64-softmmu"
%endif

buildldflags="VL_LDFLAGS=-Wl,--build-id"

./configure \
    --prefix=%{_prefix}   \
    --target-list=${buildarch}     \
    --extra-cflags="%{optflags} -fPIE -DPIE -fPIC"    \
    --extra-ldflags="-Wl,--build-id -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack" \
    --datadir=%{_datadir} \
    --docdir=%{_docdir}/%{name}  \
    --libdir=%{_libdir}     \
    --libexecdir=%{_libexecdir} \
    --localstatedir=%{_localstatedir} \
    --sysconfdir=%{_sysconfdir} \
    --interp-prefix=%{_prefix}/qemu-%%M \
    --firmwarepath=%{_datadir}/%{name}    \
    --with-pkgversion=%{name}-%{version}-%{release} \
    --python=/usr/bin/python3 \
    --disable-strip \
    --disable-slirp  \
    --enable-gtk \
    --enable-docs \
    --enable-guest-agent \
    --enable-pie \
    --enable-numa \
    --enable-mpath \
    --disable-libnfs \
    --disable-bzip2 \
    --enable-kvm \
    --enable-tcg \
    --enable-rdma \
    --enable-linux-aio \
    --enable-cap-ng \
    --enable-vhost-user \
    --enable-modules \
    --enable-libssh \
%ifarch aarch64
    --enable-fdt \
    --enable-virglrenderer \
%endif
    --enable-cap-ng \
    --enable-libusb \
    --disable-bluez \
    --disable-dmg \
    --disable-qcow1 \
    --disable-vdi \
    --disable-vvfat \
    --disable-qed \
    --disable-parallels \
    --disable-sheepdog \
    --disable-capstone \
    --disable-smartcard

make %{?_smp_mflags} $buildldflags V=1

cp -a ${buildarch}/qemu-system-* qemu-kvm

%install

make %{?_smp_mflags} DESTDIR=%{buildroot} \
    install

%find_lang %{name}
install -m 0755 qemu-kvm  %{buildroot}%{_libexecdir}/
ln -s  %{_libexecdir}/qemu-kvm %{buildroot}/%{_bindir}/qemu-kvm

rm %{buildroot}/%{_bindir}/qemu-system-*
install -D -p -m 0644 contrib/systemd/qemu-pr-helper.service %{buildroot}%{_unitdir}/qemu-pr-helper.service
install -D -p -m 0644 contrib/systemd/qemu-pr-helper.socket %{buildroot}%{_unitdir}/qemu-pr-helper.socket
install -D -p -m 0644 qemu.sasl %{buildroot}%{_sysconfdir}/sasl2/qemu.conf
install -D -m 0644 %{_sourcedir}/bridge.conf %{buildroot}%{_sysconfdir}/qemu/bridge.conf
install -D -m 0644 %{_sourcedir}/80-kvm.rules %{buildroot}/usr/lib/udev/rules.d/80-kvm.rules

# For qemu-guest-agent package
%global _udevdir /lib/udev/rules.d
install -D -p -m 0644 contrib/systemd/qemu-guest-agent.service %{buildroot}%{_unitdir}/qemu-guest-agent.service
install -D -m 0644 %{_sourcedir}/99-qemu-guest-agent.rules %{buildroot}%{_udevdir}/99-qemu-guest-agent.rules
mkdir -p %{buildroot}%{_localstatedir}/log
touch %{buildroot}%{_localstatedir}/log/qga-fsfreeze-hook.log

# For qemu docs package
%global qemudocdir %{_docdir}/%{name}
rm -rf %{buildroot}%{qemudocdir}/specs
install -D -p -m 0644 -t %{buildroot}%{qemudocdir} Changelog README COPYING COPYING.LIB LICENSE
chmod -x %{buildroot}%{_mandir}/man1/*


%ifarch aarch64
rm -rf %{buildroot}%{_datadir}/%{name}/vgabios*bin
rm -rf %{buildroot}%{_datadir}/%{name}/bios*.bin
rm -rf %{buildroot}%{_datadir}/%{name}/linuxboot.bin
rm -rf %{buildroot}%{_datadir}/%{name}/kvmvapic.bin
rm -rf %{buildroot}%{_datadir}/%{name}/sgabios.bin
rm -rf %{buildroot}%{_datadir}/%{name}/multiboot.bin
rm -rf %{buildroot}%{_datadir}/%{name}/linuxboot_dma.bin
rm -rf %{buildroot}%{_datadir}/%{name}/pvh.bin
%endif
%ifarch x86_64
rm -rf %{buildroot}%{_datadir}/%{name}/vgabios-ati.bin
%endif
rm -rf %{buildroot}%{_datadir}/%{name}/openbios-*
rm -rf %{buildroot}%{_datadir}/%{name}/slof.bin
rm -rf %{buildroot}%{_datadir}/%{name}/QEMU,*.bin
rm -rf %{buildroot}%{_datadir}/%{name}/bamboo.dtb
rm -rf %{buildroot}%{_datadir}/%{name}/canyonlands.dtb
rm -rf %{buildroot}%{_datadir}/%{name}/hppa-firmware.img
rm -rf %{buildroot}%{_datadir}/%{name}/palcode-clipper
rm -rf %{buildroot}%{_datadir}/%{name}/petalogix-*
rm -rf %{buildroot}%{_datadir}/%{name}/ppc_*
rm -rf %{buildroot}%{_datadir}/%{name}/qemu_vga.ndrv
rm -rf %{buildroot}%{_datadir}/%{name}/s390-*
rm -rf %{buildroot}%{_datadir}/%{name}/skiboot.lid
rm -rf %{buildroot}%{_datadir}/%{name}/spapr-*
rm -rf %{buildroot}%{_datadir}/%{name}/u-boot*
rm -rf %{buildroot}%{_bindir}/ivshmem*
rm -f %{buildroot}%{_datadir}/%{name}/edk2*
rm -rf %{buildroot}%{_datadir}/%{name}/firmware
rm -rf %{buildroot}%{_datadir}/%{name}/opensbi*
rm -rf %{buildroot}%{_datadir}/%{name}/qemu-nsis.bmp
rm -rf %{buildroot}%{_libdir}/%{name}/audio-oss.so
rm -rf %{buildroot}%{_libdir}/%{name}/audio-pa.so
rm -rf %{buildroot}%{_libdir}/%{name}/block-gluster.so
rm -rf %{buildroot}%{_libdir}/%{name}/ui-curses.so
rm -rf %{buildroot}%{_libdir}/%{name}/ui-gtk.so
rm -rf %{buildroot}%{_libdir}/%{name}/ui-sdl.so
rm -rf %{buildroot}%{_libexecdir}/vhost-user-gpu
rm -rf %{buildroot}%{_datadir}/%{name}/vhost-user/50-qemu-gpu.json

strip %{buildroot}%{_libdir}/%{name}/block-rbd.so
strip %{buildroot}%{_libdir}/%{name}/block-iscsi.so
strip %{buildroot}%{_libdir}/%{name}/block-curl.so
strip %{buildroot}%{_libdir}/%{name}/block-ssh.so

for f in %{buildroot}%{_bindir}/* %{buildroot}%{_libdir}/* \
         %{buildroot}%{_libexecdir}/*; do
  if file $f | grep -q ELF | grep -q -i shared; then chrpath --delete $f; fi
done

%check
make check V=1

%pre
getent group kvm >/dev/null || groupadd -g 36 -r kvm
getent group qemu >/dev/null || groupadd -g 107 -r qemu
getent passwd qemu >/dev/null || \
  useradd -r -u 107 -g qemu -G kvm -d / -s /sbin/nologin \
    -c "qemu user" qemu

%post guest-agent
%systemd_post qemu-guest-agent.service
%preun guest-agent
%systemd_preun qemu-guest-agent.service
%postun guest-agent
%systemd_postun_with_restart qemu-guest-agent.service

%files  -f %{name}.lang
%dir %{_datadir}/%{name}/
%{_libexecdir}/qemu-kvm
%{_bindir}/qemu-kvm
%{_datadir}/%{name}/efi-virtio.rom
%{_datadir}/%{name}/efi-e1000.rom
%{_datadir}/%{name}/efi-e1000e.rom
%{_datadir}/%{name}/efi-rtl8139.rom
%{_datadir}/%{name}/efi-pcnet.rom
%{_datadir}/%{name}/efi-ne2k_pci.rom
%{_datadir}/%{name}/efi-eepro100.rom
%{_datadir}/%{name}/efi-vmxnet3.rom
%{_datadir}/%{name}/pxe-virtio.rom
%{_datadir}/%{name}/pxe-e1000.rom
%{_datadir}/%{name}/pxe-ne2k_pci.rom
%{_datadir}/%{name}/pxe-pcnet.rom
%{_datadir}/%{name}/pxe-rtl8139.rom
%{_datadir}/%{name}/pxe-eepro100.rom
%{_datadir}/%{name}/trace-events-all
%{_datadir}/applications/qemu.desktop
%{_datadir}/icons/hicolor/*/apps/*
%{_datadir}/%{name}/keymaps/
%{_bindir}/elf2dmp
%{_bindir}/qemu-edid
%{_bindir}/qemu-keymap
%{_bindir}/qemu-pr-helper
%{_bindir}/virtfs-proxy-helper
%{_unitdir}/qemu-pr-helper.service
%{_unitdir}/qemu-pr-helper.socket
%attr(4755, root, root) %{_libexecdir}/qemu-bridge-helper
%config(noreplace) %{_sysconfdir}/sasl2/qemu.conf
%dir %{_sysconfdir}/qemu
%config(noreplace) %{_sysconfdir}/qemu/bridge.conf
/usr/lib/udev/rules.d/80-kvm.rules
%doc %{qemudocdir}/COPYING
%doc %{qemudocdir}/COPYING.LIB
%doc %{qemudocdir}/LICENSE
%ifarch x86_64
%{_datadir}/%{name}/bios.bin
%{_datadir}/%{name}/bios-256k.bin
%{_datadir}/%{name}/vgabios.bin
%{_datadir}/%{name}/vgabios-cirrus.bin
%{_datadir}/%{name}/vgabios-qxl.bin
%{_datadir}/%{name}/vgabios-stdvga.bin
%{_datadir}/%{name}/vgabios-vmware.bin
%{_datadir}/%{name}/vgabios-virtio.bin
%{_datadir}/%{name}/vgabios-ramfb.bin
%{_datadir}/%{name}/vgabios-bochs-display.bin
%{_datadir}/%{name}/linuxboot.bin
%{_datadir}/%{name}/linuxboot_dma.bin
%{_datadir}/%{name}/pvh.bin
%{_datadir}/%{name}/multiboot.bin
%{_datadir}/%{name}/kvmvapic.bin
%{_datadir}/%{name}/sgabios.bin
%endif

%files help
%dir %{qemudocdir}
%doc %{qemudocdir}/qemu-doc.html
%doc %{qemudocdir}/qemu-doc.txt
%doc %{qemudocdir}/qemu-ga-ref.html
%doc %{qemudocdir}/qemu-ga-ref.txt
%doc %{qemudocdir}/qemu-qmp-ref.html
%doc %{qemudocdir}/qemu-qmp-ref.txt
%doc %{qemudocdir}/interop
%doc %{qemudocdir}/README
%doc %{qemudocdir}/Changelog
%{_mandir}/man1/qemu.1*
%{_mandir}/man1/virtfs-proxy-helper.1*
%{_mandir}/man7/qemu-block-drivers.7*
%{_mandir}/man7/qemu-cpu-models.7*
%{_mandir}/man7/qemu-ga-ref.7*
%{_mandir}/man7/qemu-qmp-ref.7*
%{_mandir}/man1/qemu-img.1*
%{_mandir}/man8/qemu-nbd.8*

%files guest-agent
%defattr(-,root,root,-)
%{_bindir}/qemu-ga
%{_mandir}/man8/qemu-ga.8*
%{_unitdir}/qemu-guest-agent.service
%{_udevdir}/99-qemu-guest-agent.rules
%ghost %{_localstatedir}/log/qga-fsfreeze-hook.log

%files img
%{_bindir}/qemu-img
%{_bindir}/qemu-io
%{_bindir}/qemu-nbd

%files block-rbd
%{_libdir}/%{name}/block-rbd.so

%files block-ssh
%{_libdir}/%{name}/block-ssh.so

%files block-iscsi
%{_libdir}/%{name}/block-iscsi.so

%files block-curl
%{_libdir}/%{name}/block-curl.so

%ifarch %{ix86} x86_64
%files seabios
%{_datadir}/%{name}/bios-256k.bin
%{_datadir}/%{name}/bios.bin
%endif

%changelog
* Thu May 18 2023 liuxiangdong <liuxiangdong5@huawei.com>
- hw/pvrdma: Protect against buggy or malicious guest driver (CVE-2022-1050)

* Mon Dec 05 2022 yezengruan <yezengruan@huawei.com>
- fix CVE-2022-4144

* Fri Sep 30 2022 yezengruan <yezengruan@huawei.com>
- hw/display/ati_2d: Fix buffer overflow in ati_2d_blt (CVE-2021-3638)

* Wed Sep 07 2022 yezengruan <yezengruan@huawei.com>
- hw/usb/hcd-xhci: Fix unbounded loop in xhci_ring_chain_length() (CVE-2020-14394)

* Tue Aug 30 2022 yezengruan <yezengruan@huawei.com>
- hw/scsi/lsi53c895a: Do not abort when DMA requested and no data queued
- scsi/lsi53c895a: fix use-after-free in lsi_do_msgout (CVE-2022-0216)
- scsi/lsi53c895a: really fix use-after-free in lsi_do_msgout (CVE-2022-0216)

* Thu Aug 25 2022 yezengruan <yezengruan@huawei.com>
- Provides qemu-kvm for upgrade

* Tue Jul 19 2022 yezengruan <yezengruan@huawei.com>
- softmmu: Always initialize xlat in address_space_translate_for_iotlb (CVE-2022-35414)

* Thu Jun 02 2022 yezengruan <yezengruan@huawei.com>
- hw/block/fdc: Prevent end-of-track overrun (CVE-2021-3507)

* Sat May 28 2022 sundongxu <sundongxu3@huawei.com>
- e1000: fail early for evil descriptor
- e1000: fix tx re-entrancy problem
- hw/sd/sdcard: Do not allow invalid SD card sizes
- hw/sd/sdcard: Do not switch to ReceivingData if address is invalid
- hw/sd/sdcard: Restrict Class 6 commands to SCSD cards
- hw/sd/sdcard: Simplify realize() a bit
- hw/sd/sdcard: Update coding style to make checkpatch.pl happy
- scsi/qemu-pr-helper: Fix out-of-bounds access to trnptid_list[]
- curses: Fixes curses compiling errors.
- net/dump.c: Suppress spurious compiler warning
- tests: Replace deprecated ASN1 code

* Sat May 21 2022 yezengruan <yezengruan@huawei.com>
- hw/intc/arm_gicv3_dist: Rename 64-bit accessors with 'q' suffix
- hw/intc/arm_gicv3: Replace mis-used MEMTX_* constants by booleans
- hw/intc/arm_gicv3: Check for !MEMTX_OK instead of MEMTX_ERROR (CVE-2021-3750)
- net/colo-compare.c: Check that colo-compare is active

* Tue May 10 2022 yezengruan <yezengruan@huawei.com>
- hw/block/fdc: Extract blk_create_empty_drive()
- hw/block/fdc: Kludge missing floppy drive to fix CVE-2021-20196
- tests/fdc-test: Add a regression test for CVE-2021-20196
- display/qxl-render: fix race condition in qxl_cursor (CVE-2021-4207)
- ui/cursor: fix integer overflow in cursor_alloc (CVE-2021-4206)

* Fri Apr 15 2022 yezengruan <yezengruan@huawei.com>
- vhost-vsock: detach the virqueue element in case of error (CVE-2022-26354)
- virtio-net: fix map leaking on error during receive (CVE-2022-26353)

* Wed Apr 06 2022 yezengruan <yezengruan@huawei.com>
- hw/rdma: Fix possible mremap overflow in the pvrdma device (CVE-2021-3582)
- pvrdma: Ensure correct input on ring init (CVE-2021-3607)
- pvrdma: Fix the ring init error flow (CVE-2021-3608)

* Mon Mar 28 2022 Jinhao Gao <gaojinhao@huawei.com>
- hw/scsi/scsi-disk: MODE_PAGE_ALLS not allowed in MODE SELECT commands(fix CVE-2021-3930)

* Tue Dec 21 2021 imxcc <xingchaochao@huawei.com>
- add Phytium's CPU models: FT-2000+ and Tengyun-S2500

* Fri Dec 03 2021 Chen Qun <kuhn.chenqun@huawei.com>
- virtio-balloon: apply upstream patch.

* Wed Oct 27 2021 Chen Qun <kuhn.chenqun@huawei.com>
- fix cve-2020-35504
- fix cve-2020-35505

* Tue Oct 19 2021 imxcc <xingchaochao@huawei.com>
- fix cve-2021-3592 cve-2021-3593 cve-2021-3595

* Sun Sep 26 2021 Chen Qun <kuhn.chenqun@huawei.com>
- virtio-net: fix use after unmap/free for sg

* Wed Sep 15 2021 Chen Qun <kuhn.chenqun@huawei.com>
- uas: add stream number sanity checks.

* Tue Aug 31 2021 imxcc <xingchaochao@huawei.com>
- hw/arm/virt:Init PMU for hotplugged vCPU

* Thu Aug 19 2021 Jiajie Li <lijiajie11@huawei.com>
- add qemu-block-curl package
- add qemu-block-curl requirement for qemu.

* Mon Aug 16 2021 Chen Qun <kuhn.chenqun@huawei.com>
- usbredir: fix free call

* Fri Jul 16 2021 Chen Qun <kuhn.chenqun@huawei.com>
- x86: Intel AVX512_BF16 feature enabling
- i386: Add MSR feature bit for MDS-NO
- i386: Add macro for stibp
- i386: Add new CPU model Cooperlake
- target/i386: Add new bit definitions of MSR_IA32_ARCH_CAPABILITIES
- target/i386: Add missed security features to Cooperlake CPU model
- target/i386: add PSCHANGE_NO bit for the ARCH_CAPABILITIES MSR
- target/i386: Export TAA_NO bit to guests

* Tue Jul 13 2021 Chen Qun <kuhn.chenqun@huawei.com>
- hw/net/rocker_of_dpa: fix double free bug of rocker device

* Mon Jun 21 2021 Chen Qun <kuhn.chenqun@huawei.com>
- ide: ahci: add check to avoid null dereference (CVE-2019-12067)
- hw/intc/arm_gic: Fix interrupt ID in GICD_SGIR register
- usb: limit combined packets to 1 MiB (CVE-2021-3527)

* Tue Jun 15 2021 Chen Qun <kuhn.chenqun@huawei.com>
- vhost-user-gpu: fix resource leak in 'vg_resource_create_2d' (CVE-2021-3544)
- vhost-user-gpu: fix memory leak in vg_resource_attach_backing (CVE-2021-3544)
- vhost-user-gpu: fix memory leak while calling 'vg_resource_unref' (CVE-2021-3544)
- vhost-user-gpu: fix memory leak in 'virgl_cmd_resource_unref' (CVE-2021-3544)
- vhost-user-gpu: fix memory leak in 'virgl_resource_attach_backing' (CVE-2021-3544)
- vhost-user-gpu: fix memory disclosure in virgl_cmd_get_capset_info (CVE-2021-3545)
- vhost-user-gpu: fix OOB write in 'virgl_cmd_get_capset' (CVE-2021-3546)

* Tue Jun 08 2021 Chen Qun <kuhn.chenqun@huawei.com>
- 9pfs: Fully restart unreclaim loop (CVE-2021-20181)

* Wed Jun 02 2021 imxcc <xingchaochao@huawei.com>
- add strip for block-iscsi.so, block-rbd.so and block-ssh.so

* Wed Jun 02 2021 Chen Qun <kuhn.chenqun@huawei.com>
- bugfix: fix Uninitialized Free Vulnerability

* Tue Jun 01 2021 Chen Qun <kuhn.chenqun@huawei.com>
- hw/pci-host: add pci-intack write method
- pci-host: add pcie-msi read method
- vfio: add quirk device write method
- prep: add ppc-parity write method
- nvram: add nrf51_soc flash read method
- spapr_pci: add spapr msi read method
- tz-ppc: add dummy read/write methods
- imx7-ccm: add digprog mmio write method

* Thu May 20 2021 Chen Qun <kuhn.chenqun@huawei.com>
- hw/sd: sdhci: Don't transfer any data when command time out
- hw/sd: sdhci: Don't write to SDHC_SYSAD register when transfer is in progress
- hw/sd: sdhci: Correctly set the controller status for ADMA
- hw/sd: sdhci: Limit block size only when SDHC_BLKSIZE register is writable
- hw/sd: sdhci: Reset the data pointer of s->fifo_buffer[] when a different block size is programmed
- net: introduce qemu_receive_packet()
- e1000: switch to use qemu_receive_packet() for loopback
- dp8393x: switch to use qemu_receive_packet() for loopback packet
- sungem: switch to use qemu_receive_packet() for loopback
- tx_pkt: switch to use qemu_receive_packet_iov() for loopback
- rtl8139: switch to use qemu_receive_packet() for loopback
- pcnet: switch to use qemu_receive_packet() for loopback
- cadence_gem: switch to use qemu_receive_packet() for loopback
- lan9118: switch to use qemu_receive_packet() for loopback

* Wed May 19 2021 Chen Qun <kuhn.chenqun@huawei.com>
- scsi: mptsas: dequeue request object in case of an error

* Tue May 11 2021 Chen Qun <kuhn.chenqun@huawei.com>
- arm/cpu: Fixed function undefined error at compile time under arm

* Tue May 11 2021 Ming Yang <yangming73@huawei.com>
- add qemu-block-iscsi installing requirement

* Sun Apr 25 2021 Chuan Zheng <zhengchuan@huawei.com>
- migration/dirtyrate: add dirtyrate fearure for migration
- migration/multifd-tls: add multifd for tls migration

* Sun Apr 25 2021 imxcc <xingchaochao@huawei.com>
- hw/acpi: build smt processor structure to support smt topology
- hw/acpi: Use max_cpus instead of cpus when build PPTT

* Sun Apr 25 2021 Chen Qun <kuhn.chenqun@huawei.com>
- scsi-bus: Refactor the code that retries requests
- scsi-disk: Add support for retry on errors
- qapi/block-core: Add retry option for error action
- block-backend: Introduce retry timer
- block-backend: Add device specific retry callback
- block-backend: Enable retry action on errors
- block-backend: Add timeout support for retry
- block: Add error retry param setting
- virtio-blk: Refactor the code that processes queued requests
- virtio-blk: On restart, process queued requests in the proper context
- virtio_blk: Add support for retry on errors
- block-backend: Stop retrying when draining
- block: Add sanity check when setting retry parameters

* Sat Apr 17 2021 Chuan Zheng <zhengchuan@huawei.com>
- dirtyrate: add migration dirtyrate feature

* Fri Apr 16 2021 Huawei Technologies Co., Ltd <yangming73@huawei.com>
- add qemu-block-rbd package
- add qemu-block-ssh package

* Thu Mar 18 2021 Chen Qun <kuhn.chenqun@huawei.com>
- net: vmxnet3: validate configuration values during activate (CVE-2021-20203)

* Fri Feb 26 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- ide:atapi: check io_buffer_index in ide_atapi_cmd_reply_end

* Fri Feb 19 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- ati: use vga_read_byte in ati_cursor_define
- sd: sdhci: assert data_count is within fifo_buffer
- msix: add valid.accepts methods to check address

* Mon Jan 18 2021 Huawei Technologies Co., Ltd <fangying1@huawei.com>
- reorder the changelog

* Fri Jan 15 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- memory: clamp cached translation in case it points to an MMIO region

* Fri Dec 11 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- slirp: check pkt_len before reading protocol header for fixing CVE-2020-29129 and CVE-2020-29130

* Fri Dec 11 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- hostmem: Fix up free host_nodes list right after visited

* Wed Dec 9 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- target/arm: Fix write redundant values to kvm

* Wed Nov 18 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- ati: check x y display parameter values

* Fri Nov 13 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- json: Fix a memleak in parse_pair()

* Wed Nov 11 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- hw: usb: hcd-ohci: check for processed TD before retire
- hw: ehci: check return value of 'usb_packet_map'
- hw: usb: hcd-ohci: check len and frame_number variables
- hw/net/e1000e: advance desc_offset in case of null descriptor

* Wed Oct 21 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- net: remove an assert call in eth_get_gso_type

* Wed Oct 14 2020 Prasad J Pandit <pjp@fedoraproject.org>
- pci: check bus pointer before dereference
- hw/ide: check null block before _cancel_dma_sync

* Thu Sep 24 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- enrich commit info for some patches
- rename some patches for slirp

* Mon Sep 21 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- target/arm: Add isar_feature tests for PAN + ATS1E1
- target/arm: Add ID_AA64MMFR2_EL1
- target/arm: Add and use FIELD definitions for ID_AA64DFR0_EL1
- target/arm: Use FIELD macros for clearing ID_DFR0 PERFMON field
- target/arm: Define an aa32_pmu_8_1 isar feature test function
- target/arm: Add _aa64_ and _any_ versions of pmu_8_1 isar checks
- target/arm: Stop assuming DBGDIDR always exists
- target/arm: Move DBGDIDR into ARMISARegisters
- target/arm: Enable ARMv8.2-ATS1E1 in -cpu max
- target/arm: Test correct register in aa32_pan and aa32_ats1e1 checks
- target/arm: Read debug-related ID registers from KVM
- target/arm/monitor: Introduce qmp_query_cpu_model_expansion
- target/arm/monitor: query-cpu-model-expansion crashed qemu when using machine type none
- target/arm: convert isar regs to array
- target/arm: parse cpu feature related options
- target/arm: register CPU features for property
- target/arm: Allow ID registers to synchronize to KVM
- target/arm: introduce CPU feature dependency mechanism
- target/arm: introduce KVM_CAP_ARM_CPU_FEATURE
- target/arm: Add CPU features to query-cpu-model-expansion
- target/arm: Update ID fields
- target/arm: Add more CPU features
- target/arm: ignore evtstrm and cpuid CPU features
- target/arm: Update the ID registers of Kunpeng-920
- target/arm: only set ID_PFR1_EL1.GIC for AArch32 guest
- target/arm: clear EL2 and EL3 only when kvm is not enabled

* Fri Sep 18 2020 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- hw-sd-sdhci-Fix-DMA-Transfer-Block-Size-field.patch
- hw-xhci-check-return-value-of-usb_packet_map.patch

* Fri Sep 11 2020 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- slirp/src/ip6_input.c: fix out-of-bounds read information vulnerablity

* Thu Aug 27 2020 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- hw/usb/core.c: fix buffer overflow in do_token_setup function

* Wed Aug 12 2020 Huawei Technologies Co., Ltd <jinzeyu@huawei.com>
- backport upstream patch to support SHPCHotplug in arm

* Thu Aug 6 2020 Huawei Technologies Co., Ltd <fangying1@huawei.com>
- tests: Disalbe filemonitor testcase

* Fri Jul 24 2020 Huawei Technologies Co., Ltd <zhang.zhanghailiang@huawei.com>
- es1370: check total frame count against current frame
- exec: set map length to zero when returning NULL
- ati-vga: check mm_index before recursive call (CVE-2020-13800)
- megasas: use unsigned type for reply_queue_head and check index
- megasas: avoid NULL pointer dereference
- megasas: use unsigned type for positive numeric fields
- hw/scsi/megasas: Fix possible out-of-bounds array access in tracepoints

* Sat Jun 20 2020 Huawei Technologies Co., Ltd <zhang.zhanghailiang@huawei.com>
- target/arm: Fix PAuth sbox functions
- fix two patches' format which can cause git am failed

* Fri May 29 2020 Huawei Technologies Co., Ltd <fangying1@huawei.com>
- target/arm: Add the kvm_adjvtime vcpu property for Cortex-A72

* Wed May 27 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- Revert: "vtimer: compat cross version migration from v4.0.1"
- ARM64: record vtimer tick when cpu is stopped
- hw/arm/virt: add missing compat for kvm-no-adjvtime
- migration: Compat virtual timer adjust for v4.0.1 and v4.1.0
- vtimer: Drop vtimer virtual timer adjust

* Fri May 22 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- ip_reass: Fix use after free
- bt: use size_t type for length parameters instead of int
- log: Add some logs on VM runtime path

* Thu May 21 2020 BALATON Zoltan <balaton@eik.bme.hu>
- hw/net/xgmac: Fix buffer overflow in xgmac_enet_send()
- hw/net/net_tx_pkt: fix assertion failure in net_tx_pkt_add_raw_fragment()
- sm501: Convert printf + abort to qemu_log_mask
- sm501: Shorten long variable names in sm501_2d_operation
- sm501: Use BIT(x) macro to shorten constant
- sm501: Clean up local variables in sm501_2d_operation
- sm501: Replace hand written implementation with pixman where possible

* Fri May 15 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- ide: Fix incorrect handling of some PRDTs in ide_dma_cb()
- ati-vga: Fix checks in ati_2d_blt() to avoid crash
- slirp: tftp: restrict relative path access

* Tue May 12 2020 Huawei Technologies Co., Ltd. <zhukeqian1@huawei.com>
- arm/virt: Support CPU cold plug

* Sat May 9 2020 Huawei Technologies Co., Ltd. <pannengyuan@huawei.com>
- migration/ram: do error_free after migrate_set_error to avoid memleaks.
- migration/ram: fix memleaks in multifd_new_send_channel_async.
- migration/rdma: fix a memleak on error path in rdma_start_incoming_migration.

* Fri May 8 2020 Huawei Technologies Co., Ltd. <zhengchuan@huawei.com>
- vtimer: compat cross version migration from v4.0.1

* Fri Apr 24 2020 Huawei Technologies Co., Ltd. <zhengchuan@huawei.com>
- migration: backport migration patches from upstream

* Fri Apr 24 2020 Huawei Technologies Co., Ltd. <zhukeqian1@huawei.com>
- arm/virt: Add CPU hotplug support

* Wed Apr 22 2020 Huawei Technologies Co., Ltd. <zhukeqian1@huawei.com>
- backport patch to enable arm/virt memory hotplug

* Wed Apr 22 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- backport patch to enable target/arm/kvm Adjust virtual time

* Fri Apr 17 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- backport patch bundles from qemu stable v4.1.1

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- aio-wait: delegate polling of main AioContext if BQL not held
- async: use explicit memory barriers

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- pcie: Add pcie-root-port fast plug/unplug feature
- pcie: Compat with devices which do not support Link Width, such as ioh3420

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- qcow2-bitmap: Fix uint64_t left-shift overflow

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- COLO-compare: Fix incorrect `if` logic

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- block/backup: fix max_transfer handling for copy_range
- block/backup: fix backup_cow_with_offload for last cluster
- qcow2: Limit total allocation range to INT_MAX
- mirror: Do not dereference invalid pointers

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- file-posix: Handle undetectable alignment

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- vhost: Fix memory region section comparison
- memory: Provide an equality function for MemoryRegionSections
- memory: Align MemoryRegionSections fields

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- block/iscsi: use MIN() between mx_sb_len and sb_len_wr
- moniter: fix memleak in monitor_fdset_dup_fd_find_remove

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- tcp_emu: fix unsafe snprintf() usages
- util: add slirp_fmt() helpers
- slirp: use correct size while emulating commands
- slirp: use correct size while emulating IRC commands
- tcp_emu: Fix oob access
- iscsi: Cap block count from GET LBA STATUS (CVE-2020-1711)

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- 9pfs: local: Fix possible memory leak in local_link()
- scsi-disk: define props in scsi_block_disk to avoid memleaks
- arm/translate-a64: fix uninitialized variable warning
- block: fix memleaks in bdrv_refresh_filename
- vnc: fix memory leak when vnc disconnect
- block: fix memleaks in bdrv_refresh_filename

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- linux headers: update against "KVM/ARM: Fix >256 vcpus"
- intc/arm_gic: Support IRQ injection for more than 256 vcpus
- ARM: KVM: Check KVM_CAP_ARM_IRQ_LINE_LAYOUT_2 for smp_cpus >

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- vnc: fix memory leak when vnc disconnect

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- pcie: disable the PCI_EXP_LINKSTA_DLLA cap for pcie-root-port by default

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- cpu: add Kunpeng-920 cpu support
- cpu: parse +/- feature to avoid failure
- cpu: add Cortex-A72 processor kvm target support

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- vhost-user-scsi: prevent using uninitialized vqs

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- util/async: hold AioContext ref to prevent use-after-free

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- xhci: Fix memory leak in xhci_address_slot
- xhci: Fix memory leak in xhci_kick_epctx
- ehci: fix queue->dev null ptr dereference

* Thu Apr 16 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- tests/bios-tables-test: disable this testcase
- hw/arm/virt: Introduce cpu topology support
- hw/arm64: add vcpu cache info support

* Wed Apr 15 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- smbios: Add missing member of type 4 for smbios 3.0

* Wed Apr 15 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- bios-tables-test: prepare to change ARM virt ACPI DSDT
- arm64: Add the cpufreq device to show cpufreq info to guest

* Wed Apr 15 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- qcow2: fix memory leak in qcow2_read_extensions

* Wed Apr 15 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- pl011: reset read FIFIO when UARTTIMSC=0 & UARTICR=0xff
- pl031: support rtc-timer property for pl031
- vhost: cancel migration when vhost-user restarted

* Mon Apr 13 2020 openEuler Buildteam <buildteam@openeuler.org> - version-release
- Package init
