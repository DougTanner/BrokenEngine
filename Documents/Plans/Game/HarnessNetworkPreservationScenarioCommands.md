<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T21:57:13.313Z","dependsOn":[]} -->
# Fix: /agent-harness — five network preservation and replay scenarios have no command that produces them

## Context

An `/agent-harness` acceptance run for the network corruption-response change had
to verify that structurally valid — non-corrupt — network traffic keeps its
documented outcome. Five sub-criteria returned BLOCKED because no harness command
can produce the required situation, so the run could settle them only by reading
code. The emitting surfaces are `.agents/skills/agent-harness/SKILL.md` and the
command reference under `Projects/BrokenEngineSandbox/Documents/AgentHarness/`.

Observed symptoms, each with the commands that were actually run and what they
returned:

1. Malformed `.fullframes` (kind-5) replay record. No command produces one.
   `replay_inject_persistence_failure` accepts only the stages
   `invalidation|grid|coordinate_writer|metadata|inventory|final_manifest`, and
   `replay_drop_retained_end_frame` removes a retained terminal frame; neither
   writes a corrupt kind-5 artifact. The `Replay manifest v3 integrity matrix`
   section of `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md`
   documents the only available route as a stopped-server AppData byte edit plus a
   manual manifest SHA-256 repair, which is outside the command surface and was
   not run.
2. Out-of-range subscribe-accept slot. The guard at
   `Engine/Source/Network/Client/ClientReceive.cpp:508-517` fires only for a
   non-`kuiSubscribeRejectSlot` slot index at or beyond `mCoordSlots`. No command
   emits such an accept: `client_packet_fault_fixture` accepts only
   `engine_envelope|status_change|game_packet`, and the server builds only real
   slot indices or the `0xFF` reject sentinel
   (`Engine/Source/Network/Server/ServerReceive.cpp:375,383`). The adjacent
   rejection path was exercised instead — `set_client_grid_coord`
   `{"coord":[100,100]}` produced `Server::ClientSubscribe Rejected (not adjacent)`
   and `Client::ServerSubscribeAccept Rejected` — which does not reach the guard.
3. Stale or reordered server updates and client acks. No command delays, reorders,
   or replays an earlier update or ack. The only near route,
   `client_full_state_fixture exercise_gap`, needs a pending full state above the
   client tick; driving it through an accelerated timescale instead produced
   `ClientSession::ApplyReceivedUpdates Buffer full` resyncs and then
   `Client::TrackReceivedTick Too many missing frames, disconnecting Slot: 0
   Gap: 218` — the documented disconnect outcome — before a pending full state was
   observable.
4. Subscription cancel or ghost race. No command cancels a subscribe while its
   accept is still in flight. `set_client_grid_coord` coordinate toggles, both
   spaced out and back to back, completed normally and produced no ghost line.
5. Pre-handshake ack drop. Gate 4 at
   `Engine/Source/Network/Server/Server.cpp:254-259` returns silently with no log
   line and no counter, so the drop is not distinguishable at runtime from no
   packet having arrived at all. Zero `RecordContractViolation` lines on each
   fresh server is consistent with the documented no-count outcome but does not
   prove the gate was reached.

Effect: the outcomes documented in `Engine/Source/Network/Client/AGENTS.md`,
`Engine/Source/Network/Server/AGENTS.md`, and `Documents/Architecture/Network.md`
for these five cases can be confirmed only by code reading, so any future change
to those paths has no runtime regression signal for them.

Boundary: the observing change's approved scope bounded its harness work to the
two fault fixtures it added, `client_packet_fault_fixture` and
`engine_packet_fault_fixture`. These five scenarios were outside that boundary and
outside its `## In scope`, so the gap is recorded here rather than fixed there.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 18ad9ff4-cceb-4064-9a18-4535760702d4
- Worktree/branch UUID: a4f7f03b-ab09-4e05-83db-8f5d333daa92
- Session branch: claude/a4f7f03b-ab09-4e05-83db-8f5d333daa92
- Worktree: .claude\worktrees\BrokenEngine\a4f7f03b-ab09-4e05-83db-8f5d333daa92
- Landing ref: the session branch above, whose tip is that session's final commit
  and which survives exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, reviewed only
  when the returned commit is attributable to one session alone; never an
  aggregate or multi-session squash commit.
- Run any transcript review before /cleanup-worktrees removes the worktree
  recorded above.

## Design

The gap is fully described by the command surfaces named above and is verifiable
from the current tree, so the implementing session should root-cause from
`## Context` and those files. A transcript review is not expected to be needed; if
one is, run `/next-plan-review <landing ref>` in bounded friction mode in a new
session with the recorded client and conversation session ID.

The author's recommendation is to add exactly one observation route per blocked
scenario, each reusing the mechanism the neighbouring fixture already uses, and to
make no change to what any receive path computes or accepts:

1. `.fullframes` record — add one stage to the existing
   `replay_inject_persistence_failure` command (recommended name
   `fullframes_record`) that writes a malformed kind-5 record while recording, so
   the manifest, which is published last, still describes the file it actually
   wrote and the reader reaches record-level parsing. Recommended because it
   reuses a command, a parameter shape, and a staging point that already exist,
   instead of adding a command or reproducing the documented manual byte-edit and
   SHA-256 repair procedure inside the harness.
2. Out-of-range accept — add a server-side fixture command (recommended name
   `server_subscribe_accept_fixture`, parameter `{"slot":int}`) that sends one
   subscribe-accept with the caller's slot index to the single handshaken client,
   mirroring how `engine_packet_fault_fixture` requires exactly one handshaken
   client. `engine::Server::SendSubscribeAccept` is private
   (`Engine/Source/Network/Server/Server.h:231`), so this needs a narrow
   debug-guarded entry point on `engine::Server`; the implementing session decides
   its exact form and guard, matching whatever guard the neighbouring fixtures use.
3. Stale or reordered traffic — add a client-side fixture that re-delivers a
   previously received, still well-formed server update or client-ack packet after
   a caller-specified delay, using the same arm-then-deliver-from-the-main-loop
   mechanism `client_packet_fault_fixture` already uses. Recommended over a
   general packet-scheduling layer because a single replayed earlier packet is
   what the documented stale/reordered outcome is about, and a scheduler would be
   machinery for cases nobody has needed.
4. Cancel-ghost race — add a client-side fixture that cancels a subscription whose
   slot is still in the subscribing state, so the in-flight accept arrives after
   the cancel. `engine::Client::CancelSubscription` is private
   (`Engine/Source/Network/Client/Client.h:144`), so this needs the same kind of
   narrow debug-guarded entry point as item 2.
5. Pre-handshake ack — add one `kDebug` log line at the Gate 4 early return in
   `Engine/Source/Network/Server/Server.cpp:254-259`, naming the client and packet
   type, so the drop becomes runtime-distinguishable. The drop itself, the absence
   of a contract violation, and the absence of a counter must all stay exactly as
   they are; only a log line is added. Recommended over a query because the gate
   keeps no state a query could read, and adding state for it would change what the
   gate does.

Every new or extended command must be documented in the matching command reference
file in the same change, and each must fail with a clear message when its
precondition is absent, as the existing fixtures do.

If root-causing shows a fix lies outside the `## In scope` boundary below —
for example that item 3 cannot be done without changing how received packets are
buffered — surface it for re-planning instead of expanding scope.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` — client
  fixture handlers and the client command dispatcher; `client_packet_fault_fixture`
  (arming and main-loop delivery) and `client_full_state_fixture` are the patterns
  to mirror for items 3 and 4.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp` —
  `replay_inject_persistence_failure` (item 1), `engine_packet_fault_fixture` (the
  one-handshaken-client precondition to mirror for item 2), and the server command
  dispatcher.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.h` — shared command
  declarations and the armed-packet hand-off comment.
- `Engine/Source/Network/Server/Server.h:231` and
  `Engine/Source/Network/Server/Server.cpp:254-259` — the private
  `SendSubscribeAccept` needing a debug entry point, and the silent Gate 4 return
  needing the log line.
- `Engine/Source/Network/Client/Client.h:144` — the private `CancelSubscription`
  needing a debug entry point.
- `Engine/Source/Network/Client/ClientReceive.cpp:508-517` — the out-of-range
  accept guard item 2 must reach.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md`,
  `commands-server.md`, and `replay.md` — the command reference each addition must
  be documented in; `replay.md` also carries the manual byte-edit procedure item 1
  replaces for the kind-5 case.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` and
  `Engine/Source/Agent/AGENTS.md` — agent-command documentation kept true by the
  additions.

## In scope

- Root-cause investigation as `## Design` states.
- One new `replay_inject_persistence_failure` stage producing a malformed kind-5
  `.fullframes` record, in `AgentCommandsServer.cpp`.
- One new server fixture command emitting a subscribe-accept with a caller-supplied
  slot index, in `AgentCommandsServer.cpp`, plus the narrow debug-guarded
  `engine::Server` entry point it needs.
- One new client fixture command re-delivering an earlier well-formed server update
  or client ack after a delay, in `AgentCommandsClient.cpp`.
- One new client fixture command cancelling a subscription whose slot is still
  subscribing, in `AgentCommandsClient.cpp`, plus the narrow debug-guarded
  `engine::Client` entry point it needs.
- One `kDebug` log line at the pre-handshake Gate 4 early return in
  `Engine/Source/Network/Server/Server.cpp`.
- Dispatcher entries and declarations in `AgentCommands.h`, project membership for
  any new file through `/update-vcxproj`, and the matching documentation in
  `commands-client.md`, `commands-server.md`, `replay.md`, and the two agent
  `AGENTS.md` files.

## Out of scope

- The behavior of the network receive paths themselves: what
  `Client::ServerSubscribeAccept`, `Client::TrackReceivedTick`,
  `Server::Receive`, `ServerReceive`, or `ClientSession::ApplyReceivedUpdates`
  accept, reject, count, or drop, including the Gate 4 drop and its absence of a
  contract violation and counter.
- Wire format, message layout, protocol version, replay manifest v3 layout, and
  the manifest generation-digest scheme.
- Subscription lifetime, slot-epoch, and cancellation semantics.
- The landed network corruption-response change, and the two fault fixtures it
  added.
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository.

## Risk tier and invariants

Expected Change Workflow Tier 2 — scoped behavior of one tool surface, the agent
harness command set, at an existing trust boundary whose format and trust are
unchanged (root `AGENTS.md` risk trigger "Tier 2 — scoped behavior"). Two parts
warrant a reviewer's attention and may justify escalation to Tier 3: the new
debug-guarded entry points on `engine::Client` and `engine::Server` expose
previously private subscription operations, and the Gate 4 log line sits inside a
trust-boundary receive path.

Preserve these invariants:

- The pre-handshake drop keeps its current behavior exactly: silent drop, no
  contract violation, no counter. Only a log line is added.
- No new command runs in a non-debug build if the neighbouring fixtures do not.
- Every new command validates its parameters and preconditions and fails with a
  message, as the existing fixtures do.
- No transcript path or home path enters the repository.

## Coordination

- `Documents/Plans/Game/AgentCommandsServerFaultFixtureSplit.md` moves the two
  existing packet fault fixtures out of
  `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp` into a new
  `AgentCommandsServerFaultFixtures` pair. Neither Plan depends on the other
  landing first; whichever lands second places its server-side additions in the
  file layout that then exists and re-measures the file against the reduction
  threshold. That Plan carries no reciprocal note for this one, because it was
  authored before this Plan existed.

## Acceptance criteria

- Each of the five scenarios is reachable through documented commands alone, with
  no stopped-server byte editing and no manual manifest repair:
  a corrupt-abort on the malformed kind-5 record; the
  `Client::ServerSubscribeAccept Out-of-range, unsubscribing` line; the documented
  stale/reordered outcome from a replayed earlier update or ack; the cancelled-slot
  ghost path; and a log line proving the pre-handshake gate was reached.
- The pre-handshake gate still records no contract violation and no counter in the
  same run that produces its new log line.
- Client and server `Debug|x64` targets compile through `/compile`.
- Every added or changed command is documented in its command reference file, and
  `/validate-skill` passes wherever the root `AGENTS.md` Apply the triggered
  cleanup step triggers it.

## Notes

Duplicate search over `Documents/Plans/` for `fullframes`, `subscribe-accept`,
`reorder`, `cancel-ghost`, and `pre-handshake` found no Plan owning this gap.
`Documents/Plans/Engine/SubscriptionCancellationEpoch.md` and
`Documents/Plans/Engine/LoadResetGenerationBarrier.md` own the engine-side
subscription cancellation and generation behavior itself, not the harness
commands that would observe it; a session implementing either would benefit from
these commands but is not blocked by them, so no dependency edge is recorded.

Line numbers are from the network corruption-response head; re-locate before
implementing.
