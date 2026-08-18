<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T12:05:36.683Z","dependsOn":[]} -->
# Fix: /repo-code-review metrics protocol — the host-side Compare digest is computed twice per C++ review round

## Context

Observed symptom, from a `/next-plan-review` of an earlier landed session: the
manager ran byte-identical Compare invocations twice in a row, with no edits
between them:

`pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode Compare -Targets Temp/repo-code-review-prompt.md.targets.json -Baseline 30bd2f38... -Digest`

at `19:23:06`→`19:25:39` (153 s, roughly 19.7 KB of JSON into manager context)
and again at `19:25:51`→`19:28:04` (133 s). The first run's result had been
consumed as console text, so the digest had to be regenerated to append it to the
review scope file. Cost: about two minutes of active time and about 20 KB of
manager context per C++ review round, and the C++ review launched roughly 5.4
minutes after its sibling reviews instead of alongside them.

Current behavior, read from the working tree:

- `.agents/skills/repo-code-review/references/metrics-protocol.md:9-19` directs
  the manager to run Compare host-side and put the verbatim
  `broken-engine-code-quality-evidence/v2` digest in the scope file, but names no
  step that captures the digest bytes.
- `.agents/skills/code-quality-metrics/references/MetricContract.md:76-78`: the
  digest exists only on stdout — `-OutputPath` always receives the full report,
  never the digest — so the invocation's own stdout is the sole copy.
- `.agents/skills/codex-review/SKILL.md:59-61` says to add that digest to the
  scope file without stating how the bytes travel there.
- The root `AGENTS.md` bundled-scripts rule permits consuming a single
  invocation's own result inside that same shell call, but shell state does not
  persist between tool calls, so a later call cannot reuse a variable from the
  Compare call. Nothing in the documented flow closes that gap, which is why the
  digest was regenerated.

The analysis is deterministic on identical inputs, so the second run bought no
signal.

The misbehaving surfaces are `/repo-code-review`'s metrics protocol and
`/codex-review`'s scope-assembly prose, both outside the `## In scope` boundary
of the Plan the observing session had claimed, so this is tooling friction rather
than an in-scope acceptance failure of the change that session landed.

Verify every cited line number against the working tree before editing — the
numbers above may have moved since.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a ref
whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1b583079-2807-42e9-930f-2394560b2edc
- Worktree/branch UUID: 9210ba0b-3bb1-4251-80be-0a74eef865cc
- Session branch: claude/9210ba0b-3bb1-4251-80be-0a74eef865cc
- Worktree: .claude\worktrees\BrokenEngine\9210ba0b-3bb1-4251-80be-0a74eef865cc
- Observed in an earlier session: the six fields above are the observing
  session's, not the recording session's. That session's landed commit is
  `44b1259d50eaf4582bae5e25d5e35386abc3619b` ("Accept landing-candidate sessions
  in plan list and rename quarantined to excluded"), a single-session landing
  commit whose tree contains that session's work.
- Landing ref: `claude/a9c4f4cc-6cd6-4bf1-8511-9702c6308d1f`, the recording
  session's branch, whose tip is that session's final commit. Fallback once that
  ref is gone: `git log --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate commit,
  so review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.

## Design

The `/next-plan-review` that produced this residual has already run and proved
the symptom above, so no further transcript review is required before
implementing. The provenance block is retained only so an implementer who needs
more context can reach the observing session while it is still available.

Documentation only. Make the documented flow "run Compare once, capture its own
stdout in that same shell call, append that file to the scope evidence":

1. In `.agents/skills/repo-code-review/references/metrics-protocol.md`
   `## Running Compare`, state at the host-side paragraph that the single
   Compare invocation's own stdout is captured to a file in that same shell call
   — the result-consuming form the root `AGENTS.md` bundled-scripts rule permits
   — and that Compare is never re-run for the same targets file, baseline, and
   head. A second run of identical inputs is a defect, not evidence.
2. In `.agents/skills/codex-review/SKILL.md` step 1, make the sentence that adds
   the digest to the scope file say that it comes from that captured file rather
   than from a fresh run.

Deliberately rejected alternative: changing `Invoke-CodeQualityMetrics.ps1` or
`-OutputPath` to persist the digest to a deterministic path. The digest already
reaches the scope file from one run's stdout, so a script change would add an
output surface that two prose sentences close completely.

If root-causing shows the root `AGENTS.md` list of permitted result-consuming
forms must itself name file capture explicitly, surface that for re-planning
instead of editing root `AGENTS.md` under this Plan.

## Critical files

- `.agents/skills/repo-code-review/references/metrics-protocol.md` — the
  `## Running Compare` host-side paragraph (`:9-19`) and the direct-run paragraph
  (`:21-26`)
- `.agents/skills/codex-review/SKILL.md` — the sentence adding the digest to the
  scope file (`:59-61`)
- `.agents/skills/code-quality-metrics/references/MetricContract.md` — the
  `-Digest` and `-OutputPath` contract (`:76-78`), read to keep the prose true;
  never edited by this Plan

## In scope

- The `## Running Compare` regions of
  `.agents/skills/repo-code-review/references/metrics-protocol.md`: single-run
  requirement plus the same-call stdout capture
- The digest-to-scope-file sentence in `.agents/skills/codex-review/SKILL.md`
  (`:59-61`)

## Out of scope

- `Invoke-CodeQualityMetrics.ps1` and every other script: no change to the
  digest schema, `-Digest`, `-OutputPath`, targets handling, or runtime
- The root `AGENTS.md` bundled-scripts rule
- The `-PreflightTargets` / `-ReuseTargets` sequence, prompt assembly, and every
  other `/codex-review` step
- Compare's runtime and call-timeout guidance, its failure contracts, and the
  advisory status of metrics
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior, documentation only); escalate if the fix
reaches a script, the digest schema, or build/bootstrap coordination. The
evidence contract must stay fail-closed: the reviewer still validates the digest
against the reviewed change set's baseline, head, and targets identities, an
absent or mismatched digest remains a blocker, and no captured file may be
edited, summarized, or reconstructed.

## Acceptance criteria

- A manager following `/repo-code-review` and `/codex-review` runs Compare
  exactly once for a given targets file, baseline, and head in a review round,
  and the verbatim digest reaches the scope file from that one run
- The documented flow names the permitted same-call capture form, so no second
  run is needed to move the bytes
- `/validate-skill` passes for the changed skill packages; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the (skill/script, symptom) pair: the host-side
`code-quality-metrics` Compare digest for `/repo-code-review` is regenerated
because the documented flow never captures it. A later observation of the same
pair is a duplicate, not a new residual.
