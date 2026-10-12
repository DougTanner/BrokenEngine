<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-12T22:22:44.287Z","dependsOn":[]} -->
# Text chat between connected clients

## Context

Players connected to the same server have no way to talk to each other. The user asked for bare-minimum chat with Unicode support; moderation (bans, filtering, mutes) is explicitly not wanted yet. Research behind the design: Unicode Core Spec Ch. 3 Table 3-7 (well-formed UTF-8), Win32 `WM_CHAR`/IME docs, Dear ImGui `FONTS.md`.

What exists today:

- Dear ImGui 1.92.6 with the stock Vulkan backend (`ImGuiBackendFlags_RendererHasTextures`), so glyphs load on demand from the single font, Noto Sans SC (`Engine/Source/Graphics/Managers/ImGuiManager.cpp:104-162`). Latin, Greek, Cyrillic and CJK render; no other font, no shaping, no RTL. ImGui's Win32 backend already turns `WM_CHAR` (including surrogate pairs) and IME results into text input.
- The focused keyboard is registered with `RIDEV_NOLEGACY` in `RawInputManager::UpdateFocus` (`Engine/Source/Input/RawInputManager.cpp:23-41`, flag at :35), so no `WM_KEYDOWN`/`WM_CHAR` reaches `ImGui_ImplWin32_WndProcHandler` (`Engine/Source/Main.cpp:643`). An ImGui text box would receive nothing from a physical keyboard until this changes.
- No player names: the server identifies a connection by `ClientConnection::iClientId` (`Engine/Source/Network/Server/Server.h:35-113`, assigned from `miNextClientId` at `Server.cpp:170`).
- Engine packets: `PacketType` (`Engine/Source/Network/NetworkProtocol.h:13-33`), `PacketTypeName` (:35-59), `kiProtocolVersion` (:61, currently 21), `GetClientPacketContract` rows (:171-184). Message structs with `Visit` in `NetworkMessages.h` (:391-406, structs from :501); the Hello `NullTerminatedString` reader (:196-213, used at :523).
- Agent launches strip physical `WM_KEY*`/`WM_CHAR` (`Main.cpp:640-647`); no harness command can type into an ImGui text box (the only `AddInputCharacter` use is the `kSetSlider` script, `Engine/Source/Agent/AgentInput.cpp:281-292`, with a 32-byte ASCII buffer).

## User decisions

Chosen by the user from presented options in the planning conversation:

- Audience — "Everyone on server": every handshake-complete client receives every message, sender included (server echo).
- Sender label — "\"Player N\"": N is the server-assigned `iClientId`; it changes on reconnect; no names, no settings.
- Unicode display — "Current font only": the wire carries any valid UTF-8; unsupported scripts (Arabic, Hebrew, Indic, Thai, emoji) render as missing-glyph boxes. No new fonts or libraries.
- Placement — "Engine": network messages and relay in `Engine/Source/Network`, chat UI in the engine, not the game project.
- Testing — "Add type_text command": one new agent-harness client command types UTF-8 text into the focused ImGui text box and presses Enter.

Defaults stated in the same presentation, not objected to: Enter opens the chat box, Enter sends, Esc cancels; messages in the lower-left corner fading after about 8 seconds; a 100-line client history; 256 UTF-8 bytes maximum per message; a server per-client token bucket (burst 5, refill 1 per second, wall clock) with excess messages silently dropped; protocol version bump; no history for late joiners, no persistence, not in replays.

## Design

The author recommends the following single design.

### Packets

- Insert `kClientChat` and `kServerChat` into `PacketType` before `kGamePacketStart`, add their `PacketTypeName` cases, and bump `kiProtocolVersion` 21 → 22 (inserting engine types shifts game packet identifiers).
- `ClientChatMessage` in `NetworkMessages.h` next to `ServerTimespeedUpdateMessage`: packet type plus `NullTerminatedString text`, reusing the Hello string reader rather than adding a length-prefixed string. `kiMaxTextBytes = 256`. Client contract row: minimum 3 bytes (type, one text byte, NUL), maximum 258, per-tick cap 8, handshake required, over-cap is a violation.
- `ServerChatMessage`: packet type, `int64_t iSenderId`, `NullTerminatedString text`. Server-to-client types use the default contract sentinel row, so no row.
- Both directions use `kiChannelReliable` (`Engine/Source/Network/NetworkManager.h:18`).

### Text validation

New `Engine/Source/Network/ChatText.h/.cpp`, compiled into both client and server, included from `Engine.h` next to `Network/NetworkMessages.h` (:15). One function, `bool IsValidChatText(std::string_view text)`, returning false when:

- the text is empty or longer than 256 bytes;
- the bytes are not well-formed UTF-8 per Unicode Table 3-7 (overlong forms, surrogates U+D800-DFFF, code points above U+10FFFF, truncated sequences; second-byte constraints E0→A0-BF, ED→80-9F, F0→90-BF, F4→80-8F);
- any code point is a control character: U+0000-001F, U+007F, or U+0080-009F (a newline would also break the one-line display, since `TextUnformatted` breaks on `\n`).

The author recommends no further content rules (bidi controls, noncharacters, stacked combining marks): ImGui does no shaping or bidi reordering, so those characters cannot reorder or stack text here, and rejecting them is moderation the user excluded. Revisit them if shaping or RTL layout is ever added.

### Server relay

- `Server::Receive` switch (`Engine/Source/Network/Server/Server.cpp:296-332`) gets a `kClientChat` case calling new `Server::ClientChat(packet data, ClientConnection&)`, placed after `Server::ClientDebugFrameRequest`.
- `Server::ClientChat` order: `NetworkMessages::Read` → require `reader.AtEnd()` (an embedded NUL leaves bytes unread) and `IsValidChatText`, both failing through `ThrowCorruptStream` into the existing corrupt-violation catch (`Server.cpp:334-340`) → token bucket → broadcast.
- Token bucket: two new `ClientConnection` members beside the existing steady-clock cooldowns (`Server.h:67-69`): `float fChatTokens = 5.0f` and a `std::chrono::steady_clock::time_point` of the last refill. Refill 1 token per elapsed second, clamp to 5, spend 1 per message; with less than 1 token the message is dropped without a violation (a rate limit is not corruption, per `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`).
- Broadcast: loop over clients skipping those without `bHandshakeComplete`, write `ServerChatMessage { iClientId, text }` through the workbuffer and `SendPacket` on `kiChannelReliable`, exactly as `Server::BroadcastLoadNotification` (`Server.cpp:808-825`) does. Sender included.
- All of this runs inside `Server::Receive` during network poll; it touches no frame, `FrameInput`, CRC, save, or replay state.

### Client receive, history, and send

- New client-only `Engine/Source/Ui/ChatLog.h`: struct `ChatLog` holding a fixed ring of 100 entries `{ int64_t iSenderId; char pcText[257]; std::chrono::steady_clock::time_point receivedTime; }` with a head index and count, plus `bool bInputOpen` and `char pcInput[257]`. Fixed arrays, so no main-loop allocation. Held by value as `GameBase::mChatLog` under `BT_CLIENT`, next to `mModalMessage` (`Engine/Source/GameBase.h:226`).
- Client server-to-client dispatch switch (`Engine/Source/Network/Client/Client.cpp:201-228`) gets a `kServerChat` case calling new `Client::ServerChat`, placed beside `Client::ServerTimespeedUpdate` (:760-774): ignore unless the connection is accepted, `Read`, append to `mChatLog`, and `LOG(kNetwork, kInfo, "Client::ServerChat Player {}: {}", iSenderId, text)`. No value checks (the client trusts its server).
- New `Client::SendChat(std::string_view text)` modelled on `Client::SendDesynchronizationReport` (`Client.h:75`, `Client.cpp:830-851`), followed by a flush as rare user requests are. `ChatScreen` calls it directly on the main thread through `game::gpClientSession->mpRuntime->mpClient`, the same path `ClientDesyncCore` uses (`Engine/Source/Network/Client/ClientDesyncCore.cpp:20`), only when that client exists and its connection is accepted; otherwise the text is discarded.

### Chat screen and input

- New `MenuInputFlags::kOpenChat` (`Engine/Source/Input/Input.h:10-33`), set in `Input::BeginPoll` (`Input.cpp`, beside `kPauseMenu` at :77) from `inputPoll.KeyboardPressed(VK_RETURN)`.
- `GameBase::ProcessInput` (`Engine/Source/GameBase.cpp:45`): after the quit gate (:51-54), while `mChatLog.bInputOpen`, take the same early return as the modal path (:56-60, including the `mPreviousRawInput` copy) so gameplay keys and Esc→pause do nothing while typing. Otherwise, in game (not main menu) with `meUiState == UiState::kNone`, `kOpenChat` sets `mChatLog.bInputOpen`.
- New `Engine/Source/Ui/Screens/ChatScreen.h/.cpp`, client-only, owned by `ImGuiManager` as `mpChatScreen` (constructed in `ImGuiManager::ImGuiManager` beside the other screens, :166-173; member beside `mpHudScreen`, `ImGuiManager.h:88`). `ImGuiManager::Prepare` calls `mpChatScreen->Render()` right after `mpHudScreen->Render()` inside the `!mbShowImGui` block (:483-490).
- `ChatScreen::Render`:
  - When not in game, or `meUiState != UiState::kNone`, it clears `bInputOpen` and the input text and draws nothing.
  - History window anchored lower-left (`NoTitleBar | NoBackground | NoInputs`). Each line reads `<Player label> <N>: <text>` drawn with `TextUnformatted`, alpha fading out over 8 seconds. All lines are shown while the box is open.
  - While `bInputOpen`, it draws an `InputText` over `pcInput` (buffer 257, so at most 256 bytes) with `ImGuiInputTextFlags_EnterReturnsTrue`, taking keyboard focus on the opening frame.
    - On Enter with text that passes `IsValidChatText`, it sends the text through `Client::SendChat` (see above), clears it, and closes the box. Empty text closes the box without sending. Invalid text keeps the box open with the text unchanged, so a legitimate client never sends a message the server counts as corrupt.
    - When the item is deactivated without Enter (Esc), it clears the text and closes the box.
  - Last, it calls `gpRawInputManager->SetTextInput(bInputOpen)`.
- The player label is a new `StandardString` entry (`Engine/Source/Ui/LocalizationBase.h`) with an English row in the UTF-32 table (`LocalizationBase.cpp`). Other languages are left empty and fall back to English.
- `RawInputManager`: new member `bool mbTextInput` and new `void SetTextInput(bool bEnabled)`. When the value changes while the keyboard is registered, it re-registers the keyboard with `dwFlags = 0` (text input on) or `RIDEV_NOLEGACY` (off). `UpdateFocus(true, ...)` registers with the flag matching `mbTextInput` instead of always `RIDEV_NOLEGACY`. `SetTextInput` stores the window handle `UpdateFocus` already receives. With legacy messages on, `WM_KEYDOWN`/`WM_CHAR`/IME messages reach ImGui; `WM_INPUT` keeps arriving, so the raw key snapshot stays coherent.

### Harness command

- New client command `type_text {"text": "<UTF-8>"}` in `engine::ExecuteClientAgentCommand` (`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1190`), as a static `CommandTypeText` modelled on `CommandKey` (:972-983).
  - It throws `"type_text requires string 'text'"` when the field is missing or not a string, or is longer than 256 bytes. An empty string is allowed and only presses Enter.
  - It fills an `engine::AgentScript` and starts it through `BeginScriptAndDefer`.
- New `AgentScriptKind::kTypeText` (`Engine/Source/Agent/AgentInput.h:12-19`) with a `char pcTypedText[257]` member on `AgentScript`.
- New case in `AgentInput::AdvanceFrame` (`AgentInput.cpp:160`, switch at :179):
  - Phase 0 calls `ImGuiIO::AddInputCharactersUTF8(pcTypedText)`.
  - Phase 1 sends `ImGuiKey_Enter` down and phase 2 sends it up, mirroring the Enter handling in the `kSetSlider` case (:297, :306).
  - It then completes the deferred response.
- No change in `Tools/AgentHarness` (it forwards commands without a name list, `AgentHarness.cpp:628-648`).

## In scope

- `Engine/Source/Network/NetworkProtocol.h`: `PacketType` (two values), `PacketTypeName` (two cases), `kiProtocolVersion`, `GetClientPacketContract` (one `kClientChat` row).
- `Engine/Source/Network/NetworkMessages.h`: new `ClientChatMessage` and `ServerChatMessage` structs.
- New `Engine/Source/Network/ChatText.h/.cpp` (`IsValidChatText`); `Engine/Source/Engine.h` one include beside `Network/NetworkMessages.h`.
- `Engine/Source/Network/Server/Server.h`: `ClientConnection` (two token-bucket members), `Server::ClientChat` declaration. `Server.cpp`: one `kClientChat` case in `Server::Receive`, new `Server::ClientChat`.
- `Engine/Source/Network/Client/Client.h/.cpp`: one `kServerChat` dispatch case, new `Client::ServerChat`, new `Client::SendChat`.
- New `Engine/Source/Ui/ChatLog.h`; `Engine/Source/GameBase.h` member `mChatLog`; `Engine/Source/GameBase.cpp` `GameBase::ProcessInput` (chat-open early return and `kOpenChat` handling).
- `Engine/Source/Input/Input.h` `MenuInputFlags::kOpenChat`; `Engine/Source/Input/Input.cpp` `Input::BeginPoll` (one flag line).
- `Engine/Source/Input/RawInputManager.h/.cpp`: `mbTextInput`, the stored window handle, new `SetTextInput`, `UpdateFocus` registration flag.
- New `Engine/Source/Ui/Screens/ChatScreen.h/.cpp`; `Engine/Source/Graphics/Managers/ImGuiManager.h/.cpp`: forward declaration, `mpChatScreen` member, construction in the constructor, the `Render` call in `ImGuiManager::Prepare`.
- `Engine/Source/Ui/LocalizationBase.h/.cpp`: one `StandardString` value and its table row.
- `Engine/Source/Agent/AgentInput.h/.cpp`: `AgentScriptKind::kTypeText`, `AgentScript::pcTypedText`, the `kTypeText` case in `AgentInput::AdvanceFrame`. `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`: `CommandTypeText` and its dispatch row.
- Project membership for the new files: `ChatText.*` in both `BrokenEngineSandbox.vcxproj` and `BrokenEngineSandboxServer.vcxproj` (and `.filters`); `ChatLog.h` and `ChatScreen.*` in the client project only (via `/update-vcxproj`).
- Docs:
  - `Documents/Architecture/Network.md` `## Client → Server Contract` (new table row and the protocol-version sentence) and `## Server` (chat relay bullet).
  - `Engine/Source/Network/AGENTS.md` `## Ownership`.
  - `Engine/Source/Input/AGENTS.md` `## Device and Frame Boundaries` (the `RIDEV_NOLEGACY` invariant now has a chat-box exception).
  - `Engine/Source/Ui/AGENTS.md` `## Shared Types` (`mChatLog`).
  - `Engine/Source/Ui/Screens/AGENTS.md` `## Screen-Specific Contracts` (ChatScreen).
  - `Engine/Source/Graphics/Managers/AGENTS.md` `### ImGuiManager`.
  - `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md` `### Client commands` and `### Client command behavior` (`type_text`).

## Out of scope

- Moderation: bans, mutes, profanity or word filters, duplicate-message detection, escalating penalties, "slow down" notices.
- Player names or any persistent identity in chat; team, whisper, or nearby-only channels.
- Chat history for late joiners, chat persistence, chat in replays, saves, frames, `FrameInput`, or the CRC.
- New fonts, emoji, text shaping (HarfBuzz), bidirectional layout, Unicode normalization, confusable checks.
- Rejecting bidi controls, noncharacters, or stacked combining marks.
- Custom IME composition rendering or candidate-window placement (the OS default IME window is used).
- Any change to `Tools/AgentHarness`, the game project's `Source/` tree, or game packet types.
- Any refactor of `Server`, `Client`, `Input`, `RawInputManager`, `ImGuiManager`, or the agent script machinery beyond the named regions.

## Risk triggers and invariants

Tier 3: wire protocol change (two new packet types, version bump) and a new client-to-server trust boundary carrying free-form text.

- The server validates every chat byte before relaying it. A malformed message is a corrupt-input violation. A rate-limited message is dropped without a violation.
- Chat never touches simulation state, frames, `FrameInput`, the per-tick CRC, saves, or replays. Replay determinism is unchanged.
- No main-loop heap allocation is added on either side: fixed buffers and the workbuffer only.
- The raw-input snapshot stays the single source of key state. Legacy keyboard messages reach ImGui only while the chat box is open, and every focus-gain registration honours the current text-input state.
- `mPreviousRawInput` is still copied on every `ProcessInput` path, including the chat-open early return (`Engine/Source/Input/AGENTS.md` `## Poll Lifetime`).

## Acceptance criteria

Verified with `/agent-harness` on one server and two clients, both in game:

1. On client A, `key ENTER` then `type_text {"text":"hello 你好 привет"}`. `get_logs {"pattern":"Client::ServerChat Player"}` on both A and B shows the same line with A's client id.
2. A `screenshot` on B shows the lower-left chat line with the CJK and Cyrillic glyphs rendered (not boxes).
3. Six `type_text` sends from A in quick succession (each after `key ENTER`): B's log shows exactly five new lines, and the server log shows no corrupt-input violation for A.
4. With A's box open, `key ESCAPE` does not open the pause menu (`screenshot`). `type_text {"text":""}` then closes the box without sending (no new log line on B), and a following `key ESCAPE` opens the pause menu. The game input callback being skipped while the box is open and physical Esc closing the box are verified by code reading, because agent launches suppress physical keys.
5. The replay determinism check passes.
6. Server rejection of malformed UTF-8 and of control characters is verified by code reading of `IsValidChatText` and `Server::ClientChat`, because no normal client sends them.
7. Client and server build cleanly. Static checks pass.
