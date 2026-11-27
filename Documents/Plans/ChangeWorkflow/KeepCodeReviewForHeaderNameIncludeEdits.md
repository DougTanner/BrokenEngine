<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T03:02:13.256Z","dependsOn":[]} -->
# Keep code review owed for an edit inside an angle-bracket include

## Context

A proven comment-text-only C++ or GLSL edit owes no code review key
(`.agents/scripts/Get-ChangeWorkflowSteps.ps1` header; the round inventory in
`.agents/scripts/SessionChangeInventory.psm1` skips such a modified path where
it calls `Test-InventoryCommentOnlyChange`). That function tokenizes both
sides with `Get-AgentCppTokenPattern`
(`.agents/scripts/AgentScriptCommon.psm1`) and calls the change comment-only
when every non-comment token is byte-identical and each comment pair holds as
many line feeds.

The pattern has no alternative for a header name. In `#include <.//version>`
the `//` inside the angle brackets starts a `comment` token running to the end
of the line, so an edit to the rest of that line, such as changing
`<.//ShaderRandom.h>` to `<.//ShaderLayoutsBase.h>`, compares as a comment
edit and the round owes no code review for it. Quoted includes are not
affected, because the pattern's string-literal alternative consumes
`"..."` whole. No tracked file uses such a spelling today.

Observed symptom: an adversarial review reported, for a GLSL fragment shader,
that `glslc` from the installed Vulkan SDK compiled the two include spellings
above into different output (a fragment value of 0.75 against 0.25), and that
with the comment-only classification the step script owed only
`/comment-review` and the landing check passed after that round closed.

Unverified: whether MSVC 14.51 with `/Zc:preprocessor` loads a header for a
header name containing `//`. WG21 N4950 [lex.header] makes `//` in a header
name conditionally-supported with implementation-defined semantics, and
Microsoft Learn's `#include` directive page documents MSVC's include search.
If MSVC rejects the spelling, the C++ case is a compile error a build catches,
not a silent bypass.

## Design

1. Confirm the reach first: re-run the GLSL observation with `glslc` on two
   scratch shaders differing only in the angle-bracket include operand, and
   check the MSVC behavior against the two references in `## Context` (by
   `/verify-external-claims` for the documented behavior, and a scratch
   compile when the documents do not settle it). Record both results.
2. If either compiler gives the two spellings different meaning, make the
   smallest fix inside `Test-InventoryCommentOnlyChange`. Recommended: return
   false when a comment token on either side starts on a `#include` line, so
   such an edit stays owed code review. A fix that needs
   `Get-AgentCppTokenPattern` itself changes its other consumers
   (`Invoke-StaticChecks.ps1`, `Find-SessionCandidates.ps1`) and returns for
   re-planning instead.
3. If neither compiler gives them different meaning, complete with no change
   and record why.

Change Workflow tier: Tier 2 — tightening a check inside one unit
(`Test-InventoryCommentOnlyChange`) at an existing boundary, with the format
and the trust unchanged. No C++, shader, build, `.pack`, replay, or
live-verification exposure in the change itself.

## Critical files

- `.agents/scripts/SessionChangeInventory.psm1`
  (`Test-InventoryCommentOnlyChange`)

## In scope

- The investigation in Design 1, on scratch files under `Temp/`.
- `Test-InventoryCommentOnlyChange` and its comment.

## Out of scope

- `Get-AgentCppTokenPattern` and its other consumers.
- Other comment-only classification rules (splices, line-feed counts).
- Tracked shader or C++ sources.

## Acceptance criteria

- The executing session records the `glslc` and MSVC results of Design 1.
- When a fix is made: in a scratch repository, a round after an edit that
  changes only the text after `//` inside an angle-bracket include of a GLSL
  or C++ file owes the code review key for that file type, and an edit to a
  `//` comment on any other line still owes none.
- The static-checks runner passes for the changed files.
