# Documents - Design-Time Reference and Planning

Holds design-time documentation and three parallel planning trees. No build artifacts, no code.

## Reference Material

- `FreshMachineSetup.md` — ordered fresh-machine bootstrap: Developer
  Mode/symlink privilege, clone, one-time primary ThirdParty build, primary data
  export, first wrapper run (session start rebuilds the shared AgentTools and
  ThirdParty binaries incrementally and prebuilds DataPacker Release so new
  worktrees can be pre-loaded with the built binaries instead of rebuilding
  them, then, unless that prebuild did not complete cleanly, runs that
  DataPacker to re-export the primary game data from the primary checkout's
  current assets). Human-facing counterpart to the wrapper scripts under
  `.agents/scripts/`.
- `C++StyleGuide.txt` — numbered style rules (Hungarian notation, Allman braces, `auto` restrictions, DirectXMath conventions). Source of truth for the `code-style-review`, `comment-review`, and `repo-code-review` skills.
- `FloatingPointDeterminism.txt` — the rules that keep rollback and replay bit-identical across client and server: `/fp:strict`, FMA3 disabled, per-thread MXCSR, fixed 32 Hz timestep, deterministic RNG, single shared CRC. Read before touching simulation math.
- `UserInterfaceDesign.txt` — the ImGui layout contract for player-facing screens: one scale factor (`engine::UiScale()`), the three sizing rules, shared layout constants, pivot centering, size-to-content, themed buttons, and vertical rhythm. Layout counterpart to `FloatingPointDeterminism.txt`; source of truth for `Ui/Screens/` geometry. Read before touching menu/HUD layout.
- `Architecture/` — external detail too large for AGENTS.md: Mermaid diagrams (`FrameUpdatePipeline.md`, `GameReconciliation.md`) and the network protocol specification (`Network.md`). Relevant subsystem AGENTS.md files link to them; update the affected document when its phase ordering, reconciliation flow, or protocol contract changes.

## Planning Trees

Design-time documents live in three sibling directories:

| Directory | Scope |
|-----------|-------|
| `Plans/` (`Plans/AGENTS.md`) | Tasks ready to be picked up by another session through `/next-plan`, such as refactors and bugfixes. Debt reduction — cleaning, decomposing, renaming, deleting dead code, fixing races/NaNs/precision, defensive shader clamps. |
| `Features/` (`Features/AGENTS.md`) | Ideas saved for another day, decided or not. Manually executed; never scheduler-tracked. |
| `Investigations/` (`Investigations/AGENTS.md`) | Work wanted soon but not to be picked up automatically by `/next-plan`, plus findings records and overviews. Never a scheduler input. |

Decision-complete means every choice needed to implement is already made: no open options, no TBDs; a Plan's investigation portion (`Plans/AGENTS.md` `## Plan files`) is the one open part it allows.

Deciding test: *is this wanted soon?* No → `Features/`. Yes, and ready to be
picked up by another session through `/next-plan` → `Plans/`, written
decision-complete; otherwise → `Investigations/`. Every plan document under
`Plans/` carries a byte-zero, Git-tracked metadata marker; a marker-less one is
a validation error, not a manual document. WorktreeCli selects executable Plans
deterministically by immutable creation time and normalized path. Each tree uses
area subfolders; `Plans/AGENTS.md` defines the Plans areas.
