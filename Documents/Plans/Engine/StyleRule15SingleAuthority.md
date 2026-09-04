<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T00:59:29.889Z","dependsOn":[]} -->
# Style rule 15's permitted `auto` forms live in one place

## Context
`Documents/C++StyleGuide.txt:80-87` owns style rule 15: `auto` is forbidden
except for an XMVECTOR/XMMATRIX result, a type obvious from a template parameter
on the right, an iterator, a structured binding, and a lambda expression
assigned directly to the variable, and it is forbidden outright in range-based
`for` loops.

Two other tracked files restate that same permitted set:

- `.agents/skills/code-style-review/references/worker.md:110-113` (a `## Rules`
  bullet) spells out all five permitted forms and the range-based-`for`
  prohibition in prose. These bytes are pre-existing: they are unchanged at
  baseline `56816149`, where they sit at lines 107-110.
- `.agents/scripts/Find-SessionCandidates.ps1:44` encodes the same permitted set
  in the `style-rule-15` entry's `Except` regex
  (`auto\s*&?&?\s*\[|\bauto\s+(?:vec|mat)|\bauto\s*&?\s+(?:it|\w+It)\b|=\s*\[|<[^<>]*>\s*[({]|\bdecltype\s*\(\s*auto\s*\)`).

The session that added the scanner entry made this the third copy of one rule,
which is the residue `/progressive-disclosure-review` reported at that session's
Step 6. The root `AGENTS.md` progressive-disclosure directive requires a fact to
live once at its owning layer and be referenced elsewhere; the style guide is
that owning layer, and the `code-style-review` skill's own invariant is that
every row cites a rule number rather than restating the rule. Nothing is
currently wrong at runtime — the three copies agree today — so this is drift
risk, not a defect: a future edit to rule 15 would silently leave the prose
bullet and the regex behind.

## Design
Recommended resolution, with the reasoning behind each half:

1. Replace the `worker.md` `## Rules` bullet's restated list with a citation of
   `Documents/C++StyleGuide.txt` rule 15. The bullet's only job is to tell the
   `mechanic` that rule 15 has permitted forms and that step 9 adjudicates them
   against the guide; naming the five forms adds nothing the worker cannot read
   from the authority it is already told to have in hand at step 6. Recommended
   replacement shape: one sentence pointing at rule 15 as the list of permitted
   forms, keeping any adjudication guidance that is genuinely the skill's own
   (not the guide's).
2. Keep the scanner's `Except` regex, and change only its accompanying comment.
   The author's recommendation is that the regex is an *implementation* of rule
   15 rather than a copy of its text: it is an approximate line-level matcher
   whose job is to suppress obvious permitted forms so the candidate list stays
   short, and it deliberately does not reproduce the rule (it cannot see types,
   and it matches on spelling conventions such as `vec`/`mat` and `it` name
   prefixes that the guide never states). Deleting it would defeat the
   scanner's purpose; duplicating rule text is not what it does. What should
   change is its documentation: the entry's comment should cite rule 15 as the
   authority and describe the regex as an approximation of the permitted forms,
   rather than restating what those forms are.

The alternative — treating the regex as a third copy and removing it — is not
recommended: it would push every bare `auto` into the candidate list and undo
the noise reduction the scanner exists for. The decision between these two
readings is the one substantive judgment in this Plan and belongs to the
implementing session's reviewer if it disagrees with the recommendation above.

No rule text in `Documents/C++StyleGuide.txt` changes. The guide stays the sole
authority, and the scanner's emitted `kind` values, fields, exit codes, and
schema are untouched, so `code-style-review` behaves identically.

## Critical files
- `.agents/skills/code-style-review/references/worker.md` (the `## Rules`
  bullet restating rule 15)
- `.agents/scripts/Find-SessionCandidates.ps1` (the `style-rule-15` entry's
  comment)
- `Documents/C++StyleGuide.txt` (read-only rule authority)

## In scope
- The `## Rules` bullet in `.agents/skills/code-style-review/references/worker.md`
  that restates rule 15's permitted `auto` forms and the range-based-`for`
  prohibition — replace with a citation of rule 15
- The comment documenting the `style-rule-15` entry in
  `.agents/scripts/Find-SessionCandidates.ps1` — cite rule 15 instead of
  restating its permitted forms

## Out of scope
- Any rule text in `Documents/C++StyleGuide.txt`
- The `style-rule-15` `Except` regex itself and every other pattern-table entry,
  along with the scanner's schema, fields, `kind` values, exit codes, and
  `-IncludeUntracked` behavior
- Every other restatement elsewhere in `worker.md`, in
  `comment-classification.md`, or in any other skill, unless it is a
  restatement of rule 15 specifically
- Auditing the rest of the repository for unrelated duplicated style-rule prose

## Risk tier and invariants
Expected Tier 1 (mechanical documentation and comment work, behavior-preserving,
with no public signature or invariant exposure); this author's classification,
to be confirmed at the implementing session's Step 1. Trigger: the change is
instruction prose and one script comment only. Invariants to preserve:
`Documents/C++StyleGuide.txt` remains the sole rule authority; the scanner stays
read-only and candidates-only, emitting the same rows for the same input; the
`mechanic` still learns from `worker.md` that rule 15 rows need adjudication
against the guide.

## Acceptance criteria
- `Documents/C++StyleGuide.txt:80-87` is the only tracked place that enumerates
  rule 15's permitted `auto` forms.
- Running `.agents/scripts/Find-SessionCandidates.ps1` over an unchanged input
  produces the same `style-rule-15` rows as before the change.

## Notes
Origin: a `/progressive-disclosure-review` residual raised at Step 6 of the
session that added the `style-rule-15` scanner entry. No acceptance criterion of
that session was unmet; the item was recorded as a residual because the
`worker.md` `## Rules` band was outside that change's approved boundary and the
prose bytes were pre-existing.
