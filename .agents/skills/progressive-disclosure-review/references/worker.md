# Progressive Disclosure Review Worker

Steps and rules for the dispatched reviewer. The public
[`SKILL.md`](../SKILL.md) owns the triggers, inputs, and handoff form.

## Steps

1. Run below with zero disk writes (`-Head` from `Baseline` for a committed
   head; `-UntrackedPath` or `-IncludeUntracked` from `Scope`). Require `status`
   `pass`, else report unavailability without enumerating hunks. Keep
   instruction-doc paths and read `counts.unlistedUntracked`. Done when ready or
   unavailable.

```powershell
pwsh -NoProfile -Command 'Import-Module ./.agents/scripts/SessionChangeInventory.psm1; Get-SessionChangeInventory -RepositoryRoot (Get-Location).Path -Baseline <baseline SHA> -Regions|Select-Object status,coretide,counts,entries,regions|ConvertTo-Json -Depth 5'
```

2. Flag changed prose that restates or paraphrases content another location
   already owns — a reference doc, a script's own documented usage, a parent
   `AGENTS.md`, another skill, elsewhere in the same file, or code and comments
   reached through pointers in the changed prose. Changed prose copying a value
   the root `Progressive disclosure` directive forbids, with or without a unit
   (`default 2`), is a `duplication` finding when the owning symbol can be
   named, and that symbol is the replacing reference. Done when each such
   finding cites the changed location and the owning location and gives the
   reference that replaces it.

3. Flag detail sitting above its owning layer — a schema, mechanics, or a long
   example in a `SKILL.md` body instead of `references/` or a script; a
   subsystem constraint narrated in a skill instead of the owning `AGENTS.md`;
   local rationale in an `AGENTS.md` instead of a code comment.

   This includes changed prose that computes a repeatable verdict from
   explicit machine-readable inputs through Git/range/path parsing, set
   operations, schema-field mechanics, or exit/status/retry algorithms. Done
   when each such finding names the changed prose, its deterministic inputs and
   result, and the script/reference that owns the computation or where it
   should be extracted.

4. Flag as `instruction-change` prose in the step 1 regions that breaks the
   root `Instruction changes` directive, leaving model names to the
   static-checks `model-names` row. When the dispatch supplies
   `Stated net size`, run the static-checks command of the Change Workflow Run
   targeted pre-review checks step, adding `-Head <commit>` for a committed
   head, and flag its `instruction-size` row's `netTokens` above that size as
   `instruction-change` naming both sizes. Done when every step 1 region the
   directive covers is checked and any stated size is compared.

5. Measure each changed `SKILL.md` with `pwsh -NoProfile -File
   .agents/scripts/Measure-Tokens.ps1 -Path <file>`. A `SKILL.md` body over
   4,500 `bt-token-v1` (measured on the body with `-StartLine` when frontmatter
   pushes the file over) needs a stated reason why the detail cannot move to a
   reference or script;
   [`Validate-Skill.ps1`](../../external-skill-creator/scripts/Validate-Skill.ps1)
   `BODY002` enforces the mechanical body-byte ceiling separately. Done when
   every changed `SKILL.md` has a measurement and each body over 4,500
   `bt-token-v1` lacking a stated reason is a `NEEDS_ACTION` finding, not
   advice.

## Rules

- Keep when-to-invoke rules, ordering, typed-result branching, roles,
  authority, user interaction, and irreducible judgment in the skill; those are
  orchestration, not misplaced mechanics.
- Scope guard: judge only session-changed bytes. Report pre-existing excess in
  untouched prose as a residual and never demand trimming it.
- Precision guard: every finding names the owning location, the exceeded
  threshold, or the replacing text or mechanism. No such name, no finding.
- Leave frontmatter, discovery, invocation policy, and bundled-link mechanics
  to the `/external-skill-creator` findings-only validate mode.
- Leave `AGENTS.md` content correctness, chain sync, and advisory budget
  assessment to `/update-claude-docs`; its
  [`content-rules.md`](../../update-claude-docs/references/content-rules.md)
  owns the response to an over-budget verdict.
- Leave C++ and GLSL comment content to `/comment-review` and formatting to
  `/code-style-review`.
- Leave scope authorization and unnecessary extra work to the Review and
  resolve correctness review of each changed artifact type
  ([`scope-authorization.md`](../../../references/scope-authorization.md)).
