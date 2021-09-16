Name: qemu
Version: 4.1.0
Release: 81
Epoch: 2
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
Patch0031: tcp_emu-Fix-oob-access.patch
Patch0032: slirp-use-correct-size-while-emulating-IRC-commands.patch
Patch0033: slirp-use-correct-size-while-emulating-commands.patch
Patch0034: util-add-slirp_fmt-helpers.patch
Patch0035: tcp_emu-fix-unsafe-snprintf-usages.patch
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
Patch0156: ip_reass-Fix-use-after-free.patch
Patch0157: bt-use-size_t-type-for-length-parameters-instead-of-.patch
Patch0158: log-Add-some-logs-on-VM-runtime-path.patch
Patch0159: Revert-vtimer-compat-cross-version-migration-from-v4.patch
Patch0160: ARM64-record-vtimer-tick-when-cpu-is-stopped.patch
Patch0161: hw-arm-virt-add-missing-compat-for-kvm-no-adjvtime.patch
Patch0162: migration-Compat-virtual-timer-adjust-for-v4.0.1-and.patch
Patch0163: vtimer-Drop-vtimer-virtual-timer-adjust.patch
Patch0164: target-arm-Add-the-kvm_adjvtime-vcpu-property-for-Co.patch
Patch0165: target-arm-Fix-PAuth-sbox-functions.patch
Patch0166: tests-Disalbe-filemonitor-testcase.patch
Patch0167: es1370-check-total-frame-count-against-current-frame.patch
Patch0168: exec-set-map-length-to-zero-when-returning-NULL.patch
Patch0169: ati-vga-check-mm_index-before-recursive-call-CVE-202.patch
Patch0170: megasas-use-unsigned-type-for-reply_queue_head-and-c.patch
Patch0171: megasas-avoid-NULL-pointer-dereference.patch
Patch0172: megasas-use-unsigned-type-for-positive-numeric-field.patch
Patch0173: hw-scsi-megasas-Fix-possible-out-of-bounds-array-acc.patch
Patch0174: hw-arm-acpi-enable-SHPC-native-hot-plug.patch
Patch0175: hw-tpm-rename-Error-parameter-to-more-common-errp.patch
Patch0176: tpm-ppi-page-align-PPI-RAM.patch
Patch0177: tpm-Move-tpm_tis_show_buffer-to-tpm_util.c.patch
Patch0178: spapr-Implement-get_dt_compatible-callback.patch
Patch0179: delete-the-in-tpm.txt.patch
Patch0180: tpm_spapr-Support-TPM-for-ppc64-using-CRQ-based-inte.patch
Patch0181: tpm_spapr-Support-suspend-and-resume.patch
Patch0182: hw-ppc-Kconfig-Enable-TPM_SPAPR-as-part-of-PSERIES-c.patch
Patch0183: docs-specs-tpm-reST-ify-TPM-documentation.patch
Patch0184: tpm-rename-TPM_TIS-into-TPM_TIS_ISA.patch
Patch0185: tpm-Use-TPMState-as-a-common-struct.patch
Patch0186: tpm-Separate-tpm_tis-common-functions-from-isa-code.patch
Patch0187: tpm-Separate-TPM_TIS-and-TPM_TIS_ISA-configs.patch
Patch0188: tpm-Add-the-SysBus-TPM-TIS-device.patch
Patch0189: hw-arm-virt-vTPM-support.patch
Patch0190: docs-specs-tpm-Document-TPM_TIS-sysbus-device-for-AR.patch
Patch0191: test-tpm-pass-optional-machine-options-to-swtpm-test.patch
Patch0192: test-tpm-tis-Get-prepared-to-share-tests-between-ISA.patch
Patch0193: test-tpm-tis-Add-Sysbus-TPM-TIS-device-test.patch
Patch0194: build-smt-processor-structure-to-support-smt-topolog.patch
Patch0195: target-arm-Add-isar_feature-tests-for-PAN-ATS1E1.patch
Patch0196: target-arm-Add-ID_AA64MMFR2_EL1.patch
Patch0197: target-arm-Add-and-use-FIELD-definitions-for-ID_AA64.patch
Patch0198: target-arm-Use-FIELD-macros-for-clearing-ID_DFR0-PER.patch
Patch0199: target-arm-Define-an-aa32_pmu_8_1-isar-feature-test-.patch
Patch0200: target-arm-Add-_aa64_-and-_any_-versions-of-pmu_8_1-.patch
Patch0201: target-arm-Stop-assuming-DBGDIDR-always-exists.patch
Patch0202: target-arm-Move-DBGDIDR-into-ARMISARegisters.patch
Patch0203: target-arm-Enable-ARMv8.2-ATS1E1-in-cpu-max.patch
Patch0204: target-arm-Test-correct-register-in-aa32_pan-and-aa3.patch
Patch0205: target-arm-Read-debug-related-ID-registers-from-KVM.patch
Patch0206: target-arm-monitor-Introduce-qmp_query_cpu_model_exp.patch
Patch0207: target-arm-monitor-query-cpu-model-expansion-crashed.patch
Patch0208: target-arm-convert-isar-regs-to-array.patch
Patch0209: target-arm-parse-cpu-feature-related-options.patch
Patch0210: target-arm-register-CPU-features-for-property.patch
Patch0211: target-arm-Allow-ID-registers-to-synchronize-to-KVM.patch
Patch0212: target-arm-introduce-CPU-feature-dependency-mechanis.patch
Patch0213: target-arm-introduce-KVM_CAP_ARM_CPU_FEATURE.patch
Patch0214: target-arm-Add-CPU-features-to-query-cpu-model-expan.patch
Patch0215: target-arm-Update-ID-fields.patch
Patch0216: target-arm-Add-more-CPU-features.patch
Patch0217: hw-usb-core-fix-buffer-overflow.patch
Patch0218: target-arm-ignore-evtstrm-and-cpuid-CPU-features.patch
Patch0219: Drop-bogus-IPv6-messages.patch
Patch0220: hw-sd-sdhci-Fix-DMA-Transfer-Block-Size-field.patch
Patch0221: hw-xhci-check-return-value-of-usb_packet_map.patch
Patch0222: hw-net-xgmac-Fix-buffer-overflow-in-xgmac_enet_send.patch
Patch0223: hw-net-net_tx_pkt-fix-assertion-failure-in-net_tx_pk.patch
Patch0224: sm501-Convert-printf-abort-to-qemu_log_mask.patch
Patch0225: sm501-Shorten-long-variable-names-in-sm501_2d_operat.patch
Patch0226: sm501-Use-BIT-x-macro-to-shorten-constant.patch
Patch0227: sm501-Clean-up-local-variables-in-sm501_2d_operation.patch
Patch0228: sm501-Replace-hand-written-implementation-with-pixma.patch
Patch0229: pci-check-bus-pointer-before-dereference.patch
Patch0230: hw-ide-check-null-block-before-_cancel_dma_sync.patch
Patch0231: elf2dmp-Fix-memory-leak-on-main-error-paths.patch
Patch0232: io-Don-t-use-flag-of-printf-format.patch
Patch0233: hw-display-omap_lcdc-Fix-potential-NULL-pointer-dere.patch
Patch0234: hw-display-exynos4210_fimd-Fix-potential-NULL-pointe.patch
Patch0235: block-vvfat-Fix-bad-printf-format-specifiers.patch
Patch0236: block-Remove-unused-include.patch
Patch0237: ssi-Fix-bad-printf-format-specifiers.patch
Patch0238: net-l2tpv3-Remove-redundant-check-in-net_init_l2tpv3.patch
Patch0239: ati-check-x-y-display-parameter-values.patch
Patch0240: migration-dirtyrate-setup-up-query-dirtyrate-framwor.patch
Patch0241: migration-dirtyrate-add-DirtyRateStatus-to-denote-ca.patch
Patch0242: migration-dirtyrate-Add-RamblockDirtyInfo-to-store-s.patch
Patch0243: migration-dirtyrate-Add-dirtyrate-statistics-series-.patch
Patch0244: migration-dirtyrate-move-RAMBLOCK_FOREACH_MIGRATABLE.patch
Patch0245: migration-dirtyrate-Record-hash-results-for-each-sam.patch
Patch0246: migration-dirtyrate-Compare-page-hash-results-for-re.patch
Patch0247: migration-dirtyrate-skip-sampling-ramblock-with-size.patch
Patch0248: migration-dirtyrate-Implement-set_sample_page_period.patch
Patch0249: migration-dirtyrate-Implement-calculate_dirtyrate-fu.patch
Patch0250: migration-dirtyrate-Implement-qmp_cal_dirty_rate-qmp.patch
Patch0251: migration-dirtyrate-Add-trace_calls-to-make-it-easie.patch
Patch0252: migration-dirtyrate-record-start_time-and-calc_time-.patch
Patch0253: migration-dirtyrate-present-dirty-rate-only-when-que.patch
Patch0254: migration-dirtyrate-simplify-includes-in-dirtyrate.c.patch
Patch0255: migration-tls-save-hostname-into-MigrationState.patch
Patch0256: migration-tls-extract-migration_tls_client_create-fo.patch
Patch0257: migration-tls-add-tls_hostname-into-MultiFDSendParam.patch
Patch0258: migration-tls-extract-cleanup-function-for-common-us.patch
Patch0259: migration-tls-add-support-for-multifd-tls-handshake.patch
Patch0260: migration-tls-add-trace-points-for-multifd-tls.patch
Patch0261: qemu-file-Don-t-do-IO-after-shutdown.patch
Patch0262: multifd-Make-sure-that-we-don-t-do-any-IO-after-an-e.patch
Patch0263: migration-Don-t-send-data-if-we-have-stopped.patch
Patch0264: migration-Create-migration_is_running.patch
Patch0265: migration-fix-COLO-broken-caused-by-a-previous-commi.patch
Patch0266: migration-multifd-fix-hangup-with-TLS-Multifd-due-to.patch
Patch0267: multifd-tls-fix-memoryleak-of-the-QIOChannelSocket-o.patch
Patch0268: net-remove-an-assert-call-in-eth_get_gso_type.patch
Patch0269: json-Fix-a-memleak-in-parse_pair.patch
Patch0270: Bugfix-hw-acpi-Use-max_cpus-instead-of-cpus-when-bui.patch
Patch0271: slirp-check-pkt_len-before-reading-protocol-header.patch
Patch0272: hw-usb-hcd-ohci-check-for-processed-TD-before-retire.patch
Patch0273: hw-ehci-check-return-value-of-usb_packet_map.patch
Patch0274: hw-usb-hcd-ohci-check-len-and-frame_number-variables.patch
Patch0275: hw-net-e1000e-advance-desc_offset-in-case-of-null-de.patch
Patch0276: hostmem-Fix-up-free-host_nodes-list-right-after-visi.patch
Patch0277: target-arm-Fix-write-redundant-values-to-kvm.patch
Patch0278: memory-clamp-cached-translation-in-case-it-points-to.patch
Patch0279: scsi-bus-Refactor-the-code-that-retries-requests.patch
Patch0280: scsi-disk-Add-support-for-retry-on-errors.patch
Patch0281: qapi-block-core-Add-retry-option-for-error-action.patch
Patch0282: block-backend-Introduce-retry-timer.patch
Patch0283: block-backend-Add-device-specific-retry-callback.patch
Patch0284: block-backend-Enable-retry-action-on-errors.patch
Patch0285: block-backend-Add-timeout-support-for-retry.patch
Patch0286: block-Add-error-retry-param-setting.patch
Patch0287: virtio-blk-Refactor-the-code-that-processes-queued-r.patch
Patch0288: virtio-blk-On-restart-process-queued-requests-in-the.patch
Patch0289: virtio_blk-Add-support-for-retry-on-errors.patch
Patch0290: migration-Add-multi-thread-compress-method.patch
Patch0291: migration-Refactoring-multi-thread-compress-migratio.patch
Patch0292: migration-Add-multi-thread-compress-ops.patch
Patch0293: migration-Add-zstd-support-in-multi-thread-compressi.patch
Patch0294: migration-Add-compress_level-sanity-check.patch
Patch0295: doc-Update-multi-thread-compression-doc.patch
Patch0296: configure-Enable-test-and-libs-for-zstd.patch
Patch0297: ati-use-vga_read_byte-in-ati_cursor_define.patch
Patch0298: sd-sdhci-assert-data_count-is-within-fifo_buffer.patch
Patch0299: msix-add-valid.accepts-methods-to-check-address.patch
Patch0300: ide-atapi-check-io_buffer_index-in-ide_atapi_cmd_rep.patch
Patch0301: block-backend-Stop-retrying-when-draining.patch
Patch0302: migration-fix-memory-leak-in-qmp_migrate_set_paramet.patch
Patch0303: migration-tls-fix-inverted-semantics-in-multifd_chan.patch
Patch0304: migration-tls-add-error-handling-in-multifd_tls_hand.patch
Patch0305: net-vmxnet3-validate-configuration-values-during-act.patch
Patch0306: block-Add-sanity-check-when-setting-retry-parameters.patch
Patch0307: hw-pci-host-add-pci-intack-write-method.patch
Patch0308: pci-host-add-pcie-msi-read-method.patch
Patch0309: vfio-add-quirk-device-write-method.patch
Patch0310: prep-add-ppc-parity-write-method.patch
Patch0311: nvram-add-nrf51_soc-flash-read-method.patch
Patch0312: spapr_pci-add-spapr-msi-read-method.patch
Patch0313: tz-ppc-add-dummy-read-write-methods.patch
Patch0314: imx7-ccm-add-digprog-mmio-write-method.patch
Patch0315: util-cacheinfo-fix-crash-when-compiling-with-uClibc.patch
Patch0316: arm-cpu-Fixed-function-undefined-error-at-compile-ti.patch
Patch0317: blockjob-Fix-crash-with-IOthread-when-block-commit-a.patch
Patch0318: vhost-user-gpu-fix-resource-leak-in-vg_resource_crea.patch
Patch0319: vhost-user-gpu-fix-memory-leak-in-vg_resource_attach.patch
Patch0320: vhost-user-gpu-fix-memory-leak-while-calling-vg_reso.patch
Patch0321: vhost-user-gpu-fix-memory-leak-in-virgl_cmd_resource.patch
Patch0322: vhost-user-gpu-fix-memory-leak-in-virgl_resource_att.patch
Patch0323: vhost-user-gpu-fix-memory-disclosure-in-virgl_cmd_ge.patch
Patch0324: vhost-user-gpu-fix-OOB-write-in-virgl_cmd_get_capset.patch
Patch0325: ide-ahci-add-check-to-avoid-null-dereference-CVE-201.patch
Patch0326: hw-intc-arm_gic-Fix-interrupt-ID-in-GICD_SGIR-regist.patch
Patch0327: usb-limit-combined-packets-to-1-MiB-CVE-2021-3527.patch
Patch0328: hw-net-rocker_of_dpa-fix-double-free-bug-of-rocker-d.patch
Patch0329: x86-Intel-AVX512_BF16-feature-enabling.patch
Patch0330: i386-Add-MSR-feature-bit-for-MDS-NO.patch
Patch0331: i386-Add-macro-for-stibp.patch
Patch0332: i386-Add-new-CPU-model-Cooperlake.patch
Patch0333: target-i386-Add-new-bit-definitions-of-MSR_IA32_ARCH.patch
Patch0334: target-i386-Add-missed-security-features-to-Cooperla.patch
Patch0335: target-i386-add-PSCHANGE_NO-bit-for-the-ARCH_CAPABIL.patch
Patch0336: target-i386-Export-TAA_NO-bit-to-guests.patch
Patch0337: target-i386-Introduce-Denverton-CPU-model.patch
Patch0338: target-i386-Add-Snowridge-v2-no-MPX-CPU-model.patch
Patch0339: i386-Add-CPUID-bit-for-CLZERO-and-XSAVEERPTR.patch
Patch0340: crypto-add-support-for-nettle-s-native-XTS-impl.patch
Patch0341: crypto-add-support-for-gcrypt-s-native-XTS-impl.patch
Patch0342: tests-benchmark-crypto-with-fixed-data-size-not-time.patch
Patch0343: tests-allow-filtering-crypto-cipher-benchmark-tests.patch
Patch0344: target-i386-handle-filtered_features-in-a-new-functi.patch
Patch0345: target-i386-introduce-generic-feature-dependency-mec.patch
Patch0346: target-i386-expand-feature-words-to-64-bits.patch
Patch0347: target-i386-add-VMX-definitions.patch
Patch0348: vmxcap-correct-the-name-of-the-variables.patch
Patch0349: target-i386-add-VMX-features.patch
Patch0350: target-i386-work-around-KVM_GET_MSRS-bug-for-seconda.patch
Patch0351: target-i386-add-VMX-features-to-named-CPU-models.patch
Patch0352: target-i386-add-two-missing-VMX-features-for-Skylake.patch
Patch0353: target-i386-disable-VMX-features-if-nested-0.patch
Patch0354: i386-cpu-Don-t-add-unavailable_features-to-env-user_.patch
Patch0355: target-i386-do-not-set-unsupported-VMX-secondary-exe.patch
Patch0356: migration-fix-multifd_send_pages-next-channel.patch
Patch0357: migration-Make-sure-that-we-don-t-call-write-in-case.patch
Patch0358: virtio-don-t-enable-notifications-during-polling.patch
Patch0359: usbredir-Prevent-recursion-in-usbredir_write.patch
Patch0360: xhci-recheck-slot-status.patch
Patch0361: vhost-Add-names-to-section-rounded-warning.patch
Patch0362: vhost-user-Print-unexpected-slave-message-types.patch
Patch0363: contrib-libvhost-user-Protect-slave-fd-with-mutex.patch
Patch0364: libvhost-user-Fix-some-memtable-remap-cases.patch
Patch0365: xics-Don-t-deassert-outputs.patch
Patch0366: i386-Resolve-CPU-models-to-v1-by-default.patch
Patch0367: block-curl-HTTP-header-fields-allow-whitespace-aroun.patch
Patch0368: block-curl-HTTP-header-field-names-are-case-insensit.patch
Patch0369: backup-Improve-error-for-bdrv_getlength-failure.patch
Patch0370: mirror-Make-sure-that-source-and-target-size-match.patch
Patch0371: iotests-143-Create-socket-in-SOCK_DIR.patch
Patch0372: nbd-server-Avoid-long-error-message-assertions-CVE-2.patch
Patch0373: block-Call-attention-to-truncation-of-long-NBD-expor.patch
Patch0374: qemu-img-convert-Don-t-pre-zero-images.patch
Patch0375: qcow2-Fix-qcow2_alloc_cluster_abort-for-external-dat.patch
Patch0376: mirror-Wait-only-for-in-flight-operations.patch
Patch0377: virtio-net-delete-also-control-queue-when-TX-RX-dele.patch
Patch0378: target-i386-enable-monitor-and-ucode-revision-with-c.patch
Patch0379: target-i386-set-the-CPUID-level-to-0x14-on-old-machi.patch
Patch0380: target-i386-kvm-initialize-feature-MSRs-very-early.patch
Patch0381: target-i386-add-a-ucode-rev-property.patch
Patch0382: migration-use-migration_is_active-to-represent-activ.patch
Patch0383: migration-Rate-limit-inside-host-pages.patch
Patch0384: hw-pci-pcie-Move-hot-plug-capability-check-to-pre_pl.patch
Patch0385: qapi-block-core-Introduce-BackupCommon.patch
Patch0386: drive-backup-create-do_backup_common.patch
Patch0387: blockdev-backup-utilize-do_backup_common.patch
Patch0388: qapi-add-BitmapSyncMode-enum.patch
Patch0389: block-backup-Add-mirror-sync-mode-bitmap.patch
Patch0390: block-backup-add-never-policy-to-bitmap-sync-mode.patch
Patch0391: block-backup-loosen-restriction-on-readonly-bitmaps.patch
Patch0392: block-backup-hoist-bitmap-check-into-QMP-interface.patch
Patch0393: block-backup-deal-with-zero-detection.patch
Patch0394: mirror-Fix-bdrv_has_zero_init-use.patch
Patch0395: blockdev-fix-coding-style-issues-in-drive_backup_pre.patch
Patch0396: blockdev-unify-qmp_drive_backup-and-drive-backup-tra.patch
Patch0397: blockdev-unify-qmp_blockdev_backup-and-blockdev-back.patch
Patch0398: blockdev-honor-bdrv_try_set_aio_context-context-requ.patch
Patch0399: blockdev-Return-bs-to-the-proper-context-on-snapshot.patch
Patch0400: block-Fix-cross-AioContext-blockdev-snapshot.patch
Patch0401: vl-Don-t-mismatch-g_strsplit-g_free.patch
Patch0402: seqlock-fix-seqlock_write_unlock_impl-function.patch
Patch0403: target-i386-kvm-initialize-microcode-revision-from-K.patch
Patch0404: target-i386-check-for-availability-of-MSR_IA32_UCODE.patch
Patch0405: hw-arm-virt-Init-PMU-for-hotplugged-vCPU.patch
Patch0406: Fixed-integer-overflow-in-e1000e.patch
Patch0407: migration-fix-cleanup_bh-leak-on-resume.patch
Patch0408: qmp-fix-leak-on-callbacks-that-return-both-value-and.patch
Patch0409: qga-commands-posix-fix-use-after-free-of-local_err.patch
Patch0410: file-posix-Fix-leaked-fd-in-raw_open_common-error-pa.patch
Patch0411: object-return-self-in-object_ref.patch
Patch0412: lm32-do-not-leak-memory-on-object_new-object_unref.patch
Patch0413: cris-do-not-leak-struct-cris_disasm_data.patch
Patch0414: hppa-fix-leak-from-g_strdup_printf.patch
Patch0415: mcf5208-fix-leak-from-qemu_allocate_irqs.patch
Patch0416: microblaze-fix-leak-of-fdevice-tree-blob.patch
Patch0417: ide-fix-leak-from-qemu_allocate_irqs.patch
Patch0418: make-check-unit-use-after-free-in-test-opts-visitor.patch
Patch0419: xhci-fix-valid.max_access_size-to-access-address-reg.patch
Patch0420: qga-fix-assert-regression-on-guest-shutdown.patch
Patch0421: char-fix-use-after-free-with-dup-chardev-reconnect.patch
Patch0422: migration-Count-new_dirty-instead-of-real_dirty.patch
Patch0423: qga-Plug-unlikely-memory-leak-in-guest-set-memory-bl.patch
Patch0424: chardev-tcp-Fix-error-message-double-free-error.patch
Patch0425: colo-compare-Fix-memory-leak-in-packet_enqueue.patch
Patch0426: hw-block-nvme-fix-pin-based-interrupt-behavior.patch
Patch0427: hw-block-nvme-fix-pci-doorbell-size-calculation.patch
Patch0428: virtio-pci-fix-queue_enable-write.patch
Patch0429: hw-pci-pci_bridge-Correct-pci_bridge_io-memory-regio.patch
Patch0430: linux-user-mmap.c-fix-integer-underflow-in-target_mr.patch
Patch0431: migration-rdma-cleanup-rdma-context-before-g_free-to.patch
Patch0432: pc-bios-s390-ccw-net-fix-a-possible-memory-leak-in-g.patch
Patch0433: block-qcow2-do-free-crypto_opts-in-qcow2_close.patch
Patch0434: qemu-img-free-memory-before-re-assign.patch
Patch0435: block-qcow2-threads-fix-qcow2_decompress.patch
Patch0436: block-Avoid-memleak-on-qcow2-image-info-failure.patch
Patch0437: block-bdrv_set_backing_bs-fix-use-after-free.patch
Patch0438: hmp-vnc-Fix-info-vnc-list-leak.patch
Patch0439: migration-colo-fix-use-after-free-of-local_err.patch
Patch0440: migration-ram-fix-use-after-free-of-local_err.patch
Patch0441: block-mirror-fix-use-after-free-of-local_err.patch
Patch0442: block-fix-bdrv_root_attach_child-forget-to-unref-chi.patch
Patch0443: virtio-serial-bus-Plug-memory-leak-on-realize-error-.patch
Patch0444: virtio-blk-delete-vqs-on-the-error-path-in-realize.patch
Patch0445: fix-vhost_user_blk_watch-crash.patch
Patch0446: vhost-user-blk-delay-vhost_user_blk_disconnect.patch
Patch0447: usbredir-fix-buffer-overflow-on-vmload.patch
Patch0448: display-bochs-display-fix-memory-leak.patch
Patch0449: audio-fix-integer-overflow.patch
Patch0450: migration-multifd-clean-pages-after-filling-packet.patch
Patch0451: migration-multifd-not-use-multifd-during-postcopy.patch
Patch0452: migration-Define-VMSTATE_INSTANCE_ID_ANY.patch
Patch0453: migration-Change-SaveStateEntry.instance_id-into-uin.patch
Patch0454: apic-Use-32bit-APIC-ID-for-migration-instance-ID.patch
Patch0455: virtio-add-ability-to-delete-vq-through-a-pointer.patch
Patch0456: virtio-pmem-do-delete-rq_vq-in-virtio_pmem_unrealize.patch
Patch0457: virtio-crypto-do-delete-ctrl_vq-in-virtio_crypto_dev.patch
Patch0458: vhost-user-blk-delete-virtioqueues-in-unrealize-to-f.patch
Patch0459: vhost-user-blk-convert-to-new-virtio_delete_queue.patch
Patch0460: block-nbd-extract-the-common-cleanup-code.patch
Patch0461: virtio-gracefully-handle-invalid-region-caches.patch
Patch0462: migration-savevm-release-gslist-after-dump_vmstate_j.patch
Patch0463: virtio-input-fix-memory-leak-on-unrealize.patch
Patch0464: target-arm-only-set-ID_PFR1_EL1.GIC-for-AArch32-gues.patch
Patch0465: target-arm-clear-EL2-and-EL3-only-when-kvm-is-not-en.patch
Patch0466: target-arm-Update-the-ID-registers-of-Kunpeng-920.patch
Patch0467: hw-net-fix-vmxnet3-live-migration.patch
Patch0468: include-Make-headers-more-self-contained.patch
Patch0469: migration-register_savevm_live-doesn-t-need-dev.patch
Patch0470: vmstate-add-qom-interface-to-get-id.patch
Patch0471: linux-headers-Update-against-Add-migration-support-f.patch
Patch0472: vfio-Add-function-to-unmap-VFIO-region.patch
Patch0473: vfio-Add-vfio_get_object-callback-to-VFIODeviceOps.patch
Patch0474: vfio-Add-save-and-load-functions-for-VFIO-PCI-device.patch
Patch0475: vfio-Add-migration-region-initialization-and-finaliz.patch
Patch0476: vfio-Add-VM-state-change-handler-to-know-state-of-VM.patch
Patch0477: vfio-Add-migration-state-change-notifier.patch
Patch0478: vfio-Register-SaveVMHandlers-for-VFIO-device.patch
Patch0479: vfio-Add-save-state-functions-to-SaveVMHandlers.patch
Patch0480: vfio-Add-load-state-functions-to-SaveVMHandlers.patch
Patch0481: memory-Set-DIRTY_MEMORY_MIGRATION-when-IOMMU-is-enab.patch
Patch0482: vfio-Get-migration-capability-flags-for-container.patch
Patch0483: vfio-Add-function-to-start-and-stop-dirty-pages-trac.patch
Patch0484: vfio-Add-vfio_listener_log_sync-to-mark-dirty-pages.patch
Patch0485: vfio-Dirty-page-tracking-when-vIOMMU-is-enabled.patch
Patch0486: vfio-Add-ioctl-to-get-dirty-pages-bitmap-during-dma-.patch
Patch0487: vfio-Make-vfio-pci-device-migration-capable.patch
Patch0488: qapi-Add-VFIO-devices-migration-stats-in-Migration-s.patch
Patch0489: vfio-Move-the-saving-of-the-config-space-to-the-righ.patch
Patch0490: vfio-Set-the-priority-of-the-VFIO-VM-state-change-ha.patch
Patch0491: vfio-Avoid-disabling-and-enabling-vectors-repeatedly.patch
Patch0492: kvm-split-too-big-memory-section-on-several-memslots.patch
Patch0493: kvm-Reallocate-dirty_bmap-when-we-change-a-slot.patch
Patch0494: accel-kvm-Fix-memory-waste-under-mismatch-page-size.patch
Patch0495: memory-Skip-dirty-tracking-for-un-migratable-memory-.patch
Patch0496: Fix-use-after-free-in-vfio_migration_probe.patch
Patch0497: vfio-Make-migration-support-experimental.patch
Patch0498: vfio-Change-default-dirty-pages-tracking-behavior-du.patch
Patch0499: vfio-Fix-vfio_listener_log_sync-function-name-typo.patch
Patch0500: vfio-Support-host-translation-granule-size.patch
Patch0501: vfio-migrate-Move-switch-of-dirty-tracking-into-vfio.patch
Patch0502: vfio-Fix-unregister-SaveVMHandler-in-vfio_migration_.patch
Patch0503: migration-ram-Reduce-unnecessary-rate-limiting.patch
Patch0504: migration-ram-Optimize-ram_save_host_page.patch
Patch0505: qdev-monitors-Fix-reundant-error_setg-of-qdev_add_de.patch
Patch0506: linux-headers-update-against-5.10-and-manual-clear-v.patch
Patch0507: vfio-Maintain-DMA-mapping-range-for-the-container.patch
Patch0508: vfio-migration-Add-support-for-manual-clear-vfio-dir.patch
Patch0509: hw-arm-smmuv3-Support-16K-translation-granule.patch
Patch0510: hw-arm-smmuv3-Set-the-restoration-priority-of-the-vS.patch
Patch0511: hw-vfio-common-trace-vfio_connect_container-operatio.patch
Patch0512: update-linux-headers-Import-iommu.h.patch
Patch0513: vfio.h-and-iommu.h-header-update-against-5.10.patch
Patch0514: memory-Add-new-fields-in-IOTLBEntry.patch
Patch0515: hw-arm-smmuv3-Improve-stage1-ASID-invalidation.patch
Patch0516: hw-arm-smmu-common-Allow-domain-invalidation-for-NH_.patch
Patch0517: memory-Add-IOMMU_ATTR_VFIO_NESTED-IOMMU-memory-regio.patch
Patch0518: memory-Add-IOMMU_ATTR_MSI_TRANSLATE-IOMMU-memory-reg.patch
Patch0519: memory-Introduce-IOMMU-Memory-Region-inject_faults-A.patch
Patch0520: iommu-Introduce-generic-header.patch
Patch0521: pci-introduce-PCIPASIDOps-to-PCIDevice.patch
Patch0522: vfio-Force-nested-if-iommu-requires-it.patch
Patch0523: vfio-Introduce-hostwin_from_range-helper.patch
Patch0524: vfio-Introduce-helpers-to-DMA-map-unmap-a-RAM-sectio.patch
Patch0525: vfio-Set-up-nested-stage-mappings.patch
Patch0526: vfio-Pass-stage-1-MSI-bindings-to-the-host.patch
Patch0527: vfio-Helper-to-get-IRQ-info-including-capabilities.patch
Patch0528: vfio-pci-Register-handler-for-iommu-fault.patch
Patch0529: vfio-pci-Set-up-the-DMA-FAULT-region.patch
Patch0530: vfio-pci-Implement-the-DMA-fault-handler.patch
Patch0531: hw-arm-smmuv3-Advertise-MSI_TRANSLATE-attribute.patch
Patch0532: hw-arm-smmuv3-Store-the-PASID-table-GPA-in-the-trans.patch
Patch0533: hw-arm-smmuv3-Fill-the-IOTLBEntry-arch_id-on-NH_VA-i.patch
Patch0534: hw-arm-smmuv3-Fill-the-IOTLBEntry-leaf-field-on-NH_V.patch
Patch0535: hw-arm-smmuv3-Pass-stage-1-configurations-to-the-hos.patch
Patch0536: hw-arm-smmuv3-Implement-fault-injection.patch
Patch0537: hw-arm-smmuv3-Allow-MAP-notifiers.patch
Patch0538: pci-Add-return_page_response-pci-ops.patch
Patch0539: vfio-pci-Implement-return_page_response-page-respons.patch
Patch0540: vfio-common-Avoid-unmap-ram-section-at-vfio_listener.patch
Patch0541: vfio-Introduce-helpers-to-mark-dirty-pages-of-a-RAM-.patch
Patch0542: vfio-Add-vfio_prereg_listener_log_sync-in-nested-sta.patch
Patch0543: vfio-Add-vfio_prereg_listener_log_clear-to-re-enable.patch
Patch0544: vfio-Add-vfio_prereg_listener_global_log_start-stop-.patch
Patch0545: hw-arm-smmuv3-Post-load-stage-1-configurations-to-th.patch
Patch0546: usbredir-fix-free-call.patch
Patch0547: vfio-common-Fix-incorrect-address-alignment-in-vfio_.patch
Patch0548: vfio-common-Add-address-alignment-check-in-vfio_list.patch
Patch0549: uas-add-stream-number-sanity-checks.patch

BuildRequires: flex
BuildRequires: gcc
BuildRequires: bison
BuildRequires: texinfo
BuildRequires: perl-podlators
BuildRequires: kernel
BuildRequires: chrpath
BuildRequires: gettext
BuildRequires: python-sphinx

BuildRequires: zlib-devel
BuildRequires: zstd-devel
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
BuildRequires: spice-server-devel >= 0.12.5
BuildRequires: spice-protocol >= 0.12.3
%ifarch aarch64
BuildRequires: libfdt-devel
BuildRequires: virglrenderer-devel
%endif

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
    --enable-tpm \
    --enable-modules \
    --enable-libssh \
    --enable-spice \
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
    --disable-smartcard \
    --enable-zstd

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
* Thu Sep 16 2021 Chen Qun <kuhn.chenqun@huawei.com>
- uas: add stream number sanity checks.

* Tue Sep 14 2021 Chen Qun <kuhn.chenqun@huawei.com>
- vfio/common: Add address alignment check in vfio_listener_region_del

* Wed Sep 08 2021 Chen Qun <kuhn.chenqun@huawei.com>
- vfio/common: Fix incorrect address alignment in vfio_dma_map_ram_section

* Thu Aug 19 2021 Jiajie Li <lijiajie11@huawei.com>
- add qemu-block-curl package
- add qemu-block-curl requirement for qemu.

* Mon Aug 16 2021 Chen Qun <kuhn.chenqun@huawei.com>
- usbredir: fix free call

* Wed Aug 04 2021 Chen Qun <kuhn.chenqun@huawei.com>
- vfio: Support host translation granule size
- vfio/migrate: Move switch of dirty tracking into vfio_memory_listener
- vfio: Fix unregister SaveVMHandler in vfio_migration_finalize
- migration/ram: Reduce unnecessary rate limiting
- migration/ram: Optimize ram_save_host_page()
- qdev/monitors: Fix reundant error_setg of qdev_add_device
- linux-headers: update against 5.10 and manual clear vfio dirty log series
- vfio: Maintain DMA mapping range for the container
- vfio/migration: Add support for manual clear vfio dirty log
- hw/arm/smmuv3: Support 16K translation granule
- hw/arm/smmuv3: Set the restoration priority of the vSMMUv3 explicitly
- hw/vfio/common: trace vfio_connect_container operations
- update-linux-headers: Import iommu.h
- vfio.h and iommu.h header update against 5.10
- memory: Add new fields in IOTLBEntry
- hw/arm/smmuv3: Improve stage1 ASID invalidation
- hw/arm/smmu-common: Allow domain invalidation for NH_ALL/NSNH_ALL
- memory: Add IOMMU_ATTR_VFIO_NESTED IOMMU memory region attribute
- memory: Add IOMMU_ATTR_MSI_TRANSLATE IOMMU memory region attribute
- memory: Introduce IOMMU Memory Region inject_faults API
- iommu: Introduce generic header
- pci: introduce PCIPASIDOps to PCIDevice
- vfio: Force nested if iommu requires it
- vfio: Introduce hostwin_from_range helper
- vfio: Introduce helpers to DMA map/unmap a RAM section
- vfio: Set up nested stage mappings
- vfio: Pass stage 1 MSI bindings to the host
- vfio: Helper to get IRQ info including capabilities
- vfio/pci: Register handler for iommu fault
- vfio/pci: Set up the DMA FAULT region
- vfio/pci: Implement the DMA fault handler
- hw/arm/smmuv3: Advertise MSI_TRANSLATE attribute
- hw/arm/smmuv3: Store the PASID table GPA in the translation config
- hw/arm/smmuv3: Fill the IOTLBEntry arch_id on NH_VA invalidation
- hw/arm/smmuv3: Fill the IOTLBEntry leaf field on NH_VA invalidation
- hw/arm/smmuv3: Pass stage 1 configurations to the host
- hw/arm/smmuv3: Implement fault injection
- hw/arm/smmuv3: Allow MAP notifiers
- pci: Add return_page_response pci ops
- vfio/pci: Implement return_page_response page response callback
- vfio/common: Avoid unmap ram section at vfio_listener_region_del() in nested mode
- vfio: Introduce helpers to mark dirty pages of a RAM section
- vfio: Add vfio_prereg_listener_log_sync in nested stage
- vfio: Add vfio_prereg_listener_log_clear to re-enable mark dirty pages
- vfio: Add vfio_prereg_listener_global_log_start/stop in nested stage
- hw/arm/smmuv3: Post-load stage 1 configurations to the host

* Tue Aug 03 2021 Chen Qun <kuhn.chenqun@huawei.com>
- kvm: split too big memory section on several memslots
- kvm: Reallocate dirty_bmap when we change a slot
- accel: kvm: Fix memory waste under mismatch page size
- memory: Skip dirty tracking for un-migratable memory regions
- Fix use after free in vfio_migration_probe
- vfio: Make migration support experimental
- vfio: Change default dirty pages tracking behavior during migration
- vfio: Fix vfio_listener_log_sync function name typo

* Thu Jul 29 2021 Chen Qun <kuhn.chenqun@huawei.com>
- vfio: Move the saving of the config space to the right place in VFIO migration
- vfio: Set the priority of the VFIO VM state change handler explicitly
- vfio: Avoid disabling and enabling vectors repeatedly in VFIO migration

* Thu Jul 29 2021 imxcc <xingchaochao@huawei.com>
- hw/net: fix vmxnet3 live migration
- include: Make headers more self-contained
- migration: register_savevm_live doesn't need dev
- vmstate: add qom interface to get id
- linux headers: Update against "Add migration support for VFIO devices"
- vfio: Add function to unmap VFIO region
- vfio: Add vfio_get_object callback to VFIODeviceOps
- vfio: Add save and load functions for VFIO PCI devices
- vfio: Add migration region initialization and finalize function
- vfio: Add VM state change handler to know state of VM
- vfio: Add migration state change notifier
- vfio: Register SaveVMHandlers for VFIO device
- vfio: Add save state functions to SaveVMHandlers
- vfio: Add load state functions to SaveVMHandlers
- memory: Set DIRTY_MEMORY_MIGRATION when IOMMU is enabled
- vfio: Get migration capability flags for container
- vfio: Add function to start and stop dirty pages tracking
- vfio: Add vfio_listener_log_sync to mark dirty pages
- vfio: Dirty page tracking when vIOMMU is enabled
- vfio: Add ioctl to get dirty pages bitmap during dma unmap
- vfio: Make vfio-pci device migration capable
- qapi: Add VFIO devices migration stats in Migration stats

* Wed Jul 28 2021 imxcc <xingchaochao@huawei.com>
- object: return self in object_ref()
- file-posix: Fix leaked fd in raw_open_common() error path
- qga/commands-posix: fix use after free of local_err
- qmp: fix leak on callbacks that return both value and error
- migration: fix cleanup_bh leak on resume
- Fixed integer overflow in e1000e
- lm32-do-not-leak-memory-on-object_new-object_unref.patch
- cris-do-not-leak-struct-cris_disasm_data.patch
- hppa-fix-leak-from-g_strdup_printf.patch
- mcf5208-fix-leak-from-qemu_allocate_irqs.patch
- microblaze-fix-leak-of-fdevice-tree-blob.patch
- ide-fix-leak-from-qemu_allocate_irqs.patch
- make-check-unit-use-after-free-in-test-opts-visitor.patch
- virtio-pci: fix queue_enable write
- hw/block/nvme: fix pci doorbell size calculation
- hw/block/nvme: fix pin-based interrupt behavior
- colo-compare: Fix memory leak in packet_enqueue()
- chardev/tcp: Fix error message double free error
- qga: Plug unlikely memory leak in guest-set-memory-blocks
- migration: Count new_dirty instead of real_dirty
- char: fix use-after-free with dup chardev & reconnect
- qga: fix assert regression on guest-shutdown
- xhci: fix valid.max_access_size to access address registers
- block/qcow2: do free crypto_opts in qcow2_close()
- qemu-img: free memory before re-assign
- block/qcow2-threads: fix qcow2_decompress
- block: Avoid memleak on qcow2 image info failure
- block: bdrv_set_backing_bs: fix use-after-free
- hmp/vnc: Fix info vnc list leak
- migration/colo: fix use after free of local_err
- migration/ram: fix use after free of local_err
- block/mirror: fix use after free of local_err
- block: fix bdrv_root_attach_child forget to unref child_bs
- virtio-serial-bus: Plug memory leak on realize() error paths
- virtio-blk: delete vqs on the error path in realize()
- fix vhost_user_blk_watch crash
- vhost-user-blk: delay vhost_user_blk_disconnect
- hw-pci-pci_bridge-Correct-pci_bridge_io-memory-regio.patch
- linux-user-mmap.c-fix-integer-underflow-in-target_mr.patch
- migration-rdma-cleanup-rdma-context-before-g_free-to.patch
- pc-bios-s390-ccw-net-fix-a-possible-memory-leak-in-g.patch
- usbredir-fix-buffer-overflow-on-vmload.patch
- apic: Use 32bit APIC ID for migration instance-ID
- audio: fix integer overflow
- display/bochs-display: fix memory leak
- migration: Change SaveStateEntry.instance_id into uint32_t
- migration: Define VMSTATE_INSTANCE_ID_ANY
- migration/multifd: clean pages after filling packet
- migration/multifd: not use multifd during postcopy
- virtio: add ability to delete vq through a pointer
- virtio-pmem: do delete rq_vq in virtio_pmem_unrealize
- virtio-crypto: do delete ctrl_vq in virtio_crypto_device_unrealize
- vhost-user-blk: delete virtioqueues in unrealize to fix memleaks
- vhost-user-blk: convert to new virtio_delete_queue
- block/nbd: extract the common cleanup code
- virtio: gracefully handle invalid region caches
- migration/savevm: release gslist after dump_vmstate_json
- virtio-input: fix memory leak on unrealize
- target/arm: only set ID_PFR1_EL1.GIC for AArch32 guest
- target/arm: clear EL2 and EL3 only when kvm is not enabled
- target/arm: Update the ID registers of Kunpeng-920

* Fri Jul 23 2021 imxcc <xingchaochao@huawei.com>
- hw/arm/virt: Init PMU for hotplugged vCPU

* Fri Jul 23 2021 Chen Qun <kuhn.chenqun@huawei.com>
- vl: Don't mismatch g_strsplit()/g_free()
- seqlock: fix seqlock_write_unlock_impl function
- target/i386: kvm: initialize microcode revision from KVM
- target/i386: check for availability of MSR_IA32_UCODE_REV as an emulated MSR

* Thu Jul 22 2021 Chen Qun <kuhn.chenqun@huawei.com>
- qapi/block-core: Introduce BackupCommon
- drive-backup: create do_backup_common
- blockdev-backup: utilize do_backup_common
- qapi: add BitmapSyncMode enum
- block/backup: Add mirror sync mode 'bitmap'
- block/backup: add 'never' policy to bitmap sync mode
- block/backup: loosen restriction on readonly bitmaps
- block/backup: hoist bitmap check into QMP interface
- block/backup: deal with zero detection
- mirror: Fix bdrv_has_zero_init() use
- blockdev: fix coding style issues in drive_backup_prepare
- blockdev: unify qmp_drive_backup and drive-backup transaction paths
- blockdev: unify qmp_blockdev_backup and blockdev-backup transaction paths
- blockdev: honor bdrv_try_set_aio_context() context requirements
- blockdev: Return bs to the proper context on snapshot abort
- block: Fix cross-AioContext blockdev-snapshot

* Thu Jul 22 2021 Chen Qun <kuhn.chenqun@huawei.com>
- hw/pci/pcie: Move hot plug capability check to pre_plug callback

* Thu Jul 22 2021 Chen Qun <kuhn.chenqun@huawei.com>
- migration: use migration_is_active to represent active state
- migration: Rate limit inside host pages

* Thu Jul 22 2021 Chen Qun <kuhn.chenqun@huawei.com>
- virtio-net: delete also control queue when TX/RX deleted
- target/i386: enable monitor and ucode revision with -cpu max
- target/i386: set the CPUID level to 0x14 on old machine-type
- target/i386: kvm: initialize feature MSRs very early
- target/i386: add a ucode-rev property

* Thu Jul 22 2021 Chen Qun <kuhn.chenqun@huawei.com>
- qcow2: Fix qcow2_alloc_cluster_abort() for external data file
- mirror: Wait only for in-flight operations

* Wed Jul 21 2021 Chen Qun <kuhn.chenqun@huawei.com>
- block/curl: HTTP header fields allow whitespace around values
- block/curl: HTTP header field names are case insensitive
- backup: Improve error for bdrv_getlength() failure
- mirror: Make sure that source and target size match
- iotests/143: Create socket in $SOCK_DIR
- nbd/server: Avoid long error message assertions CVE-2020-10761
- block: Call attention to truncation of long NBD exports
- qemu-img convert: Don't pre-zero images

* Wed Jul 21 2021 Chen Qun <kuhn.chenqun@huawei.com>
- virtio: don't enable notifications during polling
- usbredir: Prevent recursion in usbredir_write
- xhci: recheck slot status
- vhost: Add names to section rounded warning
- vhost-user: Print unexpected slave message types
- contrib/libvhost-user: Protect slave fd with mutex
- libvhost-user: Fix some memtable remap cases
- xics: Don't deassert outputs
- i386: Resolve CPU models to v1 by default

* Wed Jul 21 2021 imxcc <xingchaochao@huawei.com>
- target/i386: handle filtered_features in a new function mark_unavailable_features
- target/i386: introduce generic feature dependency mechanism
- target/i386: expand feature words to 64 bits
- target/i386: add VMX definitions
- vmxcap: correct the name of the variables
- target/i386: add VMX features
- target/i386: work around KVM_GET_MSRS bug for secondary execution controls
- target/i386: add VMX features to named CPU models
- target/i386: add two missing VMX features for Skylake and CascadeLake Server
- target/i386: disable VMX features if nested=0
- i386/cpu: Don't add unavailable_features to env->user_features
- target/i386: do not set unsupported VMX secondary execution controls
- migration: fix multifd_send_pages() next channel
- migration: Make sure that we don't call write() in case of error

* Tue Jul 20 2021 Chen Qun <kuhn.chenqun@huawei.com>
- crypto: add support for nettle's native XTS impl
- crypto: add support for gcrypt's native XTS impl
- tests: benchmark crypto with fixed data size, not time period
- tests: allow filtering crypto cipher benchmark tests

* Tue Jul 20 2021 Chen Qun <kuhn.chenqun@huawei.com>
- target/i386: Introduce Denverton CPU model
- target/i386: Add Snowridge-v2 (no MPX) CPU model
- i386: Add CPUID bit for CLZERO and XSAVEERPTR

* Mon Jul 19 2021 Chen Qun <kuhn.chenqun@huawei.com>
- x86: Intel AVX512_BF16 feature enabling
- i386: Add MSR feature bit for MDS-NO
- i386: Add macro for stibp
- i386: Add new CPU model Cooperlake
- target/i386: Add new bit definitions of MSR_IA32_ARCH_CAPABILITIES
- target/i386: Add missed security features to Cooperlake CPU model
- target/i386: add PSCHANGE_NO bit for the ARCH_CAPABILITIES MSR
- target/i386: Export TAA_NO bit to guests

* Mon Jul 19 2021 Chen Qun <kuhn.chenqun@huawei.com>
- hw/net/rocker_of_dpa: fix double free bug of rocker device

* Mon Jun 28 2021 imxcc <xingchaochao@huawei.com>
- spec: add gcc buildrequire

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

* Fri May 28 2021 Chen Qun <kuhn.chenqun@huawei.com>
- blockjob: Fix crash with IOthread when block commit after snapshot

* Thu 20 May 2021 zhouli57 <zhouli57@huawei.com>
- arm/cpu: Fixed function undefined error at compile time under arm

* Wed May 19 2021 Ming Yang <yangming73@huawei.com>
- add strip for block-iscsi.so, block-rbd.so and block-ssh.so.

* Wed 19 May 2021 zhouli57 <zhouli57@huawei.com>
- util/cacheinfo: fix crash when compiling with uClibc

* Fri Mar 26 2021 Chen Qun <kuhn.chenqun@huawei.com>
- hw/pci-host: add pci-intack write method
- pci-host: add pcie-msi read method
- vfio: add quirk device write method
- prep: add ppc-parity write method
- nvram: add nrf51_soc flash read method
- spapr_pci: add spapr msi read method
- tz-ppc: add dummy read/write methods
- imx7-ccm: add digprog mmio write method

* Thu Mar 18 2021 Chen Qun <kuhn.chenqun@huawei.com>
- block: Add sanity check when setting retry parameters

* Wed Mar 17 2021 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- qemu.spec: enable strip for qemu-block-rbd.so and qemu-block-ssh.so

* Fri Mar 12 2021 Chen Qun <kuhn.chenqun@huawei.com>
- net: vmxnet3: validate configuration values during activate (CVE-2021-20203)

* Fri Mar 12 2021 Chen Qun <kuhn.chenqun@huawei.com>
- migration: fix memory leak in qmp_migrate_set_parameters
- migration/tls: fix inverted semantics in multifd_channel_connect
- migration/tls: add error handling in multifd_tls_handshake_thread

* Thu Mar 11 2021 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- qemu.spec: add iscsi rpm package requirement

* Wed Mar 10 2021 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- qemu.spec: make iscsi rpm package

* Tue Mar 02 2021 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- qemu.spec: Add --enable-zstd compile parameter

* Fri Feb 26 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- block-backend: Stop retrying when draining

* Fri Feb 26 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- ide:atapi: check io_buffer_index in ide_atapi_cmd_reply_end

* Fri Feb 19 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- ati: use vga_read_byte in ati_cursor_define
- sd: sdhci: assert data_count is within fifo_buffer
- msix: add valid.accepts methods to check address

* Thu Feb 04 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- migration: Add multi-thread compress method
- migration: Refactoring multi-thread compress migration
- migration: Add multi-thread compress ops
- migration: Add zstd support in multi-thread compression
- migration: Add compress_level sanity check
- doc: Update multi-thread compression doc
- configure: Enable test and libs for zstd

* Sat Jan 30 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
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

* Mon Jan 18 2021 Huawei Technologies Co., Ltd <fangying1@huawei.com>
- feature: enable spice protocol

* Mon Jan 18 2021 Huawei Technologies Co., Ltd <fangying1@huawei.com>
- reorder changelog in desceding order

* Fri Jan 15 2021 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- memory: clamp cached translation in case it points to an MMIO region

* Wed Dec 9 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- target/arm: Fix write redundant values to kvm

* Fri Dec 11 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- hostmem: Fix up free host_nodes list right after visited

* Fri Dec 25 2020 Huawei Technologies Co., Ltd <yangming73@huawei.com>
- add qemu-block-rbd package
- add qemu-block-ssh package

* Fri Dec 11 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- hostmem: Fix up free host_nodes list right after visited

* Fri Dec 11 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- slirp: check pkt_len before reading protocol header for fixing CVE-2020-29129 and CVE-2020-29130

* Wed Dec 9 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- target/arm: Fix write redundant values to kvm

* Wed Dec 2 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- migration/tls: save hostname into MigrationState
- migration/tls: extract migration_tls_client_create for common-use
- migration/tls: add tls_hostname into MultiFDSendParams
- migration/tls: extract cleanup function for common-use
- migration/tls: add support for multifd tls-handshake
- migration/tls: add trace points for multifd-tls
- qemu-file: Don't do IO after shutdown
- multifd: Make sure that we don't do any IO after an error
- migration: Don't send data if we have stopped
- migration: Create migration_is_running()
- migration: fix COLO broken caused by a previous commit
- migration/multifd: fix hangup with TLS-Multifd due to  blocking handshake
- multifd/tls: fix memoryleak of the QIOChannelSocket object when cancelling migration

* Wed Nov 18 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- ati: check x y display parameter values

* Fri Nov 13 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- json: Fix a memleak in parse_pair()

* Wed Nov 11 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- hw: usb: hcd-ohci: check for processed TD before retire
- hw: ehci: check return value of 'usb_packet_map'
- hw: usb: hcd-ohci: check len and frame_number variables
- hw/net/e1000e: advance desc_offset in case of null descriptor

* Fri Oct 30 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- migration/dirtyrate: setup up query-dirtyrate framwork
- migration/dirtyrate: add DirtyRateStatus to denote calculation status
- migration/dirtyrate: Add RamblockDirtyInfo to store sampled page info
- migration/dirtyrate: Add dirtyrate statistics series functions
- migration/dirtyrate: move RAMBLOCK_FOREACH_MIGRATABLE into ram.h
- migration/dirtyrate: Record hash results for each sampled page
- migration/dirtyrate: Compare page hash results for recorded sampled page
- migration/dirtyrate: skip sampling ramblock with size below MIN_RAMBLOCK_SIZE
- migration/dirtyrate: Implement set_sample_page_period() and is_sample_period_valid()
- migration/dirtyrate: Implement calculate_dirtyrate() function
- migration/dirtyrate: Implement qmp_cal_dirty_rate()/qmp_get_dirty_rate() function
- migration/dirtyrate: Add trace_calls to make it easier to debug
- migration/dirtyrate: record start_time and calc_time while at the measuring state
- migration/dirtyrate: present dirty rate only when querying the rate has completed
- migration/dirtyrate: simplify includes in dirtyrate.c

* Fri Oct 30 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- elf2dmp: Fix memory leak on main() error paths
- io: Don't use '#' flag of printf format
- hw/display/omap_lcdc: Fix potential NULL pointer dereference
- hw/display/exynos4210_fimd: Fix potential NULL pointer dereference
- block/vvfat: Fix bad printf format specifiers
- block: Remove unused include
- ssi: Fix bad printf format specifiers
- net/l2tpv3: Remove redundant check in net_init_l2tpv3()

* Thu Oct 29 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- Bugfix: hw/acpi: Use max_cpus instead of cpus when build PPTT table

* Wed Oct 21 2020 Huawei Technologies Co., Ltd <alex.chen@huawei.com>
- net: remove an assert call in eth_get_gso_type

* Wed Oct 14 2020 Prasad J Pandit <pjp@fedoraproject.org>
- pci: check bus pointer before dereference
- hw/ide: check null block before _cancel_dma_sync

* Mon Sep 28 2020 Huawei Technologies Co., Ltd <fangying1@huawei.com>
- sm501: Replace hand written implementation with pixman where possible
- sm501: Clean up local variables in sm501_2d_operation
- sm501: Use BIT(x) macro to shorten constant
- sm501: Shorten long variable names in sm501_2d_operation
- sm501: Convert printf + abort to qemu_log_mask
- hw/net/net_tx_pkt: fix assertion failure in net_tx_pkt_add_raw_fragment
- hw/net/xgmac: Fix buffer overflow in xgmac_enet_send()

* Fri Sep 18 2020 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- hw-sd-sdhci-Fix-DMA-Transfer-Block-Size-field.patch
- hw-xhci-check-return-value-of-usb_packet_map.patch

* Fri Sep 11 2020 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- slirp/src/ip6_input.c: fix out-of-bounds read information vulnerability

* Tue Sep 08 2020 Huawei Technologies Co., Ltd <liangpeng10@huawei.com>
- target/arm: ignore evtstrm and cpuid CPU features

* Fri Aug 21 2020 Huawei Technologies Co., Ltd <lijiajie11@huawei.com>
- hw/usb/core.c: fix buffer overflow in do_token_setup function

* Wed Aug 19 2020 Huawei Technologies Co., Ltd <liangpeng10@huawei.com>
- target-arm-convert-isar-regs-to-array.patch
- target-arm-parse-cpu-feature-related-options.patch
- target-arm-register-CPU-features-for-property.patch
- target-arm-Allow-ID-registers-to-synchronize-to-KVM.patch
- target-arm-introduce-CPU-feature-dependency-mechanis.patch
- target-arm-introduce-KVM_CAP_ARM_CPU_FEATURE.patch
- target-arm-Add-CPU-features-to-query-cpu-model-expan.patch
- target-arm-Update-ID-fields.patch
- target-arm-Add-more-CPU-features.patch

* Wed Aug 19 2020 Huawei Technologies Co., Ltd <liangpeng10@huawei.com>
- target-arm-Add-isar_feature-tests-for-PAN-ATS1E1.patch
- target-arm-Add-ID_AA64MMFR2_EL1.patch
- target-arm-Add-and-use-FIELD-definitions-for-ID_AA64.patch
- target-arm-Use-FIELD-macros-for-clearing-ID_DFR0-PER.patch
- target-arm-Define-an-aa32_pmu_8_1-isar-feature-test-.patch
- target-arm-Add-_aa64_-and-_any_-versions-of-pmu_8_1-.patch
- target-arm-Stop-assuming-DBGDIDR-always-exists.patch
- target-arm-Move-DBGDIDR-into-ARMISARegisters.patch
- target-arm-Enable-ARMv8.2-ATS1E1-in-cpu-max.patch
- target-arm-Test-correct-register-in-aa32_pan-and-aa3.patch
- target-arm-Read-debug-related-ID-registers-from-KVM.patch
- target-arm-monitor-Introduce-qmp_query_cpu_model_exp.patch
- target-arm-monitor-query-cpu-model-expansion-crashed.patch

* Tue Aug 18 2020 Huawei Technologies Co., Ltd <fanhenglong@huawei.com>
- hw/acpi/aml-build.c: build smt processor structure to support smt topology

* Thu Aug 13 2020 Huawei Technologies Co., Ltd <jiangfangjie@huawei.com>
-target/arm: Aarch64 support vtpm

* Wed Aug 12 2020 Huawei Technologies Co., Ltd <jinzeyu@huawei.com>
- backport upstream patch to support SHPCHotplug in arm

* Thu Aug 6 2020 Huawei Technologies Co., Ltd <zhang.zhanghailiang@huawei.com>
- es1370: check total frame count against current frame
- exec: set map length to zero when returning NULL
- ati-vga: check mm_index before recursive call (CVE-2020-13800)
- megasas: use unsigned type for reply_queue_head and check index
- megasas: avoid NULL pointer dereference
- megasas: use unsigned type for positive numeric fields
- hw/scsi/megasas: Fix possible out-of-bounds array access in tracepoints

* Thu Aug 6 2020 Huawei Technologies Co., Ltd <fangying1@huawei.com>
- tests: Disalbe filemonitor testcase

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
