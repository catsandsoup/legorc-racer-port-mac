# Supersonic RC native port: instructions for AI agents

**Read [docs/PRODUCT_RULES.md](docs/PRODUCT_RULES.md) first, every
session.** Then read [HANDOFF.md](HANDOFF.md) (current state, how to run
and test) and [docs/ROADMAP.md](docs/ROADMAP.md) (goals by feasibility).

Working rules:

- Commit and push every change to `origin main` with small commits and
  clear messages. Before each push, check that nothing game-derived is
  staged: no `.dcr`/`.cct`, converted data, screenshots of the game, or
  local absolute paths.
- Changes to gameplay (physics, timing, content, progression) are opt-in
  hooks, off by default, with a UI entry point.
- Prove features with fresh screenshots of the real build. Compare against
  the original running under Wine ([docs/REFERENCE.md](docs/REFERENCE.md)).
- Submodules: `git submodule update --init` (not `--recursive`).
