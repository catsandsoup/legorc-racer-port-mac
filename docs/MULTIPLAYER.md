# Multiplayer scope: split-screen and online

Status: scope only, nothing built. Written in session 2 (2026-10-10) at the
owner's request. The original game is single-player. Everything here is a
new mode and follows `docs/PRODUCT_RULES.md`: opt-in, off by default,
reachable from a menu, saved, and proven with screenshots.

Asked for:

1. Split-screen on one Mac, with two controllers or with a keyboard and a
   controller.
2. Online play against another Mac, and later a PC.

Sources: Godot 4.7.2 engine source and Godot's docs (`godot-docs` master),
the Jolt Physics docs, and Apple's TN3179. Paths and line numbers are in
the tables below.

## What the code assumes today

| Assumption | Where | Effect on multiplayer |
|---|---|---|
| One `Car`, one `ChaseCamera`, one `Hud`, one `GameMode` per race | `src/driving.h`, `DrivingSession` members | Each player needs their own car, camera, HUD and mode state |
| Input is read from global actions once per logic tick | `src/driving.cpp`, `DrivingSession::logic_tick` | Needs per-player actions, or input read per device |
| `DriveInput` is a plain struct, and `set_drive_input()` already exists | `src/car.h`, `src/driving.h` | This is the seam for a second player, or for input arriving over the network |
| Cars collide only with the world (`collision_mask = LAYER_WORLD`) | `src/car.cpp` | Two cars pass through each other until car-to-car collision is turned on |
| Audio is the original's 8 Director sound channels | `src/sound.h` | Two engine sounds would fight over the same channels |
| HUD and menus are drawn on a 640×480 stage | `src/game.cpp`, `src/driving.cpp` | Each half of the screen needs its own HUD |
| Every joypad binding matches any controller | `src/game.cpp`, `ANY_JOYPAD` (fixed in session 2) | Split-screen must bind each player to one device |

## 1. Split-screen

### How Godot supports it

- **Several views of one world.** A `SubViewport` uses the same world as
  the root viewport by default. Two SubViewports, each with its own
  `Camera3D`, show the same race from two cars
  (`tutorials/rendering/viewports.rst`, "Worlds" section).
- **Telling the players apart.** Every input event carries a `device` ID.
  Joypads are 0–15. The keyboard is 16 (`InputEvent::DEVICE_ID_KEYBOARD`)
  and the mouse is 32 (Godot 4.7.2 `core/input/input_event.h`, lines
  64–67). An action event set to device -1 (`InputMap::ALL_DEVICES`)
  matches any device; any other value matches only that device
  (`core/input/input_map.cpp`, `_find_event`). A new controller gets the
  lowest free ID (`core/input/input.cpp`, `get_unused_joy_id`).
- **Two keyboards are not supported.** IDs 17–31 are "reserved for
  multiple keyboard support in the future" (`input_event.h`, line 67).
  Two players on one keyboard would have to split it (arrows and WASD).

### Plan

1. **Player slots and a join screen.** "Press A or Enter to join": the
   first event from a device claims player 1, the next a different device
   claims player 2. This covers two controllers, or keyboard plus
   controller.
2. **Per-player actions.** Copy the driving actions per player
   (`p1_accelerate`, `p2_accelerate`, …), each bound to that player's
   device ID. Each car gets its own `DriveInput` through
   `set_drive_input()`. Menus keep accepting any device.
3. **A second car.** Build `DrivingSession` from a list of players, each
   with a car, camera, HUD and mode state. The world, lights and props
   stay shared.
4. **Views.** Two `SubViewportContainer`s side by side (best on 16:9),
   with top/bottom as an option. Each view scales the 640×480 HUD.
5. **Car-to-car collision.** A setting, off by default ("ghost cars"),
   because bumping changes the original's feel. Turning it on adds
   `LAYER_CAR` to each car body's collision mask.
6. **Sound.** One engine player per car, panned to that player's side.
   Shared effects stay on the Director channels.
7. **A two-player mode.** Start with a head-to-head race over the
   existing `Checkpoint_N` points (2 laps, first home wins). The five
   original modes stay single-player.

Effort: about 1–2 weeks for a playable head-to-head race, then polish.
Risk: low. Every piece is a standard engine feature, and the scene is
small (5,538 triangles), so two views should hold frame rate. Measure it
on the Mac before calling it done.

## 2. Online play

### Key decision: who computes the physics

| Option | How it works | Verdict |
|---|---|---|
| Lockstep: both machines simulate everything from shared inputs | Needs bit-identical physics on both machines | **Not viable for Mac against PC.** Jolt is deterministic across platforms only with `CROSS_PLATFORM_DETERMINISTIC` (Jolt `Docs/Architecture.md`, "Deterministic Simulation"). Godot's `modules/jolt_physics/SCsub` doesn't set it. Even Mac to Mac needs the same binary and the same API call order |
| Rollback | Rewind and replay when late inputs arrive | **Not viable with stock Godot.** Jolt has `SaveState`/`RestoreState` ("Rolling Back a Simulation"), but Godot's physics server doesn't expose them |
| **Each player owns their car** (recommended) | Each machine simulates its own car with the original physics and sends its pose (position, rotation, velocities) about 30 times a second. The other car is shown interpolated and moved kinematically | Viable. Your own car has zero input lag and exactly the 2003 feel. Car-to-car contact is approximate (a remote car is a moving obstacle), so ghost cars stay the default. Cheating is possible, which is fine for playing with friends |
| Host runs both cars | The client sends inputs and the host sends back state | Fair, but the client feels its own car lag behind by the round trip unless it predicts, and prediction needs the rollback Godot lacks. Revisit if ranked play is ever wanted |

### Transport

- **ENet**, built into Godot (`ENetMultiplayerPeer`). It is UDP only and
  supports IPv6 (`tutorials/networking/high_level_multiplayer.rst`,
  "Hosting considerations"). `MultiplayerSynchronizer` "synchronizes
  properties from the multiplayer authority to the remote peers"
  (`modules/multiplayer/doc_classes/MultiplayerSynchronizer.xml`). That
  matches the plan of each player owning their car.
- **Same network (LAN):** connect by IP, or find hosts with a UDP
  broadcast.
- **Internet:** home routers use NAT, so the host must forward a **UDP**
  port (same Godot doc). Godot's `UPNP` class can ask the router to do it
  automatically. Godot's own docs warn that not every router supports
  UPnP and some users turn it off, and suggest a relay server, NAT hole
  punching or STUN/TURN as fallbacks (`modules/upnp/doc_classes/UPNP.xml`).
- **WebRTC** does NAT traversal but needs a signalling server. On desktop
  it also needs the separate `webrtc-native` GDExtension
  (`tutorials/networking/webrtc.rst`). Its licence is MIT
  (`godotengine/webrtc-native/LICENSE`).

### Plan

1. **Network layer.** An ENet host/join screen, a version handshake and a
   ready check.
2. **Synced cars.** Each player owns and simulates their car and sends
   its pose; the other car is interpolated. Reuse the split-screen
   player/car list, so online is split-screen with one view per machine.
3. **Race state.** The host runs the race clock, countdown, checkpoint
   order and results. Clients report checkpoint times.
4. **LAN first,** then UPnP for the internet, then a manual "forward this
   UDP port" fallback with in-game help.
5. **PC later.** ENet is cross-platform. The work is a Windows build of
   the game and the converter, and a CI job.

Effort: about 2–3 weeks for LAN play between two Macs once split-screen
exists. Add 1–2 weeks for internet play via UPnP and port-forward help.
Add a relay or WebRTC path only if testers can't connect.

## Dependencies you need to know about

| What | Who owns it | Where | When it bites | Why it matters |
|---|---|---|---|---|
| Physics isn't identical across Mac and PC | Jolt / Godot | Jolt `Docs/Architecture.md`; Godot `modules/jolt_physics/SCsub` | Choosing the netcode model | Rules out lockstep and rollback. Each player owning their car avoids the problem |
| macOS local network permission | Apple | TN3179 "Understanding local network privacy" | First LAN connection on macOS 15 or later | macOS shows a permission alert the first time the app touches the local network. The app needs `NSLocalNetworkUsageDescription` in its Info.plist (and `NSBonjourServices` if Bonjour is used). If the player denies it, LAN play fails silently, so the game must explain and point to System Settings → Privacy & Security → Local Network |
| Ad hoc signing | Apple; the project signs ad hoc (`scripts/package-macos.sh`) | TN3179, ad hoc section | Every new build | Apple: local network privacy tracks the app by its code signature and says to sign with an Apple-issued identity for reliable tracking. With ad hoc signing, players may be asked again after each update. A paid Developer ID would fix it |
| Port forwarding and UPnP | The player's router | Godot `high_level_multiplayer.rst`; `UPNP.xml` | Internet play | The host must open a UDP port; UPnP helps where the router allows it |
| WebRTC on desktop is a separate plugin | Godot (`webrtc-native`, MIT) | `tutorials/networking/webrtc.rst` | Only if a NAT-traversal path is needed | An extra download in the app, plus a signalling server someone has to run |
| Both players need matching game data | Each player | `manifest.json` from `ssrc-import` | Joining a game | Each player converts their own copy, and no game bytes are ever sent. The handshake compares the build version and the converter's `format_version` |
| Two keyboards aren't supported | Godot | `input_event.h`, line 67 | Keyboard plus keyboard | Use keyboard plus controller, or split one keyboard |
| A Windows build | This project | `game/export_presets.cfg` has macOS and Linux presets only; no Windows preset or CI | Mac against PC | Needs a Windows export preset, the converter and the game library built for Windows, a CI job, and testing |
| Godot 4.8 default change | Godot | `controllers_gamepads_joysticks.rst`, "Window focus" | Upgrading past 4.7 | New 4.8 projects ignore controllers while the window is unfocused. Irrelevant for one window, but worth knowing when upgrading |

## Suggested order

1. Split-screen head-to-head race (local). It builds the player list,
   per-device input, a second car and HUD, and a two-player mode.
2. LAN play between two Macs on top of the same player list.
3. Internet play with UPnP and port-forward help.
4. Windows build, then Mac-against-PC.

Each step ends with the usual proof: before/after screenshots, measured
speed at 1.00× and critiques. The original single-player game stays the
default throughout.
