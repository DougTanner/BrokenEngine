# Analyze Visual Studio CPU Captures Worker

The direct researcher's analysis steps and rules. Triggers, inputs, and the
report live in [`../SKILL.md`](../SKILL.md). Read
[`capture-forensics.md`](capture-forensics.md), which owns steps 1-4, before
analyzing or reporting; the steps below continue its numbering at 5.

## Steps

5. Cluster sibling leaves under one cause, especially in Debug, and decide what
   is actionable from each cluster's measured share:

   - Below 1%: never a standalone plan.
   - 1–3%: plan only for Effort 1–2 or a shared clustered root cause.
   - 3–10%: plan algorithmic/data-layout cost; measured share is the gain
     ceiling.
   - At least 10%: always investigate the root cause, including config-looking
     or memory-helper cost.

   Done when every cluster carries a share band and a plan-or-no-plan decision.
6. Treat accepted Debug costs (`/RTC`, Vulkan validation) and expected Profile
   overlay cost as yielding no plan. A proven configuration regression may yield
   a config plan; Profile/Release findings should target algorithm or data
   layout. Done when each cluster's plan decision reflects that split.
7. Inspect each hotspot's call sites and enclosing frame phase before
   classifying it or drafting a plan, and confirm whether its inputs or writes
   can affect PostRender/CRC state:

   - Capture membership only narrows the source search. Do not infer
     simulation, render phase, or determinism from client/server presence or
     absence.
   - Only confirmed PostRender exposure triggers the bit-identical constraint
     (`same float operations and order`, `/fp:strict`).
   - Record client-only visual or Interpolate classification only after the
     same source confirmation.

   Done when every hotspot's frame phase and PostRender/CRC exposure are
   confirmed from source.
8. Inspect every independent top non-OS/driver cluster directly, gathering full
   function bodies, call sites with enclosing loop and frame-phase context, and
   container/comparator types behind template hits. Include memory helpers when
   their clustered share is meaningful. Write each cluster's bounded evidence
   to its `Temp/analyze-diagsession/` file and cite its `## <hotspot cluster>`
   selector under `Evidence`. Done when every cluster has source context or an
   exact residual and every attribution is interpreted.
9. Return this run's profiling report in the [`../SKILL.md`](../SKILL.md)
    Handoff shape. Done when that report is returned.

## Rules

- Use deterministic tools for extraction, xperf, share computation, PDB checks,
  and profile-text searches.
- Inspect source context directly and retain verbatim quotes and file:line
  evidence for each independent hotspot cluster.
- Do not ask the user, decide proposal disposition, create a follow-up Plan,
  approve or perform landing, or involve another agent. Main owns those actions.
