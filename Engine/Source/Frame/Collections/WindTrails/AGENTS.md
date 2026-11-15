# /Engine/Source/Frame/Collections/WindTrails/

Directional wind-deposit quads: each trail emits one flat XY quad per unpaused
rendered frame spanning its previous→current render position, carrying intensity
and normalized motion direction that `WindDeposit.frag` injects as velocity into
the ping-pong wind texture. Sync pattern (parent owns lifetime).

## Unique Aspects

- Quads build at base height; width axis is `cross(dir, worldZ)`; the
  previous→current vector scales by a per-trail length multiplier about the
  current position. Trails with negligible motion or failing visibility culling
  are skipped.
- When wind is enabled, the build loop snapshots every trail's current position
  — including culled and zero-motion trails — as next frame's previous position
  before its visibility test, so each quad spans exactly one render frame of
  motion and trails never smear when re-entering view; a trail deposits nothing
  its first rendered frame (previous defaults to current).
- Two ping-pong wind-deposit pipelines (A/B) share one dynamic
  `shaders::QuadLayout` buffer; `EndRender` writes the indirect draw count only
  to the side matching the active wind texture, the other gets 0 — both passes
  are always recorded, so the zero count is what no-ops the inactive side.
- `kbManualRender` opts WindTrails out of the merge of interpolate and render
  work; its render path is called explicitly with SmokeTrails from the main
  render sequence.
- When wind is disabled, `BeginRender` prunes stale render state and returns
  before capacity or GPU work. With wind disabled or gameplay paused, `Render`
  snapshots every current trail position for CPU cache maintenance under
  allocation suppression and generates no quads. `sPreviousPositions.clear()`
  clears cached previous positions on world reset.
- `ReconcileDispatcher::Run` also clears `sPreviousPositions` on the main
  thread after the dispatch joins whenever any coord injects or adopts a pending
  full state; reconcile workers never touch it. Every trail then deposits
  nothing for one rendered frame.

## See Also
- `../WindRadials/AGENTS.md` - Stationary sibling; same deposit shader and
  ping-pong scheme
