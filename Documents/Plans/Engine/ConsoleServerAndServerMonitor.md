<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-25T13:47:43.488Z","dependsOn":[]} -->
# Run the server as a console program and move its monitor window into the Server Monitor tool

## Context

The dedicated server is a Windows-subsystem program. It creates a borderless window covering the work area and paints a GDI monitoring display into it: a statistics panel, a Map tab, and a Profile tab with a Copy button.

- `Engine/Source/Main.cpp`: window creation `:216-271`, show/focus `:346-368`, repaint from the server main-loop branch `:461-487`, server `WindowProcedure` cases `:714-768`.
- `Engine/Source/Server/ServerDisplay.{h,cpp}`: statistic aggregation, content hash, painting, click handling.

Painting costs server CPU, and the real (Azure) server should not pay for a UI nobody is looking at.

User decisions (this Plan's record of them, per `Documents/Plans/AGENTS.md` `## Authorship`):

- The server runs as a console program with no visible window. A window that is never shown, kept only to receive `WM_ENDSESSION` (logoff/shutdown final autosave), is acceptable.
- The monitor becomes a separate Windows program, the Server Monitor. It runs on the developer's PC and connects to a local test server or to the Azure server.
- The monitor connects on a second port protected by a shared secret, because the code is open source. The secret is never sent.
- One game-generic monitor. The server sends every label already joined to its value, the map data, and the Profile tab as ready-made text, so the monitor contains no game code. Single project `Tools/ServerMonitor/Platforms/VisualStudio2026/ServerMonitor.vcxproj` with its own single-project `.sln`; sources under `Tools/ServerMonitor/`; no base classes.
- Azure: include the monitor port now, admitted only from the user's IP (like RDP/SSH).

Facts that shape the design:

- `ServerUpdateDisplayStatistics` (`ServerDisplay.cpp:84-119`) runs every main-loop iteration and writes the explosion counter, the mimalloc statistics, and `UpdateProfileText()` (which smooths CPU timers). The server `query_profile` agent command (`Engine/Source/Agent/AgentCommandsServerGeneric.cpp:175-220`) reads them, so that per-tick part must keep running whether or not a monitor is connected.
- The server loads `user32.dll`. Microsoft's `SetConsoleCtrlHandler` documentation says such a process does not get `CTRL_LOGOFF_EVENT` / `CTRL_SHUTDOWN_EVENT` in a console handler and must use a hidden top-level window handling `WM_ENDSESSION`. The current final-autosave path (`Main.cpp:745-755`) depends on it.
- Windows ends the process when a console handler returns from `CTRL_CLOSE_EVENT`. The handler runs on a thread Windows creates.
- `gbQuit` is a plain `bool` (`Engine/Source/Agent/AgentCommandServer.h:96`), written at `Main.cpp:732,751,837,844` and `AgentCommandsShared.cpp:110`, read at `Main.cpp:391`. A console-handler write would be a data race.
- ENet is reachable only through `ThirdParty.$(Configuration).lib`; a Winsock TCP client needs only `ws2_32.lib`.
- Header-only precedent: `Common/Sha256Hasher.h` is compiled by path by the PCH-less tools and includes no header (`Common/AGENTS.md` `## Overview`). Code needed by more than one program belongs in Common (`.agents/references/cpp-conventions.md`).
- The game port `kiDefaultPort = 27'015` is UDP (`Engine/Source/Network/NetworkProtocol.h:85`); discovery is UDP `+1` (`:248`).
- The harness launches the server with `Start-Process -WindowStyle Hidden`, `--app-data-directory`, `--loopback-only` unless cross-machine, readiness by ping (`.agents/skills/agent-harness/scripts/Invoke-HarnessLaunch.ps1:136-172`). Nothing depends on a window.
- No Azure VM exists yet (`Documents/Plans/ChangeWorkflow/CreateAzureServerVm.md:7`); `/deploy-server` is create-only, so the template change takes effect when that Plan runs.

**One stage.** Every partial cut leaves either two displays (window plus monitor, duplicated formatting) or none.

## Design

Agent-made choices below are the authoring agent's recommendations, with rationale.

### D1 — Transport

Plain Winsock TCP on fixed port `kiServerMonitorPort = 27'017`; no launch option. The server serves it from its main thread with non-blocking calls and adds no thread (race-free by construction, matching the ENet main-thread rule in `Engine/Source/Network/AGENTS.md`).

Rationale: ordered, reliable stream for multi-KB snapshots; the engine-free monitor needs only `ws2_32.lib`; the server is single-instance per machine (`Main.cpp` mutex), so a fixed port cannot collide; 27017 continues the 27015/27016 block.

### D2 — Shared layout header

`Common/ServerMonitorProtocol.h`, namespace `common`, added to `Common.h`. The monitor compiles it by relative path (`#include "../../Common/ServerMonitorProtocol.h"`, the `CoordinationStore.cpp` precedent). Like `Sha256Hasher.h` it includes no header; the includer supplies `<windows.h>`, `<bcrypt.h>`, `<array>`, `<cstdint>`, `<span>`, `<string_view>`. Content (little-endian x64 both ends):

- Constants: `kiServerMonitorPort = 27'017`, `kuiServerMonitorMagic = 0x4E4D5442` ("BTMN"), `kuiServerMonitorProtocolVersion = 1`, `kiServerMonitorNonceSize = 32`, `kiServerMonitorResponseSize = 32`, `kuiServerMonitorMaximumFrameSize = 4 * 1'024 * 1'024`.
- `enum class ServerMonitorMessageType : uint32_t { kChallenge = 1, kSnapshot = 2 };`
- POD structs, each with a size `static_assert`:
  - `ServerMonitorFrameHeader { uint32_t uiSize; ServerMonitorMessageType eType; }` (8; `uiSize` = body bytes after the header)
  - `ServerMonitorChallenge { uint32_t uiMagic; uint32_t uiVersion; std::array<uint8_t, 32> nonce; }` (40)
  - `ServerMonitorSnapshotHeader { uint32_t uiStatisticsSectionCount; uint32_t uiProfileSectionCount; uint32_t uiCellCount; }` (12)
  - `ServerMonitorTextSection { uint32_t uiColor; uint32_t uiTextSize; }` (8; `uiColor` is a GDI `COLORREF`; `uiTextSize` text bytes follow)
  - `ServerMonitorCell { int32_t iX; int32_t iY; uint32_t uiClientCount; uint32_t uiFlags; int64_t iEntityCount; }` (24); flags `kuiServerMonitorCellActive = 1`, `kuiServerMonitorCellSubscribed = 2`
- Snapshot body order: snapshot header, statistics sections (header + text each), profile sections, cell records.
- `ServerMonitorSecretKey(std::string_view fileText) -> std::string_view`: trims `" \t\r\n"` both ends. Both ends must derive identical key bytes, so it lives once here.
- `ComputeServerMonitorResponse(std::string_view secretKey, std::span<const uint8_t, 32> nonce, std::array<uint8_t, 32>& rResponse) -> bool`:
  one `BCryptHash(BCRYPT_HMAC_SHA256_ALG_HANDLE, key, nonce, out)` call, returns
  `BCRYPT_SUCCESS` (the Windows 10 pseudo-handle needs no provider open/close).
  The server stores the result in a local and `ASSERT`s it; the monitor ignores
  it (a wrong response is simply rejected by the server). Neither call can
  realistically fail with fixed valid arguments, so no failure path is added.

### D3 — Handshake and server-side validation

One monitor connection at a time; states none → awaiting response → authenticated. `accept` only while none; listen backlog 1.

On accept: set non-blocking; generate a 32-byte nonce with `BCryptGenRandom(nullptr, …, BCRYPT_USE_SYSTEM_PREFERRED_RNG)` (result stored in a local and `ASSERT`ed `BCRYPT_SUCCESS`); queue the challenge frame; record the accept time.

The monitor's only record is exactly 32 raw response bytes. The server receives
at most the bytes still missing from those 32. When all 32 have arrived: with a
secret, compare them (`==` on the arrays) to `ComputeServerMonitorResponse`;
without a secret, accept any 32 bytes (the bind already limits to loopback). A
plain comparison suffices: each connection gets a fresh nonce and one attempt,
so a timing difference reveals nothing reusable. Close the connection
(`closesocket`; one attempt per connection) on: mismatch; 5 s without all 32
bytes; any byte received while authenticated; `recv` returning 0 or error; any
`send` error other than `WSAEWOULDBLOCK`. The server never reads a length from
the monitor, so no monitor value sizes an allocation.

Accepted limit: a silent peer holds the single pending slot for 5 s.

### D4 — Bind rule

The server reads `gpFileManager->mAppDataDirectory / L"ServerMonitorSecret.txt"`
through `common::GetFileOrStringContent` and `ServerMonitorSecretKey`
(precedent: `GitHubCrashReportToken.txt`, `Engine/Source/CrashReport.cpp:537-557`).
When the key is empty or `--loopback-only` is set, bind `127.0.0.1`; otherwise
`INADDR_ANY`. No `SO_REUSEADDR` (on Windows it would let another socket take the
port). A failed `socket`/`bind`/`listen`/`ioctlsocket` logs `kError` and throws
`std::runtime_error`, matching `Server::Server` (`Server.cpp:35-54`).

Rationale: binding enforces "loopback only" before any byte is read.

### D5 — Snapshot and cadence

`ServerMonitorHost::Update()` runs once per main-loop iteration:

1. The former `ServerUpdateDisplayStatistics` body, unchanged (including its `kbProfiling` early return and `UpdateProfileText()`), unconditional for `query_profile`.
2. Service the socket (accept, receive, flush pending send bytes).
3. Only while authenticated, every eighth call (~4 Hz at 32 Hz, the old repaint cadence), build a snapshot; skip while the previous frame still has unsent bytes.

Snapshot text reproduces what the window painted:

| Section | Color | Text |
| --- | --- | --- |
| Statistics 1 | RGB(100,180,255) | `"FPS: %lld\nPotential: %lld\n\n"` |
| Statistics 2 | RGB(200,200,200) | `"Tick: %lld\nTime: %.2f s\nActive cells: %lld\nClients: %lld\n"` (tick/time from the origin cell's `game::GetServerCellStatistics`) |
| Statistics 3 | RGB(150,220,150) | one `"%s: %lld\n"` per `game::kServerEntityCountNames` total, then `"Explosions: %lld\n"` |
| Statistics 4, `#if !defined(ENABLE_CRT_DEBUG_HEAP)` | RGB(220,180,100) | `"\nCommitted: %lld MiB\nPeak cmtd: %lld MiB\nHeap used: %lld MiB\nPeak heap: %lld MiB\n"` |
| Profile 1 | RGB(100,180,255) | `"FPS: %lld  Potential: %lld\n"` |
| Profile 2 | RGB(220,220,220) | `FormatCpuTimersText` output |
| Profile 3 | RGB(150,220,150) | `FormatCpuCountersText` output |
| Profile 4, same guard | RGB(220,180,100) | the four memory lines |

- Call `gpProfileManager->TickVisibilityCadence()` exactly once per snapshot and pass it to both formatters.
- One cell record for every coordinate in `game::gpGame->mActiveCoordinates`, in any client's `authorizedCoordinates`, or in any client slot with `SubscriptionFlags::kActive`. `uiClientCount` = authorized entries equal to it; Active flag when active; Subscribed flag when any client has an active slot there (the `FindSlotForCoordinate` rule); `iEntityCount` = sum of the cell's `entityCounts` plus `iExplosions` for active cells, else 0.
- `ASSERT` the frame fits `kuiServerMonitorMaximumFrameSize`.

`ServerDisplayContentChanged` and its hash are deleted; the monitor repaints once per received snapshot.

### D6 — The monitor program

`Tools/ServerMonitor/ServerMonitor.cpp`, one file, `wWinMain`, SubSystem Windows, no PCH, only standard/Windows headers plus `Common/ServerMonitorProtocol.h`.

- Startup: `SetProcessDPIAware()` (pixel geometry matches the old window and the
  capture recipe), `WSAStartup`. Optional first command-line argument is the
  server host, default `127.0.0.1`, resolved with `GetAddrInfoW` for IPv4 TCP on
  the fixed port. Read the secret once from
  `%APPDATA%\ServerMonitor\ServerMonitorSecret.txt`
  (`SHGetKnownFolderPath(FOLDERID_RoamingAppData)`, `std::ifstream`,
  `ServerMonitorSecretKey`); missing file → empty key. Window
  `WS_OVERLAPPEDWINDOW`, `CW_USEDEFAULT` size, title `Server Monitor - <host>`,
  `GetMessage` loop.
- Networking on the UI thread from a 50 ms `SetTimer` (single-threaded, race-free):
  1. Disconnected: every 2 s create a non-blocking socket and `connect`.
  2. Connecting: zero-timeout `select` for writable/failed.
  3. Awaiting challenge, then connected: `recv` all available into a buffer and
     parse each frame once its `uiSize` body bytes have all arrived.
     Server-to-client data is trusted (root `AGENTS.md` trust policy), so the
     monitor adds no frame-size cap or per-read bounds checks beyond the magic
     and version check. On a challenge with the right magic, send
     `ComputeServerMonitorResponse` (or 32 zero bytes with no key) and move to
     connected; wrong version → show
     `"Server protocol version N, monitor expects 1"`. Each snapshot replaces the
     model and invalidates the window.
  - Any close → disconnected, model cleared, status `"Disconnected from <host> - retrying"` painted where the statistics go.
- Painting moves from `ServerDisplay.cpp`: cached GDI objects, double-buffered
  paint, tab bar, profile panel with Copy button, clipboard copy, click
  handling. Layout constants unchanged (tab bar left 250, tab 100×32, Copy 80×28
  at margin 10, Consolas 28; line height 36 statistics / 32 profile, 4 px
  between profile sections), so the capture recipe's coordinates still hold.
  `PaintGridMap` becomes `PaintMap` over cell records: bounds from Active cells
  plus one cell padding; colors, borders, red subscribed inset, labels
  unchanged; `"(x,y)"` formatted by the monitor. Copy text = concatenated
  profile section texts.

### D7 — Console entry point

Keep `wWinMain`; set the server project's Link `SubSystem` to `Console` and `EntryPointSymbol` to `wWinMainCRTStartup` in all three configurations. The client is unchanged. Rationale: `Main.cpp` keeps one entry point for both targets.

### D8 — Shutdown

- `gbQuit` becomes `std::atomic<bool>`; writes `store(true, std::memory_order_relaxed)`, loop read `load(std::memory_order_relaxed)` (a flag that publishes no data).
- `Main.cpp` gains server-only file-static `std::atomic<bool> sbFinalAutosaveDone` and `ConsoleControlHandler`. For `CTRL_C_EVENT`, `CTRL_BREAK_EVENT`, `CTRL_CLOSE_EVENT`: set `gbQuit`; `sbFinalAutosaveDone.wait(false, std::memory_order_acquire)`; return `TRUE`. Other events return `FALSE`. It logs nothing (the thread has no `ThreadLocal`).
- `SetConsoleCtrlHandler(ConsoleControlHandler, TRUE)` just before `"Enter main loop"` (a Ctrl+C earlier ends the process with nothing to save).
- After the post-loop `FinalAutosave()`, the main thread does `store(true, std::memory_order_release)` then `notify_all()`.
- The server window is still registered and created (`WS_POPUP`, position/size 0) and never shown, so `WM_ENDSESSION` keeps its final-autosave path (user-approved).

Accepted limit: Windows force-ends a `CTRL_CLOSE_EVENT` after its own timeout (~5 s), so a longer final autosave is cut off on console close. Ctrl+C has no such limit.

### D9 — Console output

- `kbAlsoLogToPrintf` becomes `true` under `BT_SERVER`, stays `false` otherwise (`Engine/Source/EngineSwitches.h`). The console shows the same stream as `--log-file`. The host constructor logs `ServerMonitorHost listening on <address>:27017 (secret required|no ServerMonitorSecret.txt: loopback only)`.
- `Common/Log/Log.cpp` `LogWrite` (alive path): move the existing `ScopedSuppressAllocationTracking` up to cover `printf` as well as the file write.
- At the start of `ProcessMain` under `BT_SERVER`, when `GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), …)` succeeds, `SetConsoleMode` adds `ENABLE_EXTENDED_FLAGS` and removes `ENABLE_QUICK_EDIT_MODE`. Under conhost a click in the console starts a selection that blocks `printf` while `gLogMutex` is held, stalling every logging thread and the simulation.

### D10 — Placement of the old code

| Content | Destination |
| --- | --- |
| Per-tick aggregation, snapshot text/cell assembly, socket host | `Engine/Source/Server/ServerMonitorHost.{h,cpp}` (`BT_SERVER`-guarded; `.cpp` keeps the `Frame/ServerCellStats.h` and `Game.h` includes and file-static `sTotalEntityCounts`) |
| All GDI painting and input | `Tools/ServerMonitor/ServerMonitor.cpp` |
| `ServerDisplayContentChanged`, repaint throttle, heartbeat, `IsIconic`, `InvalidateRect` in `Main.cpp` | deleted |
| `ServerDisplay.{h,cpp}` | deleted |

`ServerMonitorHost` is a plain class owned by a `std::unique_ptr` local in `MainThread` (only `Main.cpp` uses it), constructed right after `pGame` in the server pre-window block, so it is destroyed before the game it reads and after `NetworkManager`'s `WSAStartup`.

### D11 — Documentation placement

- The monitor wire contract lives in `Engine/Source/Server/AGENTS.md` under a new `## Monitor Channel`.
- `Engine/Source/Network/AGENTS.md` `## Ownership` gets one routing bullet.
- Root trust-policy owner list gets the new section.
- `Documents/Architecture/Network.md` and `kiProtocolVersion` are unchanged (the ENet wire is untouched; the contract table covers ENet packets only).

### D12 — Azure exposure (admitted only from the user's IP)

- `.agents/skills/deploy-server/assets/server.bicep` NSG (`:220-266`): add rule `Admin-Monitor` after `Admin-SSH`: priority 130, Inbound, Allow, `Tcp`, source `'${adminSourceAddress}/32'`, source port `*`, destination `*`, destination port `'27017'`.
- `/deploy-server` texts and `CreateAzureServerVm.md` list the fourth rule; the re-admit operation updates all three admin rules.
- `Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md` (unlanded Plan) gains the VM-side pieces: a Windows Firewall rule for TCP 27017 and copying the secret file to the VM's server app-data folder (the local copy is kept, because the monitor on the PC reads it).

### D13 — Verification path

`/agent-harness` for the server (and a client); start the monitor by hand. The capture recipe becomes `.agents/skills/agent-harness/references/server-monitor.md`: the current recipe with the PID parameter renamed `-MonitorPid` and monitor launch/stop added. No harness command is added.

### D14 — Related Plans

- `Documents/Plans/Engine/ServerMonitorHexCellMap.md` draws hexes with engine geometry the engine-free monitor cannot compile. It gains a `dependsOn` edge on this Plan and a paragraph at the top of `## Context` saying its target moved and must be re-derived at claim.
- `Documents/Plans/Engine/HexagonalSimulationCells.md` only renames fields in `ServerDisplay.cpp`; its four path citations are retargeted. Either landing order works; if it landed first, carry `GridCoord`'s renamed fields into `ServerMonitorCell` unchanged.

## Critical files

- `Engine/Source/Main.cpp`; `Engine/Source/Server/ServerDisplay.{h,cpp}` (deleted); `Engine/Source/Server/ServerMonitorHost.{h,cpp}` (new)
- `Common/ServerMonitorProtocol.h` (new), `Common/Common.h`, `Common/Log/Log.cpp`; `Engine/Source/EngineSwitches.h`
- `Engine/Source/Agent/AgentCommandServer.h`, `Engine/Source/Agent/AgentCommandsShared.cpp`
- `Tools/ServerMonitor/ServerMonitor.cpp`, `Tools/ServerMonitor/AGENTS.md`, `Tools/ServerMonitor/Platforms/VisualStudio2026/ServerMonitor.{sln,vcxproj,vcxproj.filters}` (new)
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` + `.filters`; projects that need the new Common header per `/update-vcxproj`
- `.agents/skills/compile/{scripts/Invoke-CompileBuild.ps1,SKILL.md,references/worker.md}`; `.agents/scripts/Invoke-IncludeOrderCheck.ps1`; `.agents/skills/update-vcxproj/{scripts/Resolve-VcxprojMembership.ps1,SKILL.md,references/worker.md}`
- `.agents/skills/agent-harness/references/{server-window.md→server-monitor.md,command-reference.md,worker.md}`; `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`
- `.agents/skills/deploy-server/{assets/server.bicep,SKILL.md,references/azure-facts.md}`
- Docs: root `AGENTS.md`, `Common/AGENTS.md`, `Engine/Source/AGENTS.md`, `Engine/Source/Server/AGENTS.md`, `Engine/Source/Network/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`, `Documents/Architecture/FrameUpdatePipeline.md`
- Plans: `Documents/Plans/Engine/ServerMonitorHexCellMap.md`, `Documents/Plans/Engine/HexagonalSimulationCells.md`, `Documents/Plans/ChangeWorkflow/CreateAzureServerVm.md`, `Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md`

## In scope

**`Engine/Source/Main.cpp`**
- Replace the `Server/ServerDisplay.h` include with `Server/ServerMonitorHost.h`.
- Construct `pServerMonitorHost` after `pGame` in the server block (`:195-214`), keeping the `:199` comment.
- Server `CreateWindow` branch (`:243-254`): `WS_POPUP` at 0,0,0,0 with a comment that the window is never shown and exists only for `WM_ENDSESSION`; remove `SystemParametersInfo` and the server rectangle.
- Delete the server `ShowWindow` and `SetFocus` branches (`:346-368`).
- Replace the server main-loop block (`:461-487`) with one `ScopedSuppressAllocationTracking` scope calling `pServerMonitorHost->Update()`, with a heap comment naming Winsock, BCrypt, and the snapshot buffer.
- Add the D8 handler, `sbFinalAutosaveDone`, the installation before `"Enter main loop"`, and the release-store + notify after `FinalAutosave()` (`:526-528`).
- Add the D9 QuickEdit block at the start of `ProcessMain`.
- `WindowProcedure`: delete server cases `WM_ERASEBKGND`, `WM_PAINT`, `WM_KEYDOWN`, `WM_LBUTTONDOWN` (`:714-743`) and the server-only `SC_MOVE`/`SC_SIZE`/`SC_MAXIMIZE`/`SC_RESTORE` suppression (`:763-768`). Keep `WM_ENDSESSION`.
- Convert every `gbQuit` access to atomic `store`/`load`.

**Other engine and Common code**
- `Engine/Source/Agent/AgentCommandServer.h:96`: `inline std::atomic<bool> gbQuit = false;`. `AgentCommandsShared.cpp` `CommandQuit`: atomic store.
- `Engine/Source/Server/ServerMonitorHost.{h,cpp}` (new, D3–D5, D10). Members:
  listen and monitor `SOCKET`s, state, secret key `std::string`, nonce, response
  bytes and count, accept time, `std::vector<uint8_t>` send buffer + sent
  offset, snapshot counter. Private: `AcceptMonitor`, `ReceiveResponse`,
  `FlushSend`, `BuildSnapshot`, `CloseMonitor(std::string_view reason)`.
  Destructor closes both sockets. Logs: `kInfo` connection from `<ip>`,
  authenticated, disconnected `<reason>`; `kWarning` rejected `<reason>`
  (`wrong response`, `response timeout`, `unexpected data`).
- Delete `Engine/Source/Server/ServerDisplay.{h,cpp}`.
- `Common/ServerMonitorProtocol.h` (new, D2); `Common/Common.h` includes it.
- `Engine/Source/EngineSwitches.h` `kbAlsoLogToPrintf`; `Common/Log/Log.cpp` `LogWrite` suppression scope (D9).

**The Server Monitor tool**
- `Tools/ServerMonitor/ServerMonitor.cpp` (new, D6).
- `ServerMonitor.vcxproj`, `.vcxproj.filters`, `.sln` (new), modeled on `AgentHarness.vcxproj`: Debug and Release x64, MultiByte, Level4, stdcpp23, no PCH; defines `WIN32_LEAN_AND_MEAN;_CRT_SECURE_NO_WARNINGS`; SubSystem Windows; `bcrypt.lib;ws2_32.lib`; `OutDir $(ProjectDir)Output\`; TargetName `$(ProjectName)` (Release), `$(ProjectName).$(Configuration)` (Debug); new lowercase project GUID; `ServerMonitor` filter.
- `Tools/ServerMonitor/AGENTS.md` (new): purpose, D6 threading and retry rules, secret file location, command-line host, that it compiles no engine or game code, the layout constants the recipe depends on.

**Project membership**
- `BrokenEngineSandboxServer.vcxproj`: `SubSystem` Console and `EntryPointSymbol` `wWinMainCRTStartup` at `:144,207,276` and the matching per-configuration blocks.
- Server `.vcxproj`/`.filters`: swap the `ServerDisplay` items (`:409,543`; filters `:630,987`) for `ServerMonitorHost`.
- `Common/ServerMonitorProtocol.h` membership in every project `/update-vcxproj`'s resolver reports for a shared Common header.

**Build scripts and skills**
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`: add `ServerMonitor` to the `-Target` `ValidateSet` (`:14`), `$script:ReleaseOnlyTargets` (`:47`), and `$script:TargetFiles` (`Tools\ServerMonitor\Platforms\VisualStudio2026\ServerMonitor.sln`).
- `.agents/skills/compile/SKILL.md`: description, `## When to use`, `## Inputs` Release-only list. `.agents/skills/compile/references/worker.md`: the `-Target` list.
- `.agents/scripts/Invoke-IncludeOrderCheck.ps1` `$script:ProjectFiles`: add a `^Tools/ServerMonitor/` row.
- `.agents/skills/update-vcxproj/scripts/Resolve-VcxprojMembership.ps1`: a separate row like the ModelViewer row (`mapping.tools-servermonitor`, project `ServerMonitor`, filter root `ServerMonitor`, detail "ServerMonitor owns its own source tree."), not inside the AgentTools loop. `references/worker.md`: a table row. `SKILL.md`: add `ServerMonitor` to the outcome-line project list and Release|x64 build guidance.

**Harness recipe**
- `git mv` `.agents/skills/agent-harness/references/server-window.md` →
  `server-monitor.md`. Heading `### Server Monitor window capture and click`.
  Prose: the server has no window; build with `/compile -Target ServerMonitor`;
  start from the adopted worktree root with
  `Start-Process -FilePath 'Tools\ServerMonitor\Platforms\VisualStudio2026\Output\ServerMonitor.exe' -WindowStyle Minimized -PassThru`;
  retain the PID; a harness server's app-data root holds no secret file unless
  the scenario writes one; stop with `Stop-Process -Id <pid>`; ignored copy
  `Temp\Capture-ServerMonitor.ps1`. Script block: rename `-ServerPid` →
  `-MonitorPid` and the PID messages; coordinates and the rest unchanged.
- Update links/wording in `command-reference.md:11`, `worker.md:576-578`, `Projects/BrokenEngineSandbox/Documents/AgentHarness.md:26`.

**Azure (D12)**
- `.agents/skills/deploy-server/assets/server.bicep`: the `Admin-Monitor` rule exactly as D12.
- `.agents/skills/deploy-server/SKILL.md`: description and `## When to use` "re-admitting RDP and SSH" → "re-admitting RDP, SSH, and the Server Monitor port" (`:7`, `:24`); the re-admit operation (`:93-97`) runs once each for `Admin-RDP`, `Admin-SSH`, `Admin-Monitor` (`-n <Admin-RDP|Admin-SSH|Admin-Monitor>`); step 6 inbound-rules line (`:177`) adds "TCP 27017 from the user's /32"; step 8 (`:208`) "exactly the three rules" → "exactly the four rules".
- `.agents/skills/deploy-server/references/azure-facts.md:18`: add `Admin-Monitor` to the rule list.
- `Documents/Plans/ChangeWorkflow/CreateAzureServerVm.md`: `:28` inbound-access line adds TCP 27017 from the user's IP; `:68` expected `az network nsg rule list` adds `Admin-Monitor` (130, `Tcp`, the user's /32, port 27017).
- `Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md`:
  - `:9` context NSG sentence adds TCP 27017 (the user's IP only).
  - Windows Firewall bullet (`:112-119`): also create `BrokenEngine-Monitor-TCP`, an inbound Allow rule for TCP local port 27017, same `-Program` and `-Profile Any`, removed first if present; `Initialize-ServerHost.ps1` (`:199`) "recreates the firewall rule" → "recreates both firewall rules".
  - `Publish-Server.ps1` (`:167-172`): add a new step right after step 5 (the GitHub token copy), renumbering the later steps.
    It copies `%APPDATA%\ServerMonitor\ServerMonitorSecret.txt` (the monitor's own secret file on the PC) into `C:\BrokenEngine\State\Broken Engine Sandbox Server\`, creating that folder when absent, and sets the same ACL as the token.
    When the secret file is absent, skip only this step and report that no secret was copied and that any secret already on the VM is unchanged.
    Never print the secret, and **keep** the local file (the monitor reads it).
    The step runs before the server start, because the server reads the secret once, at startup.
  - Acceptance list (`:240-245` region): add that `Get-NetFirewallRule -Name BrokenEngine-Monitor-TCP` is enabled with action Allow, and, when the secret was supplied, the newest `server-*.log` contains `ServerMonitorHost listening on 0.0.0.0:27017 (secret required)`.
  - `:210` risk line: the exposure now also includes TCP 27017 from the user's IP, fixed by `CreateAzureServerVm.md`.

**Documentation**
- `Engine/Source/Server/AGENTS.md`: rewrite for the host. `## Ownership and Cadence`: unconditional per-tick aggregation and why; snapshot only while authenticated, every eighth update; inherited allocation suppression; keep the game-type ownership record. New `## Monitor Channel`: port, bind rule, framing, handshake, every validation and close, single connection. Keep `## Profile Presentation` ("paint" → "snapshot"). Drop every GDI/hash/window bullet.
- `Engine/Source/Network/AGENTS.md` `## Ownership`: one bullet — the Server Monitor TCP channel is a separate, non-ENet protocol owned by `../Server/AGENTS.md` `## Monitor Channel`.
- `Engine/Source/AGENTS.md`: `:161` subsystem line → "Server Monitor host: statistics aggregation and the monitor channel"; one `## Startup and Main Loop` bullet: console subsystem; hidden window only for `WM_ENDSESSION`; Ctrl+C, Ctrl+Break, and console close quit through the console handler after the final autosave; QuickEdit disabled.
- `Common/AGENTS.md` `## Overview`: ServerMonitor compiles `ServerMonitorProtocol.h` by path.
- Root `AGENTS.md`: `## Directory Structure` adds `/Tools/ServerMonitor/` — "Server Monitor: Windows program showing a local or remote server's statistics over the monitor channel"; trust-policy owners add `Engine/Source/Server/AGENTS.md` `## Monitor Channel`.
- `Projects/BrokenEngineSandbox/Source/AGENTS.md:14`, `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md:60,66`: "server monitoring window" → "Server Monitor snapshot".
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` `## Build Configuration`: the server links SubSystem Console with entry `wWinMainCRTStartup`; the client stays Windows.
- `Documents/Architecture/FrameUpdatePipeline.md` `## Server Main Loop`: text and diagram node `ServerUpdateDisplayStatistics` → `ServerMonitorHost::Update`.

**Plans (D14)**
- `Documents/Plans/Engine/ServerMonitorHexCellMap.md`: `dependsOn` →
  `["Documents/Plans/Engine/ConsoleServerAndServerMonitor.md","Documents/Plans/Engine/HexagonalSimulationCells.md"]`
  (ordinal-sorted; `createdUtc` unchanged). First paragraph of `## Context`:
  after `ConsoleServerAndServerMonitor.md` the map is painted by `PaintMap` in
  `Tools/ServerMonitor/ServerMonitor.cpp` from snapshot cell records; the
  monitor compiles no engine header; so this Plan's Design, scope, and recipe
  citation must be re-derived against that file at claim.
- `Documents/Plans/Engine/HexagonalSimulationCells.md` (only if still present): `:132`, `:557` `Engine/Source/Server/ServerDisplay.cpp:228-240` → the Server Monitor's `PaintMap` (`Tools/ServerMonitor/ServerMonitor.cpp`); `:485` `Engine/Source/Server/ServerDisplay.cpp` → `Engine/Source/Server/ServerMonitorHost.cpp` (rename only); `:515` `ServerDisplay.cpp` → `ServerMonitorHost.cpp`.

## Out of scope

- Encryption or integrity protection of the stats stream; more than one monitor; any monitor-to-server record besides the 32-byte response; a port/address launch option; configuration files besides the secret files; reconnect behavior beyond the 2 s retry; connection-attempt rate limiting.
- Running `/deploy-server` or any live Azure operation; writing the `DeployAndRunHostedServer.md` scripts themselves (only its Plan text changes).
- The hex map redraw and any `GridCoord` geometry in the monitor.
- Changing what the statistics compute (including the pre-existing zero entity totals in Release where `kbProfiling == false`).
- `Engine/Source/CrashReport.cpp`'s own token trim.
- Client window/input/entry behavior; `Documents/Architecture/Network.md`; `kiProtocolVersion`; ENet code.
- New harness commands or scripts; `Invoke-HarnessLaunch.ps1`; AgentTools bootstrap/promotion (`ServerMonitor` is not an AgentTool, so `Bootstrap-AgentTools.ps1` and `Provision-WorktreeThirdParty.ps1` are untouched).
- `Common/Sha256Hasher.h` (editing it triggers AgentTools promotion).

## Risk triggers and invariants

Tier 3: new trust boundary and wire surface (a TCP port that can bind every interface); threading (console handler thread, `gbQuit`); build coordination (`/compile`, include-order check, membership resolver used by every session); Azure exposure; edits spanning Engine, Common, Tools, `.agents`, and four other Plans.

- **I1:** The server validates every monitor byte as D3 states, never reads a monitor-supplied length or count, and closes the connection on every violation before any further read.
- **I2:** The secret never leaves either process (except the D12 operator copy to the VM); the nonce is fresh per connection from `BCryptGenRandom`.
- **I3:** With an empty trimmed secret or `--loopback-only`, the listen socket binds `127.0.0.1`.
- **I4:** The server adds no thread; all monitor socket calls run on the main thread.
- **I5:** Per-tick aggregation (explosion counter, mimalloc stats, `UpdateProfileText`) runs every update regardless of monitor state; `query_profile` output is unchanged.
- **I6:** Determinism and CRC untouched: the host reads simulation/transport state after `ServerUpdate` and writes only what the old code wrote.
- **I7:** The console handler touches only `gbQuit` and `sbFinalAutosaveDone`, both atomics, and returns only after the final autosave has finished.
- **I8:** The monitor compiles no engine or game header and names no game concept; its only repository include is `Common/ServerMonitorProtocol.h`.
- **I9:** ENet packets and `kiProtocolVersion` unchanged.
- **I10:** Stop and return to the user if `ServerMonitorHexCellMap.md` is absent and `PaintGridMap` draws hexes (it landed first; moving its engine geometry is not covered here).
- **I11:** Stop and return to the user if `CreateAzureServerVm.md` is absent from the tree (the VM already exists without the monitor rule; the create-only template cannot add it).

## Acceptance criteria

- **A1 — Build and static checks.** `/compile` succeeds with no new warnings for Server Debug and Release, Client Debug and Release, ModelViewer Debug, DataPacker, ServerMonitor. `Invoke-IncludeOrderCheck.ps1` passes on every changed `.h/.cpp`; the static-check runner passes; `/update-vcxproj` reports every added/removed item settled.
- **A2 — Console subsystem.** The PE optional-header `Subsystem` of the built Debug `BrokenEngineSandboxServer` executable is 3 (`IMAGE_SUBSYSTEM_WINDOWS_CUI`), read with a Temp PowerShell snippet; the client's is 2.
- **A3 — Loopback with no secret** (`/agent-harness`, Debug, loopback). Launch server and client, connect the client, start the monitor per `server-monitor.md`, capture `-Target Map` and `-Target Profile`.
  - Server log: `ServerMonitorHost listening on 127.0.0.1:27017 (no ServerMonitorSecret.txt: loopback only)`, then a connection line and an authenticated line.
  - Map: the client's cell filled client-authorized blue with its `"(x,y)"` label and an entity count.
  - Statistics panel: FPS, Potential, Tick, Time, `Active cells`, `Clients: 1`, one row per entity label, Explosions, memory lines.
  - Profile: FPS/Potential header, timer rows, counter rows, memory lines, Copy button.
  - `query_profile` on the server still returns its counter rows, including the explosion counter.
- **A4 — Secret enforcement** (same flow, fresh server).
  1. Before launch write a 64-hex-character secret to `<appDataRoot>\Broken Engine Sandbox Server\ServerMonitorSecret.txt` (the server's app-data game folder). Confirm `%APPDATA%\ServerMonitor\ServerMonitorSecret.txt` does not exist; if it does, stop and ask the user.
  2. Start the monitor: server log shows `rejected (wrong response)`; the monitor shows disconnected/retrying.
  3. Stop the monitor, write the same secret to `%APPDATA%\ServerMonitor\ServerMonitorSecret.txt`, restart it: server log shows authenticated; Map capture shows the map.
  4. Delete both secret files by exact literal paths.
- **A5 — Quit still saves.** Harness `quit` ends the server; its log contains `Exit main loop`; `ServerAutosave.save` in the harness app-data game folder has `LastWriteTimeUtc` after the quit was sent.
- **A6 — No visible console from the harness.** A Temp snippet lists visible top-level windows (`EnumWindows` + `IsWindowVisible`, with owning process names) before and 10 s after the harness server launch: no new visible window belongs to the server, `conhost.exe`, `OpenConsole.exe`, or `WindowsTerminal.exe`. If this fails, stop and report; do not change the harness launch.
- **A7 — User-run.** The user starts the Debug server from a terminal and sees the log stream. Ctrl+C ends it with `Exit main loop` logged and `ServerAutosave.save` updated. Restarting and closing the console window also updates `ServerAutosave.save`. Clicking in the console does not stop the tick count shown by the monitor.
- **A8 — Code read.** `ServerMonitor.cpp` includes no `Engine/` or `Projects/` path and no game name; `ServerMonitorHost.cpp` reads at most 32 bytes per connection before authentication and closes on any byte after it, on timeout, and on mismatch; `gbQuit` has no non-atomic access; `ServerDisplay` appears nowhere in the tree outside history.
- **A9 — Azure text.** `server.bicep` has exactly four NSG rules with `Admin-Monitor` as D12 states; every `/deploy-server`, `azure-facts.md`, `CreateAzureServerVm.md`, and `DeployAndRunHostedServer.md` line listed in scope names the fourth rule; no other rule changed. Verified by code read (nothing reaches the template until `CreateAzureServerVm.md` runs).

## Residuals

- Untested assumptions: whether `Start-Process -WindowStyle Hidden` keeps a console program hidden when Windows Terminal is the default terminal (A6 gates it); whether `FinalAutosave()` fits the `CTRL_CLOSE_EVENT` timeout (A7 observes it).
- `CrashReport.cpp` keeps its own whitespace trim; `ServerMonitorSecretKey` is a separate protocol-owned derivation.

