# New Plan File

`.agents/scripts/New-PlanFile.ps1` is the repository-owned writer for one
executable Plan. It writes the immutable `broken-engine-plan/v1` metadata marker
at byte zero with a canonical `createdUtc` and a unique ordinal-sorted
`dependsOn`, copies the supplied body verbatim below it in BOM-less UTF-8,
refuses to overwrite an existing path, stages that exact new Plan as the sole
new index entry from the invocation, and folds
`WorktreeCli plan validate --plan <normalized Plan path>` into its result. Never
reconstruct its marker, timestamp, encoding, dependency, filename, overwrite,
staging, or validation operations inline.

## Invocation

From the session worktree root:

```powershell
pwsh -NoProfile -File .agents/scripts/New-PlanFile.ps1 -Area <existing area> -Name <PascalCase.md> -Body <body file path> -DependsOn <plan paths as one comma-separated token>
```

- `-Area` — an existing directory beneath `Documents/Plans`; the script creates
  none.
- `-Name` — a bare filename matching `^[A-Z][A-Za-z0-9]*\.md$`.
- `-Body` — a file holding the plan text that goes below the marker. The body
  travels as a file so no transcript text is ever executed.
- `-DependsOn` — `Documents/Plans/**/*.md` paths. `pwsh -File` hands every
  argument over as one literal string, so several dependencies can only travel
  as one comma-separated token. Omit the parameter entirely when the Plan has no
  dependencies: the script rejects a blank entry and defaults to an empty
  dependency list only when the parameter is absent.

## Result

Parse the single `broken-engine-new-plan-file/v1` JSON object on stdout. It
carries `schemaVersion`, `status`, `code`, `message`, `plan`, `createdUtc`,
`dependsOn`, `written`, `validation` (the folded `plan validate` `exitCode`,
`status`, `code`, `message`, `diagnostics`, and `notices`), and `truncated`.

Exit `0` with nested validation `exitCode: 0`, `status: valid`, and `code: ok` is
the only outcome reportable as created. Exit `1` (`error`) and exit `2`
(`blocked`) both block reporting the Plan as created; report the returned `code`
and `message`. Whether a file exists on disk is `written`, not the exit code: a
validation failure leaves the written Plan staged and in place, so correct the
body and revalidate instead of recreating the Plan or rewriting the file by
hand. Staging affects only the exact new Plan path; existing staged and
worktree changes remain untouched.

Two codes report a plan scheduler lock outcome rather than a body problem, and
both also leave the written Plan staged and in place: `scheduler.busy` (exit
`2`, `blocked`) means another session held the scheduler for the full wait, so
tell the user and retry validation later; `scheduler.guard-unavailable` (exit
`1`, `error`) means the scheduler's lock storage is unusable, so re-running will
not help. Editing the Plan body changes neither outcome.

`truncated` `true` means a cap applied — 64 `dependsOn` entries, 16 diagnostics
or notices, a 256-character message, or a `validation` projection dropped from
an oversized envelope — so the reported detail is incomplete.
