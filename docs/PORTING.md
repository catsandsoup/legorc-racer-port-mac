# How the port was made

## 1. What the original is

`game.dcr` is an Afterburner-compressed Director 8.5 movie. It contains:

- about 3,000 lines of Lingo across 40 scripts
- the playfield as a Shockwave 3D (W3D) scene with 110 models, plus three
  props
- the car (`holypolycrap`)
- a Havok physics scene with 84 static collision bodies
- 9 baked lightmaps (one Lingo script holds their UVs as a literal list)
- 2D menu art
- 32 Shockwave Audio sounds, all of which are MP3 inside

`GUILibrary.cct` is a shared button and slider library.
`SupersonicRC.exe` is just the Shockwave 10 projector.

## 2. Conversion (`importer/`)

`ssrc-import` loads the movie with DirPlayer's Director parser and writes
open formats. Scene graphs are kept as authored (Director's Z-up inches)
under one root node that converts to Y-up metres, so node names such as
`Checkpoint_3` and `prizes_42` still work as data. The lightmap UV literal
is parsed back from the decompiled `getUVs_UV_Lightmap` handler and
attached to each mesh as `TEXCOORD_1`.

## 3. Game logic (`src/`)

Each Lingo script maps to C++:

| Lingo | C++ |
|---|---|
| `car`, `wheel`, `carsound` | `Car`, `Wheel` (`car.cpp`) |
| `drivingManager`, `havokManager` | `DrivingSession` (fixed 60 Hz update, Jolt rigid body) |
| `camera`, `camera_fromadistance` | `ChaseCamera` |
| `gameTypeManager`, `checkpoint_gametype`, five modes | `GameMode`, `CheckpointMode`, `FreeDriving`, ... |
| `chasetimer`, `pointsgoalcounter`, `pointscounter` | `GameMode::timer_*`, `add_points` |
| `stuntdetector`, `pickupManager`, `getStuntText` | `GameMode::stunt_update`, `pickup_update`, `stunt_text` |
| `insceneoverlay`, `3d_textsprite`, `textmessagemanager` | `Hud` |
| `lightMapManager` | `apply_director_materials` |
| `mainManager`, `titleManager`, `selectLevelManager`, `helpManager`, `fadein`, `fadeout` | `SupersonicGame` |

The code works in Director space, so constants are copied unchanged.

### Physics

The original stepped Havok with `step(dt * 2.5, 7)` once per update of
at most 20 ms, clamped to 15 ms. That's one 0.0375 s simulation step per
1/60 s, i.e. 2.25 simulated seconds per real second. Forces applied from
Lingo only survive Havok's first integration micro-step, so a force F
changes velocity by F·dt / (6·7·m). DirPlayer measured this against
Director; we apply forces /42 and torques /434 (pitch, roll) and /158.76
(yaw). With those values the suspension settles at the same height and
the car accelerates and turns like the original.

### Rendering

"Original" lighting is Director's fixed-function model computed per pixel
in gamma space: texture × clamp(emissive + ambient + Σ diffuse·light·N·L)
× lightmap. Some details only showed up when comparing with the real game
running under Wine:

- `lightMapManager` clones each lightmapped model's shaders before it
  retunes `column`, `Ex_Main_02` and `D_Clone`, so the retune never
  reaches lightmapped models
- the playfield's UVs need a V flip, the car's don't
- the checkpoint arrow is drawn by its own camera (Director's default
  30° field of view) with `rootNode` set to the arrow, so it is unlit

## 4. Verification

Side-by-side screenshots against `SupersonicRC.exe` running under Wine
match the sampled colours to within a few RGB levels. A timed
full-throttle run puts the car at the same spot at 1.5, 3 and 4.5 seconds.
