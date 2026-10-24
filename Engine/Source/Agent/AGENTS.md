# Agent - Harness Transport and Client Automation

## Overview

Engine side of the agent harness: the loopback TCP JSON command channel and the engine-generic commands, input, UI snapshots, and harness rig state it serves. Game-specific command semantics live in the game project's own `Source/Agent/AGENTS.md`.

The harness exists so agents can drive the running client and server the way a
player or developer would, to verify a change live. Agents are the harness's
users: harness commands and their entry kinds, options, and result fields are
building blocks agents combine to set up and observe a live test of a change. A
building block is valuable even when no doc, recipe, or Plan references it, so a
missing reference is never grounds to remove or reject one. The only purpose the
harness bans is an automated unit-test-style run: never add a harness command
whose purpose is to execute a scripted pass/fail scenario itself.

## Architecture

- `AgentCommandServer` is dormant unless the agent build flag and `--agent-port` enable it. A background `jthread` owns socket I/O; `Drain()` executes at defined client/server main-thread points and dispatches to `game::ExecuteAgentCommand`.
- `ExecuteSharedAgentCommand` owns build-agnostic process lifecycle and log
  commands; the game dispatcher tries it before side-specific handlers. It stays
  engine-generic: game state such as the current tick (-1 before game creation)
  arrives as a plain `int64_t`, never through game globals. Focused Commands
  modules own the `crash_report_harness_rig` and the read-only
  `cell_coordinate_probe`, and `AgentCommandsShared.h` defines the one agent
  position shape and the checked grid-coordinate and float readers that engine
  and game commands share; the harness command reference
  (`../../../.agents/skills/agent-harness/references/command-reference.md`)
  defines their schemas.
- The game dispatcher tries the shared handlers first, then its own side
  handler, then the engine side-generic handler (`ExecuteClientAgentCommand`
  or `ExecuteServerAgentCommand`). A game side handler that handles an engine
  command name takes that command over: it validates its own parameters, calls
  the exported engine command function — `CommandDescribeScene`
  (`Commands/DescribeScene.h`, whole-file `BT_CLIENT`, outside `Engine.h`),
  `CommandStatus`, `CommandLoad`, or `CommandReplayTransferCapture` — and then
  writes only its own fields.
- `ExecuteClientAgentCommand` (`AgentCommandsClientGeneric.cpp`, whole-file
  `BT_CLIENT`) owns the client-generic handlers, `send_raw_packet`,
  `set_client_grid_coord`, and `release_client_grid_coord`, and dispatches the
  focused `audio_streaming_harness_rig`, `desync_probe`, and `describe_scene`
  modules; the client commands of the harness command reference
  (`../../../.agents/skills/agent-harness/references/command-reference.md`)
  define their schemas. `set_client_grid_coord` requires a connected session
  whose owned entity is already assigned (`## Game contract`), because before
  assignment the client subscription policy falls back to origin and discards
  the requested cell. The handlers read live game and profile state directly through
  `game::gpGame` and `gpProfileManager`, and a deferred UI script reads
  `game::gpGame` when the script completes. `query_profile` reads existing
  main-thread telemetry without forcing query reads. A focused module owns the
  read-only `presentation_continuity_probe`, which reports the
  render-published presentation-continuity snapshot
  (`../Graphics/Render/AGENTS.md`).
- `ExecuteServerAgentCommand` (`AgentCommandsServerGeneric.cpp`, whole-file
  `BT_SERVER`) owns `status`, `pause`, `timescale`, `save`, `load`, `reset`, the
  server `query_profile`, `create_cell`, and the five replay commands; the
  server commands of the harness command reference define their schemas. It exports
  `RefuseDuringReplay`, which `create_cell` and the game `edit_frame` share.
- `AudioStreamingHarnessRig` owns the client Debug audio/File harness rig state,
  snapshots, controls, and subsystem hooks; `AgentCommandServer` owns its
  instance, while `Commands/AgentCommandsAudioStreaming.cpp` owns the command's
  validation, action dispatch, and deferred replies.
  Loader-thread hooks discover it through a separately published atomic
  attachment rather than through the command-server global. Teardown clears the
  deferred response while captured dependencies are live, releases the read hold
  and the loader stall, drains accepted loader jobs, and detaches under the
  loader queue lock before destroying the harness rig, so neither an active job
  nor the idle-loader lookup can retain it across destruction.
- `ReplayHarnessRigs` owns one server slot bound to live `Replay` for controls and replay observations. The slot keeps a per-event list of the recording's captured transfer events and never drops a captured event. It carries replay-timed transfer counts opaquely and invokes the classifier the game provides (`## Game contract`). The [File Replay contract](../File/AGENTS.md#replay-streams) owns production replay state and storage.
- Construction binds on the startup thread with `SO_REUSEADDR` and retries a
  `WSAEADDRINUSE` bind (port still in TIME_WAIT after a rapid relaunch) for up
  to ~2.5 s before throwing `std::runtime_error`. Only the constructor call has
  the fatal agent-startup handler, and it rethrows `std::system_error`; after a
  handled listener failure, startup drains the eager asset future while its
  managers are still alive and returns nonzero. The same socket setting also
  lets a duplicate launch on the same port bind silently instead of failing
  fast; the harness's quit-and-wait-for-exact-PID relaunch rule is the guard
  against that misuse, not a bind-time check.
- Keep listener accept nonblocking: the listener thread retries accept between
  50 ms condition-variable waits, so it observes a stop request without another
  thread closing its socket. The listener thread owns both sockets: it closes
  the accepted socket after I/O exits and the listen socket when its loop ends;
  teardown never touches either socket; it requests stop under the transport
  lock and wakes the condition variable. Keep active-connection readiness,
  receive, and send outside the transport lock. The accepted socket remains
  nonblocking, and receive waits check stop before and after each 50 ms
  readiness wait. The response wait also peeks the socket every 50 ms, so a peer
  that closes mid-wait releases the connection. The listener flushes a published
  response despite stop under a single 3 s deadline.
- Only one request is in flight. Deferred handlers return a poll function; later drains publish its first result under the original request ID. Each request carries its connection generation (the reuse counter); a reply, synchronous or deferred, whose connection has since torn down is discarded rather than sent to a later client, and the drain-count timeout prevents a lost async result from blocking the channel indefinitely.
- The command channel is an exempt developer tool, so handlers need not range-check arguments; a check that exists throws into the failure envelope, which `Drain()` produces from the exception.
- `AgentInput` is client-only and runs one frame-stepped synthetic script at a time. It queues ImGui item activation, typed text, and keys, adds wheel notches to the published `RawInput` scroll value, and overlays that snapshot's keys for engine key bindings. Physical input suppression is owned by the startup/input path.
- ImGui item activation is the only way the harness drives the UI: an agent
  lists items, offscreen ones included, with `describe_ui`, and `click` or
  `set_slider` activates one by the ImGui ID behind its label, the way ImGui's
  own navigation does. Label naming is owned by
  [Ui/Screens/AGENTS.md](../Ui/Screens/AGENTS.md) `## Shared Authoring Rules`.
  With the agent layer live, `ImGuiManager` turns off ImGui's per-item clipping
  so offscreen items stay listed and activatable
  (`../../../.agents/skills/agent-harness/references/command-reference.md`
  `### Client command behavior`). Clicks on the 3D world area, such as picking
  an individual ship, may be allowed in future; programmatic picking through a
  command would still be preferred then.
- `AgentUiRegistry` is client-only and double-buffers fixed-capacity ImGui window/item snapshots, including optional current numeric text from menu controls created with `MenuUtils::WrapperSlider` and `MenuUtils::WrapperPlusMinus` and checked state supplied by `MenuUtils::RadioRow`. Publish after `ImGui::Render()`; label resolution and `describe_ui` read only the completed buffer.

## Constraints

- Keep steady-state input and registry paths allocation-free.
- Client implementations remain whole-file `BT_CLIENT`; server hook stubs keep shared ImGui integration linkable.
- Do not execute game mutations or Vulkan/UI work on the socket thread.

## Game contract

The engine declares no agent hook: the files here reach game state only through
these game-provided names (`../AGENTS.md` `## Hub Conventions`), and a game
project that provides them gets every engine command.

- Globals: `game::gpGame`, for `GameBase` members and `Game::HasFocusedClientEntity`
  (the owned-entity check of `set_client_grid_coord`); `game::gpClientSession`,
  for the `ClientSessionBase` members `mpRuntime` and `mpDesynchronizationCore`;
  `game::gpServerSession`, for the `ServerSessionBase` members
  `mpTransferManager` and `StepTimescale`.
- Functions: `game::ExecuteAgentCommand`, which `AgentCommandServer::Drain`
  calls, and `game::CountCapturedReplayTransfers`, the classifier
  `ReplayHarnessRigs` invokes.
- Types: `game::Frame` (`Cell::TailFrame` and the desync probe's snapshot),
  `game::FrameInterpolate` and `game::FramePostRender` (`FrameValueColumns`),
  `game::StatusChange` (`ReplayHarnessRigs::ObserveAcceptedTransfers`), and
  `game::ReplayTransferCaptureCounts`, carried opaquely per captured event.
- Compile-time symbols: `game::kCpuTimerPostRenderUpdateNavigationQuery`, the
  server `query_profile` raw timer row.
- Game headers: `Game.h`, `Agent/AgentCommands.h`,
  `Agent/Commands/ServerSimulationHarnessRigs.h` (from `ReplayHarnessRigs.h`),
  `Frame/Frame.h`, `Network/Client/ClientSession.h`,
  `Network/Server/ServerSession.h`, and `Profile/ProfileRows.h`.
- `Commands/FrameEdit.h` (whole-file `BT_SERVER`) owns the `edit_frame` and
  `read_frame` column machinery and the Explosions and Pushers column lists. It
  includes the game `Frame/Frame.h` for `FrameValueColumns`, so it stays out of
  `Engine.h` and only the game frame-edit commands include it; the game keeps its
  own collections' column lists and `FrameCollection` specializations.

## See Also

- Engine Input (`../Input/AGENTS.md`)
- Agent Harness (`../../../Tools/AgentHarness/AGENTS.md`)
