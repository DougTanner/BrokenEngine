<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T12:47:21.955Z","dependsOn":[]} -->
# Fix: Sandbox builds — fail Debug and Profile on the unused-name and RTTI-use warnings behind style guide rules 39 and 43

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` maps every
numbered rule of `Documents/C++StyleGuide.txt` to what enforces it on the agent
Change Workflow path. Two rules have a compiler warning that decides them, but
the warning fails only a Release build:
- Rule 39 (`Documents/C++StyleGuide.txt:169`): an unused variable or parameter
  carries `[[maybe_unused]]`. MSVC reports the violation as C4100
  (unreferenced parameter), C4101 (unreferenced local) and C4189 (local
  initialized but never read) at warning level 4.
- Rule 43 (`:178`): RTTI is disabled. Both Sandbox projects build with
  `<RuntimeTypeInfo>false</RuntimeTypeInfo>`
  (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:131/:185/:252`,
  `BrokenEngineSandboxServer.vcxproj:129/:183/:250`). Under that setting MSVC
  still compiles `dynamic_cast` and `typeid` on a polymorphic type and reports
  C4541 as a warning.

Current tree:
- Both Sandbox projects use `<WarningLevel>Level4</WarningLevel>` in every
  configuration, but set `<TreatWarningAsError>true</TreatWarningAsError>`
  only for Release (`BrokenEngineSandbox.vcxproj:248`,
  `BrokenEngineSandboxServer.vcxproj:246`); Debug and Profile set it false
  (client :127/:181, server :125/:179).
- `/compile` builds Client and Server in Debug unless another configuration
  is requested (`.agents/skills/compile/references/worker.md:117-119`), and its
  handoff reports errors, not warnings (`.agents/skills/compile/SKILL.md:66-68`).
  So on the default agent path neither warning fails anything.
- DataPacker builds Release only with warnings as errors
  (`DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:215`), so its code
  is already covered.
- A search at the audit baseline found no `dynamic_cast` or `typeid` in
  `Common`, `Engine`, `Projects` or `DataPacker`.

## Design
The author's recommendation: in the Debug and Profile `ClCompile` blocks of
both Sandbox projects, promote C4100, C4101, C4189 and C4541 to errors with
`<TreatSpecificWarningsAsErrors>4100;4101;4189;4541;%(TreatSpecificWarningsAsErrors)</TreatSpecificWarningsAsErrors>`,
keeping the two projects mirrored as
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:17`
requires. Add the four codes and their rule numbers to that AGENTS.md
Compiler bullet.

If a Debug or Profile build then reports pre-existing C4100, C4101 or C4189
sites, fix each with `[[maybe_unused]]`, rule 39's own form. If it reports a
pre-existing C4541 site, stop and return it for re-planning.

Rationale: the compiler already decides both rules exactly; promoting the
warnings makes the default agent build fail on them, with no review judgment.

## Critical files
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`

## In scope
- The Debug and Profile `ClCompile` blocks of both Sandbox vcxproj files:
  one `TreatSpecificWarningsAsErrors` element each
- The Compiler bullet in the VS2026 `AGENTS.md`
- `[[maybe_unused]]` at any pre-existing C4100, C4101 or C4189 site those
  builds report

## Out of scope
- Release configurations, DataPacker, and the Tools projects
- `WarningLevel`, `TreatWarningAsError`, Clang-Tidy and code-analysis
  settings
- Any other warning code; `.clang-tidy`; `Documents/C++StyleGuide.txt`
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped build behavior): trigger is a compile-setting change in the two
mirrored Sandbox projects. Escalate to Tier 3 if the change reaches
ThirdParty provisioning, DataPacker export, or other bootstrap coordination.
Both vcxproj files stay mirrored. Never embed transcript paths or home paths.

## Acceptance criteria
- `/compile` Client and Server builds pass in Debug, Profile and Release
- With a temporary, uncommitted scratch edit adding an unused local to a
  tracked Sandbox `.cpp`, a Client Debug build fails with C4189; with a scratch
  `dynamic_cast` on a polymorphic type, it fails with C4541; revert the
  scratch edits afterwards

## Notes
Originating record:
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`.
