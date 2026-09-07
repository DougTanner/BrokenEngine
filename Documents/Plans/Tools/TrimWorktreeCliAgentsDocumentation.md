<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:55:53.642Z","dependsOn":[]} -->
# Trim WorktreeCli agent documentation

## Context

`Tools/WorktreeCli/AGENTS.md` measures 2,189 `bt-token-v1` tokens against a 1,947-token formula budget (27,066 direct code tokens, zero direct child documents), an advisory excess of 242. Command descriptions and `## Coordination State` repeat scheduler semantics already owned by `Documents/Plans/AGENTS.md` and skill references.

## Design

The author recommends retaining WorktreeCli-specific command contracts, exit codes, locking safety, and project ownership while replacing generic Plan metadata/selection explanations with a direct link to `Documents/Plans/AGENTS.md`. Shorten command inventories to behavior callers must know and remove repeated examples or negative descriptions where the same boundary is already explicit.

## Critical files

- `Tools/WorktreeCli/AGENTS.md`

## In scope

- Condense `## Executable and Commands`, especially long `lock` and `plan` bullets, without dropping supported verbs, state distinctions, identity checks, or exit semantics.
- In `## Coordination State`, remove scheduler metadata, selection, stale-edge, and claim-lifecycle duplication owned by `Documents/Plans/AGENTS.md`; retain tool-specific storage, healing, hash, guard, and fail-closed mutation rules.
- Tighten `## Project Ownership` bootstrap narration while retaining project membership, shared-tool, promotion, exclusion, and DataPacker bootstrap obligations.

## Out of scope

- C++, project files, CLI commands/options/results/exits, lock or scheduler behavior, storage/healing, safety gates, bootstrap, or build behavior.
- Editing Plans guidance, scripts, compile guidance, or other tool documentation.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the formula budget or explain unavoidable operative excess.
- All caller-visible commands, meaningful state/error distinctions, identity constraints, scheduler guard/fail-closed rules, build-result contract, project ownership, and bootstrap obligations remain explicit or directly linked to their owner.
- Generic Plan scheduler semantics are not duplicated from `Documents/Plans/AGENTS.md`.

## Classification

Tier 1 — documentation-only condensation with no CLI or coordination behavior change.

## Coordination

None.

## Notes

Safety and fail-closed coordination rules take priority over the advisory target.
