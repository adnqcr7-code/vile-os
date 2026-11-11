#!/bin/bash
# Vile ISO build12 (0.5.4) — mounts + mkarchiso in ONE call.
# Mounts do NOT persist across exec calls, so everything happens here.
# /dev setup (Nov 10, 2026 — this sandbox is a user namespace, uid_map
# 0 131072 65536): mknod is NOT permitted (all 7 fail, build10) AND a
# bind of the whole /dev directory fails with rc=32 "wrong fs type"
# (build11 — dir binds of a mount point with submounts are blocked).
# What DOES work here: bind-mounting individual device FILES
# (verified: mount --bind /dev/null /tmp/bt1/null → rc=0, char device).
# So: fresh tmpfs on $R/dev, touch the 7 target files, bind each host
# device file onto its target — the same chroot_bind_device pattern
# the chroot's own (patched Nov 10) pacstrap already uses one level
# down. That is what mkarchiso's pacstrap needs: its bind sources are
# the OUTER chroot's /dev/null, /dev/zero, ... — with build10/11 they
# did not exist, so pacstrap died with "special device /dev/zero
# does not exist". The earlier builds only succeeded because they
# reused a work dir whose pacstrap stage was already done.
# Resumes the build10 work dir (airootfs copy stage already done).
set -x
R=/home/hatch/workspace/vile-os/build-env/root.x86_64
# --- CA trust (Nov 11, 2026 — the sandbox egress CA ROTATES: C1:06…,
# then 37:BD:8F:38… (0.5.5), now 0C:B0:1E:CB…). mkarchiso's pacstrap
# verifies mirror SSL against the CHROOT's trust store, NOT the
# host's SSL_CERT_FILE bundle, so a rotated CA breaks the build with
# "SSL certificate OpenSSL verify result: self-signed certificate in
# certificate chain (19)". Copy the CURRENT host egress CA into the
# chroot's anchors and regenerate the bundle before building — this
# makes the build self-healing on the next rotation. (The chroot
# keeps the old CAs too; only the current one matters.)
if [ -f /run/hatch/egress-tls/hatch-egress-ca.pem ]; then
  cp /run/hatch/egress-tls/hatch-egress-ca.pem \
    "$R/etc/ca-certificates/trust-source/anchors/hatch-egress-ca.crt"
  chroot "$R" update-ca-trust 2>&1
fi
# --- mounts (plain chroot; arch-chroot does not work here) ---
mount -t proc proc "$R/proc" 2>&1
mount --bind /sys "$R/sys" 2>&1
# /dev: fresh tmpfs + per-device file binds (see header — mknod and
# whole-dir binds are both blocked in this user namespace).
mount -t tmpfs tmpfs "$R/dev" 2>&1
for d in null zero full random urandom tty ptmx; do
  touch "$R/dev/$d" 2>&1
  mount --bind "/dev/$d" "$R/dev/$d" 2>&1
done
mkdir -p "$R/dev/pts" "$R/dev/shm" 2>&1
mount -t devpts devpts "$R/dev/pts" 2>&1
mount -t tmpfs tmpfs "$R/dev/shm" 2>&1
ln -sf /proc/self/fd "$R/dev/fd" 2>&1
ln -sf /proc/self/fd/0 "$R/dev/stdin" 2>&1
ln -sf /proc/self/fd/1 "$R/dev/stdout" 2>&1
ln -sf /proc/self/fd/2 "$R/dev/stderr" 2>&1
mkdir -p "$R/run" 2>&1
mount --bind /run "$R/run" 2>&1 || true
# Egress CA bundle (build16 fix): curl/pacman inside the chroot inherit
# SSL_CERT_FILE=/run/hatch/egress-tls/ca-bundle.pem from the sandbox env,
# but the /run dir-bind above fails (rc=32, mount point with submounts),
# so that path does not exist in the chroot and curl dies with error 77
# before ever trying the system bundle. Bind the FILE (file binds work
# in this user namespace) so the live bundle is visible in the chroot.
mkdir -p "$R/run/hatch/egress-tls" 2>&1
touch "$R/run/hatch/egress-tls/ca-bundle.pem" 2>&1
mount --bind /run/hatch/egress-tls/ca-bundle.pem "$R/run/hatch/egress-tls/ca-bundle.pem" 2>&1
# --- disk quota (Nov 11, 2026 — build21b died at mksquashfs 17%):
# the HOME filesystem has a USER quota (df shows 52G free but writes
# fail once headroom is gone; the build peaks at ~6G: 3G airootfs +
# 1.5G .sfs + 1.5G ISO). /run/hatch/resume is the SAME disk but a
# different mount, outside the home quota (52G free, writable).
# Bind it over the chroot's work/output dirs so the big writes land
# there; only the final ISO (1.5G) is copied back to the home.
# (Mounts do not persist across exec calls or VM resets — this block
# re-establishes them every run. The cleanup below does NOT unmount
# these two; they are unmounted by hand after the ISO is copied out.)
mkdir -p /run/hatch/resume/vile-iso-work /run/hatch/resume/vile-iso-out 2>&1
mkdir -p "$R/tmp/vile-iso-work" "$R/root/vile-iso-out" 2>&1
mount --bind /run/hatch/resume/vile-iso-work "$R/tmp/vile-iso-work" 2>&1 || true
mount --bind /run/hatch/resume/vile-iso-out "$R/root/vile-iso-out" 2>&1 || true
# --- build ---
chroot "$R" mkarchiso -v -w /tmp/vile-iso-work -o /root/vile-iso-out /root/vile-iso
RC=$?
echo "MKARCHISO EXIT: $RC"
# --- unmount (best effort, reverse order; device file binds first or
# the tmpfs unmount fails EBUSY and mounts stack up across runs) ---
umount "$R/run/hatch/egress-tls/ca-bundle.pem" 2>/dev/null
umount "$R/run" 2>/dev/null
umount "$R/dev/pts" 2>/dev/null; umount "$R/dev/shm" 2>/dev/null
for d in null zero full random urandom tty ptmx; do
  umount "$R/dev/$d" 2>/dev/null
done
umount "$R/dev" 2>/dev/null; umount "$R/sys" 2>/dev/null; umount "$R/proc" 2>/dev/null
exit $RC
