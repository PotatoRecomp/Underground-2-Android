# Need for Speed: Underground 2 for Android
**English** | [Bahasa Indonesia](README.id.md) | [Español](README.es.md) | [Português](README.pt.md)

An unofficial native Android port of the original Xbox version of *Need for Speed: Underground 2*, with touch controls, controller and keyboard support and extended graphics options.

The game's x86 code is statically recompiled ahead of time to C++ with [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp), then built for arm64. Drawing goes through Vulkan, and windowing, input and audio through SDL3. No emulator is involved. The Android host (Vulkan renderer, DirectSound, touch controls, settings panel) comes from [MostWanted-Android](https://github.com/RanggaCasper/MostWanted-Android), which carries it from [Black-Android](https://github.com/RanggaCasper/Black-Android).

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build.

> [!WARNING]
> Early work in progress. See [Status](#status).

## Installation
1. Install the APK.
2. Open **Underground 2** and allow **all files access** when asked (on Android 10, the **storage** permission). The game can't start without it.
3. Copy the game to `/storage/emulated/0/Underground2`, or to an `Underground2` folder at the top of an SD card: either the disc image (`.iso`), or `default.xbe` with the disc's `NFSUNDER` folder (and `B3`) beside it. The app creates the folder on the phone's storage once access is allowed, and starts the game as soon as it finds the files.

Saves are kept in `/storage/emulated/0/Underground2/hdd` and logs in `/storage/emulated/0/Underground2/logs`. Settings (`underground2.toml`, `android.toml`, `layout.toml`) and the shader cache are in `Android/data/com.racas.underground2/files`.

The supported disc is the USA release (title `4541005A`, `default.xbe` SHA-1 `428b5a70b9400f19cc5be7679db6c72a6f80a1c9`, 4,038,656 bytes, XDK 5849).

## Features
The settings panel opens from the gear icon at the top left of the screen (or F10 on a keyboard). The Android back button is the controller's Start button.

- **Graphics**: render resolution (20% to 200% of the screen's height), aspect ratio, V-Sync, max frame rate, FPS counter, shader preloading, anisotropic filtering, anti-aliasing (FXAA, SSAA 2x), bloom, fog and screen filters.
- **Touch controls**: a full Xbox controller on screen, with layouts, opacity, size, tilt steering and a position editor.
- **Controller**: any controller SDL3 supports, with rumble and rebinding. **Keyboard**, fully rebindable.
- **General**: audio output (auto, mono, stereo, 5.1), background pause, language of the settings panel.
- **My Music**: your own songs from `/storage/emulated/0/Underground2/music` (MP3, M4A/AAC, OGG, Opus, FLAC, WAV).

## Status
The recompiled image and the host are complete and build; the game-specific work is still to do.

- `xbe_symbols` finds 173 of 238 XDK functions the host knows and all 12 data symbols. The rest are not linked into this image. Eleven more are resolved by hand in `config/game.toml` (`D3DDevice_Reset`, `SetPalette`, `BeginPush`/`EndPush`, `LoadVertexShaderProgram`, `IsBusy`, the visibility test, `IDirectSoundBuffer_Pause`/`SetEG`), each checked against the code, plus `D3D_FvfShader`.
- The recompiler covers 20,797 functions (1,047,117 instructions, 497 jump tables).
- Most Wanted's game patches (settings inside the game's OPTIONS menu, render quality switches, cheats, add-on cars, texture packs, replay, cinematic camera, touchable menus, HUD editor, RESET, the game's font in the panel, file system locks for multi-core phones) used that game's addresses and have been removed. Each has to be found again in Underground 2's code before it can come back.
- Not tested on a device yet.

Logs are in `/storage/emulated/0/Underground2/logs`. Include `underground2.log`, and the crash file if there is one, when reporting a problem.

## System Requirements
- **Android** 10 or later, arm64.
- **GPU** with a Vulkan 1.1 driver, such as Adreno 6xx or newer, Mali-G7x or newer, or Xclipse.
- **Memory**: 4 GB.
- **Storage**: 4 GB for the game files and saves.

## Building
Building requires a Windows PC.

1. Clone this repository.
2. Put `default.xbe` in `assets/`, or the disc image (`.iso`) in `assets/` or `iso/`. The build extracts `default.xbe` from it.
3. Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) with *Desktop development with C++*, [Android Studio](https://developer.android.com/studio) with Android SDK Platform 34, NDK `28.2.13676358` and CMake `3.31.6`, Java 17 to 21 (`JAVA_HOME`), and [Python](https://www.python.org/).
4. Run `build.bat`. It builds the recompiler, recompiles the xbe to `generated/game`, then builds `app/build/outputs/apk/release/app-release.apk` with Gradle. The first build compiles the whole game for arm64 and takes 30 to 90 minutes.

Install it with:
```
adb install -r app/build/outputs/apk/release/app-release.apk
```

## Credits
- [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) by TGP482: the static recompiler and portable runtime (Xbox kernel, XDK libraries and Direct3D 8) this port is built on.
- [MostWanted-Android](https://github.com/RanggaCasper/MostWanted-Android), [Black-Android](https://github.com/RanggaCasper/Black-Android) and [ShaolinMonks-Android](https://github.com/RanggaCasper/ShaolinMonks-Android) by RanggaCasper: the Android host this port is built on.
- [bagasromadon](https://github.com/bagasromadon): button design.
- [SDL](https://github.com/libsdl-org/SDL), [glslang](https://github.com/KhronosGroup/glslang), [toml++](https://github.com/marzer/tomlplusplus) and [Dear ImGui](https://github.com/ocornut/imgui).
- [xemu](https://github.com/xemu-project/xemu) and [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded), which XboxXBE-Recomp used as references for Xbox hardware and API behavior.

## License
This project is licensed under the [GNU General Public License v3.0](LICENSE). The SDK in `sdk/` keeps its own MIT license (`sdk/LICENSE`), and the libraries above their own.

*Need for Speed* is a trademark of Electronic Arts Inc. This project is not affiliated with or endorsed by them.
