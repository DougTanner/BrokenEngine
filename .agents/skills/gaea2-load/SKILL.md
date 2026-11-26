---
name: gaea2-load
description: Load a Gaea 2 .terrain file into an editable Markdown view (Mermaid topology + per-node properties) under Temp/. Pairs with /gaea2-modify and /gaea2-save for round-trip editing.
argument-hint: <path-to-.terrain>
allowed-tools: [Read, PowerShell]
disable-model-invocation: true
---

# gaea2-load

## Purpose

Convert a Gaea 2 `.terrain` JSON into:
- `Temp/<basename>.md` — editable Markdown (frontmatter + Mermaid topology +
  per-node properties)
- `Temp/<basename>.passthrough.json` — sidecar with structural state needed for
  round-trip (do not hand-edit)

## Inputs

`$ARGUMENTS` holds the user-supplied path.

## Steps

1. Resolve the input path. Done when one input path ending in `.terrain` is in
   hand.

   - If `$ARGUMENTS` is empty or doesn't end in `.terrain`, ask the user for the
     file path.
   - Bare names and relative paths follow
     [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#no-inline-reconstruction).

2. Run the load wrapper from the session worktree root. One call resolves the
   input path, checks Python, probes Gaea version drift, and runs the loader —
   which writes `Temp/<basename>.md` and `Temp/<basename>.passthrough.json`.
   Done when that one call has returned.

   ```powershell
   pwsh -NoProfile -NonInteractive -File .agents/skills/gaea2-shared/scripts/Invoke-Gaea2Load.ps1 -InputPath "<input.terrain>"
   ```

   Path resolution and stderr pass-through follow
   [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#no-inline-reconstruction).

3. Act on the envelope's `code`. Stdout is one JSON object, `schemaVersion`
   `broken-engine-gaea2-load/v1`, with `status`, `code`, `message`. Done when
   one `broken-engine-gaea2-load/v1` envelope has been read and its `code`
   acted on.

   - `ok` — `markdownPath`, `passthroughPath`, and `versionDrift` are populated.
     Continue to step 4.
   - `input.needs-user` — the path was empty or didn't end in `.terrain`. Ask
     the user for the file (step 1's rule), then re-run.
   - `python.missing` / `python.stale` — follow
     [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#missing-or-stale-python).
   - `input.not-found`, `script.not-found`, `probe.not-found`,
     `python.probe-failed`, `load.failed`, `load.output-missing`,
     `internal.error` — report `message` (and `stderr` when present).

4. Act on the envelope's `versionDrift` object, which carries `state` and a
   `notice` (the probe's own output). Done when its `state` has been handled.

   - `unchanged` — proceed silently.
   - `changed` — surface the `notice` to the user in your final report as a one-
     or two-line note ("Gaea moved 2.3.0.0 → 2.4.0.0; SatMap.Library gained
     'Volcanic'"), and suggest they check that the per-type constraints in
     `../gaea2-shared/references/node-conventions.md`
     `## Per-type enum constraints` aren't stale.
   - `baseline-cached` — no PC-local baseline existed yet; it was cached
     silently, proceed.
   - `undetected` — no Gaea install or shared cache was reachable. Note it in
     the report but continue; drift detection is an information probe, not a
     gate.

5. Report what was loaded. Read the resulting `.md` file's frontmatter and the
   Mermaid block, then show the user a brief summary: number of nodes, number of
   edges, build resolution, the Mermaid topology rendered inline. Include any
   version-drift note from step 4. Done when that summary has been reported to
   the user.

   The Markdown file has three editable sections:

   - Frontmatter (between `---` lines): terrain dimensions, build settings,
     version metadata. Keys prefixed `build_*` map to `BuildDefinition` in the
     JSON.
   - `## Topology` Mermaid block: each node is `n<ID>["<Type>"]`, each edge is
     either `nA --> nB` (default `Out → In`) or
     `nA -- "FromPort → ToPort" --> nB` for non-default ports.
   - `## Nodes` section: one `### n<ID>: <Type>` heading per node, followed by
     flat `key: value` lines. `position: X, Y` is the node's canvas position.
     Other keys are node-specific properties (e.g. `Duration`, `Seed` on
     Erosion2).

6. Point the user at the next skill. The user can now use `/gaea2-modify` to
   edit, then `/gaea2-save` to write back; if something fails when they open the
   result in Gaea 2, point them at `/gaea2-diagnose`. Done when that routing has
   been stated.

## Rules

- Gaea's accepted enum values, required fields, and port catalogues can shift
  between releases, so the wrapper's `versionDrift` object exists to catch that
  before it bites.
- Never reconstruct the wrapper's operations inline; follow
  [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#no-inline-reconstruction).
- The `.passthrough.json` carries Newtonsoft `$id`/`$ref` graph state, original
  `$type` FQNs, port catalogues, modifier arrays, viewport state, GraphTabs,
  BuildProfiles, Bindings, metadata templates, and anything else the Markdown
  view drops. Treat it as opaque — hand-editing it will likely corrupt
  round-trip; if a structural change is needed, instruct the user to modify the
  source `.terrain` and run `/gaea2-load`.
