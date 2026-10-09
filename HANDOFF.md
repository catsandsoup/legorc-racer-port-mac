# Handoff

Owner: [catsandsoup](https://github.com/catsandsoup). Repo: `catsandsoup/legorc-racer-port-mac`.

Last updated: end of session 1 (2026-10-09). The repo is
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
| Settings: in-game panel (F1) | Code written, not screenshotted | — |
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
4. There is no pause menu. Esc leaves the race, as in the original. The
   settings panel pauses the game when open during a race.
5. The menus aren't fully navigable by controller (level select accepts
   A/Enter for the highest unlocked mode).
6. There's no release guard script yet, only `.gitignore`.
7. The original files aren't kept after import (only the converted data).
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

## Next steps (in order)

1. Get CI green. If the first run fails, read the job log, fix and push.
2. The owner downloads the artifact and plays it on a Mac. Fix what they
   hit first.
3. Do Level 0 in `docs/ROADMAP.md`: release guard, keep the original
   files, verify the prize modes against Wine, controller menu navigation.
4. Then the controller milestone: analog trigger pedals (opt-in), glyphs,
   rumble, then DualSense adaptive triggers through GameController.
5. Then the UI milestone: one settings model, drawn in the game's art,
   with Controls, Car Setup, Events, Mods and Developer pages.
