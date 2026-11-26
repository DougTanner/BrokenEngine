---
name: gaea2-save
description: Save edits from a Gaea 2 Markdown view back to a .terrain JSON file. Reconstitutes the Newtonsoft $id/$ref object graph; pairs with /gaea2-load and /gaea2-modify.
argument-hint: <name-or-Temp/path.md> [--output <path.terrain>]
allowed-tools: [Read, PowerShell]
disable-model-invocation: true
---

# gaea2-save

## Purpose

Convert a `Temp/<name>.md` (plus its `.passthrough.json` sidecar) back to a Gaea
2 `.terrain` JSON file.

## Inputs

`$ARGUMENTS` holds either a bare name (`Stylized Mountain`), a `.md` filename,
or a full path; optionally followed by `--output <path>`.

## Steps

1. Resolve the input path. Done when one input `.md` path and one output path
   are fixed.
   - Input path resolution (bare names, `.md` filenames, and relative paths)
     follows
     [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#no-inline-reconstruction).
   - If `--output` is absent, default to `<input-base>.terrain` next to the .md
     file. If the user wants to overwrite the original, pull the original path
     from the `.passthrough.json`'s `original_file` field and offer that.

2. Validate the Markdown before saving. Dispatch `validate_markdown.py` and
   handle `python.missing` / `python.stale` per
   [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#markdown-validator-gating).
   Done when the wrapper has printed one `broken-engine-gaea2-python/v1`
   envelope.

3. Gate on the validator's inner `broken-engine-gaea2-markdown/v1` envelope per
   [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#markdown-validator-gating)
   (including blocked input/parse codes, `internal.error`, and
   `payload.entriesTruncated`). Done when the inner `code` is `ok`, or its
   entries are resolved or explicitly waived by the user.
   - `markdown.entries-reported` — saving is blocked: stop, show
     `payload.entries` to the user, and ask the user either to run
     `/gaea2-modify` to fix them or to give explicit approval to proceed with
     the save anyway.
   - `passthrough.missing` — follow the sidecar-recovery sequence under
     [`### Failure modes`](#failure-modes).

4. Run the save wrapper. One call resolves the input and output paths, checks
   Python, and runs the saver. Done when that one call has returned.
   ```powershell
   pwsh -NoProfile -NonInteractive -File .agents/skills/gaea2-shared/scripts/Invoke-Gaea2Save.ps1 -InputPath "<name-or-Temp/path.md>" [-Output "<path.terrain>"]
   ```
   - Pass `-Output` when `--output` was given or the user chose to overwrite
     the original path; omit it for step 1's default.
   - The bundled [`save_terrain.py`](../gaea2-shared/scripts/save_terrain.py)
     owns parsing, reconstruction, and the `$id`, `$ref`, and port-parent
     fixups; do not reproduce or simplify those operations.
   - It emits WARNINGs — which reach you as the envelope's `warnings` array —
     for: nodes without a passthrough entry, edges that didn't bind to any input
     port, synthesized type FQNs.

5. Act on the save envelope's `code`. Stdout is one JSON object,
   `schemaVersion` `broken-engine-gaea2-save/v1`, with `status`, `code`,
   `message`. Done when one `broken-engine-gaea2-save/v1` envelope has been read
   and its `code` acted on.
   - `ok` — `outputPath` is the file written and `warnings` holds the saver's
     `WARNING:` lines (prefix stripped, capped by `$script:MaximumWarnings` in
     `../gaea2-shared/scripts/Invoke-Gaea2Save.ps1`, with `truncated` flagging a
     cut-off). Continue to step 6.
   - `input.needs-user` — no input name or path was supplied. Ask the user which
     Markdown view to save, then re-run.
   - `python.missing` / `python.stale` — follow
     [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#missing-or-stale-python).
   - `input.not-found`, `save.failed`, `save.output-missing`, `internal.error` —
     report `message` plus the envelope's `stderr`.

6. Sanity-check the output. Read the produced `.terrain` and confirm both of the
   following. (JSON validity needs no separate check — the script writes via
   `json.dump`.) Done when both confirmations hold.
   - First key is `"$id": "1"` and the `$id` sequence is contiguous.
   - Asset/Terrain/Nodes structure looks like the original.

7. Report the result. Surface every entry of step 5's `warnings` array to the
   user — each one is a saver WARNING, and the failure-mode bullets under
   `## Rules` explain what each means — then report node count, edge count, and
   output path back. Done when that report has been made.

8. Diff the output against the original `.terrain` (path is in
   `.passthrough.json` → `original_file`) when the user asks whether anything
   changed or you have low confidence in a tricky modification. Strip the noise
   that always changes between saves before comparing — `$id` values, dates,
   viewport floats. The remaining diff should be exactly the user's edits. Done
   when that diff has been taken, or no such request or low-confidence edit
   applies.

## Rules

- Wrapper path resolution, stderr pass-through, and no-inline-reconstruction
  rules live in
  [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#no-inline-reconstruction).

### Round-trip caveats to surface

- `$id` values change every save. That's by design (Newtonsoft regenerates
  them). Don't flag this as a diff.
- Dates change every save. `DateLastSaved` is updated by Gaea; the saver leaves
  the field alone (your edit-time value), so a Gaea-then-tool round-trip will
  diverge there. Acceptable.
- Floats may have tiny representation drift. Python's `repr(float)` differs from
  C# `double.ToString("R")` in some corner cases. If you see `15.870041`
  becoming `15.870041000000001`, that's the round-trip — values are equal in
  IEEE 754 but the text differs. Note it but don't fix.
- Unknown property keys passthrough verbatim. If the user added `Foo: 1` to an
  Erosion2 node, the saver writes `Foo: 1` into the JSON. Gaea will ignore
  unknown keys, but they survive future loads. Warn the user if you see
  suspicious keys.

### Failure modes

- `ERROR: sidecar not found` — `.passthrough.json` is missing. Stop and
  instruct the user to preserve the edited Markdown first, run `/gaea2-load`
  from the original `.terrain` to regenerate `.passthrough.json`, and pair the
  regenerated sidecar with the preserved edited Markdown (same directory and
  matching basename) before retrying `/gaea2-save`.
- `WARNING: edge ... not wired` — Mermaid block references a `to_port` name that
  doesn't exist on the target node's port catalogue. Save still produces a file;
  Gaea may load with that input disconnected. Most often: a node was added in
  `/gaea2-modify` without copying a real port catalogue.
- `WARNING: node ... has no passthrough entry` — same root cause; the saver
  synthesizes a FQN and emits no ports, which Gaea may reject outright.
