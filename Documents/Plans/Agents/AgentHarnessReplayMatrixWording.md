<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T20:50:29.373Z","dependsOn":[]} -->
# Fix: /agent-harness AgentHarness.md — replay transfer-matrix expectations read as per-snapshot field equality

## Context

Two independent harness workers in one session each had to re-derive the
intended meaning of the replay transfer-capture matrix from engine source
before they could evaluate the documented expectation.

Symptom 1 — matrix expectation phrasing.
`Projects/BrokenEngineSandbox/Documents/AgentHarness.md:111` states
`recordingEventTick == playbackEventTick == E`, and its case D at
`AgentHarness.md:120` states, for a four-event recording, "For each event,
require its matching count to be `1`, the other counts zero, and
`playbackEventTick == recordingEventTick`". Read literally, that is a single
`replay_transfer_capture` snapshot in which two fields are equal. At runtime the
snapshot cannot satisfy that reading for a multi-event recording:
`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:321-337` resets and
relatches `iRecordingEventTick` and the per-type counts on every newly recorded
event tick, so after recording they hold only the LAST event, while
`GameSaveLoad.cpp:1131` overwrites `iPlaybackEventTick` on each replayed
post-dispatch record as playback walks the recording. The criterion is therefore
only satisfiable as set equality: the polled sequence of observed
`playbackEventTick` values against the recorded event ticks observed during
recording. Both workers (the Stage-2 and Stage-3 matrix runs) read
`GameSaveLoad.cpp` to establish that before they could run the matrix. The doc
already describes the latching at `AgentHarness.md:245` ("During playback,
`playbackEventTick` is latched from the exact tick whose post-dispatch record the
reader loaded and applied"), but the matrix section never references it.

Symptom 2 — case E preamble.
`AgentHarness.md:121` says "Retire `[1,0]` after D by reading its player `uuid`,
sending `inject_status_changes ...`, and waiting for it to leave
`status.activeCoords`. Stop, poll `recording:false`, then issue `replay_play`."
That presumes recording is still active when E begins, but case D
(`AgentHarness.md:120`) ends in active playback with a completed replay loop, and
`AgentHarness.md:255` rejects injection during replay. The Stage-3 worker had to
work out and perform an undocumented step — cancel D's playback and start a fresh
recording over the still-live `[1,0]` — before E's own instructions could be
followed, then discovered the "Stop, poll `recording:false`" step only makes
sense against that fresh recording.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: a04c0506-dc39-4640-bdac-56f33ec4a75d
- Worktree/branch UUID: 37254641-80b9-4058-89da-31bb60f54f2f
- Session branch: claude/37254641-80b9-4058-89da-31bb60f54f2f
- Worktree: .claude\worktrees\BrokenEngine\37254641-80b9-4058-89da-31bb60f54f2f
- Landing ref: claude/37254641-80b9-4058-89da-31bb60f54f2f
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/AgentHarnessReplayMatrixWording.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review claude/37254641-80b9-4058-89da-31bb60f54f2f`,
supplying client `claude` and the recorded conversation session ID above.
Root-cause the friction from the proven transcript — specifically, what the two
harness workers actually had to derive from `GameSaveLoad.cpp` and what step they
had to invent between cases D and E — then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary — for example that the `replay_transfer_capture` result shape, not its
documentation, is the real defect — surface it for re-planning instead of
expanding scope.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the
  `### Replay transfer-capture fixture` section: the matrix expectation sentence
  near line 111, case D near line 120, and case E near line 121. This file is the
  authorized fix boundary.

## In scope

- Root-cause investigation via /next-plan-review, run with client `claude`, the
  review ref named in `## Design`, and the recorded conversation session ID.
- The smallest resulting fix, confined to
  `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` and, within it, to the
  matrix expectation phrasing (near line 111 and case D near line 120) and the
  case E preamble (near line 121): state how the recording-side and playback-side
  capture fields latch across a multi-event recording so the expectation is
  evaluable as written, referencing the latching already described in the command
  reference for `replay_transfer_capture`, and state explicitly the step case E
  requires between D's active playback and E's own instructions.

## Out of scope

- The landed change the observing session produced.
- Any change to `replay_transfer_capture`, `replay_transfer_fixture`, replay
  recording/playback behavior, capture result fields, or any other engine, game,
  or harness runtime code.
- Any other section of `AgentHarness.md`, the `/agent-harness` skill package, its
  bundled scripts and references, and any unrelated document.
- Any transcript path or transcript text in the repository.

## Risk tier and invariants

Expected Tier 1 (documentation-only reword of an existing verification procedure,
with no public signature or invariant exposure). The trigger that would escalate
it is root-causing concluding the runtime capture semantics or a skill's
documented behavior must change instead of its wording, which makes it Tier 2
scoped tool behavior and takes it outside this Plan's boundary; escalate further
if any fix reaches build/bootstrap coordination.

Invariants the fix must preserve:

- The documented acceptance bar for the A–G matrix does not weaken: the reword
  makes the existing expectation evaluable, and does not remove a required
  observation, log line, or rejection case.
- No transcript path and no home-directory path enters the repository.
- The reworded matrix text stays consistent with the `replay_transfer_capture`
  command-reference entry in the same document; the two must not state different
  latching semantics.

## Acceptance criteria

- A reader following only `AgentHarness.md` can evaluate the case D multi-event
  expectation without opening `GameSaveLoad.cpp`, and can run case E without
  inventing an undocumented step between D's active playback and E's first
  instruction.
- The document's matrix section and its `replay_transfer_capture` command entry
  agree on how the recording-side and playback-side fields latch.
- `WorktreeCli plan validate` exits `0` with `status: valid`; `/validate-skill`
  passes for any changed `SKILL.md` if the fix reaches one (none expected).

## Notes

This file is a tracked executable Plan created by `New-PlanFile`; its byte-zero
`broken-engine-plan/v1` marker records an empty `dependsOn` list. Root cause is
deliberately deferred to the `/next-plan-review` session named in `## Design`;
the citations above are the observed in-session symptom, not a completed
diagnosis.
