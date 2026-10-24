# Skate 2 for iPhone and iPad

A static recompilation of *Skate 2* (Xbox 360, 2009) for iOS. The game's PowerPC code is
translated ahead of time to C++ and compiled for arm64, and runs on a runtime that provides the
Xbox 360 kernel, audio, input and graphics (Vulkan on Metal through MoltenVK).

**No game content is included. You need your own copy of the Xbox 360 game.**

## Requirements

- An iPhone or iPad with an A15 Bionic or newer (iPhone 13, iPad mini 6th generation and later)
  and iOS 15 or later.
- *Skate 2* for Xbox 360: a disc image (ISO/XISO) or the extracted disc.
- Free space for the game: about 6.5 GB for an extracted disc, or the size of the image for an ISO.
- A Bluetooth controller is recommended. Without one, an on-screen gamepad is shown.

### Supported disc

The port runs the code of one game executable, so it needs the disc that carries it: the release
with media ID `6ADB5821` (executable version 0.0.0.2; `default.xex` SHA-256
`116ba3ee37fcf0d8db60b7d47c971fdb6d3eb2a7a4d3cfc3611a06704a588ae5`). When an image is picked, the
app checks it before copying anything, and checks the game again at every launch; a different
release of the game is turned away with an explanation instead of crashing.

## How to play

1. **Download** `Skate2-iOS.ipa` from the
   [latest release](https://github.com/DevZer0D4Y/Skate2-iOS/releases/latest) (or build it
   yourself, see [Building](#building)).
2. **Install it** with a sideloading tool: AltStore, SideStore, Sideloadly, ESign or TrollStore.
   The IPA is unsigned; the tool signs it. With a free Apple ID the app must be re-signed every
   7 days; your game files and saves are kept when you do.
3. **Copy the game into the app.** Extract your own *Skate 2* Xbox 360 disc and copy its contents
   into the app's `game` folder, with the Files app (On My iPhone > Skate 2) or from a computer
   (Finder's Files tab). `game/` must contain `default.xex` directly; the `$SystemUpdate` folder is
   not needed.

   Instead of extracted files, you can open the app and tap **Choose ISO** to pick a disc image
   from the Files app. The app checks it and copies it in, which takes a few minutes and needs
   free space for the copy.
4. **Play.** Connect a Bluetooth controller (Xbox, PlayStation or MFi) for the best experience,
   or use the on-screen controls.

The first launch shows a black screen for up to a minute before the intro; later launches are
quicker. Your files in the app:

```
Skate 2/
├── game/          the extracted disc: default.xex, data/
├── game.iso       or the imported disc image
├── userdata/      saves (created by the app)
├── cache/         shader cache (created by the app)
└── skate2.toml    settings (created by the app)
```

Switching apps or locking the device pauses the game, which resumes where it was.

## Controls

The on-screen gamepad shows while no controller is connected. It hides as soon as a controller
connects and comes back when the controller disconnects. It follows the game's own pad layout:

| Control | Action |
|---|---|
| Left stick | Steer, walk, menu navigation |
| Right stick | Flick-It tricks (springs back to centre like a real stick) |
| LT / RT | Grabs |
| A / B / X / Y | Push, brake and the rest; A selects and B goes back in menus |
| LB / RB, Back, Start | Along the top of the screen |

## Performance

The game runs at the console's 30 FPS. Effects are prepared for the GPU the first time they are
seen and kept in `cache/`, so the first minutes in a new area can stutter more than later sessions.

## Settings

`skate2.toml` can be edited in the Files app; changes apply on the next launch.

| Setting | Default | |
|---|---|---|
| `touch_controls` | `"auto"` | On-screen gamepad: `"auto"` while no controller is connected, `"on"`, or `"off"`. |
| `touch_controls_opacity` | `1.0` | Opacity of the on-screen gamepad, 0.1 to 1.0. |
| `present_letterbox` | `false` | `true` keeps the 16:9 picture with side bars instead of filling the screen. |
| `skate2_frame_rate` | `"original"` | `"original"` is the console's frame rate; `"60"` is experimental. |
| `vulkan_wait_until_idleclean_sync` | `false` | `true` makes the CPU wait for the GPU every frame: slower, only for surfaces that flicker or turn black. |
| `user_language` | `1` | Xbox 360 language: 1 English, 3 German, 4 French, 5 Spanish, 6 Italian. |

## Building

On a Mac with Xcode, CMake 3.25+, Ninja and Python 3.

### 1. The SDK

`sdk/` holds the runtime and the code generator. Build it once for the Mac (the code generator
and a macOS test runtime) and once for iOS:

```bash
cd sdk
cmake --preset mac-arm64
cmake --build --preset mac-arm64-release --parallel 6
cmake --install out/build/mac-arm64 --config Release

cmake --preset ios-arm64
cmake --build --preset ios-arm64-release --parallel 6
cmake --install out/build/ios-arm64 --config Release
cd ..
```

### 2. The game code

Put your extracted disc in a `game` folder next to this repository (`../game/default.xex`), then
generate the C++ from it:

```bash
export REXGLUE_HOST_TOOL=$PWD/sdk/out/install/mac-arm64/bin/rexglue
export REXGLUE_IOS_SDK=$PWD/sdk/out/install/ios-arm64
export REXGLUE_MAC_SDK=$PWD/sdk/out/install/mac-arm64

"$REXGLUE_HOST_TOOL" codegen skate2_manifest.toml
```

The output goes to `generated/` and is never committed: it is derived from the game.

### 3. The app

```bash
cmake --preset ios-arm64-release
cmake --build --preset ios-arm64-release --parallel 6
bash ios/make-ipa.sh            # writes out/Skate2-iOS.ipa
```

A macOS build runs the same code against an extracted disc, which is handy for testing:

```bash
cmake --preset mac-arm64-release
cmake --build --preset mac-arm64-release --parallel 6
out/build/mac-arm64-release/skate2 --game_data_root=<extracted disc>
```

### Analysis fixes

The function, indirect-call and switch-table fixes the code generator needs for this executable
are in `config/`, from [skate2-recompile-pc](https://github.com/christianortel/skate2-recompile-pc)
(MIT, `config/LICENSE`).

## Legal

*Skate 2* © Electronic Arts Inc. Xbox 360 is a trademark of Microsoft. This project is not
affiliated with or endorsed by Electronic Arts or Microsoft and contains no game content; the app
icon uses the game's cover art. The runtime in `sdk/` is distributed under the terms in
`sdk/LICENSE`; the files in `config/` under the terms in `config/LICENSE`.
