#!/bin/bash
# Copyright 2026 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# gce.sh: run this repo on a Google Compute Engine VM with nested
# virtualization, so the emulator gets KVM.
#
#   scripts/gce.sh create        create the VM, clone the repo, start ./dev setup
#   scripts/gce.sh ssh [CMD...]  ssh in (CMD runs in the checkout)
#   scripts/gce.sh stop|start    stop (disk is kept, compute billing stops) / start
#   scripts/gce.sh delete        delete the VM and its disk
#   scripts/gce.sh status        show the VM's state
#
# Environment:
#   FCD_GCE_PROJECT   GCP project (default: gcloud's configured project)
#   FCD_GCE_ZONE      zone (default: us-central1-a)
#   FCD_GCE_NAME      VM name (default: fuchsia-dev)
#   FCD_GCE_MACHINE   machine type (default: n2-standard-8; must be Intel or
#                     AMD, and not E2, for nested virtualization)
#   FCD_GCE_DISK_GB   boot disk size (default: 100; setup uses ~15 GB)
#   FCD_GCE_SUBNET    subnet to attach to (default: the network named "default");
#                     the VM needs an external IP or Cloud NAT to fetch setup
#   FCD_GCE_IAP       1 to ssh through IAP, for networks that allow port 22
#                     only from 35.235.240.0/20 (default: 0)
#   FCD_GCE_REPO      git URL to clone (default: this checkout's origin)
#   FCD_GCE_BRANCH    branch to clone (default: main)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${FCD_GCE_PROJECT:-$(gcloud config get-value project 2>/dev/null)}"
ZONE="${FCD_GCE_ZONE:-us-central1-a}"
NAME="${FCD_GCE_NAME:-fuchsia-dev}"
MACHINE="${FCD_GCE_MACHINE:-n2-standard-8}"
DISK_GB="${FCD_GCE_DISK_GB:-100}"
SUBNET="${FCD_GCE_SUBNET:-}"
IAP="${FCD_GCE_IAP:-0}"
REPO="${FCD_GCE_REPO:-$(git -C "${ROOT}" remote get-url origin)}"
BRANCH="${FCD_GCE_BRANCH:-main}"
CHECKOUT="fuchsia-cloud-dev"

log() { echo "gce: $*" >&2; }
die() { log "error: $*"; exit 1; }

g() {
  [[ -n "${PROJECT}" ]] || die "no project: set FCD_GCE_PROJECT or run gcloud config set project"
  gcloud --project "${PROJECT}" "$@"
}

vm_ssh() {
  local iap=()
  [[ "${IAP}" == 1 ]] && iap=(--tunnel-through-iap)
  g compute ssh "${NAME}" --zone "${ZONE}" "${iap[@]}" "$@"
}

# Runs once on the VM as the ssh user. /dev/kvm is group kvm on Ubuntu; the
# group change applies to later logins, which is when ./dev emu start runs.
remote_provision() {
  cat <<EOF
set -euo pipefail
sudo usermod -aG kvm "\$(id -un)"
if [[ ! -d ${CHECKOUT} ]]; then
  git clone --quiet --recurse-submodules --branch ${BRANCH} ${REPO} ${CHECKOUT}
fi
cd ${CHECKOUT}
./dev setup --background
EOF
}

cmd_create() {
  local net=()
  [[ -n "${SUBNET}" ]] && net=(--subnet "${SUBNET}")
  log "creating ${NAME} (${MACHINE}, ${DISK_GB} GB) in ${PROJECT}/${ZONE}"
  g compute instances create "${NAME}" --zone "${ZONE}" \
    --machine-type "${MACHINE}" \
    --enable-nested-virtualization \
    --image-family ubuntu-2404-lts-amd64 --image-project ubuntu-os-cloud \
    --boot-disk-size "${DISK_GB}GB" --boot-disk-type pd-balanced "${net[@]}"
  log "waiting for sshd"
  local i
  for i in $(seq 1 30); do
    if vm_ssh --command true -- -o ConnectTimeout=5 >/dev/null 2>&1; then break; fi
    (( i < 30 )) || die "ssh to ${NAME} did not come up"
    sleep 10
  done
  vm_ssh --command "$(remote_provision)"
  log "setup is running on the VM (first time ~9 min). Next:"
  log "  scripts/gce.sh ssh ./dev emu start"
}

cmd_ssh() {
  if [[ $# -eq 0 ]]; then
    vm_ssh
  else
    local q
    printf -v q '%q ' "$@"
    vm_ssh --command "cd ${CHECKOUT} && ${q}"
  fi
}

main() {
  local cmd="${1:-help}"
  shift || true
  case "${cmd}" in
    create) cmd_create ;;
    ssh) cmd_ssh "$@" ;;
    stop) g compute instances stop "${NAME}" --zone "${ZONE}" ;;
    start) g compute instances start "${NAME}" --zone "${ZONE}" ;;
    delete) g compute instances delete "${NAME}" --zone "${ZONE}" ;;
    status)
      g compute instances describe "${NAME}" --zone "${ZONE}" \
        --format 'value(name,status,machineType.basename(),networkInterfaces[0].accessConfigs[0].natIP)'
      ;;
    *) sed -n '/^# gce.sh:/,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//' ;;
  esac
}

main "$@"
