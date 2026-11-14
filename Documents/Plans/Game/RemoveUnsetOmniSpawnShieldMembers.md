<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:45:24.365Z","dependsOn":[]} -->
# Remove the never-set shield members of the Omni SpawnInfo

## Context

Line numbers are from commit `629ced2e`; locate by symbol if they moved. Paths
without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Nothing was built for
this Plan: every statement is from a code read.

`OmnisPostRender::SpawnInfo` declares `fShieldRotation` (`Omnis/Omnis.h:371`,
default `0.0f`) and `fShieldShrink` (`:372`, default `1.0f`). Nothing assigns
either: a search of `Projects/BrokenEngineSandbox` finds no write, and the
Transfer literal (`Omnis/OmnisNavigation.cpp:24-49`) names neither. Neither is
in `Fields()` (`Omnis/Omnis.h:388`), so neither travels in a wire
status-change payload. Their only reads are `Omnis/Omnis.cpp:287-288`, under
`BT_CLIENT`, which store them into the client-only columns `pfShieldRotations`
and `pfShieldShrinks`. So every spawn and every transfer arrival stores the
two defaults, and the members are configuration no caller can use.

Both members are still bytes in a replay. The members themselves are declared
in both builds, outside any `BT_CLIENT` guard, and the replay difference
stream writes every `SpawnInfo` as a raw image
(`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:198`):
`WriteFrameInput` (`Engine/Source/Frame/FrameBase.cpp:352-361`) passes each
payload to `common::Write`, which writes `sizeof(T)` bytes
(`Common/Serialization.h:99-103`). Removing them changes the Omni spawn and
transfer payload image a server build writes.
`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants` requires
a `FrameInput::kiVersion` bump for any such change and names that version as
the only guard: the difference stream checks no payload size.

The wire payload does not change. `Engine/Source/Network/AGENTS.md:28-36`
defines a game `StatusChange` wire payload as its `StatusChangeData` arm's
`Fields()` and requires an `engine::kiProtocolVersion` bump when that layout
changes; `Fields()` lists neither member and stays as it is.

Originating gap: the architecture phase (client/server and data shape lens)
of a findings-only deep analysis of that directory (recursive), confirmed by
an independent verification read. Pre-existing debt, not an unmet acceptance
criterion of any change. Unmet criterion: `SpawnInfo` holds no member that
nothing sets.

## Design

Recommended (the author's recommendation):

1. Delete both members from `OmnisPostRender::SpawnInfo`.
2. At `Omnis/Omnis.cpp:287-288`, store `0.0f` and `1.0f`, the values the
   defaults supply today.
3. Reword the two comments that name the members: the `bTransfer` comment
   (`Omnis/Omnis.h:383`) and the `Fields()` comment (`:386-387`).
4. Bump `FrameInput::kiVersion`
   (`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:10`), so replays
   recorded before the change are rejected; no compatibility path is added.

`Collections/AGENTS.md` `## Game-Specific Rules` stays true: client-only
shield animation state still starts from canonical client defaults on a
transfer arrival.

Risk tier: Tier 3 (the author's classification). Trigger: serialization and
replay compatibility: the raw `SpawnInfo` image in the replay difference
stream changes for every Omni spawn and transfer status change, and
`FrameInput::kiVersion` rises. No simulation value changes, so there is no
determinism/CRC exposure and no `Frame::kiVersion` bump: neither member is a
collection column, and their only reads are the two client-only stores. No
`engine::kiProtocolVersion` bump, for the reason `## Context` gives. No
threading exposure. `Omnis.h` is in the game precompiled-header closure, but
the removed members have no user outside the two stores.

## Critical files

- `Omnis/Omnis.h`
- `Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h`

## In scope

- `OmnisPostRender::SpawnInfo::fShieldRotation` and `fShieldShrink`.
- The two stores in `OmnisPostRender::Spawn(Frame&, const SpawnInfo&)` at
  `Omnis/Omnis.cpp:287-288`.
- The two `SpawnInfo` comments that name the members.
- The `FrameInput::kiVersion` literal in `FrameInput.h`.

## Out of scope

- The `pfShieldRotations` and `pfShieldShrinks` columns and their Update
  (`Omnis/Omnis.cpp:400-405`).
- Every other `SpawnInfo` member and the `Fields()` list.
- `engine::kiProtocolVersion` and `Frame::kiVersion`.
- Any compatibility path for older replays.

## Acceptance criteria

1. No `fShieldRotation` or `fShieldShrink` member remains in `SpawnInfo`, and
   `Fields()` is unchanged; the diff settles this.
2. `FrameInput::kiVersion` is higher than at commit `629ced2e`; the diff
   settles this.
3. `/compile` builds the BrokenEngineSandbox client and server.
4. Through `/agent-harness`, a replay recorded on the changed build, with
   Omnis spawning and transferring between cells, plays back with no per-tick
   CRC mismatch. The supplied evidence records no harness setup, so the
   executing session builds the scene.

## Notes

`Documents/Plans/Game/OmniAnimationTimeClientOnly.md` rewrites the same
`bTransfer` comment. Whichever lands second words the comment against the
file as it finds it.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
