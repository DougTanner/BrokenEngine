# Scheduler hash-failure isolation and deterministic verification seam

Status: Open investigation; no implementation decision has been made.

Area: Agents / WorktreeCli

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPT/shard-0060/001` in the frozen C++ Plan Trace Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding under investigation

`coordination::HashSha256` returns `std::nullopt` when BCrypt provider opening,
property lookup, hash creation/data, or finalization fails
(`Tools/ToolCommon/CoordinationStore.cpp:141-183`). `MakeLocator` preserves
that failure and refuses to construct a coordination path
(`CoordinationStore.cpp:271-284`). The scheduler does not mirror that
fail-closed behavior: `SchedulerRoot` converts a failed repository hash to the
literal `invalid`, and `ClaimPath` does the same for every normalized Plan path
(`Tools/WorktreeCli/PlanScheduler.cpp:214-228`).

`RunClaimNext` obtains that root, creates the scheduler guard, heals records,
and scans/writes claims (`PlanScheduler.cpp:692-804`). `HealClaims` can delete
a record when validation fails against the current repository or when the
claim filename does not match `ClaimPath` (`:286-323`). A provider failure can
therefore collapse distinct repositories into one `invalid` root and all Plans
into `invalid.json`, allowing contention or foreign-record healing in a
namespace that the scheduler contract says must be Git-common-directory
keyed. The same root/path helpers are used by status and list paths
(`PlanScheduler.cpp:522,628-644,817-929`), so the correction must cover every
caller rather than only `claim-next`.

The original route's acceptance criterion also required a forced
`HashSha256` failure. The frozen repository has no deterministic injection
seam or hook for that operation: a repository-wide search finds only the
production `HashSha256` definition, the scheduler/locator call sites, and no
failure-injection environment variable, CLI switch, provider callback, or
scheduler fixture control. Forcing a BCrypt failure through the operating
system would not be a deterministic repository acceptance signal. The durable
source trace proves the isolation defect, while the missing seam leaves both
the correction shape and the decisive verification mechanism open.

## Controlling contract and invariant

`Tools/WorktreeCli/AGENTS.md:18,24-28` requires scheduler claims to be keyed by
the Git common directory, binds each record to its repository and Plan, and
allows invalid/expired/orphaned records to self-heal only within the serialized
scheduler guard. `Tools/ToolCommon/AGENTS.md:3-11` owns the shared hash and
coordination primitives; its fail-closed `MakeLocator` path is the closest
existing mirror.

The unresolved invariant is that a hash-provider failure may not alias a valid
or another failed repository or Plan namespace, or mutate the wrong store. The
future verification must prove that invariant under a deterministically
induced failure without changing the normal successful hash, claim schema, or
scheduler selection behavior.

## Boundary and impact

The open boundary is `SchedulerRoot`, `ClaimPath`, and all scheduler callers
that need repository or Plan hashes, plus the test/fixture boundary used to
force a hash failure. It includes validation, list, claim-next, claim-status,
unclaim, complete, and reject paths wherever they create or inspect scheduler
storage. It excludes BCrypt algorithm/digest changes, landing-lock hashing,
Plan metadata/selection, claim schema compatibility, unrelated filesystem
errors, and game/runtime state.

On the current failure path, one repository can acquire a guard in a shared
fallback root, see a peer's claim as occupied, or delete it as invalid. A
correction that merely changes the fallback string or adds a later check still
risks creating or inspecting the wrong namespace. A verification mechanism
that relies on a machine-specific BCrypt fault cannot settle the fail-closed
invariant reliably.

## Open choices

These alternatives are recorded for a future decision; none is selected here.

### Isolation correction

1. **Checked optional path propagation.** Preserve the existing optional hash
   failure through `SchedulerRoot` and `ClaimPath`, make each caller return its
   existing unavailable-store failure before it creates a guard or reads claim
   records, and keep all successful paths unchanged. Define how `HealClaims`
   and filename validation report an unavailable Plan hash without interpreting
   it as a malformed claim.
2. **Typed scheduler-path result.** Introduce an operation-level result that
   distinguishes a valid path from an unavailable coordination store, so no
   caller can obtain a filesystem path from a failed hash. Define the smallest
   shared result surface and ensure validation/list/status and mutating
   operations all stop before storage access. This option must not add a
   compatibility namespace or change the existing success envelope.
3. **Collision-proof error namespace.** Represent an unavailable repository or
   Plan hash with a distinct, collision-proof error namespace that cannot
   collide with any valid or other failed repository/Plan key. Define the guard,
   read, healing, and publication behavior so a failure cannot mutate the wrong
   store, and provide evidence that this shape preserves all scheduler
   contracts.

All correction shapes must remove the literal scheduler fallback and prove that
repository and Plan keys cannot alias on failure. Checked propagation and a
typed result provide fail-before-access candidates; a collision-proof error
namespace or another proven isolation shape remains admissible only if its
evidence settles the same no-alias/no-wrong-store invariant. No selected
behavior or API shape is recorded here.

### Deterministic verification seam

1. **Test-only hash-provider seam.** Let the scheduler fixture replace or wrap
   the hash operation for one scoped invocation and return failure for the
   repository hash and Plan hash independently. Keep the seam out of normal
   production behavior and define how the fixture proves no guard, namespace,
   claim scan, or peer deletion occurred.
2. **Fixture-bound coordination dependency.** Route scheduler hashing through a
   fixture-owned coordination dependency or executable test double that can
   deterministically return `nullopt`, while retaining the production BCrypt
   implementation unchanged. Define the build/project membership and ensure
   the fixture exercises the real WorktreeCli command path rather than a copied
   algorithm.
3. **Explicit fault-control input.** Add a narrowly scoped environment or CLI
   fault control understood by the scheduler fixture and rejected or absent in
   ordinary use. This has a larger trust-boundary and interface cost, so its
   necessity and cleanup must be demonstrated before it can enter a future
   Plan; a machine-level BCrypt fault alone is not sufficient evidence.

The future Plan may select another deterministic seam only if it identifies
the real command boundary, keeps production success behavior unchanged, and
proves both repository-hash and Plan-hash failure paths.

## Decisive questions and acceptance evidence

- For validation, list, claim-next, claim-status, unclaim, complete, and
  reject, what externally visible failure/result or isolated-namespace behavior
  should the chosen mechanism provide when a required repository or Plan hash
  is unavailable?
- For the chosen isolation shape, how should `EnsureParentDirectory`, guard
  creation, storage reads, `HealClaims`, `ReadClaim`, peer-record deletion, and
  claim publication behave after a repository or Plan hash failure? For checked
  propagation or a typed result, which exact callers need a checked result, and
  does `ClaimPath` used by healing have a failure representation that cannot be
  mistaken for a filename? If a collision-proof error namespace or another
  shape is considered, what proves it cannot alias a valid or other failed
  repository/Plan namespace or mutate the wrong store, and how does that
  evidence compare with explicit propagation that fails before storage access?
- Which deterministic seam is allowed by the scheduler and ToolCommon
  ownership boundaries, and can it force repository-hash and Plan-hash
  failures independently in the real fixture command?
- Under each forced failure, what externally visible failure/result or
  isolated-namespace behavior should the command expose, and how should guard
  creation, storage reads, healing, and claim publication behave? Does the
  future evidence prove the selected behavior, ensure no alias with a valid or
  other failed repository/Plan namespace (including via the current `invalid`
  fallback), prevent wrong-store mutation or peer-record deletion, and leave
  coordination bytes outside the selected isolated namespace intact? Any
  mutation inside that namespace must match the eventually chosen behavior and
  must not mutate or delete a valid or peer namespace. If a collision-proof
  error namespace is used, how does independent evidence compare its isolation
  with explicit propagation that fails before storage access?
- Do successful scheduler validation/list/claim/heal operations still use
  distinct Git-common-directory and normalized-Plan namespaces, preserve the
  claim schema and expiry/orphan healing, and pass the applicable WorktreeCli
  build and fixture checks?

The eventual executable Plan must choose the correction shape and the
deterministic verification seam, name every affected scheduler operation and
result envelope, and bind normal and failure-isolation acceptance evidence.
Expected future work is Tier 3 because it changes WorktreeCli's cross-session
coordination trust boundary and can affect other sessions' claims. Until both
open decisions are made, no source or fixture change is authorized.

## Provenance

- Frozen source candidate: `CPT/shard-0060/001`.
- Frozen consolidated index: `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md`.
- Durable source evidence: `Tools/ToolCommon/CoordinationStore.cpp:141-183,271-284`, `Tools/WorktreeCli/PlanScheduler.cpp:214-228,286-323,522,628-644,692-804,817-929`, and `Tools/WorktreeCli/AGENTS.md:18,24-28`.
- Repository search evidence: no failure-injection seam was found for
  `HashSha256` or the WorktreeCli scheduler fixture; only the production hash
  implementation and its normal consumers were present in the frozen tree.
- The route was reclassified after adversarial review because the original
  forced-failure acceptance was not decision-complete without an approved
  deterministic seam. No source, coordination state, or scheduler change is
  part of this investigation.
