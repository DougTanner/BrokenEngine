<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:04:51.251Z","dependsOn":[]} -->
# Resolve file-only deletion style policy

## Context

Repository-wide style cleanup found eight existing DeleteFileW calls conflicting
with Documents/C++StyleGuide.txt rule 35's std::filesystem preference. This is
policy debt, not a proven deletion bug. The author records that the user deferred
the exception-versus-replacement decision into follow-up work; neither option
was approved. The active cleanup leaves these calls and rule 35 unchanged.

DeleteFileW refuses ordinary directories, while std::filesystem::remove also
removes empty directories. A path attribute precheck cannot preserve atomic
file-only refusal under path replacement. Sources: Microsoft's
[DeleteFileW remarks](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilew#remarks)
and the standard's [remove specification](https://eel.is/c++draft/fs.op.remove).

## Design

First return the policy choice to the user: a narrow rule 35 exception preserving
the Windows operation, or a replacement whose complete behavior is explicitly
approved. This Plan deliberately does not choose. Until approval, preserve every
existing guarantee and make no implementation or rule 35 change. After approval,
prepare and review the smallest implementation; surface any needed broader scope
for re-planning. Do not describe std::filesystem::remove as equivalent.

## Critical files

- Documents/C++StyleGuide.txt: rule 35.
- Tools/ToolCommon/CoordinationStore.cpp:84,322.
- Tools/AgentHarness/HarnessLockCommands.cpp:232.
- Tools/WorktreeCli/LandingLockCommands.cpp:240.
- Tools/WorktreeCli/BuildCommand.cpp:582-589.
- Tools/WorktreeCli/PlanScheduler.cpp:264,890,1034.

## In scope

Resolve only the eight calls listed above and any precisely approved rule 35
exception for them. Preserve these source-observed contracts unless the future
approval explicitly resolves a requested change:

- CoordinationStore's Guard destructor and WriteBytesAtomic ignore cleanup
  failures; no throwing cleanup or new failure exit.
- Harness and landing-lock release treat failed deletion as failure, including
  disappearance after locating the file.
- PlanScheduler's HealClaims appends a healed claim only after successful
  deletion; RunUnclaim and completion/rejection fail if their deletion fails.
  Preserve completion's child-rewrite-before-target-delete ordering.
- InvalidateSelectedObjects accepts ERROR_FILE_NOT_FOUND and
  ERROR_PATH_NOT_FOUND but fails other deletion errors.
- Preserve file-only refusal, ExtendedLengthPath usage, existing JSON diagnostic
  categories, owner checks, guard release ordering, and coordination lifetimes.

## Out of scope

Other filesystem calls, global rule relaxation, logging initialization policy,
new abstractions, unit tests, compatibility, and changes to claims or build
coordination protocols beyond these operations.

## Acceptance criteria

1. Record an explicit user-approved policy for all eight calls before editing.
2. Review the final diff against every per-site contract above. An exception
   changes only its narrow policy text; a replacement has evidence for ordinary
   files, empty directories, missing paths, access failures, and concurrent path
   replacement, including any explicitly approved semantic change.
3. Run targeted static checks and the applicable Change Workflow reviews. Build
   changed C++ targets through /compile; never use live coordination data for
   deletion probes. No runtime reproducer was supplied by the originating scan.

## Notes

Future implementation is Tier 3: independently owned AgentHarness, ToolCommon,
and WorktreeCli integration plus build/bootstrap coordination that can block
other sessions. No simulation/CRC, replay, wire, or packed-data change is intended.
This records deferred debt under explicit user direction despite the usual
decision-complete Plan rule. Creation does not authorize either implementation.

## Coordination

No dependencies or shared implementation boundary with existing Plans were found.
