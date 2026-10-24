# ModelViewer Capture

Read this reference only for a scenario that targets the ModelViewer. It owns
the one-call use of `../scripts/Invoke-ModelViewerCapture.ps1`, which starts the
ModelViewer Debug executable hidden in its agent mode, captures PNGs through the
viewer's own F9 path by message to its window, and closes it by exact PID; the
script owns that sequence, its result codes, and its fixed capture timeout.

## Capture

No claim, launch script, process check, or release applies: the viewer binds no
port, so the capture takes no harness lock and touches no other session's
process.

Run one call from the session worktree root, with a fresh app-data path under
`Temp` stamped with the current UTC time. `-RequireStable` captures until two
consecutive captures are identical; drop it only when one unsettled capture
suffices:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1 -RepositoryRoot '<absolute adopted worktree>' -AppDataDirectory '<absolute adopted worktree>\Temp\ModelViewerCapture-<UTC stamp>' -RequireStable
```

- Pass no log or configuration argument: the script always runs the Debug
  executable and writes the viewer log to `ModelViewer.log` inside the app-data
  directory it creates.
- Run it synchronously in the foreground and remain in-turn until its exit code
  and JSON result are captured, giving the call the host's maximum execution
  timeout, such as 600000 ms, so the host never stops a run. Never use
  `Start-Job`, a trailing `&`, or a host background-execution parameter such as
  `run_in_background`, and never end a delegated turn while the script runs.
- If the host call times out, re-invoke in the foreground with a new stamped
  app-data path; the earlier directory exists, so the identical command fails
  as `internal.error`. If the host moved the call to the background and that
  task has exited, use its exit code and JSON and do not re-invoke; if that
  task is still running, wait in-turn for it to exit and use its result, never
  stopping it, because stopping it would leave its viewer running. A host that
  hands a still-running call back to be polled, as Codex's `exec_command`
  does, has not moved it to the background: keep polling it in-turn.

The script prints one compact `broken-engine-model-viewer-capture/v1` JSON
object: `status`, `code`, `message`, `pid`, `exitCode`, `captures` (each
`{path, width, height, sha256}` in order), `stable` (`null` without
`-RequireStable`), `windowVisible`, `logFile`, and `crashReports` (the existing
crash reports in the fresh app-data directory, so each is this run's).

- Classify by `code`, never by `status`. Code `ok` means the captures finished
  and the viewer closed after `WM_CLOSE`; the last `captures` entry is the
  result image.
- Classify every other code under [`../SKILL.md`](../SKILL.md) `## Handoff`.
  `capture.executable-missing` and `internal.error` are setup limitations that
  leave the criterion `BLOCKED`. `capture.failed`, `capture.timeout`,
  `capture.unstable`, `process.unexpected-exit`, and `close.forced` are observed
  failures.
- Any result with a non-null `pid`, meaning the viewer started, that has a
  nonzero `exitCode` or any `crashReports` entry is also an observed failure,
  whatever its code, including `ok` and `internal.error`.
- Report each observed failure as a `HARNESS-F` finding citing its `exitCode`
  and `crashReports`.
- `windowVisible` must be `false`; `true` is an observed failure, because the
  capture must never show the viewer to the human sharing the PC.
- Search the result's `logFile` for `DEBUG_BREAK at` under
  [`worker.md`](worker.md) step 42's rule; this run opened that log, so it holds
  only this run's lines.
- The viewer's `ModelViewer screenshot complete` log line is the capture's
  result channel, not criterion state, so the rule against log scraping in
  `../SKILL.md` `## Handoff` does not apply to it.
- Settle pixel assertions with a script under `Temp`, per the image rule in
  [`worker.md`](worker.md) `## Rules`, rather than loading the PNG into context.
