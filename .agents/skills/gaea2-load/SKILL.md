---
name: gaea2-load
description: Load a Gaea 2 .terrain file into an editable Markdown view (Mermaid topology + per-node properties) under Temp/. Pairs with /gaea2-modify and /gaea2-save for round-trip editing.
argument-hint: <path-to-.terrain>
allowed-tools: [Read, Bash, PowerShell]
disable-model-invocation: true
---

# gaea2-load

## Purpose

Convert a Gaea 2 `.terrain` JSON into:
- `Temp/<basename>.md` — editable Markdown (frontmatter + Mermaid topology + per-node properties)
- `Temp/<basename>.passthrough.json` — sidecar with structural state needed for round-trip (do not hand-edit)

## Inputs

`$ARGUMENTS` holds the user-supplied path.

## References

- [`references/worker.md`](references/worker.md) — the load steps and the
  output-format notes.
