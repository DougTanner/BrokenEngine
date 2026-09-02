<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-02T12:04:56.934Z","dependsOn":[]} -->
# Trim `Engine/Source/Network/Client/AGENTS.md` back under the leaf token target

## Context

`Engine/Source/Network/Client/AGENTS.md` measures 2434 `bt-token-v1` tokens
against the 2,000-token leaf target, reported as `verdict: over-target` by

```
pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 Engine/Source/Network/Client/ClientReceive.cpp
```

which returns `{"path":"Engine/Source/Network/Client/AGENTS.md","kind":"leaf","tokens":2434,"target":2000,"verdict":"over-target"}`
in its `sizes` items. The same run reports the doc's chain
(`AGENTS.md`, `Engine/Source/AGENTS.md`, `Engine/Source/Network/AGENTS.md`,
`Engine/Source/Network/Client/AGENTS.md`) at 14220 tokens with chain
`verdict: ok`, so the leaf itself is the only over-target document, and every
session touching client networking pays that excess.

The overage is pre-existing: it reproduces at the session baseline commit
`3ea3da656b8a53bcf5190c43d9ebfd1cce2bed12` and is unrelated to the
`ClientTimeScaleValidation` guard change that observed it. The bulk sits in
`## Subscription Receive Invariants` (`Engine/Source/Network/Client/AGENTS.md:15-24`),
whose bullets run to full paragraphs of receive-path mechanics, and in the
similarly long bullets at lines 40-41 and 45-46.

The root `AGENTS.md` progressive-disclosure directive says each fact lives once
at its owning layer: an AGENTS.md carries constraints, invariants, and routing,
while mechanics and long detail belong in code comments, `references/`, or a
child document.

## Design

Recommended approach: apply progressive disclosure to this leaf rather than
compressing wording alone. For each over-long bullet, keep the invariant and
the routing sentence in the leaf, and move the step-by-step mechanics to the
layer that owns them — a local comment at the implementing site in
`Engine/Source/Network/Client/*.cpp/.h`, an existing linked document such as
`Documents/Architecture/Network.md` or
`Documents/Architecture/GameReconciliation.md`, or a new child document under
`Engine/Source/Network/Client/` if a block is too large for either.

Do not prescribe which bullets to cut ahead of the implementing session: the
choice depends on which mechanics already have an owning code site. The binding
outcome is that no invariant currently stated in the leaf may be lost,
weakened, or made unreachable — every relocated fact must remain findable from
the leaf through an explicit link or a named code site.

Rewording alone is a secondary option and is not expected to reach the target
on its own; relocation is the author's recommendation because it also removes
the duplication that made the doc grow.

## Critical files

- `Engine/Source/Network/Client/AGENTS.md` — the over-target leaf; lines 15-24,
  40-41, 45-46 hold the long mechanics bullets.
- `Engine/Source/Network/Client/CLAUDE.md` — sibling import stub; must keep
  matching the leaf.
- `Engine/Source/Network/Client/ClientReceive.cpp`,
  `ClientSessionRuntime.cpp`, `ReconcileReplay.cpp`, `ClientDesyncCore.cpp` —
  candidate owning code sites for relocated mechanics comments.
- `Documents/Architecture/Network.md`,
  `Documents/Architecture/GameReconciliation.md` — already-linked documents
  that may own relocated protocol and reconciliation detail.

## In scope

- Rewriting prose in `Engine/Source/Network/Client/AGENTS.md` (any section) so
  the file measures at or below the 2,000-token leaf target.
- Adding relocated mechanics as comments at the owning code sites listed above,
  or as prose in the already-linked architecture documents, or in one new child
  document under `Engine/Source/Network/Client/`.
- Updating `Engine/Source/Network/Client/CLAUDE.md` only if the leaf's import
  stub contract requires it.
- Updating the parent `Engine/Source/Network/AGENTS.md` only where a link must
  point at a newly created child document.

## Out of scope

- Any executable behavior change: no C++ statement, signature, control flow, or
  data change; comment-only edits at the code sites named above.
- Other AGENTS.md files reported `ok` by the measurement above, including the
  root and the `Engine/Source` and `Engine/Source/Network` hubs.
- The `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md` game-side
  document.
- Changing the token target itself, the measurement script, or
  `/update-claude-docs`.

## Risk tier and invariants

Expected Change Workflow Tier 1 (mechanical documentation and behavior-
preserving comment edits, no public signature or invariant exposure). The
trigger is documentation reorganization only. Escalate to Tier 2 if the
implementing session finds a leaf statement that documentation cannot preserve
without a code change.

No determinism/CRC, serialization, `.pack`/`kiVersion`, replay, wire, threading,
allocation, shader, or project-membership exposure. If any comment-only edit
touches a `.cpp`/`.h` file, the change still needs a compile of the affected
project to prove the comment edits did not break the build.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 Engine/Source/Network/Client/ClientReceive.cpp`
  reports `Engine/Source/Network/Client/AGENTS.md` with `verdict: ok` and
  `tokens` at or below `2000`, and reports no other document newly
  `over-target`.
- A reviewer maps every invariant stated in the baseline leaf to its location
  after the change — still in the leaf, or in a named code comment, linked
  architecture document, or child document reachable from the leaf — with none
  dropped or weakened.
- `/progressive-disclosure-review` passes on the changed instruction prose with
  no finding that a fact now lives in two layers or that the leaf lost routing
  a session needs.
- If any `.cpp`/`.h` file changed, the affected project builds clean.

## Notes

Observed while completing `Documents/Plans/Engine/ClientTimeScaleValidation.md`;
that Plan's change is unrelated and this document depends on nothing.
