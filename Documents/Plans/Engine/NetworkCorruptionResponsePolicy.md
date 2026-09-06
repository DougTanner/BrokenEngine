<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T19:46:41.145Z","dependsOn":[]} -->
# Enforce asymmetric network corruption responses

## Context

The user requires an asymmetric trust-boundary policy: a client that receives corrupt data from its authoritative server must fail through the repository's `ASSERT` behavior and let the resulting exception reach crash reporting; a server that receives corrupt data from a client must warn and drop that packet as early as possible. The user deferred both the policy documentation and runtime work to this follow-up so the design receives normal Tier-3 review and approval before source changes.

Current behavior does not satisfy that policy. Client engine dispatch catches every `std::exception`, logs, and continues (`Engine/Source/Network/Client/Client.cpp:246-310`); client game dispatch has two more broad catches with the same result (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:50-87`). Several client engine and game decoders also return silently on malformed input, including full-state and debug-frame reads, timespeed reads, and game player/fleet payloads (`Engine/Source/Network/Client/ClientReceive.cpp:183-189,442-463,650-659`; `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp:12-49,61-91`). Malformed status-change data returns zero changes and is accepted for later CRC mismatch/resync (`Engine/Source/Network/NetworkSerialization.cpp:275-385,429-461`; `Engine/Source/Network/Client/ClientReceive.cpp:405-417`; `Engine/Source/Network/AGENTS.md:34-36`). These paths prevent a report-producing fatal response.

The server already rejects packet-contract failures and caught handler throws through `Server::RecordContractViolation`, but some engine decoders return without recording a violation (`Engine/Source/Network/Server/Server.cpp:188-321,509-543,583-586`; `Engine/Source/Network/Server/ServerReceive.cpp:248-254,343-349,418-424,452-458`). The central route warns on the first violation and at disconnect, retains a lifetime counter, and disconnects at the existing threshold (`Documents/Architecture/Network.md:45-53`).

The base assertion path logs and throws `std::runtime_error` (`Common/ErrorUtils.h:17-21`; `Common/ErrorUtils.cpp:14-21`). In ordinary execution without a debugger, an exception that escapes receive processing reaches `engine::HandleException`, which writes the crash report (`Engine/Source/Main.cpp:861-878`; `Engine/Source/CrashReport.cpp:110-164`). The implementation therefore has to verify exception propagation, not merely add an assertion behind an existing catch.

## Design

The author's recommendation is to define corrupt network data at the owning engine-network hub as either a failure of the declared wire layout or bounded payload codec, or an invalid semantic value or relationship required before payload adoption. Semantic corruption includes non-finite vertices and invalid topology or index relationships; it is not limited to malformed byte counts. Structurally valid packets rejected for ordinary protocol state or timing remain outside this definition.

Before editing runtime behavior, build a static inventory of every server-to-client and client-to-server engine and game decoder outcome. For each failure branch, record its detector, whether it is corruption under the definition above, its mutation boundary, and its required response. The author's recommendation is to stop implementation if any failure cannot be classified from repository contracts, then settle that design question through the Tier-3 plan-review route rather than infer a new policy while editing.

On the client, the author's recommendation is to make each corrupt-result branch explicit at its detector. Boolean decode/decompression failures should assert there; `common::CorruptStreamException` from bounded or semantic readers should be converted at the receive boundary to the same assertion failure. Corruption assertions must escape the engine and game packet catches and reach `Main`. Narrow any remaining catches to errors proven by the inventory to be local and recoverable; do not classify every `std::exception` (including `.at()`, allocation, or file-I/O failures) as peer corruption. Treat malformed status-change decompression or batch data as corruption and replace the current zero-change/CRC-resync outcome for network input. Preserve replay-specific corrupt-record handling when the same codec is used outside network receive.

On the server, the author's recommendation is to send every explicit corrupt-result branch and every `common::CorruptStreamException` through the existing `Server::RecordContractViolation` route, then return before handler or state mutation. Preserve that route's current first-violation/disconnect `kWarning` cadence, lifetime counter, disconnect threshold, and last-use rule; the user's requirement does not require per-packet warning spam or a new accounting framework. Narrow generic catches according to the inventory so unrelated local exceptions are not mislabeled as corrupt client data.

Document the resulting policy once in `Engine/Source/Network/AGENTS.md`, replacing its current malformed status-change recovery statement, and align `Documents/Architecture/Network.md` plus only leaf documentation or comments made false by the implementation. Preserve the existing handling of structurally valid stale, reordered, epoch/slot, subscription-state, handshake, rate-limit, and protocol/version outcomes.

## Critical files

- `Engine/Source/Network/AGENTS.md:5-36` — owning transport contracts and current malformed status-change outcome.
- `Engine/Source/Network/Client/Client.cpp:246-310` and `Engine/Source/Network/Client/ClientReceive.cpp:14-38,183-189,405-463,642-665` — client dispatch suppression and representative decode/decompression branches.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:50-87` and `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp:12-91` — client game catches and boolean parser failures.
- `Engine/Source/Network/Server/Server.cpp:188-321,509-586` and `Engine/Source/Network/Server/ServerReceive.cpp:38-48,248-254,343-349,418-424,452-458` — server admission, central violation path, and representative silent decoder failures.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:75-95,230-252` — game admission/handler exception boundary.
- `Engine/Source/Network/NetworkSerialization.cpp:275-385,429-461` — status-change whole-batch decode outcomes.
- `Common/Serialization.h:6-21`, `Common/ErrorUtils.h:17-21`, `Common/ErrorUtils.cpp:14-21`, `Engine/Source/Main.cpp:861-878`, and `Engine/Source/CrashReport.cpp:110-164` — typed corrupt-stream, assertion, top-level exception, and crash-report mechanisms.
- `Engine/Source/Frame/NavCellData.cpp:477-557` — an existing network-carried static-data reader that detects non-finite vertices and invalid topology/index relationships as semantic corruption.
- `Documents/Architecture/Network.md:45-55` — server warning, counting, disconnect, handshake, and rate-limit policy.
- `Tools/AgentHarness/` and the existing network fixture surfaces under `Projects/BrokenEngineSandbox/Source/Agent/` — runtime verification entry points to inspect before adding the smallest needed fixture coverage.

## In scope

- Pre-implementation inventory and classification of every engine/game network decoder failure on both receive directions, including boolean returns, `common::CorruptStreamException`, and current broad exception catches.
- One owning definition and asymmetric response policy in `Engine/Source/Network/AGENTS.md`, with affected architecture, leaf-document, and source-comment synchronization.
- Client corruption detection at the earliest proven validation boundary, assertion conversion, and exception propagation through engine/game dispatch to the existing top-level crash-report path.
- Client network status-change corruption changing from zero-change/CRC-resync to the client-fatal response, while keeping replay corruption on its existing replay-owned path.
- Server corruption detection at the earliest proven validation boundary, central contract-violation accounting, whole-packet drop before mutation, and existing warning/disconnect cadence.
- Semantic payload validation required before adoption, including non-finite vertices and invalid topology/index relationships where the inventory shows those values cross the network boundary.
- Focused harness coverage needed to observe both asymmetric outcomes and preservation cases.

## Out of scope

- Packet layouts, packet identifiers, protocol or Frame versions, serialization field order/width, CRC computation, deterministic simulation results for valid input, or backward-compatibility readers.
- A generic validation/error framework, new exception hierarchy, new server logging cadence, changed violation threshold/counter lifetime, or changed disconnect policy.
- Redefining structurally valid stale/reordered traffic, epoch or slot reuse, subscription state, pre-handshake races, protocol/version mismatch, rate limits, or ordinary resync as corruption.
- Treating every locally thrown `std::exception` as peer corruption, or changing save, replay, `.pack`, and local file-I/O failure policy except where shared code must retain its existing non-network result.
- Implementing unrelated boundary-specific validation gaps already owned by another live Plan, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change alters client/server trust-boundary behavior and exception propagation, and integrates engine transport, game packet handlers, documentation, crash reporting, and runtime fixtures.

Preserve these invariants:

- A corrupt server payload cannot be adopted, resynced past, or consumed after detection; ordinary non-debugger client execution exits through the assertion exception and produces a crash report.
- A corrupt client payload cannot mutate server/game state after detection; the server drops it through existing centralized accounting and remains healthy until the existing threshold policy disconnects that client.
- One detected corrupt packet produces one response at its earliest validation boundary; layered handlers do not double-count or replace the original result.
- Valid packet bytes and valid deterministic state are unchanged, including wire identity, CRC, replay output, and client/server simulation behavior.
- Structurally valid stale/reordered/state/rate-limit cases keep their documented outcomes.

## Acceptance criteria

- Before source edits, a checked inventory maps every engine/game decoder failure in both receive directions to either the corruption response or one named preserved non-corruption outcome; the reviewed implementation and final static search account for every mapped branch.
- A malformed server engine packet and a malformed server game packet each cause an agent-launched, non-debugger client to exit, and each scenario produces a crash-report file containing the assertion failure and originating detector rather than only a warning log.
- A client packet failing byte-layout validation and one failing semantic validation are each dropped before server/game mutation, increment the existing violation counter once, produce the existing first/disconnect warning behavior, and leave the server able to process a later valid packet from a connected client.
- A malformed network status-change compressed envelope or batch takes the client-fatal path instead of becoming an empty update followed by CRC resync; the corresponding malformed replay record retains its replay-owned rejection behavior.
- Representative stale/reordered, epoch/slot, subscription-state, pre-handshake, rate-limit, and resync scenarios retain their documented results.
- Client and server `Debug|x64` builds pass through `/compile`; the applicable propagation, static, C++/comment/style, documentation, Tier-3 adversarial, runtime, and acceptance reviews pass before landing.

## Notes

This is an ordinary deferred policy/correctness Plan with no scheduler dependency. Live Plans such as `Documents/Plans/Engine/FullStateEmbeddedTickValidation.md`, `Documents/Plans/Engine/BlasterTransferTypeIndexValidation.md`, and `Documents/Plans/Engine/CollectionTypeIndexValidation.md` own narrower missing predicates, not this cross-direction response policy; re-derive their status during the inventory and avoid duplicating any still-live detector work. The already completed navigation semantic validation is evidence for the definition only and is not a dependency.
