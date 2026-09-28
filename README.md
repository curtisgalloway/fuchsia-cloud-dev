# fuchsia-cloud-dev

Build Fuchsia components and drivers in a Claude Code cloud container (or any
Linux x64 box) and run them on an emulated `core.x64`, with **no Fuchsia
checkout, no `jiri`, and no KVM**.

Everything is fetched anonymously: the Bazel SDK, clang and Bazel itself from
CIPD, the `core.x64` product bundle from GCS, and Fuchsia's QEMU build from CIPD.

## Use this for your own project

1. **Copy the repo.** On GitHub, click **Use this template** for a clean copy
   of your own (it can be private), or **Fork** if you want to pull later
   fixes from this repo or send changes back.
2. **Give Claude access.** Your copy must be reachable by the Claude GitHub
   app (install it on your account or organization if you haven't).
3. **Check the environment's network access** (next section). This is the one
   setting most likely to trip up a first session.
4. **Start a Claude Code cloud session on your copy** and ask, in plain words,
   for example: *"boot the emulator and run the hello world tests"*. Setup
   runs in the background when the session opens (about 9 minutes the first
   time, seconds afterwards); Claude waits for it automatically.
5. **Add your code under `src/`.** Copy `src/hello_world` for a component or
   `src/qemu_edu` for a driver. `AGENTS.md` gives Claude the rules this image
   imposes on both, so you can ask for "a driver for …" directly.

In a cloud session you don't type shell commands yourself: you ask Claude to
run them. The `./dev …` commands below are what Claude runs.

### Network access

Setup downloads everything anonymously over HTTPS. A cloud environment's
**Network access** setting decides which hosts it may reach. A fresh setup
contacts these hosts (measured by routing a first-session setup, emulator boot
and test run through a logging proxy on 2026-09-27):

| Host | What for |
|---|---|
| `chrome-infra-packages.appspot.com` | CIPD: Bazel, the Fuchsia SDK, clang, `rules_fuchsia`, QEMU |
| `storage.googleapis.com` | CIPD package contents and the `core.x64` product bundle |
| `fuchsia.googlesource.com` | the `third_party/fuchsia-infra-bazel-rules` submodule |
| `bcr.bazel.build` | Bazel Central Registry (module metadata) |
| `github.com` | Bazel module archives and `rules_python`'s Python toolchain |
| `release-assets.githubusercontent.com` | where those GitHub downloads are served from |
| `archive.ubuntu.com`, `security.ubuntu.com` | `apt` install of `openssh-client`, only if the container lacks `ssh` (the Claude cloud image did). Not measured: taken from the container's apt sources. |

If one is blocked, setup stops; the error at the end of `.dev/setup.log`
usually names the host, and every other `./dev` command reports that setup
did not complete.
To fix it, open the cloud environment's settings (the environment menu in the
session's title bar, then **Edit**) and either choose a broader **Network
access** level or add the missing hosts to its allowed domains. The levels are
described at <https://code.claude.com/docs/en/claude-code-on-the-web>. Then
start a new session, or ask Claude to rerun `./dev setup`.

**Behind a TLS-inspecting proxy** (one that re-signs HTTPS with its own root
CA), install that CA into the OS trust store (`update-ca-certificates`).
`./dev` exports `SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt` if it is
unset, and when `/etc/ssl/certs/java/cacerts` exists (the
`ca-certificates-java` package) setup writes `.dev/bazelrc` so Bazel's bundled
JDK uses that trust store. Without it, Bazel fails with `PKIX path building
failed`.

After setup, the edit/build/test loop is local: building, booting the
emulator, publishing packages and running tests contact no outside hosts.

## Quick start

```bash
./dev setup                              # skip in a cloud session: the SessionStart hook runs it
./dev emu start                          # ~1 min: boots core.x64 in QEMU (software emulation)
./dev test   //src/hello_world:test_pkg  # build + push + run tests      → 2 tests PASSED
./dev driver //src/qemu_edu/drivers:pkg  # build + push + load driver    → "edu device version major=1"
./dev run    //src/qemu_edu/tools:pkg    # eductl liveness check         → "Liveness check passed!"
./dev test   //src/qemu_edu/tests:pkg --realm /core/testing:devices-tests   # driver system test
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
| `src/hello_world` | Component sample from `sdk-samples/getting-started`; manifest adjusted for logging (below). |
| `src/qemu_edu` | Driver sample from `sdk-samples/drivers` (driver, `eductl` tool, system test), ported to the current SDK (below). |
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
   As a non-root user this uses `sudo -n`; without passwordless sudo, setup
   stops and asks you to install `openssh-client` yourself.
2. **QEMU rejects unix socket paths ≥ 108 bytes** → the ffx isolate dir is
   `~/.fcd-ffx`, not somewhere under the checkout.
3. **No IPv6** → the package server binds `127.0.0.1:8083`; ffx's default `[::]` fails.
4. **Running as root** → `MODULE.bazel` sets `ignore_root_user_error` for `rules_python`.
5. **Package manifests use execroot-relative paths** → `ffx repository publish`
   runs from `bazel info execution_root`.
6. **A registered driver URL sticks until reboot** → `./dev driver` reboots the
   target before re-registering.

## `qemu_edu` port

The upstream `sdk-samples/drivers` sample no longer builds, binds or connects
against this SDK and image. Changes:

1. `fuchsia.BIND_FIDL_PROTOCOL` was removed from the bind libraries.
2. The PCI bus now publishes each device as a composite node spec
   (`pci` + `acpi` parents). A non-composite bind rule registers but never
   binds. The rule is now `composite qemu_edu;` with `primary parent "pci"` and
   `optional parent "acpi"`, and the driver connects to the PCI service
   instance `"pci"` instead of `"default"`.
3. `fdf::DriverBase` was removed. The driver uses `fdf::DriverBase2`:
   constructor `DriverBase2("qemu-edu")`, `Start(fdf::DriverContext)`,
   `context.incoming()`, `FUCHSIA_DRIVER_EXPORT2`.
4. **Clients use devfs, not the driver's FIDL service.** The driver still
   serves `examples.qemuedu.Service`, but a prebuilt image routes only the
   driver services its product configuration names, so nothing outside the
   driver collection can reach it. The driver therefore also publishes a devfs
   node, and `eductl` and the system test open the first entry in
   `/dev/class/test`. Components only see a fixed set of devfs class names;
   `qemu-edu` would be silently absent, so the node uses the generic `test`
   class. `dev-class` must be used with `availability: "optional"`, since that
   is how `core` offers it.
5. **`eductl` is a component**, because `ffx driver run-tool` no longer
   exists. It runs in `ffx-laboratory` with a fixed `live` argument. Other
   commands run in its namespace with:

   ```bash
   ./dev run //src/qemu_edu/tools:pkg     # once, to create the instance
   ./dev ffx component explore core/ffx-laboratory:eductl -l namespace \
       -c '/pkg/bin/eductl fact 12'       # → Factorial(12) = 479001600
   ```
6. **The system test runs in `/core/testing:devices-tests`.** The
   `fuchsia.test` `type: "devices"` facet was removed, and a test with no
   `--realm` runs hermetically without `/dev`.

## Logging from `./dev run`

`ffx component run` puts components in `core`'s `ffx-laboratory` collection.
On this platform build, `core` offers `fuchsia.logger.LogSink` there only
inside the `diagnostics` dictionary. The SDK's `syslog/client.shard.cml` uses
`LogSink` directly from the parent, so a component that includes it starts but
cannot log. Components meant for `./dev run` should instead declare:

```json5
use: [
    { protocol: "fuchsia.logger.LogSink", from: "parent/diagnostics" },
],
```

as `src/hello_world/meta/hello_world.cml` does. `test_manager` offers both
forms, so the same manifest works under `./dev test` too. `ffx component run`
also prints a harmless `Failed to connect to PackageCache` warning.

## Limits

**Good for:** building new, generic components, and drivers for QEMU's `edu`
device, and running them on a stock `core.x64` image. Anything that needs
real hardware, arm64, or a change to what the image ships belongs on a lab
device.

What this setup cannot do, so you can tell early whether a task fits it:

- **x64 only at run time.** Only `core.x64` boots. You can build for arm64 with
  the SDK, but nothing here runs arm64 code. arm64 drivers need real hardware.
- **A prebuilt image, not an assembled one.** You cannot re-assemble the
  product, add packages to base, or change the board configuration. Drivers are
  loaded only ephemerally with `ffx driver register`. So you cannot replace a
  driver the image already ships, or test anything that needs a
  board-assembly change.
- **One extra emulated device.** `./dev emu start` adds QEMU's `edu` device
  (PCI `1234:11e8`), and nothing else. A driver can bind only to that device
  or to hardware QEMU already emulates for `core.x64`. Other devices need a
  change to the emulator arguments in `dev`.
- **Released SDK versions only, and not always the newest.** The SDK is pinned
  through the CIPD package `fuchsia/sdk/core/fuchsia-bazel-rules/linux-amd64`,
  and that package is not published for every release. On 2026-09-27 it had
  no instance for `33.20260927.4.1`, the latest release. Also, its
  `git_revision` tags name commits in Fuchsia's private integration repo, not
  `fuchsia.git` commits. To find a release's `fuchsia.git` commit, read the
  `source_manifest.json` of any build listed in
  `gs://fuchsia/development/<version>/product_bundles.json`.
- **C++ only.** The Fuchsia SDK has no Rust (or other language) toolchain or
  libraries, so `./dev` builds C++ components and drivers only.

## Known issues

- **Disk.** The Bazel cache is ~14 GB and a cloud session has about 30 GB free.
- **Speed.** No KVM means TCG emulation. The system is responsive (ffx
  commands take ~3 s) but CPU-heavy tests will be slow.

## License

BSD-style, as Fuchsia; see `LICENSE`. The samples under `src/` come from
Fuchsia's `sdk-samples` repositories.
