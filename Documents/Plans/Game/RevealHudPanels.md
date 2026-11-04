<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-03T19:09:59.966Z","dependsOn":[]} -->
# Reveal HUD Panels Through Agent Commands

## Context

The user requested a follow-up capability to bring UI panels and their items
on-screen through code using normal interaction semantics, without mouse movement.
This is a user-requested capability addition, recorded separately from the pending
fleet member [x] change; it does not satisfy or waive that change's screenshot gate.

At baseline 282bd638d30ed35ee760a63c61163dc11a837afa, a healthy focused fleet
contained enabled Fighter and Omni rows, but FleetPanel's rectangle was
[-302,180,-1,722] and every row reported visible:false. Semantic row clicks
succeeded, while a screenshot could not establish rendered row appearance.
`Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:103-129` opens the
panels through mouse proximity or missing-fleet recovery. Its RenderFleetPanel
reapplies position with NoMove at lines 162-167. `Engine/Source/Ui/MenuUtils.cpp:255`
animates the target into position. `Engine/Source/Graphics/Managers/ImGuiManager.cpp:477`
parks the agent mouse at the no-mouse sentinel. `Engine/Source/Agent/AgentInput.cpp:63`
activates enabled items by ID without requiring visibility; it does not reveal them.
`window_state` restores the OS window and cannot reveal an in-game panel.

No existing executable Plan owns this missing reveal operation. Existing
EngineTextViewParameters and EngineOrderedGuardAndDifferenceReads Plans touch
agent/UI source for mechanical changes only and explicitly exclude new harness APIs.

## Design

The author's recommendation is a narrow game-client `reveal_ui` command with
`window:"FleetPanel"` and Boolean `reveal`. This is the first supported target;
unknown targets fail explicitly. `reveal:true` holds the HUD panels' openness
target at one through the existing slide behavior; `reveal:false` releases that
request and lets ordinary hover and recovery determine openness again. Repeat
requests are idempotent. The existing focused-entity content gate still applies
and the panels retain their ordinary coupled opening when content is present.

Keep the request in the live HudScreen instance. Add the smallest client-only
UiScreens forwarding function needed for the game command to reach that instance;
the command fails if no HUD exists or the game UI state prevents HUD rendering.
Destroying/recreating the HUD clears the request. Do not set mouse coordinates,
move ImGui windows directly, fake visibility, change selection, or alter fleet state.
Command success acknowledges the request; callers wait for subsequent completed
`describe_ui` snapshots to establish visibility after animation before capturing.

This bounds the general request to the concrete obstructed panel and its items.
Additional panels, item scrolling, and a generic reveal registry remain future work.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` — game command dispatch.
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.h` and `.cpp` — reveal state and target.
- `Engine/Source/Ui/Screens/UiScreens.cpp` — live HUD forwarding and lifecycle.
- `Engine/Source/Ui/Screens/UiScreens.h` (new) — client-only forwarding declaration.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md` — command contract.
- `.agents/skills/agent-harness/references/command-reference.md` — game command discovery.
- Affected Agent and UI Screens AGENTS.md owners — only contracts made incorrect by this addition.

## In scope

- Add only the FleetPanel reveal/release command and its live-HUD forwarding path.
- Combine its target with HudScreen::Render's current force/hover targets while
  preserving the right panel's content gate and normal animation/layout helpers.
- Document parameters, supported target, lifecycle, failure conditions, and the
  difference between request acknowledgement and observed rendered visibility.
- Verify live healthy fleet member rows, release, repeat requests, and HUD recreation.

## Out of scope

Fleet [x] implementation or acceptance waivers; mouse/hover simulation; item
activation changes; generic registries, callbacks, or reveal frameworks; scrolling
arbitrary items into view; other panels; OS window control; fleet recovery changes;
new dependencies; deterministic simulation, CRC, network, save, replay, or .pack
formats; ThirdParty edits; unit tests; unrelated UI layout or harness cleanup.

## Acceptance criteria

- Through `/agent-harness`, reproduce the observed baseline setup: launch an
  isolated connected Debug client/server, restore the client window, reset, use
  `click` with `[+]##Fleet`, then `[+F]##Fighter` and `[+O]##Omni`; wait for live
  enabled member rows in `describe_ui`. With the agent mouse parked and healthy
  live members present, record the hidden panel and rows before requesting reveal.
- Issue `reveal_ui` for FleetPanel with reveal true. Wait for completed UI
  snapshots showing the live rows visible and their rectangles inside the client
  viewport. Capture and inspect a screenshot that actually shows those rows.
  Existing semantic clicks still select the Fighter and Omni rows after reveal.
- Reveal remains in effect across subsequent snapshots and row clicks. A repeated
  reveal succeeds without changing selection or layout. Release restores existing
  hover/recovery behavior; in the same healthy fleet case the panel slides closed.
  Recreating the HUD starts without a retained reveal request.
- Unsupported targets and unavailable/gated HUD requests fail clearly. Existing
  disabled-item handling, offscreen item discovery/activation, and physical-user
  mouse opening/closing remain unchanged; verify the ordinary mouse path through
  code inspection and an ordinary interactive run when available.
- Build sandbox client/server Debug|x64 through `/compile`; complete applicable
  correctness, C++ cleanup, documentation, and live acceptance
  checks. No unit tests or scripted pass/fail harness command is added.

## Notes

Future implementation is Change Workflow Tier 3 because the game agent dispatcher,
engine screen lifecycle, and game HUD are independently owned integration surfaces.
The new state is client presentation state only, outside PostRender CRC and all
serialized formats. Keep steady-state paths allocation-free and UI changes on the
main thread; preserve whole-file/client-member affinity and server header parsing.
The forwarding declaration is called only by the sandbox command; other clients
need no HUD forwarding implementation. Reconcile new-header project membership
through `/update-vcxproj`.
This Plan recommends one bounded implementation, subject to the normal approval,
alternatives, plan review, and Tier-3 grill workflow before implementation.

There are no directional prerequisites or mandatory coordination constraints with
existing Plans. Mechanical source overlap alone creates no scheduler dependency.
