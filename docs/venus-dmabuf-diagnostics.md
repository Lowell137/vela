# Venus DMA-BUF import diagnostics (PR #19)

This is a temporary, opt-in **diagnostic** path, not a compatibility layer.
Vela still requires Vulkan 1.4 and supported DMA-BUF formats/modifiers.

## Reproduce in an affected QEMU / virtio-gpu Venus guest

Install the build from the PR on the *guest*. From a TTY, stop the display
manager before launching a single supervised Vela session:

```sh
sudo systemctl stop sddm
VELA_DEBUG_DMABUF=1 /usr/bin/vela-session
```

If it crashes, retain the **complete** `${XDG_STATE_HOME:-$HOME/.local/state}/vela/vela.log`,
not only the last few lines. The session supervisor may restart the compositor,
so distinguish separate attempts. You can inspect the relevant events with:

```sh
grep -E 'DMABUF trace:|DMABUF import:|Renderer:|compositor crashed' \
  "${XDG_STATE_HOME:-$HOME/.local/state}/vela/vela.log" | tail -n 150
```

The last `before` without a matching `after` *localizes* a potentially
aborting driver call; it does **not** establish its root cause. In particular:

- `initial wlr_output_commit_state`: mode/DRM backend path, potentially
  before any GBM output buffer was created.
- `GBM allocated`, `renderer target`: exported DMA-BUF format, modifier and
  planes; correlate `dev` / `ino` between GBM and the Vulkan importer.
- `vkGetPhysicalDeviceImageFormatProperties2`: supported format/modifier,
  usage flags, query result and IMPORTABLE capability.
- `vkGetMemoryFdPropertiesKHR`: DMA-BUF FD capabilities and compatible
  memory-type bits. A failed result is now treated as an import failure.
- `vkAllocateMemory`, `vkBindImageMemory`, `vkCreateImageView`: import and
  binding stages; errors clean up resources rather than returning success.
- `frame wlr_output_commit_state` and `scanout wlr_output_commit_state`:
  actual presentation paths, which may fail after a successful Vulkan import.

The numeric file descriptor only identifies an open handle *in one process*;
the `dev` / `ino` combination identifies the backing object across duplicate
handles. Do not compare FD numbers between host and guest.

## Controlled comparisons

Change **one variable per run**, retaining logs of each run:

```sh
VELA_DEBUG_DMABUF=1 VELA_DEBUG_LINEAR=1 /usr/bin/vela-session
VELA_DEBUG_DMABUF=1 VELA_VULKAN_VALIDATION=1 /usr/bin/vela-session
```

`VELA_DEBUG_LINEAR=1` forces a linear GBM modifier only when that modifier
was advertised; it is not a Vulkan fallback. The validation-layer run
requires `VK_LAYER_KHRONOS_validation` in the guest and may change timing.
When disabled, `VELA_DEBUG_DMABUF` must not alter the rendering path.

Collect the guest's `vulkaninfo --summary`, kernel, Mesa and Vulkan driver
versions, plus the QEMU / virtio-gpu device configuration. From the host
collect QEMU / virglrenderer version and the contemporaneous `vkr:` error
messages. Correlate timestamps, but do not assume the host error was caused
by the last guest-side call.

For a SIGABRT, use the guest's core dump if available:

```sh
coredumpctl info vela-compositor
coredumpctl debug vela-compositor
```

The backtrace is important if the abort happens *within* `libvulkan_virtio.so`.
Do not enable an automatic Venus-specific workaround from the diagnostic log.

## Verification

On a Vulkan 1.4-capable development machine, run:

```sh
ctest --test-dir build -R '^functional$' --output-on-failure
```

`DmabufDiagnostics` in `tests/functional/test_session.py` checks that the
opt-in trace contains output, allocation, import, target and frame boundaries,
and that the trace is absent with the opt-in flag disabled. CI can skip the
functional suite on runners without a suitable Vulkan GPU. A successful
headless run **does not** replace a real Venus guest reproduction.
