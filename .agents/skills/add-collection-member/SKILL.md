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

The enclosing implementer's brief supplies the task-brief fields in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
and its ownership snapshot. This checklist also consumes the target collection
and member, the intended ownership and shared/client reachability, the known
producers, consumers, lifecycle and integration sites, and the approved
save/replay version and compatibility decision.

## Handoff

This specialist returns no envelope of its own; its results fold into the
enclosing [`/implement-plan`](../implement-plan/SKILL.md) handoff. Name every
consuming client/server target under `Build required`, one runtime-observable
criterion per `Runtime acceptance requests` row, the tuple-position,
carry-forward, initialization, transfer, hydration, and identity invariants
under `Reviewer focus areas`, and a stale exemplar or unresolved
CRC/version/compatibility decision under `Residuals`.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the layout-change steps
  and the rules.
