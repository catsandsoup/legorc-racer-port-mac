# Session log: decisions and what the owner asked for

A record of the key conversations, so a new session knows the reasons
behind the work. Owner: catsandsoup. The newest session comes first.

## Session 1 (2026-10-09 to 10)

### The owner's priorities (in their words, condensed)

- **"Plug and play, not technical marvel."** Free Roam, refreshing
  (settings that apply instantly, quick restarts) and a new UI come before
  technical extras.
- Download, double-click, drive with a controller. The Mac menu bar must
  not be the only way to reach anything: "as a controller user I can't play
  with them".
- **Free Roam is a major gameplay goal:** drive anywhere, no timer, quit any
  time.
- **R (or a controller button) resets the car upright**, like Need for
  Speed Most Wanted and Carbon.
- No exotic options nobody can use (the owner rejected "480 Hz").
  Physics must behave the same at 60, 120 and 240 Hz displays.
- Restarting through the original's old menus is too slow. Changes must
  apply without a restart.
- **The original under Wine is the baseline** for every model, texture and
  surface. The owner asked for automation so regressions are caught, and
  approved this four-layer plan ("I love this"):
  1. Read and write the original's car state from its memory under Wine:
     telemetry, and spawn anywhere. **Done** (`scripts/reference/orig_memory.py`).
  2. Many short routes, each run several times in the original to measure
     its own run-to-run spread. **Done** (`compare_route.py`,
     `tests/routes/`). The owner added: "spawn anywhere, you aren't bound by
     the old game's limitations".
  3. **A whole-level robot for the port alone.** Hundreds of spawns and
     headings; flag falling below the floor, ending up inside scenery,
     rolling over on flat ground, long airtime and frame drops. **Not built
     yet.**
  4. Matched-pose screenshots for looks. **Done** (`compare_views.py`).
- Docs, rules and the handoff live in the repo, so a new chat can see
  them. The owner's PlayStation-port docs (2Xtreme) were adapted into
  `docs/PRODUCT_RULES.md`, `docs/PLAYER_JOURNEY.md`,
  `docs/DESIGN_PRINCIPLES.md` and `.claude/agents/`.

### Findings that changed the plan

1. **Collision was one-sided and inverted.** The car passed through
   floors, walls, ramps and stairs. Fixed with two-sided shapes;
   `scripts/test-collision.sh` guards it.
2. **The original runs at 40 fps** (the movie's frame rate). Each frame is
   two physics updates, 12.5 + 37.5 ms of simulation (2.0x). The port had
   assumed 60 updates of 37.5 ms. Fixed; straight-line speed and braking
   match.
3. **The torque dividers were wrong.** They were refitted on the car's own
   axes against the original's telemetry: pitch 500, roll 1400, yaw 1000.
4. **The original's mass, inertia, friction and restitution** were read
   from memory and equal the port's.
5. **Texture V flip.** The world flip is not right for every mesh
   (shelves, dinosaur), and the car's lack of flip isn't right for every
   car mesh. This is the cause of the "broken textures". Not fixed yet.
6. The movie's 40 fps cadence raised the physics tick rate. The owner's fps
   fell from 92 to 48 (not fixed yet), and the arrow and wheels jittered
   (fixed: both now follow the smoothed motion).
7. Switching upscalers freezes the Mac build, so they are hidden.

### The owner's latest requests (end of session 1)

- **"The goal is a fun playable game, not a pure 1:1 copy."** Recreate
  Director behaviour where it makes the game good; don't chase every
  Director quirk.
- **GitHub page** (README) must showcase:
  - the new native menus and enhancements, why they exist, and why the
    old menus were replaced (controller players couldn't use them)
  - the modding capacity, easy for people and their agents to find
  - a call for contributors to port it to Windows, Linux, mobile and
    others ("I look forward to seeing how other people enhance this
    work")
- **The UI must respond to controller detection.** Button prompts already
  switch between keyboard, Xbox and PlayStation. Extend this (glyphs,
  hints) and say so in the README.
- **Future goal:** 2- or 4-player local co-op ("lounge"), maybe online
  later. Add to the roadmap.

### Task list at the end of session 1

| Task | State |
|---|---|
| Extract and catalogue all game assets and Lingo | Done |
| Analyse game logic from the decompiled Lingo | Done |
| Native port on Godot 4.7 (Metal, Jolt, interpolation) | Done |
| macOS menu, settings, first-run import, .app packaging | Done |
| Public repo, CI macOS build | Done (green) |
| `ssrc-import` converter | Done (texture V flip bug open) |
| Mac playtest bugs: handling, stairs, dinosaur, arrow, full screen | Partly: collision, timing and handling fit done; textures, jitter and the fps drop open |
| Layer 1: original's car state from memory | Done |
| Layer 2: short parity routes with spawn anywhere | Done (step_steer, slalom, brake, drop, hard_left) |
| Layer 3: whole-level invariant robot | **To do** |
| Layer 4: matched-pose visual comparisons | Done (12 views) |
| **Free Roam** | Done: verified on screen (Linux): no timer, points or arrow; pause, leave |
| Quick wins (R reset, restart, live lighting, help once, physics labels) | Done: R verified by telemetry; live lighting seen in Settings; help-once and restart not yet screenshotted |
| New UI: title bar, controller challenge select, pause, Settings | Done (Linux screenshots); not yet seen on the owner's Mac. The pause now really stops the race and its clock |
| UX and style critic pass on the new UI | **To do** |
| Texture coordinates (shelves, dinosaur) | **Done**: the CLOD decoder's UVs were half a tile low; the converter now shifts them by +0.5 (not the lightmap UVs). Matched views went from scores 9–20 to 6.6–10.8; shelves and dinosaur match the original |
| Car textures | **Done**: with the +0.5 shift, the car needs the same V flip as the world (`importer/src/movie.rs`). The car's rear now matches the original (dark, red side lights). All 12 matched views score 4.0–7.5 |
| README showcase (menus, why replaced, modding for people and agents, call for ports, co-op) | **Done** |
| Arrow and wheel jitter | **Done (Linux)**: arrow placed every rendered frame from the interpolated camera; wheels follow the body every physics tick. Steady in consecutive frames at speed; not yet felt on the owner's Mac |
| fps drop from 92 to 48 | **To do** |
