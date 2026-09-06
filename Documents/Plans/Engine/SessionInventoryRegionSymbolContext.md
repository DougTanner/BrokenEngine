<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T01:43:52.290Z","dependsOn":[]} -->
# Fix: Get-SessionChangeInventory.ps1 — a changed region's `symbol` names a line outside that region

## Context
A `/progressive-disclosure-review` reviewer took the changed regions from the
documented read-only inventory invocation

```powershell
pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <absolute repository toplevel> -Baseline ed01d2faf2d28a450b9b5074ea5a56a5cf3b9681 -Regions
```

with the root `AGENTS.md` carrying exactly two changed lines, 93 and 98. The
`regions` row for `startLine 93 endLine 93` reported as its `symbol` the text of
an unrelated earlier paragraph — the line beginning "Order: `/prepare-change`
first at Tier 2+" — and the row for line 98 reported the pre-change text of line
93. The reviewer read `symbol` as the region's identity, concluded from the
mismatch that the line-93 region was not the changed text, and skipped reviewing
it; the region had to be reviewed again by hand afterwards.

The symptom reproduces from the current tree with the same diff the script runs
at `.agents/scripts/Get-SessionChangeInventory.ps1:480`:

```
git diff -U0 ed01d2faf2d28a450b9b5074ea5a56a5cf3b9681 -- AGENTS.md
@@ -93 +93 @@ Order: `/prepare-change` first at Tier 2+, because the alternative investigation
@@ -98 +98 @@ Order: `/plan-audit` and `/plan-simplicity-review` in parallel; then, at Tier 3,
```

The text after the second `@@` is Git's hunk-header context: the nearest
preceding line, on the pre-change side, that matches Git's default function-name
heuristic. In this file that heuristic picks ordinary prose — the two texts above
are pre-change line 86 and pre-change line 93 — and never the region's own
content. `.agents/scripts/Get-SessionChangeInventory.ps1:502` captures that text
as regex group 5, `:507` trims and caps it, and `:519` emits it verbatim as the
row's `symbol`, next to `startLine`/`endLine` values that describe the
post-change side. Nothing in the emitted result tells a caller the two describe
different lines.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 40d5ea23-9635-484d-99d0-a21b2c0f9667
- Worktree/branch UUID: 75e972a1-4851-444c-b0a0-9d5076f1cf48
- Session branch: claude/75e972a1-4851-444c-b0a0-9d5076f1cf48
- Worktree: .claude\worktrees\BrokenEngine\75e972a1-4851-444c-b0a0-9d5076f1cf48
- Landing ref: claude/75e972a1-4851-444c-b0a0-9d5076f1cf48
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/75e972a1-4851-444c-b0a0-9d5076f1cf48` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation is to stop offering a misleading identity for
regions where Git's heuristic cannot produce one: keep the derivation for
regions in a C++ or GLSL file, where a preceding declaration is what the
heuristic is designed to find, and emit `symbol: null` for every other region —
prose, plans, skills, scripts, project files. The class for a path is already
decided by `Get-PathClass` at
`.agents/scripts/Get-SessionChangeInventory.ps1:213`, so the implementing
session should reuse it rather than adding a second extension list; how the
class reaches `Get-RegionTable` is an implementation detail for that session to
pick. The same change should state, in the comment above the region derivation,
that a non-null `symbol` is Git's preceding-context hint from the pre-change
side and is never the region's own text, so a later reader does not restore the
misleading behavior.

An alternative the implementing session may prefer, and should then say so, is to
keep emitting the value for every class but rename the field to a name that
cannot be read as the region's identity. It is a wider change, because every
consumer of the `-Regions` result reads the field by name, so the recommendation
above is the smaller one.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/scripts/Get-SessionChangeInventory.ps1` — the emitter: the hunk-header
  parse at `:501-508`, the emitted row at `:510-520`, and `Get-PathClass` at
  `:213`
- `.agents/skills/progressive-disclosure-review/references/worker.md` — the
  consumer whose reviewer was misled (`:8-11`); in scope only if the chosen fix
  changes what a caller reads

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the region-table derivation and its
  emitted row in `.agents/scripts/Get-SessionChangeInventory.ps1`, plus the
  matching wording in the consumer file named above when the fix changes what a
  caller reads

## Out of scope
- The entry table, counts, landing state, targets, and manifest outputs of the
  same script, and every mode other than `-Regions`
- The region line numbers, `kind`, `addedLines`, `deletedLines`, and the caps
  (`MaximumRegions`, `MaximumSymbolLength`, `MaximumOutputBytes`)
- Adding a Git diff driver, `.gitattributes` entry, or any parser that derives a
  heading or symbol of the tool's own
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one tool at an existing boundary — one field
of one emitted result), by the root `AGENTS.md` Tier-2 trigger for one
subsystem's tool behavior; escalate if the fix reaches build/bootstrap
coordination. Invariants to preserve: the result stays read-only and writes only
to stdout, with diagnostics on stderr; every other emitted field and the row
ordering are unchanged; the script keeps running under a read-only sandbox
without refreshing the Git index; the bundled-script rule's canonical
`pwsh -NoProfile -File <repo-relative path>` invocation from the worktree root.
Never embed transcript paths or home paths.

## Acceptance criteria
- For the reproduction above — the root `AGENTS.md` with two changed lines
  against the recorded baseline — no emitted region carries a `symbol` that
  belongs to a line outside that region
- A region in a C++ or GLSL file still reports its preceding declaration, or the
  chosen fix states plainly why it no longer does
- Region line numbers, `kind`, counts, and every other field are byte-identical
  to the current output for the same inputs
- `/validate-skill` passes wherever the root `AGENTS.md` Apply the triggered
  cleanup step triggers it; plan validate exits 0
