<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T17:49:25.388Z","dependsOn":[]} -->
# Fix: codex-review — a review that needs more than the host's 10-minute command cap has no completion path

## Context
During the `/next-plan` landing gate for the OwnedEntityRegistryToEngine Plan,
the `/verify-changes` reviewer was dispatched exactly as `/codex-review`
documents it (`.agents/skills/codex-review/SKILL.md:95-98`): run
`pwsh -NoProfile -File .codex/codex-review.ps1 -Worktree <worktree>
-PromptFile 'Temp/owned-entity-verify-prompt.md'
-OutFile 'Temp/owned-entity-verify-out.md'` with the maximum tool timeout (10
minutes), in background for a Tier-3 diff. The reviewer prompt was 62,758 bytes
over a 19-file Tier-3 diff.

Attempts 1 and 2 of that identical command both ended with host status "killed"
at the 10-minute cap, and `Temp/owned-entity-verify-out.md` did not exist after
either — `.codex/codex-review.ps1:59-62` copies the Codex output to `-OutFile`
only on exit 0, so a killed run leaves no partial result and no diagnosis. Each
attempt burned about ten minutes. Attempt 3, byte-identical to the first two,
exited 0 and wrote the PASS result. The only thing that unblocked the landing
gate was explicit user direction to retry blindly.

The skill's own advice — "For a large diff or Tier-3 scope, run in background
and wait for completion rather than truncating"
(`.agents/skills/codex-review/SKILL.md:97-98`) — cannot exceed the host's hard
per-command cap, so a review that legitimately runs longer than 10 minutes has
no documented completion path. The documented fallback
(`.agents/skills/codex-review/SKILL.md:116-119`: `CODEX-UNAVAILABLE` and a
user-authorized Opus substitute) treats a duration problem as an availability
problem, which discards a review that was in fact progressing normally.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 453f8b44-3b01-43d5-940c-c8364bdfb854
- Worktree/branch UUID: cb6e15ef-fb81-4c19-957d-fd0ecbfeb5a5
- Session branch: claude/cb6e15ef-fb81-4c19-957d-fd0ecbfeb5a5
- Worktree: .claude\worktrees\BrokenEngine\cb6e15ef-fb81-4c19-957d-fd0ecbfeb5a5
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design
In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and conversation session ID; use the proven transcript as evidence for
the change below, which is already decided and is the whole of the fix.

`.codex/codex-review.ps1` stops waiting on Codex. Its existing invocation
(`-Worktree`, `-PromptFile`, `-OutFile`, unchanged parameters and meanings)
becomes self-backgrounding: it performs the same argument, `OPENAI_API_KEY`,
and `codex` lookup checks it performs today, launches the same Codex command
detached, and returns immediately — well under the host cap — writing to stdout
a single-line JSON receipt naming a run identifier and the eventual `-OutFile`
path. The receipt is the launch result; no single call ever waits for Codex to
finish.

Completion is observed by re-invoking the same script with a new documented
`-Poll <run identifier>` parameter, which returns immediately with a
single-line JSON status of `running`, `completed`, or `failed`. `completed`
guarantees the `-OutFile` bytes are fully written and readable at that moment
(the detached run writes to a temporary file and moves it into place only after
Codex exits 0, so a reader never sees a partial file). `failed` carries the
detached run's non-zero exit code and short reason. The script's own exit code
stays meaningful: `0` for a successful launch or poll, non-zero only for the
existing local failures (`126` inherited `OPENAI_API_KEY`, `127` missing
`codex` CLI, and script errors), never for a Codex run that is merely slow.

`.agents/skills/codex-review/SKILL.md` replaces its run step's
"maximum tool timeout / run in background" guidance with this two-call
contract: invoke the script to launch, then re-invoke it with
`-Poll <run identifier>` until the status is `completed` or `failed`, each call
bounded far below the host cap, and read `-OutFile` only after `completed`. Its
`## Fallback` section keeps `CODEX-UNAVAILABLE` reserved for genuine failure —
a `failed` poll status, a non-zero script exit, or malformed output — and
states that duration alone is never a `CODEX-UNAVAILABLE` cause.

Nothing else changes: not what Codex is asked to review, not prompt assembly,
not the model, effort, sandbox, or auth handling. If implementation shows the
fix cannot be confined to the boundary below, surface it for re-planning
instead of expanding scope.

## Critical files
- `.codex/codex-review.ps1` — the wrapper that runs Codex synchronously and
  copies the output only on exit 0
- `.agents/skills/codex-review/SKILL.md` — the run step and its 10-minute
  timeout guidance, and the `CODEX-UNAVAILABLE` fallback

## In scope
- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref, client, and conversation session ID
- `.codex/codex-review.ps1`: the detached process launch, the JSON launch
  receipt, the new `-Poll <run identifier>` parameter and its status output,
  and the result-copy handling that makes `-OutFile` appear only complete
- `.agents/skills/codex-review/SKILL.md`: the run step (step 3 and its `<out>`
  read in step 4) and the `## Fallback` section's `CODEX-UNAVAILABLE` meaning

## Out of scope
- The landed OwnedEntityRegistryToEngine change the session produced
- Review prompt assembly (`New-CodexReviewPrompt.ps1`) and the review content of
  any assigned review skill — what a review asks for, how its prompt is
  assembled, and its model, effort, sandbox, or auth selection; this does not
  exclude the `.agents/skills/codex-review/SKILL.md` run-step and
  `CODEX-UNAVAILABLE` fallback edits named in scope above
- The Codex model, effort, sandbox, or auth handling in the wrapper
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination or changes how landing gates are authorized. The
wrapper must keep refusing an inherited `OPENAI_API_KEY` and keep its exit-code
contract meaningful to the skill's fallback. Never embed transcript paths or
home paths in tracked files.

## Acceptance criteria
- A real review dispatched as documented returns its launch receipt in seconds,
  and a run whose Codex work outlasts at least one poll interval still reaches
  a `completed` poll whose `-OutFile` holds the full result
- No single documented invocation waits on Codex, so the killed-at-the-cap
  failure is unreachable: every call — launch and each poll — returns far below
  the 10-minute cap
- Each poll answers running, finished, or failed from its status alone
- `CODEX-UNAVAILABLE` is reported only for a `failed` poll status, a non-zero
  script exit, or malformed output — never for duration
- `/validate-skill` passes for `.agents/skills/codex-review/SKILL.md`;
  `WorktreeCli plan validate` exits 0

## Notes
This Plan is keyed to the pair (`.codex/codex-review.ps1` /
`/codex-review` run step, host 10-minute cap kills the review before any output
file is written). Re-observing that same symptom updates nothing and is a
duplicate. It is distinct from
`Documents/Plans/Agents/CodexReviewMetricsTargetsCircularInput.md`, which is a
prompt-assembly input problem in the `repo-code-review` dispatch.
