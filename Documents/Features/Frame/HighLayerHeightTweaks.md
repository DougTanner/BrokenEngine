# High Layer Height in Tweaks

Revisit When: the user schedules live tuning of the High flight layer.

## Request and scope

Expose the High layer's height as an easy-to-adjust developer control from
0.0 to 200.0 meters, initially 100.0. Ships may fly through terrain when it
is set too low; the user explicitly accepts this. High may also be below or
equal to Low (6 m) or Medium (50 m). Keep their heights unchanged.

This is a manually executed feature under `Documents/Features`. Saving this
document authorizes no runtime implementation and creates no scheduler claim.

## UI placement

Use **Tweaks > Misc**, with an inline **Ships** group containing a
**High Layer Height** slider displaying meters and one decimal place. There
is no subtab: Misc currently contains one inline Debug Texture Range control.
There is no existing Gameplay or Ships window to extend. This recommendation
avoids adding a window or tab for one slider.

Draw game-owned content through a named game extension function called by
`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenMisc.cpp`, following the
existing inline Sound extension pattern in game `Tweaks.cpp`. Keep the
existing debug-input and runtime ImGui gates. Expose nothing in the normal
Graphics or Game Settings menus.

The slider is a request editor and displays the acknowledged server value.
Show a pending request until acknowledgement; do not immediately change a
client-side simulation global. Disable the control when there is no authorized
developer connection to a server. Include a short nearby note: "Low values
allow ships to fly through terrain."

## Recommended apply behavior

Apply live on a server-selected fixed tick. Existing ships targeting High
smoothly move toward its new height using their existing climb rate; do not
snap their position, alter XY, or change their target-plane orders. New High
spawns use the accepted height. A subsequent adjustment retargets the current
climb from its current position. A ship moving to Low or Medium keeps that
target. Use `kuiNoFlightPlane` while moving and restore the target identity
on arrival, including when the target identity stayed High but its height
changed. These are author recommendations to confirm during implementation
preparation, rather than additional recorded user choices.

High remains the same plane identity and keeps terrain collision disabled
throughout the advertised range. Do not reorder plane indices, clamp to
terrain or another plane, enable terrain avoidance for High, or rebuild a
terrain-blocked High graph. Coincident physical heights do not merge plane
identities: stored and target plane IDs still govern plane-specific targeting.
Projectile collision and explosion damage retain their existing
physical-distance rules; pushers keep theirs but act only within the
flight-plane layer nearest each ship's height.

## Deterministic ownership

This changes simulation height, so a client-only Wrapper or mutable
`gpFlightPlanes` singleton is insufficient. The current table is immutable
and worker-thread readable; rollback must retain the historical height for
each frame, rather than reading the latest UI value during an old tick.

Reuse the existing server request/admission and deterministic `FrameInput`
status-change paths, extending them narrowly for this one developer value.
Server admission accepts only finite values in [0, 200], validates developer
authority, and chooses the application tick. Coalesce slider changes to the
latest requested value for an advancing tick. All cells observe the same
accepted value at that tick; paused simulation applies it when ticks resume.
Do not change singleton state while parallel cell ticks are reading it.

Store the effective height in shared deterministic frame state, with its
ordinary serialization, CRC, difference logging, and snapshot treatment.
Frame input records the transition so reconciliation and replay use the
historically correct value. The server seeds newly activated cells from the
current accepted value; late joins, resubscriptions, and cell transfers must
not restore the compile-time 100 m default or revert an accepted setting.
Render consumers use the height belonging to the displayed frame. Start a
new session at 100 m; do not add a separate user-settings persistence format.
Saved simulation state and replay restore the recorded height.

There is no generic simulation-value Tweaks bridge in the inspected code:
`ServerSession::BuildAdvancingFrameInputs` currently processes owned-entity
requests, and `StatusChangeType` has ship/fleet changes, spawns, and transfers.
Before implementation approval, settle the narrow developer authorization
and request transport using the existing connection policy. Do not grant
arbitrary remote clients authority over the whole world's height or build
a general configuration/admin system for this slider.

## Required propagation inventory

- `Frame/FlightPlaneTable.h` and engine `Frame/FlightPlanes.{h,cpp}` currently
  define positive, ascending heights and assert every island is below the
  highest plane. Separate fixed plane identity/flags from effective height;
  replace the assumptions this feature invalidates. User intent takes
  precedence over those existing invariants.
- Game `Frame/Collections/Ships/ShipBehaviours.cpp`: climbing, exact-height
  lookup, `NavigationFlightPlane`, and `SkipsTerrainContact` currently infer
  behavior from ordered numeric heights. Make them correct for crossing and
  coincident heights without changing High into Low or Medium. Preserve
  existing Low/Medium navigation behavior; High uses its empty graph even
  when lowered. Inspect intermediate climbs as well as settled ships.
- Ship `PostRead` height bounds currently use `Lowest()`/`Highest()` and
  reject values outside those endpoints. Admit the full supported envelope,
  including ships still climbing from an earlier accepted height; preserve
  stored-plane and type-specific allowed-plane validation.
- Game frame spawn/wave code, ship queries and weapon gates, missiles and
  missile admission, transfers, and developer spawn commands: inventory
  static heights, exact-height matches, and highest-index assumptions.
  Coincident planes require identity where numeric equality is ambiguous.
  Settle how already-flying missiles retain their existing launch height
  without corrupt-stream rejection after an adjustment; do not silently
  teleport or remove them.
- Engine `Main.cpp`, `IslandTerrain`, and per-plane `NavCellData` bake
  height-dependent contours at startup. Low/Medium are unchanged and High
  remains empty, so this feature needs no dynamic contour rebuild. Check
  that ordinal assumptions do not accidentally select another graph.
- Engine `Graphics/FlightPlaneProjection.cpp`, camera/picking, smoke and wind
  plane consumers, shader-facing plane heights, and ship render/shadows:
  inventory every `gpFlightPlanes`, `kFlightPlanes`, and fixed 100 m use;
  update only consumers made incorrect by the effective High height.
- Update owning AGENTS contracts that currently promise immutable ordered
  heights, above-terrain High, or endpoint-based admissible ship heights.
  Bump game `Frame::kiVersion` for changed deterministic state/semantics,
  `FrameInput::kiVersion` for changed server-written input bytes, and
  `engine::kiProtocolVersion` for incompatible wire payloads. Increment any
  changed collection/navigation format's own version when its contract
  changes; do not add compatibility readers or bump unrelated formats.

## Acceptance criteria

1. Only developer Tweaks > Misc > Ships exposes High Layer Height, with
   range 0.0–200.0 m and session default 100.0 m; Low and Medium remain 6/50 m.
2. Exercise 0, 6, 25, 50, 100, and 200 m, changing values repeatedly while
   ships climb. Existing High-target ships move smoothly at their type's
   existing rate, new High ships use the accepted height, and unrelated
   targets and XY movement remain intact.
3. At 0, 6, and 50 m, High retains its identity, empty navigation graph, and
   terrain exemption. Ships may pass through terrain; no ordering assertion,
   terrain clamp, accidental plane relabeling, or equal-height ambiguity occurs.
4. Server and clients agree on accepted value, application tick, ship heights,
   and CRCs across multiple cells, replay, reconciliation, late join,
   save/reload, resubscription, and transfer. Older frame/input/protocol formats
   are rejected through their existing version gates.
5. Invalid/non-finite or unauthorized requests do not change state. Pending
   UI requests never drive simulation before acceptance; disconnected UI is
   disabled and reconnect displays server state.
6. Stored-plane weapon gates, missiles already in flight, pushers, rendering,
   picking, smoke, and wind remain correct through crossings and repeated
   changes. Rendering observes the displayed frame's value without races.

## Related work and future verification

Searches of Features, Plans, and Investigations found no existing owner for
this slider. Pushers act only within the flight-plane layer nearest each ship's
height (`Engine/Source/Frame/Collections/Pushers/AGENTS.md`), so a plane-height
slider moves those layer boundaries.
`Documents/Investigations/Engine/ShipForcesInThreeDimensions.md` discusses
height/navigation assumptions; it does not own this feature. Keep its scope
separate.

Future implementation requires normal preparation and approval, client/server
builds through `/compile`, and runtime/replay checks through `/agent-harness`.
Add no unit tests. No builds, captures, or runtime changes were made to save
this feature. The authorization transport and existing-missile treatment
above remain implementation decisions, so this is not an execution-ready Plan.
