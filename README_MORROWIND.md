# DXVK — Morrowind / MGE XE fork

This is a **modified** build of [DXVK](https://github.com/doitsujin/dxvk), maintained for use with
[MGE XE](https://github.com/Greatness7/MGE-XE). It is not affiliated with or endorsed by the DXVK
project; please report problems here rather than to upstream.

Modifications are confined to `src/d3d9/`, `src/util/config/config.cpp` and `dxvk.conf`. DXVK is
distributed under the zlib license, which is retained unchanged in `LICENSE`.

## Branches

| Branch | Contents |
|---|---|
| `master` | untouched mirror of `doitsujin/dxvk` |
| `mge-xe` | the fork's changes, rebased onto `master` (default branch) |

`git log master..mge-xe` is the complete diff against upstream.

## What this fork changes

Two of these apply to every Morrowind session; the rest are opt-in.

**Always active with Morrowind:**

- **`d3d9.deviceLocalStaticBuffers`** — static write-only `D3DPOOL_DEFAULT` buffers upload through
  a transient staging slice instead of holding a persistent host-visible mapping. Enabled by app
  profile for `Morrowind.exe`, where large mod lists otherwise exhaust the 32-bit address space.
  Available as a config option for other titles; defaults to `False`.
- **BC7 texture support** via the `BC7 ` FourCC. Without it, BC7 textures fail with
  `D3DERR_INVALIDCALL`.
- **Fixed-function state update fast paths** — `SetTransform` no longer dirties the fixed-function
  constant buffer when handed an unchanged matrix, and spot-light cone angles are computed at
  `SetLight` time rather than once per light per constant buffer update.

**Exposed through a private COM interface, used only if MGE XE asks for it:**

- **MSAA depth resolve** — `ResolveDepthMinV1` resolves a multisampled depth surface into an INTZ
  texture, taking the minimum sample per pixel. D3D9 provides no way to read multisampled depth, so
  without this MGE XE must disable MSAA to run any effect that samples scene depth.
- **Native per-pixel-lighting draw packets** — `DrawPplV1` accepts a fully described fixed-function
  lighting draw as one flat struct and issues it with purpose-built shaders, bypassing the D3D9
  fixed-function state machine. Version 3 packets add a per-light fade to zero before a cutoff
  distance, the falloff OpenMW uses.
- **A 32-light fixed-function limit**, up from 8, for both the ordinary path and the native packet.
- **Indexed vertex blending caps** — `MaxVertexBlendMatrixIndex` is reported as 7 rather than 0, so
  applications that check the cap can use a matrix palette.

## Interop surface

Two interfaces, both reachable by `QueryInterface` on the D3D9 device:

| IID | Interface |
|---|---|
| `2ff12bfc-4622-4d9d-bcbf-1501f37e8aa3` | `IDxvkMorrowindInterop` — version, capabilities, `ResolveDepthMinV1` |
| `275c3348-5724-4a7e-aac0-46ceda965739` | `IDxvkMorrowindPplInterop1` — capabilities, `DrawPplV1` |

Both are versioned and capability-gated, so a client detects support by asking rather than by
assuming a particular build. On stock DXVK or native D3D9 the `QueryInterface` returns
`E_NOINTERFACE` and MGE XE falls back to its own paths.

`src/d3d9/dxvk_morrowind_interop.h` and `src/d3d9/dxvk_morrowind_limits.h` define the shared ABI and
are kept byte-for-byte identical in the MGE XE tree. Both restrict themselves to platform headers
(the limits header to include guards and integer macros, since it is also included from GLSL) so
that stays possible. Packet layout is pinned by `static_assert` on both sides; a client built
against a different revision is rejected rather than misread.

MGE XE opts in through `mgeXE.toml` — `distant_land.native_ppl_packets`,
`distant_land.expanded_light_limit`, and `render.indexed_skinning`, all defaulting to off. The MSAA
depth resolve needs no key: it is used automatically when the capability is present.

## Requirements

This fork raises DXVK's instruction set floor from SSE3 to **SSE4.2** (Nehalem, 2008 / Bulldozer,
2011), matching the `x86-64-v2` baseline MGE XE's own 64-bit binaries are built against, so both
halves of the install share one hardware requirement.

## Building

Morrowind is a 32-bit process and MGE XE loads only `d3d9.dll`, so that is the sole build target:

```
meson setup --cross-file build-win32.txt --buildtype release   -Ddebug=false -Doptimization=3 -Db_ndebug=true   -Denable_dxgi=false -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false   build
ninja -C build
```

The result is `build/src/d3d9/d3d9.dll`. Drop it next to `Morrowind.exe`.

CI builds the same configuration on every push and attaches `d3d9.dll` to each tagged release, so a
prebuilt DLL can be downloaded instead of built.
