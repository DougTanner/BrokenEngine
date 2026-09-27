<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:27:38.882Z","dependsOn":[]} -->
# Fix: style guide and /code-style-review — permitted forms for rules 6, 15, 17, 36 and 39 that have no meaning-preserving fix

## Context
The `Projects/` whole-file sweep
(`Documents/Plans/Game/StyleGuideScannerRuleSweepProjects.md`) left scanner
hits unchanged because no spelling satisfies the rule without changing
meaning, layout, or the header's include boundary. `Documents/C++StyleGuide.txt`
and the permitted-forms list in
`.agents/skills/code-style-review/references/worker.md` step 10 (:100-113) do
not name these forms, so every session scan and every sibling sweep reports
them again. Paths below are under `Projects/BrokenEngineSandbox/Source/`.

- Rule 15 (`Documents/C++StyleGuide.txt:87-93`) lists five permitted `auto`
  uses, yet the guide's own rule 2 example at :11 writes a generic lambda
  parameter (`[](const auto& rSource)`). That contradiction is the reason the
  sweep treated the `std::visit` lambda parameters at `Frame/FrameInput.cpp:17`,
  `:29`, `:53` as permitted. Rule 15 also reports the Collection member-list
  accessors — the deduced return type and the explicit object parameter of
  `auto SharedMembers(this auto&& rSelf)` (`Frame/StatusChange.h:95`) and the
  `*Members` accessors in `Frame/Collections/{Players,Spaceships,Blasters,Missiles}/*.h`
  (about 35 hits) — and the abbreviated-template helpers
  `inline auto GameInterpolateCollections(auto&& rSelf)` and
  `GamePostRenderCollections` (`Frame/FrameCollections.h:11`, `:16`), which
  return a `std::tie` of references. The same pattern appears in every Engine
  collection header under `Engine/Source/Frame/Collections/`. No spelled type
  preserves these: the return type is a reference tuple that depends on the
  object's const-ness.
- Rule 6 (`:51`) says to avoid `BT_DEBUG`/`BT_RELEASE` differences and use
  `if constexpr` on a `kb*` toggle. `if constexpr` cannot guard an `#include`,
  a namespace-scope declaration, or a non-template body naming a symbol
  declared only in one configuration: `Agent/Commands/AgentCommandsAudioStreaming.cpp:8`
  (an `#include`), `:18` (namespace-scope helpers), `:428` (a body naming
  `engine::AgentCommandServer::mpAudioStreamingFixture`, declared only under
  `BT_CLIENT && BT_DEBUG` at `Engine/Source/Agent/AgentCommandServer.h:50-52`),
  and `Agent/Commands/ClientSubscriptionFixtures.cpp:21`, `:184`, `:374`
  (the `sCancelledFixture` declaration and its uses). The `kb*` definitions'
  own configuration blocks in `Pch.h` are the same kind of site.
- Rule 17 (`:105`) asks for `size_t` sizes and indices. `Frame/FrameInput.cpp:24`
  holds the status-change count as `int64_t` because that is the serialized
  field type `common::Write` writes, and `Frame/Collections/Spaceships/Spaceships.cpp:627`
  holds the island count as `int64_t` because it feeds
  `Workbuffer::PushBuffer(int64_t)` (`Common/Workbuffer.h:38`) and the
  `int64_t` loop index. The sweep already accepted the same reasoning for
  `Frame/Collections/Players/PlayersNavigation.cpp:355` (`uint32_t`, the type
  `common::Random` takes) without a listed form to cite.
- Rule 36 (`:158`) forbids initializing class members in the `.cpp`.
  `Frame::kiVersion` (`Frame/Frame.h:142`, defined at `Frame/Frame.cpp:29`) sums
  the `kiVersion` of collection types `Frame.h` only forward-declares
  (`Frame.h:15-22`); moving it would make `Frame.h` include every game
  collection header.
- Rule 39 (`:169`) asks for `[[maybe_unused]]`. `(void)pLifetime;` in
  `Agent/Commands/ClientSubscriptionFixtures.cpp:320` discards a by-value
  lambda capture held only to keep the fixture alive; a capture cannot carry
  an attribute.

## Design
Recommended edits, each naming only the form the evidence above proves:

1. `Documents/C++StyleGuide.txt` rule 15: add permitted cases for a generic
   lambda parameter, and for an explicit object (`this auto&&`) or
   abbreviated-template (`auto&&`) parameter together with the deduced return
   type of a function returning a `std::tie` member list, each with a short
   example in the guide's existing style. Recommended in the guide text rather
   than only in worker.md because the guide contradicts its own example.
2. `.agents/skills/code-style-review/references/worker.md` step 10
   permitted-forms bullet, one clause per rule in its existing order:
   - rule 6, a preprocessor guard `if constexpr` cannot replace — around an
     `#include`, a namespace-scope declaration, or code naming a symbol
     declared only in that configuration — and the `kb*` definition blocks
     in `Pch.h`;
   - rule 17, an integer type the value's consumer requires, such as a
     serialized field type or an API parameter type;
   - rule 36, also a static member whose initializer names types the header
     only forward-declares;
   - rule 39, a `(void)name;` discard of a lambda capture held only for
     lifetime.
   Recommended in worker.md rather than the guide for these four, matching the
   existing rule 25 and rule 26 clauses, which also narrow a rule to its
   intent without guide text.

## Critical files
- `Documents/C++StyleGuide.txt`
- `.agents/skills/code-style-review/references/worker.md`

## In scope
- Rule 15's permitted-case list in `Documents/C++StyleGuide.txt`
- The permitted-forms sub-bullet of step 10 in
  `.agents/skills/code-style-review/references/worker.md`

## Out of scope
- Any C++ source change, including the cited sites
- `.agents/scripts/Find-SessionCandidates.ps1` patterns and `Except` clauses,
  including a rule 15 `Except` for these forms; comment and string false
  positives belong to `Documents/Plans/ChangeWorkflow/CodeStyleReviewScannerFalsePositives.md`
- Rule 6 sites a `kb*` toggle can express, such as the `common::ValidateVector`
  guards in `Frame/Collections/Missiles/MissilesUpdate.cpp`, which belong to
  `Documents/Plans/Game/MissilesVectorValidationToggle.md`
- Every other rule and every other worker.md step

## Risk tier and invariants
Expected Change Workflow Tier 1. Trigger: documentation and skill text only,
with no code, signature, or invariant exposure (`.agents/references/risk-tiers.md`).

- Each new form covers only the cited shape; none permits `auto` in a
  range-based `for`, a `BT_*` guard around code `if constexpr` could guard, or
  a `.cpp` initializer for a member whose initializer compiles in the header.

## Acceptance criteria
- Each Projects site cited in `## Context` matches a guide case or worker.md
  clause by its text alone.
- `Documents/C++StyleGuide.txt` rule 15 no longer contradicts the rule 2
  example at :11.

## Notes
The pending sibling sweeps
`Documents/Plans/Engine/StyleGuideScannerRuleSweepEngine.md` and
`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`
will hit the same Engine collection accessors; landing this first lets them
cite a listed form, but neither order is required.
`Documents/Plans/ChangeWorkflow/CodeStyleReviewRule62SingleStatementExit.md`
edits other text in the same worker.md step 10. A worker.md change triggers
`/external-skill-creator` validation of the `code-style-review` package.
