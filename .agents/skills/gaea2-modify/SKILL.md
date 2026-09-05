---
name: gaea2-modify
description: Edit a Gaea 2 terrain previously loaded by /gaea2-load — add/remove/move/rewire nodes or change node properties. Operates on the Markdown file in Temp/ produced by /gaea2-load; write the result back to a .terrain with /gaea2-save.
argument-hint: <name-or-Temp/path.md>
allowed-tools: [Read, Edit, Write, Glob, Grep, PowerShell]
disable-model-invocation: true
---

# gaea2-modify

## Purpose

Edit the Markdown view of a loaded Gaea terrain. Nothing but Claude edits `Temp/<name>.md`; the only script this skill runs is the read-only validator run in step 3 of [`references/worker.md`](references/worker.md). The companion `.passthrough.json` is read-only here; if a change requires altering ports or modifiers, document the limitation and ask the user.

## When to use

Editing a Gaea 2 terrain previously loaded by `/gaea2-load` — adding, removing,
moving, or rewiring nodes, or changing node properties.

- Doesn't change ports/modifiers — those are in passthrough JSON.
- Doesn't change `$type` / type FQNs — pinned by passthrough.
- Doesn't validate that a property name is real for the given node type — the parser is permissive; if you invent `Foo: 1` on an Erosion2 it'll round-trip through but Gaea will likely ignore it.

## Inputs

`$ARGUMENTS` holds either a bare name (`Stylized Mountain`), a `.md` filename, or a full path.

## Handoff

### New node port-catalogue message

"I added <Type> as node n<NEW_ID>, but its port catalogue isn't in passthrough yet — saving will use a default In/Out pair. If <Type> needs more ports (e.g. Erosion2's Flow/Wear/Deposits, Combine's secondary input), you'll need to copy the port block from another file of the same type."

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The edit steps and the per-operation
  rules.
