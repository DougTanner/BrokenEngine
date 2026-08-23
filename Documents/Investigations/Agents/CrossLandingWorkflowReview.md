# Cross-landing workflow review

## Question

Should cross-landing history comparison be a new read-only skill or an
extension of `/next-plan-review`, and what reference bounds, comparability, and
provenance rules make its candidate output trustworthy?

## Current evidence

`/next-plan-review` reviews one landed change from a proven commit and parent or
child session evidence, with bounded transcript discovery and a read-only,
evidence-based improvement report. It already records governing scope,
minimality, workflow coverage, routing, measurement, and landing concerns. The
current contract does not define a cross-landing reference set, how unlike
changes become comparable, or how a candidate improvement remains read-only and
proven when multiple landings are examined.

## Options to compare

1. Add a bounded cross-landing mode to `/next-plan-review`. This reuses its
   provenance and trust rules but risks making a one-landing contract harder to
   reason about.
2. Create a separate `cross-landing-review` skill with its own explicit input
   and output contract. This keeps single-landing behavior narrow but adds a
   new skill and duplicated provenance vocabulary unless references are shared.
3. Keep cross-landing comparison as a manual, read-only analysis document
   until a recurring use case and evidence shape are proven. This avoids a new
   gate but offers less automation.

## Evidence to collect

- Explicit reference bounds: commit/ref list, parent/child window, plan or
  session scope, time range, and exclusion rules; no unbounded repository or
  transcript sweep.
- Comparability rules for tier, change size, artifact type, build/harness
  coverage, wait classification, and review/landing state. State when samples
  cannot be compared rather than normalizing them by guesswork.
- Provenance proof for every candidate landing and session, including actual
  route/receipt evidence and missing/unverified states; no config-only or
  filename-only attribution.
- Read-only candidate output shape that separates observed facts, supported
  inferences, and proposed follow-ups, with no automatic Plan claim, gate,
  score, persistence, or source mutation.

## Promotion criteria

Promote to a Tier 2 or Tier 3 implementation Plan only after the reference
bounds, comparability rules, provenance proof, and read-only output contract are
selected. Choose the new-skill or extension route from evidence about contract
overlap and recurring use, not convenience. The Plan must name the exact
existing sections or new files it changes and must preserve single-landing
provenance guarantees.

## Non-goals

- No cross-landing command, new skill, scheduler gate, score, persistence, or
  follow-up Plan is created by this investigation.
- No transcript paths, home paths, credentials, external mutations, or
  inferred landing attribution.

## Notes

The central discriminator is ownership of provenance and output semantics. A
new entry point is justified only if the existing single-landing contract
cannot express the bounded comparison without weakening its guarantees.
