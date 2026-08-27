<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:24.891Z","dependsOn":[]} -->
# Reject duplicate live client GUIDs

## Context

The retained survivor `CAI/shard-0037/002` identifies an ownership-identity
gap. `Server::ClientHello` accepts any nonempty wire GUID and assigns it before
acceptance at `Engine/Source/Network/Server/ServerReceive.cpp:305-340`, without
checking other live peers. `ServerClientManager::RelinkFromFrames` then adds
all frame players with that GUID to the new peer's `OwnedEntityRegistry`
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:40-72`;
`ServerSession.cpp:429-461`), while `ServerFleetManager::OnClientConnected`
overwrites the single `mGuidToClientId` entry at `:369-390`. The broadcaster
authorizes player updates from that registry (`Engine/Source/Network/Server/ServerBroadcaster.cpp:232-290`).

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0037.md:85`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:966`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this
routing session has not changed source. Copying a valid persistent GUID into a
second client's normal Hello is sufficient; no malformed packet is needed.

## Design

The author's recommendation is to reject a nonempty GUID already attached to a
live handshaken peer before setting the new peer accepted or invoking game
relink. Send the existing handshake rejection and remove only the new peer;
do not silently implement takeover. Preserve server-generated GUIDs for empty
inputs and repeated Hello behavior on the same peer, and keep the one-to-one
GUID-to-client mapping authoritative.

## Critical files

- `Engine/Source/Network/Server/ServerReceive.cpp:305-340` — Hello GUID
  acceptance and handshake state.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:40-72`
  and `ServerSession.cpp:429-461` — ownership relink.
- `Engine/Source/Network/Server/OwnedEntityRegistry.cpp:12-20` — paired
  ownership/coordinate records.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:369-400`
  and `Engine/Source/Network/Server/ServerBroadcaster.cpp:232-290` — GUID map
  and authorization consumers.
- `Engine/Source/Network/Server/AGENTS.md` and
  `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` — identity
  and ownership contracts.

## In scope

- Duplicate nonempty GUID detection among live handshaken peers before
  acceptance and relink.
- Rejection/removal of the second peer without mutating the existing peer's
  mapping or registry.
- Empty-GUID generation and same-peer repeated-Hello behavior.

## Out of scope

- Persistent GUID file format, authentication, takeover semantics, or a new
  identity service.
- Ownership relink ordering for unique GUIDs and fleet policy unrelated to a
  collision.
- Client UI, discovery, and packet layout changes.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: untrusted
handshake identity crosses server authorization, persistent fleet mapping, and
owned-entity relinking.

Preserve these invariants:

- One live handshaken peer owns each nonempty `ClientGuid`.
- `mGuidToClientId` and each `OwnedEntityRegistry` remain index/identity
  coherent for the accepted peer.
- A rejected duplicate cannot authorize a player update or mutate fleet state.

Tier rationale: the change is one pre-specified duplicate-GUID lookup among
live handshaken peers inside `ClientHello`, refusing the second peer through
the existing handshake rejection. No packet layout, persistent GUID format, or
relink ordering for unique GUIDs changes, so only an already-invalid handshake
takes a new path.

## Acceptance criteria

- A second valid Hello carrying an existing live GUID is rejected before
  acceptance, relink, or `mGuidToClientId` overwrite.
- The original peer retains its owned registry and fleet mapping; disconnecting
  the rejected peer does not clear the original mapping.
- Empty GUIDs still receive unique server-generated identities and a same-peer
  repeated Hello still preserves its existing GUID.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0037/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:966`. No source fix or build
was performed during routing.
