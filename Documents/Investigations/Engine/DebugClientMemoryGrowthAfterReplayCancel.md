# Debug Client Memory Growth After Replay Cancel

In one harness run, the Debug BrokenEngineSandbox client stopped responding with about twice its expected committed memory. That run did not show whether the client's growth caused the system to run out of commit or followed it. This record keeps the observed facts and the next check. It is an Investigation because the cause is unproven, so no fix can be decision-complete yet.

## Observation

- Build: Debug BrokenEngineSandbox client and server at primary `5f205af6a24a17ebf732f7005f03a48e0e6f91e0`, plus a server-only session change. The client C++ was identical to primary.
- Scenario, driven through `/agent-harness`, in order:
  1. Launch the server and one client.
  2. Click `[+F]##Fighter`.
  3. Record about 600 ticks, including a spawn.
  4. Play the recording back as a looping replay.
  5. Cancel the replay.
  6. Play about 80 s more, including the death and respawn of a fleet Fighter.
- About 80 s after the replay cancel, the client stopped responding: 0 CPU, with Windows reporting it as not responding.
  - Client private bytes were about 21.4 GB. The server's were about 10.3 GB, and each process was expected near 10 GiB.
  - Each Debug game process commits a 10 GiB mimalloc arena up front (`kiMimallocArenaReserveMebibytes`, `Engine/Source/Memory/GlobalAllocator.cpp:230`). mimalloc can add further arenas on demand (`Engine/Source/Memory/GlobalAllocator.cpp:260`).
- System commit was 68.7 of 75.9 GB. Total commit minus the client and server equaled the pre-launch baseline, so the excess of about 11 GB belonged to the client.
- Within the same second, the client's main loop stalled for about 71 ticks and dwm.exe crashed with 0xc00001ad (memory exhaustion). The server later dropped the client on an ENet timeout. The client wrote no crash report.

## Ruled out

- The session's server change: it preserves behavior, and the client binary's source was unchanged from primary.
- The harness working-directory change: every site that depends on the working directory is inert in a normal run:
  - the crash-report fallback paths (`Engine/Source/CrashReport.cpp:15-16`);
  - the PATH-relative `renderdoc.dll` load (`Engine/Source/Graphics/Managers/InstanceManager.cpp:235`), reached only with `--renderdoc`;
  - StackWalker's `.` symbol path, used only during crash handling.
- Primary's replay change: `Engine/Source/File/Replay.cpp` is entirely `BT_SERVER` code and is not in the client.

## Open questions

- Did the client's growth cause the commit exhaustion, or did the client grow after the exhaustion started?
- If the client grew first, which allocation grew: an extra on-demand mimalloc arena, or memory outside mimalloc?
- There is no valid control. An earlier run of the same scenario at `db9cbe5` quit right after the replay cancel, so it never reached the point where this run hung.

## Next check

1. With at least about 22 GB of commit free, run the same `/agent-harness` scenario.
2. Let it play at least 3 minutes past the replay cancel, through a Fighter death and respawn.
3. Sample client private bytes and system commit every 2 s.

Pass: client private bytes stay near 10.5 GB, and no dwm.exe 0xc00001ad event occurs.

If the client hangs, capture a process dump before the harness release. The `/agent-harness` skill does not document a dump step, so the investigator chooses the capture tool. The observing run took no memory samples before the hang, so if the growth reproduces, compare samples from the recording, the replay, the period after the cancel, and the time after the respawn to find where it starts.

## Provenance

- Client: `claude`. Worktree/branch UUID: `7506ab5a-9e57-43e7-b136-6f8500a09e36`. Branch: `claude/7506ab5a-9e57-43e7-b136-6f8500a09e36`. Conversation session ID: `c9339f81-d2e5-414f-9900-93426279e210`.
- Found during the `/next-plan` run that completed the Game Plan CarryFleetStateInSpawnQueue. That Plan file is deleted when it completes.
