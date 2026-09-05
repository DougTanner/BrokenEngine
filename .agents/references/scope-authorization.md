# Scope Authorization

The Review and resolve correctness step's review of each changed artifact type
also checks its own changed regions for scope authorization, diff-observable
minimality, and diff-observable simplicity. It applies to Tier-2+ changes only.

Authorization source: the approved plan's `## In scope` and `## Out of scope`
sections, or the explicit user-instruction list for unplanned work, plus the
execution card when one exists. Without it, report `Scope: not supplied` rather
than guessing.

1. Authorization pass: map each changed region to the `## In scope` entry or
   user instruction that authorizes it, counting the mechanical necessities the
   named change requires (includes, declarations). An unmapped region is an
   `unauthorized` finding; a region matching an `## Out of scope` entry is
   likewise `unauthorized`.
2. Minimality pass over added bytes only: flag an unused option, a speculative
   path with no current consumer, one-use indirection with no required
   contract, or backward-compatibility code the authorization source did not
   request.
3. KISS pass, diff-observable only: flag complexity visible in the diff itself
   that a plainly simpler form of the same authorized change avoids. Do not
   hunt the repository for simplifications.

Every finding cites the specific clause violated or states the specific
authorization that is absent. No clause named, no finding.

When all three passes above find nothing, report one line in the review's
declared `Scope:` field — a single `PASS` covering the authorization,
minimality, and KISS passes together. Passing passes get no `Decisive checks`
row and nothing else in the handoff.
