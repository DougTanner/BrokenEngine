<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T22:42:41.732Z","dependsOn":[]} -->
# Fix: Propagate autosave commit results

## Context
The save path currently drops the result of committing the grid save. `GameSaveLoad.h:45` declares `Autosave` without exposing the existing write result, and `GameSaveLoad.cpp:123-140` discards the `WriteGridSave` bool. The shutdown path at `Main.cpp:463-481` consequently cannot observe or log a failed final autosave. This is the residual recorded by the removed `Documents/Investigations/Engine/AutosaveCommitResultPolicy.md` investigation.

The existing `FileManager` and `GridSave` diagnostics remain the source of detailed write errors. Existing log-file and agent log retrieval are the operator channels for those diagnostics. The gap is the missing propagation and policy observation at the periodic and shutdown call paths.

## Design
Recommend changing `Autosave` to return the existing `WriteGridSave` bool. The periodic caller should observe that result, always reset its timer after an attempt, and log qualified success or failure. The shutdown caller should observe and log failure, then continue the exit path. Keep the existing `FileManager`/`GridSave` diagnostics.

Do not introduce a richer result type. Do not add retries, a nonzero exit status, new UI/agent/protocol behavior, a test-only trigger, or atomic-writer/file-format changes.

## Critical files
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:45` — `Autosave` declaration and return contract.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:123-140` — `Autosave` implementation, periodic policy, and `WriteGridSave` result propagation.
- `Engine/Source/GameBase.cpp:433-452` — existing periodic server call path, which remains intact.
- `Engine/Source/Main.cpp:463-481` — shutdown autosave observation, logging, and exit continuation.
- `Engine/Source/File/GridSave.cpp:12-47` and `Engine/Source/File/FileManager.cpp:333-359` — existing returned commit result, detailed diagnostics, and atomic cleanup behavior, which remain unchanged.

## In scope
- Change the `Autosave` declaration and implementation so it returns the existing `WriteGridSave` bool.
- Update the periodic autosave call path to observe the bool, reset its timer after every attempt, and emit qualified success/failure logging.
- Update the shutdown call path to observe and log a failed autosave while continuing exit.
- Retain the existing `FileManager` and `GridSave` diagnostics.
- Verify the source call paths statically and run the server `Debug|x64` build.
- Verify at most one representative shutdown commit-failure scenario using existing facilities, proving returned `false` reaches the shutdown failure log and shutdown continues; observe prior destination/`.tmp` behavior only as naturally exposed by that scenario, and rely on static inspection for the unchanged `FileManager` atomic/cleanup contracts.
- Verify the periodic timer policy statically because there is no bounded trigger seam; do not add one.

## Out of scope
- Richer autosave result types, retries, or nonzero process exit status.
- New UI, AgentHarness command, or protocol behavior; existing log observation remains available for verification.
- A test-only trigger or other periodic-trigger seam.
- Atomic writer or save-file format changes.
- Changes outside the Save and Main call paths named above.
- Adding unit tests.

## Risk tier and invariants
Expected Tier 3 (invariant/integration): the result/signature contract crosses the independently owned Projects Save and Engine Main subsystems. Preserve deterministic simulation/CRC behavior, save compatibility, and existing write diagnostics; no wire/protocol, replay, `.pack`, or data-layout changes are intended. Preserve the shutdown exit path even when the autosave fails.

## Acceptance criteria
- `Autosave` returns the existing `WriteGridSave` result, and both periodic and shutdown call paths consume it at the named source locations.
- The periodic path always resets its timer after an attempt and logs qualified success/failure; this policy is settled by static source inspection because no bounded trigger seam exists.
- Shutdown logs autosave failure and continues exit.
- Existing `FileManager`/`GridSave` diagnostics remain intact.
- The server `Debug|x64` build passes.
- At most one representative forced shutdown commit-failure scenario using existing facilities proves returned `false` reaches the shutdown failure log and shutdown continues; prior destination/`.tmp` behavior is observed only as naturally exposed by that scenario, with the unchanged `FileManager` atomic/cleanup contracts covered by static inspection.
- No richer result, retry, nonzero exit, new UI/agent/protocol, test-only trigger, atomic writer, or file-format change is introduced.

## Notes
No dependencies. Origin: removed investigation `Documents/Investigations/Engine/AutosaveCommitResultPolicy.md`.
