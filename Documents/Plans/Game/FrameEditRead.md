<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T17:48:27.429Z","dependsOn":[]} -->
# `read_frame`: read any named column or frame-wide value of a cell

Line numbers cite baseline `2b4cb8d38aa57ca7ccd45983cc5121f64e85cf15`, plus the names `Documents/Plans/Game/FrameEditNaming.md` introduces (`FrameCollections`, `FrameValueColumns`, `OwnerInFrame`). Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

User direction (binding): the Debug-only server harness reads any part of a cell's Frame so an agent can inspect the exact state it set up. Today the harness reads frames only through fixed-schema queries (`query_frame`, `query_players`, `query_collection`; `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:157-209`), which expose a handful of columns and nothing for `explosions`, `pushers`, or frame-wide values. The naming Plan gives every server column and frame-wide value a compile-enforced name; this Plan adds the one read command that walks those same lists, so a newly named value is readable with no further code. The existing queries keep their schemas: they stay the cheap counts and summaries other harness recipes use.

Decisions this Plan makes (author's, with rationale):

1. One command, `read_frame`, returns a whole row or the whole frame-wide value set, keyed by the same member names `edit_frame` takes. No column filter: a row is a few hundred bytes, far below the 16 MiB response cap (`Engine/Source/Agent/AgentCommandServer.h:72-73`), and a filter would be an option with no current consumer.
2. Reads include what writes refuse: identity and handle columns come back as their uuid integers, and the clock values come back as numbers. Reading breaks no invariant, and an agent needs ids to address rows across commands (`query_players` already exposes `uuid`).
3. `read_frame` requires `kbDebugInput` like `edit_frame`, following the user's Debug-only decision for the whole feature, and is not refused during recording or playback, because it changes nothing.
4. It lives in `ServerFrameEdit.cpp` beside the lists it walks; the module owns `edit_frame` and `read_frame`.

## Design

### Schema

`read_frame {"coord":[x,y],"collection":<name>,"index"?:int}`; these are the only keys, and any other key fails. The frame is `QueryFrame(rParameters)` (`AgentCommandsServerQueries.cpp:140-155`), so the cell must be loaded with a current frame. `collection` is a `FrameCollections` `kName` or `frame`.

- Collection: `index` is required, a JSON integer with `0 <= index < count` where `count` is the pair's PostRender `iCount`; an out-of-range or missing index fails with `'<collection>' read requires integer 'index' in [0,<count>)`. Response: `{"count":int,"index":int,"values":{<column>:<value>,...}}` with one entry per column of the pair's column list, Interpolate columns first, in list order.
- `frame`: `index` must be absent (`'frame' read takes only 'coord' and 'collection'`). Response: `{"values":{...}}` with one entry per `FrameValueColumns` entry in list order.

Value forms, the inverse of `edit_frame`'s (`ServerFrameEdit.cpp:123-177`), one `WriteElement(nlohmann::json& rValue, const ELEMENT& rElement)` template with a dependent `static_assert` for any other type:

- `float`: number. `int32_t`, `int64_t`, `uint8_t`, `uint16_t`, `uint64_t`: integer.
- `XMVECTOR`: `[x,y,z]` (w is the lane invariant, position 1 and direction 0, and `edit_frame` keeps it).
- `common::Flags<E>`: the underlying integer (`std::to_underlying(meFlags)`, as `ExtractPlayers` does at `:64`).
- `engine::AlignmentIdentifier`: integer `uiValue`. `engine::GlobalId`: integer `iValue`. `engine::ClientGuid`: `[uiHigh,uiLow]`. `engine::GridCoord`: `[iX,iY]`. `engine::Id<T>`: integer `uuid.iValue`.
- `common::RandomEngine`: unsigned integer `State()`.
- `engine::Alignments`: array of `{"a":uint32,"b":uint32,"flags":uint8}` in stored order, `a = uiKey >> 32`, `b = uiKey & 0xFFFFFFFF` (`Engine/Source/Frame/Alignments.cpp:11-20`).
- An array-of-pointer column (`pfTrailTimes`): array of `std::extent_v` values, element `j` reading `member[j][index]`, mirroring `WriteMember` (`:251-263`).

Structure: `ReadMember<IS_COLUMN, MEMBER>(const Frame&, int64_t iIndex, nlohmann::json& rValues, std::string_view name)` mirrors `WriteMember`; `ReadKeys<IS_COLUMN>(FrameColumnList<COLUMNS...>, ...)` is a fold adding every column; `CommandReadFrame` dispatches over `FrameCollections` the way `ApplyEdit` does, with the `frame` arm on `FrameValueColumns`, and the final error names every `kName` plus `frame`. The `kbDebugInput` refusal uses the `if constexpr (!kbDebugInput)` idiom (`:362-365`) with the text `read_frame requires kbDebugInput build`.

`OwnerInFrame` (as the naming Plan leaves it) takes `Frame&`, so the read path cannot reach it through `const Frame&`. It becomes a template over the frame type as well, `OwnerInFrame<OWNER>(FRAME& rFrame)` returning `std::conditional_t<std::is_const_v<FRAME>, const OWNER, OWNER>&`, so a `const Frame&` yields `const OWNER&` and the existing `Frame&` callers are unchanged. Each tuple lookup names the element type that tuple yields: `ServerCollections(this auto&&)` (`Engine/Source/Frame/FrameBase.h:105-108`) yields const references on a const frame, while `GameInterpolateCollections`/`GamePostRenderCollections` (`Projects/BrokenEngineSandbox/Source/Frame/FrameCollections.h:11-19`) and the Players arms dereference pointers and yield non-const references, which the return type then binds as const. So the two `ServerCollections()` lookups become `std::get<std::conditional_t<std::is_const_v<FRAME>, const OWNER, OWNER>&>` and the two game-tuple lookups stay `std::get<OWNER&>`; the naming Plan's `kbInTypeList` arm selection is unchanged, because it compares cv-stripped element types and so matches the tuple holding `OWNER` on either frame constness.

### Dispatch

`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.h` declares `void CommandReadFrame(const nlohmann::json& rParameters, nlohmann::json& rResult);`. `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp` `ExecuteAgentCommandServer` gains one `read_frame` row directly after the `edit_frame` row (`:343-347`).

### Documentation

- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`: one `read_frame` entry after the `edit_frame` entry stating the schema, the two response shapes, the value forms including identity ids and clock values, the `kbDebugInput` refusal, and that recording and playback do not refuse it. The ownership sentences at `:87` and `:92` say frame edits and reads live in `Commands/ServerFrameEdit.cpp`. The `edit_frame` entry's parenthetical "no query reports it for `explosions` or `pushers`" becomes "`read_frame` reports it as `count`".
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Overview` (`:5`): the frame-edit module owns `edit_frame` and `read_frame`. `## Contracts`: the `edit_frame keys` bullet also covers `read_frame` (same lists, every named value readable, identity and clock values included).
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.h:10-11` comment: frame edits and reads are in `Commands/ServerFrameEdit.cpp`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.h`, `.cpp`: `CommandReadFrame`, `WriteElement`, `ReadMember`, `ReadKeys`, and the const-generic `OwnerInFrame`.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp`: dispatch row.
- Documentation: `commands-server.md`, game `Agent/AGENTS.md`, `AgentCommandsServerQueries.h` comment.

## In scope

- `ServerFrameEdit.cpp`: `WriteElement`, `ReadMember`, `ReadKeys`, `CommandReadFrame`, and nothing in the existing write path beyond sharing `FrameCollections`, `FrameValueColumns`, `IntegerFromValue`, and `kbIsIdentity`, and making `OwnerInFrame` generic over the frame type's constness per `### Schema`.
- `ServerFrameEdit.h`: the `CommandReadFrame` declaration.
- `AgentCommandsServer.cpp` `ExecuteAgentCommandServer`: the `read_frame` row.
- The documentation edits under `### Documentation`.

## Out of scope

- Any change to `edit_frame` behavior, value forms, refusals, or response; row add or remove; cells and pins; the save format.
- `query_frame`, `query_players`, `query_collection`, and their extractors and schemas; `status`.
- A column filter, paging, whole-collection or whole-frame dumps, binary export, and reads of `CellStaticData`, `uiSharedCrc`, `transferRequests`, `idToIndexMap`, `iCapacity`, or client-only columns (not on the server).
- Client-build code and client commands; the vcxproj files (no file is added).

## Risk triggers and invariants

Tier 2 (`.agents/references/risk-tiers.md`): one new read-only command in the game Agent subsystem; it changes no determinism, wire, serialization, threading, or trust surface.

- The command writes nothing: the frame, its CRC, and every counter are byte-identical after any `read_frame`.
- Every value form is the inverse of the `edit_frame` form for the same element type, except the w lane (never exposed) and the read-only types (`engine::Id<T>`, `int64_t`).
- Main thread, at the existing drain point; exempt developer channel.

## Acceptance criteria

Builds: server Debug and Profile compile, proving every named element type has a `WriteElement` arm.

Harness checks (`/agent-harness`, Debug server, after `reset`; the player is created with `inject_payload` `SpawnPlayer` and one unpaused tick, then `pause {"paused":true}`):

| # | Check | Expected observation |
|---|---|---|
| 1 | `read_frame` `players` index 0; compare with `query_players` row 0 | `count` equals `query_players.total`; `values.pVecPositions` equals `local`, `pVecDirections` equals `dir`, `pfArmors` equals `armor`, `pfShields` equals `shield`, `pFlags` equals `flags`, `pAlignments` equals `alignment`, `pIds` equals `uuid`, `pGlobalPlayerIds` equals `globalId`; `pPushers` is nonzero; `pClientGuids` is `[0,0]`; `pFleetWantedCoordinates` is `[0,0]`; every column of `PlayersFrameColumns` is present and no other key |
| 2 | `read_frame` `pushers` index 0 and `read_frame` `frame` | the pusher's `pIds` equals the player's `pPushers`; `frame.values.iTick` equals `status.tick`, `fDeltaTime` equals `1/32` as a float, `randomEngine` is nonzero, `uiNextUuid` is at least 3, `alignments` is a non-empty array sorted by `(a,b)`, `enemyAlignment` and `playerAlignment` are nonzero and distinct, `gameFlags` is `2`, and every `FrameValueColumns` key is present |
| 3 | Round trips through `edit_frame` then `read_frame`: `players` `pfArmors: 0.25`, `pVecPositions: [10,-20,z]`, `pFleetWantedCoordinates: [3,-4]`, `pClientGuids: [7,8]`, `puiPendingWeaponModeTicks: 5`; `frame` `randomEngine: 1234567`, `alignments: [{"a":2,"b":1,"flags":1},{"a":1,"b":2,"flags":3}]`, `fSpawnTimer: 0.25` | each read returns the written value: `pVecPositions` `[10,-20,z]`, `randomEngine` `1234567`, `alignments` exactly `[{"a":1,"b":2,"flags":3}]` (one pair, sorted, last flags win), `fSpawnTimer` `0.25` |
| 4 | Rejections, each its own command: unknown top-level key; `collection: "frame"` with `index: 0`; `players` without `index`; `players` `index` equal to `count`; `players` `index: -1`; `explosions` `index: 0` on an empty collection; unknown collection | `ok:false` each, the index ones naming `[0,<count>)`, the unknown collection listing every name plus `frame` |
| 5 | While recording (`replay_record {"start":true}`, poll `recording:true`) and during playback: `read_frame` `frame` | `ok:true` both times |
| 6 | Profile server: `read_frame` | `ok:false` with `read_frame requires kbDebugInput build` |

By code reading: `CommandReadFrame` binds `const Frame&` and calls no mutating member; `WriteElement` for `XMVECTOR` emits three lanes; the `static_assert(false)` fallback is unreachable on the server build because the build succeeded.

## Execution card

- Goal: `read_frame` per `## Design`, the dispatch row, and the documentation edits under `## In scope`.
- Tier 2; roles as the Change Workflow assigns: `implementer` runs `/implement-plan` in one slice, then `/update-affected-code`; `builder` runs `/compile` (server Debug and Profile); `reviewer` runs `/plan-audit` and `/plan-simplicity-review` before implementation and `/repo-code-review`, `/comment-review`, and `/coherence-review` after; `/update-claude-docs` and `/progressive-disclosure-review` follow; `/finalize-changes` lands with user confirmation.
