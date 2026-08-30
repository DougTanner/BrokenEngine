<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T17:35:52.702Z","dependsOn":[]} -->
# Tier 1 Investigation deletion fast path

## Context

The post-landing review of commit `68d0840d1a0a8b6e692e3d1fcaab27c330ae812c`
found one changed path and one operation: deletion of
`Documents/Investigations/Agents/CppPlanTraceAudit.md`. The session lasted
22m03.988s. Deletion implementation took 78.246s and the Tier-1 combined
reviewer took 65.943s, while finalizer control work took 537.810s and the
final verifier took 209.621s. Control work measured 47.7–52.5% of active
agent-time. The current root Change Workflow at `AGENTS.md:78-85` does not
have a narrow deletion-only eligibility branch, so ordinary Tier-1
documentation deletion still incurs the Step 3 implementer and the Step 5
Tier-1 combined review before the landing gate.

## Design

The approved design is a deliberately narrow fast path for ordinary
Investigation Markdown. Main may perform the authorized deletion directly
only when the deterministic eligibility test below succeeds; all other work
uses the normal Change Workflow.

Eligibility is true only when every condition holds:

- The current stage is classified Tier 1.
- Every changed path has Git deletion status and is a normalized path under
  `Documents/Investigations/**/*.md`.
- No changed path is any `AGENTS.md` or `CLAUDE.md`, a symlink, a gitlink, or
  another non-ordinary mode.
- No path belongs to a Plan, Feature, instruction, skill, reference, code,
  script, shader, or project-membership surface. A path/status class outside
  the stated Investigation Markdown set makes the whole stage ineligible.
- The diff has no addition, modification, rename, mode-only change, mixed
  status, or other path class, and the authorized stage has no unresolved
  decision or finding.

For this exact class, main performs the authorized deletion directly and
prepares the candidate through the existing candidate/landing mechanisms. The
Step 3 implementation `implementer` dispatch and the Step 5 Tier-1 combined
review are omitted. Candidate identity and final-diff preparation remain
required; they are not replaced by an informal shortcut.

After candidate preparation, main retains exactly one fresh Step-8
`/verify-changes` reviewer bound to the final diff. That verifier uses the
Git-derived inventory plus Git history/reference search to prove authorization,
the deletion-only shape, that no tracked reference remains requiring
propagation, and that no hygiene, build, or runtime check is triggered. An
inconclusive eligibility or reference result falls back to the normal
workflow. The existing explicit confirmation, SmartGit/summary ordering where
applicable, landing lock/CAS, rollback, and cleanup remain in force. The
resulting route is:

`direct main deletion → candidate preparation → one final-diff verifier → explicit confirmation → landing and cleanup`

This clause removes obligations for the eligible class; it does not add a new
reviewer, new reminder rule, new script, configuration, or schema. Mixed,
added, modified, instruction, Plan/Feature, code, script, shader,
project-membership, symlink/gitlink, or unresolved-decision cases stay on the
normal workflow.

## Critical files

- `AGENTS.md:78-85` — classification, implementation/propagation, Tier-1
  combined review, acceptance, and landing routing where the clause belongs.
- `.agents/scripts/Get-SessionChangeInventory.ps1` — existing Git-derived
  status/path/mode inventory used to make eligibility deterministic; no new
  inventory format is needed.
- `.agents/skills/finalize-changes/SKILL.md:12-48,63-71` and
  `.agents/skills/finalize-changes/references/workflow.md:115-160` — existing
  candidate preparation, final-diff verifier ordering, confirmation, and
  landing ownership that the fast path must preserve.
- `.agents/skills/verify-changes/SKILL.md:43-84,86-168` — final-diff
  inventory, acceptance table, full-head binding, and landing verification
  contract.

## In scope

- Adding one deterministic eligibility clause to the root Change Workflow for
  Tier-1 stages whose complete Git diff is ordinary deletions under
  `Documents/Investigations/**/*.md`, with all stated exclusions and the
  unresolved-decision/finding fallback.
- Removing the Step 3 implementation-worker and Step 5 Tier-1 combined-review
  obligations only for that eligible class.
- Preserving candidate preparation, one fresh Step-8 `/verify-changes` pass
  over the final diff, explicit confirmation, landing lock/CAS, rollback, and
  cleanup, with the verifier's inventory/history/reference evidence listed in
  `## Design`.
- Documenting the normal-workflow fallback for every ineligible or
  inconclusive case and validating the affected instructions/skills as
  applicable.

## Out of scope

- Any behavior or routing change for Tier 2/3 work, additions, modifications,
  renames, mixed diffs, instructions, Plans, Features, skills, references,
  code, scripts, shaders, project membership, symlinks, gitlinks, or unresolved
  decisions/findings.
- Removing the final Step-8 verifier, explicit user confirmation, SmartGit or
  landing summary controls, landing lock/CAS, rollback, claim cleanup, or the
  final-diff/full-head binding.
- A generalized Tier-1 bypass, a new reviewer, a prose-only duplicate-search
  reminder, a new script/config/schema, instrumentation, compatibility mode,
  unit tests, or implementation during this recording stage.

## Risk tier and invariants

Plan-file authoring is current Change Workflow Tier 1 documentation. Executing
this Plan is Tier 3 because it changes manager/worker/reviewer routing and
landing controls. The implementation must preserve:

- deterministic Git status, normalized path, and file-mode eligibility with no
  best-effort classification;
- one fresh verifier bound to the final candidate diff and full head for the
  eligible class;
- exact user confirmation before the existing landing lock/CAS and cleanup;
- normal workflow coverage for every non-eligible, mixed, or uncertain case;
  and
- no new machine or document format beyond the existing inventory and landing
  contracts.

## Acceptance criteria

- A Tier-1 one-file Investigation Markdown deletion follows
  direct-main → candidate preparation → exactly one fresh final-diff
  `/verify-changes` reviewer → explicit confirmation → landing lock/CAS and
  cleanup, with no Step 3 implementer or Step 5 combined-review dispatch.
- The final verifier's evidence includes the Git inventory, authorization,
  deletion-only status/mode/path proof, Git history/reference search, absence
  of remaining tracked references requiring propagation, and the proof that
  no hygiene/build/runtime check is triggered.
- Any addition, modification, rename, mode change, mixed path/status class,
  `AGENTS.md`/`CLAUDE.md`, non-Investigation or non-Markdown path,
  symlink/gitlink, unresolved decision/finding, or inconclusive search remains
  on the normal workflow with its ordinary implementation and review gates.
- Candidate preparation, exact final-head binding, confirmation, landing
  lock/CAS, rollback, and cleanup remain covered by the existing finalization
  route; no primary mutation can occur without the retained verifier and
  confirmation.
- No new script, configuration, schema, compatibility input, or reviewer is
  introduced. Applicable `/progressive-disclosure-review` and
  `/validate-skill` checks pass for changed instruction/skill files, and the
  compact scheduler check remains valid.

## Notes

- This Plan has no dependency and is independently executable.
- The narrow class is intentionally a removal of existing obligations. The
  broader question of which other Tier-1 gates can be removed or consolidated
  is recorded separately in
  `Documents/Investigations/Agents/Tier1WorkflowFastPath.md`.
