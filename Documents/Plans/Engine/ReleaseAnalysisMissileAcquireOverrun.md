<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T15:14:05.985Z","dependsOn":[]} -->
# Release Builds Fail on the Missile Acquire-Chunk Buffer-Overrun Analysis Warning

## Context

`BrokenEngineSandboxServer` cannot be built in `Release|x64` at all. The
Microsoft `/analyze` pass reports a buffer-overrun warning on the missile
target-acquisition chunk buffer, and Release promotes it to an error:

```
Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp(282):
warning C6386: Buffer overrun while writing to 'piAcquireRows': the writable
size is '512' bytes, but 'iAcquireCount++' bytes might be written.
Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp(293,1):
error C2220: the following warning is treated as an error
```

Evidence: `/compile` (WorktreeCli) build of `BrokenEngineSandboxServer`
`Release|x64` on 2026-08-22, retained log
`Temp/AgentBuildLogs/brokenenginesandboxserver-20260822T143753293Z-9752.log`
lines 99-100 (machine-local; `Temp/` is not tracked, so the log is recoverable
only until that worktree is cleaned up — the failure reproduces from a fresh
Release build of the same project). `Debug|x64` of the same project built clean
seconds earlier, so this is Release-only, and specifically the
`EnablePREfast` `/analyze` gate described in
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:22-25`.

Pre-existing, not introduced by the session that recorded this Plan: the loop
was added by commit `774a1de` ("Replace the game Targets collection with a
transient engine FrameRegistry"), and `git status --porcelain` at that session's
baseline `79d9606` showed `MissilesUpdate.cpp` untouched.

Impact: the server has no buildable Release configuration, which also blocks any
Release-only verification (for example observing behavior with
`kbProfiling == false`).

Not yet checked, and part of this Plan's investigation: the same file is also a
member of the client project (`BrokenEngineSandbox.vcxproj`), whose Release
configuration sets `EnablePREfast` identically, so the client `Release|x64`
build is expected to fail the same way. Only Debug client builds were run in the
observing session.

## Design

Diagnosis belongs to this Plan, not to the session that recorded it. Read the
acquisition block at
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp:255-292`
first and decide between two outcomes before editing:

1. The bound holds and the analyzer simply cannot follow it. The recommended
   reading is that `AcquireChunk` resets `iAcquireCount` to `0` (line 272) and
   the loop flushes as soon as the count reaches `kiAcquireChunk` (lines
   283-286), so the highest index ever written at line 282 is
   `kiAcquireChunk - 1`; the reset happens inside a lambda, which is the likely
   reason `/analyze` loses the range. If diagnosis confirms this, make the bound
   locally visible to the analyzer with the smallest change that also stays
   readable to a human — for example an explicit bounds check or assertion
   immediately before the write, or restructuring the flush so the reset is
   visible in the loop body itself.
2. A real overrun exists. Then fix the overrun, and treat the Release build
   failure as the symptom rather than the target.

Whichever outcome holds, preserve the documented ordering contract: missile
target acquisition writes the frame, so the order and grouping of acquisitions
within a tick are part of the deterministic stream
(`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, Frame Registry query
windows). The comment at lines 249-254 states why the chunked ascending order is
equivalent to one whole-collection batch; a fix that changes chunk size,
ordering, or flush points must re-establish that equivalence, not assume it.

Suppressing the diagnostic is the author's non-recommended last resort, and
disabling the gate is out of scope entirely (see `## Out of scope`).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp:249-292`
  — the acquire-chunk buffer, its lambda, and the loop that writes `piAcquireRows`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:22-25` —
  the Release code-analysis gate this failure comes from (read-only reference)

## In scope

- Diagnosing the C6386 report at `MissilesUpdate.cpp:282` against the actual
  bound of `piAcquireRows`
- The smallest resulting change to the acquisition block at
  `MissilesUpdate.cpp:249-292` — `AcquireChunk`, `iAcquireCount`,
  `piAcquireRows`, `pAcquireResults`, and the row-collection loop — that makes
  `Release|x64` build while keeping the acquisition ordering equivalence stated
  in the block comment
- Confirming whether the client project's `Release|x64` build fails on the same
  diagnostic and covering it with the same fix

## Out of scope

- Changing `EnablePREfast`, `TreatWarningAsError`,
  `CodeAnalysisTreatWarningsAsErrors`, `BrokenEngineAnalysis.ruleset`, or any
  other project-level analysis setting to quiet the warning — the owning
  `AGENTS.md` states dropping `EnablePREfast` silently retires the gate
- Any other `/analyze` diagnostic that a full Release build surfaces after this
  one stops the compile; record those as separate follow-ups
- The engine-side `engine::AcquireRegistryTargets` implementation and the frame
  registry's reservation lifetime
- Other missile behavior: homing, falling, explosion, transfer, smoke trail

## Risk tier and invariants

Derived Tier 3. Trigger: the changed region sits inside a fixed-tick simulation
path whose acquisition ordering is CRC-exposed determinism state, which root
`AGENTS.md` lists as a Tier-3 surface. The author's recommendation is that a
session which proves its change emits identical code — for example an assertion
or a check the compiler removes, with no reordering — may present that evidence
at Step 1 and ask to classify lower; that reclassification is the classifying
session's decision, not this Plan's.

Invariants the change must not disturb:

- Acquisition happens after the main update loop, in ascending missile order,
  and each chunk folds into the shared subscription counts before the next chunk
  starts
- Both spans stay off the frame workbuffer (fixed-size stack storage), because
  cell ticks run on worker threads
- No heap allocation and no growth while the registry query window is live

## Acceptance criteria

- `BrokenEngineSandboxServer` `Release|x64` builds with zero errors through
  `/compile`, with no C6386 and no C2220 at `MissilesUpdate.cpp`
- The client `Release|x64` build is run and is likewise free of this diagnostic
- `BrokenEngineSandboxServer` and client `Debug|x64` still build clean
- If the change alters emitted acquisition ordering in any way, a replay
  determinism check through `/agent-harness` confirms client/server CRC parity;
  if it does not, state the no-behavior-change evidence instead

## Notes

- The analyzer's `Lines:` trace in the log walks 255, 256, 257, 258, 273, 260,
  275, 277, 282, 283, 285, 275, 277, 282 — it enters the lambda body at 273/260
  out of order, which is consistent with the analyzer losing the reset at line
  272 rather than with a real reachable overrun. Treat that as a starting
  hypothesis to confirm or refute, not as a conclusion.
