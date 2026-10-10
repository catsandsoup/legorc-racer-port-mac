# Design principles: Supersonic RC

How design thinking is built into every screen, hook and setting. Read with
[PRODUCT_RULES.md](PRODUCT_RULES.md) and [PLAYER_JOURNEY.md](PLAYER_JOURNEY.md).
Reviews use the two critics in `.claude/agents/` (`ux-critic`,
`style-critic`) on fresh screenshots.

## 1. Start with the experience, work back to the technology

Every screen begins as one sentence about what the player is doing and
feeling, written before any code:

| Moment | The sentence |
|---|---|
| First launch | "I opened it, it found my game, and I'm at the Supersonic RC title in seconds." |
| Returning | "I double-clicked and I'm at the title." |
| Choosing a challenge | "I picked a challenge with my controller without touching the mouse." |
| Driving | "It drives exactly like I remember, only sharper and smoother." |
| Pause | "I pressed Start, the race froze, I changed one thing and I'm back driving." |
| Settings | "I found Settings on the title screen and it looked like it belonged." |
| Error | "It told me exactly what was wrong and what to press next." |

If a design doesn't serve its sentence, it's wrong, however clever it is.

## 2. Focus means saying no

- Fewer options, each obviously worth having. A setting exists for the
  player, not because Godot exposes it.
- Defaults must be right, so most players never open Settings.
- Nothing appears until it works. No placeholder buttons, no "coming soon".
- Developer diagnostics and experiments live behind a hidden Developer page,
  never in player menus. These include:
  - physics substeps and tuning sliders
  - lightmap modes
  - the Enhanced lighting experiment, until it has been compared with the
    original

## 3. The game is the hero

- The fewest possible steps between double-click and driving.
- First-run setup appears once, then never again.
- The pause menu covers as little as it can. The race stays visible and
  frozen behind it, so picture changes are seen live.
- Keep the original screens that still work: the title, the challenge
  select, the help and the HUD. Add only what's missing: pause, Settings,
  and controller focus and prompts.

## 4. It must look like Supersonic RC

Sampled from the original running under Wine. Reference captures live in
`local/reference/` and are never published.

- **Panels:** brushed metal, light grey around `#a1a1a1`–`#aaaaaa`, with a
  bevelled edge and rounded corners. The 4:3 menus sit on black.
- **Headings:**
  - chrome, italic, stencil-like capitals with a dark outline
    ("CHOOSE YOUR CHALLENGE")
  - the chrome "Supersonic RC" logo with the red LEGO logo
- **Buttons:** recessed metal pills (face around `#919191`). Labels are
  black italic block capitals on two lines. The highlighted label is blue
  (`#002c83` deep to `#1272bf` light). Check on the original whether blue
  means hover or unlocked.
- **Body text:**
  - near-white (`#f0f0f0`) sans-serif on a darker metal inset, for the
    challenge descriptions
  - white with a grey outline (`#787677`) for "Click to Start"
- **HUD:** white italic "Time remaining" and "Points" labels with large
  spaced digits.
- **Accents:**
  - the yellow 3D arrow
  - the red, orange and yellow burst behind the title car
  - the LEGO red logo
- **Not:**
  - stock Godot grey panels or a plain dark settings window
  - macOS-style lists
  - dense tables and tiny controls
  - thin focus outlines
  - anything stretched out of 4:3

One token file (colours, type sizes, spacing, panel shapes, focus
treatment, prompt strip) drives the setup screen, the pause menu and
Settings. Exact colours are sampled from the reference screenshots, not
guessed. Where the original's own bitmaps exist (`images/art/*`), use them
rather than redrawing them.

## 5. One interface, many doors

- These all open the **same** menu, with the same state, persistence and
  validation:
  - Esc
  - the controller's Start or Options button
  - ⌘,
  - the new Settings item on the title and challenge select
- Entry context is kept. Back from Settings returns to the title or the
  challenge select. Back from pause resumes the same frame.
- The macOS menu bar only calls the menu's actions. It's a convenience for
  mouse users, never the only way to reach anything, and never holds its
  own copy of settings.
- Settings write the game's own variables: the `Tuning` table, the Director
  sound channel volumes and the saved unlocks.

## 6. Controller first, every time

- Every screen is fully usable with a controller alone, then the keyboard,
  then the mouse. That includes the title, the challenge select, help,
  driving, pause, Settings, first run and errors.
- Default controller layout:
  - Right trigger accelerates and left trigger brakes or reverses. Digital
    by default like the original; analogue as an option.
  - The left stick or D-pad steers.
  - A or ✕ is the handbrake (the original's Space).
  - Start or Options pauses.
  - B or ○ goes back in menus.
- Focus is unmistakable: the blue highlight the original already uses, plus
  a brighter, slightly larger button. Never just a thin outline.
- Prompts match the device in use: keyboard keys, Xbox glyphs or
  PlayStation glyphs. They switch as soon as another device is touched.
- No hover-only information.
- Opening or closing a menu never leaks a press into the race.
- Panels stay still as focus moves. No resizing, no jumping.

## 7. Feedback is live and honest

- Changes apply immediately and visibly where possible.
- A setting that needs a restart says so before you commit it.
- Errors say what happened and what to do, with one obvious next action.
- Hard cases are designed, not discovered:
  - controller unplugged mid-race
  - window loses focus
  - game files missing on relaunch
  - car out of the world

## 8. Judge the real thing

A UI milestone is finished only after both critics have reviewed fresh
screenshots of the complete flow: first run, returning, challenge select by
controller, pause, change a setting, back out, resume, and Settings from the
title. They report as follows.

- **UX critic:**
  - VERDICT
  - STEPS TO PLAY (first-time / returning)
  - REMOVE
  - MUST FIX: at most five, ranked, with exact changes
- **Style critic:**
  - VERDICT
  - FIT WITH SUPERSONIC RC
  - MUST FIX: at most five, each giving the screen, the problem and the
    exact visual change
  - compared side by side with the original's screens

Widgets rendering or callbacks firing is not proof. Anything not tested is
marked as not tested.
