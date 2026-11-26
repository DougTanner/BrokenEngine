# Gaea 2 Wrapper Contract

## Python wrapper dispatch

Run `gaea2-shared` Python scripts from the worktree root:

```powershell
pwsh -NoProfile -NonInteractive -Command "& '.agents/skills/gaea2-shared/scripts/Invoke-Gaea2Python.ps1' -Script <script.py> -Arguments '<arg>','<arg>'"
```

- Use `-Command` per [AGENTS.md](../../../../AGENTS.md#directives) (Bundled
  scripts); `-File` also misparses leading-dash values in `-Arguments`.
- Stdout is one `broken-engine-gaea2-python/v1` JSON object carrying `status`,
  `code`, `message`, `interpreter`, `pythonVersion`, `exitCode`, `stdout`,
  `stderr`, and `truncated` (governed by `$script:MaximumOutputCharacters` in
  `../scripts/Invoke-Gaea2Python.ps1`).
- Branch on `code`:
  - `ok` — read `stdout`.
  - `python.missing` / `python.stale` — follow
    [Missing or stale Python](#missing-or-stale-python).
  - `python.script-failed` — script returned nonzero `exitCode`; inspect
    `stdout` (inner envelope) or `stderr`.
  - `script.not-found`, `probe.not-found`, `python.probe-failed`,
    `internal.error` — dispatch failed; report `message`.

## Missing or stale Python

For `python.missing` / `python.stale` from `Invoke-Gaea2Python.ps1`,
`Invoke-Gaea2Load.ps1`, or `Invoke-Gaea2Save.ps1`:

- Nothing else ran; never install Python.
- Tell the user Python is missing or too old, quoting `message` (and
  `probeLine` when present).
- For `python.missing`, pass `installCommand` as the suggested command
  (`installCommand` is null for `python.stale`, which the user resolves by
  upgrading).
- Stop until the user confirms installation.

## No inline reconstruction

Applies to all `gaea2-*` wrappers:

- Path resolution: directory-qualified relative paths resolve against the
  repository root regardless of current directory. Bare names (no directory):
  - `Invoke-Gaea2Load.ps1`: `-InputPath` under `$script:ExamplesDirectory` (in
    `../scripts/Invoke-Gaea2Load.ps1`); outputs under repo `Temp/`.
  - `Invoke-Gaea2Save.ps1`: `-InputPath` under repo `Temp/` (appending `.md` if
    absent); relative `-Output` resolves against the repository root.
  - `Invoke-Gaea2Python.ps1`: `-Script` as a sibling in
    `.agents/skills/gaea2-shared/scripts/`; child runs with the repository root
    as working directory.
- Stderr pass-through: child stderr passes through to caller stderr; append
  `2>$null` when only JSON is wanted.
- Delegate interpreter discovery, `gaea2-shared` Python execution, and
  multi-step chaining to the applicable wrapper (`Invoke-Gaea2Python.ps1`,
  `Invoke-Gaea2Load.ps1`, or `Invoke-Gaea2Save.ps1`).

## Markdown validator gating

Run `validate_markdown.py` (read-only) from the worktree root:

```powershell
pwsh -NoProfile -NonInteractive -Command "& '.agents/skills/gaea2-shared/scripts/Invoke-Gaea2Python.ps1' -Script validate_markdown.py -Arguments '<Temp/name.md>'"
```

Outer wrapper rules and `python.missing` / `python.stale` follow
[Python wrapper dispatch](#python-wrapper-dispatch) and
[Missing or stale Python](#missing-or-stale-python).

Branch on the inner `broken-engine-gaea2-markdown/v1` `code` parsed from
`stdout` (not exit code), including under `python.script-failed` for blocked
codes or `internal.error`; fall back to `stderr` only when `stdout` has no
envelope:

- `ok` — proceed.
- `markdown.entries-reported` — `payload.entries` (`{nodeId, rule, message}`);
  each calling skill states its own gate.
- `input.invalid`, `markdown.missing`, `markdown.unreadable`,
  `passthrough.missing`, `passthrough.unreadable`,
  `markdown.structure-unparsed` — validator proved nothing; resolve before
  continuing (plus skill-specific recovery).
- `internal.error` — stop (`gaea2-save` and `gaea2-modify`) and report the inner
  `message`.
- `payload.entriesTruncated` — report `payload.entryCount` and note the
  displayed list is incomplete whether capped by `MAXIMUM_ENTRIES` or
  `MAXIMUM_ENVELOPE_CHARACTERS` in `../scripts/validate_markdown.py`.
