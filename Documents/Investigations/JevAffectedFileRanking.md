# Jev for ranking the files a change must touch

Open question: can Jev reorder the search hits `/update-affected-code` and
`/prepare-change` walk, so the files that genuinely depend on a change come
first and the topical near-misses last? Part of the series in
`JevDecisionModelWorkflowUses.md`. Not piloted; the corpus is described below.

## The decision today

`/update-affected-code` propagates a change "to every correctness-dependent
caller, producer, consumer, mirror, serialization identity, and CPU/GPU
contract the implementation did not update", by search: the brief supplies
each owned change's symbol or pattern and search scope, and the worker walks
every hit and reports verified absence rather than inferring it
(`.agents/skills/update-affected-code/SKILL.md:36-47`). `/prepare-change`
names the critical files a plan will touch from the same kind of search. Both
walk the hits in search order, and a symbol like `Reset` or `Update` returns
hundreds.

## The question to Jev

The rerank shape from the TypeSafe docs: code produces the shortlist (ripgrep
over the symbol, capped), then one `noul` per (change, hit) pair whose
probability is the sort key. The question must be about dependence, not
topic — the docs' own rerank cookbook got its gain by asking "does this passage
establish the specific proposition" rather than "is it on the same subject".
Here: "does the code at this hit read, write, mirror, serialize, or otherwise
depend on the changed behavior described, so that leaving it unchanged would
be wrong". State is a JSON object with the change description (the plan's
`## In scope` clause or the diff hunk), the hit's file path, and the hit line
with its enclosing function. The `false` criterion names the near-miss: same
symbol name in an unrelated type, a comment mention, a test double.

Every hit is still walked; the ranking decides order and lets the worker
report "verified absence" for the tail with the probability beside it, never
skip it.

## What still needs a full model

Every edit, and the verified-absence judgment for hits Jev ranked low, which
the worker still reads. Jev changes the order of reading, not what is read.

## The corpus

Landed commits that completed a Plan: the Plan text at the parent commit is
the change description, the files the commit changed are the positives, and
the ripgrep hits for the Plan's named symbols in files the commit did not
change are the negatives. Recent history has dozens of such commits (their
messages begin `Completes Documents/Plans/...` or name the Plan), so the corpus
is buildable in a script without hand labelling. Its noise: a commit may have
changed files for reasons outside the Plan, and a hit the commit left alone may
be a propagation the session missed — the second is exactly the case this
candidate exists to catch, so a disagreement there is worth a human read.

## What would make this a Plan

Success: over at least 20 Plan-completing commits, the changed files rank in
the top third of the shortlist at least 90% of the time, measured as the
rerank cookbook measures (top-k hit rate before and after reordering). The
wiring is a sort in the worker's own search loop, so a failed call falls back
to search order with a residual line, matching the behaviour today.

## Decisions a Plan needs

1. Where the shortlist is capped, and what the shortlist search is: the
   symbol alone, or symbol plus type name.
2. The state for a hit: the hit line, the enclosing function, or a fixed
   window; the docs' warning about unrelated context argues for the function.
3. Whether the probability is shown in the handoff's verified-absence rows.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`.
