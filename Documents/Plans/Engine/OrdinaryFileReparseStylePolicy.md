<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:04:53.961Z","dependsOn":[]} -->
# Resolve ordinary-file reparse style policy

## Context

Engine/Source/File/FileManager.cpp:236 is a pre-existing rule 35 candidate.
ComputeOrdinaryFileSha256 checks GetFileAttributesW and rejects every
FILE_ATTRIBUTE_REPARSE_POINT or directory before opening. It then opens with
FILE_FLAG_OPEN_REPARSE_POINT and repeats rejection using the opened handle's
GetFileInformationByHandle result at lines 248-255. Portable symlink or
regular-file classification alone does not prove equivalent Windows reparse
rejection. This is policy debt, not evidence of an existing hashing defect.

The author records that the user deferred the exception-versus-replacement
decision into follow-up work. Neither option was approved; the active cleanup
preserves the call and Documents/C++StyleGuide.txt rule 35.

## Design

Return the choice to the user before edits: a narrow rule 35 exception for
Windows reparse metadata, or a replacement proven to reject every currently
rejected Windows reparse kind while retaining the handle-based validation.
This Plan selects neither. After approval, prepare and review the smallest
implementation; surface any necessary broader redesign for re-planning.
Preserve all guarantees until approval resolves the choice.

## Critical files

- Engine/Source/File/FileManager.cpp:233-255,
  FileManager::ComputeOrdinaryFileSha256.
- Documents/C++StyleGuide.txt: rule 35.

## In scope

Resolve only the GetFileAttributesW candidate at line 236 and its precisely
approved policy text. Retain invalid-attributes failure, directory and all-tag
reparse rejection, the open flags and sharing, handle-based second validation,
hash byte selection, and existing false-return/error behavior.

## Out of scope

Other filesystem calls, removing the second check, admitting new reparse kinds,
digest or publication formats, new hash APIs, unit tests, compatibility,
or a general rule 35 relaxation.

## Acceptance criteria

1. Record explicit user approval of the policy before editing.
2. Review the final diff against both rejection checks and unchanged hashing
   behavior. A replacement needs evidence covering all currently rejected
   Windows reparse kinds and path replacement between check and open; a narrow
   exception leaves those checks intact.
3. Run targeted static checks and applicable Change Workflow reviews; compile
   affected targets through /compile if C++ changes. No runtime reproducer was
   supplied by the scan. Use existing verification routes and /agent-harness
   for live engine operations, without inventing a command for rare paths.

## Notes

Future implementation is Tier 2 for scoped file admission behavior at the
existing boundary, preserving its trust policy. Reclassify if a future choice
changes what this boundary trusts. No CRC, replay, wire, packed-data, or
serialization change is intended.
This records deferred debt under explicit user direction despite the usual
decision-complete Plan rule. Creation does not authorize either implementation.

## Coordination

No dependencies or shared implementation boundary with existing Plans were found.
