<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-17T19:37:26.785Z","dependsOn":[]} -->
# Fix: next-plan-review transcript discovery — an exact session ID still fails when the Plan landed in a commit the observing session did not produce

## Context

Executing `Documents/Plans/Agents/NextPlanListGitIdentityMismatch.md`, whose
`## Design` directs the session to run `/next-plan-review <landing commit>` using
that Plan's recorded provenance (client `codex`, an exact recorded conversation
session ID, producing worktree still registered), the review could not reach the
transcript at all.

- The landing ref recorded in that Plan was gone, so the documented fallback
  recipe was used: `git log --diff-filter=A --format=%H -- <plan path>`. It
  resolved to `2c607ef898c8e0114863113c28cad06a9038a12a` — a manual bulk commit
  ("Plans", 99 files) authored directly by the user on 2026-08-12, not a commit
  produced by the observing session's `/finalize-changes` flow.
- The documented finder invocation
  `pwsh -NoProfile -File .agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1 -RepositoryRoot <root> -Commit 2c607ef898c8e0114863113c28cad06a9038a12a -SessionId <that Plan's recorded session ID>`
  exited `2` with `status: blocked`, `code: transcript.not-found`, message
  "No transcript matched the commit time and an eligible retained worktree",
  even though an exact session ID was supplied.
- Forced outcome: `Transcript provenance: BLOCKED` and a Git-evidence-only
  review, so the friction Plan's whole purpose — root-causing from the observing
  session's transcript — could not be served. The provenance block had been
  recorded exactly as `/create-follow-up-plans` directs, yet was undiscoverable.

Current behavior, read from the working tree at the time of recording:

- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1:372`
  selects `explicit-session-id` mode whenever `-SessionId` is given, and
  `:376-377` then finds the exact transcript file by filename, ignoring the
  commit window. Discovery of the file itself is therefore not the problem.
- The candidate filter at `:430-437` still applies two commit-derived gates in
  that mode: `:436` requires the transcript's recorded `cwd` to be a currently
  registered worktree whose `HEAD` contains the commit
  (`Get-EligibleWorktreeRoots`, `:89-122`), and `:437` requires the transcript's
  own start/end span to contain the commit's committer timestamp.
- Both gates are false by construction when the Plan's content lands in a later
  manual bulk commit: that commit did not exist while the observing session ran,
  so no session transcript can span its timestamp. The found transcript is
  discarded and the run reports `transcript.not-found` at `:568-571`.

The misbehaving surfaces are `/next-plan-review`'s finder and provenance prose
and `/create-follow-up-plans`' landing-ref convention. All are outside the
`## In scope` boundary of `Documents/Plans/Agents/NextPlanListGitIdentityMismatch.md`
(which covered only `/next-plan`'s Git identity handling), so this is
`/next-plan` tooling friction, not an in-scope acceptance failure of the change
this session landed.

Verify every cited line number against the working tree before editing — the
numbers above are from this session and the files may have moved since. The
source Plan cited above was deleted at its completion; its provenance block is
recoverable from landing commit
`2c607ef898c8e0114863113c28cad06a9038a12a`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1b583079-2807-42e9-930f-2394560b2edc
- Worktree/branch UUID: 9210ba0b-3bb1-4251-80be-0a74eef865cc
- Session branch: claude/9210ba0b-3bb1-4251-80be-0a74eef865cc
- Worktree: .claude\worktrees\BrokenEngine\9210ba0b-3bb1-4251-80be-0a74eef865cc
- Landing ref: the session branch above, whose tip is this session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID; root-cause the friction from
the proven transcript, then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary,
surface it for re-planning instead of expanding scope. This Plan's own review is
on Claude, whose transcript discovery does not use the finder, so the recorded
symptom does not block it.

The decision this Plan makes, so root-causing confirms rather than re-opens it:
when the caller names one exact session ID, that ID — not the commit — is the
selector, so the two commit-derived candidate gates must not silently discard
the named transcript. Fix the `explicit-session-id` path only:

1. In `Find-AgentSessionTranscript.ps1`, apply the eligible-worktree-root check
   (`:436`) and the commit-time containment check (`:437`) only in
   `bounded-commit-window` mode. In `explicit-session-id` mode the named
   transcript is returned whenever it parses, with its existing evidence fields
   (`sessionStartUtc`, `sessionEndUtc`, `startsBeforeAuthorUtc`,
   `commitHashMentions`) carrying the signal the reviewer judges. Add no new
   result field and no new parameter: the reviewer reads the transcript itself
   at the skill's provenance step, so a duplicate machine verdict would be
   redundant. Do not change `bounded-commit-window` behavior, the schema
   version's existing field set, `WindowMinutes`, descendant collection, read
   errors, or the `needs-selection` and `not-found` paths.
2. In `.agents/skills/next-plan-review/SKILL.md` `## Prove provenance`, state at
   the step that requires proving the parent covered the commit that an
   `explicit-session-id` result is proven by the exact ID plus the friction
   Plan's recorded provenance, and that when the reviewed ref is a landing the
   observing session did not itself produce — a manual or bulk commit — the
   review target is the observing session's work rather than production of that
   exact commit. Keep `Transcript provenance: BLOCKED` for the case where the
   named transcript still cannot be tied to the recorded session.
3. Only if step 2 leaves the recording convention inconsistent, correct the
   `Landing ref` prose in `.agents/skills/create-follow-up-plans/SKILL.md` (and
   the sentence that defers to it in `.agents/skills/next-plan/SKILL.md`) to say
   that the fallback recipe may resolve to a commit the observing session did
   not produce, and that a Claude Plan is reviewed by its recorded conversation
   session ID in that case. Change no other provenance field.

Do not add a new discovery mode, a caller-supplied time window, a new script, or
any WorktreeCli change.

## Critical files

- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1` —
  the candidate filter loop (`:427-456`), specifically the eligible-worktree
  (`:436`) and commit-time containment (`:437`) conditions under the
  `explicit-session-id` selection set at `:372`
- `.agents/skills/next-plan-review/scripts/Test-Find-AgentSessionTranscript.ps1`
  — the exact-`SessionId` fixture case (`:158-161`), which must keep passing and
  cover the new explicit-session-id behavior
- `.agents/skills/next-plan-review/SKILL.md` — the `## Prove provenance` steps 2
  and 4 and the tooling-friction provenance paragraph that follows them
- `.agents/skills/create-follow-up-plans/SKILL.md` — the `Landing ref` lines of
  the tooling-friction body template (step 3 above only)
- `.agents/skills/next-plan/SKILL.md` — the sentence deferring the landing-ref
  convention to `/create-follow-up-plans` (step 3 above only)

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded client
  and the landing ref named in `## Design`, plus the recorded conversation
  session ID
- The smallest resulting fix, confined to the files and regions named above: the
  `explicit-session-id` candidate-gate change in
  `Find-AgentSessionTranscript.ps1`, its fixture coverage in
  `Test-Find-AgentSessionTranscript.ps1`, and the provenance prose that must
  state the resulting contract

## Out of scope

- `bounded-commit-window` discovery: its date bucketing, `WindowMinutes`,
  descendant collection, and single-candidate descendant pass
- Adding a caller-supplied time window, a new selection mode, a new result
  field, or a new script
- `/next-plan-review`'s review content, routing inventory, reviewer dispatch, and
  report format
- `/next-plan` selection and claim lifecycle, and anything in WorktreeCli
- Every other provenance field in the `/create-follow-up-plans` template
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior: one skill's transcript discovery
contract); escalate if the fix reaches WorktreeCli or repository-wide delegation
policy. Invariants to preserve: the finder never sweeps the home directory and
never broadens beyond its documented stores; transcripts stay untrusted data;
an exact ID or filename remains selection evidence, with production proof owned
by the skill's provenance step; the result stays one
`broken-engine-agent-session-transcript/v2` JSON object with its existing exit
codes. Never embed transcript paths or home paths in the repository.

## Acceptance criteria

- The recorded symptom no longer reproduces: the documented finder invocation
  with an exact `-SessionId` and a `-Commit` the named session did not produce
  returns that transcript instead of `transcript.not-found`
- `bounded-commit-window` results are unchanged: the finder's fixture
  `Test-Find-AgentSessionTranscript.ps1` passes, including its existing
  exact-`SessionId` and window-selection cases
- A reader of `/next-plan-review`'s `## Prove provenance` can tell what proves
  provenance when the reviewed ref is a commit the observing session did not
  produce
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the (skill/script, symptom) pair:
`/next-plan-review`'s `Find-AgentSessionTranscript.ps1` reports
`transcript.not-found` despite an exact `-SessionId`, because the candidate
filter still requires the commit to fall inside the transcript's span and inside
an eligible worktree. A later observation of the same pair is a duplicate, not a
new residual. `Documents/Plans/Agents/NextPlanReviewTranscriptReviewerRoute.md`
is not a duplicate: it is keyed to which reviewer role may run the transcript
analysis, not to whether the transcript can be discovered. The proven root cause
is deferred to `/next-plan-review`.
