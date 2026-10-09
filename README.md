# Supersonic RC: native for Apple silicon

By [catsandsoup](https://github.com/catsandsoup). See [AUTHORS.md](AUTHORS.md).

A native Mac port of **LEGO Supersonic RC** (2003, Shockwave/Director 8.5).
There's no Flash, no Shockwave and no emulator: the game logic was
decompiled from Lingo and rewritten in C++, and every asset is converted to
an open format and rendered by a modern engine.

- Apple silicon native (arm64), Metal renderer, Jolt physics on multiple cores
- Any window size or fullscreen, up to 4K and beyond, with optional supersampling
  or MetalFX/FSR upscaling
- Any frame rate (vsync off, 120 Hz, 240 Hz, unlimited). Physics runs at the
  original 60 updates a second and is interpolated, so the car handles exactly
  like it did in 2003 at any frame rate
- Mac menu bar for display, graphics, gameplay and audio settings
- Moddable: swap textures, models, sounds and physics constants by dropping
  files in a folder, or write C++ or GDScript mods

You need your own copy of the original game files (`game.dcr` and
`GUILibrary.cct`, or the zip they came in). They are not included and are
never uploaded anywhere.

## Play

1. Download `Supersonic-RC-macOS-AppleSilicon.zip` from
   [Releases](../../releases) and unzip it.
2. Put your `Supersonic_RC.zip` (or the folder with `game.dcr`) in
   **Downloads**, on the **Desktop**, or next to the app.
3. Double-click **Supersonic RC**. On first launch it finds your files and
   converts them in about a second. After that it goes straight to the title
   screen.

The app is signed ad hoc, not with a paid Apple ID. The first time you
open it, macOS will say it can't verify the developer. Open
**System Settings → Privacy & Security** and click **Open Anyway**, or run:

```sh
xattr -dr com.apple.quarantine "/Applications/Supersonic RC.app"
```

If the files aren't found automatically, the app asks you to pick them.
Use **Game → Re-import Game Files…** to convert them again.

### Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Accelerate | ↑ | Right trigger / A |
| Brake / reverse | ↓ | Left trigger / X |
| Steer | ← → | Left stick / D-pad |
| Handbrake (power slide) | Space | B / Right shoulder |
| Leave the race | Esc | Start |
| Sound on/off | S | |
| Settings | F1 or ⌘, | |
| Fullscreen | ⌃⌘F or F11 | |

Hold **Ctrl** while the game starts to unlock all modes, as in the original,
or use **Game → Unlock All Modes**. Unlocks are saved.

## Settings

Everything is in the menu bar (and in the in-game panel, F1):

- **Display:** window mode, window size (up to 3840 × 2160), render
  resolution 50 to 200 %, upscaler (bilinear, FSR 1, FSR 2, MetalFX spatial,
  MetalFX temporal), anti-aliasing (MSAA 2/4/8×, FXAA, TAA), V-Sync, frame
  rate limit, FPS counter
- **Graphics:** Original lighting (Director's own lighting model with the
  baked lightmaps, checked against the 2003 game) or Enhanced (real-time
  shadows, SSAO, glow); texture filtering; field of view; camera
- **Gameplay:** game speed 50 to 200 %; physics rate 60 to 480 Hz
- **Audio:** music and effects volume

## How it works

| Piece | What it is |
|---|---|
| `importer/` | `ssrc-import`, a Rust command-line tool that reads `game.dcr` with [DirPlayer](https://github.com/igorlira/dirplayer-rs)'s Director, Shockwave 3D and Havok readers and writes glTF scenes, PNG textures and lightmaps, MP3 audio and JSON collision data |
| `src/` | The game, in C++ as a Godot GDExtension: car and wheel physics, camera, five game modes, timers, scoring, stunts, menus, settings, mods |
| `game/` | The Godot 4.7 project that hosts it |

The original's Lingo scripts were decompiled and ported line by line. Every
constant (spring strength, drive force, grip, checkpoint radius…) has its 2003
value, collected in `tuning.json`-overridable `src/tuning.h`. The original
drove a Havok rigid body with forces from Lingo; here the same forces drive a
Jolt rigid body, including the way the Havok Xtra attenuated script forces
(measured against Director by the DirPlayer project). Side-by-side runs
against the original game show the car covering the same ground in the same
time.

See [docs/PORTING.md](docs/PORTING.md) for details and
[docs/MODDING.md](docs/MODDING.md) for mods.

## Build from source

Requirements: a Mac with Xcode command line tools, Rust, Python with `scons`,
and Godot 4.7.2.

```sh
git clone https://github.com/catsandsoup/legorc-racer-port-mac
cd legorc-racer-port-mac
git submodule update --init   # not --recursive: DirPlayer's own submodules aren't needed
cargo build --release --manifest-path importer/Cargo.toml
scons platform=macos arch=arm64 target=template_release
godot --headless --path game --import
godot --headless --path game --export-release macOS "$PWD/build/macos/Supersonic RC.zip"
scripts/package-macos.sh "build/macos/Supersonic RC.zip"
```

GitHub Actions builds the same thing on every push and attaches the app to
tagged releases.

## Legal

LEGO and Supersonic RC are trademarks of the LEGO Group, which does not
sponsor, authorise or endorse this project. This repository contains no game
data. It converts files you already have, on your own computer.

Copyright © 2026 catsandsoup. The port's code is GPL-3.0 (see [LICENSE](LICENSE)), because the converter
builds on DirPlayer (GPL-3.0). Godot is MIT. DejaVu fonts are under their own
free licence (`game/fonts/LICENSE-DejaVu.txt`).
