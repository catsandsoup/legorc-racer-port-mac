# Handoff

Owner: [catsandsoup](https://github.com/catsandsoup). Repo: `catsandsoup/legorc-racer-port-mac`.

Last updated: session 1, after the owner's first Mac playtest (2026-10-10). The repo is
`catsandsoup/legorc-racer-port-mac`.

## What this is

A native Apple silicon port of **LEGO Supersonic RC** (2003, Shockwave /
Director 8.5 with Shockwave 3D and the Havok Xtra). No Shockwave runtime
ships. The player's own `game.dcr` is converted once into open formats, and
a C++ port of the game logic plays it on Godot 4.7.2 (Metal, Jolt). See
`README.md` for players, `docs/PORTING.md` for how, `docs/ROADMAP.md`
for what's next, and `docs/NEXT_SESSION_PROMPT.md` for the brief to start a new session.

## State at handoff

| Area | State | Proof |
|---|---|---|
| Converter `ssrc-import` | Done | Converts the full game in under 1 s; no lightmap UV mismatches; unit test for the Lingo literal parser |
| Boot: title, level select, help, loading, driving | Done | Screenshots from the exported Linux build, run from a clean user folder |
| First launch, auto-import from Downloads | Done (Linux) | Clean `HOME` with `Supersonic_RC.zip` in Downloads reaches the title with no input |
| Gameplay logic, 5 modes | Ported; Free Driving checked against the original | Checkpoint and stunt messages, timer, points seen on screen |
| Physics feel | Original 40 fps cadence; straight line and braking match; steering fitted (turn 128° vs 134°, roll 6.5° vs 5.6°) | Telemetry from the original's memory (`compare_route.py`) |
| Original lighting | Matches | Floor, wall and ceiling within a few RGB levels of the original |
| Widescreen, 4K, any frame rate | Done (Linux, OpenGL) | 1600×900 and 1280×960 screenshots; physics interpolation on |
| Player menus (title bar, pause, Settings) | **Done (Linux)**: controller-first, metal style, instant apply; the menu bar is now only a convenience | Screenshots of the full journey, title to pause to Settings to resume |
| Developer page (F1) | Plain panel with every option | — |
| Free Roam | **Done (Linux)**: no timer, goals or arrow; pause and leave | Screenshots |
| Pause | **Done**: freezes the race and its clock | Paused screenshots, car stationary |
| Textures | **Fixed** half-tile UV offset; shelves and dinosaur match the original | `compare_views.py`: scores 6.6–10.8, down from 9–20 |
| Mods (assets, `tuning.json`, GDScript, GDExtension) | Asset, tuning and GDScript mods proven | `examples/mods/hello-gdscript` printed live speed in a run |
| macOS build | **Green** on `macos-15` (run 6, commit 5e69484). Builds the converter and arm64 library, exports with Godot's universal template, thins it to arm64, ad-hoc signs, and a headless launch inside the packaged app loads the game library and runs the bundled converter. Download: the `Supersonic-RC-macOS-AppleSilicon` artifact on the latest run | https://github.com/catsandsoup/legorc-racer-port-mac/actions |
| macOS app on a real Mac | **Runs** (owner's first playtest, 2026-10-10): title, challenge select and races work on Metal. Bugs below | Owner's screenshots |

All "Done" rows were proven on Linux (Xvfb, llvmpipe, Godot's
Compatibility renderer). Metal, Forward+, MetalFX, the native menu, the
Gatekeeper flow and controllers on macOS are unproven.

## Session 1 end state (read first)

Latest commit on `main` is where session 1 stopped. Also read
`docs/SESSION_LOG.md`: the owner's decisions, latest requests and a task
table marked done / in progress / to do.

**The owner's direction:** a fun, plug-and-play game, not a strict 1:1
copy of Director. Recreate behaviour where it makes the game good.

Done and seen on screen (Linux):

- **Free Roam:** `ModeId::FREE_ROAM`, from the title bar.
- **Menus (`src/menu_ui.*`):**
  - title bar: Start / Free Roam / Settings
  - controller focus on the challenge select
  - Esc / Start opens pause: Resume, Restart, Settings, Leave, Quit
  - Settings in the metal style, applies instantly
  - F1 is the developer page
- **Pause** freezes the race and its timer.
- **R / Y** resets the car upright; **⌘R / F5** restarts the race.
- **Texture coordinates:** the +0.5 shift in `importer/src/scene.rs`.

Not yet tried on the owner's Mac: everything above. The CI build of the
latest commit is the next download.

**Next, in order:**

1. **Car textures.** The car scene is converted without the V flip
   (`importer/src/movie.rs`, `flip_v = file != "car"`). With the +0.5
   shift now in, try the flip on for the car as well. Check with
   `compare_views.py`: the original's car rear is dark with red lights at
   the sides.
2. **The arrow shakes at speed, and the chassis wobbles against the
   wheels.** The wheel visuals and the arrow update on logic ticks
   without interpolation; smooth them like the car body. The owner's fps
   fell from 92 to 48 after the 40 fps cadence change (320 physics
   ticks/s): profile it, and consider Physics Precision Standard
   (160 ticks/s) as the default.
3. **Handling the owner still feels:**
   - turning is slow at high speed
   - the car bounces on angled surfaces (skate ramp, curved walls)
   - wheels visually clip into surfaces
   - the car is flung on hitting the dinosaur's tail or other solid
     objects (the owner saw this before the collision fix; re-check)

   Make it fun first, using the routes harness to measure.
4. **README rewrite (owner's request):**
   - showcase the new menus and enhancements, why they exist, and why the
     old menus were replaced (controller players couldn't use them)
   - make modding easy to find for people and their agents
     (`docs/MODDING.md`)
   - invite ports to Windows, Linux, mobile and others ("I look forward
     to seeing how other people enhance this work")
   - mention the co-op goal
5. **Controller-aware UI everywhere.** Prompts already switch between
   keyboard, Xbox and PlayStation. Add glyphs to the help screen and a
   controls page.
6. Enhanced lighting shows odd shadows. Upscaler switching froze the Mac,
   so it's hidden in the player menu.
7. **Layer 3 test robot** (whole-level invariant bot), then the UX and
   style critic passes on the new UI.
8. **Future:** 2–4 player local co-op (`docs/ROADMAP.md`).

Tools: `scripts/reference/compare_route.py` (physics telemetry against the
original), `compare_views.py` (matched screenshots), `orig_memory.py`
(read and write the original's car state), `scripts/test-collision.sh`.
Converter debug switches: `SSRC_IMPORT_DEBUG=1`, `SSRC_IMPORT_NOFLIP=1`,
`SSRC_IMPORT_NO_UV_SHIFT=1`.

## Known issues

From the owner's first Mac playtest (2026-10-10), highest impact first:

1. **Fixed (2026-10-10): floors, walls, ramps, stairs and shelf tops weren't
   solid.** The HKE triangles are wound the opposite way to Godot's front
   faces, and some meshes mix both windings. With one-sided shapes, the car
   body passed through anything it met from the visible side. Only the
   wheel rays (two-sided) held it up. The Havok shapes are now two-sided.
   - `scripts/test-collision.sh` (`SSRC_COLLISION_PROBE=1`) sweeps a
     car-sized box at eight surfaces. Before the fix 6 of 8 failed; after
     it, all pass.
   - Still to do: compare the car's behaviour on each surface with the
     original under Wine. Driving off the top of the stairs at full speed
     now flies over them and lands on the lower floor, sometimes on its
     roof. Whether the original does the same is unchecked.
2. **After falling, the car reappeared stuck in the shelves.** The respawn
   copies the original (`pickupManager`: cast a ray down from z = 800 and
   place the car 30 units above the first hit). It should be rare now that
   the floors are solid. Check against Wine.
3. **The dinosaur's lower half was black.** Cause: `lightMapManager` clones
   each lightmapped model's shaders as `<name>_Clone`. It then retunes
   `D_Clone`, the dinosaur's clone, to diffuse `#441111` with ambient 0, and
   removes its lightmap. The port wrongly assumed the retune only reached
   models without a lightmap. Fixed in `src/materials.cpp`: the blotchy
   lightmap is gone. Its underside is still dark from the directional
   lights, so compare it against Wine.
4. **Controls don't work for a controller player.** Settings live only in
   the macOS menu bar and an F1 panel. The menus aren't controller-navigable,
   and there is no pause.
   - In full screen the menu bar is hidden, so the owner couldn't quit or
     change settings. ⌘Q now quits from anywhere, and ⌘⌃F or F11 toggles
     full screen.
   - The real fix is the pause menu and Settings milestone.
5. **Handling: the car rolls and over-rotates in hard turns** (owner:
   "boaty, it bounces and wobbles and the wheels clip the floor").
   `tests/routes/hard_left.json` shows it against the original. The two
   match until 2.5 s; by 4.5 s the port's car is rolled over and sliding
   while the original stays flat. Coming down the slope beside the right
   stairs, the port also pitches and its wheel loads go negative. Suspects:
   - the inertia tensor (a plain box from the hull)
   - the torque dividers
   - the per-substep force re-application
   - wheel lateral grip
6. The owner remembers the checkpoint arrow at the top of the screen. The
   Wine capture shows it at the bottom centre, as the port draws it.
   Re-check under Wine before changing anything.
7. A few textures on the car's rear fins don't match the original.
8. Scavenger Hunt and Supersonic Challenge haven't been compared against
   the original. Under Wine the projector closed once while entering
   Scavenger Hunt.
9. The controller is digital: triggers act as on/off at 30 %.
10. There's no release guard script yet, only `.gitignore`.
11. The original files aren't kept after import (only the converted data).

## Layout

```
importer/              Rust converter (uses third_party/dirplayer-rs)
src/                   C++ game (GDExtension)
  game.*               app flow, menus, fades, settings, native menu, import, mods
  driving.*            world, collision, camera, HUD, game modes
  car.*                car, wheels, car sound (+ Havok attenuation)
  materials.*          Director lighting shader / Enhanced materials
  sound.*              Director sound channels
  assets.*             data folder + mod overrides
  tuning.*             every gameplay constant (2003 defaults)
game/                  Godot project (project.godot, export presets, fonts, icon)
game/tools/test_driver.gd   scripted test runs (SSRC_TEST*)
scripts/package-macos.sh    put the converter in the .app, ad-hoc sign, zip
.github/workflows/build-macos.yml
docs/                  PRODUCT_RULES, ROADMAP, PORTING, MODDING, REFERENCE
```

## Build and run (Linux dev container)

```sh
git submodule update --init
cargo build --release --manifest-path importer/Cargo.toml
pip install scons
scons platform=linux target=template_debug -j4
# Godot 4.7.2 editor binary (not in repo):
godot --headless --path game --import
importer/target/release/ssrc-import /path/to/game.dcr /tmp/conv
SSRC_DATA=/tmp/conv godot --path game --rendering-driver opengl3
```

Headless screenshots go through
`xvfb-run -a -s "-screen 0 1280x960x24" godot --path game --rendering-driver opengl3`
with the test variables below.

## Comparing against the original automatically

The original's car state can be read from, and written into, its memory
under Wine (`scripts/reference/orig_memory.py`). The layout is documented
in the file. So the original gives physics telemetry, and its car can be
spawned anywhere at any heading.

`scripts/reference/compare_route.py` runs a short route
(`tests/routes/*.json`):

- from the same spawn in both games
- the original several times, to measure its own run-to-run spread

It writes these to `local/reference/runs/` (git-ignored):

- `report.md`: top speed, max roll and pitch, height, heading change, and
  the first moment the port leaves the original's spread
- `plot.png`: path, height, speed, roll, pitch and heading for every run
- side-by-side screenshot sheets

```sh
WINEPREFIX=/path/to/wineprefix scripts/reference/compare_route.py tests/routes/hard_left.json \
  --orig-dir /path/to/original --godot /path/to/godot --data /path/to/converted
```

The port side uses `SSRC_TEST_SPAWN` and `SSRC_TEST_TELEMETRY`.

`scripts/test-collision.sh` checks that the collision world is solid.

Add a route for every bug before fixing it, and re-run all routes after
every physics or rendering change.

### What the comparisons found (session 1)

1. **The original runs at 40 fps, not 60.** The movie's config frame rate
   is 40 and the score has no tempo changes. `drivingManager.update` splits
   each 25 ms frame into a 5 ms chunk and a 20 ms chunk. `havokManager`
   clamps each to 3–15 ms and steps Havok 2.5× that. The result is
   12.5 + 37.5 ms of simulation per frame (2.0×), with the wheel forces,
   steering and camera updated per chunk (80 times a second).
   - Measured under Wine: 39 fps, 2.02×.
   - The port had assumed 60 updates of 37.5 ms (2.25×).
   - It now uses the original cadence (`frame_timing()` in
     `src/tuning.cpp`), and straight-line speed and braking match.
2. **Torque scaling.** The DirPlayer-derived dividers (434 for roll and
   pitch, 158.76 for yaw, applied on world axes) didn't match the real
   Havok. They now apply on the car's own axes, fitted against the
   original on `step_steer` and `slalom`: pitch 500, roll 1400, yaw 1000.

   | Step steer | Original | Port now |
   |---|---|---|
   | Turn | 134° | 128° |
   | Roll | 5.6° | 6.5° |
   | Pitch | 0.8° | 0.9° |

   Slalom roll matches (16.1° vs 16.2°).
3. **The original's mass properties** (mass 3, box inertia, restitution
   0.01, friction 10) sit in memory next to the state, and equal the
   port's.
4. **Still different:**
   - the original loses speed in hard turns and the port doesn't
     (`step_steer` top speed 305 vs 344)
   - the paths drift apart slightly before steering starts
   - the drop test bounces more in the port (less damping)

   These are the next physics items. The dividers are a fit, not a
   derivation, so re-check them whenever the force code changes.

## Developer and test variables (never for players)

| Variable | Effect |
|---|---|
| `SSRC_DATA=<dir>` | Use this converted-data folder instead of the user folder |
| `SSRC_IMPORTER=<path>` | Converter binary to run on import |
| `SSRC_TEST=1` | Enable `game/tools/test_driver.gd` |
| `SSRC_TEST_FLOW=a,b,…` | Menu steps every 2.5 s: `click`, `click:XxY` (stage coordinates), `shot:name`, `mode:N` |
| `SSRC_TEST_INPUT=action:t0-t1,…` | Hold actions between seconds after the race starts |
| `SSRC_TEST_SHOTS=t,…` | Screenshot times after the race starts |
| `SSRC_TEST_OUT` | Screenshot path prefix |
| `SSRC_TEST_QUIT` | Quit after N seconds of driving |
| `SSRC_DEBUG_CAR=N` | Print the car's physics state for N ticks |
| `SSRC_PICK="x,y;…"` | Print the model and shader under stage points at tick 200 |
| `SSRC_COLLISION_PROBE=1` | Sweep a car-sized box at eight known surfaces, print PASS/FAIL, quit (used by `scripts/test-collision.sh`) |
| `SSRC_TEST_SPAWN=x,y,z[,heading]` | Spawn the car there (Director units; heading 0 faces +Y) |
| `SSRC_TEST_TELEMETRY=<csv>` | Write the car's state every frame |
| `SSRC_LM_MODE` | Lightmap blend experiments (0 is correct; leftover from calibration) |

For a fresh first-run test, set `HOME` to an empty folder that contains
`Downloads/Supersonic_RC.zip`.

## Next steps (in order)

See "Session 1 end state" above; it is the current list.
