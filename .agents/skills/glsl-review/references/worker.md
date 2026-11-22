# GLSL Review Worker

Steps and rules for the dispatched reviewer. The public contract — purpose,
triggers, inputs, and the handoff form — lives in [`../SKILL.md`](../SKILL.md).

## Steps

1. Run below with zero disk writes (`-Head` from `Baseline` for a committed
   head; `-UntrackedPath` or `-IncludeUntracked` from `Scope`). Require `status`
   `pass`, else report unavailability without rebuilding the diff. Done when
   output prints or unavailability is reported.

```powershell
pwsh -NoProfile -Command 'Import-Module ./.agents/scripts/SessionChangeInventory.psm1; Get-SessionChangeInventory -RepositoryRoot (Get-Location).Path -Baseline <baseline SHA> -Regions|Select-Object status,coretide,entries,regions|ConvertTo-Json -Depth 5'
```

2. Select every entry whose `class` is `glsl` or `dual-language-header` (those
   rules alone pick shader `.h` files — never restate them) with its matching
   `regions`. Done when in-scope shader files and ranges are listed.
3. Include the transitive shader headers the in-scope files reach. Done when the
   review set holds the in-scope files plus every transitive shader header they
   reach.
4. Read the applicable shader `AGENTS.md`, each changed file, its nearby
   producers/consumers, and the relevant whole function. Done when each of those
   has been read.
5. Search `ShaderFunctions.h` and family `*Common.h` files before recommending
   new helper logic. Done when every helper this review would recommend has been
   checked against those files.
6. Trace every changed shader-facing header in both directions:
   - shader entry points that transitively `#include` it;
   - C++ consumers, including the project `ShaderLayouts.h` wrapper and PCH
     inclusion path;
   - DataPacker dependency capture from preprocessing (`-MD`/`-MF`) through
     dependency fingerprinting.

   Done when every changed shader-facing header has both directions traced.
7. Review changed dual-language declarations against the actual block qualifier
   and CPU representation. Done when every changed dual-language declaration has
   been checked against both.
8. Compare field order, scalar widths, array strides, offsets, descriptor
   constants, writes, and binding roles. Do not infer layout from a generic
   `vec3` rule when `layout(scalar)` applies. Done when each of those attributes
   has been compared.
9. Apply the correctness checks in the footgun reference. Done when every check
   in that reference has been applied to the changed regions.
10. Report only reachable failures supported by the changed code and repository
    evidence. Do not turn a generic checklist item into a finding. Done when
    every recorded finding cites changed code or repository evidence.
11. Emit an external-claim request per `/verify-external-claims`
    ([`../../verify-external-claims/SKILL.md`](../../verify-external-claims/SKILL.md),
    `## Inputs`) covering one single checkable statement for every finding that
    depends on a non-obvious GLSL, Vulkan, extension, device, or compiler claim,
    using only the official source set in
    [`../../verify-external-claims/references/worker.md`](../../verify-external-claims/references/worker.md).
    Do not browse directly. A pending verdict makes the review `NEEDS_ACTION`
    and keeps the dependent finding unconfirmed. Done when every such finding
    carries one single-claim request and the shared status reflects any pending
    verdict.
12. Keep locally provable repository-contract findings separate. Done when no
    locally provable repository-contract finding sits inside an external-claim
    request.
13. For a Tier-2+ change, also run the scope authorization, minimality, and KISS
    passes in
    [`../../../references/scope-authorization.md`](../../../references/scope-authorization.md)
    over the step 2 shader regions. Done when each pass has a verdict for every
    changed shader region, or the change is Tier 1.
14. Report the result in the `Scope:` field. Done when the `Scope:` field
    carries that verdict.
15. Return the report. Done when the handoff carries every section the public
    contract's form requires.
16. A shader-facing shared header has both C++ and GLSL surfaces, so explicitly
    require `/repo-code-review` as the sibling domain review when such a header
    changed; this review does not replace it. Done when the report names
    `/repo-code-review` as required whenever a changed file is a shader-facing
    shared header.
17. Report a changed shader over ~5,000 `bt-token-v1` (measure every changed
    shader in one batched run:
    `pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path 'a','b','c' -Json"`
    — use `-Command`, not `-File`, because under `-File` the comma-separated
    list binds as one filename and the run fails; the single-path
    `-File ... -Path <path>` form still works for one file) as a size
    observation in `Residuals`; splitting it via `ShaderFunctions.h`/`*Common.h`
    is follow-up work, not part of this findings-only review. Record a
    refactor observation in `Residuals` only for a design shape in code the
    review already traced that a refactor would improve;
    never read further code to find one. Done when every changed shader over
    that size has a `Residuals` size observation and every refactor
    observation cites code the review already traced.

## Rules

- Never edit the reviewed files. Delegated-review conduct:
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md).
- Cover correctness and performance, not style. Treat source-adjacent rationale
  as required only when a non-obvious mathematical, numerical, coordinate,
  ordering, layout, or hardware assumption carries correctness or measured
  performance.
- Read the footgun reference ([`shader-footguns.md`](shader-footguns.md)) on
  every review. It holds the correctness checks this review applies: numerical
  domains, coordinates, interpolation and sampling, synchronization and data
  races, subgroup operations, shared CPU/GLSL layout, performance evidence,
  algorithm checks, and optional stages.

### Broken Engine Contracts

- Flag every executable matrix `inverse(...)` call with a `mat3` or `mat4`
  operand/result. Ignore comments and string literals. The repository requires
  CPU-precomputed inverse transforms; inspect the current shared layout and
  upload path rather than relying on a stale line number.
- Require the repository's scalar-layout contract for shared UBO/SSBO data.
  Uniform blocks inherit the global scalar default; storage blocks must acquire
  scalar layout explicitly by block or shader default. Flag introduced
  `std140`/`std430`, unqualified storage blocks, and CPU/GLSL size or order
  mismatches.
- Enforce the descriptor-ownership rule in `Engine/Data/Shaders/AGENTS.md`
  `## Shared Contract`. Verify shared binding constants against C++ layout
  creation and writes.
- Require `nonuniformEXT(index)` when a descriptor-array index may vary between
  invocations. Prove compile-time or dynamically uniform indices before
  exempting them.
- Treat changes to shader-facing shared headers as both C++ and GLSL changes.
  Verify that every affected shader entry point reaches the header through the
  include graph so DataPacker records it in that shader's dependency file.

### Performance Review

- Require evidence before asserting device-, driver-, or compiler-specific cost.
  Accept a capture, target-device limit/property, compiler output,
  SPIR-V/disassembly, or a documented repository production constraint.
  Otherwise request measurement or emit external verification; do not report
  folklore as a defect.
- Inspect hot-path `pow`, transcendental functions, normalization, dependent
  reads, loop bounds, SSBO/image access, workgroup dimensions, bank layout,
  divergence, `discard`, work placed ahead of a `discard`, and depth writes only
  where the changed path is demonstrably hot or contract-significant.
- Flag an expression with at least one uniform or push-constant operand whose
  operands are all uniforms, push constants, spec/compile-time constants, or
  values derived only from those: every invocation computes the identical
  result, so it belongs on the CPU — precompute it into an existing field or a
  new uploaded field (reciprocals follow the existing `*Inverse` naming; the
  `inverse()` ban and pre-normalized `f4SunMoonNormal` are the established
  precedents). A uniform-only sub-expression inside a varying expression
  qualifies too (`uniformA * uniformB * varying` hoists the uniform product).
  This is locally provable from the source; it needs no device or compiler
  evidence. Also flag the redundant twin: re-deriving on the GPU a value the CPU
  already uploads in final form (e.g. `normalize()` of an already-normalized
  uniform). Weigh triviality per the footgun reference before proposing a layout
  change.
- For a positive integer exponent, compare semantics and generated code.
  `x * x * x` contains two multiplications, not one; do not assume how `pow`
  lowers without compiler evidence.
- Do not impose universal subgroup width, workgroup-size, bank-count,
  FP16-throughput, occupancy, or early-Z cost thresholds. Establish the target
  device and evidence first.

### Vulkan and Extension Checks

- Baseline subgroup support is described by
  `VkPhysicalDeviceSubgroupProperties`: verify supported stages and operations.
  Do not require `VK_EXT_subgroup_size_control` merely to use subgroup
  operations; require it only when code requests or depends on subgroup-size
  control/full-subgroup behavior, then verify its features and limits.
- Verify push-constant size/stage ranges, storage image formats, atomics,
  descriptor indexing, stage-specific built-ins, and optional shader extensions
  against the repository's Vulkan 1.2 target and enabled device features.
- Treat debug-only extensions and printf paths according to their actual compile
  guards and shipping configuration.
