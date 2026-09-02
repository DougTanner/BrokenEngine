# Agent Interaction Harness Worker

Harness steps and rules for the worker dispatched with
[`../SKILL.md`](../SKILL.md), which owns the purpose, the triggers, the inputs,
and the handoff this run returns.

## Contents

- [Steps](#steps)
  - [Select the project](#select-the-project)
  - [Provision and claim](#provision-and-claim)
  - [Ownership and takeover](#ownership-and-takeover)
  - [Launch](#launch)
    - [Additional simultaneous client](#additional-simultaneous-client)
  - [Invoke commands](#invoke-commands)
  - [Authoritative verification](#authoritative-verification)
  - [Lifecycle and release](#lifecycle-and-release)
  - [Process verification report](#process-verification-report)
- [Rules](#rules)
  - [Performance investigations](#performance-investigations)
  - [Durable caveats](#durable-caveats)

## Steps

1. Use server port `27100`, client port `27101`, and the absolute adopted
   worktree. Done when this run's ports and adopted worktree path are fixed.

   - A scenario needing a second simultaneous client adds port `27102` (see the
     project's [Additional simultaneous client](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md#additional-simultaneous-client)
     recipe).
   - A local server is a development instance, not an Azure production server.

2. Read the focused references only when applicable. Done when every reference
   this scenario makes applicable has been read:

   - Read the command reference (`command-reference.md`) for the five
     engine-shared command schemas (`ping`, `quit`, `get_logs`,
     `set_log_level`, `crash_report_fixture`) and the `params`/`result`
     placement convention; the request/response envelope itself is defined in
     the Invoke commands steps below. The selected project's
     `Projects/<Project>/Documents/AgentHarness.md` hub routes every game
     command schema, launch recipe, verification scenario, and game caveat to a
     focused reference.
   - Read the private-LAN firewall reference (`private-lan-firewall.md`) only
     for an explicitly requested cross-machine or private-Wi-Fi scenario.
     Ordinary same-machine runs stay loopback-only and never inspect or change
     firewall state.
   - Read the RenderDoc capture reference (`renderdoc.md`) only for a GPU
     frame-capture or capture-analysis scenario; it owns the `--renderdoc`
     launch, `renderdoc_capture`, and the headless `rdc_*` analysis scripts.

### Select the project

3. Target the project the latest `/compile` result built, unless the user or
   plan names a different one. Default today: BrokenEngineSandbox. Done when the
   target project is named.

4. Read the selected project's `Projects/<Project>/Documents/AgentHarness.md`
   hub before launching. Done when that hub has been read.

   - Its machine-readable launch configuration owns the executable names and
     output directory, while its linked `launch.md` owns the executable launch
     recipe and project launch arguments.
   - The root hub routes project command schemas and verification scenarios to
     focused children.

### Provision and claim

5. Claim through `../scripts/Invoke-HarnessClaim.ps1`, run from the session
   worktree root. Done when that single invocation has returned its JSON object:

   ```powershell
   pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<absolute adopted worktree>' -Session '<short task label>' -GameDataDirectory '<normalized Data path>'
   ```

   - It requires the selected project's server and client executables (reading
     their names and `Output` directory from the project's root
     `## Machine-readable launch configuration`, not from the human `launch.md`
     recipe) unless you pass `-ServerOnly` to declare a server-only session,
     which narrows that existence check to the server executable.
   - Declare `-ServerOnly` from what your scenario actually launches; it binds
     the whole claimed session, so a `-ServerOnly` session must not launch a
     client under that claim, and it is never inferred from the project's server
     command document, which is not uniformly client-free.
   - It also proves that the pack version this worktree's source expects matches
     the version the supplied data directory's `.manifest` files carry,
     provisions the checkout, requires the resolved `AgentHarness.exe`, mints
     the owner token, and claims the lock, printing one compact
     `broken-engine-harness-claim/v1` JSON object.
   - When another session holds the lock, it provisions once and then retries
     only the claim until it succeeds or its 500-second wait budget expires, so
     the invocation above is itself the wait — never hand-write a poll loop,
     invent a sleep window, or pass a wait budget of your own.
   - Pass the latest `/compile` result's normalized `GameDataDirectory` verbatim
     as `-GameDataDirectory` — the same packed data the launch below selects
     with `--data-directory`.
   - Add `-Configuration <name>` only for a build other than `Debug`.

6. Use only the provisioned primary AgentHarness output that claim resolved.
   Done when this run uses only that provisioned output under the session owner
   the claim settled.

   - Wrapper sessions use their existing WorktreeCli session owner; a
     non-worktree checkout may let the provisioner create a transient
     provisioning session.

7. Read the claim result and act on its exit code. Done when the claim is held
   with its values retained, or the reported block or failure is reported and
   the attempt ends.

   - Exit `0` (`status` `pass`) supplies the `owner` token, the resolved
     `agentHarness` path, and the `claim` record; retain those values outside
     shell state and substitute them as literal `<owner token>` and
     `<absolute AgentHarness path>` values in later calls, then report the claim
     metadata verbatim.
   - Every result — pass, blocked, or error — echoes the supplied data path as
     `gameDataDirectory`, canonicalized once the pack-version check resolves it,
     and carries a `packVersion` field that is `null` except on the
     `claim.pack-version-mismatch` block below.
   - Exit `2` (`status` `blocked`) carries one of three codes, and any of them
     ends the attempt here: the script never launches the game, so the Launch
     and ping-wait steps below are not performed and their absence is not a
     further failure to diagnose.
   - `claim.executable-missing` means a required project executable is absent
     for the requested configuration: no provisioning ran and no lock was taken,
     so route `/compile` for the named executables and re-enter; when only the
     client executable is named and this session's scenario launches only the
     server, re-enter with `-ServerOnly` instead of building the client.
   - `claim.pack-version-mismatch` means the pack version derived from this
     worktree's source is not the version the data carries; nothing was
     provisioned and no lock was taken.
     - This preflight block catches up front a failure that would otherwise
       happen deep inside launch, at the runtime manifest check, exiting before
       the harness ping and surfacing as the Launch step's process-check result.
     - The payload's `packVersion` carries `expected`, `found`,
       `mismatchedManifests`, and `manifestCount`, so rebuild or re-export to
       pair them rather than relaunching.
   - `claim.foreign-owner` means the wait expired and another session still
     holds the lock: the payload's `currentOwner` carries the last attempt's
     `claimedAt` and the derived `holdSeconds` — heartbeat never advances
     `claimedAt` — so reorder non-harness work or escalate rather than
     re-running the wait blind.
   - The script never steals and never touches a foreign owner's processes,
     heartbeat, or claim, whether it acquires the lock or waits the budget out.
   - Exit `1` is a failure naming its step (`claim.repository-missing`,
     `claim.provisioner-missing`, `claim.launch-doc-unreadable`,
     `claim.pack-version-underivable`, `claim.game-data-unreadable`,
     `claim.provision-failed`, `claim.harness-missing`, `claim.token-failed`,
     `claim.metadata-unreadable`, `claim.failed`, or `internal.error`); stop and
     report it.
   - Never reconstruct provisioning, harness-path resolution, token minting, the
     pack version check, or the claim inline.

8. Hold one owner token across relaunches. Done when the same owner token covers
   every command of this run.

   - Every socket command requires `--owner '<owner token>'`: after request
     acquisition, AgentHarness proves ownership before Winsock/connect,
     refreshes it when the 60-second interval becomes due while transport
     remains active, and refreshes it once more before response output.
   - Ownership loss stops local response handling with exit `1`; game work
     already dispatched cannot be retracted.

9. After the first command, compare `lock status` before/after and require
   `heartbeatAt` to advance. Done when that comparison shows `heartbeatAt`
   advanced.

   - `lock status` prints ordinary pretty-printed JSON; the backslashes in its
     `worktree` field are standard JSON escaping of the Windows path, not nested
     or stringified JSON.

10. During a non-harness phase that may exceed five minutes, run
    `lock heartbeat --key default --owner '<owner token>'`; otherwise quit and
    release before the phase, then reclaim afterward. Done when every such phase
    is covered by that heartbeat or by that quit-and-reclaim.

### Ownership and takeover

11. Treat a claim as stale only when its reported heartbeat is older than five
    minutes. Never disturb a fresh owner. Done when the claim is classified
    fresh, in which case steps 12 through 15 do not run, or stale, in which case
    they do.

12. Record the old owner and resolve listeners on all three fixed ports with
    `Get-NetTCPConnection -State Listen`; a port with no listener simply has
    nothing to validate or quit. Done when the old owner and every live listener
    are recorded.

13. Resolve each `OwningProcess` through `Get-CimInstance Win32_Process` and
    validate it against the requirements below. Done when every listener is
    validated or the run returns `BLOCKED`.

    - Require its normalized `ExecutablePath` to equal the active project's
      expected server/client executable (per its harness doc) beneath the
      reported owner worktree.
    - Require its `CommandLine` to contain the matching `--agent-port`.
    - If listener identity, command line, or worktree cannot be proved, return
      `BLOCKED`; do not stop it.

14. Send `quit` to every validated port with `--owner '<old owner token>'`, so
    no stale process survives the ownership change. Wait only on the exact
    validated listener PIDs. Done when every validated PID is gone.

    - If a validated PID remains and the command could not connect, stop only
      that exact PID with `Stop-Process -Id`; never use a process-name search or
      broad kill.

15. Generate a new token and conditionally steal with the recorded owner. Done
    when the steal call has returned lock metadata and its `owner` value is
    retained:

    ```powershell
    $NewOwner = (& '<absolute AgentHarness path>' lock token).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($NewOwner)) { throw 'Harness token generation failed' }
    $StealResponse = & '<absolute AgentHarness path>' lock steal --key default --expect '<old owner token>' --owner $NewOwner --session '<session label>' --worktree '<absolute adopted worktree>'
    if ($LASTEXITCODE -ne 0) { throw 'Harness ownership changed during takeover' }
    $StealResponse
    ```

    - Retain the returned lock metadata's `owner` value outside shell state as
      the new literal owner token for later calls.
    - Old-owner cleanup may refresh its heartbeat; that does not invalidate
      staleness established before cleanup. `--expect` still protects against an
      ownership change.

### Launch

16. Fix this launch's configuration and directory arguments. Done when the
    configuration suffix, `--loopback-only`, the log parents, and both directory
    overrides are fixed for this launch.

    - Use the compiled configuration suffix.
    - Ordinary same-machine runs pass `--loopback-only`.
    - Create log parents under `<absolute adopted worktree>\Temp`.
    - Do not change process working directories; `--data-directory` is the only
      override selecting packed assets, and `--app-data-directory` is the only
      override selecting where saves, settings, caches, and replays are written.

17. Take the concrete launch block from the selected project's documents. Done
    when this run's launch block comes from that recipe with the fixed ports
    above.

    - The selected project's hub owns the machine-readable executable paths,
      while its linked `launch.md` owns the concrete launch block — extra
      project launch arguments, game-specific connect notes, and fresh-state
      choices.
    - A server-side reset command resets server state only, so a comparison
      requiring fresh client state must launch under an app-data directory that
      did not exist before that launch; use the `launch.md` recipe.
    - The `--agent-port` values are the fixed ports above and are embedded in
      that recipe.

18. Hold window state deliberately. Done when window state and these optional
    arguments match what the criterion requires.

    - Agent-mode executables start minimized without activation. Capture
      commands temporarily restore the client without activation and re-minimize
      it.
    - A criterion that depends on render progression must hold a visible window
      for its duration — capture restores an iconic window only for the readback
      and re-minimizes it, so a capture taken mid-scenario silently returns the
      client to the non-rendering state. Use `window_state` to hold visibility
      across such a scenario.
    - Omit `--windowed` only when native-resolution UI sizing/readability is
      part of acceptance.
    - Add the optional `--renderdoc` client argument only for GPU frame capture;
      it force-loads renderdoc.dll and drops the Vulkan validation layer, so
      keep it off ordinary runs (see the RenderDoc capture reference
      `renderdoc.md`).

19. Process-check every launch on every host through
    `../scripts/Invoke-AgentHarnessProcessCheck.ps1`. Done when its `Baseline`,
    `Register`, and `Check` actions are the only way this run checks process
    identity.

    - Run each action directly, as its own
      `pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 ...`
      shell call in the form shown below.
    - The wait helpers and `../scripts/Invoke-HarnessRelease.ps1` run the
      checker themselves from the `-ProcessCheckStatePath` you pass them, so
      never invoke it yourself mid-wait.

20. Generate one absolute state path per run under
    `<absolute adopted worktree>\Temp` that no earlier run used. Done when that
    same path is the one passed to every action, wait helper, and release call
    of that run.

    - An unused path makes the run start with no state file, so it inherits no
      earlier run's registrations.

21. Per role, before its `Start-Process`, run
    `-Action Baseline -StatePath '<absolute process-check state path>' -Role '<role>' -GameName '<Game Name>'`.
    Done when every role has a baseline.

    - Add `-ConfiguredAppDataRoot '<path>'` whenever that launch passes
      `--app-data-directory`.
    - The project harness doc owns the role names, game names, and concrete
      paths.

22. On every host, launch each role with hidden `Start-Process -PassThru`. Done
    when the launch snapshot holds every launched process's exact PID and
    start-time identity.

    - Retain the returned exact non-null numeric PID and its
      `StartTime.ToUniversalTime().ToString('o')` identity in the launch
      snapshot, and use only those PIDs for lifecycle checks.
    - Quote path-valued arguments because `Start-Process` joins `ArgumentList`
      items.

23. Capture the `-PassThru` result's `.Id` and start-time identity and run
    `-Action Register -StatePath '<absolute process-check state path>' -Role '<role>' -ProcessId <PID> -StartTimeUtc '<start time UTC>'`
    immediately. Done when every role has a registration.

    - `Register` also starts a background watcher on the registered PID so
      `Check` can report that process's exit code, and reports in its `message`
      and its `watcherBound` result field whether the watcher bound; no extra
      step is required from you either way.

24. Run `-Action Check` immediately after that registration, before any
    readiness wait. Done when every registration has its immediate check:

    ```powershell
    pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Check -StatePath '<absolute process-check state path>'
    ```

25. Read each process-check result from its JSON object. Done when every action
    of this run is classified from that object.

    - Every action prints one compact
      `broken-engine-harness-process-check/v1` JSON object with `status`,
      `code`, and `message`; `Check` adds `findings`, each naming its `role`,
      `reportPath`, `headline`, `exitCode`, and `evidencePath` (the retained
      evidence directory).
    - `exitCode` is the registered process's exit code, or `null` when the exit
      code was not captured — usually because the watcher could not bind it, a
      process that had already exited when `Register` ran; prove an intentional
      clean exit (after `crash_report_fixture`, for example) from `exitCode`,
      never from a second launch.
    - Exit `0` means every registered role still holds its registered exact
      identity — the registered PID plus its registered start time, never a
      process-name search.
    - Exit `2` means a registered role exited unexpectedly — its message names
      each exited role's exit code, and a zero exit code never makes a
      disappearance intentional — and its findings carry any crash report
      written since that role's pre-launch baseline.
      - A report unchanged since that baseline is stale and is never announced
        as a new crash, and a role that exited without changing any report
        reports `reportPath` and `headline` as `null` rather than inventing
        evidence.
    - Exit `1` is a setup or state failure.
    - Never delete, move, or rewrite a reported crash report or its retained
      evidence; report it by path.
    - The checker reports only through the command, poll, or check that is
      already active and never promises out-of-band notification while none is.

26. After launching either executable, wait for readiness with
    `../scripts/Wait-HarnessPing.ps1` before the first real command. It handles
    one port per call, so launch order stays with you. Done when every launched
    port has returned a readiness result:

    ```powershell
    pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Wait-HarnessPing.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -Port 27100 -TimeoutSeconds 120 -ProcessCheckStatePath '<absolute process-check state path>'
    ```

    - The script polls until the port answers `ok:true`, tolerating the
      individual timeouts long client startup (terrain elevation and
      priority-texture waits) produces after connect already succeeded.
    - Exit `0` (`status` `pass`) reports the observed `tick`; `tick` of `-1`
      means the listener is up but the game is not yet created (see the command
      reference `command-reference.md` for `ping`). Exit `2` (`status`
      `blocked`, code `ping.timeout`) reports the attempt count and elapsed
      time, and blocks the first real command. Exit `1` is a setup failure.
    - Never hand-write a ping loop or invent a sleep window in its place.
    - The optional `-ProcessCheckStatePath` is this run's process-check state
      path; supply it whenever the roles are registered, which the launch
      sequence above makes the ordinary case.
    - While the wait is still unsatisfied, the helper checks the registered
      roles before each retry and sleep, so a dead process ends the wait early
      instead of consuming the remaining timeout: exit `2` (`status` `blocked`)
      with code `process.unexpected-exit` and the check's `findings`, rather
      than `ping.timeout`.
    - A poll that succeeds returns without a further check, so an unexpected
      exit after it surfaces at the next lifecycle point instead — the next
      command's transport-failure check or release's entry check.
    - A checker that could not run at all is advisory: it is reported once as
      `processCheckWarning` and polling continues. Every existing exit-code
      meaning is unchanged.

27. Invoke `../scripts/Wait-IslandSceneReady.ps1` only after launch and only
    when an approved criterion depends on island footprints or rendered islands.
    Done when that criterion has a readiness artifact, or the criterion does not
    depend on islands and the helper is not run:

    ```powershell
    pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Wait-IslandSceneReady.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ClientPort 27101 -TimeoutSeconds 120 -ArtifactPath '<absolute artifact path>' -ProcessCheckStatePath '<absolute process-check state path>'
    ```

    - Supply the exact literal `-AgentHarness '<absolute AgentHarness path>'`,
      harness-lock `-Owner '<owner token>'`, client `-ClientPort`, bounded
      `-TimeoutSeconds`, absolute ignored
      `-ArtifactPath '<absolute artifact path>'`, and the optional
      `-ProcessCheckStatePath '<absolute process-check state path>'`.
    - The helper requires client `ping` with `tick >= 0`, restores the client
      with `window_state {minimized:false}`, and requires the complete
      `clientGridCoord` plus nonempty island footprints to be byte-stable across
      two consecutive normalized samples.
    - Exit `0` is usable only with a
      `broken-engine-island-scene-readiness/v1` artifact reporting
      `Status:success`, `Code:ready`, and `Ready:true`; missing, malformed, or
      failed evidence blocks the criterion.
    - Supply `-ProcessCheckStatePath` whenever the roles are registered, which
      the launch sequence above makes the ordinary case: each readiness
      iteration then checks them and returns early on an unexpected exit with
      exit `1` and an artifact reporting `Status:failure`,
      `Code:ProcessUnexpectedExit`, `Ready:false`, and the check's findings in
      `Findings`, instead of consuming the remaining timeout.
    - A checker that could not run at all is advisory `ProcessCheckWarning` in
      the artifact and the wait continues. Every existing exit-code meaning is
      unchanged.
    - The selected project document owns the concrete argument values and the
      evidence check around this call.

28. Before relinking or relaunching, send `quit` and wait for the exact numeric
    PID from the latest launch snapshot. Done when that PID is gone.

    - A live executable locks its image.
    - Do not launch a duplicate to displace it — with `SO_REUSEADDR`, a
      duplicate on the same port binds alongside the live listener instead of
      failing fast, and connection routing between the two becomes
      nondeterministic.
    - The engine listener sets `SO_REUSEADDR` and briefly retries
      address-in-use binds, so relaunch immediately once the exact PID has
      exited and rely on the ping poll for readiness — never invent a sleep
      window.

29. Run a fresh `Baseline` and `Register` for the new process, in that order
    around its `Start-Process`. Done when the replacement process is registered.

    - Between that deliberate quit and re-registration the role is not checked,
      so run no process check in that window.

#### Additional simultaneous client

30. Launch an additional client for a criterion that needs two clients connected
    at once — a late joiner beside a client that must stay connected — after the
    standard pair. Done when that criterion either has its additional client or
    provably needs none.

    - Disconnecting and reconnecting the one client is not a substitute when the
      criterion depends on state the server drops when its last client leaves.

31. Build the additional client as the documented client launch with three
    changes: its own fixed agent port above, its own `--app-data-directory`
    root, and its own `--log-file`. Done when the additional client is launched
    from the project `launch.md` block with those three changes.

    - Every other argument is unchanged, including the same `--data-directory` —
      all processes of one run must select the same asset data root.
    - Each additional client needs its own app-data root because of how one root
      separates client and server state (`Engine/Source/File/AGENTS.md`).
    - The project `launch.md` owns the concrete launch block.

32. Join it to this run's process-identity flow as its own role in the same
    state path, exactly as for the first client. Done when its role is
    registered and checked.

    - Run `Baseline` with its own `-ConfiguredAppDataRoot` and the client
      `-GameName`, then `Register` and the immediate `Check`.

33. Wait for its readiness with `Wait-HarnessPing.ps1 -Port 27102` after the
    standard `27100` then `27101` waits. Done when port `27102` has returned a
    readiness result.

34. Quit the additional client before the release call, then run one fresh
    `Baseline` for its role alone. Done when its PID is gone and its role is
    baselined again.

    - `../scripts/Invoke-HarnessRelease.ps1` covers only ports `27100`/`27101`
      and the server/client PIDs you supply, so it never quits an additional
      client.
    - Quit the additional client through its own port and confirm that its exact
      retained PID is gone — a wait that times out is a failed run, so stop or
      investigate that PID rather than continuing.
    - The fresh `Baseline` clears its registration so release's entry check does
      not report the deliberate quit as an unexpected exit. Release then
      proceeds unchanged.

### Invoke commands

35. Prefer stdin JSON and always capture stdout even when exit is nonzero. Done
    when every command of this run is sent that way:

    ```powershell
    '{"cmd":"ping"}' | & '<absolute AgentHarness path>' --owner '<owner token>' --port 27100 -
    ```

36. For server-window verification, read the project-owned
    [server-window recipe](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/server-window.md#server-gdi-monitoring-window-capture-and-click).
    Done when that recipe has been read, or the scenario needs no server-window
    verification.

37. Use `--timeout-ms N` for a deferred client command; default is 15000 and
    maximum is 600000. Done when each deferred command carries a timeout matched
    to it.

    - The CLI retries the loopback connect until this same deadline rather than
      making a single short attempt, so a probe against a dead port consumes the
      full budget — give a quick negative probe a small `--timeout-ms` (e.g.
      `1000`).
    - Exit `0` means parsed `ok:true`, exit `2` means parsed `ok:false`, and
      exit `1` means transport, usage, or OS failure.

38. After a transport failure, run the process check's `-Action Check` (Launch
    steps above) once before any retry and report its result through this same
    command. Done when every transport failure has that check's result reported
    before any retry.

    - That active command is the only channel the checker reports through, so a
      lost connection is diagnosed here rather than waited out.
    - A check exit of `2` means the endpoint exited unexpectedly, including a
      handled exception that terminated with exit code `0` and wrote a crash
      report, so retry nothing and report the finding's role, report path,
      exception headline, exit code, and retained evidence path.
    - A check exit of `0` leaves the transport failure an ordinary one and the
      retry proceeds.

39. Keep every request on the envelope. Done when every request matches that
    envelope.

    - Request envelope: `{"cmd":"<name>","params":{...},"id":<optional JSON>}`.
      Omit `params` only for parameterless commands. Unknown top-level fields
      fail.
    - Responses are `{"id":<echo|null>,"ok":true,"result":{...}}` or
      `{"id":...,"ok":false,"error":"..."}`.
    - Parameters are an external trust boundary and invalid values must produce
      the error envelope.

40. The channel permits one request still in progress. Client deferred commands
    occupy it until completion; another AgentHarness call waits rather than
    bypassing it. Do not overlap calls. Done when no two calls overlap.

41. Write any multi-line PowerShell driver — anything with `function`
    definitions, loops, or more than a few statements — to a script file under
    `<absolute adopted worktree>\Temp` and run it with `pwsh -File`. Done when
    every such driver runs from its own script file.

    - Never compact such a driver into a semicolon-joined one-liner; compaction
      corrupts function definitions (observed: `function Snap([string]$p)`
      mangled into an unrecognized `Snap$d` token).

### Authoritative verification

42. Take the concrete setup recipe from the selected project's hub, including
    the commands that seed server state, confirm client connection, and address
    UI. Done when this run's setup recipe comes from that hub:

    - The hub routes [common verification](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md).
    - It routes endpoint commands.
    - It routes
      [replay determinism](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md)
      or [cross-cell](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md)
      scenario material.

43. Regardless of project, hold these verification-evidence principles. Done
    when each principle holds for every criterion this run verifies:

    - Verify with the narrowest observable combination of the project's scene,
      UI, screenshot, server-query, and log commands. `describe_ui`,
      scene/server queries, and `get_logs` close a criterion more cheaply than
      pixels; reach for a capture only when the criterion is genuinely about
      what was rendered.
    - Stable counts such as players should agree exactly; allow bounded tick
      drift for collections whose entries are being added and removed rapidly.
    - Release through the lifecycle and release steps below.

### Lifecycle and release

44. After every successful, failed, crashed, or abandoned launch attempt,
    release through `../scripts/Invoke-HarnessRelease.ps1`. One call performs
    the whole sequence, and it is safe after a crashed run whose PIDs are
    already gone. Done when that one call has returned:

    ```powershell
    pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ServerPid <server PID> -ClientPid <client PID> -ProcessCheckStatePath '<absolute process-check state path>'
    ```

45. Pass only exact non-null numeric `serverPid`/`clientPid` values retained
    from the latest launch snapshot; replace the numeric placeholders and omit
    either argument for a process that never started. Done when the call carries
    exactly those retained PIDs.

    - The latest snapshot is authoritative, including after a partial launch
      failure.
    - The script quits both ports with the owner (server quit autosaves), waits
      for those exact PIDs, stops only a supplied exact PID that survived the
      quit, and runs `lock release` only once every supplied PID is confirmed
      absent. It never searches by process name and never stops a PID you did
      not supply.
    - An additional client is outside those two ports, so Additional
      simultaneous client above governs its shutdown before this call.

46. Supply the optional `-ProcessCheckStatePath` whenever the roles are
    registered, which the launch sequence above makes the ordinary case. Done
    when the registered run's release call carries that state path.

    - The script then runs `-Action Check` once at entry, before the first quit,
      so a role that already crashed is observed while its evidence is still
      attributable, and reports the findings as an additive `crashFindings`
      array in its JSON, with an advisory `processCheckNote` when the checker
      returned no readable findings.
    - Quit, exact-PID wait, stop, cleanup, and lock-release ordering and every
      exit code are unchanged, and the reported crash reports and their evidence
      directories are never deleted, moved, or rewritten.
    - Exit `0` with a non-empty `crashFindings` therefore still means the run
      failed: cleanup succeeded and the lock was released, but a registered role
      had exited unexpectedly.

47. Read the release result. Done when the release outcome is classified and
    reported.

    - Exit `0` (`status` `pass`) means every supplied PID is absent and the lock
      was released; report the payload's per-step outcomes verbatim, including
      `crashFindings`.
    - Exit `2` (`status` `blocked`, code `release.process-survived`) means a
      supplied PID is still alive: no release was attempted, the claim stays
      held, and you decide whether to retry the release or escalate to the user.
    - Exit `1` is a failure naming its step, including
      `release.lock-owner-mismatch` and `release.lock-absent`, which are never a
      success.
    - Never reconstruct the quit, wait, stop, or release steps inline.

### Process verification report

48. Run plan-provided runtime steps when present; otherwise derive the smallest
    live checks for runtime-observable criteria only. Done when every
    runtime-observable criterion has been exercised and the handoff in
    [`../SKILL.md`](../SKILL.md) is returned.

## Rules

- Each fenced PowerShell block is an independent shell call unless it is
  explicitly labeled as temporary-script contents. Variables and functions are
  fence-local and do not survive into another shell-call block. A variable in a
  code fence is valid only when that fence assigns it, or when it is an
  automatic/environment variable.
- Retain claim, takeover, launch, and artifact outputs as literal values in
  agent context, then substitute quoted string/path placeholders or numeric PID
  placeholders into later calls.
- A temporary-script contents block is written under
  `<absolute adopted worktree>\Temp` and then run by a separate self-contained
  `pwsh -NoProfile -File '<absolute adopted worktree>\Temp\<driver filename>.ps1'`
  shell call.
- Never build AgentHarness or write through shared Output links during routine
  harness work; `/compile` owns AgentTools newly-built-binary production and
  promotion. The claim step itself verifies that the selected project's server
  and client executables exist — the server executable alone under
  `-ServerOnly` — so a missing required one blocks the claim instead of
  surfacing later.
- Never infer an identity, switch data mode, fall back to Shared data, or run
  DataPacker/Gaea/texture export.
- An owner mismatch is a hard stop. Never remove coordination state manually.
- Prefer a script over the image. Most visual criteria — frame non-black, region
  matches an expected color, two captures differ, pixel count past a threshold —
  are assertions a few lines of code settle more precisely than an eye on a
  downscaled JPEG. Write the driver under `<absolute adopted worktree>\Temp` and
  run it with `pwsh -File` as the Invoke commands steps require; its stdout is a
  few bytes of decisive text instead of megabytes of image.
- The same holds for a large driver result file: query it targeted first, and
  read the whole record only after a targeted read provably misses.
- Request only the resolution the check needs. `screenshot` downscales through
  `maxWidth` (default 1568) and `quality` (default 80), and those defaults are
  sized for UI readability. A "did terrain render at all" check does not need
  them; pass a `maxWidth` and `quality` matched to the assertion as part of
  designing the check.

### Performance investigations

- GPU performance investigations run the client fullscreen at 4K. Restore the
  client, send `fullscreen {"on":true}`, and require the response to report
  `fullscreen:true`, `width:3840`, and `height:2160` before collecting
  measurements. Keep the window visible for the full measurement interval. If
  the active display cannot provide that framebuffer, report the GPU criterion
  `BLOCKED` instead of substituting a smaller or windowed mode.
- CPU performance investigations build and run the measured client or server
  with the `Profile` configuration/target. Debug and Release CPU timings are not
  acceptance evidence for a CPU performance investigation.

### Durable caveats

The selected project's focused verification, replay, and endpoint references own
game-specific caveats (injection/pause/replay timing, queued-update semantics,
and command source-file ownership). These engine-generic caveats hold for every
project:

- Physical mouse, keyboard, wheel, and gamepad input is suppressed for an
  agent-mode client. Synthetic input is the sole game/ImGui source; Alt+F4 and
  window Close still quit. Focus messages remain real; an agent client boots
  with audio suspended, and `audio_resume` resumes it without OS focus.
- Keep ownership warm during long soaks. Never leave a background heartbeat/poll
  process after the session.
