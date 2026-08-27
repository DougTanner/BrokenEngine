<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:07.726Z","dependsOn":[]} -->
# Give camera wheel input back from collapsed ImGui windows

## Context

The retained survivor `CAI/shard-0033/002` identifies a mismatch between the
camera/UI wheel predicate and ImGui's actual scrollability. `Input::BeginPoll`
claims the wheel whenever a hovered window has positive `ScrollMax.y` and no
mouse-scroll disabling flag at `Engine/Source/Input/Input.cpp:111-118`, but it
does not check `ImGuiWindow::Collapsed`. Vendored ImGui preserves content size
while shrinking a collapsed window (`ThirdParty/imgui/imgui.cpp:6690-6701,
7691-7694,7886-7895`) and then returns before scrolling a collapsed target
(`imgui.cpp:10273-10295`). Current Tweaks sections are collapsible at
`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp:423-440`.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0033.md:64`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:894`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this
routing session has not changed source. The finding is distinct from ordinary
scrollable windows, stale-hover tolerance, and agent wheel arithmetic.

## Design

The author's recommendation is to include collapsed-state/actual-scrollability
in the wheel ownership gate, or share the same candidate condition used by
ImGui's `UpdateMouseWheel`. A collapsed title bar must fall through to camera
ownership; an expanded window with a real scroll range must continue to own
the notch. Keep the existing wheeling-lock, key-owner, and one-frame timing
rules.

## Critical files

- `Engine/Source/Input/Input.cpp:99-124` — camera/UI wheel ownership.
- `Engine/Source/Input/AGENTS.md` — scroll ownership contract.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp:423-440` —
  current collapsible root-window caller.
- `ThirdParty/imgui/imgui.cpp:6690-6701,7691-7694,7886-7895,10273-10295`
  — vendored behavior used as the reference; do not modify ThirdParty.

## In scope

- The hovered-window scrollability predicate used by `Input::BeginPoll`.
- Collapsed root-window behavior and preservation of existing expanded-window,
  wheeling-lock, and key-owner behavior.

## Out of scope

- ImGui source changes, new child-window support, or camera zoom scaling.
- Synthetic wheel parsing, RawInput lifetime publication, and unrelated menu
  layout work.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped client input/UI
behavior with no wire, serialization, deterministic simulation, or CRC impact.

Preserve these invariants:

- A wheel notch is owned by a UI target only when that target can consume it.
- A collapsed/non-scrolling window does not suppress camera zoom.
- Expanded scrollable windows, ImPlot ownership, and the existing timing
  tolerance remain unchanged.

## Acceptance criteria

- With a populated Tweaks section collapsed and hovered, a wheel notch reaches
  camera zoom instead of being discarded.
- With the same section expanded and scrollable, the notch remains UI-owned.
- Existing wheeling-lock and non-scrolling menu behavior remain coherent on the
  next input poll.
- Client `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0033/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:894`. No source fix or build
was performed during routing.
