# Acquisition-Fence Stalls

The client sometimes waits one or more extra display refreshes for its next
swapchain image. This record keeps what was traced, what was ruled out, and what
is still open, so a later session can start from it. The first traced stalls
showed no engine defect. A later focused trace found the engine's own GPU work
late, with Main's GPU work held by its wait on the swapchain image; that wait
was moved off Main (`### Engine frame finished 92 us before a vsync`).

Each claim carries one of three proof labels:

- **Proven in a trace**: matching timestamps in an event trace show it.
- **Signature only**: the logged durations look the same as a traced stall, but
  no trace covers it.
- **Not tested**: no evidence either way.

## Run conditions

- For the user's original stall in `## Symptom`, only the GPU, FIFO
  presentation, the Profile build, and the expected frame interval were
  recorded; its window mode, the other running programs, and the exact actions
  and timestamps were not.
- Every other measurement below, except the capture in
  `### Engine frame finished 92 us before a vsync`, which states its own: one
  AMD Radeon RX 9070 XT, display 3840x2160 at 120 Hz (8.3 ms per refresh), FIFO
  present mode, three swapchain images, Profile client and server. The
  automated runs were borderless fullscreen at 3840x2160; the focused session's
  window mode was not recorded.
- Another fullscreen-size program was presenting on the same GPU through the
  desktop compositor (DWM) during every run after the user's original stall,
  except that capture. It was left running.
- The traces were non-elevated DxgKrnl sessions: present, queue, flip, and vsync
  events, but no CPU context switches. An agent cannot start the kernel logger
  without elevation.
- The trace files were not kept. Only the numbers below survive.

## Symptom

- The user's original stall, in a focused manual Profile session: acquisition
  invocation 942 took 16.2 ms. Of that, the prior acquisition fence wait took
  16.18 ms and `vkAcquireNextImageKHR` took 0.04 ms. The frame interval was 17.2
  ms against 8.3 ms expected. The exact actions and time were not recorded.
- An older completed GPU sample of the same run showed normal GPU work (about
  7.3 ms of root regions). It is several frames old, so it cannot explain that
  invocation.

## Where the host waits

- `SwapchainManager::AcquireNextImage` waits on `mCurrentImageAvailableVkFence`,
  the fence handed to the previous `vkAcquireNextImageKHR`. It then resets the
  next fence and acquires again. `Graphics::PresentAcquire` calls it after the
  present.
- That fence belongs to the image the frame just rendered and presented. The
  engine records, submits, and queues a frame's present before that image is
  confirmed available again.
- The ordinary FIFO wait, for contrast (proven in a trace, unfocused composed
  client): this same fence wait takes 7.4 to 7.9 ms every frame (median 7.6 ms).
  In one run 55,501 of 56,400 waits fell in the 7 ms bucket. The fence signals
  0.15 to 0.52 ms after a vsync, when DWM releases the buffer it stopped
  showing.
- Normal pipeline in that state: the engine presents about 1 ms after vsync k,
  its image comes back just after vsync k+1, and DWM shows the frame at vsync
  k+2. Exactly one engine frame per refresh, with no spare queued frame.
- A stall is that ordinary wait plus one or more missed refreshes.

## Detection under automation

- The stall does reproduce under automation (see `## Reproduction rates`). An
  earlier statement that automated scenes did not reproduce it was wrong.
- The built-in slow-operation warning cannot fire in an unfocused agent-launched
  client. `Main.cpp` skips taking focus when `gLaunchOptions.iAgentPort` is set
  and calls `SetDiagnosticFocus(false)`. `ProfileManager` then records no
  rendered intervals while `kFocused` is clear, so
  `mExpectedRenderedFrameInterval` stays zero and `ScopedSlowOperation` neither
  times nor warns. The boundary is `kfSlowOperationFrameMultiplier`.
- So the automated runs used temporary instrumentation in `AcquireNextImage`,
  independent of focus. It logged every fence wait longer than 12.5 ms with raw
  `QueryPerformanceCounter` values around it. It was removed afterwards and is
  not in the tree.
- `Engine/Source/Profile/AGENTS.md` `## Slow-operation Diagnostics` records that
  unfocused intervals never enter the histories. Changing that is a separate
  diagnostics decision.

## Clock mapping

- The temporary instrumentation wrote raw `QueryPerformanceCounter` values. The
  traces use the same clock.
- Trace origin = a `VSyncDPC` event's `FrameQPCTime` minus 10 x its trace
  timestamp. A log line pairing a counter value with system time gave the same
  origin. The two methods agreed within 21 us, with no drift over 40 s.
- The engine's own marker events landed 20 us before the logged acquire end of
  the same acquisition, at two points 40 s apart.

## Reproduction rates

All rates are from the automated runs: fence waits longer than 12.5 ms,
unfocused composed agent client.

- Idle scene, no trace: 2 stalls in 56,400 acquisitions (about 8 minutes): 16.2
  ms and 16.1 ms.
- Idle scene, traced: 2 stalls in 43,200 acquisitions (about 6 minutes): 17.0 ms
  and 16.8 ms. Both traced.
- Fleet switches in the fleet panel plus opening and closing the hex-grid map,
  no trace, 200 s: 2 stalls (14.6 ms and 15.6 ms) in about 24,000 acquisitions.
  An idle control of the same length, no trace: 2 stalls. Same rate.
- The same actions with four back-to-back traces, 420 s: 31 stalls in about
  51,600 acquisitions, plus 24 long frame intervals with no fence stall. 19 of
  the 31 stalls were on world-hidden frames (map fully opaque). The untraced run
  with the same actions had no stall on a world-hidden frame.
- In that traced run, after stall b (below), the fence-wait histogram switched
  several times into a mode where about 98% of waits were under 1 ms.

Separately, a focused manual session, traced, caught one stall and gives no
rate: one 31.3 ms acquisition (prior fence wait 31.19 ms, invocation 1392) 1.56
s after the trace started.

## Traced stalls

### Compositor skipped a refresh during a service start-up rundown

- Proven in a trace, three times (the two idle-scene stalls and stall a of the
  action run).
- Each time, a new process of the Windows graphics performance service
  (`GraphicsPerfSvc`) started, and its start made DxgKrnl emit a state rundown
  that spanned one vsync. In the two idle-scene stalls the rundown was about
  16,000 events over about 10.2 ms; in stall a it was 8,998 events, with no
  length measured.
- DWM got that vsync's signal but ran no composition pass until the rundown
  ended. So no buffer was released at that vsync, and the engine's fence
  signalled one refresh late.
- The engine was on time: presents on the 8.3 ms cadence in all three; in the
  two idle-scene stalls its frame was ready 1.3 to 1.6 ms before the missed
  vsync and the main thread woke 21 us after the kernel released the wait.
- Control intervals just before each idle-scene stall showed the normal
  sequence: frame ready 0.7 to 1.7 ms before vsync, fence end 0.2 to 0.3 ms
  after it.
- The three traced service starts came 27 s, 176 s, and 41 s after a trace
  session started, so a link to tracing is not excluded for them.
- Also seen with no trace running: one stall in the untraced idle control came 7
  ms after a new service process was created. Signature only for that stall.
- Service start times seen on one afternoon (UTC): 13:20:04, 13:25:20.8,
  13:30:37.5, 13:33:15.8, 14:08:25.1, 14:13:00.1. The first four are multiples
  of about 158 s apart, and the third and fourth came with no investigation
  trace running.
- The first untraced idle run's two stalls do not sit on that spacing. Signature
  only.

### Compositor issued no flip for two refreshes

- Stall b of the action run, 22.8 ms. Proven in a trace that it happened;
  unclassified.
- The engine presented on cadence. DWM composed every interval and took other
  programs' frames, but issued no flip for two refreshes. The engine's queued
  GPU packet waited on the image's sync object and ran only after the next flip.
- GPU work for that frame was normal. The trace does not say why DWM issued no
  flip.

### GPU scheduler held the engine's acquire-signal packet

- Stall d of the action run, 12.6 ms, on a world-hidden frame. Proven in a trace
  that it happened; unclassified.
- The engine presented on cadence, and DWM signalled the image's release on
  time.
- The driver's acquire-signal packet on the engine's context was queued but
  reached the hardware only 12.9 ms later. The two before it went within 0.64 ms
  and 0.02 ms. Once it ran, the main thread was released 14 us before the
  engine's fence-end marker.
- The other fullscreen-size program queued fewer packets in that interval than
  in a control interval. A non-elevated trace has no scheduling data to say why
  the packet was held.

### Composed to directly flipped in a focused session

- The focused session's 31.3 ms stall. Proven in a trace for this one stall;
  seen once.
- Before it, DWM composed the client every refresh. At the stall, DWM moved the
  client onto its own display plane, and from then on the kernel flipped it
  directly.
- The engine presented, then made no present and no queue submission for 31 ms.
  It woke 0.8 ms after the second kernel flip of the new plane.
- Some desktop shell and console window activity happened just before. That is
  coincident only.
- This is a different shape from the one-extra-refresh signature.

### Engine frame finished 92 us before a vsync

- One stall, a 15.9 ms prior acquisition fence wait, in a focused manual
  session in which the user, after clearing the world, spawned the most
  Fighters and Omnis in all four fleets.
  Proven in a trace: late GPU completion of the engine's own work.
- Run conditions: the same GPU, display, and FIFO present mode, Profile client
  and server, the client focused for the whole capture. The kernel flipped the
  client directly (independent flip) on every present; it was not composed.
  Whether another program was presenting was not recorded.
- The frame whose image the stalled acquisition waited for finished its GPU
  work 92 us before a vsync. Its flip took the next refresh, so its image came
  back, and the fence signalled, one refresh late.
- The GPU ran back to back: each frame's GPU work started when the previous
  frame's ended, frame GPU spans alternated between about 7 and 10.5 ms against
  the 8.3 ms refresh, and each frame's work sat 10 to 15 ms in the queue.
- Engine-side hold: Main's submission waited on the image-available semaphore,
  and that wait held all of Main's GPU work, not only the swapchain write,
  until the image came back. In the stall the queue sat idle 1.9 ms with Main
  queued behind it, and Main started at the release. From 166.0 to 167.5 s,
  ending about 0.36 s before the stall, the same wait held the queue on half or
  more of the frames, typically 2 to 3 ms.
- Engine change made: Main no longer waits on image availability. The HDR
  resolve, Main's one swapchain write, moved into the ImGui submission, which
  takes that wait (`Engine/Source/Graphics/Managers/AGENTS.md`
  `### CommandBufferManager`). The main thread's acquisition fence wait, and so
  its warning, keeps its length.

## Ruled out for the traced stalls

- Late GPU work of an earlier frame, except in
  `### Engine frame finished 92 us before a vsync`: in the idle-scene stalls
  the engine's frame was ready 1.3 to 1.6 ms before the missed vsync, and GPU
  root regions were normal around every stall in the action runs and in the
  31.3 ms focused stall.
- Late CPU submit or present: render start and present calls stayed on the 8.3
  ms cadence for the 15 acquisitions before every automated fence stall.
- Late main-thread wake: the wait returned 21 us after the kernel released it
  (service stalls), and 14 us after the waiter release in stall d.
- The ordinary FIFO wait alone: it is 7.4 to 7.9 ms per frame. Each stall added
  at least one refresh, and the traced ones show where that refresh went.
- The `ClearRetainedEffects` queue idle wait: it ran on two world-hide frames
  for 5.3 ms and 5.9 ms with no fence stall after either.
- Also tried without a stall: screenshots, camera zoom, the pause and graphics
  menus, High lighting quality for about 50 s, and extra units.

## Outcome against the Plan "Investigate acquisition-fence stalls with GPUView"

- That Plan was completed with this record.
- No engine defect was found in `AcquireNextImage`, `PresentToQueue`, or
  `Present`, and that Plan made no engine change for this symptom.
- Not met: "the symptom no longer reproduces". There is no fix, and the stall
  still occurs.
- Not met: "a changed runtime passes its Profile builds and harness checks".
  There is no runtime change.
- Weakened: the clock mapping and the removed instrumentation cover the
  automated reproduction only. The user's invocation 942 was never matched to a
  trace.
- Weakened: the traces separate the host wait from GPU work and display pacing,
  but not from the compositor's CPU scheduling, which needs an elevated capture.

## Open questions

- The user's original manual stall was never traced. Signature only.
- Several automated stalls were never traced: the first idle run's two, the
  untraced action run's two and its idle control's two, and most of the 31 in
  the traced action run.
- Why the graphics performance service keeps restarting. Its first four starts
  were spaced at multiples of about 158 s; later starts were not.
- The two unclassified stalls (no flip for two refreshes; acquire-signal packet
  held) need CPU and GPU scheduling data from an elevated capture.
- A focused, directly flipped client was captured stalling only once, from late
  engine GPU work (`### Engine frame finished 92 us before a vsync`). The first
  focused session's second capture ran 9 minutes, all after the window had lost
  focus, so its detector was off and it caught nothing.
- Swapchain configuration lead, for a follow-up Plan: in that capture, under
  FIFO with an independent flip, a frame finishing 92 us before a vsync cost a
  whole refresh while the GPU ran back to back near the refresh budget. Whether
  another swapchain image or another present mode absorbs such a slip is not
  tested; that run's swapchain image count was not recorded.
- The alternating long and short frame GPU cost is left to a separate
  follow-up Plan.
- Whether the map and fleet actions or tracing raise the stall rate. Without a
  trace, the action run and its idle control both had 2 stalls in 200 s, a small
  sample. Whether tracing raises it is not tested: there is no traced idle
  control, so the traced action run's higher rate and its world-hidden stalls
  are not separated between the actions and the tracing.

## How to continue

- Run `/analyze-gpu-stall`.
- Use its focused route, and keep the client window focused for the whole
  capture. A focus loss turns the slow-operation warning off, and the client log
  shows it as a `WM_KILLFOCUS` line.
- Ask the user for its elevated capture to get CPU and GPU scheduling data for
  the unclassified stalls.
- Add a traced idle control of the same length before comparing rates.

## Related items handled separately

- Hex-grid map close hitch: the first world frames after the map closed spent 6
  to 11 ms in descriptor churn, a late CPU present rather than this fence stall.
  Fixed in the same landing: island residency now runs on world-hidden frames.
- Single-chunk texture adoption cost: the 5 to 7 ms spike was the main-thread
  `VirtualFree(MEM_DECOMMIT)` of the texture chunk's CPU pool range. Fixed:
  lazy pack chunk CPU buffers now come from mimalloc
  (`PackChunks::AllocateLazyChunkBuffer`), and
  `TextureManager::AdoptUploadedChunk` frees the texture buffer back to it
  through `PackChunks::FreeLazyChunkBuffer` with no OS call; a 185 s Profile run
  then measured a 470 us maximum adoption over 741 adoptions.
