Name: qemu
Version: 4.0.0
Release: 5
Epoch: 2
Summary: QEMU is a generic and open source machine emulator and virtualizer
License: GPLv2 and BSD and MIT and CC-BY
URL: http://www.qemu.org
Source0: https://www.qemu.org/download/%{name}-%{version}%{?rcstr}.tar.xz
Source1: 80-kvm.rules
Source2: 99-qemu-guest-agent.rules
Source3: bridge.conf

Patch0001: qxl-check-release-info-object.patch
Patch0002: target-i386-define-md-clear-bit.patch
Patch0003: Revert-Enable-build-and-install-of-our-rST-docs.patch
Patch0004: ARM64-record-vtimer-tick-when-cpu-is-stopped.patch
Patch0005: pl011-reset-read-FIFO-when-UARTTIMSC-0-UARTICR-0xfff.patch
Patch0006: pl031-support-rtc-timer-property-for-pl031.patch
Patch0007: vhost-cancel-migration-when-vhost-user-restarted.patch
Patch0008: qcow2-fix-memory-leak-in-qcow2_read_extensions.patch
Patch0009: hw-arm-expose-host-CPU-frequency-info-to-guest.patch
Patch0010: block-Fix-AioContext-switch-for-bs-drv-NULL.patch
Patch0011: cutils-Fix-size_to_str-on-32-bit-platforms.patch
Patch0012: qcow2-Avoid-COW-during-metadata-preallocation.patch
Patch0013: qcow2-Add-errp-to-preallocate_co.patch
Patch0014: qcow2-Fix-full-preallocation-with-external-data-file.patch
Patch0015: qcow2-Fix-qcow2_make_empty-with-external-data-file.patch
Patch0016: megasas-fix-mapped-frame-size.patch
Patch0017: kbd-state-fix-autorepeat-handling.patch
Patch0018: block-file-posix-Unaligned-O_DIRECT-block-status.patch
Patch0019: hw-add-compat-machines-for-4.1.patch
Patch0020: usb-tablet-fix-serial-compat-property.patch
Patch0021: q35-Revert-to-kernel-irqchip.patch
Patch0022: hw-Nuke-hw_compat_4_0_1-and-pc_compat_4_0_1.patch
Patch0023: vl-Fix-drive-blockdev-persistent-reservation-management.patch
Patch0024: vhost-fix-vhost_log-size-overflow-during-migration.patch
Patch0025: virtio-pci-fix-missing-device-properties.patch
Patch0026: i386-acpi-fix-gint-overflow-in-crs_range_compare.patch
Patch0027: ioapic-kvm-Skip-route-updates-for-masked-pins.patch
Patch0028: i386-acpi-show-PCI-Express-bus-on-pxb-pcie-expanders.patch
Patch0029: virtio-balloon-fix-QEMU-4.0-config-size-migration-in.patch
Patch0030: virtio-balloon-Fix-wrong-sign-extension-of-PFNs.patch
Patch0031: virtio-balloon-Fix-QEMU-crashes-on-pagesize-BALLOON_.patch
Patch0032: virtio-balloon-Simplify-deflate-with-pbp.patch
Patch0033: virtio-balloon-Better-names-for-offset-variables.patch
Patch0034: virtio-balloon-Rework-pbp-tracking-data.patch
Patch0035: virtio-balloon-Use-temporary-PBP-only.patch
Patch0036: virtio-balloon-don-t-track-subpages-for-the-PBP.patch
Patch0037: virtio-balloon-free-pbp-more-aggressively.patch
Patch0038: qemu-bridge-helper-restrict-interface-name-to-IFNAMS.patch
Patch0039: qemu-bridge-helper-move-repeating-code-in-parse_acl.patch
Patch0040: smbios-Add-missing-member-of-type-4-for-smbios-3.0.patch
Patch0041: hw-arm-virt-Introduce-cpu-topology-support.patch
Patch0042: hw-arm64-add-vcpu-cache-info-support.patch
Patch0043: xhci-Fix-memory-leak-in-xhci_address_slot.patch
Patch0044: xhci-Fix-memory-leak-in-xhci_kick_epctx.patch
Patch0045: ehci-fix-queue-dev-null-ptr-dereference.patch
Patch0046: memory-unref-the-memory-region-in-simplify-flatview.patch
Patch0047: scsi-lsi-exit-infinite-loop-while-executing-script.patch
Patch0048: util-async-hold-AioContext-ref-to-prevent-use-after-free.patch
Patch0049: vhost-user-scsi-prevent-using-uninitialized-vqs.patch
Patch0050: cpu-add-Kunpeng-920-cpu-support.patch
Patch0051: cpu-parse-feature-to-avoid-failure.patch
Patch0052: cpu-add-Cortex-A72-processor-kvm-target-support.patch
Patch0053: vnc-fix-memory-leak-when-vnc-disconnect.patch
Patch0054: pcie-disable-the-PCI_EXP_LINKSTA_DLLA-cap.patch
Patch0055: blockdev-backup-don-t-check-aio_context-too-early.patch
Patch0056: migration-dirty-bitmaps-change-bitmap-enumeration-method.patch
Patch0057: target-i386-add-MDS-NO-feature.patch
Patch0058: usbredir-fix-buffer-overflow-on-vmload.patch
Patch0059: util-hbitmap-update-orig_size-on-truncate.patch
Patch0060: mirror-Only-mirror-granularity-aligned-chunks.patch
Patch0061: qcow2-Fix-the-calculation-of-the-maximum-L2-cache-size.patch
Patch0062: dma-helpers-ensure-AIO-callback-is-invoked-after-can.patch
Patch0063: pr-manager-Fix-invalid-g_free-crash-bug.patch
Patch0064: block-create-Do-not-abort-if-a-block-driver-is-not-available.patch
Patch0065: block-nfs-tear-down-aio-before-nfs_close.patch
Patch0066: blockjob-update-nodes-head-while-removing-all-bdrv.patch
Patch0067: slirp-Fix-heap-overflow-in-ip_reass-on-big-packet-input.patch
Patch0068: slirp-ip_reass-Fix-use-after-free.patch
Patch0069: hw-core-loader-Fix-possible-crash-in-rom_copy.patch
Patch0070: migration-Fix-use-after-free-during-process-exit.patch
Patch0071: linux-headers-update-against-KVM-ARM-Fix-256-vcpus.patch
Patch0072: intc-arm_gic-Support-IRQ-injection-for-more-than-256.patch
Patch0073: ARM-KVM-Check-KVM_CAP_ARM_IRQ_LINE_LAYOUT_2-for-smp_.patch
Patch0074: 9pfs-local-Fix-possible-memory-leak-in-local_link.patch
Patch0075: scsi-disk-define-props-in-scsi_block_disk-to-avoid-memleaks.patch
Patch0076: arm-translate-a64-fix-uninitialized-variable-warning.patch
Patch0077: nbd-fix-uninitialized-variable-warning.patch
Patch0078: xhci-Fix-memory-leak-in-xhci_kick_epctx-when-poweroff.patch
Patch0079: block-fix-memleaks-in-bdrv_refresh_filename.patch

BuildRequires: flex
BuildRequires: bison
BuildRequires: texinfo
BuildRequires: perl-podlators
BuildRequires: kernel
BuildRequires: chrpath
BuildRequires: gettext

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
    --extra-cflags="%{optflags} -fPIE -DPIE -fno-inline -fPIC"    \
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
    --disable-strip \
    --disable-werror  \
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

%global qemudocdir %{_docdir}/%{name}
install -D -p -m 0644 -t %{buildroot}%{qemudocdir} Changelog README COPYING COPYING.LIB LICENSE
chmod -x %{buildroot}%{_mandir}/man1/*


%ifarch aarch64
rm -rf %{buildroot}%{_datadir}/%{name}/vgabios*bin
rm -rf %{buildroot}%{_datadir}/%{name}/bios*.bin
rm -rf %{buildroot}%{_datadir}/%{name}/linuxboot.bin
rm -rf %{buildroot}%{_datadir}/%{name}/kvmvapic.bin
rm -rf %{buildroot}%{_datadir}/%{name}/sgabios.bin
rm -rf %{buildroot}%{_datadir}/%{name}/multiboot.bin
%endif
rm -rf %{buildroot}%{_datadir}/%{name}/openbios-*
rm -rf %{buildroot}%{_datadir}/%{name}/slof.bin
rm -rf %{buildroot}%{_datadir}/%{name}/QEMU,*.bin
rm -rf %{buildroot}%{_datadir}/%{name}/bamboo.dtb
rm -rf %{buildroot}%{_datadir}/%{name}/canyonlands.dtb
rm -rf %{buildroot}%{_datadir}/%{name}/hppa-firmware.img
rm -rf %{buildroot}%{_datadir}/%{name}/linuxboot_dma.bin
rm -rf %{buildroot}%{_datadir}/%{name}/palcode-clipper
rm -rf %{buildroot}%{_datadir}/%{name}/petalogix-*
rm -rf %{buildroot}%{_datadir}/%{name}/ppc_*
rm -rf %{buildroot}%{_datadir}/%{name}/pvh.bin
rm -rf %{buildroot}%{_datadir}/%{name}/qemu_vga.ndrv
rm -rf %{buildroot}%{_datadir}/%{name}/s390-*
rm -rf %{buildroot}%{_datadir}/%{name}/skiboot.lid
rm -rf %{buildroot}%{_datadir}/%{name}/spapr-*
rm -rf %{buildroot}%{_datadir}/%{name}/u-boot*
rm -rf %{buildroot}%{_bindir}/ivshmem*

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

%ifarch %{ix86} x86_64
%files seabios
%{_datadir}/%{name}/bios-256k.bin
%{_datadir}/%{name}/bios.bin
%endif

%changelog
* Thu Jan 16 2020 Huawei Technologies Co., Ltd. <pannengyuan@huawei.com>
- block: fix memleaks in bdrv_refresh_filename

* Mon Jan 13 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- 9pfs: Fix a possible memory leak in local_link
- scsi-disk: disk define props in scsi_block to avoid memleaks
- arm/translate-a64: fix uninitialized variable warning
- nbd: fix uninitialized variable warning
- xhci: Fix memory leak in xhci_kick_epctx when poweroff

* Mon Jan  6 2020 backport from qemu upstream
- linux headers: update against "KVM/ARM: Fix >256 vcp
- intc/arm_gic: Support IRQ injection for more than 256 vpus
- ARM: KVM: Check KVM_CAP_ARM_IRQ_LINE_LAYOUT_2 for smp_cpus > 256

* Thu Dec 12 2019 backport from qemu upstream v4.0.1 release
- usb-tablet: fix serial compat property
- blockdev-backup: don't check aio_context too early
- migration/dirty-bitmaps: change bitmap enumeration method
- target/i386: add MDS-NO feature
- usbredir: fix buffer-overflow on vmload
- tpm: Exit in reset when backend indicates failure
- tpm_emulator: Translate TPM error codes to strings
- util/hbitmap: update orig_size on truncate
- mirror: Only mirror granularity-aligned chunks
- qcow2: Fix the calculation of the maximum L2 cache size
- dma-helpers: ensure AIO callback is invoked after cancellation
- pr-manager: Fix invalid g_free() crash bug
- block/nfs: tear down aio before nfs_close
- blockjob: update nodes head while removing all bdrv
- slirp: Fix heap overflow in ip_reass on big packet input
- slirp: ip_reass: Fix use after free
- hw/core/loader: Fix possible crash in rom_copy()
- migration: Fix use-after-free during process exit

* Thu Oct 17 2019 backport from qemu upstream
- vnc-fix-memory-leak-when-vnc-disconnect.patch

* Mon Sep  9 2019 backport from qemu upstream
- ehci-fix-queue-dev-null-ptr-dereference.patch
- memory-unref-the-memory-region-in-simplify-flatview.patch
- scsi-lsi-exit-infinite-loop-while-executing-script-C.patch
- util-async-hold-AioContext-ref-to-prevent-use-after-.patch
- vhost-user-scsi-prevent-using-uninitialized-vqs.patch

* Fri Aug 30 2019 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- xhci: Fix memory leak in xhci_address_slot
- xhci: Fix memory leak in xhci_kick_epctx

* Wed Aug 7 2019 Huawei Technologies Co., Ltd. <zhang.zhanghailiang@huawei.com>
- hw/arm/virt: Introduce cpu topology support
- hw/arm64: add vcpu cache info support

* Tue Aug 6 2019 Huawei Technologies Co., Ltd. <zhang.zhanghailiang@huawei.com>
- Update release version to 4.0.0-2

* Mon Aug 5 2019 Huawei Technologies Co., Ltd. <zhang.zhanghailiang@huawei.com>
- enable make check
- smbios: Add missing member of type 4 for smbios 3.0

* Mon Aug 5 2019 fix CVE-2019-13164
- qemu-bridge-helper: restrict interface name to IFNAMSIZ
- qemu-bridge-helper: move repeating code in parse_acl_file

* Wed Jul 31 2019 backport from qemu upstream
- block: Fix AioContext switch for bs->drv == NULL
- cutils: Fix size_to_str() on 32-bit platforms
- qcow2: Avoid COW during metadata preallocation
- qcow2: Add errp to preallocate_co()
- qcow2: qcow2: Fix full preallocation with external data file
- qcow2: Fix qcow2_make_empty() with external data file
- megasas: fix mapped frame size
- kbd-state: fix autorepeat handling
- block/file-posix: Unaligned O_DIRECT block-status
- hw: add compat machines for 4.1
- q35: Revert to kernel irqchip
- hw: Nuke hw_compat_4_0_1 and pc_compat_4_0_1
- vl: Fix -drive / -blockdev persistent reservation management
- vhost: fix vhost_log size overflow during migration
- virtio-pci: fix missing device properties
- i386/acpi: fix gint overflow in crs_range_compare
- ioapic: kvm: Skip route updates for masked pins
- i386/acpi: show PCI Express bus on pxb-pcie expanders
- virtio-balloon: Fix wrong sign extension of PFNs
- virtio-balloon: Fix QEMU crashes on pagesize > BALLOON_PAGE_SIZE
- virtio-balloon: Simplify deflate with pbp
- virtio-balloon: Better names for offset variables in inflate/deflate code
- virtio-balloon: Rework pbp tracking data
- virtio-balloon: Use temporary PBP only
- virtio-balloon: virtio-balloon: don't track subpages for the PBP
- virtio-balloon: free pbp more aggressively

* Tue Jul 30 2019 Huawei Technologies Co., Ltd. <zhang.zhanghailiang@huawei.com
- qcow2: fix memory leak in qcow2_read_extensions
- hw/arm: expose host CPU frequency info to guest

* Fri Jul 26 2019 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- vhost: cancel migration when vhost-user restarted
- pl031: support rtc-timer property for pl031
- pl011: reset read FIFO when UARTTIMSC=0 & UARTICR=0xffff
- ARM64: record vtimer tick when cpu is stopped

* Tue Jul 23 2019 openEuler Buildteam <buildteam@openeuler.org> - version-release
- Package init
