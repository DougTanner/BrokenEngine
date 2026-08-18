# Remediation

Treat clone and complexity metrics as navigation, not verdicts. No published result proves that a
given intervention lowers Broken Engine's metrics.

Inspect the concrete clone interval, semantics, ownership, and callers first. Prefer deleting dead
duplication, reusing an existing helper, or extracting/parameterizing genuinely common logic. Preserve
deliberate client/server and collection mirrors when their contracts differ. Do not game the metric
with trivial wrappers, helper proliferation, removing valid checks, denominator changes, or an
improvement to one metric that worsens another metric.

For erosion evidence, identify a cohesive responsibility or branch family, then apply the smallest
behavior-preserving decomposition or existing dispatch pattern. Every recommendation still requires
source inspection, normal verification, and remeasurement. Keep metrics advisory: never automatically
remediate, score contributors, or create a quality gate from them.

For excess-decision evidence, prefer in-place deletion, merging, flattening, or existing dispatch.
Extract only for independently meaningful responsibility, reuse, or a genuinely separate
abstraction. One-shot sequential or threshold-driven extraction is metric-neutral. A target decrease
proves simplification only when the diff shows decision removal and the corpus shows no attributable
offset elsewhere, or a separately evidenced structural benefit independently justifies extraction;
structural erosion alone is navigation.
