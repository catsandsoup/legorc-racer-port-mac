# Handoff

Owner: [catsandsoup](https://github.com/catsandsoup). Repo: `catsandsoup/legorc-racer-port-mac`.

Last updated: session 2 (2026-10-09). Session 2 ran alongside another
session that was getting macOS CI green, and did the Level 0 items that
don't touch CI. The repo is
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
| Physics feel | Matches the original in a timed run | Same car position at 1.5, 3 and 4.5 s of full throttle (Wine reference) |
| Original lighting | Matches | Floor, wall and ceiling within a few RGB levels of the original |
| Widescreen, 4K, any frame rate | Done (Linux, OpenGL) | 1600×900 and 1280×960 screenshots; physics interpolation on |
| Settings: macOS menu bar | Code written, never run on a Mac | — |
| Settings: in-game panel (F1, Start) | Opens, takes focus, D-pad moves, B/Start closes | Linux screenshots with placeholder art (session 2) |
| Controller menus | Level select highlight (D-pad, stick, arrows), A picks, B backs out, Start opens settings | Linux screenshots with placeholder art and injected joypad events (session 2) |
| Keep original files | Copied to `<user data>/original/` after a successful import; auto-import and **Re-import** use them | Clean `HOME`: zip import kept both files; with the zip and `data/` deleted, the next launch re-imported from `original/` |
| Real-data check (session 2) | Owner's zip: first-run import, controller menus, Start pauses a race, Esc closes the panel and the race goes on, Back leaves; port runs Scavenger Hunt and Supersonic Challenge; 1.00× = 60.0 logic updates/s and 2.250 sim s per real s | Linux screenshots from a clean `HOME` |
| Release guard | `scripts/release-guard.py`; CI workflow `release-guard.yml`; `package-macos.sh` scans the app zip; optional pre-commit hook | Caught 11 planted leaks (renamed Director files, embedded glTF/MP3 in a .pck, zipped .cct, screenshot, converter JSON, known hash, local path); clean on all history |
| Mods (assets, `tuning.json`, GDScript, GDExtension) | Asset, tuning and GDScript mods proven | `examples/mods/hello-gdscript` printed live speed in a run |
| macOS build | CI written. **Run 1 was in progress at handoff**: the converter built and passed its tests on `macos-15`, and the arm64 library was compiling | https://github.com/catsandsoup/legorc-racer-port-mac/actions |
| macOS app on a real Mac | **Never run** | — |

All "Done" rows were proven on Linux (Xvfb, llvmpipe, Godot's
Compatibility renderer). Metal, Forward+, MetalFX, the native menu, the
Gatekeeper flow and controllers on macOS are unproven.

## Known issues

1. A few textures on the car's rear fins don't match the original. The
   wing was fixed by the per-scene UV flip; the fins weren't. Suspect the
   W3D CLOD texture-coordinate decode for those meshes.
2. Scavenger Hunt and Supersonic Challenge haven't been compared against
   the original. Under Wine the projector closed once while entering
   Scavenger Hunt. The prize model's UV flip is unverified.
3. The controller is digital: triggers act as on/off at 30 %. Analog
   pedals are on the roadmap.
4. There is no separate pause menu. Esc (and the controller's Back/View
   button) leaves the race, as in the original. Start opens the settings
   panel, which pauses a race and has Main Menu. Proven with the real
   data in session 2.
5. Controller menus have no button glyphs yet. The level select's buttons
   are stacked vertically, so up/down follow the list order.
6. Prize modes run in the port but haven't been compared against the
   original under Wine (Wine isn't installed in the container; the owner
   was asked before downloading it).
7. The macOS export preset has `exclude_filter=""`. A local `game/data/`
   folder would be exported into the .pck; the guard would fail that
   build. Consider `exclude_filter="data/*"` (left alone because the CI
   session was editing the presets).
8. `docs/PRODUCT_RULES.md` wants the settings UI drawn in the game's own
   art. The panel is currently a plain Godot dark panel.

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
scripts/package-macos.sh    put the converter in the .app, ad-hoc sign, zip, run the guard
scripts/release-guard.py    no game-derived bytes in commits or the app
.githooks/pre-commit        runs the guard on staged files
.github/workflows/release-guard.yml
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
| `SSRC_LM_MODE` | Lightmap blend experiments (0 is correct; leftover from calibration) |

For a fresh first-run test, set `HOME` to an empty folder that contains
`Downloads/Supersonic_RC.zip`.

`SSRC_TEST_FLOW` also takes `joy:<button>` (a, b, x, y, start, back, up,
down, left, right) to tap a controller button.
`SSRC_TEST_TAPS=joy:start@3,key:escape@5` taps buttons or keys during a
race (seconds after it starts); `SSRC_TEST_JOY_DEVICE=1` sends taps as a
second controller. Each run ends with a game-speed line.

To test menus and import without the game files, point `SSRC_IMPORTER` at
a script that writes placeholder PNGs, `images.json`, `manifest.json` and
an empty `scenes/world.glb` into its second argument. Driving needs the
real data.

Release guard: `python3 scripts/release-guard.py --tree` (or `--staged`,
`--commits A..B`, `--app <path>`). Run
`git config core.hooksPath .githooks` once to check every commit.

## Next steps (in order)

1. Get CI green. If the first run fails, read the job log, fix and push.
2. The owner downloads the artifact and plays it on a Mac. Fix what they
   hit first.
3. Finish Level 0: compare the prize modes against Wine
   (`docs/REFERENCE.md`), and fix the car fin textures.
4. Then the controller milestone: analog trigger pedals (opt-in), glyphs,
   rumble, then DualSense adaptive triggers through GameController.
5. Then the UI milestone: one settings model, drawn in the game's art,
   with Controls, Car Setup, Events, Mods and Developer pages.
6. Multiplayer is scoped in `docs/MULTIPLAYER.md` (Apple silicon only:
   split-screen first, then LAN, then internet). Not started; wait for the owner's
   go-ahead.
