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
| Physics feel | Matches the original in a timed run | Same car position at 1.5, 3 and 4.5 s of full throttle (Wine reference) |
| Original lighting | Matches | Floor, wall and ceiling within a few RGB levels of the original |
| Widescreen, 4K, any frame rate | Done (Linux, OpenGL) | 1600×900 and 1280×960 screenshots; physics interpolation on |
| Settings: macOS menu bar | Exists, but it's the wrong door: a controller player can't use it. Must become a convenience only | Owner's playtest |
| Settings: in-game panel (F1) | Code written, not screenshotted | — |
| Mods (assets, `tuning.json`, GDScript, GDExtension) | Asset, tuning and GDScript mods proven | `examples/mods/hello-gdscript` printed live speed in a run |
| macOS build | **Green** on `macos-15` (run 6, commit 5e69484). Builds the converter and arm64 library, exports with Godot's universal template, thins it to arm64, ad-hoc signs, and a headless launch inside the packaged app loads the game library and runs the bundled converter. Download: the `Supersonic-RC-macOS-AppleSilicon` artifact on the latest run | https://github.com/catsandsoup/legorc-racer-port-mac/actions |
| macOS app on a real Mac | **Runs** (owner's first playtest, 2026-10-10): title, challenge select and races work on Metal. Bugs below | Owner's screenshots |

All "Done" rows were proven on Linux (Xvfb, llvmpipe, Godot's
Compatibility renderer). Metal, Forward+, MetalFX, the native menu, the
Gatekeeper flow and controllers on macOS are unproven.

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
5. The owner remembers the checkpoint arrow at the top of the screen. The
   Wine capture shows it at the bottom centre, as the port draws it.
   Re-check under Wine before changing anything.
6. A few textures on the car's rear fins don't match the original.
7. Scavenger Hunt and Supersonic Challenge haven't been compared against
   the original. Under Wine the projector closed once while entering
   Scavenger Hunt.
8. The controller is digital: triggers act as on/off at 30 %.
9. There's no release guard script yet, only `.gitignore`.
10. The original files aren't kept after import (only the converted data).

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
| `SSRC_COLLISION_PROBE=1` | Sweep a car-sized box at eight known surfaces, print PASS/FAIL, quit (used by `scripts/test-collision.sh`) |
| `SSRC_LM_MODE` | Lightmap blend experiments (0 is correct; leftover from calibration) |

For a fresh first-run test, set `HOME` to an empty folder that contains
`Downloads/Supersonic_RC.zip`.

## Next steps (in order)

1. **Baseline against the original under Wine** (`docs/PRODUCT_RULES.md`,
   "The baseline"):
   - compare every model and texture side by side
   - drive the same scripted routes over every surface (floor, ramps,
     stairs, shelves, upper floor, dinosaur, boxes)

   Fix known issues 1–3 and 5 with before/after screenshots.
2. **Controller-first menus, pause and Settings** in the game's own art
   (`docs/DESIGN_PRINCIPLES.md` §4–6):
   - the title, challenge select and help work by controller, with prompts
     for the device in use
   - Start, Options or Esc pauses
   - a Settings item on the title and challenge select
   - the macOS menu bar becomes a convenience

   Run both critics.
3. Level 0 in `docs/ROADMAP.md`: release guard, keep the original files,
   verify the prize modes against Wine.
4. Analogue trigger pedals (opt-in), glyphs and rumble, then DualSense
   adaptive triggers through GameController.
5. The Car Setup, Events, Mods and Developer pages.
