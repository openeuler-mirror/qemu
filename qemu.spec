Name: qemu
Version: 4.0.1
Release: 11
Epoch: 2
Summary: QEMU is a generic and open source machine emulator and virtualizer
License: GPLv2 and BSD and MIT and CC-BY
URL: http://www.qemu.org
Source0: https://www.qemu.org/download/%{name}-%{version}%{?rcstr}.tar.xz
Source1: 80-kvm.rules
Source2: 99-qemu-guest-agent.rules
Source3: bridge.conf

Patch0001: qxl-check-release-info-object.patch
Patch0002: ARM64-record-vtimer-tick-when-cpu-is-stopped.patch
Patch0003: pl011-reset-read-FIFO-when-UARTTIMSC-0-UARTICR-0xfff.patch
Patch0004: pl031-support-rtc-timer-property-for-pl031.patch
Patch0005: vhost-cancel-migration-when-vhost-user-restarted.patch
Patch0006: qcow2-fix-memory-leak-in-qcow2_read_extensions.patch
Patch0007: hw-arm-expose-host-CPU-frequency-info-to-guest.patch
Patch0008: qemu-bridge-helper-restrict-interface-name-to-IFNAMS.patch
Patch0009: qemu-bridge-helper-move-repeating-code-in-parse_acl.patch
Patch0010: smbios-Add-missing-member-of-type-4-for-smbios-3.0.patch
Patch0011: hw-arm-virt-Introduce-cpu-topology-support.patch
Patch0012: hw-arm64-add-vcpu-cache-info-support.patch
Patch0013: xhci-Fix-memory-leak-in-xhci_address_slot.patch
Patch0014: xhci-Fix-memory-leak-in-xhci_kick_epctx.patch
Patch0015: ehci-fix-queue-dev-null-ptr-dereference.patch
Patch0016: memory-unref-the-memory-region-in-simplify-flatview.patch
Patch0017: util-async-hold-AioContext-ref-to-prevent-use-after-free.patch
Patch0018: vhost-user-scsi-prevent-using-uninitialized-vqs.patch
Patch0019: cpu-add-Kunpeng-920-cpu-support.patch
Patch0020: cpu-parse-feature-to-avoid-failure.patch
Patch0021: cpu-add-Cortex-A72-processor-kvm-target-support.patch
Patch0022: vnc-fix-memory-leak-when-vnc-disconnect.patch
Patch0023: pcie-disable-the-PCI_EXP_LINKSTA_DLLA-cap.patch
Patch0024: linux-headers-update-against-KVM-ARM-Fix-256-vcpus.patch
Patch0025: intc-arm_gic-Support-IRQ-injection-for-more-than-256.patch
Patch0026: ARM-KVM-Check-KVM_CAP_ARM_IRQ_LINE_LAYOUT_2-for-smp_.patch
Patch0027: 9pfs-local-Fix-possible-memory-leak-in-local_link.patch
Patch0028: scsi-disk-define-props-in-scsi_block_disk-to-avoid-memleaks.patch
Patch0029: arm-translate-a64-fix-uninitialized-variable-warning.patch
Patch0030: nbd-fix-uninitialized-variable-warning.patch
Patch0031: xhci-Fix-memory-leak-in-xhci_kick_epctx-when-poweroff.patch
Patch0032: block-fix-memleaks-in-bdrv_refresh_filename.patch
Patch0033: iscsi-Cap-block-count-from-GET-LBA-STATUS-CVE-2020-1.patch
Patch0034: tcp_emu-Fix-oob-access.patch
Patch0035: slirp-use-correct-size-while-emulating-IRC-commands.patch
Patch0036: slirp-use-correct-size-while-emulating-commands.patch
Patch0037: tcp_emu-fix-unsafe-snprintf-usages.patch
Patch0038: block-iscsi-use-MIN-between-mx_sb_len-and-sb_len_wr.patch
Patch0039: monitor-fix-memory-leak-in-monitor_fdset_dup_fd_find.patch
Patch0040: vhost-Fix-memory-region-section-comparison.patch
Patch0041: memory-Align-MemoryRegionSections-fields.patch
Patch0042: memory-Provide-an-equality-function-for-MemoryRegion.patch
Patch0043: file-posix-Handle-undetectable-alignment.patch
Patch0044: block-backup-fix-max_transfer-handling-for-copy_rang.patch
Patch0045: block-backup-fix-backup_cow_with_offload-for-last-cl.patch
Patch0046: qcow2-Limit-total-allocation-range-to-INT_MAX.patch
Patch0047: mirror-Do-not-dereference-invalid-pointers.patch
Patch0048: COLO-compare-Fix-incorrect-if-logic.patch
Patch0049: qcow2-bitmap-Fix-uint64_t-left-shift-overflow.patch
Patch0050: pcie-Add-pcie-root-port-fast-plug-unplug-feature.patch
Patch0051: pcie-Compat-with-devices-which-do-not-support-Link-W.patch
Patch0052: aio-wait-delegate-polling-of-main-AioContext-if-BQL-not-held.patch
Patch0053: async-use-explicit-memory-barriers.patch
Patch0054: Fix-use-afte-free-in-ip_reass-CVE-2020-1983.patch

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
rm -rf %{buildroot}%{_datadir}/%{name}/linuxboot_dma.bin
rm -rf %{buildroot}%{_datadir}/%{name}/pvh.bin
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

%ifarch %{ix86} x86_64
%files seabios
%{_datadir}/%{name}/bios-256k.bin
%{_datadir}/%{name}/bios.bin
%endif

%changelog
* Fri Apr 24 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- Fix use-afte-free in ip_reass() (CVE-2020-1983)

* Sat Apr 11 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- aio-wait: delegate polling of main AioContext if BQL not held
- async: use explicit memory barriers

* Wed Mar 18 2020 Huawei Technologies Co., Ltd. <fangying1@huawei.com>
- pcie: Add pcie-root-port fast plug/unplug feature
- pcie: Compat with devices which do not support Link Width

* Tue Mar 17 2020 Huawei Technologies Co., Ltd. <zhang.zhanghailiang@huawei.com>
- Put linuxboot_dma.bin and pvh.bin in x86 package

* Mon Mar 16 2020 backport some bug fix patches from upstream
- Patch from number 0040 to 0049 are picked from stable-4.1.1

* Mon Mar 16 2020 Huawei Technologies Co., Ltd. <kuhn.chenqun@huawei.com>
- moniter: fix memleak in monitor_fdset_dup_fd_find_remove
- block/iscsi: use MIN() between mx_sb_len and sb_len_wr

* Wed Mar 11 2020 backport from qemu upstream
- tcp_emu: Fix oob access
- slirp: use correct size while emulating IRC commands
- slirp: use correct size while emulating commands
- tcp_emu: fix unsafe snprintf() usages

* Mon Mar 9 2020 backport from qemu upstream
- iscsi: Cap block count from GET LBA STATUS (CVE-2020-1711)

* Thu Feb  6 2020 Huawei Technologies Co., Ltd. <zhang.zhanghailiang@huawei.com>
- spec: remove fno-inline option for configure

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
- tpm: Exit in reset when backend indicates failure
- tpm_emulator: Translate TPM error codes to strings

* Thu Oct 17 2019 backport from qemu upstream
- vnc-fix-memory-leak-when-vnc-disconnect.patch

* Mon Sep  9 2019 backport from qemu upstream
- ehci-fix-queue-dev-null-ptr-dereference.patch
- memory-unref-the-memory-region-in-simplify-flatview.patch
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

