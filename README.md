# BareDroidNX

A standalone Android (NDK) compatibility layer for Switch homebrew.

> **Status: pre-alpha / Phase 0 in progress.** The NRO shell (APK list UI) is functional; the actual loader/shim layer is not yet written.

## What it does

Launch `BareDroidNX.nro` from the Switch homebrew menu → pick an APK from the list → it runs.

The game's native ARM64 code runs directly on the Tegra X1 — no CPU translation, no emulator. A thin shim layer fakes just enough of the Android runtime environment that the native `.so` doesn't notice it isn't on Android.

## Setup

1. Copy `BareDroidNX.nro` to `sdmc:/switch/`
2. Place `.apk` files in `sdmc:/BareDroidNX/apks/`
3. Launch from hbmenu — use D-pad or left stick to navigate, **A** to launch, **+** to quit

## Building

Requires [devkitPro](https://devkitpro.org/) with devkitA64 and libnx installed.

```sh
export DEVKITPRO=/opt/devkitpro
make
```

Output: `BareDroidNX.nro`

## Compatibility

Targets native-code-heavy Android games (Unity, Cocos2d, etc.) built for `arm64-v8a` with:
- GLES 2.0 / 3.0 rendering
- No Google Play Services dependency
- Local save data only

See the design doc (top of this repo) for the full architecture and roadmap.
