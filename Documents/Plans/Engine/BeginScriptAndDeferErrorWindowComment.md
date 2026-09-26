<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T16:45:39.917Z","dependsOn":[]} -->
# Correct the BeginScriptAndDefer errorWindow comment

## Context

The comment above `BeginScriptAndDefer` in
`Engine/Source/Agent/AgentCommandsClientGeneric.cpp` (line 842 at the time of
writing) says:

`// For label-based scripts pcErrorWindow (may be null) drives the candidate list on a not-found / ambiguous error.`

The function has no `pcErrorWindow` parameter. The parameter is
`std::string errorWindow` (line 843), a value that cannot be null. The lambda
inside the function passes `errorWindow.c_str()` to `CandidateLabels` only when
`bHasWindow` is true and passes `nullptr` otherwise (lines 863 and 867). The
comment gives the wrong parameter name and claims the value can be null, which
breaks the false-claim rule in `Documents/C++StyleGuide.txt` rule 64.

The comment already said this at baseline `d4079189`, where line 840 had the
same text above a signature that also took `std::string errorWindow`. The
`/comment-review` run for `Documents/Plans/Engine/GameBaseBackReferenceRemoval.md`
found it. That Plan's `## Out of scope` excludes other edits, so the fix was
left for a separate change.

## Design

Recommended: rewrite the second comment line so it names `errorWindow` and says
it limits the candidate list to that window only when `bHasWindow` is set. This
matches the code, and rule 64 treats a false claim as a comment defect. The
first comment line is correct and stays as it is.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` — the comment block
  directly above `BeginScriptAndDefer`.

## In scope

- The second line of the comment directly above `BeginScriptAndDefer` in
  `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`.

## Out of scope

- Any code change, including the signature, the parameters, and the lambda body
  of `BeginScriptAndDefer`, and its callers.
- Any other comment in the file.

## Risk tier

Tier 1 (mechanical). The change touches only one comment and changes no
behavior, signature, or invariant. It has no determinism/CRC, wire,
serialization, replay, threading, affinity, or build exposure.

## Notes

- Line numbers are from when this Plan was written. Find the comment by the
  `BeginScriptAndDefer` symbol.
- No live verification is needed. The diff settles it.
