<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-14T17:08:24.562Z","dependsOn":[]} -->
# Convert the coordination Guard to stateless out-reference failure reporting

## Context

`Documents/Plans/Engine/FailureReportingOutReferenceConvention.md` records the
user's decision that the repository standardizes on stateless out-reference
failure reporting and drops construct-then-interrogate reporting, where an
object records a failure reason on itself during construction and exposes it
through a post-construction query.

That Plan's Phase 1 repository-wide inventory found exactly one in-pattern site
outside its own worked example: `toolcli::coordination::Guard` in
`Tools/ToolCommon/CoordinationStore.h` / `.cpp`. Both halves of the in-pattern
test hold:

- `muiLastError` (`Tools/ToolCommon/CoordinationStore.h:44`) is written only
  inside the constructor's acquisition retry loop
  (`Tools/ToolCommon/CoordinationStore.cpp:37`), and `mbContentionObserved`
  (`.h:45`) is written only at `.cpp:43`, in the same loop.
- Four public queries exist only to report those two members after
  construction: `TimedOut()` (`.h:34`, `.cpp:86-89`), `ContentionObserved()`
  (`.h:36`, `.cpp:91-94`), `LastError()` (`.h:37`, `.cpp:96-99`), and the
  composing `FailureReason()` (`.h:39`, `.cpp:101-104`).

The active Plan's own escalation clause directed this site into its own Plan
rather than converting it under its Tier-2 boundary: `Guard` is in the AgentTools
coordination subsystem, which is independently owned relative to that Plan's
engine file/replay subsystem and its `Save` consumer, and changing
`Tools/ToolCommon` source requires a shared AgentTools rebuild and promotion —
build/bootstrap coordination that can block other sessions.

Behavior is not being changed. Every accept/reject decision, every emitted
message, and all lock and lifetime semantics stay exactly as they are today.

Decision material the executor should not re-derive:

- `IsValid()` (`.h:33`, `.cpp:81-84`) is **not** in pattern and stays. It reports
  `mhFile`, which is also written in the destructor (`.cpp:75`) and is a live
  RAII resource, not constructor failure state. All nine `!guard.IsValid()` call
  sites keep that call unchanged.
- `TimedOut()` and `LastError()` have no callers anywhere outside
  `FailureReason()`; a repository search for `TimedOut(`, `LastError()` on a
  `Guard`, `ContentionObserved(`, and `FailureReason(` returns only the
  definitions plus the consumers listed under `## Critical files`. They are
  therefore deleted outright rather than converted into out-params.
- Only two pieces of failure detail actually reach a caller: the contention flag
  (`PlanScheduler`) and the composed reason string (`LandingLockCommands`,
  `HarnessLockCommands`). Those two become the constructor's out-params.

## Design

### Guard's new construction contract

```cpp
explicit Guard(const std::filesystem::path& rPath, bool& rbContentionObserved, std::string& rFailureReason,
    int64_t iMaximumWaitMilliseconds = 10'000, int64_t iMaximumDeniedAccessMilliseconds = kiUnboundedDeniedAccessMilliseconds);
```

The two out-params precede the two defaulted timeout parameters so both defaults
survive; the call sites that pass timeouts positionally are updated accordingly.

- Delete the members `muiLastError` (`.h:44`) and `mbContentionObserved`
  (`.h:45`), and the queries `TimedOut()`, `ContentionObserved()`,
  `LastError()`, and `FailureReason()` from both `.h:34-39` and `.cpp:86-104`.
  Keep the two explanatory comments that survive their subjects: the
  live-holder comment at `.h:35` moves to the `rbContentionObserved` parameter,
  and the reason-format comment at `.h:38` moves to `rFailureReason`.
- Add one file-local helper in `CoordinationStore.cpp`, placed immediately above
  the constructor, holding the exact text `FailureReason()` produces today:

  ```cpp
  std::string FailureReasonFor(DWORD uiError)
  {
      return uiError == ERROR_SHARING_VIOLATION || uiError == ERROR_LOCK_VIOLATION ? "timed out" : "Windows error " + std::to_string(uiError);
  }
  ```

- At the top of the constructor body, before the zero-wait early return at
  `.cpp:24-27`, write both out-params: `rbContentionObserved = false;` and
  `rFailureReason = FailureReasonFor(ERROR_SUCCESS);`. The second is what keeps
  the zero-wait and immediate-success paths reporting exactly what
  `FailureReason()` reports for them today (`"Windows error 0"`, since
  `muiLastError` starts at `ERROR_SUCCESS`), so no early return leaves a caller
  reading an unwritten value.
- Replace the member write at `.cpp:37` with a loop-local
  `const DWORD uiLastError = ::GetLastError();` immediately followed by
  `rFailureReason = FailureReasonFor(uiLastError);`, and use `uiLastError` for
  every existing test in the loop (`.cpp:39`, `:43`, `:46`). Overwriting
  `rFailureReason` on each iteration reproduces today's behavior exactly, because
  today's queries report the last observed error only.
- Replace the member write at `.cpp:43` with
  `rbContentionObserved = rbContentionObserved || uiLastError == ERROR_SHARING_VIOLATION || uiLastError == ERROR_LOCK_VIOLATION;`,
  keeping the accumulate-across-iterations semantics.
- The destructor, `mhFile`, `mPath`, `IsValid()`, the denied-access budget logic
  and its comment at `.cpp:45`, the sleep interval, and the deadline loop are
  untouched.

### Call-site updates

Each site declares the two locals immediately before the `Guard`, passes them,
and keeps its existing `IsValid()` test and its existing outcome unchanged.
Sites that consume only one of the two still declare both; the unread one is
written by the callee and produces no warning.

- `Tools/WorktreeCli/LandingLockCommands.cpp:256-261` and `:394-399` — construct
  with the locals, then `Fail("could not acquire lock transition guard (" +
  failureReason + ")")` with byte-identical message text.
- `Tools/WorktreeCli/PlanScheduler.cpp:516-521`, `:690-695`, `:806-811`,
  `:844-849`, `:914-919` — construct with the locals plus the existing
  `kiSchedulerGuardWaitSeconds * 1'000, 10'000` timeouts, then
  `return bContentionObserved ? Failure("busy", kiExitStateConflict) :
  Failure("guard-unavailable");` — same codes, same exit codes, at all five
  sites.
- `Tools/AgentHarness/HarnessLockCommands.cpp:150-154` — same `Fail` message as
  the WorktreeCli landing sites.
- `Tools/AgentHarness/HarnessLockCommands.cpp:251-255` — `RefreshHarnessHeartbeat`
  reads neither out-param; it declares both, passes them, and keeps its bare
  `return false;`.

No documentation edit is required: `Tools/WorktreeCli/AGENTS.md` describes the
`busy`/`guard-unavailable` outcomes in behavior terms and never names a `Guard`
query, and `Tools/ToolCommon/AGENTS.md` never names one either. Both stay
correct as written. The convention itself is recorded once, by the parent Plan,
in `Engine/Source/File/AGENTS.md`.

## Critical files

- `Tools/ToolCommon/CoordinationStore.h` — `Guard` constructor declaration
  (`:27`), queries (`:34-39`), members (`:44-45`)
- `Tools/ToolCommon/CoordinationStore.cpp` — constructor (`:20-69`), query
  definitions (`:86-104`)
- `Tools/WorktreeCli/LandingLockCommands.cpp` — `:256-261`, `:394-399`
- `Tools/WorktreeCli/PlanScheduler.cpp` — `:516-521`, `:690-695`, `:806-811`,
  `:844-849`, `:914-919`
- `Tools/AgentHarness/HarnessLockCommands.cpp` — `:150-154`, `:251-255`

## In scope

- `toolcli::coordination::Guard`: adding the `rbContentionObserved` and
  `rFailureReason` constructor out-params ahead of the two defaulted timeout
  parameters; deleting `TimedOut()`, `ContentionObserved()`, `LastError()`,
  `FailureReason()`, `muiLastError`, and `mbContentionObserved`; adding the
  file-local `FailureReasonFor` helper and using loop-local error state inside
  the acquisition loop
- The nine `Guard` construction sites named in `## Critical files`: declaring and
  passing the two locals and rewriting their failure branches against those
  locals, with identical messages, codes, and exit codes

## Out of scope

- Any behavior change: no new, removed, or reworded message, no changed
  accept/reject decision, no changed exit code, no changed wait, retry, denied-
  access budget, or sleep behavior
- `Guard::IsValid()`, `mhFile`, `mPath`, the destructor, and its best-effort
  `DeleteFileW` release
- Every other entity in `CoordinationStore.h`/`.cpp`: `Locator`, `MakeLocator`,
  timestamp and hashing helpers, metadata read/write/validate helpers, and the
  lock record schema
- The landing-lock lease protocol, the plan scheduler's selection, claiming,
  healing, and completion rules, and the harness lock surface
- `.agents/scripts/New-PlanFile.ps1` and every other caller that only reads
  WorktreeCli's JSON output
- Introducing a result/status struct, error-code enum, exception type, or
  `std::expected`-style wrapper

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: `Tools/ToolCommon` and both AgentTools
executables are shared build/bootstrap coordination that can block other
sessions — a source change here requires the `/compile` candidate/promotion path
and lands through the landing gate — and the change spans two independently
owned tools, WorktreeCli and AgentHarness.

Invariants to preserve:

- Contention alone still reports `busy` as a state conflict (exit `2`), while an
  unusable lock file or its storage still reports `guard-unavailable` as an OS
  failure (exit `1`), exactly as `Tools/WorktreeCli/AGENTS.md` "Coordination
  State" describes.
- Contention is observed across the whole acquisition, not only its final
  iteration: a sharing or lock violation at any point proves a live holder even
  when the final error is an `ERROR_ACCESS_DENIED` delete-pending flicker.
- The denied-access budget still covers the current consecutive run only.
- Every scheduler state change still runs under an acquired guard, and the guard
  file is still released and deleted by the same destructor path.
- The reported failure detail stays runtime-only: it is never written to a lock
  record, a metadata file, or any JSON field beyond the existing message text.

## Acceptance criteria

- `Guard` exposes no post-construction failure query: a repository search over
  `Common/`, `Engine/`, `Projects/`, `DataPacker/`, and `Tools/` returns no hits
  for `TimedOut`, `ContentionObserved`, `FailureReason`, `muiLastError`, or
  `mbContentionObserved`.
- `WorktreeCli` and `AgentHarness` both build clean, and `BrokenEngineSandbox`
  client and server are unaffected.
- `.agents/scripts/Test-WorktreeCliPlanScheduler.ps1` returns unchanged results
  against the rebuilt executable.
- Contention path unchanged: with the scheduler guard file held open exclusively
  by another process, `plan validate` reports code `busy` and exits `2`.
- Guard-unavailable path unchanged: with `%LOCALAPPDATA%` pointed at an unusable
  location, `plan validate` reports code `guard-unavailable` and exits `1`.
- Landing-lock path unchanged: with the landing lock's `.guard` file held open
  exclusively, `lock claim` fails with the message
  `could not acquire lock transition guard (timed out)` and exits `1`.
- A successful acquisition still succeeds with no message emitted from any guard
  failure branch.

## Notes

- Origin: the Phase 1 inventory of
  `Documents/Plans/Engine/FailureReportingOutReferenceConvention.md`, split out
  under that Plan's own escalation clause so the parent stays Tier 2. Nothing
  here is a bug fix; the current code is correct as landed and no behavior may
  drift.
- The parent Plan and this one are order-independent and share no file. This
  Plan carries no dependency edge on it: if the parent is completed and deleted
  first, the convention statement it adds to `Engine/Source/File/AGENTS.md` is
  the written precedent; if it has not landed yet, `ReadAndValidateVersionHeader`
  in `Engine/Source/File/FileManager.h:309-318` is the same precedent in code.

## Coordination

`Documents/Plans/Agents/PlanValidateLintOnlyMode.md` edits the same
`Tools/WorktreeCli/PlanScheduler.cpp` `RunValidate` guard block (`:516-521`):
that Plan makes the whole scheduler block conditional on a new `--lint-only`
switch, while this Plan rewrites how that block reads the guard's failure
detail. The two are order-independent and no directional dependency is required;
whichever lands second locates the region by symbol rather than line number and
must preserve both the `--lint-only` gating and this Plan's out-reference call
shape, keeping the `busy` and `guard-unavailable` codes and exit codes intact.
