<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T20:18:43.381Z","dependsOn":[]} -->
# Fix: /code-style-review — rule 18 top-level `const` on a local value variable passes review

## Context
Observed symptom: during a `/next-plan` run's Change Workflow pre-review checks,
a session change added
`const int64_t iPageLimit = std::max<int64_t>(iLimit, 0);` to `ClampWindow` in
`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`, and
the `/code-style-review` mechanic reviewed that range and returned PASS with no
fixes. `Documents/C++StyleGuide.txt:107-110` (rule 18) says: "Do not add
top-level "const" to local value variables." The review's handoff listed the
hand-read rules 2, 3, 14, 16, 21, 41, 49, 51, 56 and 62, and
`.agents/scripts/Find-SessionCandidates.ps1` reported 0 hits. The implementer
and static-pass workers also passed the line. The user caught it at the landing
confirmation, before it landed, which forced a correction after review had
passed; the `const` was removed.

Why no reviewer covers it, from the current tree:
- `.agents/skills/code-style-review/references/worker.md` step 7 names the
  hand-read rules (2, 3, 14, 16, 21, 41, 49, 51, 56, 62) and states that those
  rules plus the scanner's `style-rule-<n>` kinds are the review's whole style
  mandate; every other guide rule is outside it.
- `Find-SessionCandidates.ps1` `$script:CandidatePatterns` (~:37-57) emits
  `style-rule-<n>` kinds for rules 2, 15, 19, 27, 28, 29, 32, 41, 50, 52, 57
  and 58, plus `style-rule-61` from `Test-Rule61Line`. There is no rule 18 kind.
- `.clang-tidy:25` keeps `misc-const-correctness` deferred (disabled). That
  check would push the opposite way — it suggests adding `const` to locals — so
  no tidy check covers rule 18 either.

Rule 18 is a real, current guide rule; the user confirmed "We don't const".

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
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CodeStyleReviewRule18Coverage.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/a913b46c-e7c2-43fe-ae32-798e0f36a975` in bounded
friction mode, supplying client `claude` and conversation session ID
`1e4f73e5-7796-45c4-9684-86a1ad415395`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

Recommended fix (the author's recommendation): add a `style-rule-18` entry to
`$script:CandidatePatterns` in `Find-SessionCandidates.ps1`. The observed
violation sits on one added line, so a line pattern detects it mechanically,
matching the scanner's existing candidates-only contract; step 10 of the worker
then adjudicates it like any other `style-rule-<n>` row, and the mandate
sentence in worker.md step 7 already admits every scanner kind. The pattern
matches an added line that declares with a leading top-level `const` a
non-reference, non-pointer variable initialized by `=`, `{` or `(`; its
`Except` clears `constexpr`, `static`, and any line whose declarator carries
`&` or `*` (rule 18's permitted reference and pointee contracts). Place the
entry after `style-rule-15` so `const auto` lines keep reporting rule 15 first.
Rationale: the scanner is deterministic and cannot be skipped by a worker's
hand read, which is how this line escaped.

Fallback, if the pattern cannot be made to exclude function parameters,
members, and namespace-scope declarations without heavy false positives: add
rule 18 to worker.md step 7's hand-read list instead.

Either way, removing the top-level `const` from a local is an auto-fix only
when the variable is not used in a constant expression (rule 25 makes such a
variable `static constexpr`); otherwise route it per worker.md step 12.

## Critical files
- `.agents/scripts/Find-SessionCandidates.ps1`
- `.agents/skills/code-style-review/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `Find-SessionCandidates.ps1`
  `$script:CandidatePatterns` (a new `style-rule-18` entry and the kind-list
  comment above it) and, only for the fallback or if the new kind needs a step
  10 adjudication note, `worker.md` steps 7 and 10

## Out of scope
- The landed change the session produced, including
  `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`
- `.clang-tidy` and enabling `misc-const-correctness`
- `Documents/C++StyleGuide.txt`; sweeping existing rule 18 violations in
  unchanged code
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior): trigger is a behavior change in a
review skill's candidate scanner. Escalate if the fix reaches build/bootstrap
coordination. The scanner stays read-only and candidates-only. Never embed
transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  after a temporary, uncommitted scratch edit that adds
  `const int64_t iPageLimit = std::max<int64_t>(iLimit, 0);` inside a function
  body of a tracked C++ file, a scanner run with `-Baseline` set to the full
  40-character SHA of `HEAD` (from `git rev-parse HEAD`) and no `-Head`
  reports one `style-rule-18` hit on that line, or, under the fallback,
  `/code-style-review` reports the rule 18 finding; revert the scratch edit
  afterwards
- Under the scanner fix, an added `const T&`, `const T*`, `constexpr`, or
  `static const` declaration in the same scratch edit produces no
  `style-rule-18` hit
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
