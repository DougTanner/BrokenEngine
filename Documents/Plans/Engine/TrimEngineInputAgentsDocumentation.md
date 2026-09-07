<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:54:13.028Z","dependsOn":[]} -->
# Trim engine Input agent documentation

## Context

`Engine/Source/Input/AGENTS.md` measures 1,849 `bt-token-v1` tokens against a 1,149-token formula budget (4,271 direct code tokens, zero direct child documents), an advisory excess of 700. The largest removable region is the single long `## Scroll Ownership` paragraph, which combines the core ownership rule with fixture history and detailed ImGui timing examples.

## Design

The author recommends keeping the scroll routing predicate and its known one-frame/two-poll tolerances, while removing scenario narration and implementation-history detail that can be recovered from code or focused plans. Also tighten the introductory include/guard explanation and the DirectXTK keyboard rationale without weakening client-only ownership, poll lifetime, focus, or synthetic-input contracts.

## Critical files

- `Engine/Source/Input/AGENTS.md`

## In scope

- Condense lines 3-5 and `## Poll Lifetime` by removing repeated ownership and include mechanics.
- Rewrite `## Scroll Ownership` as concise routing, tolerance, baseline, and child-window-extension rules; remove coordinate-command examples and repeated ImGui implementation narration.
- Tighten the DirectXTK keyboard rationale and `## See Also` descriptions.

## Out of scope

- Input code, project membership, bindings, focus behavior, camera/UI wheel routing, or agent-input behavior.
- Removing the accepted hover timing tolerance, plot ownership delay, child-window caveat, or lifetime-baseline rule.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the formula budget or identify the remaining operative rules causing unavoidable advisory excess.
- Poll lifetime, client-only affinity, keyboard ownership, wheel routing/tolerances, mode switching, focus, and synthetic-input contracts remain unambiguous.
- No historical scenario is retained when a shorter invariant states the same constraint.

## Classification

Tier 1 — documentation-only condensation.

## Coordination

None.

## Notes

The advisory budget does not authorize changing the documented accepted wheel-routing behavior.
