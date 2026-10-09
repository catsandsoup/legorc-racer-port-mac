# Modding Supersonic RC

Open **Game → Open Mods Folder**. On macOS that's
`~/Library/Application Support/Supersonic RC/mods`. Each mod is a folder
there. Mods load alphabetically, or in the order listed in
`mods/load_order.txt`. Later mods win. **Game → Reload Mods** applies
changes to the next race.

## Replacing assets

A mod mirrors the layout of the converted game data (**Game → Show
Converted Data**). Any file with the same relative path replaces the
original:

| To replace | Put this in your mod |
|---|---|
| A world texture | `scenes/world_textures/<Name>.png` |
| A car texture | `scenes/car_textures/<Name>.png` |
| A lightmap | `lightmaps/<Name>LightingMap.png` |
| The whole car model | `scenes/car.glb` (glTF binary; keep a node named `Car` with children `Wheel1`..`Wheel4`) |
| Checkpoint star / prize / arrow | `scenes/star.glb`, `scenes/prize.glb`, `scenes/arrow.glb` |
| Menu art | `images/art/Title-v8.png`, `images/art/Background.png`, ... |
| A sound or music loop | `audio/<name>.ogg`, `.wav` or `.mp3` (e.g. `audio/musicloop1.ogg`) |
| Collision | `physics/collision.json` |

Textures can be any resolution, so 4K texture packs work. Models are
plain glTF 2.0, so you can open `scenes/car.glb` in Blender, edit it and
export it back. Models use Director's coordinate system: Z up, 1 unit =
1 inch.

## Physics and rules: `tuning.json`

Every gameplay constant from the 2003 game is listed by name in
[`src/tuning.h`](../src/tuning.h). A `tuning.json` in any mod overrides
values:

```json
{ "drive_force": 9500, "gravity_factor": 0.15, "checkpoint_radius": 400 }
```

`examples/mods/fast-car` is a working example. Game speed and physics
rate are in the Gameplay menu, not in tuning.

## Script mods (GDScript)

Put a `mod.gd` in the mod folder. It is attached to a node that lives as
long as the game. `get_meta("game")` returns the game object:

- signals `session_started(session)` and `session_ended()`
- `get_session()` returns the race in progress. It has children named
  `car` (a `RigidBody3D`), `camera`, `world` (the converted playfield) and
  `hud`
- `get_tuning()`, `start_mode(n)`, `go_title()`, `go_select_level()`

See `examples/mods/hello-gdscript`.

## C++ mods (GDExtension)

Build a GDExtension against `godot-cpp` 4.5 or later and put the
`.gdextension` file and its libraries in the mod folder. It loads at
startup. To have the game instantiate one of your classes, add a
`mod.json`:

```json
{ "node_class": "MyModNode" }
```

The class must extend `Node`. It gets the same `game` meta as script mods.
