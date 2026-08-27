<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:32:10.715Z","dependsOn":[]} -->
# Fix: Keep full Graphics teardown creation-free

## Context

The accepted survivor `CAI/shard-0027/003` shows that
`Graphics::~Graphics` promotes the destroy tier to `kSurface`, but
`Graphics::Destroy` still calls `RecreateResources` and recreates samplers for
that tier (`Engine/Source/Graphics/Graphics.cpp:142-150,662-711`).  Those calls
can issue Vulkan creation and descriptor writes against the old or already
lost device.  Device-loss recovery then cannot reach the replacement
`Graphics` constructor (`Engine/Source/Main.cpp:422-433`).

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0027.md:95`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:791`.
All 14 frozen target rows matched and the report found ordinary partial
recreation otherwise coherent.  The defect is pre-existing at frozen commit
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source changes were made in
this routing session.

Impact: a supported device-loss event can throw again from the old Graphics
destructor while it is unwinding, terminating the client instead of allowing
in-place graphics recovery.

## Design

Author's recommendation: make the full `kSurface` branch strictly
destruction-only.  Guard `RecreateResources`, sampler recreation, and the
associated descriptor rewrites so they run only when
`meDestroyType < DestroyType::kSurface`; retain all current partial-tier
ordering and clear destroy flags before returning.  The replacement Graphics
constructor remains the sole owner of resource creation after a full teardown.

## Critical files

- `Engine/Source/Graphics/Graphics.cpp:598-711` — resource recreation and
  destroy-tier branches.
- `Engine/Source/Graphics/Graphics.cpp:713-779` — partial/full manager
  destruction ordering.
- `Engine/Source/Graphics/Graphics.h` — destroy-tier state and comments if the
  contract wording needs correction.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:319-462` — sampler
  creation called by the failing full path.

## In scope

- Preventing `RecreateResources`, `TextureManager::CreateSamplers`, and global
  or per-pipeline sampler descriptor writes from executing at
  `DestroyType::kSurface`.
- Preserving the existing worker/device drain before old-resource destruction,
  full manager reset, and replacement-constructor resource creation.
- Updating only comments needed to make the full-versus-partial tier contract
  accurate.

## Out of scope

- The separate one-shot constructor latch failure, texture registration replay,
  or any other device-loss candidate.
- Vulkan error-policy changes, device-loss retry policy, shutdown UI behavior,
  or a new Graphics ownership abstraction.
- Partial swapchain/sampler/pipeline recreation behavior when the device is
  healthy.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: this is
device-loss recovery across Graphics managers, Vulkan resource lifetime, worker
drains, and replacement device creation.

Tier rationale: the Design fully specifies one guard condition
(`meDestroyType < DestroyType::kSurface`) around the recreation and sampler
calls in a single teardown function, a local recovery-ordering correction
inside the Graphics subsystem. Healthy partial tiers and every other code path
keep their current behavior.

Preserve these invariants:

- Full teardown waits workers and destroys old resources without issuing any new
  Vulkan resource creation or descriptor publication.
- Healthy partial tiers still recreate exactly the resources selected by their
  existing destroy flags and preserve manager ownership order.
- A replacement Graphics object owns all resources created after a surface or
  device loss; no old-device handle is reused.
- No deterministic simulation, wire, save, replay, or `.pack` layout changes.

## Acceptance criteria

- A code trace and targeted fault/recovery scenario show that a `kSurface`
  `Graphics::Destroy` path performs no `Create*`, sampler creation, or
  descriptor rewrite call before the old managers are reset.
- A healthy sampler/pipeline/swapchain setting recreation still performs its
  required partial resource rebuild and renders a frame.
- Client Debug and Release builds pass `/compile`; an `/agent-harness`
  device-loss/recreate scenario, where the harness can expose it, reaches the
  replacement Graphics instance without a destructor exception.
- The destroy flags/tier are cleared exactly as before, and no new retry or
  compatibility path is added.

## Notes

The existing comment at `Graphics.cpp:692` claims the intended partial-only
behavior; the implementation must make that documented boundary true rather
than broaden teardown error handling.
