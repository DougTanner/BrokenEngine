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

## References

- [`references/worker.md`](references/worker.md) — worker entry: the
  layout-change steps and the rules.
