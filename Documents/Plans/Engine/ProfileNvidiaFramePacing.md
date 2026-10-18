<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T16:16:35.926Z","dependsOn":[]} -->
# Profile NVIDIA Frame Pacing Below Refresh Rate

## Context and original request

The user requested profiling soon and explicitly requested a saved, discoverable handoff for another machine. This
document recommends one bounded profiling protocol, including a temporary GPU workload edit, followed by complete
restoration. It does not presume an engine defect or authorize a product fix from an unproven hypothesis.

Original instructions, verbatim:

> Do some profiling in Debug and Profile
>  STUTTER when GPU < 100 fps
>  Without netork delay (turn off 'China' lag and have none)
>  deltaNs spike -> only in PROFILE (Or just remove, with reconcilliation this can spike)
>   And fix the log it makes its all zeros

> The notes above refer to something I noticed a while ago that may not even be true anymore. My monitor is 120Hz and should be dynmaic refresh rate with VSync, when game framerate got low-ish like 100fps it seemed like things were no longer smooth (even though 100 fps is not bad). So you should force low framerates by running client in UHD and double-triple rendering to reduce framerate to 100/60/30 fps and measure smoothness and spikes to see if you can find any issues.

Later clarification: the current machine has an AMD RX 9070 XT; AMD Software reports adaptive refresh as “Not
supported.” The monitor is the original Acer X27 G-SYNC model, not the newer X27U OLED. The user requested wrap-up and a
full handoff for profiling on an NVIDIA machine. The original suspicion may no longer reproduce.

## Design and completion boundary

The author recommends repeated instances of the existing HDR resolve fullscreen draw as the sole temporary GPU load
mechanism. The earlier experiment demonstrated useful scaling without repeating frame orchestration. Use the same
mechanism on the NVIDIA machine, recalibrating instance counts to actual achieved rates. Run matched Debug and Profile
static and verified zoom-motion windows at native load and approximately 100, 60, and 30 fps, with VSync and functioning
G-SYNC established first. Record focus throughout and include a controlled background contrast at approximately 100 fps.
Correlate native GPU/CPU timings, application present intervals, logs, and input timestamps before drawing a cause
conclusion.

Successful execution produces an evidence-backed diagnostic conclusion and restores the temporary changes. “No
reproducible engine defect under these conditions” is a valid conclusion. A failed hardware prerequisite produces a
blocked handoff with explicit evidence; it is not completion of the NVIDIA experiment. A verified defect requires
separate scoped fix work, not an improvised fix in this protocol.

This is a decision-complete temporary profiling protocol, so the recommended destination is
`Documents/Plans/Engine/ProfileNvidiaFramePacing.md`, created with `/save-plan` and its metadata script. There are no
code dependencies: the separately repaired ImGui descriptor leak is not a dependency or proposed stutter fix. At
execution, a claim still requires the normal user approval under the repository Change Workflow.

## Established evidence from the AMD session

The investigation started at commit `c82f8cfeb1a8332d404e271541d58174ad3bb7b7`. Read current code before applying the
temporary edit because line numbers can move. The prior runtime confirmed RX 9070 XT, UHD 3840×2160, and 120 Hz.
`NetworkSimulation` was already compiled with `kDisabled`; no simulated China delay was active. The `deltaNs` detector
was already Profile-only. Current logs contain nonzero warnings, including `0.0222 > 0.0083` and `0.1363 > 0.0261`; the
emitted values are seconds despite the name. No all-zero log fix was justified.

The source defaults to FIFO with a three-image preference. No general render multiplier exists. Maximum tested UI
graphics quality, MSAA 8×, and sample shading could not lower the empty-scene frame rate below 120. Do not repeat that
unsuccessful settings search as routine preparation.

Changing only the HDR resolve draw instance count increased HDR timing from approximately 90 μs at N=1 to 168–171 μs at
N=2 and 252–271 μs at N=3. N=80 produced approximately 5.4 ms HDR and 7.9 ms summed root GPU work while remaining at 120
fps. N=120 produced approximately 100.5–100.8 fps, 7.4–7.5 ms HDR, and 9.7–9.9 ms summed root work in both
configurations. N=220 later produced approximately 68 fps in five native samples per configuration; this was not a full
30-second PresentMon/cohort run. There is no completed 60 fps or 30 fps measurement. Instance counts are not portable
between GPUs.

At N=120 the AMD application present-interval results were:

| Configuration/condition | Median ms | p95 ms | p99 ms | Maximum ms |
|---|---:|---:|---:|---:|
| Debug static | 9.8429 | 12.6425 | 17.3957 | 21.4649 |
| Debug zoom motion | 10.0095 | 10.9787 | 11.5602 | 20.8072 |
| Profile static | 9.4041 | 16.7648 | 16.8991 | 25.6825 |
| Profile zoom motion | 9.8792 | 10.7658 | 16.8298 | 17.1527 |

In Debug, 144 of 149 long intervals clustered between 13.86 and 21.06 seconds; short/long medians were 8.225/17.006 ms,
roughly one long interval every five frames, without a sustained GPU-work increase. Profile also switched between
approximately 8.33/16.67 ms and approximately 10 ms. This is consistent with fixed-120-Hz FIFO behavior; it does not
prove an engine defect or identify a VRR failure. AMD adaptive-refresh unsupported status limits interpretation. Earlier
N=3 foreground PIDs belonged to other applications, and N=120 focus was not recorded. Focused-versus-background behavior
remains an essential missing control.

Prior raw artifacts were local `Temp/pacing-live.md`, `Temp/pacing-n2.md`, `Temp/pacing-n3.md`, `Temp/pacing-n80.md`,
`Temp/pacing-n120.md`, `Temp/pacing-n120-diagnosis.md`, and `Temp/N220-HarnessHandoffs.md`. They may not transfer to the
next machine. The facts and protocol needed to execute are included here; do not depend on those files existing.

## Critical files and tools

- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`: existing `kPipelineHdrResolve` `RecordDraw` call,
  previously line 449, between the existing HDR GPU start/stop calls.
- The existing `Pipeline::RecordDraw` implementation, HDR resolve shader, fullscreen vertex shader, and pipeline state:
  read-only proof that the instance count reaches `vkCmdDrawIndexed`, vertex position/UV do not vary with instance,
  fragment code ignores instance, and blending/depth do not alter repeated output.
- Current `NetworkSimulation` definition and delta warning implementation: read-only verification of disabled artificial
  lag, Profile-only detector, and current formatting.
- `/compile`: every build; `/agent-harness`: all game launch, driving, queries, screenshots, logs, and release
  operations; `/external-diagnose-bug`: cause investigation. Read applicable subsystem instructions before edits.
- Official portable PresentMon 2.6.0, release binary SHA256
  `b2a706bc6ad475749e3b7e3409263aa1e6906d45bdcf993f6dbc0f660188f1af`; obtain and verify it locally rather than assuming
  prior Temp tools transfer. Console documentation:
  https://github.com/GameTechDev/PresentMon/blob/v2.6.0/README-ConsoleApplication.md . Original X27 product context:
  https://www.acer.com/ch-de/predator/monitors/x27 . Recheck current NVIDIA setup documentation when configuring the
  machine.

## In scope

- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`: temporarily replace only the HDR resolve `RecordDraw`
  instance-count argument, originally `1`, with a positive calibrated integer; restore the original file bytes
  afterward. No other render pass or frame orchestration edit is authorized.
- Ignored `Temp/` artifacts for this experiment: source/settings byte backups and hashes, bounded measurement/Win32
  focus helpers, raw machine/settings/focus/process evidence, full query JSON, present CSV/stdout/stderr, logs/CPU
  dumps, screenshots, and a final diagnostic handoff. Helpers schedule and collect measurements; they add no persistent
  engine feature or testing framework.
- Experiment-owned persisted `GraphicsSettings` and `ClientState` files: temporarily set UHD/VSync and the matched
  scene/camera; restore their original bytes after every completed experiment. Discover exact paths from the current
  harness output rather than copying the prior machine's paths.
- NVIDIA driver/monitor environment: establish and record G-SYNC support, enabled status, active state in the tested
  window mode, refresh rate, and actual foreground game PID; use a bounded external Win32 activation helper for the
  owned client only. Return any OS-denied activation to the user to focus the window.
- The final result within the Plan execution's normal completion artifact: measured achieved rates, coverage, timing
  distributions, focused/background contrast, log assessment, supported causal conclusions, limitations, and restoration
  receipts. No additional tracked documentation is required for the experiment.

Scope is both target and ceiling. Naming a file grants no authority to edit other regions. Make only the smallest
complete change needed for this protocol; return any nontrivial new decision to the calling session.

## Out of scope

- Permanent render multipliers, settings/UI/command additions, present-mode changes, swapchain count changes, new
  tracing subsystems, driver-policy or game focus-policy changes, synthetic activation keys, CPU frame caps, and sleeps
  added to rendering.
- Repeating the entire Main render path, particle work, query ranges, semaphores, or submissions to generate load;
  changing simulation, reconciliation, network trust, CRC behavior, or shared layouts.
- Speculative stutter fixes, warning removal/threshold changes, or a log-format fix without a reproduced formatting
  defect. The ImGui descriptor repair is already separate work; do not repeat it or claim it resolves pacing.
- Claims about physical panel presentation, VRR failure, display drops, simulation motion, or continuous camera panning
  from application present intervals and sampled zoom positions alone. Unit tests are prohibited by repository
  instructions.

## Risk and invariants

The temporary render-load change is Tier 2. Preserve all existing render-pass, query, synchronization, descriptor,
output-image, and deterministic simulation invariants. The sole changed draw count must remain within one existing
pass/query/submission. Verify its current implementation and shader contracts before using the earlier proof. If this no
longer holds, stop the workload edit and return the specific changed invariant for scope/tier review rather than invent
another mechanism. Escalate any newly reached Tier-3 trigger under the current risk-tier reference.

Save original source bytes and SHA256 before editing. Restore those bytes with a current modification timestamp so
incremental builds rebuild the uninstrumented source; restoring an old timestamp can leave loaded binaries behind.
Preserve settings and client-state bytes/hashes before the first launch. Leave no temporary workload source delta or
loaded build as the final verified runtime.

## Execution protocol

1. Establish a suitable NVIDIA/G-SYNC machine before building the experiment. Record GPU identity from the actual
   runtime, driver version, original Acer X27 identity, 3840×2160 at 120 Hz, connection, Windows and NVIDIA VSync/G-SYNC
   settings, and window mode. Confirm G-SYNC enabled and active during normal focused play, using available NVIDIA
   indicator/settings evidence. Record the exact evidence rather than equating a checked setting with activation. If
   support or activation cannot be established, return BLOCKED with the missing prerequisite; do not silently substitute
   the AMD machine or label a fixed-refresh run the requested G-SYNC test.
2. Use `/compile` to build client and server Debug and Profile. A build worker owns builds only; a runtime worker using
   `/agent-harness` owns launches and measurements. Inspect current lag/delta code. If artificial lag is no longer
   disabled or detector semantics changed, return that mismatch before expanding source scope. Record current nonzero
   warning/log behavior rather than carrying the original all-zero allegation forward as fact.
3. Create an isolated artifact directory and source/settings backups. Use a fresh harness claim per configuration and
   retain process IDs, paths, readiness, and heartbeat evidence. Keep the owned client/server pair intact until its
   clean release; quitting both and relaunching only one under the same claim can produce a stale-process false finding.
   Preserve any pre-existing `ProfileDump.csv` before every Profile launch because it holds CPU timer samples at
   approximately 1 Hz and may be overwritten.
4. Establish the shared empty island/water scene: basis `[0,0]`, camera approximately `[20.4,-76.3,354]`, zero units,
   UHD framebuffer. Record actual scene and effective settings for both configurations and keep them matched. Prior
   effective settings were FIFO, MSAA off (4× selected), sample shading off/minimum 0.60, water High, terrain
   shadow/lighting/smoke Medium, anisotropy 16, mip bias 1, terrain cadence 4, lighting cadence 2, smoke area 1.20,
   stagger smoke/wind enabled. Use existing controls; do not add new ones. Take an N=1 baseline screenshot before
   editing for restored-image comparison.
5. Establish actual foreground focus, verifying `GetForegroundWindow`'s owning PID equals the harness-owned client PID.
   The harness `window_state` restore path uses `SW_SHOWNOACTIVATE`; a visible/restored window is not proof of focus and
   there is no harness activation command. If needed, use one bounded external `SetForegroundWindow` helper targeted
   only at the owned client, verify success, and request manual focus if the OS denies it. Do not change game activation
   behavior or send synthetic activation keys. Log actual foreground PID with timestamps throughout each capture,
   preferably on the existing one-second sample schedule and at input boundaries. Any observed loss invalidates a
   focused-cohort claim; reacquire and repeat that window.
6. Measure N=1 first, then temporarily increase only the HDR resolve instance count. N=2/N=3 already established the
   mechanism; do not repeat those builds unless scaling fails on the new machine. Adapt counts from warmed HDR/root
   timings and achieved fps, increasing or decreasing the integer rather than replaying all previous counts. Select one
   count per rate, shared between Debug/Profile when achieved rates permit. Aim within 10% of 100, 60, and 30 fps; if a
   shared count cannot achieve that, calibrate separately and report the actual count/rate. This tolerance is a protocol
   recommendation, not a claim that exact target rates are possible under every present policy. Require increased GPU
   work as the load cause; do not replace it with CPU caps or render sleeps. Report any target blocked by a changed
   mechanism or hardware limit rather than relabeling another rate.
7. For each configuration at N=1 and each of the three achieved load levels, discard at least 10 seconds of warm-up,
   issue one discarded warm native query, then collect a 30-second static window: 30 full `query_profile` responses at a
   one-second schedule, grouped into five equal six-sample cohorts. Retain full GPU rows/current/average/max, native
   clock/counters and publication counts, fps, query timestamps, focus, and raw responses. GPU rows can be latched;
   smoothers overlap. Summed root GPU timers are observed work-duration sums, not utilization or independent GPU-busy
   measurements; do not add nested stages again.
8. Collect a separate 30-second zoom-motion window at every configuration/load, after restoring the baseline camera.
   Send 12 coordinate-bearing wheel events at `(1800,1500)`, alternating `+2/-2` notches at elapsed 0,2,…,22 seconds,
   then no input through 30 seconds. Retain accepted input times and scene samples proving movement. Prior eye Z ranged
   approximately 323.44–353.02 m from a 354 m baseline; establish actual movement again. WASD gameplay panning did not
   move the camera and is not a valid substitute. Capture the same native/profile/focus evidence as static windows. Keep
   static and motion windows disjoint; screenshots and settings transitions stay outside all measured windows.
9. At approximately 100 fps in both configurations, collect one additional matched 30-second static background window
   using the same measurement recipe, deliberately moving focus to a known other application and recording its PID.
   Restore and verify game focus afterward. Compare with the corresponding focused static window before attributing
   periodic pacing changes to the engine. The background condition is not a substitute for a valid focused window.
10. Run PresentMon concurrently with each timed window using PID filtering and reduced tracking, unique session/output
    names, and retained stdout/stderr. Launch the helper hidden. The known reliable invocation is
    `PresentMon-2.6.0-x64.exe --process_id PID --session_name UNIQUE --v1_metrics --no_console_stats --no_track_display --no_track_gpu --no_track_input --output_file PATH --timed 30 --terminate_after_timed`.
    Expand PID, UNIQUE, and PATH to the owned client and experiment paths. Preserve exit status and actual capture
    start/end times. Sampling sleeps belong only in the external collector, never in engine rendering.
11. Check capture completeness before computing pacing conclusions: observed span should cover approximately 30 seconds
    and row counts should agree with the native published-frame advance. Report coverage numerically. Prior reduced PID
    mode achieved approximately 99% or better coverage; investigate a materially lower count or shortened span and rerun
    instead of calling missing rows stalls. Process-name/default full tracking previously captured only about four
    seconds of a nominal 30 despite continuing native GPU work. PresentMon can label Vulkan-associated records `DXGI`;
    that field alone is not evidence the game changed graphics API. Full display/GPU tracking is an optional focused
    diagnostic only if a specific unresolved question needs it and its own coverage is adequate, not automatic extra
    work.
12. For every complete window report achieved native fps, count and span, median/p95/p99/maximum application present
    interval, counts above 1.5× and 2× its median, and locations of outlier clusters. Align raw present times, GPU
    timers, CPU dump samples, warning times, focus and zoom events on a recorded clock mapping. Compare Debug/Profile,
    static/motion, native/loaded, and focused/background at matched actual rates. Preserve warnings before/after each
    window and full process logs; empty Debug warning output cannot prove no outliers because the detector is disabled
    there. Do not infer a causal relationship merely from overlapping smoothers or coarse 1 Hz samples.
13. If a reproducible issue remains, use `/external-diagnose-bug` within the collected evidence and bounded
    existing-code inspection to prove or narrow the cause. Distinguish acquisition/fence wait, submission/work,
    application cadence, background behavior, and fixed-refresh quantization only as evidence permits. Report remaining
    hypotheses as unproven. Return any product change as a separately scoped proposal; the experiment does not authorize
    it.
14. Cleanly release each claim through `/agent-harness`; retain empty `crashFindings` and surviving-process arrays, or
    explicitly report failures. Restore original source bytes with current timestamp and verify source hash, rebuild
    uninstrumented Debug and Profile clients through `/compile`, and run a short N=1 smoke/image comparison in each
    configuration through `/agent-harness` outside measurement cohorts, using the baseline scene/settings as needed.
    Cleanly release both smoke runs and verify no owned processes survive before restoring persisted settings/client
    state to their original bytes and verifying original SHA256 hashes. Retain final restoration receipts timestamped
    after the final smoke shutdown, which saves graphics settings and client state. Inspect the final Git diff/status
    against the execution baseline, preserving others' changes. Retain raw evidence and a concise final diagnostic
    handoff with achieved matrix and limitations.

## Acceptance criteria

| Criterion | Required evidence |
|---|---|
| Suitable hardware and active G-SYNC | Actual NVIDIA runtime identity, original X27/UHD/120 Hz, effective VSync/window settings and G-SYNC activation evidence; unsupported setup explicitly blocked |
| No artificial network lag | Current compile-time disabled setting and successful owned client/server startup, distinguishing normal loopback scheduling latency |
| Original warning/log requests assessed | Current detector build guard, full logs and representative emitted values; only a demonstrated formatting defect may justify a separate fix proposal |
| Four load levels in both configurations | N=1 and measured approximately 100/60/30 fps, actual counts/rates and warm GPU scaling evidence; 60/30 never inferred from the prior N=220 samples |
| Static and real motion measured | Separate complete 30-second windows at each configuration/load, five six-sample native cohorts each, accepted zoom timestamps and measured camera changes |
| Focus and presentation coverage valid | Foreground PID evidence throughout focused windows, matched approximately-100-fps background contrast in both configurations, full capture spans/count coverage before percentile analysis |
| Pacing assessed within evidence limits | Per-window distributions/outlier counts, clock alignment, Debug/Profile and motion contrasts, logs/CPU/GPU correlation, explicit conclusion and unresolved limitations without unsupported panel/VRR claims |
| Temporary workload preserves contracts | Only HDR instance count edited; current pipeline/shader proof; no repeated queries, passes, synchronization or simulation changes |
| Restoration complete | Clean release receipts, original source hash and uninstrumented rebuilds, N=1 smoke/image comparison in both configurations, original settings/state restore and hash receipts after final smoke shutdown with no owned processes surviving, and final diff excluding temporary workload changes |

If a required row cannot be met, return its exact evidence gap and status to the caller rather than asserting experiment
completion. No production stutter fix is required for diagnostic completion, and no unproven fix is permitted.
