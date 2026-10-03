<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T23:09:04.707Z","dependsOn":[]} -->
# Extract ClientSubscriptions from Client and merge Client's .cpp files

## Context

The engine `Client` class (`Engine/Source/Network/Client/Client.h`) spreads its member definitions over three `.cpp` files at baseline `81cf299f2bd76ea5c37aa181a24b24cfdcb78b13`: `Client.cpp` (3,221 bt-token-v1), `ClientReceive.cpp` (5,414), and `ClientSend.cpp` (1,787). Keeping every member definition of one `class` in one `.cpp` would merge them into ~10,422 bt-token-v1, over the `/reduce-file` `.cpp` threshold of 10,000 (`.agents/skills/reduce-file/references/worker.md` step 2). The user decided that a class too large to merge is split into multiple classes, one Plan per class, covering `class` types only.

Merely moving the `SubscribeRequests` member definitions (`Client.cpp:14-86`, ~460 tokens) out would leave the merged `Client.cpp` at ~9,950, so `Client` itself has to give up a responsibility.

Measured from the code, the coord subscription slot state is the cohesive responsibility to extract. These members read and write only `mCoordinateSlots` and `mSubscribeRequests`, plus each other:

- `Client::FreeSlot`, `Client::RecoverTimedOutSubscriptions` and the slot loop of `Client::ResetAllSlots` (`Client.cpp:160-194`).
- `Client::IsStaleRetainedEpoch`, `ClassifyFullState`, `ClassifyCoordinateUpdate`, and `ClassifySubscribeAccept` (`ClientReceive.cpp:39-148`), with their `FullStateFlags`, `CoordUpdateFlags`, and `SubscribeAcceptFlags` enums and `*_t` aliases (`Client.h:190-216`).
- `SubscribeRequests` and the `kSubscriptionTransitionTimeout` constant both of them use (`Client.cpp:14-86`).

None of those members touches the ENet host or peer, the receive buffers, the state flags, or the timing and bandwidth state. Everything else in `Client` is the transport peer: ENet lifetime, `Poll`, dispatch, packet handlers, sends, ACK tracking (`TrackReceivedTick` needs `mStateFlags` and `mFramesReceived`), and RTT, jitter, and bandwidth. That part stays in `Client`.

## Design

Recommended boundary, chosen because it is the only part of `Client` whose members use only their own state. Each choice below is the author's recommendation.

1. New `Engine/Source/Network/Client/ClientSubscriptions.h` (whole file `#if defined(BT_CLIENT)`, like `Client.h`) declares `engine::ClientSubscriptions`, a concrete class. Move these declarations into it unchanged from `Client.h`: `CoordSubscriptionState`, `ClientCoordSlot`, `SubscribeRequestFlags`, `SubscribeRequest`, and `SubscribeRequests`, together with their comments. The class has:
   - public state `std::vector<ClientCoordSlot> mCoordinateSlots;` and `SubscribeRequests mSubscribeRequests;` (rule 49: already public on `Client`);
   - public members moved from `Client` with their bodies unchanged except for the renames below: `FreeSlot(int64_t)`, `RecoverTimedOutSubscriptions()`, `IsStaleRetainedEpoch(...) const`, the three `Classify*` functions, and their flag enums and aliases. They become public because `Client` calls them across the class boundary;
   - `Reset()`: the body of today's `Client::ResetAllSlots` without its `ClientNetworkFixtures::Reset(*this)` call. It frees every slot and clears `mSubscribeRequests`.
   It has no constructor. `Client`'s constructor keeps sizing `mSubscriptions.mCoordinateSlots` inside its existing `ScopedSuppressAllocationTracking` scope.
2. New `Engine/Source/Network/Client/ClientSubscriptions.cpp` holds every `SubscribeRequests` and `ClientSubscriptions` member definition, plus `kSubscriptionTransitionTimeout`. Log strings keep their current text, including the `Client::` prefixes, so log output does not change.
3. `Client` owns `ClientSubscriptions mSubscriptions;` by value as a public member, in place of `mCoordinateSlots` and `mSubscribeRequests`. `Client.h` includes `Network/Client/ClientSubscriptions.h`. `Engine/Source/Engine.h` adds it to the `// Network client` group directly before `Network/Client/Client.h`. `Client::ResetAllSlots` remains and calls `ClientNetworkFixtures::Reset(*this)` and then `mSubscriptions.Reset()`. Inside `Client`, calls to moved members go through `mSubscriptions`.
4. Merge the `Client` member definitions and the file-local `static DecompressAndReadFrame` from `ClientReceive.cpp` and `ClientSend.cpp` into `Client.cpp`, keeping each definition's text and relative order, and delete the two files. The include block becomes the union of the three files' needed includes, ordered by rule 47 with `Network/Client/Client.h` first.
5. Rename the moved members at their callers and change nothing else at those sites:
   - `rClient.mCoordinateSlots` / `mpClient->mCoordinateSlots` / `gpClient->mCoordinateSlots` becomes `....mSubscriptions.mCoordinateSlots`;
   - `....mSubscribeRequests` becomes `....mSubscriptions.mSubscribeRequests`;
   - `mpClient->RecoverTimedOutSubscriptions()` becomes `mpClient->mSubscriptions.RecoverTimedOutSubscriptions()`.
6. Route membership through `/update-vcxproj`: remove `ClientReceive.cpp` and `ClientSend.cpp`, and add `ClientSubscriptions.h` and `ClientSubscriptions.cpp` with the same client-only membership and filter as `Client.cpp`.
7. Route `/update-claude-docs` for `Engine/Source/Network/Client/AGENTS.md`. Lines 13 and 26 name `ClientReceive.cpp`, and line 17 places `SubscribeRequests` in `Client.h`. Those references must name the new owners.

Exposure: no wire or protocol change: packet layouts, send order, channels, and `kuiProtocolVersion` stay the same. Threading is unchanged: everything stays on the main thread with no new locking. Determinism/CRC, serialization, `.pack`, replay, and the client trust policy are untouched. Every allocation stays inside its existing `ScopedSuppressAllocationTracking` scope.

Change Workflow tier: Tier 2. Trigger: a behavior-preserving change to `engine::Client`'s public interface. The member path changes and member moves are consumed across engine Network, engine Agent, engine Profile, and game Agent fixtures. None of the Tier-3 surfaces (wire/protocol, threading, determinism/CRC, serialization, trust) changes. A reviewer may escalate if the diff shows otherwise.

## Critical files

- `Engine/Source/Network/Client/Client.h`, `Client.cpp`, `ClientReceive.cpp` (deleted), `ClientSend.cpp` (deleted)
- `Engine/Source/Network/Client/ClientSubscriptions.h`, `ClientSubscriptions.cpp` (new)
- `Engine/Source/Engine.h`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`
- `Engine/Source/Agent/Commands/ClientNetworkFixtures.cpp`
- `Engine/Source/Profile/ProfileNetworkScreen.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientFullStateFixture.cpp`, `ClientSubscriptionFixtures.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` and `.vcxproj.filters` (through `/update-vcxproj`)
- `Engine/Source/Network/Client/AGENTS.md` (through `/update-claude-docs`)

## In scope

- Moving the declarations and definitions listed in `## Design` steps 1-2 from `Client.h` and `Client.cpp`/`ClientReceive.cpp` into the `ClientSubscriptions` pair, and adding `ClientSubscriptions::Reset`.
- In `Client`: replacing `mCoordinateSlots` and `mSubscribeRequests` with `mSubscriptions`, reducing `Client::ResetAllSlots` to its fixture reset plus `mSubscriptions.Reset()`, and routing the moved calls inside `Client` member bodies through `mSubscriptions`.
- Merging every `Client` member definition from `ClientReceive.cpp` and `ClientSend.cpp` into `Client.cpp` and deleting those two files.
- The member-path renames in `## Design` step 5 at the caller files listed in `## Critical files`, the `Engine.h` include line, the project membership change, and the `Client/AGENTS.md` reference updates.

## Out of scope

- Any change to statement logic, branch order, log text, packet layout, channel, send timing, or ACK/RTT/jitter/bandwidth behavior.
- Moving `TrackReceivedTick`, `SendSubscribe`, `SendUnsubscribe`, `SendAcknowledgement`, or any packet handler out of `Client`.
- `ClientSessionRuntime`, `ClientDesyncCore`, `ReconcileReplay*`, and `Server` structure beyond the listed call-site renames.
- `Documents/Features/Network/ForwardErrorCorrection.md`, a manual idea document that quotes old paths.
- Changing which `Client` members are public or private beyond the moves above, and adding accessors.

## Acceptance criteria

- `Client` member definitions live only in `Client.cpp`, and `ClientSubscriptions` and `SubscribeRequests` member definitions live only in `ClientSubscriptions.cpp`.
- `Measure-Tokens.ps1` reports each of `Client.cpp`, `ClientSubscriptions.cpp` at or below 10,000 bt-token-v1 (expected ~8,700 and ~1,900), and each of `Client.h` and `ClientSubscriptions.h` at or below 5,000.
- `ClientReceive.cpp` and `ClientSend.cpp` no longer exist, and `/update-vcxproj` validation passes.
- The client and server builds of BrokenEngineSandbox both compile cleanly through `/compile`.
- `Test-IncludeOrder.ps1` reports no violation in the changed `.cpp` files.
- Live check through `/agent-harness`: a client connects to a local server, reaches active subscribed slots, and keeps receiving updates. The existing client subscription and full-state agent fixtures complete with their current outcomes.

## Notes

- Expected sizes: `Client.cpp` ~8,700 (from 3,221, absorbing 5,414 + 1,787 less ~1,800 moved out); `ClientSubscriptions.cpp` ~1,900 (new); `Client.h` ~1,500 (from 2,188); `ClientSubscriptions.h` ~900 (new).
- `Documents/Plans/Game/SplitCppCorrespondingHeaderOrder.md` lists include-order rows for `Engine/Source/Network/Client/ClientReceive.cpp`. This Plan deletes that file, which supersedes those rows, and that Plan already says to skip a listed path that was removed. Whichever lands first, the other still completes, so neither needs a dependency edge or a Coordination section.
- `Documents/Plans/Game/CancelledSubscriptionRangesContains.md` and `ObserveCancelledSubscriptionState.md` edit lines in `ClientSubscriptionFixtures.cpp` near this Plan's `mSubscribeRequests` rename (around lines 228-237). Their content does not depend on each other, so the later change only rebases.
