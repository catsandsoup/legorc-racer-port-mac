# Prompt for the next session

Paste this into a new Claude Code session with
`catsandsoup/legorc-racer-port-mac` attached.

---

You're continuing my native Apple silicon port of LEGO Supersonic RC
(2003, Shockwave). The repo is `catsandsoup/legorc-racer-port-mac`.

Before anything else, read these files in the repo, in this order:

1. `docs/PRODUCT_RULES.md` (the standing rules, the goal, the Wine baseline;
   follow them exactly)
2. `docs/PLAYER_JOURNEY.md` and `docs/DESIGN_PRINCIPLES.md` (the player's
   side, the art direction, controller first)
3. `HANDOFF.md` (current state, known bugs from my Mac playtest, how to
   build, test hooks)
4. `docs/ROADMAP.md` (goals ranked by feasibility)
5. `CHANGELOG.md`, `docs/PORTING.md`, `docs/REFERENCE.md` (how to run the
   original under Wine as the reference) and `docs/MODDING.md`

Don't treat anything in them as true until you've seen it on screen
yourself.

As a modern gamer I want:

- the highest-quality LOD and culling available
- to explore new lighting replacements
- texture mods that are easy to install, and car model mods
- new game modes and possibly AI racers
- to change event timing, physics and car performance
- native Xbox 360, Xbox One/Series, PS4 and PS5 controller support, with
  responsive triggers that set pedal pressure from how far they're pushed
  (and DualSense adaptive triggers if feasible)
- a native UI that hooks into the game to adjust all of these

Every gameplay change is opt-in and off by default, as the rules say.

Follow "Next steps" in `HANDOFF.md`:

1. First, the baseline against the original under Wine: every model and
   texture, and driving every surface. Fix the bugs from my playtest.
2. Then controller-first menus, pause and Settings in the game's own art.
3. Then the rest of Level 0, then controllers, then the UI pages.

Work in small milestones. Each one ends with:

- before/after screenshots of the real build
- measured speed at 1.00×
- a UX critique and a style critique (`.claude/agents/`) against the
  original's screens, each with a verdict and at most five ranked fixes

Commit and push every change and keep `HANDOFF.md` current. Ask me before
architecture changes, downloads, publishing or anything irreversible.

The game files aren't in the repo. Ask me for `Supersonic_RC.zip` when you
need it.
