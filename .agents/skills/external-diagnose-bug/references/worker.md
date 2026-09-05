# Diagnose Bug Worker

The numbered diagnosis steps and the rules they assume. Triggers, the purpose
boundary, and the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

### Establish a reproducing signal

1. Build a reproducing signal before forming any theory: the tightest practical
   pass/fail evidence that goes red on *this* bug and green once it is fixed —
   normally one agent-runnable command already run at least once, quoted with
   its output. Any of these satisfies the gate:

   - a command that drives the actual failing path and shows the exact symptom
     the user described, not a nearby failure;
   - close code inspection that shows the bug unambiguously and
     deterministically;
   - supplied logs, a crash dump, or a capture that directly evidences the
     failure.

   Done when one of those three forms of evidence is in hand.
2. Build the command from these sources, in rough order of preference.

   1. `/agent-harness` — a scenario driving the sim, UI, or scene queries; its
      replay determinism sequence for desync and CRC mismatch.
   2. `get_logs` at a lower log-level threshold (`set_log_level`), so the
      failing transition is visible without unrelated noise.
   3. `/compile` — for build, link, and static failures.
   4. A differential run: the same input through the old and new build or two
      configurations, diffing the output.
   5. `/analyze-diagsession` — for a performance regression, measure a baseline
      before touching anything.

   Done when the command's source is chosen from this list.
3. Tighten whatever signal you get: assert on the specific symptom, cut
   unrelated setup, and pin sources of variation (seed, tick count, fixed data
   baseline). Done when the signal asserts on the specific symptom with
   unrelated setup cut and variation pinned.
4. For an intermittent bug, aim for a higher reproduction rate rather than a
   clean one — loop the trigger, add load, narrow the timing window. A failure
   that appears half the runs is diagnosable; one in a hundred is not. Done
   when the reproduction rate is high enough to diagnose.
5. When no signal can be built, say so, list what you tried, and ask the user
   for the environment, a captured artifact, or permission to add temporary
   instrumentation. Stop there; do not hypothesize without a signal. Done when
   that request is made and no hypothesis has been formed.
6. Name the signal and show it going red before continuing. Done when the
   signal is named and its red result is shown.

### Reproduce and minimize

7. Rerun a runnable signal; recheck static or captured evidence against the
   same symptom. Either way, confirm it produces the user's failure mode and
   capture the exact symptom text, wrong value, or timing. Done when that
   confirmation and capture are in hand.
8. Shrink the scenario one element at a time — inputs, units, cells, config,
   steps — re-running after each cut, or narrow the inspected code path or
   captured evidence without losing what proves the failure. Done when removing
   any remaining element makes the failure disappear.

### Rank hypotheses

9. State three to five hypotheses before testing any of them, ranked most to
   least likely. Each states its prediction: "if X is the cause, then changing
   Y removes the failure." Done when each ranked entry carries its prediction.
10. Sharpen or drop every hypothesis you cannot state a prediction for; it is a
    guess. Done when no prediction-less hypothesis remains.
11. Show the ranked list to the user before testing; they often re-rank it
    instantly. Proceed with your own ranking if they are away. Done when the
    ranked list has been shown.

### Instrument

12. Test one variable at a time, each probe tied to a named prediction. Prefer
    targeted logs at the boundary that separates two hypotheses over broad
    logging. For a performance regression, measure rather than log. Done when
    every probe is tied to a named prediction.
13. Tag every temporary log with a unique marker, for example `[DEBUG-a4f2]`,
    so removal is one search. Done when every temporary log carries that
    marker.
14. Treat instrumentation as temporary session state: it stays in the diagnosis
    worktree, never lands, and is removed before the handoff. Done when no
    instrumentation remains at the handoff.
15. Gather evidence until it confirms exactly one hypothesis, or falsifies the
    current set — which sends you back to step 9 to rerank. Done when exactly
    one hypothesis is confirmed or the current set is falsified.

### Confirm and hand off

16. Confirm the root cause before reporting it: either close code inspection
    shows it unambiguously and deterministically, or the evidence you gathered
    directly demonstrates it. Done when the root cause is confirmed by one of
    those two routes.
17. When that confirmation is uncertain, say so and re-investigate; "I know what
    the bug is" is not confirmation, and never fabricate a justification when
    challenged. Done when the uncertainty is stated and re-investigation has
    run.
18. Verify before reporting:

    - every `[DEBUG-...]` log removed (search the marker);
    - every throwaway script and scratch file deleted;
    - the reproducing signal still goes red — rerun it when it is runnable,
      recheck the inspection or capture when it is not.

    Done when all three verifications pass.
19. Propose the acceptance check that matches the reproducing signal: a harness
    scenario or replay determinism check where one applies, otherwise the check
    that settles that signal, such as a `/compile` result or a profiling
    baseline. Done when one acceptance check is proposed.
20. When no check can exercise the real failure pattern at its call site, report
    that missing verification seam as a residual instead of proposing
    architecture work to create one. Done when that residual is reported.
21. Return the handoff form in [`../SKILL.md`](../SKILL.md). The manager takes
    the handoff into the Change Workflow Approve and classify step, which
    classifies the fix and routes it to `/resolve-findings` or a plan. Done when
    that handoff form is returned.

## Rules

- Report progress at each `###` phase heading so the user can re-rank or stop
  the work.
- The signal-source skills own their own timeouts and run duration; do not
  invent a speed budget for them.
- Do not add unit tests.
