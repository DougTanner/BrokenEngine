<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:54:49.226Z","dependsOn":[]} -->
# Trim engine UI Screens agent documentation

## Context

`Engine/Source/Ui/Screens/AGENTS.md` measures 1,835 `bt-token-v1` tokens against a 1,362-token formula budget (6,060 direct code tokens, one direct child document), an advisory excess of 473. `## Shared Authoring Rules` repeats visual layout guidance from `Documents/UserInterfaceDesign.txt` and shared helper ownership from the parent UI document, while several screen-specific bullets carry extended rationale.

## Design

The author recommends retaining runtime and automation contracts in the Screens document, replacing general visual-design mechanics with one direct authority link, and shortening per-screen explanations to observable invariants. Preserve standard-menu game boundaries, model lifetime/reread rules, constructor lifetime, feature gating, automation labels, opacity registration, font-stack ownership, and persistent naming behavior.

## Critical files

- `Engine/Source/Ui/Screens/AGENTS.md`

## In scope

- Condense `## Standard Menu Contract` ownership and feature-gating explanations without changing behavior.
- Shorten `## Screen-Specific Contracts` rationale for audio naming, Time of Day, graphics controls, samplers, timers, and header actions.
- In `## Shared Authoring Rules`, replace layout rules already owned by `Documents/UserInterfaceDesign.txt` with one direct link, and remove duplicated parent UI helper rules without linking the parent document; retain screen-local automation, lifetime, opacity, font-stack, and helper-boundary rules.
- Tighten `## See Also`.

## Out of scope

- C++, UI layout or behavior, persisted names, automation labels, game/engine ownership, or harness behavior.
- Editing the design guide, parent UI, child TweaksScreen, or game screen documentation.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the formula budget or explain unavoidable operative excess.
- All screen-local runtime, lifetime, automation, feature-gating, and ownership contracts remain explicit.
- General layout mechanics appear once at `Documents/UserInterfaceDesign.txt` and are directly linked.

## Classification

Tier 1 — documentation-only condensation.

## Coordination

None.

## Notes

Do not rename player-visible or harness-visible labels during documentation trimming.
