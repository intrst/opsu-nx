# opsu!nx

An open-source osu! client for Android, written in C++17

opsu!nx is a native rewrite of [fluddokt's libGDX port](https://github.com/fluddokt/opsu) of [opsu!](https://github.com/itdelatrisu/opsu). It does not use Java or libGDX for the game itself.

> **Status:** early work in progress. The project builds and starts, and the game logic is being ported over.

## Requirements

- Android 10 (API 29) or newer
- A device with Vulkan 1.1
- arm64-v8a or armeabi-v7a

## Tech

| Part | Used |
| --- | --- |
| Window and touch | Android NDK (`GameActivity`) |
| Graphics | Vulkan 1.1 |
| Images and fonts | stb_image, stb_truetype |
| Audio | [BASS](https://www.un4seen.com/) + BASS FX |

Game logic (hit objects, timing, curves, beatmap and replay parsing) is plain C++17 with no Android or Vulkan headers, so it can be tested on a desktop.

## Building

### NixOS / Nix

```sh
nix-shell
./gradlew assembleDebug
```

The APK is written to `app/build/outputs/apk/debug/`.

### Other systems

Install JDK 17, the Android SDK (platform 35, build-tools 35.0.0), the Android NDK, CMake and Ninja. Then:

```sh
export ANDROID_HOME=/path/to/android-sdk
export ANDROID_NDK_ROOT=/path/to/android-ndk
./gradlew assembleDebug
```

### Versions and names

- Versions look like `2026.1112.0` (`year.monthday.patch`) and are set from the build date. Bump the last number with `./gradlew assembleDebug -Ppatch=1`.
- APK files are named `opsu-nx-<version>-debug.apk` or `-release.apk`. The `-debug` part comes from Gradle and is normal.
- Debug builds install next to release builds. Debug APKs are signed with your own key, so uninstall any other build first if the install fails.

## Using the game

opsu!nx needs beatmaps to play. Put `.osz` files (and `.osk` skins) in the `Import` folder and they are unpacked on the next start. The game keeps its files in a shared `opsu` folder on your device storage:

- `Songs/` beatmaps
- `Skins/` skins
- `Replays/` replays
- `Import/` drop `.osz`, `.osk` and `.osr` files here
- `opsu.cfg` settings

On Android 11+ the app asks for "All files access" to use this folder.

## Credits and license

Based on [opsu!](https://github.com/itdelatrisu/opsu) by Jeffrey Han and the [libGDX port](https://github.com/fluddokt/opsu) by fluddokt. See [CREDITS.md](CREDITS.md).

Licensed under the **GNU GPL v3**. See [LICENSE](LICENSE).

BASS is free for non-commercial use only. Check [un4seen.com](https://www.un4seen.com/) before distributing commercially.
