<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T22:44:52.339Z","dependsOn":[]} -->
# Audit: C++ style guide rule coverage — give every rule a deterministic check or a named review step

## Context
Observed symptom: in a `/next-plan` run, a session change added a rule 18
violation (top-level `const` on a local value variable) and every check in the
Change Workflow that ran passed it — the implementer, the static pass,
`/code-style-review`, and `/repo-code-review`. The Jev style-rule judgment does
not ask about rule 18, and Clang-Tidy does not run under `/compile`, so neither
checked it. The user caught it at the landing confirmation and asked how no
review could be checking the rule. The user then asked for this Plan: "add a
plan to deep-dive the code review for style guide and advise for how to fix it
such that every rule is either determinstic or actually checked".

The rule 18 gap itself is already recorded in
`Documents/Plans/ChangeWorkflow/CodeStyleReviewRule18Coverage.md`, whose
`## Context` gives the per-mechanism evidence for that one rule. This Plan
covers the general gap behind it: no document maps each guide rule to what
enforces it, so a rule can have no owner and nobody notices. Evidence from the
current tree:
- `Documents/C++StyleGuide.txt` has 63 numbered rules, 1 through 64; there is
  no rule 45.
- `.agents/skills/code-style-review/references/worker.md` step 7 (~:67-76)
  names the hand-read rules (2 in part, 3, 14, 16, 21, 41 in part, 49, 51, 56,
  62). It states that those rules plus the scanner's `style-rule-<n>` kinds are
  "this review's whole style mandate; every other guide rule is outside it."
- `.agents/scripts/Find-SessionCandidates.ps1` `$script:CandidatePatterns`
  (~:37-57) emits `style-rule-<n>` kinds for rules 2, 15, 19, 27, 28, 29, 32,
  41, 50, 52, 57 and 58, and `Test-Rule61Line` (~:147) emits `style-rule-61`.
  Its comment at ~:36 says the step 7 hand-read list "is the complement of the
  style-rule-<n> kinds". Together the two lists cover about 21 rules, so this
  claim is imprecise (rules 2 and 41 appear in both lists; 42 rules are outside
  both).
- `.agents/scripts/Test-StyleRuleJudgment.ps1` asks Jev about rules 3, 14, 49
  and 62, and rule 56 per identifier. worker.md step 6 only adds flagged rule
  14, 49 and 62 entries as extra candidates and ignores rules 3 and 56, so Jev
  is advisory and does not count as enforcement.
- `.agents/skills/comment-review/` owns rule 64.
- `.agents/skills/repo-code-review/SKILL.md` (~:7-8, ~:25-26) sends style and
  naming to `/code-style-review`.
- `.clang-tidy` maps some checks to the guide (for example
  `modernize-use-nullptr`, `modernize-use-override`, `modernize-use-using`,
  `llvm-prefer-defined`). It defers `misc-const-correctness` (:25) and
  `readability-identifier-naming` (:26).
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` (~:26)
  says agent builds through `/compile` force-disable Clang-Tidy, so its
  diagnostics appear only in the IDE and never on the agent Change Workflow
  path.
- `.editorconfig` holds Visual Studio formatting settings (tabs, braces, and
  others). They take effect only when an editor formats the file. No agent
  script or review step checks them.
- `.agents/references/static-checks.md` (~:8-11) keeps compilation and
  Clang-Tidy out of the static pass. Its only C++ row is the implementation
  slice's own focused reads.
- The compiler enforces some rules on its own: `<RuntimeTypeInfo>false` in
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  (rule 43), and the compile-poisoned `XMVector3Rotate` and
  `XMVector3InverseRotate` names the guide cites for rule 63.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1e4f73e5-7796-45c4-9684-86a1ad415395
- Worktree/branch UUID: a913b46c-e7c2-43fe-ae32-798e0f36a975
- Session branch: claude/a913b46c-e7c2-43fe-ae32-798e0f36a975
- Worktree: .claude\worktrees\BrokenEngine\a913b46c-e7c2-43fe-ae32-798e0f36a975
- Landing ref: claude/a913b46c-e7c2-43fe-ae32-798e0f36a975
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StyleGuideRuleCoverageAudit.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
The root cause is already established from the current tree, in `## Context`
above. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/a913b46c-e7c2-43fe-ae32-798e0f36a975` in bounded
friction mode, supplying client `claude` and conversation session ID
`1e4f73e5-7796-45c4-9684-86a1ad415395`.

This Plan's change is an audit plus its routed output. It changes no review
skill, script, or config. Recommended method (the author's recommendation):

1. Coverage map. Read `Documents/C++StyleGuide.txt` and, for each of the 63
   numbered rules, record every mechanism that touches it today, citing
   `path:line`. A mechanism counts as enforcement only when it runs on the
   agent Change Workflow path for session-changed C++ and can fail or produce
   a finding:
   - a `Find-SessionCandidates.ps1` `style-rule-<n>` kind;
   - a worker.md step 7 hand-read mandate;
   - a `/comment-review` mandate;
   - a `/repo-code-review` mandate;
   - a compiler error or poison under `/compile`.
   These are recorded but do not count as enforcement:
   - Jev judgment (advisory);
   - a Clang-Tidy check (IDE only while `/compile` disables Clang-Tidy);
   - an `.editorconfig` setting (applied only by an editor's formatter);
   - partial coverage, recorded with the uncovered part named (for example
     rule 2's non-scanner forms, or rule 41's "always write `std::`" half).
   A rule with no enforcing mechanism is `unowned`.
2. Recommendations. For every `unowned` rule, and every rule enforced only in
   part, record one recommendation with its reason:
   - Deterministic check, preferred when the rule has a pattern shape that a
     line or two-line pattern with `Except` forms can decide under the
     scanner's candidates-only contract, or that a compiler setting or poison
     can enforce.
   - An addition to a named review step's mandate when the rule needs type,
     semantic, or cross-line judgment. This means worker.md step 7's hand-read
     list, or `/repo-code-review` when the rule is a correctness contract
     rather than style (for example rules 9, 53 and 60).
   - Record the rule as `decision needed` when the guide's text itself is
     ambiguous, or when the only deterministic route is enabling Clang-Tidy on
     the agent path (blocked by the crash the VS2026 AGENTS.md records).
   Rule 18 maps to `Documents/Plans/ChangeWorkflow/CodeStyleReviewRule18Coverage.md`
   while that Plan is live, or to its landed mechanism once it has landed. It
   gets no new recommendation.
3. Output location. Write the map and recommendations as one findings record,
   `Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`: one
   table row per rule (rule, current mechanisms, enforced / partial / unowned,
   recommendation, routed-to path). Add a `## Decisions needed` section holding
   every `decision needed` rule, each with its options.
4. Routing. Run `/create-follow-up-plans` for the decided recommendations,
   grouped by mechanism and target file:
   - one Plan for all new scanner kinds (`Find-SessionCandidates.ps1`
     `$script:CandidatePatterns`, its kind-list comment, and worker.md steps
     7 and 10);
   - one Plan for the step 7 hand-read mandate extension. It also rewrites the
     "whole style mandate" sentence so no guide rule is left outside every
     owner, and corrects the scanner's "complement" comment;
   - one Plan per other owning skill or build setting the map names.
   Each follow-up Plan that edits the scanner table or the step 7 list carries
   a `dependsOn` edge to `CodeStyleReviewRule18Coverage.md` while that Plan is
   live, because both edit the same table and list. Then record each created
   Plan's path in the findings record's routed-to column.
5. Decisions. Before landing, the executing session presents every
   `decision needed` row to the user with its options, their trade-offs, and a
   recommendation. It routes each answer into a follow-up Plan as step 4 does
   and records that Plan's path in the row, so every rule ends owned or
   deterministic as the user asked. Only rows the user explicitly defers stay
   in `## Decisions needed`.

No `dependsOn` edge on this Plan (the author's recommendation). The audit only
writes Documents files. It edits neither the scanner nor worker.md, and it maps
rule 18 correctly whether or not `CodeStyleReviewRule18Coverage.md` has landed.
The file overlap it would create is carried instead by the follow-up Plans'
own edges.

## Critical files
- `Documents/C++StyleGuide.txt` (read)
- `.agents/skills/code-style-review/SKILL.md` (read)
- `.agents/skills/code-style-review/references/worker.md` steps 6-10 (read)
- `.agents/scripts/Find-SessionCandidates.ps1` `$script:CandidatePatterns`,
  `Test-Rule61Line` (read)
- `.agents/scripts/Test-StyleRuleJudgment.ps1` (read)
- `.agents/skills/comment-review/SKILL.md` (read)
- `.agents/skills/repo-code-review/SKILL.md` (read)
- `.clang-tidy`, `.editorconfig` (read)
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` (read)
- `.agents/references/static-checks.md` (read)
- `Documents/Plans/ChangeWorkflow/CodeStyleReviewRule18Coverage.md` (read)
- `Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` (new)
- New follow-up Plans under `Documents/Plans/<area>/`, created by
  `/create-follow-up-plans`

## In scope
- Creating `Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`
  with the per-rule coverage map, recommendations, and `## Decisions needed`
  section as `## Design` steps 1-3 describe
- Creating the follow-up Plans `## Design` steps 4 and 5 describe, through
  `/create-follow-up-plans`, and recording their paths in the findings record

## Out of scope
- Editing any file under `.agents/`, `.clang-tidy`, `.editorconfig`, any
  vcxproj, or any `AGENTS.md`; the follow-up Plans own those changes
- Editing `Documents/C++StyleGuide.txt`; ambiguous rule text goes to
  `## Decisions needed`
- Editing or re-planning `CodeStyleReviewRule18Coverage.md`
- Sweeping existing violations in unchanged code
- Enabling Clang-Tidy in `/compile` builds
- The landed change the observing session produced; any transcript path or
  transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation). Trigger: the change writes only an Investigations
findings record and new Plan files. Each follow-up Plan records its own tier.
Never embed transcript paths or home paths.

## Acceptance criteria
- `Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` has
  exactly one row for each of the 63 numbered guide rules, and each row cites
  `path:line` for every mechanism it lists
- Every row marked `unowned` or `partial` names a recommendation, its reason,
  and a routed-to path: a created follow-up Plan, the rule 18 Plan, or its
  own entry in `## Decisions needed`
- No row counts Jev, Clang-Tidy, or `.editorconfig` alone as enforcement
- Every `decision needed` row was put to the user with options, trade-offs,
  and a recommendation before landing; each answered row routes to a created
  follow-up Plan, and only rows the user explicitly deferred remain in
  `## Decisions needed`
- The scheduler-state check `Documents/Plans/AGENTS.md` documents reports
  `status: valid` with every created follow-up Plan valid; no created Plan
  duplicates `CodeStyleReviewRule18Coverage.md`
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
