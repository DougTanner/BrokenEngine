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

The enclosing implementer's brief supplies the task-brief fields in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
and its ownership snapshot. This checklist also consumes the exact collection
variant, the collection name and ownership, all known frame/phase/integration
sites, and the approved save/replay version and compatibility decision.

## Handoff

This specialist returns no envelope of its own; its results fold into the
enclosing [`/implement-plan`](../implement-plan/SKILL.md) handoff. Name every
consuming client/server target under `Build required`, one runtime-observable
criterion per `Runtime acceptance requests` row, the tuple-ordering,
shared/client subset, lifecycle, version, and identity invariants under
`Reviewer focus areas`, and a stale exemplar or unresolved compatibility
decision under `Residuals`.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the variant, wiring,
  version, and verification steps, and the rules.
