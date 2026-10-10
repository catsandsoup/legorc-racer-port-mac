# Changelog

Project owner: catsandsoup.

## Unreleased: session 2

- Release guard (`scripts/release-guard.py`): fails a commit, a push (CI
  workflow `release-guard.yml`) or a built app (`package-macos.sh`) that
  contains original or converted game data, screenshots, known original
  hashes or local paths. Optional pre-commit hook in `.githooks/`.
- The player's `game.dcr` and `GUILibrary.cct` are kept in
  `<user data>/original/` after import. Auto-import looks there first.
  New: **Game → Re-import Saved Game Files**, **Game → Show Saved Game
  Files**, and **Re-import** in the F1 panel.
- Controller menus: the level select has a highlight that the D-pad, left
  stick or arrow keys move over the unlocked modes. A or Enter picks the
  highlighted mode, and B goes back from the level select and help.
- Start opens the settings panel (pausing a race) instead of leaving the
  race. Back/View and Esc still leave the race. A, B and the D-pad work
  in the panel: Godot's `ui_accept`/`ui_cancel` had no joypad buttons.
- Closing the panel with Esc during a race no longer also leaves the race.
- Any connected controller now drives. Joypad bindings were made on
  device 0 only, so a second controller (or one that reconnected with a
  new ID) was ignored. Godot 4.7.2 only moves code-made keyboard and mouse
  bindings to their own device IDs.
- Test driver: `joy:<button>` flow steps, `SSRC_TEST_TAPS` for taps during
  a race, `SSRC_TEST_JOY_DEVICE`, and a game-speed check at the end of a run.

## Unreleased: 0.1.0 (first port)

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
  - ad-hoc signs it and runs a headless launch smoke test
  - uploads the artifact, and attaches it to tagged releases
- Original app icon. GPL-3.0 licence.
