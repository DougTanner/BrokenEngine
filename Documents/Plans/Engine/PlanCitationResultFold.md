<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T22:39:36.168Z","dependsOn":[]} -->
# Fix: Test-PlanCitations.ps1 — full per-citation array floods main context when only four summary values are read

## Context
During a `/next-plan` run the main session ran the documented invocation

```powershell
pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Test-PlanCitations.ps1 Temp/next-plan/ValidateSkillAccurateChecksBound.resolved.md
```

The single `broken-engine-plan-citations/v1` object it returned measured about
14,339 characters: a complete `citations.items[]` array of 45 records, each
carrying `citation`, `path`, `pathExists`, `lineExists`, and an `excerpt` line
of source text. Main's decision used exactly four values from that object —
`headings.inScopePresent`, `headings.outOfScopePresent`, `card.present`, and
`card.missingFields` — and the preparation `implementer` and the `/plan-audit`
reviewer had already each reported those same values as one `Decisive checks`
handoff row. The entire per-citation array therefore entered the main context
without being read.

The emitting code in the current tree confirms the shape:
`Test-PlanCitations.ps1:296-300` builds `citations` as `items` plus
`totalCount`/`omittedCount`, `:287-291` adds one output record per matched
citation, and `:257-268` attaches a capped `excerpt` string to each record. The
success path at `:365` returns the whole object unconditionally, with no
caller-selectable projection.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 48395902-6e97-4935-b2f0-8260d4caab4b
- Worktree/branch UUID: aa97c3c8-4755-4e7c-a524-272c5f68c6a1
- Session branch: claude/aa97c3c8-4755-4e7c-a524-272c5f68c6a1
- Worktree: .claude\worktrees\BrokenEngine\aa97c3c8-4755-4e7c-a524-272c5f68c6a1
- Landing ref: claude/aa97c3c8-4755-4e7c-a524-272c5f68c6a1
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/aa97c3c8-4755-4e7c-a524-272c5f68c6a1` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation is to fold the returned `citations` payload the way
`.agents/skills/next-plan/scripts/Get-NextPlanList.ps1:10-26` already folds the
Plan listing — keep counts, drop the rows nobody reads. Concretely: keep
`citations.totalCount` and `citations.omittedCount`, add the counts a reader
needs (how many records had `pathExists` false and how many had `lineExists`
false), and return in `items[]` only the records whose `pathExists` or
`lineExists` is false, since those are the only rows `/plan-audit`'s lead
investigation acts on. Records that resolved cleanly are represented by counts
alone. The implementing session should confirm whether the per-record `excerpt`
is still worth carrying on the surviving unresolved rows, where by definition
the cited line does not exist, and state its choice.

Whatever projection is chosen, every value the current consumers read must
survive unchanged, because each is the documented decision input of a step:
- `headings.inScopePresent` / `headings.outOfScopePresent` —
  `.agents/skills/prepare-change/references/worker.md:36-38` and
  `.agents/skills/next-plan/references/worker.md:51-54`
- `card.present` / `card.missingFields` —
  `.agents/skills/prepare-change/references/worker.md:36-38`
- `truncated` / `citations.omittedCount` —
  `.agents/skills/plan-audit/references/worker.md:21-25`
- the unresolved-citation leads `/plan-audit` investigates —
  `.agents/skills/plan-audit/references/worker.md:26-29`

If the fold changes what a consumer must read, the matching consumption wording
in those files is updated in the same change. If root-causing shows the fix lies
outside the boundary below, surface it for re-planning instead of expanding
scope.

## Critical files
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` — the emitter
  (`Convert-ToOutputCitation` at `:257-268`, `Set-CitationPayload` at
  `:287-301`, the `:365` success exit)
- `.agents/skills/plan-audit/references/worker.md` — documented consumption
  (`:10-29`)
- `.agents/skills/plan-audit/SKILL.md` — `## Inputs`, which states what the
  citation check reads from the supplied file
- `.agents/skills/next-plan/references/worker.md` — the snapshot check that
  reports the two heading values (`:51-54`)
- `.agents/skills/prepare-change/references/worker.md` — the drafting check that
  reads the heading and card values (`:32-38`)
- `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1` — read-only reference
  for the existing fold shape; not modified

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the returned-payload assembly in
  `Test-PlanCitations.ps1` (`Convert-ToOutputCitation` and the `citations`
  payload built in `Set-CitationPayload`) and to the matching consumption
  wording in the `plan-audit`, `next-plan`, and `prepare-change` files named
  above

## Out of scope
- The citation grammar and which tokens become records
- The heading and execution-card detection, and every value those two result
  sections carry
- The caps (`MaximumCitations`, `MaximumOutputBytes`, excerpt length) and the
  shed ordering
- The success-exit `status`/`code` selection, which
  `Documents/Plans/Engine/PlanCitationTruncatedStatus.md` owns
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Coordination
`Documents/Plans/Engine/PlanCitationTruncatedStatus.md` changes the
success-exit `status`/`code` selection of this same script and reads
`$script:Result.truncated`, which the shed loop sets from the emitted record
count. Neither Plan requires the other first. Whichever lands second must
re-read the other's landed change and keep it working: this Plan must not remove
or bypass the `truncated` signal that Plan's status selection depends on, and
that Plan must not reintroduce the full unfiltered `items[]` array.

## Risk tier and invariants
Expected Change Workflow Tier 2, on the root `AGENTS.md` trigger "one
subsystem's runtime or tool behavior" — the emitted result shape of one
review-support script at an existing boundary; escalate if the fix reaches
build/bootstrap coordination. Invariants to preserve: the schema name
`broken-engine-plan-citations/v1`; the existing exit codes and their meanings;
the script stays read-only and writes its whole result to stdout; the
bundled-script rule's canonical `pwsh -NoProfile -File <repo-relative path>`
invocation from the worktree root. Never embed transcript paths or home paths.

## Acceptance criteria
- A plan file with dozens of fully resolving citations returns a result whose
  size is a small multiple of its counts rather than of its citation count, and
  the run that produced the recorded ~14,339-character result returns
  substantially less
- Every unresolved citation (`pathExists` or `lineExists` false) is still
  individually identifiable in the result
- `headings.inScopePresent`, `headings.outOfScopePresent`, `card.present`,
  `card.missingFields`, `truncated`, and the omitted-record count are unchanged
  in name and meaning
- The `plan-audit`, `next-plan`, and `prepare-change` consumption wording matches
  the emitted shape
- `/validate-skill` passes for any changed `SKILL.md`; plan validate exits 0
