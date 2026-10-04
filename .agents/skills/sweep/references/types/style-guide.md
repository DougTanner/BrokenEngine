# Sweep type: style-guide

Brings C++ files up to the style guide rules the Change Workflow checks.
`Documents/C++StyleGuide.txt` is the authority for every rule; the user rulings
below are the only additions.

## Find rules

Rule set: the `/code-style-review` mandate — the scanner's `style-rule-<n>`
kinds plus the hand-read list and permitted forms in
`.agents/skills/code-style-review/references/worker.md` steps 7 and 10 — and
rule 64 as `.agents/skills/comment-review` applies it.

Procedure:

1. Run the scanner's whole-file mode on exactly the unit files, from the
   repository root, with the repository root and the scanner path arguments the
   prompt states:
   `pwsh -NoProfile -Command "& '.agents/scripts/Find-SessionCandidates.ps1' -RepositoryRoot '<repository root>' -Path <scanner path arguments>"`.
   Adjudicate every row it returns, `style-rule-17` rows included, against
   `Documents/C++StyleGuide.txt`: a scanner row is a candidate, not a verdict.
   If the scanner cannot run, say so in the output and continue by hand.
2. Hand-read every line of every unit file for the rest of the rule set.

User rulings (never report these as findings):

- `using namespace std::chrono_literals;` stays as is.
- The DirectXMath-style `XM_PIDIV*` constants in `Common/ExternalHeaders.h`
  keep their `XM_` names.
- A struct member of a Vulkan `Vk*` type uses the plain `vk` prefix
  (`TextureHeader::vkFormat`).
- An enum whose values are used as indices or integer ids and which ends in a
  `k...Count` enumerator (e.g. `common::Threads`) is an index-and-count enum
  under rule 55 and stays a plain `enum`.
- Rule 62: conditions that together validate one object's related fields with
  the same exit (e.g. a texture's width, height and mip count against limits)
  are one check and stay in one `if`.
- A Win32 file API call (`DeleteFileW`, `MoveFileExW`, etc.) is never replaced
  with `std::filesystem`, overriding rule 35: their error and attribute
  semantics differ, so such a finding is `out-of-bound`.
- A declaration that must match a system or SDK declaration (e.g. `operator new`
  replacements with `_Success_(return != NULL)` matching `vcruntime_new.h`)
  keeps its annotation text verbatim: `NULL` inside a SAL annotation stays,
  overriding rule 28 for that text.

## Fix bound

Wider than `/code-style-review`'s meaning-preserving bound (its worker steps
11-12): a fix may change a type, container, signature, overload choice,
error-handling path, or `kb*` toggle, provided observable behavior is
unchanged. Accepted examples: removal of trivial accessors and forwarders
(rule 49), pointer-and-count parameters replaced by `std::span`, and long-form
renames.

A finding is out of bound, and left unfixed, when its fix would change any of:
sim output or the per-tick CRC; serialized, save, replay, wire, or `.pack`
bytes; threading; or what a trust-boundary check accepts.

## Cleanup checklist

Regressions found in earlier sweeps:

- An integer changed to `size_t` instead of `int64_t` with `std::ssize()` (a
  `std::span` extent or other API consumer keeps `size_t`).
- A `d`-prefixed double.
- A lowercase lambda or `std::function` variable (an immediately invoked
  initializer lambda's variable holds its result and is exempt).
- A member rename applied to a DirectXMath type that has no such member.
- A one-line braced list split across lines.
- An anonymous namespace reintroduced (rule 66).
- A named namespace's closing comment removed (rule 65).

## Deferred fixes

Record: `Documents/Investigations/ChangeWorkflow/StyleGuideSweepDeferredFixes.md`
