# gaea2-load Worker

The load steps and the rules for the worker dispatched with
[`../SKILL.md`](../SKILL.md), which owns the purpose and the inputs.

## Steps

1. Resolve the input path. Done when one input path ending in `.terrain` is in hand.

   - If `$ARGUMENTS` is empty or doesn't end in `.terrain`, ask the user for the file path.
   - If it's a bare filename (no directory), try `C:/Program Files/QuadSpinner/Gaea 2/Examples/<name>.terrain` — that's where Gaea ships its example library.
   - The shared `.claude/skills/gaea2-shared/examples/` directory is intentionally empty (the example files are © QuadSpinner and aren't redistributed); see its `README.md` for the rationale and the 28-file working set.

2. Run the load wrapper from the session worktree root. One call resolves the input path, checks Python, probes Gaea version drift, and runs the loader — which writes `Temp/<basename>.md` and `Temp/<basename>.passthrough.json`. Done when that one call has returned.

   ```powershell
   pwsh -NoProfile -File .agents/skills/gaea2-shared/scripts/Invoke-Gaea2Load.ps1 -InputPath "<input.terrain>"
   ```

   - The wrapper resolves relative paths and `Temp/` against the repository root whatever the shell's current directory is.
   - It passes the child processes' stderr through to yours — append your shell's stderr redirect (`2>$null` in PowerShell, `2>/dev/null` in Bash) when you only want the JSON.

3. Act on the envelope's `status`. Stdout is one JSON object, `schemaVersion` `broken-engine-gaea2-load/v1`, with `status`, `code`, `message`. Done when one `broken-engine-gaea2-load/v1` envelope has been read and its `status` acted on.

   - `ok` (exit 0) — `markdownPath`, `passthroughPath`, and `versionDrift` are populated. Continue to step 4.
   - `input.needs-user` (exit 2) — the path was empty or didn't end in `.terrain`. Ask the user for the file (step 1's rule), then re-run.
   - `python.missing` / `python.stale` (exit 2) — no usable x64 CPython 3.12+ interpreter, and nothing else ran. Do not install anything: report to the user that Python is missing or too old, quoting the envelope's `message` (it carries the probe's own `MISSING`/`STALE` line), and — for `python.missing` — the `installCommand` the envelope carries as the suggested command for them to run (`installCommand` is null for a stale interpreter, which the user resolves by upgrading). Stop until they say it's installed.
   - `input.not-found`, `load.failed`, `load.output-missing`, `internal.error` (exit 1) — report `message` plus the envelope's `stderr`, which carries the failing script's own diagnostics.

4. Act on the envelope's `versionDrift` object, which carries `state` and a `notice` (the probe's own output). Done when its `state` has been handled.

   - `unchanged` — proceed silently.
   - `changed` — surface the `notice` to the user in your final report as a one- or two-line note ("Gaea moved 2.3.0.0 → 2.4.0.0; SatMap.Library gained 'Volcanic'"), and suggest they check that `../../gaea2-shared/references/node-conventions.md`'s per-type constraints aren't stale.
   - `baseline-cached` — no PC-local baseline existed yet; it was cached silently, proceed.
   - `undetected` — no Gaea install or shared cache was reachable. Note it in the report but continue; drift detection is an information probe, not a gate.

5. Report what was loaded. Read the resulting `.md` file's frontmatter and the Mermaid block, then show the user a brief summary: number of nodes, number of edges, build resolution, the Mermaid topology rendered inline. Include any version-drift note from step 4. Done when that summary has been reported to the user.

   The Markdown file has three editable sections:

   - Frontmatter (between `---` lines): terrain dimensions, build settings, version metadata. Keys prefixed `build_*` map to `BuildDefinition` in the JSON.
   - `## Topology` Mermaid block: each node is `n<ID>["<Type>"]`, each edge is either `nA --> nB` (default `Out → In`) or `nA -- "FromPort → ToPort" --> nB` for non-default ports.
   - `## Nodes` section: one `### n<ID>: <Type>` heading per node, followed by flat `key: value` lines. `position: X, Y` is the node's canvas position. Other keys are node-specific properties (e.g. `Duration`, `Seed` on Erosion2).

6. Point the user at the next skill. The user can now use `/gaea2-modify` to edit, then `/gaea2-save` to write back; if something fails when they open the result in Gaea 2, point them at `/gaea2-diagnose`. Done when that routing has been stated.

## Rules

- Gaea's accepted enum values, required fields, and port catalogues can shift between releases, so the wrapper's `versionDrift` object exists to catch that before it bites.
- Never reconstruct the wrapper's operations inline: don't run the shared `.agents/scripts/Detect-Python.ps1` probe the wrapper already calls and parse its `OK`/`MISSING`/`STALE` line yourself, don't invoke `check_gaea_version.py` or `load_terrain.py` with a bare `python` or a captured interpreter path, and don't chain the calls by hand.
- The `.passthrough.json` carries Newtonsoft `$id`/`$ref` graph state, original `$type` FQNs, port catalogues, modifier arrays, viewport state, GraphTabs, BuildProfiles, Bindings, metadata templates, and anything else the Markdown view drops. Treat it as opaque — hand-editing it will likely corrupt round-trip; if a structural change is needed, modify the source `.terrain` and re-run `/gaea2-load`.
