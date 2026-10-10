# Changelog

Project owner: catsandsoup.

## Unreleased

### Fixed (from the owner's Mac playtests)
- **Timing now matches the original.** The movie runs at 40 frames a second
  (its config frame rate), and each frame is split into two physics updates
  of 12.5 and 37.5 ms of simulation. That's 2.0x real time; the port had
  assumed 60 updates of 37.5 ms (2.25x). Measured against the original
  under Wine (39 fps, 2.02x). Straight-line speed and braking now match it,
  and the camera and steering update as often as in 2003.
- FSR 2 and MetalFX are hidden until they're proven on a Mac (one of them
  froze the owner's game).
- Floors, walls, ramps, stairs and shelf tops are solid. The Havok shapes
  are two-sided; one-sided shapes let the car through them.
- The dinosaur no longer has the blotchy black lightmap. As in the
  original, its shader clone loses the lightmap and gets a dark red tint.
- ⌘Q quits even in full screen, where the menu bar is hidden.

### Added
- R (or Y on a controller) puts the car back on its wheels, facing the same
  way. Not in the original.
- Restart the race with ⌘R or F5, or from the Game menu.
- The help screen shows before the first race only (setting: every race,
  as in the original).
- `--race N` launch option: straight into challenge N.
- Lighting and texture filtering apply instantly, even mid-race.
- `scripts/reference/`: drives the original under Wine, reads and writes its
  car physics in memory (telemetry, spawn anywhere), and compares it with
  the port route by route.
- `scripts/test-collision.sh`: a regression test that the collision world
  is solid.
- Docs: `PLAYER_JOURNEY.md`, `DESIGN_PRINCIPLES.md`, rewritten
  `PRODUCT_RULES.md` (with the Wine baseline), and the UX and style
  critics.

## 0.1.0 (first port)

### Converter (`ssrc-import`)
- Reads the player's `game.dcr` and `GUILibrary.cct` with DirPlayer's
  Director, W3D and HKE readers.
- Writes:
  - glTF scenes: world, car, star, prize and arrow. Original node names
    are kept and a single root converts Z-up inches to Y-up metres.
  - The world's lightmap UVs, parsed from the `UV_Lightmap_UVs` Lingo
    literal, as `TEXCOORD_1`.
  - W3D textures as PNG, including the zlib alpha block that follows a
    JPEG.
  - 2D art with registration points.
  - All 32 sounds as plain MP3. They are Shockwave Audio, and the parser
    had mislabelled 6 of them as IMA ADPCM.
  - The Havok collision world, placed by model transforms, and the car
    hull, as JSON.
  - The world's lights.
- The V flip is per scene: on for the world, off for the car. Both were
  checked against the original.

### Game (C++ GDExtension on Godot 4.7.2, Jolt, Metal)
- Port of every gameplay script:
  - car, wheels and car sound
  - driving and Havok managers, chase and overhead cameras
  - all five modes: Free Driving, Checkpoint Chase, Stunt Rally,
    Scavenger Hunt, Supersonic Challenge
  - chase timer with alarm, points and goal counters
  - stunt detector and stunt texts, flip recovery and respawn
  - checkpoint stars and arrow, prizes, text messages
  - title, level select with unlocks, help, loading, fades
- Physics:
  - a fixed 60 Hz logic tick at 0.0375 s of simulation per tick, as the
    original
  - Havok Xtra force attenuation (forces /42, torques /434 and /158.76)
  - physics interpolation for any frame rate
  - optional 2, 4 or 8× substeps
- Original lighting: Director's fixed-function model in gamma space with
  the scene's six directional lights, ambient light and the lightmaps.
  Faithful quirks:
  - the shader retune only reaches non-lightmapped models
  - the arrow is unlit and drawn with a 30° camera projection
- Enhanced lighting: Godot lights, shadows, SSAO and glow.
- HUD in the original layout with DejaVu Sans italics. The 4:3 menus are
  pillarboxed and gameplay fills widescreen.
- Settings in the macOS menu bar and an in-game panel (F1 or ⌘,): window
  size up to 4K, render scale 50–200 %, FSR 1/2 and MetalFX upscaling,
  MSAA, FXAA, TAA, V-Sync, FPS limit, FPS counter, lighting, texture
  filtering, FOV, camera, game speed, physics rate, volumes.
- Controls: arrows, Space handbrake, Esc, and S for sound (original
  keys). Gamepad: triggers, stick and buttons, currently digital
  thresholds.
- First run finds `Supersonic_RC.zip` or `game.dcr` in Downloads, Desktop,
  Documents or next to the app and converts it automatically. There is a
  native file picker otherwise, and Re-import in the Game menu.
- Mods: per-file asset overrides, `tuning.json`, GDScript (`mod.gd`) and
  GDExtension mods, with examples.
- Unlocks are saved. Holding Ctrl at launch unlocks all modes, as in the
  original.

### Build and release
- GitHub Actions on `macos-15`:
  - builds the converter and the arm64 library
  - exports and packages the `.app` with the converter inside
  - exports with Godot's universal macOS template (the only one it ships)
    and keeps just the Apple silicon slice
  - ad-hoc signs it and runs a headless launch smoke test
  - uploads the artifact, and attaches it to tagged releases
- Original app icon. GPL-3.0 licence.
