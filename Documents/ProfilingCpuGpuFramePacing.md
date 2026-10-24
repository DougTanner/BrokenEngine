# Investigating CPU/GPU frame pacing

Use this runbook to turn an intermittent presentation gap into an interval with
identified CPU states, native wait callsites, and independently timed GPU work.
Start with the cheapest measurement that can distinguish the remaining causes.
The commands and decoder contracts below stand alone; historical helpers under
ignored `Temp/` are not required or guaranteed to exist in a fresh worktree.

## Handoff and operating contract

Give the next session the symptom, reproduction controls, source/build hashes,
current process identities, measurement QPC bounds/frequency, artifact root,
largest exact present intervals, settled causes, and remaining unknowns. Separate
successful capture, successful diagnosis, and verified performance improvement.

All game claims, launches, driving, state queries, screenshots, logs, and releases
go through `/agent-harness`. Read that skill's current workflow; this document
does not replace its ownership or persisted-state preservation contract. Obtain
matching `/compile` receipts for `BrokenEngineSandbox` and
`BrokenEngineSandboxServer`, `Profile|x64`, for the sources being measured. Save
both EXE/PDB pairs before another build overwrites them. A build with diagnostic
probes and a build with a production fix are distinct experiments.

Work from the session worktree root. Repository scripts retain their documented
form, `pwsh -NoProfile -File <repo-relative-script> <arguments>`; do not change
directory or execution policy. Do not launch a game, build, or tracing session
merely by reading this document. Probe changes require their own authorized
scope; these are diagnosis techniques, not instructions to review or remove
temporary probes.

Preserve initial persisted-state presence, bytes and hashes before any writers;
retain changed runtime output, then restore originals only after final writers
stop. Release the viewer pin, exact owned game PIDs and harness claim. Keep full
startup through measurement-end logs, not just the selected window: startup
recovery, recreation, and process-lifetime logging budgets affect interpretation.

Enumerate every persisted-state writer and its actual redirected path, including
`GameSettings.bin` (language/font scale), beyond graphics/client-state backups.
Verify the requested settings before capture; changing scale after launch does
not recreate a fresh font atlas. Use a fresh process for an atlas baseline.

## 1. Establish the application presentation baseline

Record fullscreen geometry, native resolution/refresh, focus/visibility, driver
and GPU, user-confirmed G-SYNC/VSync settings, network simulation (the successful
case used default disabled/zero), scene and workload identity, and any FPS cap.
Use the same controls between runs. Do not introduce a guessed cap to hide a
cadence problem. Focus loss invalidates the controlled window; record it rather
than recovering focus silently during measurement.

Discover installed tools once and retain their versions/help. These discovery
commands do not trace or launch the game:

```powershell
$Pwsh = (Get-Command pwsh -ErrorAction Stop).Source
$Wpt = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/Windows Performance Toolkit'
$Wpr = Join-Path $Wpt 'wpr.exe'  # ADK, not an accidental inbox WPR
$Xperf = Join-Path $Wpt 'xperf.exe'
$GpuProfile = (Join-Path $Wpt 'gpuview/log.wprp') + '!GPUView.Light'
$Tracerpt = (Get-Command tracerpt.exe -ErrorAction Stop).Source
$PresentMon = (Get-Command PresentMon.exe -ErrorAction Stop).Source
# If PresentMon is not on PATH, supply its actual downloaded executable path.
$Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$VsRoot = & $Vswhere -latest -property installationPath
$Llvm = Get-ChildItem (Join-Path $VsRoot 'VC/Tools/MSVC/*/bin/Hostx64/x64/llvm-symbolizer.exe') |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
Get-Item $Pwsh,$Wpr,$Xperf,$Tracerpt,$PresentMon,$Llvm |
    Select-Object FullName,@{n='Version';e={$_.VersionInfo.FileVersion}}
& $Wpr -help advanced
& $PresentMon --help
```

The working capture used ADK WPT 10.0.26100.7705 and PresentMon 2.6.0 on
2026-10-19; these are provenance, not required latest versions. Confirm installed
flags and the profile's actual providers before reuse. Do not hardcode a
versioned WindowsApps PowerShell path or a previous run's PID.

For a standalone 60-second baseline, set `$GamePid` from the current harness
process receipt and `$RunRoot` to a fresh artifact directory. `$PID` is a
PowerShell automatic variable; do not repurpose it.

```powershell
$Instance = 'BEHitch_' + [guid]::NewGuid().ToString('N')
$RunRoot = Join-Path (Get-Location).Path ('Temp/FramePacing/' + $Instance)
if (Test-Path -LiteralPath $RunRoot) { throw 'Use a fresh RunRoot' }
$null = New-Item -ItemType Directory -Path $RunRoot
# $GamePid = <current client PID from /agent-harness>
& $PresentMon --process_id $GamePid --session_name $Instance --v1_metrics `
    --qpc_time --no_console_stats --no_track_display --no_track_gpu `
    --no_track_input --output_file "$RunRoot/presents.csv" `
    --timed 60 --terminate_after_timed
if ($LASTEXITCODE -ne 0) { throw 'PresentMon failed; retain its output' }
```

Retain every raw row. Group by current PID/swapchain, sort raw `QPCTime`, and
compute consecutive differences: `gap_ms = (q[i]-q[i-1])*1000/frequency`.
The first row has no captured predecessor. Report median, p95/p99 with the
quantile convention, maxima, threshold counts, and exact endpoints. Define
whether a boundary-straddling interval belongs to its ending present or is
excluded. API duration, application inter-present interval, queued presentation,
flip and physical scanout are different observations. A low `Present` API time
can coexist with delayed issuance. Disabled display/GPU tracking cannot measure
panel scanout. See the [PresentMon project](https://github.com/GameTechDev/PresentMon).

## 2. Split CPU phases before expanding instrumentation

Use deferred steady-clock nanosecond endpoints around the actual operations:
main-loop reset, render entry/return, current fence status/wait, conditional
all-framebuffer drain, mutation, worker publication/entry/callback return,
present/future join, tail image acquire, next-loop input and network drain.
Record owner TID, sequence, framebuffer slot, actual native result, and lifecycle
identity. Keep normal synchronization and call order unchanged. An
`Acquire to global` timer can start on main and stop on a worker; its recorded
thread is not automatically the main thread.

Record reset-before/reset-after and QPC-before/steady-anchor/QPC-after plus QPC
frequency. Old `deltaNs` text alone was insufficient to identify endpoints;
neither reset interval nor a render counter is a presentation-frame identity.
Check the measured clock bridge for this run instead of assuming a common
epoch. The successful run observed steady_ns approximately `100*QPC` at 10 MHz,
within its bracketing uncertainty; this is not a portable conversion constant.

Retained marker contracts used in the investigation:

| Family | Essential evidence and interpretation |
|---|---|
| `PACING_SPIKE` | reset endpoints, anchor/QPC bracket/frequency, delta/mean/threshold, dump/cap/rate state |
| `PACING_CPU` | dump, sequence, timer/name, TID, flags, start/end/elapsed ns; deduplicate sequence |
| `PACING_MAIN`, `PACING_WORKER` | actual post-fence/drain/mutation/pre-submit and wake/entry/return boundaries; join same slot and unique containing intervals |
| `PACING_PREFIX_FENCE` | direct status/wait begin/end, initial status, waited, owner TID, observed render/slot |
| `PACING_PREFIX_ACQUIRE` | separate image-fence and native acquire intervals, process-lifetime acquire sequence, previous slot, success-only acquired slot, raw result, owner TID |

Prefix probes selected completed intervals at least 20 ms, with independent
256-row process-lifetime budgets and no rate suppression. Absence only excludes
that threshold on completed emitter paths while budget remains; exceptions can
escape before emission. Join successful acquire only to the actual following
same-thread matching-slot render in its lifecycle, not the just-presented frame.

Legacy completion history was filtered at 8 ms, retained only 64 entries, eligible
for 250 ms, printed at most 24 rows, and dumped at least 500 ms apart. Main/worker
logs also had 500 ms rate limiting. Later probes allowed 256 spikes and 64 dumps;
earlier runs had smaller budgets. Recheck compiled gates and footer predicates:
a stale `dump_cap` comparison at 16 falsely declared exhaustion although the
actual gate was 64 and later dumps existed. No explicit cap does not mean full
history. Missing samples mean unretained work, not zero work.

Bracket diagnostic formatting/output separately: each owner records
`emit_begin_ns` plus the previous actual emission's begin/end. The current end
is available only in its next emitted row; terminal ends remain unknown.
Check all owner chains against the interval, including prior emissions. An
emission after a measured operation cannot cause that preceding operation,
but missing/suppressed brackets cannot exclude observer work globally. Do not
subtract estimated overhead. Avoid timed game queries; a query-free control
reproduced a large reset residual, but differing runs did not estimate query
cost. Nested CPU/GPU timers must not be added; GPU result availability and
per-image cached roots do not establish a submission generation or CPU clock
alignment. Native submit/wait CPU elapsed is not GPU execution time.

## 3. Reproduce streaming with lifecycle proof

Use existing `/agent-harness` `set_client_grid_coord` with `coord:[x,y]` to pin
the observed initial viewer cell before measurement, switch to a distinct target,
then return. The controlled log runs used 20 seconds per phase (60 total), two
timed setters and zero timed queries. Record actual dispatch and acknowledgement
UTC/QPC brackets, not nominal 20/40-second boundaries. Choose duration with the
existing 300-render-frame eviction grace and references in mind: elapsed wall
time or setter acknowledgement does not prove eviction/subscription completion.
Release the pin through the harness after measurement.

A setter acknowledgement proves only the local pin. Before timing, verify server
admission and client subscribed cells/cell states against the current owned
fleet. Historical coordinates may now be rejected as the fleet moves; repeating
the old coordinate/timing sequence then measures a different scene. Origin is exempt from the adjacency check; verify actual admission because slot
allocation can still fail. Also verify live ownership, camera and scene controls. Keep setup queries outside the measured window.

Prove the resource chain from records, not camera motion:

1. Completed `STREAM_RESIDENCY` grace/arena eviction names elevation CRC and
   old/current texture slot, mesh state, references and age.
2. A later mint of the same elevation CRC maps `colors_crc`, `normals_crc`,
   `ao_crc`, `masks_crc`. Only mint rows carry that channel mapping.
3. Per-channel `STREAM_UPLOAD` submission rows have contiguous half-open
   `source_begin..source_end`, first at zero and final at full source size,
   native submit begin/return, and final observed `fence_end_ns`.
4. The channel's transferred `STREAM_ADOPTION_DETAIL` follows final completion;
   completed elevation restoration/mesh upload establishes the final state.

CRC identifies a resource, not allocation or GPU generation. Nonfinal uploads
have `fence_end_ns=0`; fallback rows have no native submission interval. Join
`STREAM_DRAIN`, render-drain adoption and eviction/restoration sweeps by exact
render plus slot, with lifecycle chronology. `wait_for_textures` adoption is a
different context. Preserve signed pending counts without clamping or treating
separate observations as an atomic pair. Classify useful mutation from completed
transfers/fallbacks, evictions, restorations, mesh uploads, indirect clears and
reblur; CPU promotion alone is not a GPU mutation.

The diagnostic contracts were 256 drain rows, 256 sweeps per site, 256 adoption
summaries per context, 1024 shared adoption details, 1024 shared residency details,
and 1024 upload rows. These budgets survive graphics/upload-manager recreation;
they have no reset command. Track observed/emitted/suppressed and final cap rows.
After exhaustion, matching summary counts cannot recover missing CRC identities.
Every family has its own emission chain. Exact timer/probe presence must be
verified in the measured source/build rather than assumed from this document.

For streaming/settled cohorts, retain final submit, fence, adoption and mutation
endpoints separately; the latest relevant local completion defines settling.
The first-batch capture's last drain mutation followed its last transferred
adoption, so adoption alone would have cut the active cohort short. This proves
only captured-batch settling, not all-world residency. Retain boundary-crossing
gaps separately rather than silently assigning them to settled statistics.
Bracket screenshot commands and exclude every overlapping gap from natural
cadence statistics. Keep those gaps in the raw capture as observer activity.

## 4. Capture CPU scheduling and GPU queues together

When phase logs leave a gap unclassified, use the installed ADK
`gpuview/log.wprp!GPUView.Light` profile. It supplied SampledProfile, CSwitch,
ReadyThread and stacks, DxgKrnl queue/present events, context rundown and the
explicit hardware scheduling log collector. Built-in `GPU.Light` on this machine
omitted that dedicated scheduler keyword; CPU-only profiles cannot reconstruct
GPU chronology. Verify the resolved installed profile rather than guessing from
the word Light. See [GPUView](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/using-gpuview).

The successful experiment used one continuous 30-second recording: 10 seconds
stationary, 10 outward, 10 return, after the recording receipt. Derive PresentMon
offline from this ETL; do not add a simultaneous live PresentMon consumer.

Kernel/profile collection required an elevated trace controller. The ordinary
token's actual WPR start failed with `0xc5585011`; Performance Log Users
membership alone was insufficient here. Obtain the user's normal Windows UAC
approval for the controller; keep the game/harness ordinary-token. Do not change
policy, seek a bypass, or elevate the game. Discover `$Pwsh` as above if launching
an elevated shell; never reuse the old WindowsApps version path.

Only the controller invokes WPR. Use a fresh absent `$RunRoot` and unique
`$Instance` for every attempt. The following are the working native commands,
with variables set in that elevated PowerShell at the worktree root. Allocate
a new instance/root here even if the baseline already created a directory.
The controller creates its directory after verifying absence. The coordination
steps in the following table must occur at the indicated points; this command
block is the native command recipe, not an unattended controller script:

```powershell
& $Wpr -status -instancename $Instance
& $Wpr -status
& $Xperf -loggers
# Save raw stdout/stderr and exit codes from each command before proceeding.
$Owned = $false
try {
    # After readiness/start.signal and repeated successful preflight:
    & $Wpr -start $GpuProfile -filemode -recordtempto $RunRoot -instancename $Instance
    if ($LASTEXITCODE -ne 0) { throw 'Start failed; retain native error' }
    $Owned = $true
    # Publish recording.json, await stop.signal or 45-second fallback.
} finally {
    if ($Owned) {
        & $Wpr -stop "$RunRoot/hitch.etl" 'CPU GPU frame pacing' -skipPdbGen -instancename $Instance
        # Save exit code immediately; failed stop retains an owned-instance residual.
    }
}
& $Wpr -status -instancename $Instance
& $Xperf -loggers
```

`-instancename` is last on every named control command. Never use global
cancel/stop, `xperf -d`, or GPUView `log.cmd` as recovery. Preserve any native
failure; stop only a successfully started instance owned by this controller.
See [WPR commands](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options).

For coordination with a separate runtime worker, implement this small receipt
protocol in the capture controller or perform its steps manually with the same
evidence. No permanent controller helper is provided by this runbook:

| Artifact | Writer and contract |
|---|---|
| `ready.json` | Controller after actual admin-token/tool/profile checks, absent instance, positively idle WPR status and saved logger inventory; include instance, controller PID, absolute root and UTC/QPC/frequency. |
| `start.signal` | Runtime only after normal game setup/focus and source/process/state preservation; controller waits at most 300 s from ready, aborts on premature stop, repeats preflight immediately before start. |
| `recording.json` | Controller only after successful native start; include exact arguments, exit, instance, ETL path and command UTC/QPC brackets. Runtime waits for this before phase timing. |
| `stop.signal` | Runtime after bounded reproduction, also in runtime finally on failure. Controller polls at 100 ms and falls back to owned stop at 45 s after successful start return. |
| `terminal.json` | Controller after its finally attempts named save/stop and post-status/inventory; include successful-start flag, stop reason/exit, ETL existence, residual owned instance and failure details. |

Write receipts to a temporary file then rename in the same directory so readers
never accept partial JSON. Keep per-command arguments, exit, separate stdout and
stderr, UTC/QPC before/after. The runtime must observe terminal failure while
waiting; UAC refusal may produce no ready receipt at all. Timeout bounds the
readiness/recording waits, not WPR start or stop/flush/merge. Do not kill a
controller while saving. Missing terminal or failed stop requires inspecting
that exact owned instance, not a global cleanup. A finally cannot save after
forced termination or power loss.

Check disk/RAM first: this installed profile declared about 1.2 GB nominal
collector buffers, with potentially large file-mode output and perturbing stack
collection. This is no file-size cap or measured memory guarantee. Preserve
before/prestart/after inventory; a snapshot is not a lock against other tools.
Do not infer conflict from broad name regexes: `wPr` inside
`DefenderApiLoggerLowPriv` caused a false rejection. Nor does any existing
`NT Kernel Logger` prove incompatibility with an instance-qualified system
collector. Native named start decides compatibility; it never authorizes stopping
foreign sessions. See Microsoft's [instance naming explanation](https://devblogs.microsoft.com/performance-diagnostics/controlling-the-event-session-name-with-the-instance-name/)
and [StartTrace system logger rules](https://learn.microsoft.com/en-us/windows/win32/api/evntrace/nf-evntrace-starttracew).

## 5. Export, validate clocks, and reconstruct CPU states

Run offline after the controller has saved the ETL. Reuse the discovered tool
paths, captured `$GamePid`, and absolute `$RunRoot`; retain every exit code and
warning. These exact analysis flags worked:

```powershell
$Etl = Join-Path $RunRoot 'hitch.etl'
& $Xperf -i $Etl -o "$RunRoot/stats.txt" -a tracestats -detail
& $Xperf -i $Etl -o "$RunRoot/events.csv" -a dumper -add_fieldnames
& $Xperf -i $Etl -o "$RunRoot/thread-usage.txt" -a cswitch -thread
& $Xperf -i $Etl -o "$RunRoot/cpu-samples.txt" -a profile -detail
& $Tracerpt $Etl -of XML -rts -o "$RunRoot/events-raw.xml"
& $PresentMon --etl_file $Etl --process_id $GamePid --v1_metrics --qpc_time `
    --no_console_stats --output_file "$RunRoot/presents.csv"
```

Require zero event/buffer loss, usable presents, and actual CPU/GPU event-family
coverage before precise attribution. A tracerpt schema warning needs per-family
inspection of `EventData` versus `ProcessingErrorData`, not blanket acceptance or
rejection. Keep original ETL and raw XML for undecoded payloads and arrays.

Read the ETL clock mode and frequency from retained header/summary. Verify
PerfCounter before treating `TimeCreated/@RawTime` as QPC. Match at least two
same-PID present-start events to exact PresentMon QPC values. Do not guess epochs
or treat xperf's rounded trace-relative microseconds as absolute QPC. Retain UTC
and `Stopwatch.GetTimestamp()/Frequency` brackets at capture and phase boundaries.

For multi-GB XML, use `.NET XmlReader` streaming: advance to each `Event`, read
one outer XML element, parse its namespace-aware System/EventData, emit compact
JSONL/CSV and schema counts/examples keyed by provider/ID/version/opcode. Do not
load the entire XML into PowerShell, print bulk events into session context, or
flatten duplicate named array members irreversibly. Keep payload values and raw
header PID/TID separate. `SampleProf` uses payload InstructionPointer/ThreadId;
header PID/TID can be -1. Validate decoded counts against trace statistics.

Reconstruct each relevant thread independently, from rundown and this run's
markers/stacks: main, submit worker, and driver presenting thread may differ.
Re-derive roles from matched stacks after recreation: worker/presenting TIDs can
change. A timer that ends on a worker does not identify that worker as main.
Sort raw CSwitch/ReadyThread events by QPC, preserve unknown initial state, and
emit half-open segments on each transition:

```text
CSwitch NewThread == tid: Running
CSwitch OldThread == tid: Waiting if OldState == 5;
                         Ready if OldState is 1 or 7;
                         otherwise retain the actual state as unresolved
ReadyThread Thread == tid: Ready
```

Retain wait reason/CPU and clip segments to each exact present gap. Sum interval
intersections by state; Running + Ready + Waiting + unresolved must cover the
gap. Waiting is blocked time; Ready-to-next-run is scheduling delay; Running is
CPU time. Statistical sample counts do not measure a blocked span. Sample
absence does not establish absent execution elsewhere. Switch/ready stacks and
wait reasons supply the blocked callsite; aggregate utilization cannot.

Expand cached stack keys with their matching create/delete/rundown dictionary
and verify no missing keys. When stack records use rounded relative times, join
by TID/actual switch chronology with a stated tolerance, not pretend exact QPC
equality. The successful analysis used a preceding stack within 50 microseconds;
the first three decisive stacks preceded switch-out by about 5 microseconds.
Do not silently assign a stack to switch-outs lacking a usable match.

Symbolize against the retained matching EXE/PDB, with ETW image-load/rundown base
and source version/hash. Address minus actual loaded module base is the RVA;
ASLR means an old base is invalid. The working LLVM invocation was:

```powershell
# Fill from the current image/rundown and selected application stack addresses.
# $ImageBase = [UInt64]...; $Addresses = @([UInt64]..., [UInt64]...)
$Image = Join-Path $RunRoot 'Binaries/BrokenEngineSandbox.Profile.exe'
$Offsets = @($Addresses | ForEach-Object { '0x' + ($_ - $ImageBase).ToString('x') })
$Symbols = & $Llvm "--obj=$Image" --relative-address --output-style=JSON @Offsets
$SymbolExit = $LASTEXITCODE
$Symbols | Set-Content "$RunRoot/symbols-batch.json"
if ($SymbolExit -ne 0) { throw 'Retain failed batch; retry only smaller bounded queries' }
$Resolved = $Symbols | ConvertFrom-Json
```

Keep EXE/PDB together and retain input RVA/address order plus output. Resolve
function, source file and line, including inline frames, then inspect the
matching source at the native call. Installed LLVM resolved the original 75
addresses, but crashed on a later 229-address batch; DbgHelp failed with error
126. Smaller bounded queries with retained outputs are the practical recovery,
not discarding errors or claiming unresolved addresses were symbolized. Private
NVIDIA/Windows names were not available or required to identify the engine call.

If optimized line tables place adjacent native calls on an ambiguous line, use
the exact retained EXE/PDB and its PE preferred image base (`dumpbin /headers`).
Compute `RVA = captured_address - loaded_base`, then disassembly address =
preferred base + RVA. With the installed VS `dumpbin`, use `/disasm` and a
bounded `/range:start,end` around that address; align decoding before the call.
Treat a caller stack address as a return address: identify the immediately
preceding call, trace its indirect target through the PDB-named function pointer,
and check arguments and instruction length. Keep hashes and native output.
For example, RVA `0x2b181e` mapped to `0x1402b181e`; `call rax` at `0x1402b181c`
loaded `vkQueueSubmit`. This settled the observed 36.476 ms block as submit,
not the following `vkWaitForFences`, whose return RVA was `0x2b1885`.

## 6. Reconstruct GPU ownership and actual execution

Discover schemas from this trace before hardcoding IDs. In the successful
DxgKrnl schemas, join payload ownership in this order:

| Event | Join |
|---|---|
| Device 27 Start / 29 rundown | `hProcessId` equals current captured client PID; retain adapter and `hDevice` |
| Context 30 Start / 32 rundown | `hDevice` to owned device, yielding `hContext` |
| HwQueue 422 Start / 424 rundown | `hContext` to owned context, retaining each hardware queue |
| AssociateDxgSchedulerObject 433 | `pDxgObject` to context, retaining OS and KMD handles separately |
| SchedulingLog 432 | decoded context matched association `hOsHandle` in this capture, not distinct `hKmdHandle` |

Rundown emitter PID is not necessarily owner PID. Verify each equality in the
payload; normalize numeric hex identities without collapsing distinct handles.
When recording begins before client launch, ownership can appear only in Start
events; a rundown-only decoder can falsely find no client contexts.
Keep presentation contexts separate from rendering contexts and queue roots.
QueuePacket 178/180, DmaPacket 175/176/177, HAGS DMA 450/451, waits/signals and
present/flip events supply queued/completed/synchronization chronology. A queued
packet or a short CPU submit is not proof of GPU execution. See Microsoft's
[hardware queue description](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/gpu-hardware-queue).

Assign queue roles by joining native submission marker brackets to QueuePacket
events on the actual submitting TID; retain all candidates and require a unique
match before naming a packet. Join completion by the observed context/sequence/
packet identity, or hardware queue/progress fence for 450/451. Use this adapter's
NodeMetadata to identify 3D/COPY nodes: a Vulkan transfer-family index does not
identify a WDDM copy node. The first-batch asynchronous uploads actually ran on a
3D node. Queued-to-complete latency includes preceding work and synchronization;
even a completion near CPU readiness does not isolate that upload's GPU cost.

For HAGS SchedulingLog payloads, the working decoder checked hex byte length
against `SchedulingLogSize` and divisibility by 32. Each little-endian entry used
GPU timestamp UInt64 at offset 0, operation UInt32 at 8, context UInt64 at 16,
and state UInt32 at 24. Only operation 0 (context state change) occurred here;
do not decode other operations as that union member. For each owning log:

```text
QPC = CpuTimestamp0 + (GPU - GpuTimestamp0)
                      * (CpuTimestamp1 - CpuTimestamp0)
                      / (GpuTimestamp1 - GpuTimestamp0)
```

Use decimal arithmetic, reject equal GPU calibration endpoints, retain original
GPU/QPC pairs and converted values, and clip decoded entry times to measurement.
Log emission time is not execution time; entries can predate capture. Apply
calibration per log, not one global guessed ratio. Sort per context/node and
build RUNNING/IDLE intervals; retain unknown/other states and boundary gaps.
Check state alternation and compute interval unions for one owned context.
Bound coverage by observed transitions: do not extend a final IDLE through the
unobserved tail. Report that tail as UNKNOWN even when no later upload markers
exist. Keep repeated adjacent states visible, and report missing copy-context
coverage rather than inferring copy-engine idleness from unrelated contexts.
Never sum mirrored queue roots or unrelated/nested scopes. No precision bound
for the driver's calibration was established.

The [entry layout](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgk_scheduling_log_entry),
[context state change](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgk_scheduling_log_context_state_change)
and [state meanings](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ne-d3dkmddi-_dxgk_scheduling_log_context_state)
define RUNNING as executing and IDLE as no pending work on that context.
These states do not name a Vulkan command buffer, resource CRC, or frame
generation. Correlate CPU waits, owned execution, queued progress and next
present independently. If the driver/profile lacks decodable scheduling data,
return GPU attribution unresolved rather than inventing a reconstruction.

## Optional separate experiment: PCIe counters

Run this as a separate diagnostic with its own controls and artifacts.
[`nvidia-smi dmon`](https://docs.nvidia.com/deploy/nvidia-smi/index.html#device-monitoring)
is Linux-only; on Windows bind the shipped `nvml.dll` directly. The
[NVIDIA header](https://raw.githubusercontent.com/NVIDIA/nvidia-settings/main/src/nvml.h)
defines `NVML_PCIE_UTIL_TX_BYTES=0`, `NVML_PCIE_UTIL_RX_BYTES=1`.
Use exact export names/Cdecl; handles are pointers, status/enums are int,
values are unsigned int through output pointers. A minimal sequence is:

```text
nvmlInit_v2(); check status
try, after successful init:
  nvmlDeviceGetHandleByIndex_v2(index, &device); check status
  retain device identity, QPC frequency, QPC-before / UTC / QPC-after anchor
  until bounded deadline, sequentially bracket each native query with QPC:
    nvmlDeviceGetPcieThroughput(device, RX=1 then TX=0, &value)
    nvmlDeviceGetCurrPcieLinkGeneration(device, &value); GetCurrPcieLinkWidth
    nvmlDeviceGetMaxPcieLinkGeneration(device, &value); GetMaxPcieLinkWidth
    save each query's name, before/after, status, value or blank, nvmlErrorString
    sleep only the remaining requested interval; never issue catch-up bursts
finally: nvmlShutdown(); retain init/handle/query/shutdown codes and process exit
```

The width exports above have the same `nvmlDevice` prefix and `(device, &value)`
arguments. Check handle acquisition before querying; preserve failures as missing
readings. Query current generation/width under load, alongside maximum capability.
[NVML](https://docs.nvidia.com/deploy/nvml-api/latest/api/group__nvmlDeviceQueries.html)
reports KB/s over a 20 ms driver window; the KB base and precise internal window
endpoints are unspecified. RX/TX are serial observations, not a joint sample.

The measured run requested 100 ms but achieved about 110 ms spacing; each RX/TX
call took about 31 ms median. This is intrusive, with unquantified interference,
and was not a matched A/B test. Retain per-query latency, actual spacing and
missing windows; a deadline checked between samples cannot interrupt a native
call. Buffer output rather than printing every sample. Sparse GPU-wide samples
cannot attribute game traffic, integrate unseen transfers, or establish bus
saturation or its absence. Successful measurement also does not establish runtime
health, a universal stutter cause, a driver-reset cause, or fix performance.

If sampling runs in a separate helper process, retain its actual numeric exit
code in a terminal receipt before the launching shell loses the process handle,
along with PID/start identity and stdout/stderr. Normal metadata, successful
NVML shutdown and later process absence do not establish a process exit code.
Select phase samples by each query's own bounds; counts can differ by direction.
A call wholly after adoption can still reflect a driver window crossing that
boundary, so retain that uncertainty when comparing active and settled samples.

## 7. Preserve the evidence and state the limits

Keep original ETL, raw XML, tool versions/help/profile, trace stats and warnings,
all receipts/inventories, current PID/TID/process-start identities, both matching
EXE/PDB pairs, source hashes/revisions, settings, full logs, phase anchors and
focus checks. Hash retained artifacts with `Get-FileHash -Algorithm SHA256`.
Compact outputs should retain exact gap endpoints, per-thread state segments,
stack associations/RVAs/source lines, per-family schema counts/examples, GPU
handle maps, calibration pairs, context intervals, and nearby queue/present/flip
chronology. Derived summaries never replace the raw trace.

Accept a causal explanation only when independently timed evidence agrees.
Do not claim every hitch has one cause, resource upload overlap proves a
dependency, a CPU wait measures GPU cost, or app intervals prove physical
scanout. If expensive GPU workload identity remains unknown, the missing next
measurement is a reliable context/queue/progress-to-Vulkan-workload map or a
calibrated workload capture, not another identical log-only repeat. Evaluate a
fix with comparable runtime controls and independent wait/cadence evidence;
building it does not prove improved pacing.

Apply health checks to their intended lifecycle window. A validator that treats
every Graphics::Destroy as recovery can reject ordinary final shutdown; retain
the failed result and inspect ordering instead of reporting it as a health pass.
Keep native fault/debug-break notes separate from harness release success and
bounded OS event-query results.

## Matched UI experiments

Use fresh processes/atlases, identical language/scale seeds, admitted scene and
camera controls, cache seeds, warmup and input schedule on both sides. If exact
world equality matters, existing save/load and pause controls can prepare a
fixture; verify actual loaded state and ownership before timing. A successful
load envelope alone can conceal a reset-to-fresh fallback. Do not let one side's
runtime caches seed the other. Retained binaries still require a supported,
receipt-bound harness launch; copying EXEs into current Output is not a launch
override.

Record diagnostic asymmetry and actual texture events: old sampled upload stacks
do not establish WantCreate versus WantUpdates, update rectangles or exact glyphs.
On the instrumented side, retain status, rectangle, image/descriptor identity,
generation and event bounds. Match event-associated waits and present gaps,
rather than average FPS or a worst interval from an unmatched historical scene.
Restart for independent fresh-glyph repetitions; typing cached glyphs again is
not a repeat of atlas growth. Keep forced-growth triggers disabled equally and
screenshots outside measurement.

The later matched-after attempt captured two real WantUpdates but its old
initial/return cell was rejected. Its valid trace and readable UI therefore
leave the ImGui matched performance criterion (A9) unresolved. Proposed fixture
and input procedures are preparation, not evidence of a successful comparison.

## Worked example: the ImGui queue stall, 2026-10-19

The elevated GPUView.Light capture succeeded: a 30.0194908-second runtime driver,
2159 offline PresentMon rows (2138 in the measured window), zero event/buffer
loss, PerfCounter at 10 MHz, and two exact raw DXGI/PresentMon QPC matches. The
ETL covered a slightly wider interval than the runtime driver. Those checks
settled clock alignment and coverage before causal analysis.

The worst application interval was 29.4364 ms. Main ran for 0.5855 ms, was ready
for 0.2206 ms, and waited for 28.6303 ms. Four clipped waits totaling 28.3002 ms
(96.14% of the interval) had matched switch stacks resolving through the retained
EXE/PDB to `ImGui_ImplVulkan_UpdateTexture:965`, at `vkQueueWaitIdle(v->Queue)`.
The chain continued through RenderDrawData, ImGuiManager::Submit and the main
render path. Independently, the owned node-0 client GPU context was RUNNING for
27.584 ms, with earlier queued work completing near the gap's end. The main
producer was draining a busy graphics queue in the ImGui texture update path;
sustained CPU execution or runnable scheduling delay did not explain this gap.

The source branch handled both WantCreate and WantUpdates. Font-atlas activity
was likely from source evidence, but texture/status/update rectangle and actual
glyph were not recorded. Neither a newly encountered glyph nor all hitches
sharing this cause was proved. Other resolved gaps included one-shot upload
fence waits. App present/flip chronology did not measure physical panel scanout.
A production change was implemented, built and statically reviewed during that
session; runtime performance verification was still pending when this account
was written, so this is a successful diagnosis, not a claim of a verified fix.

Optional historical evidence was in ignored `Temp/NvidiaFramePacing/` files
`etw-cpu-diagnosis.md`, `etw-gpu-diagnosis.md` and `etw-run2-handoff.md`, with the
original trace and retained binaries beside them. These are provenance only;
future sessions must produce fresh artifacts using the contracts above.
