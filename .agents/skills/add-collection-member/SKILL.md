---
name: add-collection-member
description: >-
  Add a Structure-of-Arrays member pointer to an engine or game Collection without breaking allocation, persistence, serialization, CRC, transfer, hydration, or identity behavior. Use when adding a field, member, or data column to a collection, and proactively whenever an implementation adds a `* __restrict` pointer to a Collection struct. Follow the complete layout-change checklist even when the request names only the declaration.
allowed-tools: [Read, Edit, Bash, PowerShell]
---

# Add a Collection Member

## Purpose

Add one SOA member pointer to an existing Collection with every layout-dependent
site — tuple, version, CRC, persistence, creation, transfer, hydration, identity
— updated.

## When to use

- An enclosing `/implement-plan` implementation adds a field, member, or data
  column to an existing Collection.
- Apply this specialist checklist in that implementer's context whenever a
  `* __restrict` pointer is added; do not dispatch a separate member worker.

## Inputs

Require the approved change, target collection and member, intended ownership
and shared/client reachability, known producers, consumers, lifecycle and
integration sites, the session baseline, acceptance criteria, and the approved
save/replay version and compatibility decision. The enclosing implementer's
brief supplies those values and its ownership snapshot.

## Handoff

Fold this specialist's results into the enclosing `/implement-plan` shared
handoff rather than returning a second envelope:

```text
Runtime acceptance requests: <setup, action, observation, and required evidence per criterion, or none>
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <none>
Changed files: <each declaration, tuple, version, CRC, persistence, initialization, transfer, hydration, identity, or query region>
Decisive checks: <collection-layout auditor and focused producer/consumer traces>
Build required: <every consuming client/server target, configuration/platform, and selected project-member .cpp; every consuming target for changed headers>
Evidence: <focused check output or path plus selector, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Self-audit resolved: <member-specific Claim -> Check -> Result rows>
Affected-site triggers: <member/layout/version/CRC/lifecycle pattern and search scope>
Propagation required: /update-affected-code — <C++ scope>
Reviewer focus areas: <tuple position, carry-forward, initialization, transfer, hydration, or identity invariants>
Residuals: <stale exemplar, unresolved CRC/version/compatibility decision, or none>
```

This is the enclosing implementer's complete shared handoff from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
with the `/implement-plan` extension fields and member runtime requests;
`Residuals` remains last. Main owns the returned build and runtime requests at
their Change Workflow stages.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the layout-change steps
  and the rules.
