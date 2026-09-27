# AGENTS.md

Fuchsia component and driver development without a Fuchsia checkout: Bazel SDK
builds, run on an emulated `core.x64` (QEMU, no KVM). Read `README.md` for the
workarounds and measured timings.

## Loop

```bash
./dev emu start                          # once per session, ~1 min (waits for background setup)
./dev test   //src/<pkg>:test_pkg        # the main dev loop; ~15 s after an edit
./dev driver //src/<drv>/drivers:pkg     # load a driver; reloading reboots the target (~2 min)
./dev log <filter>                       # target logs
./dev ffx <args>                         # any other ffx command, already pointed at the emulator
```

- Setup runs in the background at session start (first session ~9 min,
  cached afterwards). `./dev` commands wait for it; `tail -f .dev/setup.log`
  shows progress. Don't run `tools/bazel` directly until it has finished.
- Always go through `./dev ffx`, never a bare `ffx`: it sets the short isolate
  dir that QEMU's socket-path limit needs.
- Prefer `./dev test` to `./dev run`: components started with `run` cannot log
  (see README "Known issues").
- New drivers for PCI devices must use **composite** bind rules
  (`primary parent "pci"`, `optional parent "acpi"`) and `fdf::DriverBase2`.
  `src/qemu_edu` is the working reference.
- The SDK is at API level 31 (`.bazelrc`). Upstream docs and samples often use
  APIs that have since been removed; check the headers under
  `$(tools/bazel info output_base)/external/fuchsia_infra++cipd_ext+fuchsia_sdk/`.

## Conventions

- `dev` and the hook are bash; keep them `shellcheck`-clean.
- The image version is derived from the SDK pin; never pin it separately.
- `.dev/` is disposable cache; `./dev setup` rebuilds it.
