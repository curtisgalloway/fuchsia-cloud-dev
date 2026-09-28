#!/bin/bash
# Copyright 2026 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Prepares a Claude Code on the web container for Fuchsia development:
# toolchain, QEMU and product bundle (see ./dev setup). Setup runs in the
# background so the session starts at once; every other ./dev command waits
# for it to finish. Progress: .dev/setup.log. The emulator is not started here.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

exec "${CLAUDE_PROJECT_DIR}/dev" setup --background
