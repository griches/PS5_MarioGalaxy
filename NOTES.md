# Super Mario Galaxy for the PlayStation 5 — working notes

Handoff notes for whoever picks this up next (a new session after compaction).
State as of 2026-10-20 ~13:00.

## Where it stands

- **2026-10-20 ~13:30: the title screen renders on the PS5 at 3840x2160** with Nebula's own
  Direct3D 12 renderer running unchanged on **vkd3d 2.1** (Wine's Direct3D 12 on Vulkan) over
  the RADV linked into the eboot. Gary was alerted (sound + push). Title PPSA99315
  "Super Mario Galaxy", console 192.168.0.190.
- Before that (commit `5ce9765`): boots at 60 frames/s with no picture (null renderer).
- Not yet checked: frame rate with the renderer, input past the title screen, in-game scenes,
  shader-compile hitches (the first frames hitched ~1.3 s while pipelines compiled).

## Gary's working rules for this port

- A short update **every minute** during long work; honest hour estimates when asked.
- Test runs are **short** (`console.sh run 30`) and stop at the first failure; never sit out
  a long timeout on a dead run.
- **When a title screen first displays: make an audible notification for Gary** so he can
  go and look — `afplay /System/Library/Sounds/Glass.aiff; say "Mario Galaxy title screen is up"`
  on the Mac, plus a PushNotification.
- Close his game without asking when a build or test needs the console (only one title
  runs at a time). Never push to a public remote unless he asks. The built eboot contains
  code translated from his disc: never distribute it.

## Layout

```
vendor/nebula/            keepvaibin/nebula (GPL-3.0) submodule at f02780d, with my edits
                          in its working tree; saved as ps5/patches/nebula.patch
                          (git -C vendor/nebula diff > ps5/patches/nebula.patch).
                          Some upstream files are CRLF: edit them byte-for-byte (python
                          open(...,'rb')), never let a tool rewrite line endings.
ps5/CMakeLists.txt        the whole title (runtime + compat + generated modules + platform)
ps5/compat/include        windows.h and ~25 Win32 headers, msvc_crt.h (force-included in
                          every unit), cxx_polyfill.h (std::atomic_ref for LLVM 18 libc++)
ps5/compat/src            the Win32 implementations (handles, fibers, files, exceptions,
                          system, window, bcrypt, xinput, xaudio2)
ps5/gx/gx_null.cpp        the GX API with no picture (PE events only) - to be replaced
ps5/gx/include/galaxy/gx/renderer_d3d12.h   shadows Nebula's D3D12 header for the front
                          end (vertex_loader.cpp needs UploadRing); CPU memory for now
ps5/src                   platform.c (klog, pad, audio out), main.cpp (wmain on a 256 MiB
                          stack thread, stream rebinding, stderr tee, memory guard),
                          modules.cpp (game/home exports for LoadLibrary), paths.c, shims.c,
                          startup.c (env.txt)
ps5/tools                 vm.sh build|check, console.sh deploy|game-files|run|env|quit,
                          link-title.sh (PS5_Vulkan's recipe + RADV), setup-sdk.sh
dump/, gamedata/          game files, never committed (see below)
```

Game files: `dump/*.wbfs` (from `~/Downloads/Super Mario Galaxy (USA, Canada) (En,Fr,Es).7z`,
RMGE01 rev 0), `dump/disc` (`nebula-recomp extract`), `gamedata/generated` (`generate`:
782 C++ files), `gamedata/content` (`package-content`: game.pak 4.3 GB + loose files) and
`gamedata/RMGE01_boot_image.bin` (`build-boot-image`). The recompiler is built on the Mac
(`cargo build --release -p nebula-recomp` in vendor/nebula); I patched build-pak,
package-content and build-boot-image to run off Windows.

## Loop

```
ps5/tools/vm.sh build                 # Lima VM "ps5"; full rebuild ~4-6 min, relink ~10 s
ps5/tools/vm.sh check FILE            # syntax-only compile of one source (runtime/src
                                      # relative, or ps5/...)
bash ps5/tools/console.sh deploy      # eboot + sce_sys (from the Mac)
bash ps5/tools/console.sh run 30      # launch, follow klog, fetch userdata/stderr.log into
                                      # klog/<time>/ ; stops at exit/crash/guard
bash ps5/tools/console.sh env K=V ... # /app0/env.txt (GALAXY_* options); no args removes it
bash ps5/tools/console.sh quit
bash ps5/tools/console.sh game-files  # content/ and the boot image (only after regenerating)
```

- The full log is `klog/<time>/stderr.log` (klog drops lines in the runtime's long bursts).
- After a console restart: klogsrv may be autoloaded; ps5vkctl is not —
  `nc -w 5 192.168.0.190 9021 < ../PS5_Doom3/build/ps5vkctl.elf`. klogsrv lives at
  `/data/pldmgr/payloads/klogsrv/klogsrv_v0.9.elf` on the console.
- Crash records: the eboot loads at 0x400000; symbolise `addr - 0x400000` with
  `llvm-symbolizer-22 --obj=build/last-title.elf` in the VM.
- `GALAXY_PS5_TRACE_FILES=1` in env.txt prints every failing file call.
- The home module (`gamedata/generated/home`, one 148,000-line function) is built `-O0`:
  -O2 takes over 10 minutes.

## What the console needed (all in the tree, keep them)

- libc++'s std::cerr was bound to the stderr FILE before the platform swapped it for klog's
  pipe: main.cpp points cout/cerr/clog (and the wide ones) at the current stderr, and tees
  stderr through a pipe thread into `/app0/userdata/stderr.log` (flushed every second).
- game.pak is read with `pread` (a 4.3 GB file mmap took the console's memory and hung the
  shell; Gary had to restart it). A memory guard exits under 1.5 GiB direct / 96 MiB
  flexible memory free.
- A title may not call exit(2) (SIGSYS): ExitProcess goes through `catchReturnFromMain`.
- **Fastmem is off on the PS5** (native_api.h): code pages are execute-only, so the MMIO
  fault handler cannot read the faulting instruction. The checked flat path (range test,
  then a direct read of the flat view) is used instead; no faults happen.
- Guest arena: `ps5_vrange_reserve` + `ps5_shm_create/map` at ~0x10_0000_0000, MEM1/MEM2
  each at their three aliases; `flat_guest_memory::guest_base()` replaces kGuestBase.
- Fibers are my own (fibers.cpp, asm switch, 16 MiB stacks from the title heap).
- GetThreadPriority must return what SetThreadPriority set (the deadline broker checks).
- NAND: NTFS alternate data streams emulated as shadow files under
  `/app0/userdata/.streams/<path>@/<name>` (files.cpp); folders report 1 link and files at
  least 1 (the console's /data reports st_nlink 0 for files); folders report
  case-insensitive (FileCaseSensitiveInfo flags 0); GetVolumeInformationW reports
  FILE_NAMED_STREAMS. Saves live in `/app0/userdata/saves`.
- `#pragma comment(lib)` → link with `--no-dependent-libraries`. SoftFloat's state is TLS:
  C++ needs `THREAD_LOCAL=thread_local`. No funopen, fnmatch, timegm, wcsdup, wcsnlen in
  the console's libc (replaced).
- The XInput mapping (xinput.cpp): Cross A, Circle B, Square X, Triangle Y, Options Start,
  touchpad button Back, raw sticks. The Wii Remote pointer is still Nebula's right-stick
  mapping; DualSense gyro/touchpad pointer is a later job (pad samples carry motion data in
  `reserved[]` of platform.c's struct).

## The renderer: Nebula's Direct3D 12 on vkd3d

Rather than rewriting Nebula's renderer (gx_backend 12.7k lines, renderer_d3d12 10.8k,
pipeline/texture caches, post-processing) for Vulkan, the console runs it as written:

- `ps5/tools/setup-vkd3d.sh` (build.sh runs it) fetches vkd3d 2.1 into `.deps/vkd3d` with
  `ps5/patches/vkd3d.patch` and builds it once for the VM (`~/ps5/build/vkd3d-host`) for the
  bison/flex sources, the version header and a host `vkd3d-compiler`. CMake builds vkd3d
  (common, shader, device, utils) as a static library with `ps5/vkd3d/config.h`.
- The patch: `VKD3D_SYSV_ABI` (COM methods use System V, as the compat layer does, not
  ms_abi), `VKD3D_HOST_WINDOWS_TYPES` (vkd3d's headers leave HWND/LUID/RECT to windows.h),
  `VKD3D_UTILS_PS5` (no D3D12CreateDevice/D3DCompile of vkd3d-utils').
- Headers: `ps5/compat/include/vkd3d_com.h` makes vkd3d's widl headers sit on the compat
  windows.h (`_INC_WINDOWS`, `WIDL_EXPLICIT_AGGREGATE_RETURNS`, interface macros);
  objbase.h's IUnknown has the COM layout (no virtual destructor!) and `__uuidof` returns
  the real IIDs from `__CRT_UUID_DECL`. d3d12.h, d3dcompiler.h, d3d12shader.h,
  d3d12sdklayers.h, d3dcommon.h wrap vkd3d's; dxgi1_6.h is my own (only what Nebula calls).
- `ps5/gx/d3d12_ps5.cpp`: one vkd3d instance (VK_KHR_display) from
  `vk_icdGetInstanceProcAddr`, devices with VK_KHR_swapchain, fences signal compat events.
  **D3DCompile** wraps vkd3d-utils' D3DCompile2: `*_5_1` sources with no register space
  compile as `*_5_0` (vkd3d's 5.1 output lacks resource arrays such as `Texture2D tex[8]`).
- `ps5/gx/dxgi_ps5.cpp`: factory/adapter/output and a swap chain whose buffers are VkImages
  given to vkd3d (`vkd3d_create_image_resource`, PRESENT = copy-source layout); Present
  waits for vkd3d's submission (`vkd3d_queue_signal_on_cpu`), blits the buffer to the
  VK_KHR_display swapchain on vkd3d's queue and presents (FIFO). Display: 3840x2160 59.94 Hz.
- `ps5/compat/src/window_queue.cpp`: real (invisible) windows and per-thread message queues
  (posted and sent messages, WM_QUIT) for Nebula's window thread; first ShowWindow sends
  activate/focus. AdjustWindowRect* succeed with no frame.
- Shaders: uber HLSL embedded as text (compiled at start, ~3 s); specialised pipelines
  compiled at run time by vkd3d-shader on Nebula's workers; presentation passes compiled at
  build time by `ps5/tools/post-shaders.sh` with the host vkd3d-compiler (Nebula uses fxc).
- Saving: the console's /data has **no hard links**, so Nebula's journalled ISFS Rename (backup
  hard link of the old target, link count 2) failed and ended the runtime while saving the
  banner. On PS5 the journal is skipped (native_host.cpp, `#if !defined(GALAXY_PS5)`): POSIX
  rename() replaces atomically; compat's MoveFileExW moves the metadata streams with the file.
  A crash there left `<file>.__galaxy_isfs_rename_journal__.<pid>.<n>` beside the save.
- DXGI's GetFrameLatencyWaitableObject hands the caller the handle (Nebula closes it): a new
  event each call. Nebula's crash reporter must not read code bytes (execute-only): skipped.
- **Launcher environment**: the console skips Nebula's launcher, so main.cpp applies what
  LaunchOptions.cs sets (not recording) and installer/runtime-env.json (generated
  launcher_env.inc). Without it movies ran at ~49 frames/s (the game's own THP decoder).
- **120 Hz (works, 2026-10-20 ~14:50; opt-in through env.txt)**: the title links its own RADV
  (ps5/tools/setup-radv.sh + ps5/patches/radv.patch, copied from the Eternal Darkness port:
  keeps 119.88 Hz when the TV runs it as VRR, idle vblank 20.9 ms = the TV's 48 Hz floor;
  PS5_Vulkan's archive gives that up). dxgi_ps5.cpp picks the fastest mode when max_fps is 0 (follow the display: Nebula clamps
  the setting to 1-60, 0 allowed)
  and reports 119 Hz to Nebula (compat::set_display_refresh_hz -> EnumDisplaySettings).
  Interpolated frames are Nebula's experimental replay: env.txt from ps5/test/120hz.env
  (`bash ps5/tools/console.sh env $(cat ps5/test/120hz.env)`). On a 60 Hz output nothing
  changes. Upstream clamps max_fps to 1-60, so interpolation could never start: render_config.cpp
  allows up to 240 only with GALAXY_EXPERIMENTAL_INTERPOLATION=1. Measured in the Comet
  Observatory: ~118 presents/s, 1 subframe a game frame (cpu ~1.7 ms, gpu ~3.5 ms each), game
  frames mostly 60 but some 2 s windows at 52-58. GALAXY_TRACE_GX_STALLS=1 profiles every GX
  command and drops the game to ~5 frames/s: never in a test env.
- Small Nebula edits (in nebula.patch): uint64 lambda return types, RECT LONG casts,
  display_modes' EDID names off on PS5.
- Internal resolution follows the window: 6x (3840x3168) by default.
- Screenshots: `PS5_DevLoop/tools/ps5loop.py load` (once a boot), `open --beside`, `shot FILE`,
  `close`. The title has no `grab` hook yet.

