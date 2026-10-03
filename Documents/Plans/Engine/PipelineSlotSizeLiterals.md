<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:20:41.060Z","dependsOn":[]} -->
# Use C++23 size_t literals for pipeline slot minima

## Context

Inventory item F001: C++23 size_t literal suffix (`uz`, P0330R8). The user requested executable adoption plans only where adversarial review established real value. At baseline `6c6245f4b1dee82d7721ef271ce4925ca754ff47`, `Engine/Source/Graphics/Objects/PipelineCreator.cpp:636` (`PipelineCreator::CreateGraphicsPipeline`) and `:681` (`PipelineCreator::CreateComputePipeline`, host-visible indirect branch) both initialize `int64_t iCommandBufferCount` with `std::max(uiFramebufferCount, static_cast<size_t>(3))`. Each immediately preceding declaration obtains the framebuffer vector size as `size_t`.

The cast exists only to give the literal the operand type that `std::max` deduction requires. A typed literal makes the minimum count easier to read without conversion syntax; this is a small clarity improvement, with no performance claim. Searches of live Plans for `PipelineCreator`, `3uz`, and size-literal terminology found no overlapping adoption plan when authored.

## Design

The author's recommendation is to replace exactly those two `static_cast<size_t>(3)` operands with `3uz`, retaining `std::max(uiFramebufferCount, 3uz)` and the surrounding declarations and consumers. This keeps the graphics and host-visible compute paths parallel without introducing a helper or named constant.

The unsigned size suffix produces `std::size_t`; both old and new operands therefore have type `size_t` and value 3. The same `std::max` specialization, comparison, selected value, and final conversion to `int64_t` remain. Bare `3` and signed `3z` are unsuitable substitutes because they change the deduction operand type. The selected reference returned by `std::max` is consumed to initialize a value within the same full-expression; no reference escapes the temporary lifetime. See [WG21 P0330R8, section 6.3](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0330r8.html).

Compiler adoption needs no configuration change: the client project already selects `stdcpp23` in its configurations (lines 125, 179, and 248 at the baseline), and [Microsoft documents P0330R8 support in Visual Studio 2022 17.13](https://devblogs.microsoft.com/cppblog/msvc-compiler-updates-in-visual-studio-2022-version-17-13/); this repository uses Visual Studio 2026.

## Critical files

- `Engine/Source/Graphics/Objects/PipelineCreator.cpp`: the two slot-count initializers above; the only implementation edit.
- `Documents/C++StyleGuide.txt`: existing rules 11 (C++ casts), 13 (count/API types), 15 (explicit declarations), and 17 (vector size/index type) suffice; read-only policy verification.
- `Engine/Source/Graphics/Objects/AGENTS.md`, Resource Contracts: existing slot-allocation and resource contracts remain accurate; read-only documentation verification.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`: existing C++23 client configuration; read-only build context.

## In scope

The author's proposed scope is the literal operand in `PipelineCreator::CreateGraphicsPipeline`'s indirect slot-count calculation and the identical operand in `PipelineCreator::CreateComputePipeline`'s `kIndirectHostVisible` branch. Preserve surrounding comments and formatting.

## Out of scope

Other literal conversions, signed-size suffixes, count-type migration, final `int64_t` conversion changes, named constants/helpers, additional headers, feature guards, project membership, shader code, resource sizing policy, and broad modernization. No source additions, unit tests, or documentation/style-guide edits are proposed: the existing numbered rules suffice and no invariant or routing changes need AGENTS.md text.

## Risk and invariants

Future implementation is Change Workflow Tier 1: a local behavior-preserving expression change with no public signature or invariant exposure. Both paths retain the greater of the framebuffer count and three slots; buffer consumers, allocation size/count, mapped-memory lifetime, synchronization, and client-only affinity remain identical. The device-local compute branch stays untouched. No simulation/determinism/CRC, threading, wire, serialization, pack format, save/replay compatibility, or layout surface changes.

## Acceptance criteria

1. The implementation diff contains exactly the two operand replacements in the named functions; no unrelated source, project, comment, or documentation changes.
2. Both initializers read `int64_t iCommandBufferCount = std::max(uiFramebufferCount, 3uz);`, with the preceding `size_t` declarations and all consumers unchanged.
3. Source/type review confirms identical operand type/value, deduction, temporary lifetime, and result conversion; minimum-three allocation remains parallel in both paths.
4. The affected client target builds successfully with its existing C++23 configuration.

## Verification

Inspect the scoped Git diff against criteria 1-3. Run `/compile` for the BrokenEngineSandbox client in `Debug|x64` with its existing data mode and retain the build result for criterion 4. Follow the applicable Tier-1 C++ review/cleanup workflow, including documentation synchronization review; unchanged rules/contracts justify no documentation edits. Literal equivalence and client compilation settle acceptance without a runtime harness, benchmark, server build, or unit tests.

## Notes

This is an author's implementation recommendation, not a record of new user-approved binding design decisions. There are no prerequisites or mandatory coordination constraints. All adoption evidence needed to execute is retained here; no temporary audit or investigation artifact is required. Plan creation performs no implementation or build. Landing any later verified implementation uses `/finalize-changes` when authorized.
