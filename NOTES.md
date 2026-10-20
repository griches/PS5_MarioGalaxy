# Super Mario Galaxy for the PlayStation 5 — working notes

Handoff notes for whoever picks this up next (a new session after compaction).
State as of 2026-10-20 ~13:00.

## Where it stands

- **Boots and runs at 60 frames/s on the PS5 with no picture.** Commit `5ce9765` (local,
  no remote). Title **PPSA99315** "Super Mario Galaxy", registered by ShadowMountPlus on the
  console at 192.168.0.190, deployed with the game data in place.
- Proven in klog/stderr: all game code runs, 8 guest threads on fibers, VI retraces at
  60 Hz, `[gx-null] presents/s=60.0` with ~61 XFB copies and PE events a second, NAND
  saves initialise (SYSCONF, setting.txt, play_rec, game data folder), XAudio2 submits audio
  to the console's output, memory flat (~370 MB heap, 11 GB direct free).
- **Next job: the Vulkan renderer** (plan below). The first goal is a picture of the title
  screen.

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

## The Vulkan renderer: plan

Nebula's GX path: `gx_backend.cpp` (12.7k lines: threading, FIFO parse, draw batching,
EFB copies, PE completion) drives `RendererD3D12` (`renderer_d3d12.cpp`, 10.8k) plus
`pipeline_cache.cpp` (3.3k, HLSL from `shader_gen.cpp`, FXC on worker threads),
`texture_cache.cpp` (2.7k), `post_process.cpp` (1k; SMAA/FXAA/FSR1), `efb_copy.cpp`,
`placed_texture_allocator.cpp`, `temporal_upscaler.cpp` (FSR3, drop), `display_modes.cpp`,
`window_owner.cpp`. Front-end files with no D3D12 (keep as they are): `fifo_parser`,
`gx_state`, `vertex_loader`, `shader_keys`, `shader_gen`, `frame_completion`,
`render_config`, `game_asset_extract`.

Approach: keep `gx_backend.cpp` and write a Vulkan renderer with the interface it uses
(the ~80 `renderer_.` calls: begin_frame/end_frame/completed_fence_value, bind_efb,
clear_efb, draw(DrawCall), copy_efb_to_xfb, copy_efb_to_texture, present, peek_efb,
vertex/index/constant/matrix rings, srv ring, sampler tables, pointer-depth and async-peek
captures; the replay/interpolation/temporal/motion calls can be stubs). Then port
TextureCache, PipelineCache and PostProcessor to Vulkan types. gx_backend.cpp's own D3D12
uses (47 lines) go behind `#if defined(GALAXY_PS5)`.

- Root signature → pipeline layout: b0 (VS constants, root CBV), b1 (PS constants, root
  CBV), t0-t7 texture table, s0-s7 sampler table, t0 space1 matrix palette (root SRV,
  StructuredBuffer<float4>). Use VK_KHR_push_descriptor (one set per draw) or dynamic
  offsets; RADV supports both.
- DrawCall (renderer_d3d12.h ~line 348) maps to pipeline + 2 UBO ranges + 8 images +
  8 samplers + storage buffer range + vertex/index buffer range + scissor + XF viewport
  (reversed depth: see the comment in RendererD3D12::draw about near==far==1 clear quads).
- EFB: RGBA8 + D24S8 at 640x528 × internal scale; XFB textures from display copies;
  present scales the selected XFB. Depth 24-bit reversed.
- **Shaders**: start with the uber shaders only. `uber_vs.hlsl` / `uber_ps.hlsl` compile
  to SPIR-V unchanged with the VM's DXC:
  `~/ps5/dxc/build-v1.9.2609/bin/dxc-3.7 -spirv -T ps_6_0 -E main -fspv-target-env=vulkan1.2 -fvk-use-dx-layout uber_ps.hlsl`
  (56 KB; VS 28 KB). Make PipelineCache always use the uber pipeline at first (no runtime
  compiler on the console); specialised pipelines later via glslang's HLSL front end or DXC
  built for the console. post_process.hlsl (SMAA/FXAA/FSR1) the same way, offline.
- Device/surface: as PS5_VulkanTemplate (`ps5/src/main.cpp`: `volkInitializeCustom(
  vk_icdGetInstanceProcAddr)`; `base/VulkanSwapChain.cpp`: VK_KHR_display, display mode,
  `vkCreateDisplayPlaneSurfaceKHR`). RADV is linked into the eboot already
  (link-title.sh). Output 3840x2160.
- Order of work: device + swapchain + clear colour on screen (proves presentation) →
  EFB targets + uber pipeline + draws → display copy to XFB + present (**title screen**:
  notify Gary audibly) → EFB→texture copies, peeks, pointer depth → texture cache formats
  (CMPR, I4/I8/IA4/IA8, RGB565, RGB5A3, RGBA8, C4/C8/C14X2 + TLUT) → post-processing →
  specialised shaders and speed.
- Nebula's own references: `docs/GRAPHICS_ARCHITECTURE.md` in vendor/nebula.
