# Supersonic RC native port: instructions for AI agents

Read these first, every session, in this order:

1. [docs/PRODUCT_RULES.md](docs/PRODUCT_RULES.md): the standing rules, the
   goal, the Wine baseline and what "done" means.
2. [docs/PLAYER_JOURNEY.md](docs/PLAYER_JOURNEY.md): every stage from the
   player's side.
3. [docs/DESIGN_PRINCIPLES.md](docs/DESIGN_PRINCIPLES.md): how every screen
   is designed, including the sampled art direction and the controller-first
   rules.
4. [HANDOFF.md](HANDOFF.md): current state, known bugs, how to run and test.
5. [docs/ROADMAP.md](docs/ROADMAP.md): goals by feasibility.

Judge every milestone against the player journey and "Done means". For any
screen, hook, setting or visual change, run the critics
(`.claude/agents/ux-critic.md`, `.claude/agents/style-critic.md`) on fresh
screenshots before calling it done.

Working rules:

- Commit and push every change to `origin main` with small commits and
  clear messages. Before each push, check that nothing game-derived is
  staged. That means no:
  - `.dcr` or `.cct` files
  - converted data
  - screenshots of the game
  - anything from `local/`
  - local absolute paths
- Changes to gameplay (physics, timing, content, progression) are opt-in
  hooks, off by default, with a UI entry point.
- The original under Wine is the ground truth
  ([docs/REFERENCE.md](docs/REFERENCE.md)). Prove features with fresh
  screenshots of the real build compared against it, not against this
  repo's earlier screenshots or claims.
- Submodules: `git submodule update --init` (not `--recursive`).
