<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T16:40:44.712Z","dependsOn":[]} -->
# Correct the mCoordFrames iteration claim in FloatingPointDeterminism.txt

## Context

`Documents/FloatingPointDeterminism.txt:104-105`, in section "7. Container
Iteration Safety", states:

```
  - mCoordFrames: iterated only for rendering (BeginRender), with a
    deterministic rActiveCoords vector providing the iteration order.
```

"iterated only for rendering" is false. The server iterates `mCoordFrames`
directly in two non-render places while computing the active set:

- `Engine/Source/Network/Server/ServerSessionRuntime.cpp:299-309`
  (`SyncActiveFrames`) walks the map with the iterator-safe `it = erase(it)`
  form to drop every frame whose coord is absent from `mActiveCoords`, calling
  `ServerSession::OnFrameRetiring` before each erase.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:318-330`
  (`AddGameRequiredCoords`, the game hook called from
  `ServerSessionRuntime::ComputeActiveSet`) range-iterates the map and appends
  every player-holding coord to `mActiveCoords`.

Pre-existing: at session baseline `45f2d1d0` the same two loops lived in
`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp` at `:339`
(`for (const auto& [rCoord, rFrames] : gpGame->mCoordFrames)`) and `:363` (the
erase loop). The `Documents/Plans/Network/ActiveSetSkeletonToEngine.md` move
relocated those loops without changing what they iterate, and that Plan's
`## In scope` covers only the two `Network/Server/AGENTS.md` hub sentences, so
this determinism document is outside its approved boundary.

## Design

Rewrite that one bullet so it lists the real iteration sites and states, in the
section's existing "why hash order is safe" style, the property each relies on:

- Rendering (`BeginRender`) iterates in the order of the deterministic
  `mActiveCoords` vector, exactly as the current text says.
- Server active-set computation iterates the map itself, in unspecified hash
  order, in `SyncActiveFrames` (the erase pass) and in the
  `AddGameRequiredCoords` game hook. Hash order is safe there because both loops
  decide set membership rather than compute a value: the erase pass removes
  exactly the entries whose coord is absent from `mActiveCoords`, and the hook
  appends the same deduplicated coords whatever the visit order. The resulting
  order of `mActiveCoords` affects only the dispatch order of per-cell ticks,
  which are independent by construction — simulation code samples only its own
  cell's `FrameStaticData` and never `mCoordFrames`
  (`Engine/Source/Frame/AGENTS.md:27`).

Exact wording is the implementer's; the content above is the required content.
No code changes and no other document changes.

## Critical files

- `Documents/FloatingPointDeterminism.txt:98-110` — section 7's `mCoordFrames`
  bullet, the only region to change

## In scope

- The `mCoordFrames` bullet of section "7. Container Iteration Safety" in
  `Documents/FloatingPointDeterminism.txt`

## Out of scope

- Any C++, shader, or project-file change
- The `Collision::sResults` and `mFrameInputs` bullets and every other section
  of the same document
- `Engine/Source/Network/Server/AGENTS.md` and the game
  `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` hub text
- Auditing or changing the actual iteration-order behavior of the server active
  set

## Risk tier and invariants

Change Workflow Tier 1 — documentation only, no public signature, no invariant
exposure, no behavior change. The corrected text must remain true against the
cited `path:line` sites; re-read both before editing, because a sibling
active-set move Plan may shift those line numbers.

## Acceptance criteria

- The bullet no longer claims render-only iteration, names both server iteration
  sites with their current paths, and states why unspecified hash order is safe
  at each.
- Every `path:line` the new text cites resolves to the loop it names in the
  working tree at implementation time.
- No file other than `Documents/FloatingPointDeterminism.txt` changes.
