<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:50.698Z","dependsOn":[]} -->
# De-duplicate the directory launch option parsing

## Context

In `ParseLaunchOptions` (`Engine/Source/LaunchOptions.cpp:10-184`), the `--data-directory` block (68-105) and the `--app-data-directory` block (106-143) are line-for-line identical apart from the target member (`gLaunchOptions.dataDirectory`, `gLaunchOptions.appDataDirectory`) and the option name inside the log text. Each clears its member, rejects a missing value, then rejects a non-absolute path, a failed `std::filesystem::canonical`, and a failed or false `std::filesystem::is_directory`, logging at `kError` and setting `bSuccess = false`, before storing the canonical path.

Origin: a user-directed read-only review of the top 20 functions by complexity mass at `43450112`. The user's rule: a function is never split because it is big; code that belongs together stays together, and only a piece that does not belong moves. The copy is the piece; the other options' parsing was judged to belong and stays as is.

## Design

Recommended: a file-static `static bool ParseDirectoryOption(const wchar_t* pcValue, const char* pcOptionName, std::filesystem::path& rDirectory)` above `ParseLaunchOptions`. It holds the absolute, `canonical`, and `is_directory` checks once, takes the option name as a log format argument (for example `"Launch option {} must be absolute: \"{}\""`), so each logged line is byte-identical to today's, assigns `rDirectory` only on success, and returns false on any rejection. Each caller block keeps its `clear()`, its missing-value check and log, and its `bSuccess = false`, and becomes `if (!ParseDirectoryOption(pArgumentValues[++i], "--data-directory", gLaunchOptions.dataDirectory)) { bSuccess = false; }`, and the same for `--app-data-directory`.

Risk tier: Tier 1 (`.agents/references/risk-tiers.md`). Trigger: a local behavior-preserving de-duplication into a file-static function in one translation unit, with no public signature or invariant exposure. Parsing runs before allocation tracking (comment at 12-13), so the helper's allocations stay untracked as today. The file is compiled into both client and server.

## Critical files

- `Engine/Source/LaunchOptions.cpp`

## In scope

- New file-static `ParseDirectoryOption` in `LaunchOptions.cpp`.
- The `--data-directory` and `--app-data-directory` blocks of `ParseLaunchOptions` (68-143).

## Out of scope

- Every other launch option, the `CommandLineToArgvW` and `LocalFree` handling, and the return value.
- `LaunchOptions.h`, `gLaunchOptions`, and the callers of `ParseLaunchOptions`.
- Any log level, message text, or validation rule change.

## Acceptance criteria

- Client and server build (`/compile`).
- `/agent-harness`: a client and server launch, which pass both options, starts normally.

## Notes

- Each log line's text is decisive from the diff: the option name moves from the literal into a format argument with the same characters.
