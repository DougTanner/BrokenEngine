<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T23:00:04.851Z","dependsOn":[]} -->
# Fix: Test-PlanCitations.ps1 — reports `status: pass` while `truncated: true` hid two citations

## Context
During a `/plan-audit` the auditor ran the documented invocation

```powershell
pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Test-PlanCitations.ps1 Temp/CompileTargetRecommendation-plan-snapshot.md
```

and the one `broken-engine-plan-citations/v1` object it returned carried
`status: pass` and `code: ok` together with `truncated: true` and
`omittedCount: 2`. Two citations were therefore absent from the result while the
top-level verdict said the lookup succeeded. The auditor had to resolve the two
omitted citations by hand; a caller that trusts the `pass`/`ok` pair alone ships
an audit whose citation coverage was silently incomplete.

The emitting code was re-read in the current tree and confirms the shape:
`Test-PlanCitations.ps1:230` sets `truncated` whenever records were shed or a
text field was capped, the shed loop at `:220-236` drops records to fit
`MaximumOutputBytes`, and the success path at `:288` calls
`Complete-PlanCitations 0 'pass' 'ok' 'Plan citation lookup completed.'`
unconditionally — the status and code never observe `truncated`.
`.agents/skills/plan-audit/references/worker.md:21-25` does tell the auditor to
read `truncated` and `omittedCount`, so the defect is that the machine-readable
verdict contradicts that instruction rather than that the data is missing.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 62f98e1f-3ac2-49b9-8c3a-08d631bb56db
- Worktree/branch UUID: ec8cd668-9c77-4241-bf54-16269ae03fa8
- Session branch: claude/ec8cd668-9c77-4241-bf54-16269ae03fa8
- Worktree: .claude\worktrees\BrokenEngine\ec8cd668-9c77-4241-bf54-16269ae03fa8
- Landing ref: claude/ec8cd668-9c77-4241-bf54-16269ae03fa8
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/ec8cd668-9c77-4241-bf54-16269ae03fa8` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation is to stop emitting a bare `pass` for a truncated
result: keep exit `0` and the existing schema name and fields, and have the
success path at `:288` choose its `status`/`code` from `$script:Result.truncated`
so a truncated lookup is distinguishable without reading the nested counts —
for example a distinct code such as `ok.truncated` alongside the existing `ok`,
or a distinct status. The implementing session should pick one and state which.
Whichever is chosen, `/plan-audit`'s documented consumption of the result is
updated in the same change so the auditor is told what the new value means.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` — the emitter
  (`:220-236` shed loop and `truncated`, `:288` success exit)
- `.agents/skills/plan-audit/references/worker.md` — the documented consumption
  (`:17-25`)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the success-exit status/code selection
  in `Test-PlanCitations.ps1` and the matching consumption wording in
  `plan-audit/references/worker.md`

## Out of scope
- The citation grammar at `Test-PlanCitations.ps1:14` and which tokens it
  matches
- The caps themselves (`MaximumCitations`, `MaximumOutputBytes`) and the shed
  ordering
- Any audit judgment rule: the script still renders no verdict on a plan
- Moving the script, which `Documents/Plans/Engine/ExecutionCardFile.md` owns
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one tool at an existing boundary — the
emitted status/code of a review-support script); escalate if the fix reaches
build/bootstrap coordination. Invariants to preserve: the schema name
`broken-engine-plan-citations/v1` and every existing field; exit `0` for a
completed lookup, so no caller starts treating truncation as a failed run; the
script stays read-only and writes its whole result to stdout; the bundled-script
rule's canonical `pwsh -NoProfile -File <repo-relative path>` invocation from the
worktree root. Never embed transcript paths or home paths.

## Acceptance criteria
- A plan file whose citations exceed the caps returns a status/code pair that a
  caller can distinguish from a complete lookup, with `truncated: true` and the
  existing counts unchanged
- A plan file within the caps returns the unchanged `pass`/`ok` pair
- `plan-audit/references/worker.md` states what the new value means for the
  auditor
- `/validate-skill` passes for any changed `SKILL.md`; plan validate exits 0

## Notes
`dependsOn` names `Documents/Plans/Engine/ExecutionCardFile.md` because that Plan
moves this script to `.agents/scripts/` and rewrites every reference to its path;
doing this fix afterwards keeps one script location in play instead of two.

Lead for the fix session, not a scope item: the citation grammar at
`Test-PlanCitations.ps1:14` matches `h|cpp|md|ps1|py|txt|glsl` only, so a
`.vcxproj` token is never a citation record at all and cannot be one of the two
records `omittedCount` reported. Confirm what the two omitted records actually
were before assuming the observed pair; the emitted-verdict defect above holds
either way.
