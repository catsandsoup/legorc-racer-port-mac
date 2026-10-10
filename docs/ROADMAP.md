# Roadmap

Goals for a modern player, ranked by how hard they are. Every gameplay
change follows `docs/PRODUCT_RULES.md`: off by default, a hook, a UI entry
point, saved, and proven with screenshots of the real build.

Facts to plan with:

- **Small scene.** The playfield is 5,538 triangles in 110 models with 46
  textures and 9 baked 512×512 lightmaps. The car is 5 models with 13
  textures.
- **Already full detail.** The W3D meshes are progressive (CLOD). They are
  converted at full resolution: the lightmap UV counts, authored at
  Director's `lod.level = 100`, match the vertex counts exactly.
- **Logic size.** The game logic is about 3,000 lines of Lingo, now C++
  in `src/`.
- **Physics.** One Jolt rigid body plus four ray-cast wheels, driven by the
  original's forces (`src/car.cpp`). Every constant is in `src/tuning.h`
  and overridable by `tuning.json`.

## Level 0: finish what exists (do first)

| Goal | Notes |
|---|---|
| First green macOS CI build, then play the downloaded app on a Mac | Never run on macOS yet. Check Metal, the menu bar, MetalFX, the DualSense, first-run import from Downloads, and the Gatekeeper prompt |
| ~~Release guard~~ (done in session 2; hashes still to add) | Script plus CI step that fails if a commit or built app contains game-derived bytes: file types (`.dcr .cct .w3d .hke .glb`, converted PNG/MP3) and known hashes of `game.dcr`/`GUILibrary.cct` |
| ~~Keep a copy of the player's original files~~ (done in session 2) | Copy `game.dcr` + `GUILibrary.cct` into `Application Support/Supersonic RC/original/` on import; re-import from there |
| Verify the prize modes against the original | Scavenger Hunt and Supersonic Challenge haven't been compared with the Wine reference (prize textures, UV flip for `prize.glb`) |
| Car rear fin textures | A few fin UVs differ from the original (see the HANDOFF screenshots). Check the CLOD texture-coordinate decode for those meshes |
| Pause menu | The original has none (Esc quits the race). Add one only as an opt-in hook that opens the same settings model |
| Controller-driven menus | Focus navigation done in session 2 (level select, settings panel, Start opens settings). Still to do: button glyphs, proof on the real menus |

## Level 1: hooks on systems that already exist (days each)

| Goal | How |
|---|---|
| **Analog throttle and brake from triggers** (Xbox 360/One/Series, PS4, PS5) | `Wheel::update` scales `drive_force` and `brake_force` by `Input.get_action_strength`. Digital keys stay at 1.0. Expose a response curve and deadzone. Off by default (the original is digital) |
| Analog steering with progressive lock | Already partly done: the stick sets a target angle and steering moves toward it at the original rate. Add sensitivity and linearity settings |
| Controller auto-detect and glyphs | Godot reports the device name (`Input.get_joy_name`). Ship Xbox and PlayStation glyph sets and show the right one in prompts and the help screen |
| Rumble | `Input.start_joy_vibration` on crashes (`Car::after_physics_contacts`), landing a stunt and skidding. Strength setting |
| **Car performance** (power, grip, top speed, weight, steering lock) | Already data in `Tuning`. Add a "Car Setup" page with sliders and presets ("2003", "Grippy", "Drift"). Write `tuning.json` in the user folder |
| **Physics changes** (gravity, suspension, damping, Havok attenuation) | Same as above, on a developer page |
| **Event timing** (time limits, time bonuses, score goals, laps) | Move the per-mode literals (150 s, +2 s, 2500 points, 2 laps…) from `src/driving.cpp` into a `modes.json` table read like `tuning.json`; expose them per mode |
| Easy texture mods | Asset overrides already work (`mods/<name>/…`). Add a Mods page: list, enable/disable, load order, and "Install mod…" that takes a zip and unpacks it into `mods/` |
| Easy car-model swaps | `scenes/car.glb` overrides already load. Validate the node contract (`Car` + `Wheel1..4`), show errors in the Mods page, and add a model and texture preview |
| Highest quality rendering | Already full-resolution meshes. Add 4×/8× MSAA plus supersampling presets, anisotropic 16× by default, mipmap bias, and an optional sharpening filter |

## Level 2: new subsystems built on engine features (weeks each)

| Goal | How |
|---|---|
| **DualSense adaptive triggers** (pedal resistance that rises with throttle; brake feel) | Godot has no API for this. Add an Objective-C++ bridge in the GDExtension using Apple's GameController framework: `GCDualSenseAdaptiveTrigger` (macOS 11.3+) and `setModeSlopeFeedback…` (macOS 12.3+). Drive resistance from `load` and speed. Needs a real DualSense to prove |
| **Lighting replacements** | Enhanced mode exists (Godot lights, shadows, SSAO, glow). Next: PBR material upgrade with roughness and normal maps from texture packs, baked `LightmapGI` or SDFGI as a replacement for the 2003 lightmaps, time-of-day presets ("store lights on"). All as alternatives to Original |
| HD texture packs | A pipeline: export converted textures, upscale them outside the game, repack as a mod. Ship tooling and docs, never the textures |
| Occlusion culling and LOD | Not needed for speed (5.5k triangles), but `OccluderInstance3D` baking and visibility ranges matter if mods add heavy models. Add when a mod needs it |
| **New game modes** | A data-driven mode format (JSON): checkpoint sets placed by name or position, timers, goals, prize sets, unlock chains. The five originals become data. A ghost or time-trial mode records and replays the car transform per logic tick |
| Car mods with their own physics | Read hull, mass, wheel positions and tuning from the glTF (extras) so a new car drives differently, not just looks different |

## Level 3: research projects (months, high risk)

| Goal | How and risk |
|---|---|
| **AI racers** | Needs racing lines (start from the checkpoint order and the `Checkpoint_N` positions), an AI driver feeding the same `DriveInput` to more `Car` instances, car-to-car collision layers, and a race mode with positions. The physics already supports more cars; the hard part is AI that handles stunts and recovery without cheating. Prototype on Free Driving's checkpoint loop first |
| Split-screen multiplayer | Scoped in [MULTIPLAYER.md](MULTIPLAYER.md): per-device player slots, a second car, two SubViewports, a head-to-head checkpoint race. About 1–2 weeks; low risk |
| New tracks | Replace `world.glb` plus `collision.json` plus lightmaps; needs an authoring guide and a lightmap bake |
| Online play (Mac to Mac, later PC) | Scoped in [MULTIPLAYER.md](MULTIPLAYER.md): each player owns their car (lockstep and rollback aren't viable with Godot's Jolt), ENet, LAN first, then UPnP. About 2–3 weeks after split-screen. Leaderboards need a server and a privacy review |

## UI for all of this

One settings model (`build_options()` in `src/game.cpp`) feeds the macOS
menu bar and the in-game panel today. Extend it with pages:

- Display
- Graphics
- Controls (bindings, trigger curves, rumble, adaptive triggers)
- Car Setup
- Events
- Mods
- Developer (physics, hidden)

Draw the in-game version in the original's art direction (brushed-metal
panels and chrome italic headings from `images/art/Background.png` and the
buttons). Every page must be fully usable with a controller. Every setting
must be saved and have a README line.
