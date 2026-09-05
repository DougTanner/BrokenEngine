---
name: add-collection
description: >-
  Add a dynamically allocated Structure-of-Arrays collection to the engine or
  game frame system. Use for new frame entity, projectile, light, audio, or
  effect types; new Collection<T> structs; FrameBase.h or game Frame.h
  collection registration; and ForEach phase participation. Also use
  proactively whenever implementation creates a struct derived from
  Collection<T>.
allowed-tools: [Read, Edit, Write, Glob, Grep, Bash, PowerShell]
---

# Add a Collection

## Purpose

Add paired Interpolate/PostRender storage without breaking element counts, tuple
order, serialization, deterministic CRCs, save/replay versions, or build
affinity.

## When to use

- An enclosing `/implement-plan` implementation creates a `Collection<T>`
  struct or adds a new frame collection pair.
- Apply this specialist checklist in that implementer's context; do not
  dispatch a separate collection worker.

## Inputs

Require the approved change and exact collection variant, the collection name
and ownership, all known frame/phase/integration sites, the session baseline,
acceptance criteria, and the approved save/replay version and compatibility
decision. The enclosing implementer's brief supplies those values and its
ownership snapshot.

## Handoff

Fold this specialist's results into the enclosing `/implement-plan` shared
handoff rather than returning a second envelope:

```text
Runtime acceptance requests: <setup, action, observation, and required evidence per criterion, or none>
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <none>
Changed files: <each collection declaration, implementation, registration, version, transfer, hydration, query, or project-membership region>
Decisive checks: <collection-layout auditor and focused searches/reads>
Build required: <every consuming client/server target, configuration/platform, and selected project-member .cpp; every consuming target for changed headers>
Evidence: <focused check output or path plus selector, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Self-audit resolved: <collection-specific Claim -> Check -> Result rows>
Affected-site triggers: <collection/layout/version/transfer/query pattern and search scope>
Propagation required: /update-affected-code — <C++ scope>
Reviewer focus areas: <tuple ordering, shared/client subsets, lifecycle, version, or identity invariants>
Residuals: <stale exemplar, unresolved compatibility decision, or none>
```

This is the enclosing implementer's complete shared handoff from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
with the `/implement-plan` extension fields and collection runtime requests;
`Residuals` remains last. Main owns the returned build and runtime requests at
their Change Workflow stages.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the variant, wiring,
  version, and verification steps, and the rules.
