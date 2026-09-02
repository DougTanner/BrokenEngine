# Report

```markdown
# Next-plan review: <subject> (<short hash>)

## Executive verdict
- Transcript provenance: PROVEN | BLOCKED
- Governing-scope conformance: aligned | partial | divergent | unverified — <basis>
- Solution minimality: minimal | mixed | overengineered | unverified — <basis>
- Process assessment: <outcome>
- Execution-model routing: compliant | compliant fallback | violation | unverified — <affected-child count; exposed token/active-time cost or unavailable; basis>
- Control-work share of observed active agent-time: <point share | lower–upper bound | unverified> — <T, coverage, confidence, basis>

## Evidence timeline
| Time | Event | Evidence | Assessment |
|---|---|---|---|

## Findings by concern
### Governing-scope conformance
### Solution minimality
### Workflow coverage
### Token efficiency
### Execution-model routing
| Parent event / child or headless route | Relationship evidence | Actual concern | Requested role/type and explicit model/effort | Commit-time configured model/effort mapping | Actual executor/model/effort proof | Fallback evidence | Verdict | Tokens / active time | Citation |
|---|---|---|---|---|---|---|---|---|---|
### Control-work measurement
| Agent/session | Control work | Actual work | Unattributed | Excluded pauses/waits | Coverage | Control-work share/bounds | Confidence | Evidence |
|---|---:|---:|---:|---:|---:|---:|---|---|
### Process overhead
### Worktree isolation and landing
### Speed

## Proposed improvements (highest priority first)
1. **P0 | P1 | P2 — <action>**
   - Evidence: <source>
   - Change: <specific workflow/script/instruction>
   - Expected benefit: <benefit>
   - Tradeoff: <cost or no meaningful cost>
   - Control-work ranking: <measured burden, frequency, unique signal, and safety risk; when applicable>

## Strengths to preserve
- <proven control or none identified>

## Residual uncertainty
- <gap or none>
```

Keep findings separate from recommendations. Omit empty recommendations rather
than manufacturing work. Each minimality recommendation names the unnecessary
mechanism, a simpler removal or consolidation alternative, and why it preserves
governing scope and required invariants. Prefer deterministic tooling, a clearer
precondition, or a specifically justified removal over generic care advice, and
explain the safety tradeoff of weakening any tier-required control. Rank a
mechanism fix, a skill-to-skill contract correction, or a deleted obligation
above any new rule an agent must remember; a proposed rule states why the
mechanism could not be fixed, and weighs per-change cost against how often the
problem fires. Rank proven `candidate removable` control work by
burden/frequency, unique signal, and safety risk: higher burden/frequency, lower
unique signal, and lower safety risk rank first. Place it before recommendations
that add or retain control work. A demonstrated higher-risk P0/blocking issue may
outrank such a removal; otherwise do not let a required-control or new-rule
recommendation displace proven lowest-value removable control work.
