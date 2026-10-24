<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T14:02:45.238Z","dependsOn":[]} -->
# Find Claude transcripts of landings whose worktree was removed

## Context
`/next-plan-review` proves which session produced a landing through
`.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`. On a
bounded (commit-window) search, the finder accepts a transcript only when its
recorded working directory is a registered, non-prunable worktree whose `HEAD`
contains the commit:

- `Get-EligibleWorktreeRoots` (lines 90-123) builds that set from
  `git worktree list --porcelain -z`.
- Claude discovery lists only the project directories those roots encode
  (`Get-ClaudeProjectDirectory`, lines 148-156; loop at lines 474-476).
- The candidate loop drops any root transcript whose canonical `cwd` is not
  in the set (lines 532-533).
- The not-found message names the rule: "No transcript matched the commit
  time and an eligible retained worktree." (line 674).
- `.agents/skills/next-plan-review/references/worker.md` step 6 (lines 73-81)
  builds its production proof on that rule ("used the eligible retained
  registered worktree selected by the finder").

`/cleanup-worktrees` removes landed session worktrees, so most landings' Claude
transcripts can no longer be found, even though they are still on disk.
Observed on 2026-10-24, during transcript research for the findings record
`Documents/Investigations/ChangeWorkflow/MergingImplementerSteps.md`:

- The finder proved 1 of the 60 most recent Plan-deleting landings on `main`.
  Of the others, 52 were `transcript.not-found`, 4 had a Codex parent,
  2 were `needs-selection`, and 1 had no full landing hash in its transcript.
- 510 of the 541 Claude project directories match no registered worktree
  (45 registered).
- For 4 of 6 not-found landings checked directly (`0078d6c2e`, `a64bff917`,
  `d6eee5dc3`, `5bddaa89e`), top-level Claude transcripts that contain the
  full commit hash and whose record timestamps span the commit time exist.
  All of them sit in project directories that no eligible worktree encodes.
  For the other 2 (`d8ce779b3`, `1e88528bc`), no top-level transcript holding
  the hash spans the commit, so those are genuine absences.

That research was dropped, so nothing in the record waits on this fix. The fix needs a different way to prove that a transcript belongs to a
landing, and that changes the `/next-plan-review` proof contract in both the
script and `worker.md`.

## Design
The author recommends replacing the registered-worktree gate for Claude
transcripts only. Codex keeps its current gate. Rationale: the observed misses
are all Claude. A Claude project directory name is a one-way encoding of the
checkout path, so a transcript can be checked against its own directory
without the worktree still existing.

1. Claude discovery (bounded search, lines 466-476): list the top-level
   `*.jsonl` files of every directory directly under the Claude store root,
   not only the eligible roots' directories. Keep the last-write lower bound.
   Also skip a file created after the commit time, because it cannot hold a
   record from before the commit. Never recurse into the `<sessionId>/`
   sibling directories.
2. `Get-ClaudeTranscriptMetadata` (lines 293-336): check each file for the
   full commit hash in its raw text first, and skip a file with no match
   before parsing any record as JSON. Rationale: the full-store scan reads
   many more files, and step 6 of `worker.md` already requires the transcript
   to record the full hash.
3. Claude candidate gate (lines 531-534): a root Claude transcript is a
   candidate when all of these hold:
   - its canonical recorded `cwd` encodes, under the rule at lines 148-156,
     to exactly the name of the project directory that holds it;
   - its record timestamps span the commit;
   - it mentions the full hash at least once.

   No worktree registration is required. Codex candidates keep the
   eligible-root check at lines 532-533 and 565.
4. Rewrite the not-found message at line 674 to state each client's rule.
   The result fields do not change, so the author recommends keeping
   `schemaVersion` `broken-engine-agent-session-transcript/v2`. The
   preparing session confirms this against the
   format-version rule in root `AGENTS.md` `## Directives`.
5. `worker.md` step 6 (lines 73-81): state the production proof per client.
   - Claude: the finder's match of directory and `cwd`, its time span, and
     its hash mention, plus the reviewer's own proof from the transcript that
     finalization produced the full hash.
   - Codex: the existing retained-registered-worktree proof.
   - Limit the bounded `git worktree list --porcelain` query for a
     `selection.mode: explicit-session-id` result to Codex.

Alternative pre-staged for the preparing session: keep the registration gate
and record each landing's worktree path at finalization so the finder can map
a hash to its directory. The author does not recommend it, because it adds a
persisted record and leaves past landings unfindable.

## Critical files
- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`
- `.agents/skills/next-plan-review/references/worker.md` — step 6
- `.agents/skills/next-plan-review/SKILL.md` (read only; it does not state
  the gate)

## In scope
- `Find-AgentSessionTranscript.ps1`: Claude discovery in the
  bounded-commit-window branch (lines 466-476),
  `Get-ClaudeTranscriptMetadata`, the bounded-search candidate gate (lines
  531-534), which both clients share and which checks no client today, split
  so a Claude record takes the new gate and a Codex record keeps its current
  checks, and the not-found message (line 674)
- `worker.md` step 6, the bullets at lines 73-81

## Out of scope
- Codex discovery, the Codex candidate gate, and the Codex descendant pass
  (lines 558-640)
- The explicit-session-id discovery and its message (lines 436-450, 671)
- OpenCode support
- The Codex retention note in
  `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
  (lines 47-51), which stays true
- Any new script, persisted landing record, or output field
- Any transcript path, transcript text, session ID, or home path in a
  tracked file

## Risk tier and invariants
Tier 2. Trigger: scoped behavior, meaning one skill's tool behavior (the
finder) and its proof rules. It touches no determinism/CRC, wire,
serialization, save/replay, threading, or trust-boundary surface. Invariants:

- The finder still returns store-relative locators and never transcript text.
- It never reads below a project directory's top level.
- A Codex result for the same commit is unchanged.

## Acceptance criteria
This live check needs the 4 transcripts named in `## Context` to still be in
the Claude store on the executing machine. When they are gone, record that as
the reason the check cannot run.

- Running the finder as `worker.md` step 2 documents, on each of
  `0078d6c2e`, `a64bff917`, `d6eee5dc3`, and `5bddaa89e`, returns `pass` or
  `needs-selection` with at least one `claude` candidate.
- On `d8ce779b3` and `1e88528bc`, it still returns `transcript.not-found`.
- Each of those runs finishes within the shell tool's default 120-second
  timeout.
- `worker.md` step 6 states the Claude proof and the Codex proof separately.
  No changed text says a Claude transcript needs a retained registered
  worktree.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes
- Validate the cross-check of `cwd` against the directory encoding on the
  current store. If a session's `cwd` changes partway through, the first
  recorded `cwd` (line 314) may not match the directory. The preparing
  session decides whether to compare the first `cwd` or any recorded `cwd`.
- Never sweep the store's contents beyond what the finder itself reads.
  Verification uses only the finder's parsed output.
