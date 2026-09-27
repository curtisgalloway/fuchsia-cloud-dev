# fuchsia-cloud-dev

Build Fuchsia components and drivers in a Claude Code cloud container (or any
Linux x64 box) and run them on an emulated `core.x64`, with **no Fuchsia
checkout, no `jiri`, and no KVM**.

Everything is fetched anonymously: the Bazel SDK, clang and Bazel itself from
CIPD, the `core.x64` product bundle from GCS, and Fuchsia's QEMU build from CIPD.

## Quick start

```bash
./dev setup                              # skip in a cloud session: the SessionStart hook runs it
./dev emu start                          # ~1 min: boots core.x64 in QEMU (software emulation)
./dev test   //src/hello_world:test_pkg  # build + push + run tests      → 2 tests PASSED
./dev driver //src/qemu_edu/drivers:pkg  # build + push + load driver    → "edu device version major=1"
```

`./dev` with no arguments prints the command list.

## Measured (cloud container, 4 vCPU, no KVM, 2026-09-27)

| Step | Time |
|---|---|
| First `./dev setup` (Bazel, SDK, clang, QEMU, product bundle) | 8.5 min, ~15 GB |
| `./dev setup` again (all cached) | 0.5 s |
| `./dev emu start` | ~1 min |
| `./dev test` after an edit | ~15 s |
| `./dev driver` (first load) | ~45 s |
| `./dev driver` (reload; reboots the target) | ~2 min |

## Layout

| Path | What |
|---|---|
| `dev` | The CLI. Every workaround below lives here. |
| `.claude/hooks/session-start.sh` | Runs `./dev setup` in the background when a cloud session starts. |
| `MODULE.bazel`, `manifests/` | Pinned SDK, clang, rules_fuchsia and QEMU. |
| `third_party/fuchsia-infra-bazel-rules` | Submodule; provides the CIPD and Bazel bootstrap. |
| `src/hello_world` | Component sample, unchanged from `sdk-samples/getting-started`. |
| `src/qemu_edu` | Driver sample from `sdk-samples/drivers`, ported to the current SDK (below). |
| `.dev/` | Git-ignored state: QEMU, product bundle, package repository. |

## Session startup

The SessionStart hook runs `./dev setup` **asynchronously**: the session opens
immediately and setup continues in the background (output in
`.dev/setup.log`). Every other `./dev` command waits for it to finish, printing
`waiting for ./dev setup to finish` if it has not, and refuses to run if setup
failed. The hook takes the setup lock before going async, so there is no window
where a command can run ahead of it.

## Versions

The SDK version is pinned by `manifests/bazel_sdk.ensure` (a `git_revision`).
`./dev setup` reads the SDK's own version ID (currently `33.20260919.6.1`) and
downloads the `core.x64` image with that same version, so image and SDK always
match. To move both, update the `git_revision` in `bazel_sdk.ensure` and
`rules_fuchsia.ensure` and regenerate the `.resolved` files with
`.cipd_client ensure-file-resolve -ensure-file manifests/<name>.ensure`.

## Container workarounds (all in `dev`)

1. **No `ssh` client** → installs `openssh-client`; ffx talks to the target over ssh.
2. **QEMU rejects unix socket paths ≥ 108 bytes** → the ffx isolate dir is
   `~/.fcd-ffx`, not somewhere under the checkout.
3. **No IPv6** → the package server binds `127.0.0.1:8083`; ffx's default `[::]` fails.
4. **Running as root** → `MODULE.bazel` sets `ignore_root_user_error` for `rules_python`.
5. **Package manifests use execroot-relative paths** → `ffx repository publish`
   runs from `bazel info execution_root`.
6. **A registered driver URL sticks until reboot** → `./dev driver` reboots the
   target before re-registering.

## `qemu_edu` port

The upstream `sdk-samples/drivers` sample no longer builds or binds against
this SDK. Three changes:

1. `fuchsia.BIND_FIDL_PROTOCOL` was removed from the bind libraries.
2. The PCI bus now publishes each device as a composite node spec
   (`pci` + `acpi` parents). A non-composite bind rule registers but never
   binds. The rule is now `composite qemu_edu;` with `primary parent "pci"` and
   `optional parent "acpi"`, and the driver connects to the PCI service
   instance `"pci"` instead of `"default"`.
3. `fdf::DriverBase` was removed. The driver uses `fdf::DriverBase2`:
   constructor `DriverBase2("qemu-edu")`, `Start(fdf::DriverContext)`,
   `context.incoming()`, `FUCHSIA_DRIVER_EXPORT2`.

The sample's `tools/` (`eductl`) and `tests/` were not brought over yet.

## Known issues

- **`./dev run` starts a component but its logs are lost.** `core.x64` does not
  route `fuchsia.logger.LogSink` to the `ffx-laboratory` collection that
  `ffx component run` uses. Tests are not affected (test_manager routes it), so
  prefer `./dev test` for now.
- **Disk.** The Bazel cache is ~14 GB and a cloud session has about 30 GB free.
- **Speed.** No KVM means TCG emulation. The system is responsive (ffx
  commands take ~3 s) but CPU-heavy tests will be slow.

## License

BSD-style, as Fuchsia; see `LICENSE`. The samples under `src/` come from
Fuchsia's `sdk-samples` repositories.
