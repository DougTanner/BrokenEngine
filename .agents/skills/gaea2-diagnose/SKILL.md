---
name: gaea2-diagnose
description: Diagnose why Gaea 2 rejected a .terrain file or failed a build. Use when the user reports an error like "File is corrupt or missing additional data", "Swarm failed to load", a node loading as deactivated (dashed border), or unexpected behavior after a /gaea2-save round-trip. Finds the latest Gaea 2 session log (under %APPDATA%\QuadSpinner\Gaea\2.0\Logs — NOT Gaea 1's location), decodes its base64+gzip ERR payloads to extract the underlying Newtonsoft exception with JSON path, then cross-references the shipping Examples library to suggest the valid value or shape.
argument-hint: [--latest-swarm | --blob <base64> | --log <path>]
allowed-tools: [Read, Edit, Bash, PowerShell, Glob, Grep]
disable-model-invocation: true
---

# gaea2-diagnose

## Purpose

The Gaea 2 UI reports failures with a vague modal ("File is corrupt or missing additional data"); the real Newtonsoft exception is in a log. Find the right log, decode the exception, cross-reference the shipping samples for the valid value, and tell the user what to change.

## When to use

- The user reports an error like "File is corrupt or missing additional data" or
  "Swarm failed to load".
- A node loads as deactivated (dashed border).
- Behavior is unexpected after a `/gaea2-save` round-trip.
- This skill diagnoses; it does not edit the .terrain. Apply fixes via `Temp/<basename>.md` + `/gaea2-save` (step 6 of [`references/worker.md`](references/worker.md)); structural graph changes chain to `/gaea2-modify`.
- Runtime crashes in the Gaea binary itself go to QuadSpinner support, not this skill.

## References

- [`references/worker.md`](references/worker.md) — the log streams, the
  diagnosis steps, and the common error patterns.
