<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T17:54:58.746Z","dependsOn":[]} -->
# Deduce the scene texture lookup iterator

## Context

`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:369` defines the anonymous-namespace helper `UsesTexture(const tinygltf::ParameterMap& rParameters, const char* pcTextureName, int64_t iIndex)`. Its local declaration repeats the map's full iterator type before a single guarded lookup result. `ThirdParty/tinygltf/tiny_gltf.h:515` defines `ParameterMap` as `std::map<std::string, Parameter>`. The const map's `find` returns exactly the existing `const_iterator` type, so value deduction removes repetition without changing semantics or performance.

`Documents/C++StyleGuide.txt` rule 15(c) already permits iterator `auto`. No documentation or style-policy amendment is warranted: this application changes no contract. A search of existing Plans for `UsesTexture`, `SceneVerticesLoader`, and `ParameterMap` found no owning Plan.

## Design

In `UsesTexture`, replace only the explicit type in this declaration:

```cpp
tinygltf::ParameterMap::const_iterator iterator = rParameters.find(pcTextureName);
```

with:

```cpp
auto iterator = rParameters.find(pcTextureName);
```

Keep the initializer and following short-circuit return expression unchanged. Plain `auto` deduces the same value iterator type; do not introduce a reference, add `const`, or alter the lookup.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` — sole implementation edit, `UsesTexture` local declaration.
- `ThirdParty/tinygltf/tiny_gltf.h` — read-only evidence for `ParameterMap`.
- `Documents/C++StyleGuide.txt` — read-only rule 15(c) authority.

## In scope

Only replace `tinygltf::ParameterMap::const_iterator` with `auto` for the existing `iterator` local in `UsesTexture`.

## Out of scope

Other declarations, iterator renames, XMVECTOR or XMMATRIX conversions, multiple-declarator rewrites, helper restructuring, ThirdParty edits, new tests, export/version changes, and repository-wide deduction policies. Do not amend AGENTS.md or the style guide for this change.

## Risk and invariants

Future implementation is Tier 1: one mechanical, local behavior-preserving type spelling change, with no public signature or invariant exposure.

- Deduced type remains `tinygltf::ParameterMap::const_iterator`, including const access to the mapped value.
- Preserve one `find` call, comparison against `rParameters.end()`, and short-circuit protection before `TextureIndex()` access.
- Preserve texture classification results, iterator lifetime, allocation behavior, exported bytes, and runtime cost.
- No simulation, serialization, layout, threading, or trust boundary changes.

## Acceptance criteria and verification

1. Diff inspection shows the one local type spelling replacement and unchanged initializer and return expression.
2. Review the const parameter and `ParameterMap` alias to confirm that `auto` selects exactly the previous value iterator type; this establishes unchanged lookup behavior and cost without benchmarking.
3. Through `/compile`, build DataPacker Release successfully after the edit. No game, server, or tools build is needed.
4. Apply the required C++ review and cleanup workflow within the stated scope. No `/agent-harness` scenario or asset export is needed: the exact type and unchanged expressions settle runtime behavior. Do not add unit tests.

These are future implementation checks; no build or runtime verification has been performed while authoring this Plan.
