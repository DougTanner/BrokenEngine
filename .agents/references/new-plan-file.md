# New Plan File

`.agents/scripts/New-PlanFile.ps1` is the repository-owned writer for one
executable Plan. It writes the immutable byte-zero metadata marker
(`Documents/Plans/AGENTS.md`) with a canonical `createdUtc` and a unique
ordinal-sorted `dependsOn`, copies the supplied body verbatim below it in
BOM-less UTF-8, refuses to overwrite an existing path, stages that exact new
Plan as the sole new index entry from the invocation, and folds
`WorktreeCli plan validate --plan <normalized Plan path>` into its result. Never
reconstruct its marker, timestamp, encoding, dependency, filename, overwrite,
staging, or validation operations inline.

## Invocation

From the session worktree root:

```powershell
pwsh -NoProfile -File .agents/scripts/New-PlanFile.ps1 -Area <area> -Name <PascalCase.md> -Body <body file path> -DependsOn <plan paths as one comma-separated token>
```

- `-Area` — a directory beneath `Documents/Plans`, or beneath
  `Projects/Coretide/Documents/Plans` when the session has a Coretide clone,
  where the Plan is staged in the clone's index: an existing one, or an area
  `Documents/Plans/AGENTS.md` `## Plan files` lists, whose absent directory is
  created only after all inputs pass; any other missing directory fails with
  `input.area-not-found`.
- `-Name` — a bare filename matching `^[A-Z][A-Za-z0-9]*\.md$`.
- `-Body` — a file holding the plan text below the marker (no transcript text is
  executed). An un-fenced `## Sweep` heading fails with
  `input.sweep-not-allowed` without writing a file or creating a missing area
  directory; see [`Documents/AGENTS.md`](../../Documents/AGENTS.md)
  `## Planning Trees`.
- `-DependsOn` — `Documents/Plans/**/*.md` paths (and Coretide Plans for a
  Coretide area; engine areas reject them with `input.depends-on-invalid`), as
  one comma-separated token for `pwsh -File`; omit for an empty `dependsOn`
  (blanks fail). When to add one:
  [`Documents/Plans/AGENTS.md`](../../Documents/Plans/AGENTS.md)
  `## Plan files`.
- `-AgentCli` — optional CLI lock written to metadata (values in
  `Documents/Plans/AGENTS.md`); invalid or blank fails with
  `input.agent-cli-invalid`.

## Result

Parse the single `broken-engine-new-plan-file/v1` JSON object on stdout. It
carries `schemaVersion`, `status`, `code`, `message`, `plan`, `createdUtc`,
`dependsOn`, `written`, `validation` (the folded `plan validate` `exitCode`,
`status`, `code`, `message`, `diagnostics`, and `notices`), and `truncated`.

Exit `0` with nested validation `exitCode: 0`, `status: valid`, and `code: ok`
is the only outcome reportable as created. Exit `1` (`error`) and exit `2`
(`blocked`) both block reporting the Plan as created; report the returned `code`
and `message`. Whether a file exists on disk is `written`, not the exit code: a
validation failure leaves the written Plan staged and in place, so correct the
body and revalidate instead of recreating the Plan or rewriting the file by
hand. Staging affects only the exact new Plan path; existing staged and worktree
changes remain untouched.

Two codes report a plan scheduler lock outcome rather than a body problem, and
both also leave the written Plan staged and in place: `scheduler.busy` (exit
`2`, `blocked`) means another session held the scheduler for the full wait, so
tell the user and retry validation later; `scheduler.guard-unavailable` (exit
`1`, `error`) means the scheduler's lock storage is unusable, so re-running will
not help. Editing the Plan body changes neither outcome.

`truncated` `true` means a cap applied — 64 `dependsOn` entries, 16 diagnostics
or notices, a 256-character message, or a `MaximumOutputBytes` envelope trim
(which may drop `validation` to `null`) — so the reported detail is incomplete.
