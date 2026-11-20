# Radeon Driver Watchdog Crash

On 2026-11-19 Windows stopped responding and then rebooted from a blue screen
while several Claude Code sessions were working in session worktrees. The crash
dump blames the AMD Radeon kernel driver. This record keeps what was found, what
was changed afterwards, and how to compare a later crash against this one.

Each claim carries one of three proof labels. A section that states one label
for all its claims labels only the exceptions.

- **Proven**: the crash dump, the event log, a file timestamp, a transcript
  entry, or a setting read back from the machine shows it.
- **Inferred**: it follows from proven facts, but nothing records it directly.
- **Not tested**: no evidence either way.

All times are local (UTC-5).

## Machine

Proven, read from the machine after the reboot.

- AMD Ryzen 9 9950X3D (32 logical processors), ASRock X870E Taichi, BIOS 3.50
  dated 2025-09-17, 32 GB of memory.
- AMD Radeon RX 9070 XT, driver 32.0.32015.12012 dated 2026-10-27 (AMD Software
  26.10.2), driving the only display at 3840x2160 and 120 Hz.
- The processor's integrated AMD Radeon Graphics was enabled with no display
  attached, on a different driver version: 32.0.21045.5002 dated 2026-08-16.
- Windows 11 Home 26H2. The registry reports build 26300.9974; the dump reports
  kernel 10.0.26100.9974.
- Uptime at the crash was 6 days 8 hours. The dump's power record counts 7
  resumes from sleep.

## The crash

Proven from the dump `C:\Windows\Minidump\111926-16406-01.dmp` and the System
event log.

- Bug check `DPC_WATCHDOG_VIOLATION` (0x133) with arguments 0x1, 0x1E00,
  0xFFFFF807737C63C8, 0x0. Argument 1 set to 1 means the processors together
  spent too long at a priority level where no ordinary program code can run.
- `!analyze -v` reports:
  - `DPC_TIMEOUT_TYPE: DPC_QUEUE_EXECUTION_TIMEOUT_EXCEEDED`
  - `MODULE_NAME: amdkmdag`, `IMAGE_NAME: amdkmdag.sys`
  - `SYMBOL_NAME: amdkmdag+191e616`
  - `FAILURE_BUCKET_ID: 0x133_ISR_amdkmdag!unknown_function`
  - `FAILURE_ID_HASH: {ce335412-b260-6bc1-3af9-ea5584f041c4}`
  - `PROCESS_NAME: System`
- The interrupted thread was the idle thread of processor 1, inside
  `amdkmdag+0x191e616`. The return addresses above it are `amdkmdag+0x199295d`
  and `amdkmdag+0x19925fc`.
- The loaded `amdkmdag.sys` has image size 0x05667000 and checksum 0x055F79BE.
  Offsets into it compare only against a dump with the same two values.
- The bug check happened at 15:22:27.8 (the dump's debug session time). Windows
  booted again at 15:22:54.
- Inferred: the freeze began at about 15:20:28. The watchdog period, argument 2,
  is 7,680 clock ticks, which is 120 seconds at the default 15.625 ms tick. The
  last write to any session transcript is at 15:20:28 (proven), which agrees.
- Event 6008 says the previous shutdown was at 14:45:36. That is not the crash
  time. The System and Application logs hold no event between 14:45 and the
  reboot, although the machine was working until 15:20:28. Inferred: the logs
  were not written to disk in that window.
- Windows wrote a small memory dump (`CrashDumpEnabled` was 3). It holds one
  thread's stack and names no user program, so it cannot say what the driver
  was doing or for which process.

## What the sessions were doing

Proven from the session transcripts under `%USERPROFILE%\.claude\projects` and
from file timestamps in each session worktree's `Temp` folder. Worktrees are
named by the first eight characters of their folder name.

- `583d6a45`: a runner ran the Profile client and server twice. The second
  client exited at 15:18:29 (`Temp/client-59428-exit.json`).
- `eb09a76a`: a diagnosis worker launched ModelViewer four times, each run
  taking one screenshot and exiting, between 15:18:31 and 15:18:48
  (`Temp/ModelViewerCapture-20261119T2018*`).
- `21ac1d45`: a builder ran a Debug client build of BrokenEngineSandbox from
  15:19:35 to 15:20:13.
- `285f5cc6`, session `adf1d3fc-1e95-4b1a-86b9-999666d0017e`: a runner started
  a Release server at 15:19:57. No client was connected at 15:20:06. At
  15:20:16 it wrote `Temp/Capture-ServerMonitor.ps1`, the helper from
  `.agents/skills/agent-harness/references/server-monitor.md`.
- `2c24c0a5`, `dc240152`, and the other workers of `583d6a45` and `21ac1d45`:
  reading files and running `git grep`. Their last recorded calls are between
  15:20:07 and 15:20:11.
- Inferred: no client and no ModelViewer was running when the freeze began. No
  transcript records one, but each transcript lost its last seconds
  (`## What the crash destroyed`).

## Most likely trigger

- Inferred: the Server Monitor window capture in `285f5cc6`. After writing the
  helper, that recipe starts Server Monitor, raises its window, and captures it
  with `PrintWindow`. It was the only display-related work in the last seconds
  before the freeze.
- Not tested: whether the capture had started. The part of the runner's
  transcript after 15:20:11 was lost.
- Not tested: whether the two client and four ModelViewer launches in the
  minutes before, or driver state built up over the uptime, played a part.
- Inferred: whatever the trigger, the fault lies in the driver or in the
  hardware under it (`## Mitigations not yet applied`), not in what the
  sessions did. Starting a Vulkan program or capturing a window must not freeze
  the machine.

## What the crash destroyed

Proven.

- The end of every transcript being written was filled with zero bytes instead
  of text. The five files checked lost between 5,502 and 60,664 bytes each.
- `Temp/Capture-ServerMonitor.ps1` in `285f5cc6` is 15,759 zero bytes.
- Inferred: a session resumed after such a crash is missing its last seconds,
  and a file written in those seconds may be empty.

## Earlier driver timeouts on this machine

Proven from the dump file names under `C:\Windows\LiveKernelReports`. Their
contents were not examined. Not tested: what was running at those times.

- `WATCHDOG` dumps (live kernel event 0x141: the graphics driver did not answer
  in time and Windows reset it):
  - 2026-09-27 at 16:26.
  - 2026-10-17 at 16:03, 16:08, 16:13, 16:15, 16:25, 16:27, 16:57, 16:58,
    17:02, 17:40, 17:41, and 17:48.
  - 2026-10-28 at 17:46.
- Each 2026-10-17 dump from 16:13 onward has a matching `AMD_WATCHDOG` dump
  (event 0xA1000001) and `AMD_REPORT_UM` dump (event 0xA2000002).
- Inferred: more than one driver version is involved, because the September and
  2026-10-17 timeouts are older than the current driver's date. The date the
  current driver was installed was not checked.
- The System log holds no other bug check in the 90 days before the crash.
- Windows Error Reporting logged all of these again between 15:23 and 15:26 on
  2026-11-19, when it went through its queue after the reboot. The time of
  those events is not the time of the incident; the dump file name is.

## Changes made after the crash

- Proven: `CrashDumpEnabled` under
  `HKLM\SYSTEM\CurrentControlSet\Control\CrashControl` was changed from 3
  (small memory dump) to 2 (kernel memory dump) and read back.
- Inferred: the setting takes effect at the next restart, and the next crash
  then writes `C:\Windows\MEMORY.DMP`, which holds every processor's kernel
  state.
- Proven: the AMD Bug Report Tool form was filled in with the findings above.
  Not tested: whether it was sent; that was left to the user.
- Proven: no repository file was changed apart from this record and its
  evidence folder (`## Evidence kept`).

## Mitigations not yet applied

Not tested: none of these is known to prevent the crash.

- Disable the integrated graphics, in the BIOS or in Device Manager, so that
  only one AMD display driver version is loaded.
- Reinstall the Radeon driver cleanly, or move to a newer one.
- Rule out unstable hardware: update the BIOS, and run with stock memory and
  processor settings.
- Restart the machine after a long uptime with several resumes from sleep.

Changing the harness, such as spacing out ModelViewer launches or capturing the
Server Monitor window another way, is held back. It would be a guess until a
kernel memory dump shows the trigger.

## If it happens again

1. Read the bug check from the System log: event 1001 from
   `Microsoft-Windows-WER-SystemErrorReporting`. Compare its code and arguments
   with `## The crash`.
2. Copy the dump out of `C:\Windows` from an elevated shell; it is not readable
   otherwise. Analyze the copy with `cdb.exe`, which ships in the `amd64`
   folder of the WinDbg Store package:

   ```text
   cdb.exe -z <dump copy> -c "!analyze -v; q"
     -y "srv*<cache folder>*https://msdl.microsoft.com/download/symbols"
   ```

3. Compare `FAILURE_BUCKET_ID` with this record. Compare the `amdkmdag+`
   offsets only when the image size and checksum match.
4. With a kernel memory dump, also run `!running -ti` and `!dpcwatchdog`. They
   list every processor's running thread and its time in driver callbacks, and
   are expected to show which process the driver was serving. Not tested: the
   small dump of this crash could not run them.
5. Take the crash time from the dump's debug session time. Subtracting the
   watchdog period gives only an estimate of when the freeze began, because
   the watchdog adds up time that need not be one unbroken stretch; check it
   against the last transcript write. Do not use event 6008.
6. Find the sessions active at that time: sort the `*.jsonl` transcripts under
   `%USERPROFILE%\.claude\projects` by last write time, read each one's last
   recorded tool calls, and list each session worktree's `Temp` files by last
   write time. Expect zero-filled tails.
7. Add the new crash to this record as a dated section.

## Evidence kept

Proven. The folder `RadeonDriverWatchdogCrash`, beside this record, holds the
debugger's text output for this crash:

- `DebuggerAnalyze.txt`: the full `!analyze -v` output.
- `DebuggerModuleThreadPower.txt`: the driver image details (`lmvm amdkmdag`),
  the interrupted thread (`!thread`), and the dump's device, power, and logon
  records (`!blackboxpnp`, `!blackboxbsd`, `!blackboxwinlogon`).

Both are as the debugger wrote them, except that the user profile path is
replaced with `%USERPROFILE%` and trailing spaces are removed.

The dump itself is not in the repository: it is a 7 MB binary that holds kernel
memory of the machine, and this repository is public. It stays in
`C:\Windows\Minidump` until Windows removes it.
