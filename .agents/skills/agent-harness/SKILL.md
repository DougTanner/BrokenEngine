---
name: agent-harness
description: >-
  Drive the Broken Engine client/server for automated verification. Use for
  runtime-observable acceptance criteria, replay determinism, or whenever the
  user or a plan's Verification section asks to launch, drive, query, or
  screenshot the game, or requests GPU frame capture or RenderDoc capture
  analysis.
allowed-tools: [PowerShell]
---

# Agent Interaction Harness

Drive the local headless server and rendered client through loopback, length-prefixed JSON. Use server port `27100`, client port `27101`, and the absolute adopted worktree. A scenario needing a second simultaneous client adds port `27102` (see the project's [Additional simultaneous client](../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md#additional-simultaneous-client) recipe). A local server is a development instance, not an Azure production server.

Each fenced PowerShell block is an independent shell call unless it is explicitly labeled
as temporary-script contents. Variables and functions are fence-local and do not survive
into another shell-call block. Retain claim, takeover, launch, and artifact outputs as
literal values in agent context, then substitute quoted string/path placeholders or
numeric PID placeholders into later calls. A variable in a code fence is valid only when
that fence assigns it, or when it is an automatic/environment variable. A temporary-script
contents block is written under `<absolute adopted worktree>\Temp` and then run by a
separate self-contained `pwsh -NoProfile -File '<absolute adopted worktree>\Temp\<driver filename>.ps1'`
shell call.

Read the focused references only when applicable:

- Read the command reference (`references/command-reference.md`) for the five engine-shared command schemas (`ping`, `quit`, `get_logs`, `set_log_level`, `crash_report_fixture`) and the `params`/`result` placement convention; the request/response envelope itself is defined in the Invoke commands section below. The selected project's `Projects/<Project>/Documents/AgentHarness.md` hub routes every game command schema, launch recipe, verification scenario, and game caveat to a focused reference.
- Read the private-LAN firewall reference (`references/private-lan-firewall.md`) only for an explicitly requested cross-machine or private-Wi-Fi scenario. Ordinary same-machine runs stay loopback-only and never inspect or change firewall state.
- Read the RenderDoc capture reference (`references/renderdoc.md`) only for a GPU frame-capture or capture-analysis scenario; it owns the `--renderdoc` launch, `renderdoc_capture`, and the headless `rdc_*` analysis scripts.

## Select the project

Target the project the latest `/compile` result built, unless the user or plan names a different one. Default today: BrokenEngineSandbox. Before launching, read the selected project's `Projects/<Project>/Documents/AgentHarness.md` hub; its machine-readable launch configuration owns the executable names and output directory, while its linked `launch.md` owns the executable launch recipe and project launch arguments; the root hub routes project command schemas and verification scenarios to focused children.

## Provision and claim

Provision the checkout and use only its provisioned primary AgentHarness output. Wrapper sessions use their existing WorktreeCli session owner; a non-worktree checkout may let the provisioner create a transient provisioning session.

Claim through `scripts/Invoke-HarnessClaim.ps1`. It requires the selected project's server and client executables (reading their names and `Output` directory from the project's root `## Machine-readable launch configuration`, not from the human `launch.md` recipe), proves that the pack version this worktree's source expects matches the version the supplied data directory's `.manifest` files carry, provisions the checkout, requires the resolved `AgentHarness.exe`, mints the owner token, and claims the lock, printing one compact `broken-engine-harness-claim/v1` JSON object. When another session holds the lock, it provisions once and then retries only the claim until it succeeds or its 500-second wait budget expires, so the invocation below is itself the wait — never hand-write a poll loop, invent a sleep window, or pass a wait budget of your own. Pass the latest `/compile` result's normalized `GameDataDirectory` verbatim as `-GameDataDirectory` — the same packed data the launch below selects with `--data-directory`. Add `-Configuration <name>` only for a build other than `Debug`. Run this from the session worktree root:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<absolute adopted worktree>' -Session '<short task label>' -GameDataDirectory '<normalized Data path>'
```

Exit `0` (`status` `pass`) supplies the `owner` token, the resolved `agentHarness` path, and the `claim` record; retain those values outside shell state and substitute them as literal `<owner token>` and `<absolute AgentHarness path>` values in later calls, then report the claim metadata verbatim. Every result — pass, blocked, or error — echoes the supplied data path as `gameDataDirectory`, canonicalized once the pack-version check resolves it, and carries a `packVersion` field that is `null` except on the `claim.pack-version-mismatch` block below. Exit `2` (`status` `blocked`) carries one of three codes, and any of them ends the attempt here: the script never launches the game, so the Launch and ping-wait steps below are not performed and their absence is not a further failure to diagnose. `claim.executable-missing` means a required project executable is absent for the requested configuration: no provisioning ran and no lock was taken, so route `/compile` for the named executables and re-enter. `claim.pack-version-mismatch` means the pack version derived from this worktree's source is not the version the data carries, so a launch would die at the runtime manifest check as a hidden modal dialog instead of a diagnosable failure; nothing was provisioned and no lock was taken, and the payload's `packVersion` carries `expected`, `found`, `mismatchedManifests`, and `manifestCount`, so rebuild or re-export to pair them rather than relaunching. `claim.foreign-owner` means the wait expired and another session still holds the lock: the payload's `currentOwner` carries the last attempt's `claimedAt` and the derived `holdSeconds` — heartbeat never advances `claimedAt` — so reorder non-harness work or escalate rather than re-running the wait blind. The script never steals and never touches a foreign owner's processes, heartbeat, or claim, whether it acquires the lock or waits the budget out. Exit `1` is a failure naming its step (`claim.repository-missing`, `claim.provisioner-missing`, `claim.launch-doc-unreadable`, `claim.pack-version-underivable`, `claim.game-data-unreadable`, `claim.provision-failed`, `claim.harness-missing`, `claim.token-failed`, `claim.metadata-unreadable`, `claim.failed`, or `internal.error`); stop and report it. Never reconstruct provisioning, harness-path resolution, token minting, the pack version check, or the claim inline.

Hold one owner token across relaunches. Every socket command requires `--owner '<owner token>'`: after request acquisition, AgentHarness proves ownership before Winsock/connect, refreshes it when the 60-second interval becomes due while transport remains active, and refreshes it once more before response output. Ownership loss stops local response handling with exit `1`; game work already dispatched cannot be retracted. After the first command, compare `lock status` before/after and require `heartbeatAt` to advance. `lock status` prints ordinary pretty-printed JSON; the backslashes in its `worktree` field are standard JSON escaping of the Windows path, not nested or stringified JSON. During a non-harness phase that may exceed five minutes, run `lock heartbeat --key default --owner '<owner token>'`; otherwise quit and release before the phase, then reclaim afterward.

Never build AgentHarness or write through shared Output links during routine harness work; `/compile` owns AgentTools newly-built-binary production and promotion. The claim step itself verifies that the selected project's server and client executables exist, so a missing one blocks the claim instead of surfacing later.

## Ownership and takeover

A claim is stale only when its reported heartbeat is older than five minutes. Never disturb a fresh owner. For a stale owner:

1. Record the old owner and resolve listeners on all three fixed ports with `Get-NetTCPConnection -State Listen`; a port with no listener simply has nothing to validate or quit.
2. Resolve each `OwningProcess` through `Get-CimInstance Win32_Process`. Require its normalized `ExecutablePath` to equal the active project's expected server/client executable (per its harness doc) beneath the reported owner worktree and its `CommandLine` to contain the matching `--agent-port`. If listener identity, command line, or worktree cannot be proved, return `BLOCKED`; do not stop it.
3. Send `quit` to every validated port with `--owner '<old owner token>'`, so no stale process survives the ownership change. Wait only on the exact validated listener PIDs. If a validated PID remains and the command could not connect, stop only that exact PID with `Stop-Process -Id`; never use a process-name search or broad kill.
4. Generate a new token and conditionally steal with the recorded owner:

```powershell
$NewOwner = (& '<absolute AgentHarness path>' lock token).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($NewOwner)) { throw 'Harness token generation failed' }
$StealResponse = & '<absolute AgentHarness path>' lock steal --key default --expect '<old owner token>' --owner $NewOwner --session '<session label>' --worktree '<absolute adopted worktree>'
if ($LASTEXITCODE -ne 0) { throw 'Harness ownership changed during takeover' }
$StealResponse
```

Retain the returned lock metadata's `owner` value outside shell state as the new literal
owner token for later calls. Old-owner cleanup may refresh its heartbeat; that does not
invalidate staleness established before cleanup. `--expect` still protects against an
ownership change.

## Launch

Require the latest `/compile` result's `DataBuildMode`, `RunDataPacker=false`, and normalized `GameDataDirectory`.

Never infer an identity, switch data mode, fall back to Shared data, or run DataPacker/Gaea/texture export.

Use the compiled configuration suffix. Ordinary same-machine runs pass `--loopback-only`. Create log parents under `<absolute adopted worktree>\Temp`. Do not change process working directories; `--data-directory` is the only override selecting packed assets, and `--app-data-directory` is the only override selecting where saves, settings, caches, and replays are written.

On every host, launch with hidden `Start-Process -PassThru`, retain the returned exact non-null numeric PIDs and each process's `StartTime.ToUniversalTime().ToString('o')` identity in the launch snapshot, and use only those PIDs for lifecycle checks. Quote path-valued arguments because `Start-Process` joins `ArgumentList` items.

Every launch is process-checked on every host through `scripts/Invoke-AgentHarnessProcessCheck.ps1`, whose `Baseline`, `Register`, and `Check` actions you run directly, each as its own `pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 ...` shell call in the form shown below. The wait helpers and `scripts/Invoke-HarnessRelease.ps1` run the checker themselves from the `-ProcessCheckStatePath` you pass them, so never invoke it yourself mid-wait. Generate one absolute state path per run under `<absolute adopted worktree>\Temp` that no earlier run used, so the run starts with no state file and inherits no earlier run's registrations, and pass that same path to every action, wait helper, and release call of that run. Per role, before its `Start-Process`, run `-Action Baseline -StatePath '<absolute process-check state path>' -Role '<role>' -GameName '<Game Name>'`, adding `-ConfiguredAppDataRoot '<path>'` whenever that launch passes `--app-data-directory`; the project harness doc owns the role names, game names, and concrete paths. Capture the `-PassThru` result's `.Id` and start-time identity, run `-Action Register -StatePath '<absolute process-check state path>' -Role '<role>' -ProcessId <PID> -StartTimeUtc '<start time UTC>'` immediately, then run `-Action Check` immediately after that registration, before any readiness wait:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Check -StatePath '<absolute process-check state path>'
```

Every action prints one compact `broken-engine-harness-process-check/v1` JSON object with `status`, `code`, and `message`; `Check` adds `findings`, each naming its `role`, `reportPath`, `headline`, and `evidencePath` (the retained evidence directory). Exit `0` means every registered role still holds its registered exact identity — the registered PID plus its registered start time, never a process-name search. Exit `2` means a registered role exited unexpectedly — a zero exit code never makes a disappearance intentional — and its findings carry any crash report written since that role's pre-launch baseline; a report unchanged since that baseline is stale and is never announced as a new crash, and a role that exited without changing any report reports `reportPath` and `headline` as `null` rather than inventing evidence. Exit `1` is a setup or state failure. Never delete, move, or rewrite a reported crash report or its retained evidence; report it by path. The checker reports only through the command, poll, or check that is already active and never promises out-of-band notification while none is.

The selected project's hub owns the machine-readable executable paths, while its linked `launch.md` owns the concrete launch block — extra project launch arguments, game-specific connect notes, and fresh-state choices. A server-side reset command resets server state only, so a comparison requiring fresh client state must launch under an app-data directory that did not exist before that launch; use the `launch.md` recipe. The `--agent-port` values are the fixed ports above and are embedded in that recipe.

Agent-mode executables start minimized without activation. Capture commands temporarily restore the client without activation and re-minimize it. A criterion that depends on render progression must hold a visible window for its duration — capture restores an iconic window only for the readback and re-minimizes it, so a capture taken mid-scenario silently returns the client to the non-rendering state. Use `window_state` to hold visibility across such a scenario. Omit `--windowed` only when native-resolution UI sizing/readability is part of acceptance. Add the optional `--renderdoc` client argument only for GPU frame capture; it force-loads renderdoc.dll and drops the Vulkan validation layer, so keep it off ordinary runs (see the RenderDoc capture reference `references/renderdoc.md`).

After launching either executable, wait for readiness with `scripts/Wait-HarnessPing.ps1` before the first real command. It handles one port per call, so launch order stays with you:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Wait-HarnessPing.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -Port 27100 -TimeoutSeconds 120 -ProcessCheckStatePath '<absolute process-check state path>'
```

The script polls until the port answers `ok:true`, tolerating the individual timeouts long client startup (terrain elevation and priority-texture waits) produces after connect already succeeded. Exit `0` (`status` `pass`) reports the observed `tick`; `tick` of `-1` means the listener is up but the game is not yet created (see the command reference `references/command-reference.md` for `ping`). Exit `2` (`status` `blocked`, code `ping.timeout`) reports the attempt count and elapsed time, and blocks the first real command. Exit `1` is a setup failure. Never hand-write a ping loop or invent a sleep window in its place.

The optional `-ProcessCheckStatePath` is this run's process-check state path; supply it whenever the roles are registered, which the launch sequence above makes the ordinary case. While the wait is still unsatisfied, the helper checks the registered roles before each retry and sleep, so a dead process ends the wait early instead of consuming the remaining timeout: exit `2` (`status` `blocked`) with code `process.unexpected-exit` and the check's `findings`, rather than `ping.timeout`. A poll that succeeds returns without a further check, so an unexpected exit after it surfaces at the next lifecycle point instead — the next command's transport-failure check or release's entry check. A checker that could not run at all is advisory: it is reported once as `processCheckWarning` and polling continues. Every existing exit-code meaning is unchanged.

Invoke `scripts/Wait-IslandSceneReady.ps1` only after launch and only when an approved criterion depends on island footprints or rendered islands. Supply the exact literal `-AgentHarness '<absolute AgentHarness path>'`, harness-lock `-Owner '<owner token>'`, client `-ClientPort`, bounded `-TimeoutSeconds`, absolute ignored `-ArtifactPath '<absolute artifact path>'`, and the optional `-ProcessCheckStatePath '<absolute process-check state path>'`.

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Wait-IslandSceneReady.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ClientPort 27101 -TimeoutSeconds 120 -ArtifactPath '<absolute artifact path>' -ProcessCheckStatePath '<absolute process-check state path>'
```
 The helper requires client `ping` with `tick >= 0`, restores the client with `window_state {minimized:false}`, and requires the complete `clientGridCoord` plus nonempty island footprints to be byte-stable across two consecutive normalized samples. Exit `0` is usable only with a `broken-engine-island-scene-readiness/v1` artifact reporting `Status:success`, `Code:ready`, and `Ready:true`; missing, malformed, or failed evidence blocks the criterion. Supply `-ProcessCheckStatePath` whenever the roles are registered, which the launch sequence above makes the ordinary case: each readiness iteration then checks them and returns early on an unexpected exit with exit `1` and an artifact reporting `Status:failure`, `Code:ProcessUnexpectedExit`, `Ready:false`, and the check's findings in `Findings`, instead of consuming the remaining timeout. A checker that could not run at all is advisory `ProcessCheckWarning` in the artifact and the wait continues. Every existing exit-code meaning is unchanged. The selected project document owns the concrete argument values and the evidence check around this call.

Before relinking or relaunching, send `quit` and wait for the exact numeric PID from the latest launch snapshot. A live executable locks its image. Do not launch a duplicate to displace it — with `SO_REUSEADDR`, a duplicate on the same port binds alongside the live listener instead of failing fast, and connection routing between the two becomes nondeterministic. The engine listener sets `SO_REUSEADDR` and briefly retries address-in-use binds, so relaunch immediately once the exact PID has exited and rely on the ping poll for readiness — never invent a sleep window. Quitting a role to relink or relaunch it is followed immediately by a fresh `Baseline` and `Register` for the new process, in that order around its `Start-Process`; between that deliberate quit and re-registration the role is not checked, so run no process check in that window.

### Additional simultaneous client

A criterion that needs two clients connected at once — a late joiner beside a client that must stay connected — launches an additional client after the standard pair. Disconnecting and reconnecting the one client is not a substitute when the criterion depends on state the server drops when its last client leaves.

The additional client is the documented client launch with three changes: its own fixed agent port above, its own `--app-data-directory` root, and its own `--log-file`. Every other argument is unchanged, including the same `--data-directory` — all processes of one run must select the same asset data root. Each additional client needs its own app-data root because of how one root separates client and server state (`Engine/Source/File/AGENTS.md`). The project `launch.md` owns the concrete launch block.

It joins this run's process-identity flow as its own role in the same state path: `Baseline` with its own `-ConfiguredAppDataRoot` and the client `-GameName`, then `Register` and the immediate `Check`, exactly as for the first client. Wait for its readiness with `Wait-HarnessPing.ps1 -Port 27102` after the standard `27100` then `27101` waits.

`scripts/Invoke-HarnessRelease.ps1` covers only ports `27100`/`27101` and the server/client PIDs you supply, so it never quits an additional client. Before that release call, quit the additional client through its own port and confirm that its exact retained PID is gone — a wait that times out is a failed run, so stop or investigate that PID rather than continuing — then run one fresh `Baseline` for its role alone; that clears its registration so release's entry check does not report the deliberate quit as an unexpected exit. Release then proceeds unchanged.

## Invoke commands

Prefer stdin JSON and always capture stdout even when exit is nonzero:

```powershell
'{"cmd":"ping"}' | & '<absolute AgentHarness path>' --owner '<owner token>' --port 27100 -
```

For server-window verification, read the project-owned [server-window recipe](../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/server-window.md#server-gdi-monitoring-window-capture-and-click).

Use `--timeout-ms N` for a deferred client command; default is 15000 and maximum is 600000. The CLI retries the loopback connect until this same deadline rather than making a single short attempt, so a probe against a dead port consumes the full budget — give a quick negative probe a small `--timeout-ms` (e.g. `1000`). Exit `0` means parsed `ok:true`, exit `2` means parsed `ok:false`, and exit `1` means transport, usage, or OS failure.

After a transport failure, run the process check's `-Action Check` (Launch section above) once before any retry and report its result through this same command — that active command is the only channel the checker reports through, so a lost connection is diagnosed here rather than waited out. A check exit of `2` means the endpoint exited unexpectedly, including a handled exception that terminated with exit code `0` and wrote a crash report, so retry nothing and report the finding's role, report path, exception headline, and retained evidence path. A check exit of `0` leaves the transport failure an ordinary one and the retry proceeds.

Request envelope: `{"cmd":"<name>","params":{...},"id":<optional JSON>}`. Omit `params` only for parameterless commands. Unknown top-level fields fail. Responses are `{"id":<echo|null>,"ok":true,"result":{...}}` or `{"id":...,"ok":false,"error":"..."}`. Parameters are an external trust boundary and invalid values must produce the error envelope.

The channel permits one request still in progress. Client deferred commands occupy it until completion; another AgentHarness call waits rather than bypassing it. Do not overlap calls.

Write any multi-line PowerShell driver — anything with `function` definitions, loops, or more than a few statements — to a script file under `<absolute adopted worktree>\Temp` and run it with `pwsh -File`. Never compact such a driver into a semicolon-joined one-liner; compaction corrupts function definitions (observed: `function Snap([string]$p)` mangled into an unrecognized `Snap$d` token).

## Performance investigations

- GPU performance investigations run the client fullscreen at 4K. Restore the client, send `fullscreen {"on":true}`, and require the response to report `fullscreen:true`, `width:3840`, and `height:2160` before collecting measurements. Keep the window visible for the full measurement interval. If the active display cannot provide that framebuffer, report the GPU criterion `BLOCKED` instead of substituting a smaller or windowed mode.
- CPU performance investigations build and run the measured client or server with the `Profile` configuration/target. Debug and Release CPU timings are not acceptance evidence for a CPU performance investigation.

## Authoritative verification

The selected project's hub routes the concrete setup recipe — [common verification](../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md), endpoint commands, and [replay determinism](../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md) or [cross-cell](../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md) scenario material — including the commands that seed server state, confirm client connection, and address UI. Regardless of project, hold these verification-evidence principles:

- Verify with the narrowest observable combination of the project's scene, UI, screenshot, server-query, and log commands. `describe_ui`, scene/server queries, and `get_logs` close a criterion more cheaply than pixels; reach for a capture only when the criterion is genuinely about what was rendered. Stable counts such as players should agree exactly; allow bounded tick drift for collections whose entries are being added and removed rapidly.
- Release through the lifecycle and release section below.

## Lifecycle and release

After every successful, failed, crashed, or abandoned launch attempt, release through `scripts/Invoke-HarnessRelease.ps1`. One call performs the whole sequence, and it is safe after a crashed run whose PIDs are already gone:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ServerPid <server PID> -ClientPid <client PID> -ProcessCheckStatePath '<absolute process-check state path>'
```

Pass only exact non-null numeric `serverPid`/`clientPid` values retained from the latest launch snapshot; replace the numeric placeholders and omit either argument for a process that never started. The latest snapshot is authoritative, including after a partial launch failure. The script quits both ports with the owner (server quit autosaves), waits for those exact PIDs, stops only a supplied exact PID that survived the quit, and runs `lock release` only once every supplied PID is confirmed absent. It never searches by process name and never stops a PID you did not supply. An additional client is outside those two ports, so Additional simultaneous client above governs its shutdown before this call.

Supply the optional `-ProcessCheckStatePath` whenever the roles are registered, which the launch sequence above makes the ordinary case. The script then runs `-Action Check` once at entry, before the first quit, so a role that already crashed is observed while its evidence is still attributable, and reports the findings as an additive `crashFindings` array in its JSON, with an advisory `processCheckNote` when the checker returned no readable findings. Quit, exact-PID wait, stop, cleanup, and lock-release ordering and every exit code are unchanged, and the reported crash reports and their evidence directories are never deleted, moved, or rewritten. Exit `0` with a non-empty `crashFindings` therefore still means the run failed: cleanup succeeded and the lock was released, but a registered role had exited unexpectedly.

Exit `0` (`status` `pass`) means every supplied PID is absent and the lock was released; report the payload's per-step outcomes verbatim, including `crashFindings`. Exit `2` (`status` `blocked`, code `release.process-survived`) means a supplied PID is still alive: no release was attempted, the claim stays held, and you decide whether to retry the release or escalate to the user. Exit `1` is a failure naming its step, including `release.lock-owner-mismatch` and `release.lock-absent`, which are never a success. Never reconstruct the quit, wait, stop, or release steps inline.

An owner mismatch is a hard stop. Never remove coordination state manually.

## Process verification report

Run plan-provided runtime steps when present; otherwise derive the smallest live checks for runtime-observable criteria only. Report each criterion `PASS`, `FAIL`, or `BLOCKED` with exact command/query/scene/UI/screenshot/log evidence. Treat setup limitations as blocked checks.

A process-check finding — from launch, a poll, a transport-failure check, or release entry — is evidence like any other: `FAIL` the criterion that was live when it was observed, citing the finding's role, report path, exception headline, and retained evidence path. A finding observed outside any criterion, such as at release entry, fails the run and is reported as a residual with the same four values. A criterion whose endpoint crashed before it could be exercised is `BLOCKED` on that crash, not silently retried. Do not diagnose or edit a failure in this role; return reproducing commands and evidence to the main agent for the `/resolve-findings` decision and affected-check retest.

Captures stay on disk. The `screenshot` result `{path, width, height}` is the evidence — cite it by path. Loading an image into context is a deliberate act for a check that genuinely needs pixels, and the report names which check and why.

- Prefer a script over the image. Most visual criteria — frame non-black, region matches an expected color, two captures differ, pixel count past a threshold — are assertions a few lines of code settle more precisely than an eye on a downscaled JPEG. Write the driver under `<absolute adopted worktree>\Temp` and run it with `pwsh -File` as above; its stdout is a few bytes of decisive text instead of megabytes of image. The same holds for a large driver result file: query it targeted first, and read the whole record only after a targeted read provably misses.
- Request only the resolution the check needs. `screenshot` downscales through `maxWidth` (default 1568) and `quality` (default 80), and those defaults are sized for UI readability. A "did terrain render at all" check does not need them; pass a `maxWidth` and `quality` matched to the assertion as part of designing the check.

If a required command, parameter, result field, query, or input primitive is missing, return that criterion `BLOCKED`. Name the missing capability and the narrowest harness extension that would expose it. The main agent decides whether the authorized change includes that extension or whether user authority/criterion revision is required. Never fake state with pixel guessing or log scraping, create an out-of-scope runtime edit, waive the gate with a follow-up plan, or silently skip the criterion.

End delegated process verification with:

```text
Files changed: none
Functions/regions touched: none
Residuals:
- <failed criterion, crash finding (role, report path, headline, evidence path), blocked prerequisite/missing capability, or none>
```

Return the complete report inline. A failed or blocked in-scope criterion remains incomplete until the capability/environment is supplied or the user explicitly revises acceptance.

## Durable caveats

The selected project's focused verification, replay, and endpoint references own game-specific caveats (injection/pause/replay timing, queued-update semantics, and command source-file ownership). These engine-generic caveats hold for every project:

- Physical mouse, keyboard, wheel, and gamepad input is suppressed for an agent-mode client. Synthetic input is the sole game/ImGui source; Alt+F4 and window Close still quit. Focus messages remain real, and audio requires real OS focus.
- Keep ownership warm during long soaks. Never leave a background heartbeat/poll process after the session.
