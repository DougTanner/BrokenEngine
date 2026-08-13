<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T18:50:29.328Z","dependsOn":[]} -->
# Fix: external-grill-plan — required scheduler `status: valid` never matches the listing's `status: ok`

## Context

`/external-grill-plan`'s Plan Context section requires the preparation brief to carry a passing scheduler
envelope, named literally: "Require that brief to carry the passing envelope — exit `0`, `status: valid`,
`code: ok`" (`.agents/skills/external-grill-plan/SKILL.md:133`), and the following sentence makes the miss
fatal: "Missing or non-passing evidence blocks this skill: stop and return the exact diagnostic to the
manager."

The scheduler listing the preparation step runs does not emit that token. `Get-NextPlanList.ps1` invokes
`WorktreeCli plan list` and writes its stdout through unchanged (`:9-14`, with the comment at `:10`,
"WorktreeCli owns the listing contract; preserve its failure code and emit successful listings unchanged"), so
a successful listing is WorktreeCli's `plan list` envelope verbatim. In this session that envelope was
`{"operation":"list","status":"ok","code":"ok"}` — exit `0`, `code: ok`, empty diagnostics, but `status` is
`ok`, never `valid`. `valid` is the success token of a different operation, `plan validate`.

The grill worker therefore could not satisfy the named signal on a run where nothing was actually wrong. It
had to report the literal mismatch back to the manager rather than either passing or blocking, and the
manager had to adjudicate whether a healthy scheduler counted as passing evidence. One of the two texts is
wrong — either the skill names the wrong operation's success token, or the listing should be validated (or
should report `valid`) — and that is exactly the judgment this Plan defers to root-causing.

Both files are outside the claimed Plan's `## In scope`
(`Documents/Plans/Frame/FrameUtilsSharedHelpers.md`), which authorizes only engine and game frame C++ plus the
frame AGENTS.md sentences naming moved helpers.

Session provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Session: 0d0c7774-565c-4822-bf8c-ff2ca3578181
- Session branch: claude/0d0c7774-565c-4822-bf8c-ff2ca3578181
- Worktree: .claude\worktrees\BrokenEngine\0d0c7774-565c-4822-bf8c-ff2ca3578181
- Landing commit: `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/GrillPlanSchedulerStatusToken.md`
- Run the review before /cleanup-worktrees removes this worktree: Codex transcript discovery requires the
  producing worktree to remain registered, and Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the recorded client and session id,
root-cause the friction from the proven transcript, then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead
of expanding scope.

The symptom fixes the question to answer first: which evidence the grill step is actually supposed to
require — the `plan list` inventory it names in the same paragraph, or a `plan validate` result. Once that is
settled, the fix is either correcting the token the skill names for the operation it means, or requiring the
operation whose success token is `valid`. Do not change the WorktreeCli contract to satisfy the prose.

## Critical files

- `.agents/skills/external-grill-plan/SKILL.md`
- `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance.
- The smallest resulting fix, confined to the files named above — specifically the `## Plan Context` section's
  required-envelope sentence and, only if root-causing proves it necessary, the listing script's emitted
  envelope.

## Out of scope

- The landed change the session produced.
- `Tools/WorktreeCli` and the `plan list` / `plan validate` contracts themselves.
- Any change to scheduler selection, claims, or plan state.
- Unrelated skills and scripts; any transcript path or transcript text in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches build/bootstrap coordination. Never embed
transcript paths or home paths. WorktreeCli remains the only component that parses or changes scheduler state,
and the grill skill must still never run a command that changes it.

## Acceptance criteria

- The recorded symptom no longer reproduces: on a healthy scheduler, the evidence the documented preparation
  step produces satisfies the token `/external-grill-plan` names, with no manager adjudication.
- `/validate-skill` passes for any changed `SKILL.md`; `plan validate` exits 0.
