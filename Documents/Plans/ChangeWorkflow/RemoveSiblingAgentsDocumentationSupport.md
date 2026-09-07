<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T16:14:50.126Z","dependsOn":["Documents/Plans/Engine/GraphicsManagerDocumentationConsolidation.md"]} -->
# Remove sibling AGENTS detail-document support

## Context

The `/update-claude-docs` skill currently recognizes sibling `*.AGENTS.md`
detail documents even though those files are outside the normal ancestor
`AGENTS.md` loading chain. This support appears in six locations:

- `.agents/skills/update-claude-docs/references/content-rules.md` defines a hub
  as linking detail documents and recommends a split when size targets conflict
  with needed guidance.
- `.agents/skills/update-claude-docs/references/audit-mode.md` excludes Graphics
  manager detail files from audit discovery and exempts linked detail files
  from CLAUDE stub requirements.
- `.agents/skills/update-claude-docs/references/worker.md` repeats the Graphics
  manager exclusion in the stub-sweep result contract.
- `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1`
  implements that exclusion in `$script:StubSweepExclusions`.

The public `.agents/skills/update-claude-docs/SKILL.md` contains no sibling
split instruction. Ordinary links to separately owned automatic AGENTS.md and
architecture documents, directory ancestor/descendant chains, hub/leaf token
targets, and the rule against discarding decision-changing knowledge remain
valid. No existing Plan owns removal of the sibling-detail policy.

This Plan depends on the Graphics manager consolidation so removing the special
case cannot temporarily classify the ten existing detail files as malformed or
orphaned directory memory.

## Design

The author's recommendation is to define a hub only through ordinary directory
`AGENTS.md` ancestry and descendant subsystem documents. Remove the sibling
detail-document vocabulary and replace overflow split guidance with in-place
trimming: preserve every decision-changing rule, shorten redundancy and
inventories, and report a target overrun when it cannot be resolved without
losing required meaning.

Remove the Graphics manager `*.AGENTS.md` exclusions from audit discovery,
stub-pair documentation, and `$script:StubSweepExclusions`. Keep the
bidirectional directory `AGENTS.md`/`CLAUDE.md` contract uniform. A nonstandard
`*.AGENTS.md` file receives no special discovery or stub exemption after the
dependency has removed the repository's only live instances.

Retain the rules for links to authoritative external owners, automatic
ancestor loading, named relocation of knowledge, directory hierarchy chains,
hub/leaf size measurement, audit/sync modes, and the public skill handoff. Edit
the public `SKILL.md` only if a changed reference description or link requires
it; it needs no policy rewrite on current evidence.

Change Workflow tier: Tier 2, scoped tool behavior. The references change skill
policy and the PowerShell discovery script changes which documents enter the
audit and stub sweep. The change does not affect engine runtime behavior,
public engine signatures, determinism/CRC, serialization, wire formats,
threading, trust boundaries, C++, GLSL, or project membership.

## Critical files

- `.agents/skills/update-claude-docs/references/content-rules.md`
- `.agents/skills/update-claude-docs/references/audit-mode.md`
- `.agents/skills/update-claude-docs/references/worker.md`
- `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1`
- `.agents/skills/update-claude-docs/SKILL.md`

## In scope

- Remove sibling `*.AGENTS.md` detail-document terminology, split guidance,
  audit exclusions, and stub exemptions from the three named references.
- Replace size-overflow split guidance with in-place trimming and explicit
  overrun reporting that never drops decision-changing knowledge.
- Remove the Graphics manager detail glob and its special-case rationale from
  `$script:StubSweepExclusions` in `Get-AffectedAgentsDocs.ps1`.
- Preserve automatic ancestor/descendant chains, hub/leaf token targets,
  ordinary authoritative links, named knowledge relocation, and bidirectional
  directory stub validation.
- Update the public `SKILL.md` only if required to keep its references or
  description accurate after the owned changes.

## Out of scope

- Consolidating or deleting Graphics manager detail documents; the prerequisite
  Plan owns that work.
- Changing engine or game documentation outside this skill package.
- Changing root progressive-disclosure policy, the
  `/progressive-disclosure-review` skill, token thresholds, audit scoring, or
  Change Workflow routing.
- Adding backward compatibility for sibling detail documents or another
  special-case exclusion.
- Changing C++, GLSL, project files, engine runtime behavior, or build output.

## Invariants

- Every directory `AGENTS.md` and sibling `CLAUDE.md` continues to participate
  in the same bidirectional stub contract, subject only to the retained general
  exclusions.
- Required knowledge remains in an automatically loaded AGENTS.md or a named
  source comment; size pressure never authorizes its deletion.
- Links to authoritative external owners and architecture documents remain
  allowed and distinct from sibling detail-document support.
- Discovery output retains its existing schema and error channels.

## Acceptance criteria

- Searches of the skill package find no instruction that recommends, defines,
  excludes, or exempts sibling `*.AGENTS.md` detail documents.
- `Get-AffectedAgentsDocs.ps1` run on the repository root and on a Graphics
  manager path returns `status=pass`, reports the ordinary governing chain,
  and reports no stub finding for the consolidated manager directory.
- The discovery result schema, path canonicalization, size measurements,
  output cap, and retained general exclusions behave as before.
- `/validate-skill` passes for `update-claude-docs`, and the applicable
  Markdown links, progressive-disclosure review, and PowerShell static checks
  pass.
- The diff is limited to the named skill package and contains no new
  compatibility path or special-case exclusion.

## Notes

The six support locations are four files with two distinct policy statements
in both `content-rules.md` and `audit-mode.md`. The public skill currently needs
no direct policy removal.
