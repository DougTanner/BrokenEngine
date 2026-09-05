# gaea2-save Worker

The save steps and the rules for the worker dispatched with
[`../SKILL.md`](../SKILL.md), which owns the purpose and the inputs.

## Steps

1. Resolve the input path. Done when one input `.md` path and one output path are fixed.
   - Bare name → `Temp/<name>.md`.
   - `.md` filename without dir → `Temp/<filename>`.
   - Otherwise use as given.
   - If `--output` is absent, default to `<input-base>.terrain` next to the .md file. If the user wants to overwrite the original, pull the original path from the `.passthrough.json`'s `original_file` field and offer that.

2. Validate the Markdown before saving. Dispatch the validator through the shared Python wrapper, from the session worktree root. Done when the wrapper has printed one `broken-engine-gaea2-python/v1` envelope.
   ```powershell
   pwsh -NoProfile -Command "& '.agents/skills/gaea2-shared/scripts/Invoke-Gaea2Python.ps1' -Script validate_markdown.py -Arguments '<Temp/name.md>'"
   ```
   - Use `-Command`, not `-File`, for this wrapper: that is the form that passes the `-Arguments` list through intact.
   - On `python.missing` / `python.stale` (exit 2) nothing ran — apply the missing-interpreter rule in step 5.

3. Gate on the validator's own `broken-engine-gaea2-markdown/v1` envelope, which is the string in the wrapper's `stdout` field, and on that inner `code` rather than on the exit code. Done when the inner `code` is `ok`, or its entries are resolved or explicitly waived by the user.
   - That holds for `python.script-failed` too, which is what the validator's own blocked codes (it exits 2) look like from outside: parse the inner envelope out of `stdout` for the diagnostic code first, and fall back to the wrapper's `stderr` only when `stdout` carries no envelope.
   - `ok` — no entries; proceed to step 4.
   - `markdown.entries-reported` — `payload.entries` lists `{nodeId, rule, message}` findings (`no-path-to-output`, `required-input-unwired`, `missing-version-2`, `duplicate-node-id`, and the rest). Every one of them is a defect Gaea would surface much later as a failed load or a silently deactivated node, so saving is blocked: fix them in `/gaea2-modify`, or get the user's explicit say-so to save anyway after showing them the entries.
   - `input.invalid`, `markdown.missing`, `markdown.unreadable`, `passthrough.missing`, `passthrough.unreadable`, `markdown.structure-unparsed` — the validator couldn't read or parse the input at all, so it proved nothing. Resolve that first; a missing sidecar means re-running `/gaea2-load` from the original `.terrain`.
   - Report `payload.entriesTruncated` if set — more than 64 entries were found and the list is cut off.

4. Run the save wrapper. One call resolves the input and output paths, checks Python, and runs the saver. Done when that one call has returned.
   ```powershell
   pwsh -NoProfile -File .agents/skills/gaea2-shared/scripts/Invoke-Gaea2Save.ps1 -InputPath "<name-or-Temp/path.md>" [-Output "<path.terrain>"]
   ```
   - `-InputPath` accepts the same forms as step 1 (a bare name becomes `Temp/<name>.md`); omitting `-Output` defaults to `<input-base>.terrain` beside the `.md`, so pass the original path explicitly when the user chose to overwrite it.
   - The saver parses frontmatter, Mermaid topology, node sections, notes, and variables.
   - It reconstitutes the asset structure with Build/Automation/State/BuildProfiles from passthrough.
   - It renumbers `$id` values document-order, then rewrites all `$ref` pointers via the old-to-new remap. The two passes are required: Newtonsoft expects $id 1..N contiguous in document order, and every $ref must resolve to an earlier $id — collapsing the renumber and ref-fixup into one pass would break refs that point forward, so don't simplify it.
   - It sets each port's `Parent: {$ref: <node-$id>}` after the renumber pass.
   - It emits WARNINGs — which reach you as the envelope's `warnings` array — for: nodes without a passthrough entry, edges that didn't bind to any input port, synthesized type FQNs.

5. Act on the save envelope's `status`. Stdout is one JSON object, `schemaVersion` `broken-engine-gaea2-save/v1`, with `status`, `code`, `message`. Done when one `broken-engine-gaea2-save/v1` envelope has been read and its `status` acted on.
   - `ok` (exit 0) — `outputPath` is the file written and `warnings` holds the saver's `WARNING:` lines (prefix stripped, at most 64; `truncated` flags a cut-off). Continue to step 6.
   - `input.needs-user` (exit 2) — no input name or path was supplied. Ask the user which Markdown view to save, then re-run.
   - `python.missing` / `python.stale` (exit 2) — no usable x64 CPython 3.12+ interpreter, and nothing else ran. Do not install anything: report to the user that Python is missing or too old, quoting `message`, and — for `python.missing` — the `installCommand` the envelope carries as the suggested command for them to run (`installCommand` is null for a stale interpreter, which the user resolves by upgrading). Stop until they say it's installed.
   - `input.not-found`, `save.failed`, `save.output-missing`, `internal.error` (exit 1) — report `message` plus the envelope's `stderr`.

6. Sanity-check the output. Read the produced `.terrain` and confirm both of the following. (JSON validity needs no separate check — the script writes via `json.dump`.) Done when both confirmations hold.
   - First key is `"$id": "1"` and the `$id` sequence is contiguous.
   - Asset/Terrain/Nodes structure looks like the original.

7. Report the result. Surface every entry of step 5's `warnings` array to the user — each one is a saver WARNING, and the failure-mode bullets under `## Rules` explain what each means — then report node count, edge count, and output path back. Done when that report has been made.

8. Diff the output against the original `.terrain` (path is in `.passthrough.json` → `original_file`) when the user asks whether anything changed or you have low confidence in a tricky modification. Strip the noise that always changes between saves before comparing — `$id` values, dates, viewport floats. The remaining diff should be exactly the user's edits. Done when that diff has been taken, or no such request or low-confidence edit applies.

## Rules

- Both wrappers resolve relative paths and `Temp/` against the repository root whatever the shell's current directory is, and pass the child processes' stderr through to yours — append your shell's stderr redirect (`2>$null` in PowerShell, `2>/dev/null` in Bash) when you only want the JSON.
- Never reconstruct these operations inline: don't run the shared `.agents/scripts/Detect-Python.ps1` probe the wrappers already call and parse its `OK`/`MISSING`/`STALE` line yourself, and don't invoke `save_terrain.py` or `validate_markdown.py` with a bare `python` or a captured interpreter path.

### Round-trip caveats to surface

- `$id` values change every save. That's by design (Newtonsoft regenerates them). Don't flag this as a diff.
- Dates change every save. `DateLastSaved` is updated by Gaea; the saver leaves the field alone (your edit-time value), so a Gaea-then-tool round-trip will diverge there. Acceptable.
- Floats may have tiny representation drift. Python's `repr(float)` differs from C# `double.ToString("R")` in some corner cases. If you see `15.870041` becoming `15.870041000000001`, that's the round-trip — values are equal in IEEE 754 but the text differs. Note it but don't fix.
- Unknown property keys passthrough verbatim. If the user added `Foo: 1` to an Erosion2 node, the saver writes `Foo: 1` into the JSON. Gaea will ignore unknown keys, but they survive future loads. Warn the user if you see suspicious keys.

### Failure modes

- `ERROR: sidecar not found` — the `.passthrough.json` was deleted. Re-run `/gaea2-load` from the original `.terrain` to regenerate.
- `WARNING: edge ... not wired` — Mermaid block references a `to_port` name that doesn't exist on the target node's port catalogue. Save still produces a file; Gaea may load with that input disconnected. Most often: a node was added in `/gaea2-modify` without copying a real port catalogue.
- `WARNING: node ... has no passthrough entry` — same root cause; the saver synthesizes a FQN and emits no ports, which Gaea may reject outright.
