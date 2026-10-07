<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-07T11:50:28.203Z","dependsOn":[]} -->
# Replace "fixture" with "HarnessRig" repo-wide

## Context

"Fixture" is testing jargon. In this repository it names the agent-harness-only code that sets up or forces a situation (a broken packet, a paused audio stream, a replay transfer capture) and records what happens so the harness can check it. The user found the word opaque and asked: "Plan to remove Fixture repo-wide and replace with HarnessRig".

The user chose these, by picking presented options:
- **Full form everywhere.** The presented mapping was `AudioStreamingFixture -> AudioStreamingHarnessRig`, `ClientPacketFaultFixture.cpp -> ClientPacketFaultHarnessRig.cpp`, `client_packet_fault_fixture -> client_packet_fault_harness_rig`, `registry_fixture -> registry_harness_rig`, `pFixture -> pHarnessRig`, and prose "fixture" -> "harness rig".
- **Plain fitting words for the three non-harness meanings.** The presented mapping was:
  - `Validate-Skill.ps1 -Fixture -> -Scratch`
  - "disposable fixture" -> "scratch package"
  - `BuildCommand.cpp` "Fixtures may shorten" -> "Tests may shorten"
  - `Tools/WorktreeCli/AGENTS.md` "exists for fixtures" -> "exists for tests"
  - `ToolCliCommon.cpp` "deep CI/fixture" -> "deep CI/test"

Outcome: `git grep -i -I fixture` returns nothing in tracked files. ThirdParty is a set of submodules and is never modified, so its own uses stay. This Plan file itself is exempt and is never edited for the rename: its name and text must say "fixture", and it is deleted when the change lands.

Inventory at base commit `a4564d66`: 1268 occurrences in 87 tracked files. No replay, save, `.pack`/`.manifest`, CRC, or version data contains the word. No "rig" or "HarnessRig" exists anywhere, so nothing collides. Line numbers below are from that commit and only locate occurrences.

## Design

A pure rename. No logic, control flow, data layout, or message structure changes.

Rename rules, per casing. Apply them to every occurrence of "fixture" in any case:

| Form | Example old | Example new |
|---|---|---|
| PascalCase within identifiers and file names | `AudioStreamingFixture`, `ReplayFixtures`, `RequireFixtureClient`, `ToFixtureState` | `AudioStreamingHarnessRig`, `ReplayHarnessRigs`, `RequireHarnessRigClient`, `ToHarnessRigState` |
| Hungarian-prefixed names | `pFixture`, `rFixture`, `sFixture`, `mpAudioStreamingFixture`, `gpAttachedAudioStreamingFixture`, `bFirstFixtureAction`, `sbFixtureActionIssued`, `pDeliveredFixture` | `pHarnessRig`, `rHarnessRig`, `sHarnessRig`, `mpAudioStreamingHarnessRig`, `gpAttachedAudioStreamingHarnessRig`, `bFirstHarnessRigAction`, `sbHarnessRigActionIssued`, `pDeliveredHarnessRig` |
| camelCase | `fixture`, `clearFixture`, `replayTransferFixtures`, JSON reply key `pendingTransferFixtureCount` | `harnessRig`, `clearHarnessRig`, `replayTransferHarnessRigs`, `pendingTransferHarnessRigCount` |
| snake_case harness command names | `client_packet_fault_fixture` | `client_packet_fault_harness_rig` |
| Prose | "fixture", "fixtures" | "harness rig", "harness rigs" |

Rules for the edge cases:
- Plural `Fixtures` becomes `HarnessRigs`.
- Where prose already says "harness" next to the word, write a single "harness rig", never "harness harness rig". For example, "a harness fixture" becomes "a harness rig". An "Agent fixture" becomes an "agent harness rig".

The 12 harness command names:

| Old | New |
|---|---|
| `audio_streaming_fixture` | `audio_streaming_harness_rig` |
| `client_subscribe_accept_fixture` | `client_subscribe_accept_harness_rig` |
| `client_stale_update_fixture` | `client_stale_update_harness_rig` |
| `client_cancelled_subscription_fixture` | `client_cancelled_subscription_harness_rig` |
| `client_packet_fault_fixture` | `client_packet_fault_harness_rig` |
| `client_full_state_fixture` | `client_full_state_harness_rig` |
| `game_packet_fault_fixture` | `game_packet_fault_harness_rig` |
| `engine_packet_fault_fixture` | `engine_packet_fault_harness_rig` |
| `server_pre_handshake_ack_fixture` | `server_pre_handshake_ack_harness_rig` |
| `crash_report_fixture` | `crash_report_harness_rig` |
| `collection_layout_capacity_fixture` | `collection_layout_capacity_harness_rig` |
| `registry_fixture` | `registry_harness_rig` |

Non-harness meanings:
- `Validate-Skill.ps1`: the option `-Fixture` becomes `-Scratch`.
- The author recommends renaming its internal variables and result property too, so no "fixture" remains; the In scope list includes these renames: `$fixtureCount` becomes `$scratchCount`, the `Fixture` property and `$Fixture` become `Scratch` and `$Scratch`, and "disposable fixture(s)" becomes "scratch package(s)" in that skill's prose. `SKILL.md:37`'s `` `Fixture: true` `` becomes `` `Scratch: true` ``.
- WorktreeCli and ToolCommon comments: "fixtures" becomes "tests", and "CI/fixture" becomes "CI/test".

No backward compatibility, per the root AGENTS.md directive. The old command names, the old JSON key, and the old `-Fixture` option are removed outright. The existing dispatchers already reject an unknown command, and `Validate-Skill.ps1` already rejects an unknown option as `SETUP_ERROR`/2. No alias is added.

Edit tracked files with the host `Edit` tool, using `replace_all` per distinct token per file, so BOM, CRLF, and the trailing newline are preserved. Rename files with `git mv`.

## In scope

In every file listed, the only permitted edits are:
- each occurrence of "fixture" in any case, renamed per the Design rules
- re-sorting an `#include` block whose order changes because a renamed header path sorts differently (`.editorconfig` include sort)
- re-wrapping a comment or prose line only where the longer name pushes it past the file's existing wrap width

### File renames (`git mv`, 22 files as `.h`/`.cpp` pairs)

`Engine/Source/Agent/Commands/`:
- `AudioStreamingFixture` becomes `AudioStreamingHarnessRig`
- `ClientNetworkFixtures` becomes `ClientNetworkHarnessRigs`
- `CrashReportFixture` becomes `CrashReportHarnessRig`
- `ReplayFixtures` becomes `ReplayHarnessRigs`

`Projects/BrokenEngineSandbox/Source/Agent/Commands/`:
- `ClientFullStateFixture` becomes `ClientFullStateHarnessRig`
- `ClientPacketFaultFixture` becomes `ClientPacketFaultHarnessRig`
- `ClientSubscriptionFixtures` becomes `ClientSubscriptionHarnessRigs`
- `CollectionLayoutCapacityFixture` becomes `CollectionLayoutCapacityHarnessRig`
- `RegistryFixture` becomes `RegistryHarnessRig`
- `ServerFaultFixtures` becomes `ServerFaultHarnessRigs`
- `ServerSimulationFixtures` becomes `ServerSimulationHarnessRigs`

The renamed files' own contents follow the same rules: namespaces `engine::ClientNetworkFixtures` and `engine::ReplayFixtures`, the class `AudioStreamingFixture` and every `AudioStreamingFixture*` enum, struct, and alias, every `Command*Fixture` handler, and every local, static, and member listed in the Design table.

### Project membership

`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/`:
- `BrokenEngineSandbox.vcxproj` (16 `ClInclude`/`ClCompile` entries) and its `.vcxproj.filters` (16 entries)
- `BrokenEngineSandboxServer.vcxproj` (12 entries) and its `.vcxproj.filters` (12 entries)

Only the renamed paths change. Each file keeps its current client or server membership and filter.

### Engine C++

- `Engine/Source/Agent/AgentCommandServer.h`, `AgentCommandServer.cpp`: `mpAudioStreamingFixture` member, `AudioStreamingFixture` type uses, include.
- `Engine/Source/Agent/AgentCommandsShared.h:9`: comment. `AgentCommandsShared.cpp`: include, `CommandCrashReportFixture` call, and the `"crash_report_fixture"` dispatch literal at line 238.
- `Engine/Source/Audio/AudioManager.cpp`, `StreamingVoice.cpp`, `StreamingVoices.cpp`: includes, `AudioStreamingFixture*` type and enum uses, `gpAttachedAudioStreamingFixture`, `pFixture`.
- `Engine/Source/File/PackChunks.cpp`, `PackChunkLoader.cpp`: includes, `AudioStreamingFixture*` uses, `pFixture`.
- `Engine/Source/File/Replay.cpp`: include, `ReplayFixtures::` calls (40), comment at line 395.
- `Engine/Source/GameBase.cpp`: include, `ReplayFixtures` uses.
- `Engine/Source/CrashReport.cpp:156`: comment.
- `Engine/Source/Network/Client/Client.h:80`: comment. `Client.cpp`: include, `ClientNetworkFixtures` uses.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`: include, `ClientNetworkFixtures` uses, `pDeliveredFixture`, `QueryFixtureCoordinateUpdateState`.
- `Engine/Source/Network/Server/Server.h:157`: comment.
- `Engine/Source/Network/Server/ServerBroadcaster.h:7`: comment. `ServerBroadcaster.cpp`: include.
- `Engine/Source/Network/Server/ServerTransferManager.h:8`: comment. `ServerTransferManager.cpp`: include, `DrainReplayTransferFixtures`, `ResetReplayTransferFixtures`.

### Project C++

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.cpp`: includes, handler calls, the `"collection_layout_capacity_fixture"` and `"registry_fixture"` dispatch literals at lines 22 and 28.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp`: includes, handler calls, six dispatch literals at lines 138-163.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp`: includes, `ExecuteServerSimulationFixtureCommand`, `CountReplayTransferFixtures`, the JSON key at line 110, three dispatch literals at lines 303-313.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.h:10`: comment.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentCommandsAudioStreaming.h`: `CommandAudioStreamingFixture`.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`: every "fixture" occurrence in any case (283 on 228 lines), `kpcSchema`, and the `"audio_streaming_fixture requires …"` error texts.
- In each renamed Project command file, the `"<command> requires …"` and `"… is already active"` error texts carry the new command name.
- `Projects/BrokenEngineSandbox/Source/Game.cpp`: include, comment at line 550.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`: includes, `ResetClientPacketFaultFixture`, `DetachClientFullStateFixture`, `DetachClientSubscriptionFixtures`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`: include, `DetachServerSimulationFixtures`.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:10`: comment. `GameSaveLoad.cpp`: include, `ResetReplayTransferFixtures`.

### Tools C++ and docs (non-harness meaning)

- `Tools/WorktreeCli/BuildCommand.cpp:57`: "Fixtures" becomes "Tests".
- `Tools/WorktreeCli/AGENTS.md:14`: "exists for fixtures" becomes "exists for tests".
- `Tools/ToolCommon/ToolCliCommon.cpp:360`: "CI/fixture" becomes "CI/test".

### Skill validator (non-harness meaning)

- `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1`: help text at lines 3 and 6, `$fixtureCount` (43, 58, 64), the `-ieq '-Fixture'` compare at line 56, the result property at line 69, `$Fixture` at lines 763 and 797, the error text at line 803.
- `.agents/skills/external-skill-creator/SKILL.md:37`.
- `.agents/skills/external-skill-creator/references/frontmatter-schema.md:118, 125-126, 130`.
- `.agents/skills/external-skill-creator/references/semantic-review.md:22-23, 100`.
- `.agents/skills/external-skill-creator/references/validation.md:35, 56`.

### Harness docs and skills

- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md:38`.
- In `Projects/BrokenEngineSandbox/Documents/AgentHarness/`:
  - `commands-client.md`, `commands-both.md`, `cross-cell.md`, `verification.md`: command names and prose.
  - `commands-server.md`: command names, prose, the `status` reply key at line 7, and the anchor at line 71, which becomes `replay.md#replay-transfer-capture-harness-rig`.
  - `replay.md`: the heading `### Replay transfer-capture fixture` at line 20 becomes `### Replay transfer-capture harness rig`, plus the prose.
- `.agents/skills/agent-harness/references/command-reference.md:18`: command name and quoted error texts. `.agents/skills/agent-harness/references/worker.md:338`.

### AGENTS.md and documents

- `Engine/Source/AGENTS.md`
- `Engine/Source/Agent/AGENTS.md`, `Engine/Source/Audio/AGENTS.md`, `Engine/Source/File/AGENTS.md`
- `Engine/Source/Network/Client/AGENTS.md`, `Engine/Source/Network/Server/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
- `Documents/C++StyleGuide.txt:231-232`: the include-sort example. Keep the two lines in sorted order after the rename.
- `Documents/Plans/Game/CollectionTimesToChrono.md:26, 32, 66, 75`: path strings and "fixture scalar".
- `Documents/Investigations/ChangeWorkflow/OwnIntegerTypeSweepDeferredFixes.md`: path strings in headings and table cells, and prose at lines 586-588.

### Occurrences added after the base commit

Any further "fixture" occurrence in a tracked file outside ThirdParty and this Plan file at execution time is in scope under the same rules: the harness meaning becomes HarnessRig; a non-harness meaning goes to the user as a decision.

## Out of scope

- ThirdParty (all submodules): never modified.
- Any change to behavior, logic, control flow, data layout, message fields other than the one key rename, error-message wording beyond the command name, or file membership.
- Splitting, merging, or reorganizing the renamed files. Renaming any identifier that does not contain "fixture".
- Fixing, restyling, or reformatting adjacent code or prose encountered during the rename. This includes pre-existing style findings in touched files, apart from the include re-sort and re-wrap the In-scope rules permit.
- Compatibility aliases for old command names, the old JSON key, or `-Fixture`.
- Binary files whose bytes coincidentally contain "rig" (`.wav`/`.png`/texture data under `Projects/BrokenEngineSandbox/Data/`).

## Risk tier and invariants

- **Tier 2.** The change is behavior-preserving except for the agent-harness command names, one `status` reply key, and the `Validate-Skill.ps1` option name. These are developer-tool surfaces: the agent command channel is exempt from trust and wire rules (root AGENTS.md trust policy).
- Determinism and CRC, the network wire protocol, serialization, save and replay formats, threading, and trust boundaries are untouched. No version bump applies.
- Client and server project membership is unchanged per file. Run `/update-vcxproj` validation.
- The Change Workflow triggers that apply: `/update-affected-code`, `/update-vcxproj`, `/code-style-review` over the changed ranges, `/update-claude-docs`, `/progressive-disclosure-review` (AGENTS.md and skill files change), and `/external-skill-creator` validate mode (the external-skill-creator package changes).

## Acceptance criteria

1. From the worktree root, `git grep -i -I fixture -- ':!Documents/Plans/Game/ReplaceFixtureWithHarnessRig.md'` prints nothing, and `git ls-files | grep -i fixture` prints nothing except `Documents/Plans/Game/ReplaceFixtureWithHarnessRig.md`.
2. `/compile` builds BrokenEngineSandbox client and server in Debug with no errors.
3. `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA>` (the Change Workflow's static-check step) passes, and `/update-vcxproj` validation reports no membership drift.
4. `/agent-harness` with a running client and server:
   - `registry_harness_rig` and `collection_layout_capacity_harness_rig` succeed on both endpoints.
   - The server `status` reply carries `pendingTransferHarnessRigCount`.
   - `registry_fixture` is rejected as an unknown command.
5. `Validate-Skill.ps1 -Path .agents/skills/external-skill-creator` returns `VALID`/0. With `-Fixture` added it returns `SETUP_ERROR`/2. A scratch package created under a temporary directory outside the repository validates with `-Scratch`, following `frontmatter-schema.md`'s validator-change guidance.
6. The anchor link in `commands-server.md` resolves to the renamed heading in `replay.md`.
