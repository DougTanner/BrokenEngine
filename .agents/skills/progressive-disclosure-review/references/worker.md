# Progressive Disclosure Review Worker

Steps and rules for the dispatched reviewer. The public
[`SKILL.md`](../SKILL.md) owns the triggers, inputs, and handoff form.

## Steps

1. Take the changed regions from the read-only inventory rather than
   re-deriving hunks: `pwsh -NoProfile -File
   .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <absolute
   repository toplevel> -Baseline <full 40-character SHA> -Regions` (add `-Head
   <commit>` for a committed head and `-IncludeUntracked <comma-separated
   paths>` for untracked files), filtered to instruction-doc paths.

   Only `status` `pass` is usable; any other status means the diff input is
   unavailable — report that instead of enumerating hunks inline. Read
   `truncated` and `counts.unlistedUntracked` from the result. Done when the
   filtered instruction-doc regions are in hand or the unavailable input is
   reported.

2. Flag changed prose that restates or paraphrases content another location
   already owns — a reference doc, a script's own documented usage, a parent
   `AGENTS.md`, another skill, elsewhere in the same file, or code and comments
   reached through pointers in the changed prose. Done when each such finding
   cites the changed location and the owning location and gives the reference
   that replaces it.

3. Flag detail sitting above its owning layer — a schema, mechanics, or a long
   example in a `SKILL.md` body instead of `references/` or a script; a
   subsystem constraint narrated in a skill instead of the owning `AGENTS.md`;
   local rationale in an `AGENTS.md` instead of a code comment.

   This includes changed skill prose that computes a repeatable verdict from
   explicit machine-readable inputs through Git/range/path parsing, set
   operations, schema-field mechanics, or exit/status/retry algorithms. Done
   when each such finding names the changed prose, its deterministic inputs and
   result, and the script/reference that owns the computation or where it
   should be extracted.

4. Measure each changed skill markdown file with `pwsh -NoProfile -File
   .agents/scripts/Measure-Tokens.ps1 -Path <file>`. A `SKILL.md` body over
   10,000 `bt-token-v1` needs a stated reason why the detail cannot move to a
   reference or script, 15,000 is the ceiling, and a reference file over 2,000
   needs a table of contents. Done when every changed skill markdown file has a
   measurement and each threshold breach is a `NEEDS_ACTION` finding, not
   advice.

## Rules

- Keep when-to-invoke rules, ordering, typed-result branching, roles,
  authority, user interaction, and irreducible judgment in the skill; those are
  orchestration, not misplaced mechanics.
- Scope guard: judge only session-changed bytes. Report pre-existing excess in
  untouched prose as a residual and never demand trimming it.
- Precision guard: every finding names the owning location or the exceeded
  threshold. No owner named, no finding.
- Leave frontmatter, discovery, invocation policy, and bundled-link mechanics
  to `/validate-skill`.
- Leave `AGENTS.md` content correctness, chain sync, and leaf/hub size targets
  to `/update-claude-docs`.
- Leave C++ comment style and formatting to `/code-style-review`.
- Leave scope authorization and unnecessary extra work to the Step-5
  correctness review of each changed artifact type
  ([`scope-authorization.md`](../../../references/scope-authorization.md)).
