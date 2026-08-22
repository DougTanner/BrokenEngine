# Server - Headless Monitoring Window

Server-only GDI monitoring for simulation statistics, profiling, memory, and the active-coordinate map. It has no Vulkan dependency.

## Ownership and Cadence

- `Main.cpp` owns the window, message loop, and repaint scheduling; this directory owns statistic aggregation, paint content, and click handling.
- Deliberate game-type ownership record, which the Engine hub (`../AGENTS.md`) requires this leaf to carry: the one game type this directory consumes is `game::GetServerCellStats` (`../../../Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`). Everything else it draws — the awake-cell list, client transport records, profile counters and timers, memory stats — is already engine-owned.
- Entity totals for the left panel are summed here from those per-cell records, because the per-collection profile counters showing the same numbers are named in the game counter enum and the engine's contract with that enum is only the four names game Profile (`../../../Projects/BrokenEngineSandbox/Source/Profile/AGENTS.md`) lists. The game keeps publishing those counters from its own post-tick path for the profile overlay and the agent profile query; the only counter this directory writes is the engine-owned explosion count.
- Statistic aggregation runs per tick only when `kbProfiling`; when disabled, the entity totals and profile counters remain empty and the Profile tab still paints its own header and non-aggregated rows, while tick/time and the map stay live.
- Full-window painting is throttled and double-buffered to keep the server loop responsive, because painting costs far more than aggregating the statistics it draws. `Main.cpp` considers a repaint only every eighth tick, skips it while the window is minimized or hidden, and otherwise repaints only when the content actually changed or a 1 Hz heartbeat fires. Click-driven invalidation may repaint immediately.
- `ServerDisplayContentChanged()` is stateful, not a plain question: it folds the drawn statistics into one number (a hash), compares that against the last painted one, and overwrites the stored number as it reports a change. Call it exactly once per repaint decision — a second call in the same window reports "unchanged" and silently cancels the repaint, leaving a window that looks like a hung server. The hash deliberately leaves out the free-running tick and time text, which is what the heartbeat exists to keep alive, so any newly painted statistic must be folded into the hash or it will never trigger a repaint on its own.
- Teardown drains one last message pump while the game object is still alive, so a pending `WM_PAINT` is dispatched and validated instead of reaching a display whose data is gone. `Main.cpp` validates the window immediately before its own teardown pump, closing the window left by an uncover or resize between the two, and nothing invalidates the window after that point.
- Everything in this directory runs inside the allocation-tracked server main loop, and `Main.cpp` already wraps the statistic update, the repaint decision, and `WM_PAINT` in broad `ScopedSuppressAllocationTracking` guards because GDI and `InvalidateRect` allocate internally. Code here inherits that suppression rather than adding its own; the single local guard in this directory covers the file-static `std::string` cache the Copy button pushes to the clipboard.
- Cached GDI objects (back-buffer device context and bitmap, font, brushes, pens) and the repaint, tab, entity-total, and clipboard state are file-static because the process has exactly one server window. The bitmap is recreated only when the client area resizes, and nothing is explicitly destroyed — process teardown reclaims it. Supporting a second window means moving that state per-window first.
- The map renders North (+y) upward and distinguishes client-authorized, active, idle, and subscribed coordinates.

## Profile Presentation

- Reuse the CPU timer/counter formatters and one `TickVisibilityCadence` decision per paint so timer and counter rows change visibility together.
- Build transient text in the thread-local workbuffer. UI rows hide sustained zeros, while the server agent profile query returns raw rows including zeros.

## See Also

- `../Profile/AGENTS.md`
- Game Profile: `../../../Projects/BrokenEngineSandbox/Source/Profile/AGENTS.md`
