<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T20:42:14.624Z","dependsOn":[]} -->
# Remove citations of deleted Plans from live Plans and stop completion from leaving new ones

## Context

Live Plans under `Documents/Plans/` still name Plans that primary already completed or deleted. At this Plan's creation (commit `214e32f7`), 33 lines in 9 live Plans name 16 deleted Plan file names:

- `ChangeWorkflow/DeployAndRunHostedServer.md` `## Context` `:8-9` — `HardenServerForUnattendedHosting.md`, `ProvisionAzureServerVm.md` (past tense).
- `Engine/AgentHarnessIntoEngine.md` — `SharedSessionProtocol.md` `:13`, `:129`, `:302` ("moves", "establishes", "owned by"); `FrameEditRows.md` `:19`, `:23` ("removes") and `:100`, `:117` (past tense); `GameShellIntoGameBase.md` `:133` ("adds and leaves this site to this Plan"); `ProjectAwareTooling.md` `:58`, `:303`; `EngineOwnedSaveSettingsAndSwitches.md` `:108`, `:299`; `RemoveSandboxGameplayFromEngine.md` `:302`; `SharedEngineProjectFiles.md` `:341`.
- `Engine/ConnectClientToHostedServer.md` `## Coordination` `:81` — `GameShellIntoGameBase.md` ("Either order works").
- `Engine/EngineGameHookSweep.md` — `EngineOwnedSaveSettingsAndSwitches.md` `:31`; `EngineOwnedProfileManager.md` and `RemoveNetworkSessionContractStruct.md` `:43` ("prerequisite Plans written alongside this one"); `SharedSessionProtocol.md` and `SharedCollectionHelpers.md` `## Out of scope` `:104` and `## Notes` `:116` ("Depends on").
- `Engine/FlightPlanePresentation.md` `## Coordination` `:319` — `GameShellIntoGameBase.md` ("Whichever lands second").
- `Engine/NavigationPerFlightPlane.md` `## Context` — `FlightPlaneData.md` `:19`, `SplitIslandTerrainCollision.md` `:65`.
- `Game/CrossPlaneWeapons.md` `## Coordination` `:253` — `RemoveSandboxGameplayFromEngine.md`.
- `Game/FlightPlaneClimbing.md` — `FlightPlaneData.md` `:18`; `RemoveSandboxGameplayFromEngine.md` `## Coordination` `:347`.
- `Game/ShipTypeSplit.md` — `FlightPlaneData.md` `:56` ("prerequisite, written in parallel"); `## Coordination` `:672` `DeduplicateDescribeSceneUnitLoops.md`, `:673` `RemoveSandboxGameplayFromEngine.md` (with conditional "If that Plan lands first, it supersedes decision 14's renames ..."), `:687` `GameShellIntoGameBase.md`, `:688` `SharedCollectionHelpers.md` (recommends landing it first), `:696` `SandboxSmallCleanups.md`, `:697` `FrameEditRows.md` ("lands first").

Line numbers shift as those Plans change; the quoted text and the file names locate each site, and the current tree wins.

The pending-tense citations are false: they describe work as still to come, give ordering instructions against Plans that no longer exist, or leave conditional decisions unresolved. A session that claims one of these Plans then has to reconstruct which branch applies.

Root cause, from the completion path: `.agents/skills/next-plan/scripts/Complete-NextPlan.ps1` runs `WorktreeCli plan complete` (or `plan reject`),
which removes only direct-child `dependsOn` metadata edges and deletes the target (`Documents/Plans/AGENTS.md` `## Git-backed scheduler`; `.agents/skills/next-plan/SKILL.md` step 10, "Success removes only direct-child dependency edges").
Nothing in the claim exit or in the `/next-plan` preparation brief finds prose citations of the Plan being completed. For example, landing commit `fe7ca062` deleted `SharedSessionProtocol.md` and changed `AgentHarnessIntoEngine.md` in just 2 lines (its `dependsOn` edge), which left the prose citations at `:13`, `:129`, `:302` in place. Landing commit `096ea76c` deleted `FrameEditRows.md` and left `:19`, `:23` in place.
The claim-time `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` reports a missing full path only as an advisory lead (`next-plan/SKILL.md` step 4), and it does not see a bare file name such as `FrameEditRows.md`.

Some of these Plans were deleted by bulk user commits (`d8ce779b`, `1e88528b`, both "Opus 5.5 plans") instead of by `plan complete`. So whether their work landed has to be checked against the current tree, not assumed.
Spot checks at creation: `RemoveSandboxGameplayFromEngine.md` landed (`kOwnedEntityTransferType` at `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:63`, `mVecLastKnownTargetPosition` at `Engine/Source/Graphics/CameraBase.h:143`); `GameShellIntoGameBase.md` appears landed (`engine::Cell::TailFrame` exists, `IsMainMenuFrame` is gone); `SharedCollectionHelpers.md` did not land (`ModelInstances` appears nowhere under `Engine/` or `Projects/`).

## Design

Invariant: no live Plan other than the one being worked names, outside its byte-zero metadata line, the file name of a `Documents/Plans/**/*.md` Plan that is not live.

1. One-time cleanup. At claim time, rebuild the list of deleted Plan file names from main's history: every `Documents/Plans/**/*.md` path that `git log main --diff-filter=D` reports and that the live tree does not hold. For each live Plan line that names one of them, the author recommends rewriting the citation as the current-tree fact it stood for, naming no Plan file:
   - a pending claim ("moves", "removes", "adds", "owns") becomes the present-tense fact with its current `path:line` or symbol, when the tree shows the work landed, or is removed when the tree shows it did not;
   - a `## Coordination` bullet whose only content is ordering against a deleted Plan is removed. A conditional decision inside it ("If that Plan lands first, ...") is resolved to the branch the tree shows, and the citing Plan's affected decisions are updated to match, as that Plan's own conditional directs;
   - a past-tense record keeps its fact but drops the file name.
   When the tree settles neither branch of a conditional, return it to main as a user decision instead of choosing one.
2. Durable fix in `Complete-NextPlan.ps1`. After the support-folder checks and before the `plan complete`/`plan reject` call, so that a refusal changes nothing (mirroring the support-folder pre-check), scan every tracked `Documents/Plans/**/*.md` except `AGENTS.md` files and the held Plan.
   Read worktree content, skip line 1 (the metadata marker, whose edges `plan complete` removes), and look for the held Plan's file name (`<Name>.md`) where it is not preceded by an ASCII letter or digit.
   Any hit returns exit 2, `status: blocked`, a new code `completion.plan-cited`, one diagnostic naming the first citing path and line, a message giving the number of citing lines and telling the session to rewrite them by the citation rule that item 3 adds to `.agents/skills/next-plan/SKILL.md` step 4, naming that step, and rerun, and `nextAction: stop-report-to-user`. The result shape is unchanged, so `broken-engine-next-plan-completion-result/v4` stays.
3. Preparation, so that item 2 is normally a backstop. In `.agents/skills/next-plan/SKILL.md` step 4, add to the paragraph that has the preparation `implementer` apply the Leftovers rule: the implementer finds every live Plan that names the claimed Plan's file name and puts rewriting each such citation on the execution card as approved scope.
   The same addition states the citation rule itself, in as few sentences as it needs, because this Plan leaves the tree on completion: the three citation cases of item 1, written out with no reference to this Plan, with "landed" judged against the tree after the change, so the claimed Plan's own change counts as landed, and a conditional neither branch of which that tree settles returned to main as a user decision.
   That way the rewrites are reviewed with the change and ride its landing. In step 10, add that the script refuses while another live Plan still names the held Plan.

The author recommends one Plan for both parts: they share the root cause, the invariant, and the final grep. The cleanup without the check would re-accumulate on the next completion, as every completion since `fe7ca062` shows, and the check without the cleanup leaves the invariant false.

Change Workflow tier: Tier 2. Trigger: `Complete-NextPlan.ps1` gains a blocking check, which is one subsystem's tool behavior. The Plan edits are Tier 1 documentation. There is no determinism/CRC, serialization, `.pack`/`kiVersion`, replay, wire, affinity, threading, shader, or build exposure, and no live verification.

## Critical files

- `.agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
- `.agents/skills/next-plan/SKILL.md`
- The 9 live Plans listed in `## Context`, or whichever of them are still live at claim time

## In scope

- `Complete-NextPlan.ps1`: the citation pre-check of Design item 2, placed between the support-folder violation check and the `plan $operation` call.
- `next-plan/SKILL.md` step 4: the Design item 3 addition, kept to the fewest sentences that state the finding duty and the citation rule, in the paragraph beginning "The brief also has the preparation `implementer` apply". Step 10: one sentence in the paragraph beginning "Success removes only direct-child dependency edges".
- Every line of a live Plan, other than this Plan, that names a deleted Plan file name at claim time: rewritten per Design item 1, together with any decisions in the same Plan that a resolved conditional changes.

## Out of scope

- `WorktreeCli` `plan complete`/`plan reject` and `plan validate`: no C++ change and no new validation rule.
- `Test-PlanCitations.ps1` and the `/plan-audit` advisory-lead policy.
- Deletions made outside `Complete-NextPlan.ps1` (a user's direct commit); the item-1 cleanup handles past ones.
- Bare names without `.md` that describe a landed change ("the landed GameShellIntoGameBase change"), `Documents/Investigations/`, `Documents/Features/`, and `AGENTS.md` files.
- Any change to a citing Plan's design beyond what resolving a stale conditional or ordering requires.

## Acceptance criteria

- Rebuilding the deleted-name list as in Design item 1, no live Plan other than this one contains any of those names outside line 1.
- Each rewritten site states a fact the current tree confirms, or is removed. No `## Coordination` bullet orders against a Plan that is not live.
- This Plan's own claim exit runs `Complete-NextPlan.ps1` to `code: ok`, since no live Plan names this Plan. Review of the script diff confirms that the blocking path runs before any change and reports `completion.plan-cited`.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports `status: valid`.

## Notes

- A citing Plan claimed and completed before this one takes its sites with it. A citation added after this Plan's creation is in scope by the acceptance grep.
- This Plan names deleted Plans as evidence. It is excluded from its own acceptance grep and leaves the tree when it completes.
