<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:21:48.407Z","dependsOn":[]} -->
# Use direct child indices in scene vertex traversal

## Context

`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:289` defines `LoadVertices`. Its child traversal at line 296 uses a sequence counter only to read `rNode.children[i]` twice in the recursive call. `ThirdParty/tinygltf/tiny_gltf.h:1012` declares `Node::children` as `std::vector<int>`. A range-based loop names the actual child node index and removes irrelevant sequence indexing without allocation or meaningful copying cost.

Both `rNode` and `rModel` are const references. Recursion writes the separate `LoadVerticesContext`; it does not mutate the input child range. The named range outlives the loop. The existing primitive loop at line 318 needs its sequence index for diagnostics and `BuildVertices`, so it remains indexed.

Style rules 8 and 15 in `Documents/C++StyleGuide.txt` already prefer range-based traversal when possible and require explicit range element types. No style-guide, AGENTS.md, serialization-version, or architecture-document amendment is warranted. The existing `SceneTextureLookupIteratorDeduction.md` Plan owns only `UsesTexture` in the same source file; its scope is independent of this loop.

## Design

Replace the child loop in `LoadVertices` with:

```cpp
for (int iChild : rNode.children)
{
	Parent parent {pParent, matNode, iCurrentNodeIndex};
	LoadVertices(&parent, iChild, rModel.nodes[iChild], rModel, rContext);
}
```

Change only the loop header and the two child-index expressions in the recursive call. `int` exactly matches the child element type and the recursive function's node-index parameter. Copying that scalar into the loop variable does not copy a node or a container.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` — `LoadVertices` child loop, sole implementation edit.
- `ThirdParty/tinygltf/tiny_gltf.h` — read-only evidence for `Node::children` element type.
- `Documents/C++StyleGuide.txt` — read-only rules 8 and 15.

## In scope

Only replace the `rNode.children` indexed traversal in `LoadVertices` with `for (int iChild : rNode.children)` and substitute `iChild` for both `rNode.children[i]` occurrences in that loop's recursive call.

## Out of scope

The `rMesh.primitives` loop, all other traversals and declarations, helper extraction, validation changes, recursion restructuring, ThirdParty edits, format or version changes, new tests, documentation/style amendments, and any repository-wide range-loop adoption policy.

## Risk and invariants

Future implementation is Tier 1: a local mechanical, behavior-preserving traversal rewrite with no public signature or invariant exposure.

- Visit each child exactly once in the existing vector order, including the unchanged behavior for an empty child list.
- Pass the same child index and corresponding `rModel.nodes` element to each recursive call.
- Keep `Parent parent` inside each iteration with identical initialization and lifetime spanning its synchronous recursive call.
- Preserve traversal before the mesh check, parent transforms, context writes, exported bytes, and diagnostic behavior.
- Preserve immutable input traversal and introduce no allocation or additional container/node copy; complexity remains linear in child count.
- Preserve the primitive loop's index, validation, and `BuildVertices` arguments.

## Acceptance criteria and verification

1. Inspect the diff: only the child-loop header and its two indexed child expressions change, exactly as specified above.
2. Review `Node::children`, the const input parameters, and the recursive body to establish equal child values/order, stable range lifetime, unchanged `Parent` lifetime, and no input mutation or allocation. The diff and type evidence settle behavioral and performance equivalence; no benchmark is needed.
3. Through `/compile`, build DataPacker Release successfully after implementation. No client/server or AgentTools build is required.
4. Complete the required C++ review and cleanup workflow within this scope. No asset export or `/agent-harness` run is required because the local traversal equivalence is decisive. Do not add unit tests.

These checks belong to future implementation; no build or runtime verification was performed during Plan authoring.
