<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T16:24:53.386Z","dependsOn":[]} -->
# Fix: repo-code-review — `## Inputs` places `status` in the saved targets file, which carries no such field

## Context
`.agents/skills/repo-code-review/SKILL.md` `## Inputs` (line 58) follows the
saved-run instruction with "On `status` `pass` (exit 0) stdout carries only the
targets bytes; `blocked` (exit 2) or `error` (exit 1) leaves stdout empty and
reports the envelope on stderr". Read in sequence with the preceding sentence,
which tells the manager to save that stdout to a file, the wording reads as if
the saved file carries a `status`. It does not:
`.agents/scripts/Get-SessionChangeInventory.ps1` `Write-SessionTargets`
(lines 668-698) emits a document holding exactly `schemaVersion` and `paths`,
and `Complete-SessionChangeInventory` (lines 98-103) shows that in `-EmitTargets`
mode the `status`/`code`/`message` envelope goes to stderr only on a non-zero
exit, so `status` exists solely as the script's exit code plus that stderr
envelope.

Observed symptom in this session: after saving the targets file, main checked it
in the documented form and the probe returned `{"status":null,"truncated":null}`,
because those keys are absent from the document. Main then spent a second shell
call reading `schemaVersion` and the `paths` count to confirm the saved file was
in fact valid, so the verification of one saved file cost two calls and an
apparent failure that was not one.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: e99c34ab-c9ee-4261-a00a-9f9db0dfccc2
- Worktree/branch UUID: e5b6e0e4-e0c9-4bf8-ad8e-e9d0fa455e78
- Session branch: claude/e5b6e0e4-e0c9-4bf8-ad8e-e9d0fa455e78
- Worktree: .claude\worktrees\BrokenEngine\e5b6e0e4-e0c9-4bf8-ad8e-e9d0fa455e78
- Landing ref: the session branch above. The recording session had not landed its
  change when this Plan was written, so the branch tip named here contains this
  Plan only once that session's work is committed to it; until then the Plan
  exists as tracked content in the worktree above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the cited lines should be sufficient, so the transcript is not expected to be
needed. Only when it genuinely is, in a new session run
`/next-plan-review <landing ref above>` in bounded friction mode, supplying
client `claude` and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is a documentation-only fix that separates the two
surfaces in that sentence: say that `status` is the script's own run outcome —
its exit code and, on a non-zero exit, the structured envelope on stderr — read
before or as the redirection completes, and that the saved file itself carries
only `schemaVersion` and `paths`, so a saved-file check confirms those two keys
rather than a `status` or `truncated` field. Whether the fix names the two keys
literally or points at the script's emitted document is a wording choice for the
fix session.

## Critical files
- `.agents/skills/repo-code-review/SKILL.md` — `## Inputs`, the sentence at
  line 58

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Inputs` paragraph of
  `.agents/skills/repo-code-review/SKILL.md` named above

## Out of scope
- The landed change the session produced
- `.agents/scripts/Get-SessionChangeInventory.ps1` and its result shape, which
  this Plan documents rather than changes
- The save form and save-path convention for the targets file, owned by the two
  Plans named under `## Coordination`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Coordination
- `Documents/Plans/ChangeWorkflow/CodeReviewTargetsFileSaveForm.md` and
  `Documents/Plans/ChangeWorkflow/RepoCodeReviewTargetsFileSaveForm.md` rewrite
  the same `## Inputs` paragraph to state how the targets file is saved and where
  it goes. Whichever of the three runs later re-reads that paragraph as it then
  stands and keeps the other Plans' wording intact; none of them may drop this
  Plan's separation of the run's `status` from the saved document's keys.

## Risk tier and invariants
Tier 1 (mechanical: skill documentation prose, no public signature or invariant
exposure); escalate only if the fix reaches script behavior. Never embed
transcript paths or home paths.

## Acceptance criteria
- `## Inputs` distinguishes the run outcome `status` from the saved document's
  contents, so a manager checking the saved file checks keys the file has
- The recorded symptom no longer reproduces under the documented invocation
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
