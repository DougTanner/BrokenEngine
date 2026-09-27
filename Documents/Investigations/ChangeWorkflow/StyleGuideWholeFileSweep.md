# One-pass style guide sweep of existing C++

Runbook for bringing every first-party C++ file up to the style guide rules
the Change Workflow checks, in one pass: per unit (a header and its `.cpp`, or a lone
file), one Opus subagent checks every line and applies the fixes, and a second spot-checks them. It lives
here, not in `Documents/Plans`, so the scheduler never claims it; the user
starts it by asking a session to run this document. It replaces the earlier
per-rule and per-area style sweep Plans and their residual Plans.

## Relation to the Change Workflow

The user's request to run this document is the approval, and this runbook
takes the place of the Change Workflow steps from Approve and classify through
Verify the acceptance table for the sweep only: no `/prepare-change`,
`/plan-alternatives`, `/plan-audit`, `/plan-simplicity-review`, execution card,
`/repo-code-review`, `/comment-review` dispatch, `/coherence-review`, or
`/verify-acceptance`. What stays is listed under each phase below. The Verify
and land step applies unchanged: every stage lands through `/finalize-changes`
with one explicit user confirmation.

Recommended first: land `Documents/Plans/ChangeWorkflow/CodeStyleReviewPermittedForms.md`,
which names forms the guide permits; without it the sweep reports them again.

## Scope

- In: tracked `*.h` and `*.cpp` under `Common/`, `DataPacker/`, `Tools/`,
  `Engine/` and `Projects/` (about 570 files, 114,000 lines at `ff516591`).
- Out: `ThirdParty/`, and shaders except the reference updates a C++ rename
  propagates.
- Rules: the `/code-style-review` mandate — the scanner's `style-rule-<n>`
  kinds plus the hand-read list and permitted forms in
  `.agents/skills/code-style-review/references/worker.md` steps 7 and 10 — and
  rule 64 as `/comment-review` applies it. `Documents/C++StyleGuide.txt` is
  the authority for each.

## Fix bound

Wider than `/code-style-review`'s meaning-preserving bound (its worker steps
11-12): a fix may change a type, container, signature, overload choice,
error-handling path, or `kb*` toggle, provided observable behavior is
unchanged. Earlier sweeps stopped at the narrow bound and turned every such
site into a residual Plan; this sweep fixes them in place.

A finding is out of bound, and left unfixed, when its fix would change any of:
sim output or the per-tick CRC; serialized, save, replay, wire, or `.pack`
bytes; threading; or what a trust-boundary check accepts. Out-of-bound findings
are collected in the ledger (Phase 3) for the user to decide on; the sweep
creates no follow-up Plans on its own.

## Units and batches

- Unit: one header together with its same-name `.cpp`, or a lone file. A unit
  is the file scope of one fix-and-spot-check pipeline.
- Batch: one directory's units. `Engine/Source/<subsystem>/` directories
  holding more than 20 C++ files (`Agent`, `Frame`, `Graphics`, `Network`,
  `Ui`) and `Projects/BrokenEngineSandbox/Source/` `Agent/`, `Frame/`,
  `Network/` and `Ui/` are batches of their own; the rest of each area is one
  batch.
- Stage: one top-level area, landed on its own, in the order `Common/`,
  `DataPacker/`, `Tools/`, `Engine/`, `Projects/`, so a rename lands before the
  areas that consume it are swept.

Scratch files live under `Temp/StyleSweep/` (ignored by Git):
`<unit>.findings.md` per unit, `Ledger.md`, and `Progress.md`, which records
each finished unit and batch so a session that is compacted or restarted
resumes where it stopped.

## Phase 1 — per-unit pipeline

Batches run one after another. Within a batch, run about six unit pipelines
at a time; each pipeline is two fresh dispatches in order. Each subagent
writes its detail to its own section of the unit's findings file and returns
only a short handoff (counts, out-of-bound and cross-file items), keeping main's context
small.

1. Check and fix — fresh `implementer`. Runs the scanner's whole-file mode on
   the unit's files (invocation in `.agents/skills/code-style-review/SKILL.md`
   `## Inputs`), adjudicates each row, and hand-reads every line of the unit
   for the rest of the rule set in `## Scope`. Writes one row per finding:
   `path:line`, rule, the violating text, the fix, and a class — `local`
   (edits only the unit), `cross-file` (a rename or signature change whose
   references lie outside the unit), or `out-of-bound` (`## Fix bound`).
   Applies every `local` finding, including renames whose references stay
   inside the unit, and marks each applied or declined with its reason. Never
   edits a file outside the unit; `cross-file` findings are left for Phase 2.
2. Spot-check — fresh `reviewer`, findings only. Reads the findings file and
   `git diff HEAD -- <unit files>`, and reports any change that is not
   behavior-preserving, does not match its rule, or has no finding behind it,
   and any `local` finding left unapplied without a reason. Any report goes to
   one fresh `implementer` repair dispatch carrying a continuation capsule
   (`.agents/references/subagent-handoff.md` `## Continuation capsule`);
   whatever that round leaves unresolved goes to the ledger.

## Phase 2 — per-batch close

1. Propagate — one `implementer` applies the batch's `cross-file` findings and
   runs `/update-affected-code` over them, searching the whole repository for
   each renamed or changed symbol.
2. Build — `builder` runs `/compile` for the targets the handoffs name, and at
   least Client and Server Debug; a failure goes to `/resolve-findings`.
   Running this runbook authorizes Local generation (`-RunDataPacker`, Gaea
   export still forbidden) for any build whose changes trigger Local data mode,
   such as `DataPacker/**` or `Common/DataFile.h`
   (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`).
   `Tools/` changes to WorktreeCli or AgentHarness follow `/compile`'s
   AgentTools policy.
3. Commit — main commits the worktree (`/finalize-changes` squashes at
   landing), so the next batch's spot-checks diff against that batch's own
   start, and records the batch in `Progress.md`.

## Phase 3 — per-stage close and landing

1. `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree root> -Baseline <stage baseline SHA>`.
2. `implementer` runs `/update-claude-docs` for the stage's renamed or
   re-signatured identifiers.
3. `builder` runs `/compile` for Client and Server Debug and Release, Profile
   when the stage edited code conditional on `BT_PROFILE`, DataPacker Release
   when a changed file compiles into it, and WorktreeCli and AgentHarness for
   the `Tools/` stage.
4. `/agent-harness` replay determinism check for every stage except `Tools/`.
5. Main reports the stage: units swept, fixes applied, and the ledger
   (`Temp/StyleSweep/Ledger.md`: every out-of-bound and unresolved finding with
   `path:line`, rule, and why it was left), then lands it through
   `/finalize-changes`.

## Cost

About 350 units, so roughly 700 Opus dispatches plus one propagation,
build, and commit per batch (about 15 batches), and one landing per stage.
