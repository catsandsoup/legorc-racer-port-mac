# Supersonic RC: native for Apple silicon

By [catsandsoup](https://github.com/catsandsoup). See [AUTHORS.md](AUTHORS.md).

A native Mac port of **LEGO Supersonic RC** (2003, Shockwave/Director 8.5),
rebuilt to be a fun, modern, plug-and-play game. There's no Flash, no
Shockwave and no emulator. The game logic was decompiled from Lingo and
rewritten in C++, and every asset is converted to an open format and
rendered by a modern engine (Godot 4.7, Metal, Jolt physics).

You need your own copy of the original game files (`game.dcr` and
`GUILibrary.cct`, or the zip they came in). They are not included and are
never uploaded anywhere.

## What's new compared with the 2003 game

- **Play with a controller, everywhere.** Xbox and PlayStation pads work on
  every screen: title, challenge select, races, pause and settings. Button
  prompts switch automatically to the device you're holding (keyboard,
  Xbox or PlayStation).
- **New menus in the game's own style.** The original only had mouse menus
  and no pause, and settings lived in the Mac menu bar, which a controller
  player can't reach. So the port adds:
  - a title bar with **Start, Free Roam, Settings**
  - a **pause menu** on Esc or Start: Resume, Restart, Settings, Leave,
    Quit
  - a **Settings** page drawn in the original's brushed-metal look, where
    every change applies instantly

  The original screens (title art, challenge select, help, HUD) are kept.
  The macOS menu bar still works, as a shortcut.
- **Free Roam.** Drive anywhere in the toy shop with no timer and no goals.
  Quit from the pause menu whenever you like.
- **Quality of life:**
  - **R** (or **Y** / **△**) puts the car back on its wheels
  - **⌘R** restarts a race
  - the help screen shows once per session
  - unlocks are saved
- **Any resolution and any frame rate,** up to 4K and beyond, 60, 120 or
  240 Hz. The physics keeps the original's 40-frame rhythm (two physics
  updates per frame), independent of your display, and is smoothed on
  screen. So the car handles the same on every Mac.
- **Checked against the real thing.** The original game runs under Wine
  as a reference. Tools in `scripts/reference/` read its car physics
  straight from memory and compare it with the port, route by route and
  view by view.

## Modding (for people and their AI agents)

Supersonic RC is built to be modded. Everything lives in plain files:

| You want to | Do this |
|---|---|
| Swap textures (4K packs welcome) | Drop PNGs into a mod folder with the same path, e.g. `scenes/world_textures/ToyBox003.png` |
| Swap the car or props | Replace `scenes/car.glb` (glTF 2.0; keep `Car` and `Wheel1`..`Wheel4`) |
| Change physics, speed, grip, timers | A `tuning.json` with any constant from [`src/tuning.h`](src/tuning.h), e.g. `{ "drive_force": 9500 }` |
| Change sounds or music | `audio/<name>.ogg` / `.wav` / `.mp3` |
| Script new behaviour | `mod.gd` (GDScript), with access to the race, the car and the HUD |
| Write native code | A GDExtension (C++, Rust…) in the mod folder |

Mods live in `~/Library/Application Support/Supersonic RC/mods/<name>/`
(**Game → Open Mods Folder**), load in alphabetical order, and later mods
win. Full guide: [docs/MODDING.md](docs/MODDING.md). Examples:
`examples/mods/`.

**For AI agents working on this repo:** start with [CLAUDE.md](CLAUDE.md).
It points to the rules, the handoff, the session log and the testing
tools (spawn the car anywhere, telemetry, matched screenshots against the
original).

## Help wanted: other platforms and more

This port targets Apple silicon Macs first, but nothing in it is
Mac-only. The engine (Godot) exports to Windows, Linux, iOS and Android,
the converter is plain Rust, and there's already a Linux export preset.
**Ports are very welcome**: Windows, Linux, Steam Deck, mobile, anything.
Build the GDExtension for your platform, add an export preset and a CI
job, and open a pull request.

Planned next, and open to help:

- **local co-op**: 2- or 4-player split screen ("lounge") with one car per
  controller
- maybe online play later
- analogue trigger pedals and rumble
- AI racers and new modes

See [docs/ROADMAP.md](docs/ROADMAP.md).

I look forward to seeing how other people enhance this work. — catsandsoup

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
| Put the car back on its wheels | R | Y / △ |
| Restart the race | ⌘R or F5 | |
| Pause (Resume, Restart, Settings, Leave) | Esc | Start / Options |
| Sound on/off | S | |
| Settings | ⌘, (or from the title or pause menu) | from the title or pause menu |
| Developer settings | F1 | |
| Fullscreen | ⌃⌘F or F11 | |

Hold **Ctrl** while the game starts to unlock all modes, as in the original,
or use **Game → Unlock All Modes**. Unlocks are saved.

## Settings

Open **Settings** from the title screen, the pause menu or ⌘,. It works
with a controller, and every change applies at once:

- window or full screen
- frame rate limit, V-Sync, and a frame-rate counter
- lighting: Original (the 2003 lightmaps) or Enhanced (real-time
  shadows, SSAO, glow)
- smooth edges, textures (smooth or retro) and field of view
- help screen: before the first race only, or every race
- music and sound effects volume

Technical options (render resolution, upscaler, physics precision, game
speed, camera) are on the developer page (F1) and in the Mac menu bar.

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
scons platform=macos arch=arm64 target=template_debug     # used by the Godot editor
scons platform=macos arch=arm64 target=template_release   # shipped in the app
godot --headless --path game --import
```

To play straight from source (fastest while developing), unzip the game, run the
converter once on its `game.dcr` and point the game at the output:

```sh
importer/target/release/ssrc-import /path/to/unzipped/game.dcr ~/ssrc-data
SSRC_DATA=~/ssrc-data godot --path game
```

To make the double-click app, install Godot's 4.7.2 export templates
(Editor → Manage Export Templates) and run:

```sh
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
