<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T17:59:25.189Z","dependsOn":[]} -->
# Name the ACK-stream contract initializer fields

## Context

`Engine/Source/Network/NetworkProtocol.h:174`, `GetClientPacketContract`, initializes the `kClientAcknowledgmentStream` row with five positional values. The last two are `true, false`, which require consulting `ClientPacketContract` at lines 158–165 to identify the handshake and over-cap violation policies. This is a narrowly useful clarity refactor authorized by the C++20 adoption review, not a runtime defect or a reason to convert every aggregate initializer.

`Engine/Source/Network/Server/Server.cpp:293` consumes `bRequiresHandshake` for pre-handshake drops; lines 299–306 consume `iMaximumPerTick` and `bOverCapCountsViolation` for capped traffic. Naming the fields makes the ACK exception visible at its owning declaration. The neighboring Hello row at `NetworkProtocol.h:177` already uses designated initialization.

## Design

The author recommends changing only the ACK row's return initializer to the following member/value mapping, in declaration order:

```cpp
return {.iMinimumSize = NetworkMessages::ClientAckStreamMessage::kiFixedSize, .iMaximumSize = kiMaximumAcknowledgmentStreamPacketSize, .iMaximumPerTick = 128, .bRequiresHandshake = true, .bOverCapCountsViolation = false};
```

Retain all five explicit expressions and the existing trailing explanation. Do not rely on a default for either boolean. This exposes the policy without introducing a helper, type, or new abstraction.

`ClientPacketContract` is a public scalar aggregate with no constructors or bases. C++20 draft N4861 [dcl.init.aggr] paragraphs 2–4 associate positional or designated expressions with aggregate members, and paragraph 6 specifies member-order evaluation ([draft text](https://timsong-cpp.github.io/cppwp/n4861/dcl.init.aggr)). The proposed mapping initializes the same members from the same constant expressions in the same order. It introduces no calls, branches, allocation, lookup, storage, or extra evaluation. This establishes semantic preservation; it makes no measured speedup or byte-identical code-generation claim.

Risk tier: **Tier 1**, local mechanical style change with no signature or invariant change. Although the declaration owns a network admission policy, its values, resulting admission decisions, wire format, and trust boundary remain identical. The concrete implementation risk is misassigning a value or dropping explicit `false`; verify the complete mapping.

## Critical files

- `Engine/Source/Network/NetworkProtocol.h:GetClientPacketContract` — the sole implementation edit, specifically `PacketType::kClientAcknowledgmentStream`.
- `Engine/Source/Network/NetworkProtocol.h:ClientPacketContract` — read-only member-order and default reference.
- `Engine/Source/Network/Server/Server.cpp:Server::Receive` — read-only consumer evidence for unchanged handshake and over-cap handling.
- `Documents/C++StyleGuide.txt:42` and `Engine/Source/Network/AGENTS.md` `## Transport Contracts` — existing style and contract owners; no planned edits.

## In scope

Replace only the five positional initializer entries in `GetClientPacketContract`'s `kClientAcknowledgmentStream` return with their corresponding designated entries. Preserve its expressions, explicitness, field order, and existing comment.

## Out of scope

All other switch cases and defaults; the `ClientPacketContract` declaration; consumers, constants, caps, policies, protocol versions, wire/data layouts, simulation/CRC, serialization, save/replay, threading, and allocation behavior. No game contract or graphics conversion, helper/table/type redesign, style sweep, unit tests, or runtime experiment.

## Acceptance criteria

1. The ACK return explicitly names all five members and retains exactly the mapping shown above, including `.bRequiresHandshake = true` and `.bOverCapCountsViolation = false`.
2. The source diff changes only that initializer; neighboring/default returns, aggregate member defaults, constants, and consumers are unchanged.
3. Required client and server compile verification through `/compile` succeeds; record actual target/configuration and results during implementation.

## Verification

Review the diff against the aggregate declaration and the two `Server::Receive` gates to establish equal field values and admission behavior. Apply the required C++ style/correctness/comment review workflow to the changed range. Build the BrokenEngineSandbox client and server via `/compile` because the shared protocol header participates in both targets. No `/agent-harness` exercise or benchmark is warranted for this constant-expression substitution; no unit tests are added.

## Coordination

No dependencies or mandatory coordination constraints. Current Plan searches by `ClientPacketContract`, `GetClientPacketContract`, `NetworkProtocol.h`, designated initialization, ACK-stream policy, and Coordination found no competing implementation boundary.

## Notes

`Documents/C++StyleGuide.txt` rule 42 already requires designated struct initializers; no new rule is needed. `Engine/Source/Network/AGENTS.md` `## Transport Contracts` and `Documents/Architecture/Network.md` `## Client → Server Contract` already describe the unchanged contract ownership and behavior. Run the required `/update-claude-docs` assessment during implementation, with no documentation edit expected. This plan records verified source evidence only; no compile or runtime verification has been performed while authoring it.
