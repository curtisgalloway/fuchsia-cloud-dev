#!/bin/bash
# Copyright 2026 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Prepares a Claude Code on the web container for Fuchsia development:
# toolchain, QEMU and product bundle (see ./dev setup). The emulator itself is
# not started here; `./dev emu start` boots it in about a minute.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

cd "${CLAUDE_PROJECT_DIR}"
./dev setup
