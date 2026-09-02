# Comment Classification

How `/code-style-review` classifies a selected changed comment: preserve it,
rewrite it, or route it.

- In selected changed comments, remove text that merely explains a language
  feature or established house pattern already visible in the declaration.
  Preserve invariants, required ordering and consequences, lifetime or threading
  contracts, and platform or driver workarounds. When such a comment calls for
  semantic enforcement — code could and should enforce its constraint, so the
  absent guard is a gap worth review — keep the constraint sentence, delete only
  the process or navigation wording that constraint makes redundant, and list
  the missing enforcement under Routed Findings for `/repo-code-review` or the
  implementing caller; the constraint comment stays until that enforcement
  exists. A constraint that legitimately lives only as a comment, such as a
  platform or driver quirk code cannot check, is preserved with nothing routed.
  Flag text that describes change history rather than the present code
  (Rule 64). When the surviving present-tense constraint is evident from the
  comment and the surrounding code, rewrite the comment to state that
  constraint; the reader's meaning is unchanged, so this stays inside the
  auto-fix boundary in [`worker.md`](worker.md). When it is not evident, route
  the comment as a finding rather than guessing at it.
