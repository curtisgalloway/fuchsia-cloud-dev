#!/bin/bash
# Copyright 2026 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Prepares a Claude Code on the web container for Fuchsia development:
# toolchain, QEMU and product bundle (see ./dev setup). Runs asynchronously so
# the session starts at once; every other ./dev command waits for setup to
# finish. Progress: .dev/setup.log. The emulator is not started here.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

cd "${CLAUDE_PROJECT_DIR}"
mkdir -p .dev
# Lock before going async so no ./dev command can run ahead of setup.
exec 9>.dev/setup.lock
flock 9

echo '{"async": true, "asyncTimeout": 1200000}'

# Setup runs without fd 9, so only this hook holds the lock and it is released
# when setup ends (a daemon setup starts, like bazel's server, can't keep it).
FCD_SETUP_LOCK_HELD=1 ./dev setup > .dev/setup.log 2>&1 9>&-
