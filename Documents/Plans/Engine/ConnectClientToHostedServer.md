<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-12T22:42:29.897Z","dependsOn":["Documents/Plans/ChangeWorkflow/ProvisionAzureServerVm.md"]} -->
# Connect the client to the hosted server from the Remote Server button

## Context

This is the third of four Plans that put the dedicated server on the internet. `Documents/Plans/ChangeWorkflow/ProvisionAzureServerVm.md` (the prerequisite) provisioned an Azure VM with a Standard static public IP and wrote that address into `## Server address` below. `Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md` follows. It deploys the server and proves this Plan's connection end to end.

Today the client cannot reach a server outside its LAN. Main Menu's Remote Server entry is always drawn disabled, with the comment "displayed disabled because no connection action is available" (`Engine/Source/Ui/Screens/MainMenuScreen.cpp:122-129`). The only connection actions are the discovered loopback or LAN server (`Projects/BrokenEngineSandbox/Source/Game.cpp:499-501`, `:598-603`) and the main-menu Return-key fallback to `127.0.0.1` (`:604-607`). No launch option or UI field takes an address (`Engine/Source/LaunchOptions.cpp`).

User decisions (this session, binding):

- The client reaches the hosted server by the server's static public IP, embedded as a compile-time constant.
- The always-disabled Remote Server button becomes enabled and connects to that IP.

Main-session decision the user did not object to (not a user decision): the raw IP is used instead of an Azure DNS label. The cost is the same, and a raw IP avoids the blocking, unchecked `enet_address_set_host` name lookup (`Engine/Source/Network/Client/Client.cpp:43`).

For a dotted IPv4 literal, ENet's resolver needs no name lookup. When `gethostbyname` does not return the address, `enet_address_set_host` falls back to its own dotted-quad parser, `enet_address_set_host_ip` (`ThirdParty/enet/win32.c:89-101`).

## Server address

`kRemoteServerAddress` is the IPv4 literal on the line below. `ProvisionAzureServerVm.md` writes it in its final step, and this Plan becomes claimable only after that Plan lands. If the line still reads `none`, stop BLOCKED. Once this Plan lands, the C++ constant is the address's single source of truth.

Recorded address: none

## Design

- `Engine/Source/GameBase.h` `StandardMenuAction` (`:189-194`): add `kConnectToRemoteServer` after `kConnectToDiscoveredServer`. The engine names the action. The game owns the address, because which server to reach is this game's policy (`.agents/references/cpp-conventions.md`, code-placement bullet).
- `MainMenuScreen::Render`, in the `StandardMenuFeature::kRemoteServer` block (`MainMenuScreen.cpp:122-129`):
  - When `!(model.state & StandardMenuState::kClientPresent)`, draw the button enabled with its existing label and hover slot. A click calls `game::gpGame->ApplyStandardMenuAction(StandardMenuAction::kConnectToRemoteServer)`.
  - While a client is present (a connection attempt or session already exists), keep drawing it inside `ImGui::BeginDisabled()`/`EndDisabled()`, as today, so a second click cannot restart an attempt in progress.
  - Replace the old comment with one stating that rule.
  - Nothing reads the model after this action later in the same pass, so no reread is added.
- `Game.cpp`, in the `BT_CLIENT` region holding `Game::ApplyStandardMenuAction` (`:451-507`):
  - Add a file-scope `static constexpr std::string_view kRemoteServerAddress = "<Recorded address>";`.
  - Add the case `engine::StandardMenuAction::kConnectToRemoteServer: gpClientSession->ConnectToServer(kRemoteServerAddress); break;`.
  - `ClientSession::ConnectToServer` (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:184-188`) already clears the modal message and connects on `engine::kiDefaultPort`. `ClientSessionRuntime::Connect` stops discovery (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:116-139`), and an unreachable address ends in the existing "Connection failed" modal (`:233-237`).
- `Engine/Source/Ui/Screens/AGENTS.md` `## Standard Menu Contract` (`:10`): replace "missing `kRemoteServer` hides its disabled placeholder". The new wording: `kRemoteServer` shows a Remote Server entry, enabled only while no client is present, that routes `kConnectToRemoteServer`; missing it hides the entry.

Risk tier: Tier 2. This is client-only menu behavior through the engine-owned standard-menu contract, with its single game implementer. The connection uses the existing `ConnectToServer` path, so no wire format, protocol, handshake, save, determinism, threading, or trust rule changes.

## Critical files

- `Engine/Source/GameBase.h`
- `Engine/Source/Ui/Screens/MainMenuScreen.cpp`
- `Engine/Source/Ui/Screens/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Game.cpp`

## In scope

- `StandardMenuAction::kConnectToRemoteServer`.
- The Remote Server block of `MainMenuScreen::Render` and its comment.
- `kRemoteServerAddress` and the new `Game::ApplyStandardMenuAction` case.
- The `kRemoteServer` sentence in `Engine/Source/Ui/Screens/AGENTS.md`, and any further AGENTS.md wording `/update-claude-docs` finds stale.

## Out of scope

- The Local Server entry, discovery, auto-connect, and the Return-key loopback fallback (`Game.cpp:598-608`).
- An address input field, a launch option, a settings file, or a DNS name. IPv6.
- Checking the `enet_address_set_host` result (gap G13), and any `Client`, `ClientSessionRuntime`, or ENet change.
- `StandardMenuFeature` values, the button label and localization (`Engine/Source/Ui/LocalizationBase.cpp:108-110`), and button width measurement.
- Server deployment and the end-to-end internet connection test, which `DeployAndRunHostedServer.md` owns.

## Acceptance criteria

- The `kRemoteServerAddress` literal in the diff equals the `Recorded address:` line above.
- `/compile` builds the `BrokenEngineSandbox` client in Debug and Release x64 and the server in Debug, with Shared data and no new warnings. Static checks pass.
- With `/agent-harness`, a Debug client and a local Debug server run on the main menu:
  - `describe_ui` lists `REMOTE SERVER` as an enabled item while no client exists.
  - The Local Server entry still connects to the local server as before.
  - After returning to the main menu, clicking `REMOTE SERVER` disables the entry while the attempt runs. The VM is only provisioned at this point, deallocated with no server deployed (`DeployAndRunHostedServer.md` deploys it), so nothing answers the address, and the attempt ends in the `Connection failed` modal.
