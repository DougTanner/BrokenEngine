<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T19:42:16.929Z","dependsOn":[]} -->
# FrameInput Version Rule: Scope the Mandatory Bump to Server-Visible Layout

## Context

`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md:19` states the rule as a
blanket obligation: "bump `FrameInput::kiVersion` on any change to the stream
format or `StatusChange` payloads (replays validate the version; size isn't
checked for non-trivially-copyable types, so the bump is the only guard)."

A class of change that rule mis-covers was proven during the session
implementing `Documents/Plans/Engine/MissileSmokeTrailCarryReachability.md`
Branch B: removing a `StatusChange` payload member that exists only under
`BT_CLIENT` changes no persisted replay byte, because

- replay record and playback are server-only — `Engine/Source/File/Replay.cpp`
  is wrapped in `#if defined(BT_SERVER)` at `:3`, and it is the only translation
  unit that streams `FrameInput` to or from a file;
- `FrameInput`'s stream operators write each `StatusChange` payload through
  `common::Write` (`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp:20-31`),
  which emits the raw object representation of the payload struct
  (`Common/Serialization.h:117-122`) — so the bytes written are exactly the
  server build's layout, and a member the server build does not have cannot
  contribute any;
- the network path is gated separately by `engine::kuiProtocolVersion` and
  writes fixed wire sizes rather than the struct's raw layout, so it is
  unaffected by this distinction.

The session's user, holding implementation authority, explicitly decided not to
bump `FrameInput::kiVersion` for that client-only removal, and an adversarial
review found no reachable incompatibility. Under the repository authority order
an explicit user statement outranks documentation, so the rule text is now the
side that is wrong, and it currently reads as though that landed change skipped
a mandatory step.

## Design

Recommended: narrow the mandatory bump in that one sentence to changes visible
in the server build's serialized layout, and say why the client-only case is
exempt, so a future reader reaches the same answer without re-deriving it.

Recommended replacement for the first clause of `AGENTS.md:19`, keeping the rest
of the bullet — the difference-stream sentence and the `Crc()` sentence —
unchanged:

> `FrameInput` serialization: layout is versioned — bump `FrameInput::kiVersion`
> on any change to the stream format or to the `StatusChange` payload bytes a
> server build writes (replays validate the version; size isn't checked for
> non-trivially-copyable types, so the bump is the only guard). Replay record
> and playback are server-only and persist each payload's raw object
> representation, so a member that exists only under `BT_CLIENT` contributes no
> persisted byte and its addition or removal takes no bump; a wire-visible
> change still takes the separate `engine::kuiProtocolVersion` bump owned by
> `../Network/AGENTS.md`.

The alternative — leaving the blanket rule and instead recording the
client-only case as a one-off exception elsewhere — is rejected by the author
because the fact would then live in two places and the owning sentence would
still read as contradicted by the landed change.

Verified at Plan-authoring time: `Projects/BrokenEngineSandbox/Source/Frame/
AGENTS.md:19` is the only place that states the bump obligation.
`Projects/BrokenEngineSandbox/Source/Network/AGENTS.md:16` and
`Documents/Architecture/Network.md:78` only distinguish the three version gates
from one another and need no change, so no second document has to be kept in
step.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`

## In scope

- The `FrameInput` serialization bullet at
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md:19`, rewritten as above.

## Out of scope

- Every other invariant in that file, and every other `AGENTS.md`.
- The value of `FrameInput::kiVersion`, `Frame::kiVersion`, and
  `engine::kuiProtocolVersion`, and any code change whatsoever.
- The replay reader's trust-boundary rules, which the neighbouring bullet at
  `:20` owns.
- Adding a runtime or build-time check that would make the raw-size mismatch
  detectable; that would be new behavior, not a documentation refinement.

## Risk tier and invariants

Tier 1. Trigger: documentation-only wording, no public signature and no
invariant exposure. The refinement must not weaken the two obligations that
remain real — a bump for any change to bytes a server build writes, and the
separate protocol-version bump for wire-visible changes.

## Acceptance criteria

- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md:19` no longer mandates a
  bump for a change that provably alters no server-written byte, and still
  mandates one for changes that do.
- No other tracked document states the bump obligation in a form the refinement
  contradicts.

## Notes

Source: proven residual from the session implementing
`Documents/Plans/Engine/MissileSmokeTrailCarryReachability.md` Branch B, where
the user's explicit no-bump decision and the blanket rule disagreed. Recorded
under `Engine/` because `Documents/Plans` has no game-frame area and the script
creates none; the change itself is in the game frame documentation.
