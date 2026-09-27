<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T23:03:55.560Z","dependsOn":[]} -->
# Fix: style rule 47 — deterministic include-order script, folder-first rule text, and repository re-sort

## Context

`Documents/C++StyleGuide.txt` rule 47 (:184-197) orders includes as corresponding
header, engine group, game group, with "sorted alphabetically, fewer
subdirectories first" inside each group. Nothing enforces it mechanically:
`/code-style-review` covers it only by hand reading
(`.agents/skills/code-style-review/references/worker.md` step 7, :85 "rule 47:
include grouping and order"), and `.agents/scripts/Find-SessionCandidates.ps1`
has no rule 47 kind, because the rule needs the whole include block, not one
line. The hand-read style sweep of `Projects/` C++ applied rule 47
inconsistently across its batches, and "fewer subdirectories first" is read
differently by different workers.

Current tree evidence:
- Engine code mixes engine and game headers in one block, e.g.
  `Engine/Source/File/Replay.cpp:7-14` (`File/GridSave.h`, `Game.h`,
  `Network/Server/ServerSession.h`, `Profile/ProfileManager.h`), because engine
  sources compile inside the game projects and include game headers.
- Includes of a sibling header by bare name and by root-relative path both
  occur (`Engine/Source/Audio/AudioManager.cpp:8-13`).
- One relative path exists in both include roots:
  `Network/NetworkSessionContract.h` is in `Engine/Source/` and
  `Projects/BrokenEngineSandbox/Source/`; MSVC resolves a quoted include from
  the including file's directory first, then `AdditionalIncludeDirectories` in
  order (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  lists `Common`, `Engine/Source`, `Engine/Data`, then the project's `Source` and
  `Data`), so a classifier that only looks at spelled prefixes is wrong.
- Generated pack headers (`Data/Texture.h`, `Data/Audio.h`, `Data/Shader.h`,
  `Data/DataTypes.h`, ...) are written by DataPacker under
  `$(GeneratedDataIncludeRoot)` and do not exist in the source tree: one header
  per data type named in the `kDataTypes` table
  (`DataPacker/Source/Main.cpp:36-42`), whose path :606-608 builds and :624
  passes to `RunDirtyExport` (:500), which writes it (:564-566), and
  `DataTypes.h` by `GenerateDataTypesHeader` (:649-685).
- Angle-bracket includes appear only in `Tools/` (no precompiled header in
  those projects) and in `Common/ExternalHeaders.h`.
- Load-bearing orders: `Projects/BrokenEngineSandbox/Source/Pch.h:95-102`
  (documented), `Engine/Source/Engine.h` (`Engine/Source/AGENTS.md` "Preserve
  its documented include order"), and the aggregation headers
  `Common/Common.h`, `Common/ExternalHeaders.h`, `DataPacker/Source/Pch.h`,
  whose listed headers rely on earlier entries.
- Conditional include blocks such as
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:13-20`
  and `Engine/Source/Audio/AudioManager.cpp:2-4` sit next to whole-file
  `#if defined(BT_CLIENT)` / `BT_SERVER` guards whose `#endif` is the file's last
  line (`Engine/Source/Graphics/Managers/PipelineManager.cpp:1,499`).

A scratch prototype of the design below over the ~550 first-party tracked
`*.h`/`*.cpp` files found no unresolved include and changed roughly 150 files
(about 70 `Engine/`, 55 `Projects/`, 10 `DataPacker/`, 9 `Tools/`, 3 `Common/`);
the implementer's real count replaces this estimate.

## Design

User-directed decisions (from the user's own words in the requesting session):
- Within an include group, compare paths one folder level at a time: at each
  level a folder entry sorts before a file entry, like a file browser, then
  alphabetically. This applies at every level. Example the user selected:
  ```
  #include "Agent/Commands/ServerFaultFixtures.h"
  #include "Agent/Commands/ServerSimulationFixtures.h"
  #include "Agent/AgentCommandsServerQueries.h"
  #include "Network/Server/ServerFleetManager.h"
  #include "Network/Server/ServerSession.h"
  #include "Game.h"
  ```
  This replaces "sorted alphabetically, fewer subdirectories first" and reverses
  that rule text (`Documents/C++StyleGuide.txt:189`), which puts a folder's
  files before its subfolders.
- The reordering is done deterministically by a script, and the same script
  detects the group structure — corresponding header first, then engine, game,
  and external groups, each separated by exactly one blank line — reporting
  missing or extra blank lines and misgrouped includes.
- This is a separate change from the hand-read style sweep, after it lands.

Author's recommendations for the remaining choices, with rationale:

1. Script `.agents/scripts/Test-IncludeOrder.ps1` (PowerShell 7, repository
   script rules). Parameters: `-RepositoryRoot <absolute toplevel>`, exactly one
   of `-Path <repo-relative files>` or `-All`, and a `-Fix` switch. `-All` takes
   the tracked `*.h`/`*.cpp` files under `Common/`, `DataPacker/`, `Engine/`,
   `Projects/`, `Tools/` from `git ls-files`. Without `-Fix` it only reports;
   with `-Fix` it rewrites every file whose only violations are fixable and
   leaves any other file byte-for-byte unchanged. It prints one
   `broken-engine-include-order/v1` JSON object: `status` (`pass`, `fail`,
   `error`), `files` (scanned count), `violations` rows (`path`, `line`,
   `kind`), `rewritten` (paths), `truncated` (rows capped at 400). Exit 0 when no
   violation remains, 1 when any remains, 2 on an input error. Contract lives in
   the script's header comment, as `Find-SessionCandidates.ps1` does. Rationale:
   one tool for both check and fix keeps sort and check from diverging.
2. Excluded files (never read for violations, never rewritten), a fixed list in
   the script: `Projects/BrokenEngineSandbox/Source/Pch.h`,
   `DataPacker/Source/Pch.h`, `Common/ExternalHeaders.h`, `Common/Common.h`,
   `Engine/Source/Engine.h`, anything under a `Data/Shaders/` directory
   (GLSL/C++ dual-language headers), `Common/ClangTidyShims/`, and all of
   `ThirdParty/` (never in `-All`; rejected by `-Path`). Rationale: these are the
   aggregation and PCH headers whose order is load-bearing, plus shader and
   vendored code the rule does not govern.
3. Include segments. A segment is a maximal run of `#include` lines and the
   blank lines between them; any other line — comment, `#pragma`, code, or
   preprocessor conditional — ends it, so a comment stays attached to the
   include below it (`Projects/BrokenEngineSandbox/Source/Frame/Frame.h:5`). A
   segment is checked when it sits at conditional depth 0 or directly inside a
   whole-file guard: an `#if` whose matching `#endif` is the file's last
   non-blank line. Every other segment is inside a conditional include block and
   keeps its hand order, unreported. Segments are sorted independently; the
   script never moves an include across a segment boundary. Blank lines before
   the first or after the last include of a segment are not the script's
   concern.
4. Classification, in this order, per include in a checked segment:
   - group 0: `#include "Pch.h"` when it is the file's first include;
   - group 1 (corresponding): in a `.cpp`, an include that resolves to the
     sibling header with the same base name in the same directory;
   - otherwise resolve the spelled path as MSVC does for quoted includes — the
     including file's directory, then the owning project's
     `AdditionalIncludeDirectories` in order, read from the first-party vcxproj
     (`$(ProjectDir)` expanded, other `$(...)` entries skipped) chosen by source
     prefix: `Engine/`, `Common/`, `Projects/BrokenEngineSandbox/` →
     `BrokenEngineSandbox.vcxproj`; `DataPacker/` → `DataPacker.vcxproj`;
     `Tools/AgentHarness/`, `Tools/ToolCommon/` → `AgentHarness.vcxproj`;
     `Tools/WorktreeCli/` → `WorktreeCli.vcxproj`. Group 2 (engine) when it
     resolves under `Engine/`, `Common/`, `DataPacker/`, or `Tools/`; group 3
     (game) under `Projects/`; group 4 (external) under `ThirdParty/`. Any
     angle-bracket include is group 4. A quoted include that resolves nowhere
     is group 2 when its path starts with `Data/` (a DataPacker-generated pack
     header; the engine's asset pipeline owns that format); any other
     unresolved include is violation `unresolved` and makes the file
     unfixable.
   Rationale: resolution follows the compiler, so the
   `Network/NetworkSessionContract.h` collision classifies correctly; reading
   the vcxproj keeps one source for include roots.
5. Canonical form of a checked segment: includes sorted by group, then by the
   sort key; within a group no blank line; between two groups present in the
   segment exactly one blank line. Sort key: split the spelled path on `/`;
   compare level by level; at a level where one path has a folder (more
   segments follow) and the other a file, the folder sorts first; otherwise
   compare the two names ordinal case-insensitive (matching `.editorconfig`
   `cpp_sort_includes_priority_case_sensitive = false`); `..` is an ordinary
   folder name. Angle and quoted delimiters do not affect order. Equal keys keep
   their original relative order. An include line moves with any trailing
   comment on it.
6. Violation kinds: `order` (a checked segment's includes are not in canonical
   order), `blank-missing` (no blank line between two groups), `blank-extra`
   (a blank line inside a group, or more than one between groups),
   `corresponding-not-first` (the corresponding header is in a later checked
   segment than another non-`Pch.h` include — unfixable, reported for a hand
   fix), `unresolved`. `order`, `blank-missing`, and `blank-extra` are fixable.
7. Rewrite fidelity: `-Fix` replaces only the lines of rewritten segments and
   preserves each file's BOM presence, line-ending style, and final newline
   exactly. A second `-Fix` run rewrites nothing.
8. Rule 47 text (`Documents/C++StyleGuide.txt` :184-197), replaced with:
   ```
   47. Header include order (groups separated by exactly one blank line, no blank line inside a group):
   	a) Corresponding .h file (always first; only a leading "Pch.h" include precedes it)
   	b) engine:: includes: headers resolving under Engine/, Common/, DataPacker/ or Tools/, and generated Data/ pack headers
   	c) game:: includes: headers resolving under Projects/
   	d) External C++/platform headers, only in projects without a precompiled header (Tools/); elsewhere external and common:: headers go in Pch.h
   	Within each group, compare paths one folder level at a time: at each level folders sort before files, then alphabetically (case-insensitive)
   	Includes inside an #if block that is not a whole-file guard keep their hand order
   	.agents/scripts/Test-IncludeOrder.ps1 checks this rule and fixes it with -Fix

   	#include "Player.h"

   	#include "Graphics/Managers/BufferManager.h"
   	#include "Graphics/Managers/PipelineManager.h"
   	#include "Graphics/GraphicsUtils.h"
   	#include "Input/Input.h"

   	#include "Agent/Commands/ServerFaultFixtures.h"
   	#include "Agent/Commands/ServerSimulationFixtures.h"
   	#include "Agent/AgentCommandsServerQueries.h"
   	#include "Network/Server/ServerFleetManager.h"
   	#include "Network/Server/ServerSession.h"
   	#include "Game.h"
   ```
   The file's existing encoding, tabs, and line endings are kept.
9. `/code-style-review` (`references/worker.md`): replace the step 7 bullet
   "rule 47: include grouping and order" (:85) in place with a bullet that
   runs
   `pwsh -NoProfile -Command "& '.agents/scripts/Test-IncludeOrder.ps1' -RepositoryRoot '<absolute repository toplevel>' -Path '<file>','<file>' -Fix"`
   once over the distinct paths of the selected `cpp`-class ranges (or the
   caller-supplied scope's files) instead of hand reading rule 47. Each
   rewritten file is one `Fixes Applied` row (Rule 47, include block
   reordered) and adds its owning build targets to `Build required`; the
   bullet names that build as the rewrite's meaning-preservation proof that
   step 11 requires (Risk states why it suffices). Each remaining violation is
   fixed by hand when it is
   `corresponding-not-first`, then the same command is run once more so the
   file's fixable violations are rewritten and the check is confirmed (`-Fix`
   leaves a file with any unfixable violation unchanged, Design item 1); an
   `unresolved` violation is reported as a residual. So that step 7 does not
   contradict the script run, three other step 7 sentences get minimal edits:
   - the intro's closing clause "and for these halves of rules split with
     another owner:" (:71-72) becomes "and for these halves of rules split with
     another owner, except rule 47, whose half is checked by the script run its
     bullet states:";
   - the mandate sentence "The hand-read rules and the rules the scanner's
     `style-rule-<n>` kinds cover are this review's whole style mandate"
     (:88-90) becomes "The hand-read rules, rule 47 through the script run,
     and the rules the scanner's `style-rule-<n>` kinds cover are this
     review's whole style mandate", the rest of the sentence unchanged;
   - the "Done when" (:96-97) gains a final clause: "and the rule 47 script's
     last run reports no violation other than `unresolved` ones, each reported
     as a residual".
   No step is added or
   renumbered, so every citer of a worker.md step number stays valid. Keep the
   step 7 sentence that `/repo-code-review` owns rule 47's external-header
   half. Rationale: after the repository re-sort every file is in canonical
   form, so running over the whole changed files keeps it there without hand
   reading; the step 10 scanner adjudication and `Find-SessionCandidates.ps1`
   stay unchanged because the check is block-level, not line-level.
10. Repository re-sort: after the script and rule text exist, run
    `pwsh -NoProfile -File .agents/scripts/Test-IncludeOrder.ps1 -RepositoryRoot <toplevel> -All -Fix`
    once, hand-fix any `corresponding-not-first` it reports, re-run the same
    `-All -Fix` command so those files' fixable violations are rewritten, and
    commit the result as the mechanical part of this change. If a build fails because a
    header relied on an include that used to precede it, add the missing direct
    include to that header (in canonical position) — never hand-reorder around
    the script or add a file to the exclusion list for it.
11. `.editorconfig` :81-90 stays unchanged. `cpp_include_cleanup_sort_after_edits
    = false` and `cpp_sort_includes_error_tag_type = none` mean Visual Studio
    neither sorts nor flags include order on its own; only a manual "Sort
    #include directives" command would apply its plain alphabetical order, and
    the script check catches that at review. `cpp_sort_includes_priority_style
    = quoted` already agrees with external headers last.

Risk: Change Workflow Tier 2. Highest trigger: new tool behavior in one
subsystem — the new `/code-style-review` script and its workflow step. The C++
re-sort is Tier 1 mechanical (include lines reordered and blank lines adjusted
only; no signature, invariant, determinism/CRC, serialization, `.pack`, wire,
replay, affinity, or threading exposure). Compiling alone does not prove an
include reorder meaning-preserving: a macro one header defines, or a
declaration it supplies, can change a later header without an error. For
rule 47 script rewrites — the `-All -Fix` re-sort and every Design item 9
run — the accepted meaning-preservation proof is the build of every touched
project (Acceptance criterion 7; `Build required` in Design item 9), which is
sufficient here for these reasons, checked against the tree:
- Every macro a first-party conditional tests, other than compiler-defined
  ones, is set by vcxproj `PreprocessorDefinitions` (`BT_*`) or defined in an
  excluded file: `ENABLE_CRT_DEBUG_HEAP` in `DataPacker/Source/Pch.h:3`
  (commented out in the game `Pch.h:29`), `_CRTDBG_MAP_ALLOC` in
  `Common/ExternalHeaders.h:51`, and `ENABLE_32_BIT_BOOL` in the excluded
  `Engine/Data/Shaders/ShaderLayoutsBase.h:330`, tested only in function
  bodies below the include block
  (`Engine/Source/Graphics/Managers/DeviceManager.cpp:210`,
  `InstanceManager.cpp:556`). No non-excluded first-party file has `#undef`.
- The only macros non-excluded headers define are the function-like
  `ASSERT`/`CHECK_HRESULT`/`VERIFY_SUCCESS`/`DEBUG_BREAK*`, `LOG`,
  `FILE_LOG*`, and `CHECK_VK` (`Common/ErrorUtils.h`, `Common/Log/Log.h`,
  `Common/Log/DiagnosticLog.h`, `Engine/Source/Graphics/GraphicsUtils.h`);
  both `Pch.h` files include `Common.h` (the game's also `Engine.h`,
  `Pch.h:97-101`) ahead of any reordered include, `Tools/` uses none of them,
  and no conditional tests them. The only first-party `std::formatter`
  specializations are in `Common/Log/LogFormatters.h`, also pulled in by
  `Common.h`.
- Conditional include blocks keep their hand order (Design item 3).
- A header that relied on a declaration an earlier include supplied fails to
  compile, which the builds catch and Design item 10 fixes. Not ruled out by
  these facts: a header's inline code silently picking a different overload
  that stays viable after a declaration moves behind it; this change accepts
  that residual risk.
No shader, data, or vcxproj membership change.

## Critical files

- `.agents/scripts/Test-IncludeOrder.ps1` (new)
- `.agents/skills/code-style-review/references/worker.md` (step 7 rule 47 bullet, intro clause, mandate sentence, and "Done when")
- `Documents/C++StyleGuide.txt` rule 47
- First-party tracked `*.h`/`*.cpp` under `Common/`, `DataPacker/`, `Engine/`, `Projects/`, `Tools/` whose include blocks the `-All -Fix` run rewrites
- Read only: the four first-party vcxproj files named in Design item 4

## In scope

- Create `.agents/scripts/Test-IncludeOrder.ps1` with the parameters, exclusions,
  segment rule, classification, canonical form, violation kinds, result object,
  exit codes, and rewrite fidelity in Design items 1-7.
- Replace rule 47's text and example in `Documents/C++StyleGuide.txt` with
  Design item 8.
- In `.agents/skills/code-style-review/references/worker.md`: replace the step
  7 "rule 47: include grouping and order" bullet in place with the script
  bullet of Design item 9, and make Design item 9's edits to step 7's intro
  clause, mandate sentence, and "Done when"; no step is added, removed, or
  renumbered.
- Include lines and the blank lines between them in checked segments of
  first-party C++ files, as rewritten by the `-All -Fix` runs; hand fixes for
  `corresponding-not-first`; missing direct includes a failing build proves a
  header needs (Design item 10).

## Out of scope

- Any C++ change other than include-line order, blank lines inside include
  segments, and build-proven missing direct includes.
- The excluded files in Design item 2, and include lines inside conditional
  include blocks.
- `ThirdParty/`, GLSL sources, and `Data/Shaders/` dual-language headers.
- Changing an include's spelling (bare sibling name versus root-relative path)
  or removing unused includes.
- `.agents/scripts/Find-SessionCandidates.ps1` and the `/code-style-review`
  `SKILL.md`.
- In worker.md: step numbering, every step other than step 7, and within
  step 7 every rule-list entry other than the rule 47 bullet and every
  sentence other than the three Design item 9 edits.
- `/repo-code-review`'s external-header rule and `Common/ExternalHeaders.h`.
- `.editorconfig`, `.clang-tidy`, and vcxproj files.
- `Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`, a
  findings record pinned to its own baseline.
- A repository-wide static-check gate that runs the script outside
  `/code-style-review`.

## Acceptance criteria

1. `-All` without `-Fix` after the re-sort exits 0 with `status` `pass` and no
   violations.
2. A further `-All -Fix` run after the re-sort reports an empty `rewritten` list and leaves
   `git status` unchanged.
3. For every C++ file the change touches, its non-blank lines, sorted, are
   identical before and after, except build-proven added includes; and its BOM
   presence, line endings, and final newline are unchanged.
4. No excluded file of Design item 2 and no line inside a conditional include
   block differs from the baseline.
5. On a scratch copy of a file with `#include "Game.h"` above
   `#include "Network/Server/ServerSession.h"` in the game group, check mode
   reports `order`; with an engine and a game include adjacent and no blank line
   between, it reports `blank-missing`; `-Fix` produces the canonical form of
   Design item 5.
6. `Documents/C++StyleGuide.txt` rule 47 reads as Design item 8, and
   `.agents/skills/code-style-review/references/worker.md` no longer hand-reads
   rule 47 and runs the script as Design item 9 states.
7. `/compile` builds pass for every project the re-sort touches, in Debug and
   Release: BrokenEngineSandbox client and server; DataPacker when `Common/` or
   `DataPacker/` files changed; AgentHarness and WorktreeCli when `Tools/` files
   changed.
8. `/external-skill-creator` validation of the `code-style-review` package
   passes.

## Notes

- Overlap checked: no live Plan owns rule 47. The whole-file sweep runbook
  `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md` applies
  the `/code-style-review` mandate by reference, so it picks up the script with
  no edit. `Documents/Plans/ChangeWorkflow/CodeStyleReviewRule51WrappedArgumentKind.md`
  and `Documents/Plans/ChangeWorkflow/SessionCandidateScannerPrecision.md` edit
  other entries of the same worker.md step 7 list and step 10, and they,
  `Documents/Plans/ChangeWorkflow/CodeStyleReviewPermittedForms.md`, the sweep
  runbook, and `Find-SessionCandidates.ps1` cite worker.md step numbers. This
  change edits only the step 7 rule 47 bullet and three step 7 sentences in
  place and renumbers no step,
  so every such citation stays valid and the overlap is textual only: there is
  no dependency or coordination constraint.
- Live verification: none needed; include order has no runtime-observable
  effect beyond compiling.
