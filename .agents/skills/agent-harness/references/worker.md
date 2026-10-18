# Agent Interaction Harness Worker

Harness steps and rules for the worker dispatched with
[`../SKILL.md`](../SKILL.md), which owns the purpose, the triggers, the inputs,
and the handoff this run returns.

## Steps

1. Use server port `27100`, client port `27101`, and the absolute adopted
   worktree. Done when this run's ports and adopted worktree path are fixed.

   - A scenario needing a second simultaneous client adds port `27102` (see the
     project's [Additional simultaneous client](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md#additional-simultaneous-client)
     recipe).
   - A local server is a development instance, not an Azure production server.

2. Read the focused references only when applicable. Done when every reference
   this scenario makes applicable has been read:

   - Read the command reference (`command-reference.md#shared-commands`) for
     the engine-shared command schemas and the `params`/`result` placement
     convention; the request/response envelope itself is defined in the Invoke
     commands steps below. The selected project's
     `Projects/<Project>/Documents/AgentHarness.md` hub routes every game
     command schema, launch recipe, verification scenario, and game caveat to a
     focused reference.
   - Read the private-LAN firewall reference (`private-lan-firewall.md`) only
     for an explicitly requested cross-machine or private-Wi-Fi scenario.
     Ordinary same-machine runs stay loopback-only and never inspect or change
     firewall state.
   - Read the RenderDoc capture reference (`renderdoc.md`) only for a GPU
     frame-capture or capture-analysis scenario; it owns the launch script's
     `-RenderDoc` switch, `renderdoc_capture`, and the headless `rdc_*`
     analysis scripts.

### Select the project

3. Target the game project the user, plan, or brief names; otherwise the only
   game the latest `/compile` result built. When it built several and none is
   named, stop before the claim and return the game choice as a `Residuals` row;
   never pick one by default. Done when the target project is named or the
   choice is returned.

4. Read the selected project's `Projects/<Project>/Documents/AgentHarness.md`
   hub before launching. Done when that hub has been read.

   - Its machine-readable launch configuration owns the executable names and
     output directory, `../scripts/Invoke-HarnessLaunch.ps1` owns the launch
     sequence and the game argument set, and the hub's linked `launch.md` owns
     the concrete values of each launch-script call.
   - The root hub routes project command schemas and verification scenarios to
     focused children.

### Provision and claim

5. Claim through `../scripts/Invoke-HarnessClaim.ps1`, run from the session
   worktree root. Done when that single invocation has returned its JSON object:

   ```powershell
   pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<absolute adopted worktree>' -Session '<short task label>' -GameDataDirectory '<normalized Data path>'
   ```

   - It takes the project from the
     `Projects\<Name>\Platforms\VisualStudio2026\Output\Data` suffix of
     `-GameDataDirectory`, so the data path selects the game. It requires that
     project's server and client executables (reading
     their names and `Output` directory from the project's root
     `## Machine-readable launch configuration`, not from the human `launch.md`
     recipe) unless you pass `-ServerOnly` to declare a server-only session,
     which narrows that existence check to the server executable.
   - Declare `-ServerOnly` from what your scenario actually launches; it binds
     the whole claimed session, so a `-ServerOnly` session must not launch a
     client under that claim, and it is never inferred from the project's server
     command document, which is not uniformly client-free.
   - It also compares a Shared data directory with this worktree's record of
     what each required executable's build compiled against, proves that the
     pack version this worktree's source expects matches the version the
     supplied data directory's `.manifest` files carry, provisions the
     checkout, requires the resolved `AgentHarness.exe`, mints the owner token,
     and claims the lock, printing one compact `broken-engine-harness-claim/v1`
     JSON object.
   - When another session holds the lock, it provisions once and then retries
     only the claim until it succeeds or the holder becomes stale under step 11,
     so the invocation above is itself the wait and has no budget of its own —
     never hand-write a poll loop, invent a sleep window, or add a wait
     parameter. A stale holder is still waited on while a project server
     started without `--agent-port` is running, because a harness server cannot
     start beside it; once none remains the wait ends with
     `claim.foreign-owner`.
   - Run it synchronously in the foreground and remain in-turn until its exit
     code and JSON result are captured, giving the call the maximum available
     execution timeout so the wait is not preempted. Never use `Start-Job`, a
     trailing `&`, a fire-and-forget watcher, or a host background-execution
     parameter such as `run_in_background`, and never end a delegated turn while
     the claim is running — a background task's completion notification cannot
     resume a worker whose turn has ended.
   - If the host call times out, re-invoke the identical command; it resumes the
     same wait.
   - Pass the selected game's normalized `GameDataDirectory` from the latest
     `/compile` result verbatim as `-GameDataDirectory` — the same packed data
     the launch script below selects with its own `-GameDataDirectory`. Only
     when the approved acceptance needs derived data, such as a deliberately
     corrupted pack, pass instead a real byte copy of that directory that keeps
     every `.manifest` unchanged, never links to it, under
     `<absolute adopted worktree>\Temp` with the layout suffix kept, such as
     `<absolute adopted worktree>\Temp\<label>\Projects\<Name>\Platforms\VisualStudio2026\Output\Data`,
     as the `-GameDataDirectory` of both the claim and the launch script; the
     scenario derives the copy. Delete the copy's `Temp\<label>` directory when
     the run ends with nothing launched or once its last release (step 39)
     returns exit `0`; otherwise name its path under `Evidence`.
   - Add `-Configuration <name>` only for a build other than `Debug`.

6. Use only the provisioned primary AgentHarness output that claim resolved.
   Done when this run uses only that provisioned output under the session owner
   the claim settled.

7. Read the claim result and act on its exit code. Done when the claim is held
   with its values retained, or the reported block or failure is reported and
   the attempt ends or, for a stale holder, routes to the takeover steps.

   - Exit `0` (`status` `pass`) supplies the `owner` token, the resolved
     `agentHarness` path, and the `claim` record; retain those values outside
     shell state and substitute them as literal `<owner token>` and
     `<absolute AgentHarness path>` values in later calls, then report the claim
     metadata verbatim.
   - Every result — pass, blocked, or error — echoes the supplied data path as
     `gameDataDirectory`, canonicalized once the claim reaches the project
     derivation; carries `project`, the derived game project, `null` until
     derived; and carries a `packVersion` field that is `null` except on the
     `claim.pack-version-mismatch` block below.
   - Exit `2` (`status` `blocked`) carries one of six codes. A
     `claim.foreign-owner` reports a holder the script judged stale and
     continues into the takeover steps, which reconfirm that under step 11,
     while `claim.project-underivable`, `claim.executable-missing`,
     `shared-data.changed`, `shared-data.record-missing`, and
     `claim.pack-version-mismatch` end the attempt here; for every exit `2` the
     script never launches the game, so the Launch and ping-wait steps below are
     not performed and their absence is not a further failure to diagnose.
   - `claim.project-underivable` means `-GameDataDirectory` lacks the
     `Projects\<Name>\Platforms\VisualStudio2026\Output\Data` suffix: nothing
     was provisioned and no owner token or lock was taken, so re-enter passing
     the selected game's latest `/compile` `GameDataDirectory`, or a derived
     copy that keeps the suffix (step 5).
   - `claim.executable-missing` means a required project executable is absent
     for the requested configuration: no provisioning ran and no lock was taken,
     so route `/compile` for the named executables and re-enter; when only the
     client executable is named and this session's scenario launches only the
     server, re-enter with `-ServerOnly` instead of building the client.
   - `shared-data.changed` means the Shared data directory's listing differs
     from this worktree's record for a required target: nothing was
     provisioned and no lock was taken. It is a mandatory user stop: end the
     attempt `BLOCKED` with one `Residuals` row carrying the code and message,
     and never rebuild, refresh, rebase, switch data, or edit or delete a record
     to clear it.
   - `shared-data.record-missing` means no record exists for that target and
     configuration: nothing was provisioned and no lock was taken, so route
     `/compile` for the named target and re-enter; it is not a user stop.
   - `claim.pack-version-mismatch` means the pack version derived from this
     worktree's source is not the version the data carries; nothing was
     provisioned and no lock was taken.
     - This preflight block catches up front a failure that would otherwise
       happen deep inside launch, at the runtime manifest check, exiting before
       the harness ping and surfacing as the Launch step's process-check result.
     - The payload's `packVersion` carries `expected`, `found`,
       `mismatchedManifests`, and `manifestCount`, so rebuild or re-export to
       pair them rather than relaunching.
   - A `claim.foreign-owner` payload's `currentOwner` carries the last attempt's
     `claimedAt`, the derived `holdSeconds`, and `heartbeatAt` — heartbeat never
     advances `claimedAt` — so confirm staleness from `heartbeatAt` per step 11;
     the takeover steps take the old owner token from `currentOwner.owner` and
     the harness path from `agentHarness`. When step 11 instead classifies the
     holder fresh, re-invoke the claim per step 5.
   - The script never steals and never touches a foreign owner's processes,
     heartbeat, or claim, whether it acquires the lock or blocks once the holder
     is stale.
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

9. Read `lock status` after the claim and before the first launch-script call
   (step 21), read it again after the first launch or command, and require
   `heartbeatAt` to advance between them. Done when that comparison shows
   `heartbeatAt` advanced.

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

16. Fix this launch's paths. Done when the app-data root, the log paths, and
    the process-check state path (step 20) are fixed for this launch.

    - The launch script reads the executable names from the hub (step 4) and
      owns the game argument set, so the run fixes only these paths.
    - Create log parents under `<absolute adopted worktree>\Temp`.
    - Do not change process working directories; the launch script's
      `-GameDataDirectory` is the only override selecting packed assets, and its
      `-AppDataDirectory` is the only override selecting where saves, settings,
      caches, and replays are written.

17. Take the launch-script calls from the selected project's documents. Done
    when this run's launch-script calls come from that project's `launch.md`.

    - The selected project's hub owns the machine-readable executable paths,
      while its linked `launch.md` owns the concrete values of each
      launch-script call, game-specific connect notes, and fresh-state choices.
    - A server-side reset command resets server state only, so a comparison
      requiring fresh client state must launch under an app-data directory that
      did not exist before that launch; use the `launch.md` recipe.
    - The launch script derives each role's agent port from the fixed ports
      above.

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
    - Pass the launch script's client-only `-NativeResolution` switch, which
      omits `--windowed`, only when native-resolution UI sizing/readability is
      part of acceptance.
    - Pass its optional client-only `-RenderDoc` switch, which adds
      `--renderdoc`, only for GPU frame capture; it force-loads renderdoc.dll
      and drops the Vulkan validation layer, so keep it off ordinary runs (see
      the RenderDoc capture reference `renderdoc.md`).

19. Process-check every launch on every host through
    `../scripts/Invoke-AgentHarnessProcessCheck.ps1`. Done when its `Baseline`,
    `Register`, and `Check` actions are the only way this run checks process
    identity.

    - The launch script runs `Baseline`, `Register`, and the immediate `Check`
      for every launch. Run an action directly only for step 29's fresh
      `Baseline` and step 33's transport-failure `Check`, each as its own shell
      call in the form shown below, with the role's game name and app-data root
      from the project harness doc:

      ```powershell
      pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Baseline -StatePath '<absolute process-check state path>' -Role '<role>' -GameName '<Game Name>' -ConfiguredAppDataRoot '<absolute app-data root>'
      ```

      ```powershell
      pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Check -StatePath '<absolute process-check state path>'
      ```

    - The wait helpers and `../scripts/Invoke-HarnessRelease.ps1` run the
      checker themselves from the `-ProcessCheckStatePath` you pass them, so
      never invoke it yourself mid-wait.

20. Generate one absolute state path per run under
    `<absolute adopted worktree>\Temp` that no earlier run used. Done when that
    same path is the one passed to every action, wait helper, and release call
    of that run.

    - An unused path makes the run start with no state file, so it inherits no
      earlier run's registrations.

21. On every host, launch and relaunch each role through
    `../scripts/Invoke-HarnessLaunch.ps1`, one call per role in the form the
    project `launch.md` shows, server first. Never run `Start-Process` on a game
    executable. Done when every role this run launches has returned its launch
    result.

    - `-Role` is `server`, `client`, or `client-b`. Add `-Configuration <name>`
      only for a build other than `Debug`, matching the claim, the
      client-only switches only as step 18 states, and `-CrossMachine` only as
      `private-lan-firewall.md` states.
    - Run it synchronously in the foreground and remain in-turn until its exit
      code and JSON result are captured, giving the call the maximum available
      execution timeout; it returns after its port answers ping or the
      readiness wait ends (step 23).
    - It prints one compact `broken-engine-harness-launch/v1` JSON object with
      `status`, `code`, `message`, `role`, `port`, `project`,
      `gameDataDirectory`, `pid`, `startTimeUtc`, `tick`, and the
      `processCheck` and `ping` payloads. Retain each role's latest result
      outside shell state; its `pid`, `null` when no process started, is the
      only PID lifecycle checks, quits, and release use for that role.
    - Exit `0` (code `ok`) means the role started, holds its registered
      identity, and its port answered ping.
    - `shared-data.changed` (exit `2`) means the Shared data directory's listing
      differs from this worktree's record for the role's target, and nothing
      started. End the attempt through release (step 39) with the `pid` of each
      role still running; the rest is step 7's `shared-data.changed` user stop.
    - `shared-data.record-missing` (exit `2`) means no record exists for the
      role's target and configuration, and nothing started; end the attempt the
      same way and route `/compile` for the named target, with no user stop.
    - `launch.server-running` (exit `2`) means a server process was already
      running, and nothing started; follow the running-server route in the
      project `launch.md`.
    - The passed-through `check.unexpected-exit` (exit `2`, from the immediate
      `Check`) keeps its step 22 meaning, and `process.unexpected-exit` and
      `ping.timeout` (exit `2`, from the readiness wait) keep their step 23
      meanings.
    - Exit `1` is a failure naming its step (`launch.repository-missing`,
      `launch.project-underivable`, `launch.launch-doc-unreadable`,
      `launch.baseline-failed`, `launch.register-failed`,
      `launch.check-failed`, `launch.start-failed`, `launch.ping-failed`, or
      `internal.error`); stop and report it.
    - `Register` starts a background watcher on the registered PID so `Check`
      can report that process's exit code, and reports in its `message` and its
      `watcherBound` result field whether the watcher bound; no extra step is
      required from you either way.

22. Read each process-check result from its JSON object, including each launch
    result's `processCheck` payload, which holds the last action that launch
    ran. Done when every action of this run is classified from that object.

    - Every action prints one compact
      `broken-engine-harness-process-check/v1` JSON object with `status`,
      `code`, and `message`; `Check` adds `findings`, each naming its `role`,
      `reportPath`, `headline`, `exitCode`, and `evidencePath` (the retained
      evidence directory).
    - `exitCode` is the registered process's exit code, or `null` when the exit
      code was not captured — usually because the watcher could not bind it, a
      process that had already exited when `Register` ran; prove an intentional
      clean exit (after `crash_report_harness_rig`,
      for example) from `exitCode`, never from a second launch.
    - Exit `0` means every registered role still holds its registered exact
      identity — the registered PID plus its registered start time, never a
      process-name search.
    - Exit `2` means a registered role exited unexpectedly — its message names
      each exited role's exit code, and a zero exit code never makes a
      disappearance intentional — and its findings carry any crash report
      written since that role's pre-launch baseline.
      - A report unchanged since that baseline, or changed only by the client
        appending its sent trailer, is stale and is never announced as a new
        crash, and a role that exited without writing a report reports
        `reportPath` and `headline` as `null` rather than inventing evidence.
    - Exit `1` is a setup or state failure.
    - Never delete, move, or rewrite a reported crash report or its retained
      evidence; report it by path.
    - The checker reports only through the command, poll, or check that is
      already active and never promises out-of-band notification while none is.

23. Each launch call waits for its port's readiness through
    `../scripts/Wait-HarnessPing.ps1` before returning, so the first real
    command follows a passing launch result and the server's wait precedes the
    client's; never run that helper yourself. Done when every launched port's
    readiness result has been read from its launch result's `ping` payload.

    - The helper polls until the port answers `ok:true`, tolerating the
      individual timeouts long client startup (terrain elevation and
      priority-texture waits) produces after connect already succeeded.
    - A passing wait reports the observed `tick`, which the launch result also
      carries; `tick` of `-1` means the listener is up but the game is not yet
      created (see the command reference `command-reference.md` for `ping`). A
      `ping.timeout` reports the attempt count and elapsed time, and blocks the
      first real command. A wait that failed to run is `launch.ping-failed`.
    - Never hand-write a ping loop or invent a sleep window in its place.
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

24. Invoke `../scripts/Wait-IslandSceneReady.ps1` only after launch and only
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

25. Before relinking or relaunching, send `quit` and wait for the exact numeric
    `pid` from the role's latest launch result. Done when that PID is gone.

    - Use the quit and exit-wait form of the project's
      [Additional simultaneous client](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md#additional-simultaneous-client)
      shutdown, with the role's own port and `pid`.
    - A live executable locks its image.
    - Do not launch a duplicate to displace it — with `SO_REUSEADDR`, a
      duplicate on the same port binds alongside the live listener instead of
      failing fast, and connection routing between the two becomes
      nondeterministic.
    - The engine listener sets `SO_REUSEADDR` and briefly retries
      address-in-use binds, so relaunch immediately once the exact PID has
      exited and rely on the ping poll for readiness — never invent a sleep
      window.

26. Relaunch the role with its launch call (step 21), which baselines the role
    before it registers the new process. Done when the replacement process's
    launch result has returned.

    - Between that deliberate quit and re-registration the role is not checked,
      so run no process check in that window.

#### Additional simultaneous client

27. Launch an additional client for a criterion that needs two clients connected
    at once — a late joiner beside a client that must stay connected — after the
    standard pair. Done when that criterion either has its additional client or
    provably needs none.

    - Disconnecting and reconnecting the one client is not a substitute when the
      criterion depends on state the server drops when its last client leaves.

28. Launch the additional client after the standard pair with the client
    launch call (step 21) changed three ways: `-Role client-b`, its own
    `-AppDataDirectory` root, and its own `-LogFile`; the call registers,
    checks, and waits on port `27102` in this run's state path. Done when its
    launch result has returned.

    - Every other value is unchanged, including the same `-GameDataDirectory` —
      all processes of one run must select the same asset data root.
    - Each additional client needs its own app-data root because of how one root
      separates client and server state (`Engine/Source/File/AGENTS.md`).
    - The project `launch.md` owns the concrete call.

29. Quit the additional client before the release call, then run one fresh
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

30. Prefer stdin JSON and always capture stdout even when exit is nonzero. Done
    when every command of this run is sent that way:

    ```powershell
    '{"cmd":"ping"}' | & '<absolute AgentHarness path>' --owner '<owner token>' --port 27100 -
    ```

31. For server-window verification, read the project-owned
    [server-window recipe](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/server-window.md#server-gdi-monitoring-window-capture-and-click).
    Done when that recipe has been read, or the scenario needs no server-window
    verification.

32. Use `--timeout-ms N` for a deferred client command; default is 15000 and
    maximum is 600000. Done when each deferred command carries a timeout matched
    to it.

    - The CLI retries the loopback connect until this same deadline rather than
      making a single short attempt, so a probe against a dead port consumes the
      full budget — give a quick negative probe a small `--timeout-ms` (e.g.
      `1000`).
    - Exit `0` means parsed `ok:true`, exit `2` means parsed `ok:false`, and
      exit `1` means transport, usage, or OS failure.

33. After a transport failure, run the process check's `-Action Check` (Launch
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

34. Keep every request on the envelope. Done when every request matches that
    envelope.

    - Request envelope: `{"cmd":"<name>","params":{...},"id":<optional JSON>}`.
      Omit `params` only for parameterless commands. Unknown top-level fields
      fail. An absent `params` dispatches as `{}`; a present non-object
      `params`, `null` included, fails for every command with
      `params must be an object` before the command runs.
    - Responses are `{"id":<echo|null>,"ok":true,"result":{...}}` or
      `{"id":...,"ok":false,"error":"..."}`.
    - Parameters are an external trust boundary and invalid values must produce
      the error envelope.

35. The channel permits one request still in progress. Client deferred commands
    occupy it until completion; another AgentHarness call waits rather than
    bypassing it. Do not overlap calls. Done when no two calls overlap.

36. Write any multi-line PowerShell driver — anything with `function`
    definitions, loops, or more than a few statements — to a script file under
    `<absolute adopted worktree>\Temp` and run it with `pwsh -File`. Done when
    every such driver runs from its own script file.

    - Never compact such a driver into a semicolon-joined one-liner; compaction
      corrupts function definitions (observed: `function Snap([string]$p)`
      mangled into an unrecognized `Snap$d` token).

### Authoritative verification

37. Take the concrete setup recipe from the selected project's hub, including
    the commands that seed server state, confirm client connection, and address
    UI. Done when this run's setup recipe comes from that hub:

    - The hub routes [common verification](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md).
    - It routes endpoint commands.
    - It routes
      [replay determinism](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md)
      or [cross-cell](../../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md)
      scenario material.

38. Regardless of project, hold these verification-evidence principles. Done
    when each principle holds for every criterion this run verifies:

    - Verify with the narrowest observable combination of the project's scene,
      UI, screenshot, server-query, and log commands. Read supported numeric
      control values from `describe_ui` before using a capture. `describe_ui`,
      scene/server queries, and `get_logs` close a criterion more cheaply than
      pixels; reach for a capture only when the criterion is genuinely about
      what was rendered.
    - Stable counts such as omnis should agree exactly; allow bounded tick
      drift for collections whose entries are being added and removed rapidly.
    - Release through the lifecycle and release steps below.

### Lifecycle and release

39. After every successful, failed, crashed, or abandoned launch attempt,
    release through `../scripts/Invoke-HarnessRelease.ps1`. One call performs
    the whole sequence, and it is safe after a crashed run whose PIDs are
    already gone. Done when that one call has returned:

    ```powershell
    pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ServerPid <server PID> -ClientPid <client PID> -ProcessCheckStatePath '<absolute process-check state path>'
    ```

40. Pass only exact non-null numeric `serverPid`/`clientPid` values retained
    from the latest launch results' `pid`; replace the numeric placeholders and
    omit either argument for a process that never started. Done when the call
    carries exactly those retained PIDs.

    - The latest launch results are authoritative, including after a partial
      launch failure.
    - The script quits both ports with the owner (server quit autosaves), waits
      for those exact PIDs, stops only a supplied exact PID that survived the
      quit, and runs `lock release` only once every supplied PID is confirmed
      absent. It never searches by process name and never stops a PID you did
      not supply.
    - An additional client is outside those two ports, so Additional
      simultaneous client above governs its shutdown before this call.

41. Supply the optional `-ProcessCheckStatePath` whenever the roles are
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

42. Read the release result, then search for `DEBUG_BREAK at` in the
    `--log-file` of each process this run started: the server and client log
    paths from the project harness doc's launch, for each PID step 40 retains,
    and each additional client's. A log no process of this run opened still holds an
    earlier run's lines and is never searched. Done when the release outcome is
    classified and reported and every such log has been searched.

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
    - Report each log with a match as one `Evidence` row: the log path with
      `DEBUG_BREAK at` as selector, the match count, the first matched line,
      and the line logged just before it, which names the cause of an `ASSERT`
      failure or a Vulkan validation message. A match is a note for main and
      never changes a criterion result, `Status`, or a retry.

#### Release-only recovery

Only a release-only recovery ([`../SKILL.md`](../SKILL.md) `## When to use`)
runs steps 43 through 46, after steps 1-2 and in place of steps 3-42; every
other run skips them. Step 45 runs steps 3-4 itself. The recovery frees only a
lock whose record `worktree` is the adopted worktree and whose `session` is not
`normal-launch`: that holder is this session's own stopped worker at any
heartbeat age, so step 11's fresh-owner rule, which protects foreign owners,
does not apply to it, and any other holder, including the never-released lock
of a Normal launch from this worktree, is foreign and left untouched. Compare
worktree paths as the claim records them, each canonicalized by
`Get-AgentCanonicalPath` (`.agents/scripts/AgentScriptCommon.psm1`), ignoring
case.

43. Before reading the lock record, stop every orphaned claim script: each
    `Win32_Process` whose `CommandLine` names `Invoke-HarnessClaim.ps1` and a
    `-RepositoryRoot` equal to the adopted worktree. Done when each such exact
    PID is stopped with `Stop-Process -Id` and reported as one
    `Decisive checks` row, or none exists.

    - A stopped worker's synchronous step-5 claim can outlive the host stop and
      keeps retrying the claim, so left running it would take the lock after
      this run released it or saw a foreign holder.

44. Read the lock record. Done when the record's `owner` is retained for a
    holder whose `worktree` is the adopted worktree and whose `session` is not
    `normal-launch`, or the run has ended at an early exit.

    - An early exit sends no `quit`, releases nothing, and ends the run,
      returning `Status: PASS` with one `Decisive checks` row stating that
      nothing of this session's was recovered and why.
    - Take `<absolute AgentHarness path>` as the AgentHarness executable path
      [`Tools/AgentHarness/AGENTS.md`](../../../../Tools/AgentHarness/AGENTS.md)
      names, under the adopted worktree. When that executable is missing, that
      is an early exit: no claim from this worktree can have taken the lock
      without it.
    - Otherwise run `lock status` with that executable:

      ```powershell
      & '<absolute AgentHarness path>' lock status --key default
      ```

    - `{"held":false}` (exit `2`), a `worktree` other than the adopted
      worktree, or a `session` of `normal-launch` is an early exit.

45. Shut down the stopped worker's game processes. Done when every validated
    PID is gone, or step 13 has returned `BLOCKED` on an unprovable listener.

    - Run steps 3-4 for the brief's game, and take the expected server and
      client executable names for the brief's configuration from the hub's
      Debug names, mapped to that configuration as the hub states.
    - Run steps 12-14 with the record's `owner` as the old owner token,
      whatever its heartbeat age.
    - Beyond the step-12 listeners, also find each `Win32_Process` whose
      normalized `ExecutablePath` is that expected server or client executable
      inside the adopted worktree and whose `CommandLine` carries
      `--agent-port`: a game process that started but is not yet listening, or
      hung before listening, which step 12 cannot see and which blocks the next
      launch. No quit reaches it, so after the step-14 quits wait on it with the
      listener PIDs and stop it by exact PID under step 14's rule.

46. Release through step 39 with the record's `owner`, no
    `-ServerPid`/`-ClientPid`, and no `-ProcessCheckStatePath`, then classify the
    result per step 42 without its log search. Done when the release result is
    reported as a `Decisive checks` row.

    - Step 45 already confirmed every validated PID gone, listening or not.
    - The stopped run's process-check state path lives only in its scratch
      files, and a recovery verifies no criterion.
    - This run started no process, so it has no log to search.
    - A non-zero exit returns `Status: BLOCKED` with its code named in
      `Residuals`.

### Process verification report

47. Run plan-provided runtime steps when present; otherwise derive the smallest
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
- The run holds the one client/server slot on the machine, so it is bounded:
  a wall-clock budget (the brief's, or the default that `../SKILL.md`
  `## Inputs` states) measured from the moment the claim succeeds (a
  quit-and-reclaim restarts it), and at most three attempts per criterion
  where the process-check rules allow a retry at all. A criterion that has not
  settled within its attempts is `BLOCKED`, with each attempt's commands and
  observations recorded as `Decisive checks` rows. At the wall-clock budget,
  start no further command (a helper wait already in flight runs to its own
  deadline), mark every unsettled criterion `BLOCKED`, release through the
  lifecycle and release steps below, and return the handoff. Never repeat a
  scenario past its attempt cap or keep the slot to perfect a criterion; a
  partial handoff with the slot released beats a complete one that never
  arrives.
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
- The on-screen profile overlay and its `P` cycling exist only where
  `kbProfiling` is true (`Debug`, `Profile`); a `Release` client compiles both
  out, so `key {"key":"P"}` still answers `ok:true` while nothing renders and an
  overlay screenshot is empty. The claim default `Debug` already has
  `kbProfiling` true, so a criterion that observes the overlay must not pass
  `-Configuration Release` at the claim step.

### Durable caveats

The selected project's focused verification, replay, and endpoint references own
game-specific caveats (injection/pause/replay timing, queued-update semantics,
and command source-file ownership). These engine-generic caveats hold for every
project:

- Physical mouse, keyboard, wheel, and gamepad input is suppressed for an
  agent-mode client. Synthetic input is the sole game/ImGui source, except IME
  result characters (`Engine/Source/Input/AGENTS.md`
  `## Focus and Agent Input`); Alt+F4 and window Close still quit. Focus
  messages remain real; an agent client boots with audio suspended, and
  `audio_resume` resumes it without OS focus.
- Keep ownership warm during long soaks. Never leave a background heartbeat/poll
  process after the session.
