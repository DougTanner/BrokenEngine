<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T21:27:43.137Z","dependsOn":[]} -->
# Fix: `/compile` — every builder handoff pastes the full `broken-engine-build-result/v1` envelope into main even when the build is a clean success

## Context
Observed at one `/next-plan` run's checkpoint, in the run that claimed
`Documents/Plans/Engine/PackChunkLoaderResetExclusion.md`.

`.agents/skills/compile/SKILL.md:61-63` (`## Handoff`) requires the `builder` to
"Include every build's captured `broken-engine-build-result/v1` envelope verbatim
in the handoff", and `.agents/references/subagent-reporting.md:117-120` exempts
that envelope from the shared handoff size caps. The requirement has no
success/failure distinction, so a build with `status: success`,
`diagnostics: []`, and `failureKind: none` delivers the same full envelope bytes
into main's context as a failing one, even though nothing in the envelope is
then decisive.

Context-efficiency envelope for the observed run — tool `Task` (`builder`
running `/compile`), invocation: the two `/compile` dispatches of that run;
measured sizes: 10,478 characters for the first handoff and 9,175 for the
second, 19,653 characters in total. Every build in both handoffs reported `status: success`
with empty diagnostics, so the whole 19,653 characters entered main as
non-decisive text.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7bbceba3-9f2d-41e9-8efd-5d44d90fda63
- Worktree/branch UUID: 0c3c3845-ec0f-4533-af6b-469c0303a5e6
- Session branch: claude/0c3c3845-ec0f-4533-af6b-469c0303a5e6
- Worktree: .claude\worktrees\BrokenEngine\0c3c3845-ec0f-4533-af6b-469c0303a5e6
- Landing ref: claude/0c3c3845-ec0f-4533-af6b-469c0303a5e6, this session's
  branch, whose landing commit will contain this Plan.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/CompileSuccessEnvelopeHandoffCost.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`; the requirement and its cap exemption are both visible in the
cited files without any transcript. Only when the transcript is genuinely
needed, in a new session run
`/next-plan-review claude/0c3c3845-ec0f-4533-af6b-469c0303a5e6` in bounded
friction mode, supplying the recorded client and the conversation session ID
recorded above. Then make the smallest fix inside the `## In scope` boundary
below.

Before changing anything, settle the one blocking question this Plan cannot
settle from prose alone: the landing gate consumes these envelopes.
`.agents/skills/finalize-changes/references/worker.md:76-77` requires the
finalizer to "Consume every typed receipt verbatim, each
`broken-engine-build-result/v1` envelope included, never summarized", and
`.agents/skills/finalize-changes/references/landing-acceptance-table.md:67-72`
requires the authoritative envelope's fields as the build row's evidence. Any
fix that merely drops the envelope bytes from main's context without giving the
finalizer another way to read them breaks that row, so the fix has to keep an
authoritative copy reachable.

This author's recommendation for that fix, offered as a starting point rather
than a binding decision: have the `builder` persist each build's captured
envelope to a `Temp/` file and cite it as path plus selector under `Evidence`,
then, for a build whose `status` is `success` with no `severity: error`
diagnostics, report inline only `status`, `exitCode`, `failureKind`, the game
data-mode fields `## Handoff` already requires, and `retainedLog.path` per
target, instead of the envelope bytes; a build that is anything other than that
clean success keeps its verbatim envelope inline exactly as today. The
finalizer then reads the envelope verbatim from the cited file rather than from
main's context, which keeps its "verbatim, never summarized" requirement true
while removing the bytes from main. Rationale: the file-plus-selector route is
what `.agents/references/subagent-reporting.md` `## Handoffs` already prescribes
for oversized material, and the fields kept inline are the ones main itself
decides on.

Alternatives considered and not recommended: dropping the envelope entirely on
success, which leaves the landing acceptance table with no authoritative build
evidence; and shrinking the envelope schema in the build script, which changes a
typed contract other consumers read for a problem that is purely about what
travels into main's context.

If root-causing shows the fix lies outside the boundary below — for example that
the build script must emit the envelope file itself — surface it for re-planning
instead of expanding scope.

## Critical files
- `.agents/skills/compile/SKILL.md` — `## Handoff`, the envelope bullet
  (currently `:61-63`) and the per-build reporting bullets that follow it; the
  primary authorized fix boundary
- `.agents/references/subagent-reporting.md` — the cap-exemption sentence
  (currently `:117-120`) naming this envelope, which must stay consistent with
  whatever `## Handoff` ends up requiring
- `.agents/skills/finalize-changes/references/worker.md` — the typed-receipt
  sentence (currently `:76-77`), the downstream consumer whose access must be
  preserved
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md` — the
  build row (currently `:67-72`), read-only unless the chosen fix changes where
  that row's evidence is read from

## In scope
- Root-cause investigation as `## Design` states, including the blocking
  question about the landing gate's consumption of the envelope
- The smallest resulting prose fix, confined to `## Handoff` in
  `.agents/skills/compile/SKILL.md`, distinguishing the clean-success case from
  every other case and naming what the handoff carries in each
- The matching minimal wording change to the cap-exemption sentence in
  `.agents/references/subagent-reporting.md`, only if the `## Handoff` change
  makes the current sentence false
- The matching minimal wording change to the typed-receipt sentence in
  `.agents/skills/finalize-changes/references/worker.md`, only where it must
  name where the finalizer reads the envelope from

## Out of scope
- `.agents/skills/compile/scripts/**` and the `broken-engine-build-result/v1`
  schema itself, including which fields it carries and its truncation behavior
- `.agents/skills/compile/references/**`, including the worker's execution and
  result discipline, data-mode rules, and PREfast mode
- `## When to use`, `## Inputs`, the invocation section, and the frontmatter of
  `.agents/skills/compile/SKILL.md`
- The shared handoff form, its row set, and its 40-line/20,000-character caps in
  `.agents/references/subagent-reporting.md`
- Every other row of the landing acceptance table and the rest of the
  `/finalize-changes` landing flow, including the confirmation contract
- Adding any script; the landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 3 under the root `AGENTS.md` risk tiers; this author's
classification, to be confirmed at the Approve and classify step. The trigger is
that the change spans independently owned surfaces: the `/compile` build
reporting contract, the shared handoff reference, and the `/finalize-changes`
landing gate whose build evidence row depends on it. It touches no
determinism/CRC, wire/protocol, serialization, replay, or threading surface, and
no C++.

Invariants to preserve:
- Every non-success build — any `status` other than `success`, any
  `severity: error` diagnostic, `complete: false`, or a schema/result/exit
  mismatch — still returns its full envelope verbatim inline, unshortened
- The landing acceptance table's build row still resolves from an authoritative
  `broken-engine-build-result/v1` envelope, read verbatim and never summarized
- Every reported field still comes from the envelope, never from scraped
  terminal text
- `retainedLog.path` and the game data-mode, generation-authority, and Gaea
  fields stay reportable for every build
- `SKILL.md` remains the public file, delegating mechanics to its references
- The changed files keep their existing encoding and line endings; no transcript
  path or home path enters the repository

## Acceptance criteria
- A `/compile` handoff for a run whose builds are all `status: success` with
  empty diagnostics carries no verbatim envelope bytes, and its total size is a
  small fraction of the 19,653 characters recorded in `## Context`
- A `/compile` handoff for a run with any failing or incomplete build still
  carries that build's envelope verbatim
- The landing acceptance table's build row can still be filled from an
  authoritative envelope in both cases, using only what the fixed skills
  document
- The diff touches only the files named in `## In scope`
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files where the root `AGENTS.md` Apply the triggered cleanup step triggers them
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`

## Coordination
No mandatory constraint binds this Plan to another live Plan.
`Documents/Plans/Engine/PlanSimplicityHandoffSingleCopy.md` and
`Documents/Plans/Engine/NextPlanSnapshotScopeHeadingGate.md` also concern handoff
size, but both place `.agents/references/subagent-reporting.md` explicitly out of
scope and change only their own skills' `## Handoff` sections, so their regions
are disjoint from this one. Whichever of the three lands later re-locates its
regions by content rather than by the line numbers recorded here.
