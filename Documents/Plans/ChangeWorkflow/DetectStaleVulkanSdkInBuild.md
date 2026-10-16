<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T00:07:00.173Z","dependsOn":[]} -->
# Fix: /compile — a BrokenEngineSandbox build silently compiles against a stale inherited Vulkan SDK and links a ThirdParty prebuilt built against a newer one

## Context
Observed symptom: the machine's Vulkan SDK was upgraded on 2026-10-15 (commit
3d483d16 moved the repository to Vulkan SDK 1.4.363.0, which `README.md`
names). The machine-level `VK_SDK_PATH` and `VULKAN_SDK` point at
`C:\SDK\VulkanSDK\1.4.363.0`, but the agent host process started before the
upgrade and its environment still carried `C:\SDK\VulkanSDK\1.4.341.1`. Every
child it launched inherited that stale value.

- The primary checkout's
  `ThirdParty/Prebuilts/Platforms/VisualStudio2026/Output/ThirdParty.Debug.lib`,
  rebuilt by wrapper session start
  (`.agents/scripts/Bootstrap-AgentTools.ps1`) in a process that already saw the
  new SDK, was compiled against 1.4.363.0. Its debug strings name only
  `VulkanSDK\1.4.363.0\Include\vma\vk_mem_alloc.h`.
- A `/compile` client build of BrokenEngineSandbox Debug
  (`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` through the
  WorktreeCli MSBuild driver, `Tools/WorktreeCli/BuildCommand.cpp`) compiled
  engine code against the 1.4.341.1 headers. Its `CL.command.1.tlog` held 225
  references to `VulkanSDK\1.4.341.1` and none to 1.4.363.0. The build returned
  status success with no diagnostics. The project files take the SDK only from
  the inherited `$(VK_SDK_PATH)`
  (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `BrokenEngineSandboxServer.vcxproj` `AdditionalIncludeDirectories` and
  `AdditionalLibraryDirectories`, and
  `ThirdParty/Prebuilts/Platforms/VisualStudio2026/ThirdParty.vcxproj`). Nothing
  in the build compares the consumer's SDK with the SDK the linked prebuilt was
  built with.
- The client log reported "Compiled with Vulkan SDK version: 341". The only
  existing guard, `static_assert(VK_HEADER_VERSION >= 341, ...)` at
  `Engine/Source/Main.cpp:96` (client only), still names the pre-upgrade
  minimum, so it passed.
- Consequence: an ABI mismatch. VMA 1.4.363.0's `VmaVulkanFunctions` adds a
  trailing `vkGetPhysicalDeviceProperties2KHR` member that 1.4.341.1 lacks.
  `engine::DeviceManager` fills the shorter struct (member `mVmaFunctions`,
  `Engine/Source/Graphics/Managers/DeviceManager.h:40`; filled and passed to
  `vmaCreateAllocator` at
  `Engine/Source/Graphics/Managers/DeviceManager.cpp:291-304`). The library reads
  past it and calls a garbage pointer at `vk_mem_alloc.h:13446`, which is an
  access violation in `vmaCreateAllocator` at client startup. A client-only
  `/agent-harness` launch reproduced it (`check.unexpected-exit`, "Access
  Violation"; client log and crash report kept in the session's gitignored
  `Temp/` output). The server build carries the same mismatch; no harm was
  proven there.
- Rework forced: one `/external-diagnose-bug` dispatch to find the cause. The
  active change's live client and server CRC acceptance check went unrun, and the
  user waived it.

Wanted outcome: when a build's engine Vulkan headers differ from the SDK the
linked ThirdParty prebuilt was built with, the build fails loudly, or it uses
the matching SDK, instead of returning success with a binary that crashes.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 85dacfb8-ec3f-4ee8-b36c-85d8323cafe4
- Worktree/branch UUID: 9911877c-3c84-43d0-86ed-10313b26d44b
- Session branch: claude/9911877c-3c84-43d0-86ed-10313b26d44b
- Worktree: .claude/worktrees/BrokenEngine/9911877c-3c84-43d0-86ed-10313b26d44b
- Landing ref: claude/9911877c-3c84-43d0-86ed-10313b26d44b (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/DetectStaleVulkanSdkInBuild.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, using the landing ref
and supplying client `claude` and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The fix site is a choice for the fix session. Candidate mechanisms:

- The author recommends a check in `Invoke-CompileBuild.ps1` before MSBuild
  runs. When the process `VK_SDK_PATH` or `VULKAN_SDK` differs from the
  machine-level or user-level value in the registry, the build fails under a
  typed, documented block code that names both paths. A process that started
  before an SDK upgrade is the observed cause, and this check catches it in the
  one script every agent build passes through, without touching the shared
  prebuilt. The alternative of passing the registry value to the MSBuild child
  in silence would hide which SDK was used. It is not recommended.
- A stronger option checks the real version pair. Bootstrap records the
  `VK_HEADER_VERSION` or SDK path it built ThirdParty with next to the primary
  `Output/` libraries, and the compile script or the consumer PreBuildEvent
  compares it with the consumer's SDK. This also catches a prebuilt that is
  older than the consumer. It reaches build/bootstrap coordination (see the
  risk tier below).
- Bumping the `Engine/Source/Main.cpp:96` minimum to 363, to match `README.md`,
  would have stopped this one build at compile time. It covers the client only
  and only one direction, so it supplements a real check and does not replace
  one.

## Critical files
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`
- `.agents/skills/compile/references/worker.md` (only to document a new block
  code)
- `.agents/scripts/Bootstrap-AgentTools.ps1` (only if the version-stamp option
  is chosen)
- `Engine/Source/Main.cpp` (the `VK_HEADER_VERSION` minimum only)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the
  pre-MSBuild steps of `Invoke-CompileBuild.ps1`, the block-code list in the
  compile `worker.md`, the ThirdParty rebuild step of
  `Bootstrap-AgentTools.ps1`, and the `static_assert` at `Engine/Source/Main.cpp:96`

## Out of scope
- The landed change the session produced
- `engine::DeviceManager` and the VMA wrapper `ThirdParty/Prebuilts/Source/Engine/Vma.cpp`;
  the crash is a symptom of the mismatch, not a defect in them
- Changing the vcxproj SDK include and library paths, or adding a shared
  `.props` sheet
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2, triggered by scoped tool behavior: one new pre-build check in
the compile script. Escalate to Tier 3 if the fix writes or reads a stamp
produced by `Bootstrap-AgentTools.ps1`. That is build/bootstrap coordination
that can block other sessions, because a false positive would refuse every
session's build. A check must not fire when the process and registry SDK values
agree. Never embed transcript paths or home paths.

## Acceptance criteria
- With the process `VK_SDK_PATH` naming a different SDK than the one the
  primary ThirdParty prebuilt was built with, a `/compile` BrokenEngineSandbox
  build no longer returns success with mismatched headers. It either fails with
  a diagnostic naming both SDKs or builds against the matching SDK.
- With matching SDK values, client and server builds succeed unchanged.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
