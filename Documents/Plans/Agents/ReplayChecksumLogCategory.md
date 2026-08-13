<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T17:23:30.424Z","dependsOn":[]} -->
# Make per-tick replay checksums scrapeable through a dedicated Replay log category

## Context

An agent verifying a determinism-sensitive change has no way to read the literal per-tick frame CRC
sequence out of a running client or server. Confirmed from current source during preparation of
`Documents/Plans/Frame/FrameUtilsSharedHelpers.md`:

- No harness command returns a frame CRC. `status`, `query_frame`, `query_players`, and
  `query_collection` expose no checksum field
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness.md:139`, `:151`, `:152`, `:153`).
- The engine already computes and logs the exact values wanted, but only at `kVerbose` in the
  `kDefault` category: `Engine/Source/File/DifferenceStream.h:42` (writer initial), `:58` (writer
  per-tick), `:99` (writer terminal), and `:440` (reader per-tick).
- `Projects/BrokenEngineSandbox/Source/Pch.h:89` sets the `kDefault` compile floor to `kDebug`, and
  `LOG` compile-eliminates any line below its category floor (`Common/Log/Log.h:215`), so all four
  lines are absent from every Sandbox binary. `set_log_level` clamps a requested level upward to the
  compile floor (`Engine/Source/Agent/AgentCommandsShared.cpp:163-169`), so no runtime command can
  restore them.
- Consequence, accepted for the change that found it: determinism acceptance evidence must rely on
  the replay reader's own mismatch verdict (`kError` lines) instead of diffing literal per-tick CRC
  sequences, which is why the recorded recipes only test for the absence of failure lines
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness.md:76-81`, `:96`).

The user authorized recording this as follow-up work; it was out of scope for that change.

## Design

Do not add a harness CRC command. The values already exist as log lines, so the smallest complete fix
is to make those existing lines reachable, and the log system already has the mechanism: a per-category
compile floor plus a per-category ring buffer that `get_logs {"category":...}` reads directly
(`Engine/Source/Agent/AgentCommandsShared.cpp:137-141`).

Add one log category, `kReplay`, appended immediately before `kCount` per the rule at
`Common/Log/LogTypes.h:17`, and give it a `kVerbose` compile floor in both project PCHs. Move the four
`DifferenceStream.h` checksum lines from `kDefault` into `kReplay`, keeping them at `kVerbose`. The
call-site level convention in the root `AGENTS.md` is preserved: these lines are per-tick, so they stay
`kVerbose`; promoting them to `kDebug` instead would contradict that convention and would also dump
per-tick lines into the shared `Default` ring whenever an agent lowers `Default` for unrelated
diagnosis. Their runtime level keeps the ordinary `keLogRuntimeDefault` (`kInfo`), so they stay silent
out of the box and an agent turns exactly this stream on with
`set_log_level {"category":"Replay","level":"Verbose"}`, then reads only these lines with
`get_logs {"category":"Replay"}`. `kReplay` must not copy the `kTemp` always-emit exception
(`Common/Log/Log.cpp:22`), which would spam every build by default.

Nothing about checksum computation, recording, storage, file layout, or validation changes; only the
category label on four `LOG` call sites, plus the category table entries the log system requires to stay
in step. The lines format integers only, so they remain allocation-free inside allocation-tracked
simulation code. With the category silent at the default runtime level, a compiled-in line costs one
relaxed atomic load and compare per call.

Accepted limitation, not to be worked around in this change: one category ring holds 128 lines
(`.agents/skills/agent-harness/references/command-reference.md:11`), roughly four seconds of one
coordinate's ticks at 32 Hz, so an agent polls incrementally rather than collecting a whole run in one
call. Do not resize ring buffers here.

## Critical files

- `Common/Log/LogTypes.h` — `LogCategory` enum (`:19-31`) and the level/category aliases (`:44-52`):
  append `kReplay` immediately before `kCount` and add its alias.
- `Common/Log/Log.h` — `kpcLogCategoryNames` (`:11`) and `keLogLevels` (`:205-209`): add `"Replay"`
  and `keLogLevelReplay` in the same ordinal position; both `static_assert`s must still hold.
- `Common/Log/Log.cpp` — `gLogRuntimeLevels` initializer (`:20-24`): add one `keLogRuntimeDefault`
  entry in the same position; do not give `kReplay` the `kTemp` always-emit value.
- `Projects/BrokenEngineSandbox/Source/Pch.h` — add `keLogLevelReplay = kVerbose` beside the other
  `keLogLevel*` floors (`:87-94`).
- `DataPacker/Source/Pch.h` — add `keLogLevelReplay = kVerbose` beside its floors (`:12-20`), which the
  shared `keLogLevels` table requires even though DataPacker never writes replay streams.
- `Engine/Source/File/DifferenceStream.h` — the four checksum `LOG` call sites `:42`, `:58`, `:99`,
  `:440`: change the category argument from `kDefault` to `kReplay`, leaving level and message text
  unchanged.
- `.agents/skills/agent-harness/references/command-reference.md` — the `get_logs` category value list
  (`:11`) must include `Replay`.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the replay determinism recipe (`:76-81`)
  gains the CRC-sequence scrape as the way to compare literal per-tick values.
- `Common/Log/AGENTS.md` — the contracts bullet naming `kTemp` as the compile-floor exception (`:7`)
  must also name `kReplay`'s `kVerbose` floor with its default-silent runtime level.

## In scope

- Adding the `kReplay` category to the enum, alias list, name table, compile-floor table, and runtime
  level table exactly as listed above
- Declaring `keLogLevelReplay = kVerbose` in the two project PCHs
- Re-categorizing the four `DifferenceStream.h` checksum `LOG` call sites
- The documentation edits named above: the `get_logs` category list, one replay-determinism recipe
  step, and the `Common/Log/AGENTS.md` floor-exception sentence

## Out of scope

- Any harness command, response field, or schema change, including a CRC query
- Changing the message text, level, or argument values of the four checksum lines
- Checksum computation, recording, `.checksums` file layout, replay manifests, `Frame::kiVersion`,
  wire format, or `DifferenceStreamReader::ValidateChecksum` behavior
- Per-tick CRC logging outside replay recording and playback (ordinary simulation ticks emit no
  checksum line today and gain none here)
- Ring-buffer sizes, `get_logs`/`set_log_level` behavior, and the crash-snapshot ring
- Re-categorizing any other log line, in `DifferenceStream.h` or elsewhere

## Risk tier and invariants

Change Workflow Tier 3. The trigger is cross-subsystem integration: one indivisible change to the
shared log-category tables must land together across independently owned subsystems — Common logging
(`LogCategory`, `kpcLogCategoryNames`, `keLogLevels`, `gLogRuntimeLevels`), the Engine replay writer and
reader call sites in `DifferenceStream.h`, the per-project compile floors in both the Sandbox and
DataPacker PCHs, and the harness documentation that publishes the category names to agents. A partial
landing does not compile, because the `static_assert`s bind the Common tables to floors declared in each
project's PCH.

Determinism/CRC is the trigger deliberately not reached: only the category argument of existing log
lines changes, and logging is not part of the CRC. If implementation finds it must touch checksum
computation, storage, or validation, that is outside this Plan's `## In scope` boundary — stop and
re-plan rather than widening the change.

Invariants to preserve:

- `LogCategory` indices stay dense and 0-based with `kCount` last; `kpcLogCategoryNames`, `keLogLevels`,
  and `gLogRuntimeLevels` stay ordinally aligned and their `static_assert`s pass.
- `kReplay` is silent at the default runtime level in both client and server builds.
- The four checksum lines stay allocation-free in allocation-tracked code.
- The client and server keep identical checksum content and validation behavior.

## Acceptance criteria

- With the client and server built and the server launched, `set_log_level`
  `{"category":"Replay","level":"Verbose"}` returns effective `Verbose` — proving the compile floor is
  `kVerbose`, since a `kDebug` floor would clamp it upward.
- Before that command, `get_logs {"category":"Replay","count":128}` returns no checksum line during an
  active recording, proving the default-silent runtime level.
- After it, on a **single-coordinate** replay — a freshly `reset` server with exactly one active
  coordinate (`status.activeCoords` length 1) and no `replay_transfer_fixture` — `replay_record` makes
  the `Replay` category return literal `Checksum DifferenceStreamWriter Update <tick>: <crc>` lines with
  consecutive ticks, and `replay_play` returns `Checksum DifferenceStreamReader <tick>: <crc>` lines, so
  an agent can diff the two sequences tick by tick and value by value.
  The single-coordinate constraint is required, not incidental: the message text carries tick and CRC
  but no coordinate, and recording and playback walk two separate unordered coordinate maps
  (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:996-1014` writers, `:1062-1086` readers),
  so a multi-coordinate replay emits repeated ticks whose relative order may differ between the two
  runs and a positional diff would report a false mismatch. Adding a coordinate to the message text is
  out of scope above; multi-coordinate comparison is therefore not an acceptance criterion here.
- No `Checksum DifferenceStream*` line appears in the `Default` category ring.
- A replay record-then-play run reports no new `LogDifferences CRC Client`, checksum-mismatch,
  `CONFIRMED DESYNC`, or replay-read failure line, proving the re-categorization changed no replay
  behavior.
- Client, server, and DataPacker compile.
