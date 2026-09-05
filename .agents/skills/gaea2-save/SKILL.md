---
name: gaea2-save
description: Save edits from a Gaea 2 Markdown view back to a .terrain JSON file. Reconstitutes the Newtonsoft $id/$ref object graph; pairs with /gaea2-load and /gaea2-modify.
argument-hint: <name-or-Temp/path.md> [--output <path.terrain>]
allowed-tools: [Read, Bash, PowerShell]
disable-model-invocation: true
---

# gaea2-save

## Purpose

Convert a `Temp/<name>.md` (plus its `.passthrough.json` sidecar) back to a Gaea 2 `.terrain` JSON file.

## Inputs

`$ARGUMENTS` holds either a bare name (`Stylized Mountain`), a `.md` filename, or a full path; optionally followed by `--output <path>`.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The save steps, the round-trip caveats,
  and the failure modes.
