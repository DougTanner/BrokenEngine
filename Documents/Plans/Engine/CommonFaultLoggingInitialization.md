<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:29:41.180Z","dependsOn":[]} -->
# Common Fault Logging Initialization

## Context

The all-rule lint review confirmed 8 remaining obligations in
the saved ledger. The source forms remain present and fail the named style
rules; the declarations, missing defaults, parameter forms or ordered guard
groups in candidates.json establish the local cause. These were pre-existing
forms at the recorded baseline, outside the newly narrowed landing boundary.
The user deferred implementation explicitly. False positives and completed
mechanics are excluded. The support ledger is durable evidence, not a new scan
or permission to expand a hand audit.

## Design

Move six `S-constant-globals` definitions into their existing headers as inline constinit with exact
initializers: `giExpectedThrowDepth` remains thread_local and zero; `giMyOutputDebugString` remains atomic
zero; three log buffers use `{}`; runtime category array preserves the DataPacker kVerbose/other kInfo branch,
kTemp slot1 and kReplay slot8. Retain lock-free/category-count assertions after definitions. Move the default
constant needed by the array to the owning header once, preserving its two branches and explanatory comments.
Remove superseded definitions. Move `gDbgHelpMutex` to `inline std::recursive_mutex gDbgHelpMutex {};`,
removing its .cpp definition; preserve StackWalker recursive try-lock behavior and include order before
dependent globals. Keep `gLogMutex`, `init_seg(lib)`, file stream and alive guard in their current relative
order and keep `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR`.

Add this single exception at the owning rule36 in `Documents/C++StyleGuide.txt:215`: “Exception:
common::gLogMutex remains declared in Log.h and defined in Log.cpp under init_seg(lib), preserving its
lifetime before user static initialization and after user static destruction.” The code-style-review worker's
rule36 permitted-forms guidance references that owning exception instead of restating it; the scanner remains
candidate-only and need not suppress the hit. Existing logging lifetime documentation remains its invariant
owner; correct only statements made false by other moved globals. No general global-lifetime exception.

## Critical files

- `Common/Determinism.h` — only the named candidate regions in candidates.json.
- `Common/Log/Log.h` — only the named candidate regions in candidates.json.

The exact current logging definitions and category initializer are preserved in
`CommonFaultLoggingInitialization/initializers.txt`. Installed MSVC mutex
construction under _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR is non-constexpr;
constinit is therefore unavailable for gLogMutex or gDbgHelpMutex. Microsoft
documents that init_seg(lib) applies to subsequent dynamic globals and that
warning(push)/warning(pop) controls warning state only. Preserve the existing
phase and avoid a header pragma or restoration proposal.
See https://learn.microsoft.com/en-us/cpp/preprocessor/init-seg and
https://learn.microsoft.com/en-us/cpp/preprocessor/warning plus
https://eel.is/c++draft/dcl.constinit .

- `.agents/skills/code-style-review/references/worker.md` — matching definitions or directly dependent
  contract.
- `Common/Determinism.cpp` — matching definitions or directly dependent contract.
- `Common/Log/Log.cpp` — matching definitions or directly dependent contract.
- `Common/StackWalker.h` — matching definitions or directly dependent contract.
- `Documents/C++StyleGuide.txt` — matching definitions or directly dependent contract.

## In scope

Implement exactly 8 saved candidate rows in `CommonFaultLoggingInitialization/candidates.json` and their
correctness-dependent declarations/consumers, including the single guide36 exception and its reviewer
reference. The exception preserves the mutex source form; it is not a source migration. Run independent
skill-package, coherence and progressive-disclosure checks for the reviewer-instruction edit. The ledger
preserves original identity, source text and exact wrapper definitions; this Design overrides historical
alternatives or stale action labels. Do not repeat completed formatter return-type, namespace or SAL edits.

## Out of scope

All candidates outside the saved ledger; new scanner rules or suppression;
filesystem policy and publication-container decisions; Wrapper constructors,
registrars, defaults/ranges, project membership or removal of emptied files;
new helpers/configuration; wire/save/replay/.pack formats and versions; frame
or Collection layout, CRC computation or trust changes; ThirdParty edits;
new unit tests, harness APIs, speculative cleanup or performance claims.
An unexpected behavior change returns for re-planning instead of widening scope.

## Acceptance criteria

- Every saved row is implemented once, using the exact symbol/type/initializer,
  order, guard and lifetime contracts in Design; no rejected row is changed.
- Before editing, retain the current-tree build identity and /agent-harness Debug sandbox
  startup/connected-render/server-tick/shutdown logs and category thresholds. Repeat with isolated matching
  settings after the edit; require equal defaults and no new constructor, allocation or lifetime failure. No
  originating runtime reproduction command sequence was supplied; this is a prospective behavior-preservation
  baseline. Read exact library-phase mutex/file/alive-guard order and recursive try-lock consumers. Build
  DataPacker Release|x64, sandbox client/server and ModelViewer Debug|x64 and Release|x64, plus AgentHarness,
  WorktreeCli and ServerMonitor Release|x64 through /compile.
- Required reviews and checks pass; no format/layout/CRC or compatibility
  behavior changes beyond the expressly recorded internal-offset choice.

## Notes

Future implementation is Change Workflow Tier 3: fault-path mutex initialization and Common logging lifetime
affect independently owned runtime, asset and tool consumers.

The user explicitly deferred all remaining implementation while preparing the
completed cleanup to land. This Plan records wanted debt, remains unclaimed,
and supplies no new implementation approval. Claim and obtain normal Change
Workflow approval before edits; reuse earlier analysis only after checking it
against the then-current tree. Tier 2+ needs plan audit, applicable simplicity
review, and Tier 3 needs grill and adversarial review. Apply triggered C++/GLSL
propagation, owner-document synchronization, style, correctness, comments,
over-engineering and independent acceptance verification; finalize verified
work through /finalize-changes. No unit tests or new harness commands.

Baseline evidence is static source at ad5d456751c76bf43c86b7cc5bd038946c81b1c0
plus the saved current candidate forms. Original coordinates may drift: match
path, symbol and text. A recorded runtime baseline must identify its actual
current-tree build; it is not an unedited-session baseline. All live operations
route through /agent-harness and all builds through /compile. Keep each live
run bounded; a proposed individual run over ten minutes requires its purpose,
estimate and user permission before launch. No runtime performance claim.

## Coordination

No directional prerequisites: this Plan can build and verify independently.
No mandatory nondirectional coordination: no sibling lint Plan owns these candidate files. Other
initialization Plans must independently preserve the existing library-phase logging lifetime.
Share matching
runtime evidence only when it settles each Plan's own acceptance criteria.
The four existing filesystem/publication Plans own separate deferred choices
and need no dependency or new reciprocal constraint from this Plan.
