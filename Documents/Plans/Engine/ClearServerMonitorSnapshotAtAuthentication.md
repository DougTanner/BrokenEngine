<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:33:18.188Z","dependsOn":[]} -->
# Empty the Server Monitor snapshot slot when a monitor authenticates

## Context

Found by an architecture review of `Engine/Source/Server` (threading lens)
and verified by code reading at commit `64ec627b`. Pre-existing debt, minor:
one stale frame per reconnect; its wall-clock duration was not measured.

A monitor that reconnects is first sent a snapshot captured for the earlier
connection:

- `ServerMonitorHost::Disconnect`
  (`Engine/Source/Server/ServerMonitorHost.cpp:200-218`) resets
  `mpSentSnapshot` (`:217`) and clears `mbAuthenticated` (`:212`) but never
  empties `mpLatestSnapshot`.
- The main thread writes that slot only at `:87`, behind the
  `mbAuthenticated` check at `:79`. Only the worker sets (`:386`) and clears
  (`:212`) the flag.
- When the next connection authenticates (`:386`), the challenge frame has
  normally already been flushed (`:409-414`), so `mSendBytes` is empty. The
  next loop iteration (`:277-287`) finds the old slot non-null and different
  from the reset `mpSentSnapshot`, and serializes and sends it, before the
  main thread's next capture, which is up to eight `Update` calls away
  (`:74`, called from `Engine/Source/Main.cpp:497`).
- The monitor tool replaces its model on each snapshot frame
  (`Tools/ServerMonitor/ServerMonitor.cpp:127`), so it shows the old data
  until the next snapshot arrives.

`Engine/Source/Server/AGENTS.md` `## Ownership and Cadence` does not say the
slot survives a connection.

Unmet criterion: a newly authenticated monitor never receives a capture
taken for an earlier connection.

## Design

Recommended: in `ServerMonitorHost::Run`, just before the
`mbAuthenticated.store(true, ...)` at `:386`, the worker swaps
`mpLatestSnapshot` into a local `std::shared_ptr` under `mSnapshotMutex` and
lets the local release the old snapshot after the lock is dropped.
Rationale:

- It keeps both slot rules in `Engine/Source/Server/AGENTS.md`: the mutex
  covers pointer exchange only, and replaced-snapshot destruction stays
  outside it.
- Clearing at authentication is tighter than clearing in `Disconnect`. A
  main-thread `Update` that passed the check at `:79` before `Disconnect`
  ran can still publish at `:87` afterwards, refilling a slot `Disconnect`
  had just emptied; by the time a later connection has been accepted and
  has answered its challenge, that publish has long finished.

Update `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`: the
snapshot-slot bullet gains that the worker empties the slot when a
connection authenticates, and the worker bullet's "reads only host-owned
state and immutable snapshots" stays accurate (the slot is host-owned).

## Critical files

- `Engine/Source/Server/ServerMonitorHost.cpp`
- `Engine/Source/Server/AGENTS.md`

## In scope

- `ServerMonitorHost::Run`: the authentication branch at `:379-388`, the
  slot swap before the `mbAuthenticated` store.
- `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`: the
  snapshot-slot bullet.

## Out of scope

- `ServerMonitorHost::Disconnect`, `Update`, `Capture`, and `Serialize`.
- The capture cadence and any capture-on-authentication trigger: the first
  fresh snapshot still arrives at the main thread's next capture.
- A connection counter, epoch, or any new member.
- `mbAuthenticated`'s memory ordering.
- `Common/ServerMonitorProtocol.h` and `Tools/ServerMonitor/`.

## Risk and invariants

Change Workflow Tier 3 — trigger: threading. The worker thread gains a
write to `mpLatestSnapshot`, state shared with the main thread, which until
now only the main thread wrote.

- Every access to `mpLatestSnapshot` stays under `mSnapshotMutex`, and the
  lock is held for the pointer exchange only.
- The released snapshot is immutable and owns its storage; the worker's
  lifetime allocation-tracking suppression in `Run` already covers its
  release.
- The worker still never touches live manager or game objects.
- No wire, determinism, serialization, or trust change.
- Accepted remainder: a capture that passed `:79` for the earlier connection
  and publishes only after the later connection authenticated would still
  be sent. That needs a whole accept, challenge, and response to complete
  inside one `Capture` call, and the data it sends is then current; the
  author recommends not guarding it.

## Acceptance criteria

The diff, read against the sequence in `## Context`, settles this Plan with
a clean server build:

- After the authentication store, the first loop iteration finds
  `mpLatestSnapshot` null unless the main thread has captured since.
- No statement runs under `mSnapshotMutex` other than the pointer exchange.

## Notes

- No other Plan edits `ServerMonitorHost::Run`.
