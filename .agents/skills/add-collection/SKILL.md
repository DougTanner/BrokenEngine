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

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the variant, wiring,
  version, and verification steps, and the rules.
