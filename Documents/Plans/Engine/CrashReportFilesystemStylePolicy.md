<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:04:52.547Z","dependsOn":[]} -->
# Resolve crash-report filesystem style policy

## Context

Two pre-existing rule 35 style candidates in Engine/Source/CrashReport.cpp
cannot be mechanically replaced without examining early-startup guarantees.
ResolveCrashReportPaths:148-152 explicitly creates the report directory without
allocator use before FileManager exists. RenamePreviousCrashReport:172-183 reads
metadata, returns on failure or a two-word zero file size, then renames with
MOVEFILE_REPLACE_EXISTING. This is unresolved policy debt, not a proven defect.

The author records that the user deferred the exception-versus-replacement
decision into follow-up work. Neither option was approved; the active cleanup
preserves both calls and Documents/C++StyleGuide.txt rule 35.

## Design

Return the choice to the user before edits: a narrow rule 35 exception for the
startup operations, or a separately proven replacement preserving the relevant
allocation and error contracts. Do not choose in this Plan. First establish the
metadata operation's actual startup allocation constraints from its callers;
do not infer that an error_code overload proves freedom from allocation.
After approval, prepare and review the smallest implementation; return any
broader redesign for re-planning. Until then preserve all existing guarantees.

## Critical files

- Engine/Source/CrashReport.cpp: ResolveCrashReportPaths and
  RenamePreviousCrashReport, specifically CreateDirectoryW:152 and
  GetFileAttributesExW:173.
- Documents/C++StyleGuide.txt: rule 35.

## In scope

Resolve exactly these two candidates and their precisely approved policy text.
Preserve allocator-free single-level directory setup before FileManager,
ignored creation failure including an existing directory, precomputed fixed
path buffers and fallbacks, the mismatched-path early return, metadata failure
and zero-size early exits, and MOVEFILE_REPLACE_EXISTING rename behavior.
No API substitution is authorized by creating this Plan.

## Out of scope

Crash upload, logging/mutex initialization, exception handling redesign, report
format changes, unrelated filesystem operations, new startup allocation, unit
tests, or general relaxation of rule 35.

## Acceptance criteria

1. Record explicit user approval of the policy for both calls before edits.
2. Review every listed invariant against the final diff. A replacement requires
   evidence for its allocation behavior and unchanged missing-file, zero-size,
   creation-failure, and rename-failure outcomes; a narrow exception preserves
   source behavior and applies only to the approved operations.
3. Run targeted static checks and applicable Change Workflow reviews. Build
   affected targets through /compile if C++ changes. Use /agent-harness for any
   live crash-report verification. The originating scan supplied no runtime
   reproducer; do not invent a new harness command solely to reach a rare path.

## Notes

Future implementation is Tier 2 for scoped crash-report startup behavior;
reclassify before implementation if the approved choice exposes threading,
trust, or independently owned subsystem integration. No deterministic state,
CRC, replay, wire, packed-data, or serialization change is intended.
This records deferred debt under explicit user direction despite the usual
decision-complete Plan rule. Creation does not authorize either implementation.

## Coordination

No dependencies or shared implementation boundary with existing Plans were found.
