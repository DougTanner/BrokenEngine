<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T22:07:02.947Z","dependsOn":[]} -->
# Fix: root AGENTS.md module-import exemption presumes each skill documents the import line

## Context

Root `AGENTS.md:101`, Directives, "Bundled scripts as documented", exempts
module imports from the bundled-script rules with: "Importing a `.psm1` module
is not a script run and is exempt from both rules: keep its documented
`Import-Module <repo-relative path>` form followed by the function call in the
same shell call".

That wording only tells a reader to keep a form the owning skill has already
documented. When a skill names an exported module function without stating its
import line, the directive supplies no usable invocation, and the only
invocation form the same bullet spells out literally is the `.ps1` one —
`pwsh -NoProfile -File <repo-relative script path>`, worked through the example
`.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`, itself a next-plan
script. A reader therefore generalizes the `.ps1` form onto a module function
and the command fails, because `pwsh -File` requires a script file.

That is exactly what happened for `Get-NextPlanContext`, an exported function of
`.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1`: the reader ran
the `pwsh -NoProfile -File` form against that function name as a `.ps1` path
under `.agents/skills/next-plan/scripts/`, and PowerShell exited 64 with "not
recognized as the name of a script file", because no such script exists.
`.agents/skills/next-plan/SKILL.md:23-24` states that skill's import
line, so the next-plan instance is closed; the directive-level gap that let the
wrong generalization look correct is not, and applies to every other skill that
names an exported module function.

The canonical form is already written elsewhere in the repository —
`.agents/references/subagent-reporting.md:32-38` gives
`Import-Module ./.agents/scripts/AgentWorktreeSession.psm1` followed by the
function call, and explains that the leading `./` is required or PowerShell
searches `PSModulePath` instead of the worktree — so the directive is the one
place the form is missing.

## Design

Amend the single "Bundled scripts as documented" bullet in root `AGENTS.md` so
its module-import exemption states the canonical module-import form itself
rather than deferring to whatever the owning skill documented, keeping the
existing same-shell-call requirement and the reason for it. The stated form is
`Import-Module ./<repo-relative .psm1 path>` followed by the function call in
the same shell call, with the leading `./` required so PowerShell resolves the
worktree path instead of a `PSModulePath` module name, and with the `-File`
script form explicitly not applicable to a module function.

Wording only: no rule is added or removed, no skill is edited, and the
`.ps1` canonical form, its `Get-NextPlanList.ps1` example, the one-invocation
rule, the Bash/PowerShell tool split, and the array-argument exception all stay
byte-unchanged.

## Critical files

- `AGENTS.md` — Directives, the "Bundled scripts as documented" bullet, module-
  import exemption sentence only.
- `.agents/references/subagent-reporting.md` — read-only source of the canonical
  form and the `./` requirement; not edited.

## In scope

- The module-import exemption sentence inside the "Bundled scripts as
  documented" bullet of root `AGENTS.md`.

## Out of scope

- Every other sentence of that bullet and every other Directive.
- Any `.agents/skills/**/SKILL.md`, including `.agents/skills/next-plan/`, whose
  own `Get-NextPlanContext` invocation gap is already fixed.
- Changing the module-import behavior itself, any `.psm1`, or any script.
- Adding a new requirement that skills must restate the import line.

## Risk tier and invariants

Expected Tier 1: documentation wording in root `AGENTS.md` that makes an
existing rule self-sufficient without changing what any agent is required to do.
Escalate to Tier 2 if the resulting wording changes the required invocation
rather than stating it. The invariant that must survive: an imported module's
function call stays in the same shell call as its `Import-Module`, because the
imported functions must land in the caller's own session.

## Acceptance criteria

- The module-import exemption in root `AGENTS.md` gives a reader a complete,
  runnable import invocation without consulting any skill file.
- The `.ps1` canonical form, its example, the one-invocation rule, the tool
  split, and the array-argument exception are unchanged.
- `plan validate` exits 0.

## Notes

Related but distinct: `Documents/Plans/Skills/NextPlanContextInvocation.md`
covered the one skill-level instance (next-plan not stating its own import
line). This Plan owns the directive-level cause and applies repository-wide; it
depends on nothing, because that skill-level fix lands with the session that
recorded this Plan.
