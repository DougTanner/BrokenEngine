# Early-startup log lines never reach `--log-file`

Non-executable: this records a proven gap and the candidate resolutions. It presents options rather than a
decision-complete implementation, so it stays here until the open decisions are made and it can move to
`Documents/Plans/Engine/` with a byte-zero plan marker.

## Observed gap

`common::EnableLogFile(gLaunchOptions.logFile)` is called at `Engine/Source/Main.cpp:78`, inside
`engine::MainThread`. `wWinMain` does not enter `MainThread` until `Engine/Source/Main.cpp:822` (debugger
present) or `:828` (the `try` block). Everything logged before that point is emitted while no file sink exists:

- `ParseLaunchOptions` (`Engine/Source/Main.cpp:777`) runs first, and its trust-boundary rejections log
  `kError` lines at `Engine/Source/LaunchOptions.cpp:42`, `:72`, `:80`, `:89`, `:98`, `:110`, `:118`, `:127`,
  `:136`, `:161`.
- The single-instance abort logs `kError` at `Engine/Source/Main.cpp:793`.
- `FileManager` is constructed at `Engine/Source/Main.cpp:818`, so its constructor's AppData lines are also
  pre-sink.

`common::LogWrite` (`Common/Log/Log.cpp:118-134`) tees a line to `OutputDebugString`, to `printf` only when
`kbAlsoLogToPrintf` is true, and to the file only when `sLogFileStream.is_open()`. This project sets
`kbAlsoLogToPrintf = false` (`Projects/BrokenEngineSandbox/Source/Pch.h:12`), so a pre-sink line survives only
in `OutputDebugString` output and the in-memory ring buffers, and `EnableLogFile`
(`Common/Log/Log.cpp:35-50`) opens the file with `std::ios::trunc` without replaying anything already in those
rings.

## Why it matters

Observed while verifying `--app-data-directory` acceptance: when a launch option is rejected, `bOk` is false,
`wWinMain` returns `0` at `Engine/Source/Main.cpp:779-780`, and the process exits reporting success with no
line in the log file explaining why. Verification had to prove rejection indirectly — no listening agent port
and no created directory. Anyone diagnosing a bad launch argument, in a harness run or by hand, has nothing to
read: the file either does not exist or is empty.

## Open decisions

The end state ("a rejected launch option explains itself in the log file") cannot be reached by moving one
call, because the sink's path is itself a launch option parsed by the code whose failures need logging. Each
candidate below trades differently, and no choice has been made.

1. Enable the sink inside `ParseLaunchOptions` immediately after the `--log-file` branch
   (`Engine/Source/LaunchOptions.cpp:56-66`). Smallest change, but it captures only arguments appearing after
   `--log-file` on the command line, so behaviour depends on argument order.
2. Pre-scan the argument vector for `--log-file` and enable the sink before the main parse loop. Order
   independent and captures every rejection, at the cost of a second pass over the command line and a second
   place that knows the option's spelling.
3. Replay the retained in-memory ring into the file when `EnableLogFile` opens it. Recovers earlier lines
   regardless of where the call sits, but changes a shared `Common` logging contract used by DataPacker and the
   tools, and still needs the enable call to happen before the parse-failure `return`.
4. Move the existing `EnableLogFile` call from `MainThread` up into `wWinMain` right after a successful parse.
   Captures the single-instance and `FileManager` lines but no parse rejection at all, since a failed parse
   returns first.

A separate decision rides along: `wWinMain` returning `0` on a fatal launch-option error means the harness and
any script see a success exit code for a startup that never ran. Changing that return to a non-zero code would
make the failure observable without any logging change, but it is an observable process-contract change for
`.agents/skills/agent-harness` and the project harness documentation, so it needs explicit agreement rather
than being folded in silently.

## Deliberately not treated as a defect

The `kDebug` "AppData directory" line in the `FileManager` constructor is unobservable at the default settings
even after the sink exists: `gLogRuntimeLevels` defaults to `kInfo` (`Common/Log/Log.cpp:18`), the `LOG` macro
drops sub-threshold lines before they reach the ring (`Common/Log/Log.h:213-222`), and there is no launch-time
log-level option — the agent lowers levels live through `set_log_level` after startup. That is the documented
meaning of `kDebug`, not a bug, and it is recorded here only so a later reader does not mistake it for one.

## Provenance

Found during runtime acceptance verification of
`Documents/Plans/Engine/AppDataDirectoryLaunchOverride.md`, whose `## In scope` covered the
`appDataDirectory` member, its parse branch, the `FileManager` root selection, and two documentation
statements. The log-sink ordering is pre-existing and outside that boundary.
