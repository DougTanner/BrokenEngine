# C++ Correctness Checks

Full text of the checks [`worker.md`](worker.md) indexes. Apply a check only
when a concrete changed path makes its failure reachable. The conventions the
changed code is judged against are in the conventions reference,
[`../../../references/cpp-conventions.md`](../../../references/cpp-conventions.md);
the checks below say what to compare and what to flag.

- [General logic and ownership](#general-logic-and-ownership)
- [Type and domain modeling](#type-and-domain-modeling)
- [Changed comments](#changed-comments)
- [Trust boundaries and failure channels](#trust-boundaries-and-failure-channels)
- [Allocation-tracked paths and logging](#allocation-tracked-paths-and-logging)
- [ASSERT behavior](#assert-behavior)
- [Collections, persistence, and identity](#collections-persistence-and-identity)
- [Determinism, threading, and frame phases](#determinism-threading-and-frame-phases)
- [XMVECTOR W applicability](#xmvector-w-applicability)
- [Integration, layering, and build affinity](#integration-layering-and-build-affinity)
- [Public state and forwarding APIs](#public-state-and-forwarding-apis)
- [Repository patterns](#repository-patterns)
- [Completeness and duplication](#completeness-and-duplication)

### General logic and ownership

Trace initialization, lifetime, aliasing, ranges, conversions, arithmetic,
branch and early-return behavior, RAII cleanup, GPU-resource ownership, and
error-path progress. Judge demanded defensive checks and unvalidated file,
network, OS, user, and third-party data against the trust-boundary convention.

### Type and domain modeling

Report a primitive mixup when a changed path lets a real caller pass the wrong
value, such as adjacent parameters of the same primitive type a reachable caller
swaps; where the layout contract permits, name a focused domain type as the
smallest correction. When the change carries independent discriminators that can
contradict each other, prefer one discriminator that cannot represent the
invalid combination. Boundary validation, enum and variant consumer tracing,
`common::Flags`, and SoA, wire, CRC, layout, and existing mirror contracts stay
with the subsections that own them.

### Changed comments

Treat a changed comment as a correctness issue only when it asserts a runtime
or contract fact that is meaningfully false. Changelog narration, wording,
formatting, and documentation coherence belong outside this review.

Route comments that merely explain a language feature or established house
pattern to `/comment-review`; they are not correctness findings.

### Trust boundaries and failure channels

Require validation before externally controlled sizes, counts, indices, or
payloads can allocate, iterate, index, or change destination state. Preserve
the owning subsystem's established failure channel; there is no universal
"never throw" or "catch everything" policy.

- Network variable payload handlers parse into bounded local state before
  applying destination state. `Client::Receive` (`/Engine/Source/Network/Client/Client.cpp`)
  drops and logs one corrupt inbound packet; `Server::Receive` (`/Engine/Source/Network/Server/Server.cpp`)
  additionally records the contract violation. Keep their handshake, budget,
  and packet-specific policies distinct.
- Invalid persisted grid data follows
  `engine::ReadGridSave` (`/Engine/Source/File/GridSave.cpp`):
  clear partial grid/fleet state, log, and return `false`; apply the global-ID
  counter only after the complete read succeeds.
- Corrupt boot-required eager animation data follows
  `LoadAnimationDataFromEagerChunks` (`/Engine/Source/Graphics/AnimationData.cpp`):
  log the asset identity at `kError` and rethrow to the boot crash-report path.
- For asynchronous per-chunk failures, trace the owning worker's published
  completion and waiter/progress contract before accepting any return, throw,
  or skip path. Runtime `.pack` chunk data halts on corruption
  (`/Engine/Source/File/AGENTS.md`); do not accept a soft-fail, skip, or
  placeholder path there.

### Allocation-tracked paths and logging

Flag reachable heap allocation while Engine/Game tracking is active unless the
existing design requires it and the suppression carries the rationale the
conventions reference requires. Startup, teardown, offline tools, and other
untracked paths do not become findings merely because they allocate.

Follow `GameBase::BuildAndDispatchFrameTicks` (`/Engine/Source/GameBase.cpp`)
for a persistent member rebuilt under `ScopedSuppressAllocationTracking` for
synchronous dispatch. For loop-built dynamic log text, follow the current
`CrcValidateLoop` (`/Engine/Source/Network/Client/ReconcileReplayCrc.cpp`)
`ScopedWorkbufferArena` construction. Verify changed tracked logs use the
allocation-free wrappers and formatters the conventions reference requires,
rather than temporary strings or allocating formatting paths.

### ASSERT behavior

`ASSERT` throws in every configuration. For each added or changed assertion,
compare behavior with the assertion removed and apply the useless-ASSERT
convention, whose preferred fixes are these, in order from best to last resort:

- If the next operation fails immediately at the same location, the assertion
  is useless; require removal or make the invariant impossible at its source.
- If the condition is externally reachable, require the trust boundary's
  existing failure channel, selected from the subsystem policy above rather
  than a blanket return or throw rule. Preserve progress and waiter notification.
- If failure would otherwise silently corrupt state, output, or determinism,
  the assertion may carry real diagnostic value.

Do not accept an `ASSERT` added only to silence an analyzer. Require an
analyzer-visible reachable guard, or a narrow suppression that states the proven
invariant.

### Collections, persistence, and identity

When a `Collection<T>` gains, loses, reorders, or changes a `* __restrict`
column, load the authoritative
`add-collection-member` (`../../add-collection-member/references/worker.md`) skill and verify its
complete live-variant checklist. Do not substitute a copied checklist here.
Treat unresolved CRC membership, tuple order/subset, versioning, unconditional
persistence, creation initialization, transfer, hydration, or identity behavior
as correctness contracts, not style.

For other persisted or wire-visible layouts, trace version/identity updates,
read/write order, bounds, CRC participation, compatibility intent, and every
producer and consumer. Do not infer backward compatibility authority.

### Determinism, threading, and frame phases

For CRC-fed or replay state, check deterministic RNG and math, stable iteration
and reduction order, serialization/CRC coverage, and absence of wall-clock or
platform-dependent decisions. Dispatched workers may write only owned state or
per-thread accumulators; reductions, especially floating-point reductions, must
preserve the declared order.

Keep Interpolate data visual/client-only and PostRender data committed and
deterministic. Verify `AllocateAndCopy()` precedes current-frame access and that
no Update read depends on a later phase write.

### XMVECTOR W applicability

Apply the W rule only when the changed `XMVECTOR` semantically represents a
position (`W=1`), direction/velocity/normal/offset (`W=0`), or color (the
declared alpha). Do not impose those roles on quaternions, planes, matrix rows,
generic four-lane data, masks, or packed payloads; derive their lane contract
from the owning type.

For applicable values, trace every constructor, return, out-parameter path,
early return, arithmetic input, and collection spawn/transfer boundary. Flag
mismatched position subtraction, position-plus-offset construction, inherited
input W where the output role is fixed, and consumer-side stamping that merely
hides a producer error. Accept an explicit post-integration position clamp when
the owning math contract requires it.

### Integration, layering, and build affinity

Search all callers when a signature, unit, default, ownership rule, W role, or
phase changes. Search every switch/dispatch/serialization table for changed
enums and both CPU/GLSL consumers for shared headers. Check client/server and
per-collection mirrors without abstracting deliberate parallel boilerplate.

Engine code may call game hooks and use game globals/types; flag the reverse
ownership leak only when an engine-owned type acquires a game-specific concept.
Prefer the game's own aggregation headers where repository instructions require
them.

Verify guard placement from source and aggregation context. Emit an
`/update-vcxproj` trigger for every added or removed C++ file and every existing
file that gained or lost a whole-file `BT_CLIENT`/`BT_SERVER` guard. Do not
inspect membership or filters here.

### Public state and forwarding APIs

For each changed interface, flag a getter, setter, drain, take, `Is*`, `Can*`,
or equivalent whose entire implementation is one state access, assignment, or
pass-through call when the underlying state or component can be accessed
independently. Require direct public access. Private state is justified only
when one complete multi-statement operation preserves an invariant or required
ordering. Do not apply this check to semantic codecs or serialization adapters.

### Repository patterns

- Flag a struct or function that grew to two or more `bool` members or
  parameters in this change to use `common::Flags<EnumType>` instead (the
  conventions reference). This is a hard flag, not a suggestion.
- Flag a new standard-library or third-party `#include` added to a PCH-backed
  `.h`/`.cpp`; it belongs in `Common/ExternalHeaders.h`, not the individual
  source file (the conventions reference).

### Completeness and duplication

Flag incomplete integration and reachable edge failures. Flag substantial new
near-copies or repeated multi-condition logic only after independent source
inspection proves an existing helper fits. Deliberate client/server and
collection mirrors remain parallel.

Exclude micro-simplifications, style preferences, naming, header placement
other than the required `Common/ExternalHeaders.h` rule above, formatting, and
general documentation checks.
