---
name: optimize-build-time
description: Measure a full rebuild of the BrokenEngineSandbox client and server with vcperf (C++ Build Insights), rank the slowest translation units, headers, template instantiations, and functions, apply source-structure fixes (Pch.h contents, unused includes, forward declarations, moving includes or bodies out of headers) through the Change Workflow, measure the rebuild again, and report each metric's before/after change in percent. Use only when the user explicitly runs it to find and fix slow C++ build time; compiler, linker, and project option changes are reported as recommendations, never applied.
argument-hint: [Debug|Profile|Release]
allowed-tools: [Read, Grep, Glob, Bash, PowerShell, Agent]
disable-model-invocation: true
---

# Optimize Build Time

## Purpose

Measures a full client and server rebuild with vcperf, fixes the source
structure behind the ranked bottlenecks through the Change Workflow, measures
again, and reports each metric's before/after change in percent.

## When to use

- The user runs this skill by name to find out why the client and server C++
  build is slow and to make it faster.
- Not for CPU profiling of the running client or server; that is
  `/analyze-diagsession`.

## Inputs

- The invocation argument: the build configuration, `Debug`, `Profile`, or
  `Release`; `Debug` when absent.
- The session baseline, for each fix's change size in step 6.

## Steps

1. Set up the run. Done when the configuration, vcperf path, session name, and
   run directory are recorded and its `baseline/` and `after/` directories
   exist.
   - Configuration: the invocation argument, `Debug` when absent; stop on any
     other value. Targets: Client and Server.
   - vcperf path: the last line this query prints; stop when it prints none.

     ```powershell
     & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -version '[18.0,19.0)' -products * -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\vcperf.exe'
     ```

   - Session name: `BrokenEngineBuild-<worktree leaf directory name>`, so two
     worktrees never share one.
   - Run directory: `Temp/optimize-build-time/<UTC yyyyMMddTHHmmssZ>/`, with
     `baseline/` and `after/` created under it. vcperf calls take its absolute
     path.
2. Measure into `baseline/` with this measure sequence. Done when
   `baseline/summary.json` exists from a `Measure-BuildTrace.ps1` exit 0, or
   the skill has stopped with a report.
   1. Start the trace: `### vcperf calls` Start, with its fallbacks.
   2. Delete both targets' intermediate directories from the worktree root, so
      MSBuild recompiles every translation unit, `Pch.cpp` included, and
      relinks:

      ```powershell
      foreach ($path in 'Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Build/BrokenEngineSandbox/<Configuration>', 'Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Build/BrokenEngineSandboxServer/<Configuration>') { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse } }
      ```

   3. Dispatch one `builder` running `/compile` for Client then Server at the
      configuration, stating no agent-harness scenario and Shared data mode
      because no Local trigger path changed. Its `Evidence` row for the
      envelope file is the summary's `-BuildEnvelope` input.
   4. Stop the trace into the phase directory: `### vcperf calls` Stop, with
      its elevated retry. Stop also after a failed build, then report the
      builder's failing row and end.
   5. Summarize the trace. Exit 0 continues; on exit 2 or 1, report the
      printed `code` and `message` and end.

      ```powershell
      pwsh -NoProfile -File .agents/skills/optimize-build-time/scripts/Measure-BuildTrace.ps1 -Trace '<phase directory>/build.json' -BuildEnvelope '<envelope file>'
      ```

3. Dispatch one `researcher` with the `baseline/summary.json` path and the
   categories below to return evidence-backed fix proposals. Done when every
   proposal names its `repository-path:line`, the ranked item and measured
   cost it removes, its category, and whether it is applicable.
   - Applicable: `Pch.h` contents, unused includes, forward declarations, and
     moving includes or bodies out of headers.
   - Recommendation only: compiler, linker, and `.vcxproj` option changes, and
     any change to a `/compile` Local trigger path, such as
     `Common/DataFile.h`
     ([`runtime-data-mode.md`](../compile/references/runtime-data-mode.md)
     `## Mode selection`).
4. Run the Change Workflow from its Approve and classify step through its
   Apply the triggered cleanup step, with the applicable proposals as the
   request. Done when that cleanup step has completed and the files each fix
   changed are recorded.
   - With no applicable proposal, report the baseline rankings and the
     recommendations instead, and end.
   - The nested run skips only the user-facing pauses `## Rules` names.
5. Measure into `after/` with step 2's measure sequence, adding
   `-Baseline '<run directory>/baseline/summary.json'` to the summary call.
   Done when `after/summary.json` exists from an exit 0, or the skill has
   stopped with a report.
6. Report the result to the user. Done when every item below is presented.
   - From `after/summary.json` `comparison`: each metric's before, after, and
     percent (positive is faster), and each target's translation-unit count
     before and after.
   - The foreign-invocation count and time as a noise flag (other builds on
     the machine), the single-sample caveat (one build per phase, so each
     percent includes run-to-run noise, and noise alone can leave an item
     `belowFloor`), the recommendation-only proposals, and both `build.json`
     paths.
   - Full-rebuild check: each target's after count must equal its baseline
     count plus the net `ClCompile` files the fixes added; otherwise say the
     after measure was not a full rebuild.
   - Per applied fix: its change size, the files and added plus removed lines
     from `git diff --numstat <session baseline> -- <files it changed>`, an
     untracked new file counting all its lines as added; its targeted ranked
     item's before, after, and percent, or for a `belowFloor` row, its before
     and that its gain is unmeasured, because the after trace drops entries
     under vcperf's 10 ms floor; and the overall `buildElapsedMs` percent.
   - End by flagging each fix whose change is large (more than 100 changed
     lines or more than 5 files) and whose gain is small (its targeted item
     improved by less than 5 percent) or unmeasured.
7. Continue the Change Workflow with its Verify the acceptance table and
   Verify and land steps, where `/finalize-changes` still asks its landing
   confirmation. Done when `/finalize-changes` has returned or the user has
   declined landing.

### vcperf calls

Run each call from the worktree root as its own PowerShell tool call, never
Bash, so its output and exit code are read directly. `<vcperf>` is the
resolved path, `<session>` the session name, and `<build.json>` the absolute
path of the phase directory's `build.json`.

- Start: `& '<vcperf>' /start /noadmin /level3 <session>; "exit $LASTEXITCODE"`,
  where `/level3` with Stop's `/templates` collects template data. Exit 0
  continues. Any other exit goes to Grant: on `/noadmin`, a missing
  grant and an already running trace give the same exit codes and message.
- Grant: tell the user a UAC prompt follows for vcperf's one-time permission
  grant, which lets later runs trace without administrator rights, and that
  they may decline it. Run the elevated form with `'/grantusercontrol'`; on
  exit 0, retry Start once, and its exit 0 continues.
- Elevated start, only when the retried Start still fails: tell the user the
  unelevated start still failed and an elevated start follows, then run the
  elevated form with `'/start','/nocpusampling','/level3','<session>'`. Exit 0
  continues.
- Stop:
  `& '<vcperf>' /stop /templates <session> /timetrace '<build.json>'; "exit $LASTEXITCODE"`,
  which writes only the `/timetrace` JSON, never an `.etl` file. Exit 0 with
  `build.json` present continues. Otherwise go to Elevated stop, which also
  covers a session started elevated.
- Elevated stop: run the elevated form once with
  `'/stop','/templates','<session>','/timetrace','"<build.json>"'`. Exit 0
  with `build.json` present continues.
- Elevated form: write each path element as `'"<absolute path>"'`, because
  `Start-Process` joins the elements with single spaces and never quotes them.
  It prints vcperf's exit code but not vcperf's output, and a declined UAC
  prompt makes it throw.

  ```powershell
  (Start-Process -FilePath '<vcperf>' -ArgumentList <elements> -Verb RunAs -Wait -PassThru).ExitCode
  ```

- Any other outcome stops the skill with a report: a declined UAC prompt, a
  nonzero elevated exit, or an elevated Stop leaving no `build.json`. Give the
  failing call's exit code, plus its output when unelevated.
- In that report, for a failed elevated start, add that vcperf's start error
  outside `/noadmin` names a running trace as its cause. After a failed Stop,
  tell the user that `<session>` may still be collecting.

## Rules

- Running this skill is the user's approval for the fixes it selects. Root
  `AGENTS.md` `## IMPORTANT: Session rules` lets direct instructions from the
  human user override this repository's safety policies, so the nested Change
  Workflow run skips its user-facing pauses.
- Those pauses are exactly the `/plan-alternatives` presentation (main takes
  the simplest candidate itself), the Tier-3 `/external-grill-plan`
  interview (its rounds still run, but main answers each decision itself with
  the simplest option), and plan approval. Every automatic review, build, and
  static check still runs, and so does the `/finalize-changes` landing
  confirmation.
- Apply only the categories step 3 marks applicable, and report every other
  proposal as a recommendation; a Local trigger path change stays a
  recommendation because both measures must build in Shared mode.
- Keep the two measures like for like: the same targets, configuration, full
  rebuild, and machine.
- Follow every successful Start with a Stop, including after a failed build.
- Use the elevated forms only in the order and on the failures
  `### vcperf calls` names, and state no reboot or sign-out behavior of the
  grant.

## References

- [`scripts/Measure-BuildTrace.ps1`](scripts/Measure-BuildTrace.ps1) — trace
  summary and before/after comparison; its header owns the output schema and
  exit codes.
- [`../compile/references/runtime-data-mode.md`](../compile/references/runtime-data-mode.md)
  — the `/compile` Local trigger paths.
