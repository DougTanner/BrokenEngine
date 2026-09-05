# Coherence Review Worker

The review steps and the judgment rules the dispatched reviewer runs. Triggers,
inputs, modes, and the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Fix the mode and the components it carries from the brief, then read every
   changed region the brief names. Done when the mode and the component list are
   settled and each changed region has been read.
2. Review those changed bytes against the rules they touch: a statement that
   contradicts the source it cites, a fact that now disagrees with its owning
   location, an instruction that cannot be followed as written, and a path,
   link, or anchor that does not resolve. This component passes when no accepted
   semantic finding remains unresolved. Done when every changed region has been
   judged that way.
3. At Tier 2+, run the authorization, minimality, and KISS passes of
   [`scope-authorization.md`](../../../references/scope-authorization.md) over
   the same regions. Done when each pass has run against the supplied
   authorization source, or its absence is reported as that reference directs.
4. At Tier 2+, when the dispatch allows edits, fix the meaning-preserving
   wording and formatting problems in those regions yourself and self-check each
   edit, and route what you cannot fix that way with the semantic findings. Done
   when each such problem is either fixed and self-checked, or reported.
5. In Tier-1 mode, run in this same context, in full, every reviewer-role check
   the Apply the triggered cleanup step of root
   [AGENTS.md](../../../../AGENTS.md) lists — today `/validate-skill` and
   `/progressive-disclosure-review`, each on the trigger that step states;
   never route one to another worker. Each passes on its own skill's pass
   condition. Done when each triggered skill has run in full and its handoff is
   placed as `../SKILL.md` `## Handoff` states.
6. In Tier-1 mode, when the Verify the acceptance table step applies — a stage
   completing without landing — verify the acceptance table as
   [`../../verify-acceptance/SKILL.md`](../../verify-acceptance/SKILL.md)
   requires, under the Tier-1 evidence ceiling it applies. This component passes
   when every approved criterion and invariant maps to such evidence. Done when
   the `Criteria` rows are complete.

## Rules

- The Apply the triggered cleanup mechanic and implementer checks —
  `/code-style-review`, `/update-vcxproj`, `/update-claude-docs` — are not
  reviewer dispatches and are unaffected by the combined mode.
- Change no repository file apart from the wording and formatting fixes step 4
  requires; every semantic problem is a finding for the manager to decide.
- The Review and resolve correctness step's fix-round rule is unchanged: main
  decides once, accepted fixes go to a separate `implementer`, and only affected
  regions are re-reviewed. A re-run restates only the criterion rows whose
  evidence the fix changed.
