# ZenBoost

A native performance mod for **Minecraft Bedrock 26.30.5** on Android.  
Delivered as `libZenBoost.so` — inject via KernelSU module, Magisk, or a compatible launcher.

## Features

| Feature | Description |
|---|---|
| **FPS Uncap** | Patches `eglSwapInterval(0)` to remove the 60 FPS cap |
| **VSync Toggle** | Forces VSync on/off independently |
| **Memory Cleaner** | Runs `malloc_trim` + `MADV_DONTNEED` every 30 s |
| **Thread Boost** | Raises render thread priority (`nice -10`) |
| **120 FPS Limiter** | Sleep-based cap for smooth 120 FPS target |
| **Boost Mode** | One-tap: enables FPS Uncap + Memory Cleaner + Thread Boost |
| **HUD Overlay** | Live FPS (colour-coded), frame time, RAM usage |
| **Low-End Mode** | Disables GL dithering + sets `GL_FASTEST` hints |

## GUI

- **FAB button** (top-right corner) — tap to open/close the ZenBoost panel  
- **FPS graph** — rolling 90-frame history  
- **Feature cards** — per-feature ON/OFF toggle buttons  
- **HUD** — always-on overlay (can be toggled)

## Build

GitHub Actions produces both `arm64-v8a` and `armeabi-v7a` builds.  
Download from the [Actions](../../actions) tab after pushing.

## ABI Targets

- `arm64-v8a` — modern Android (recommended)  
- `armeabi-v7a` — older 32-bit devices

## Requirements

- Rooted Android device (KernelSU or Magisk)  
- Android 8.0+ (API 26+)