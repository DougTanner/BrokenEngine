# Debug Client Memory Growth After Replay Cancel

In one harness run, the Debug BrokenEngineSandbox client stopped responding with about twice its expected committed memory. That run did not show whether the client's growth caused the system to run out of commit or followed it. This record keeps the observed facts and the next check. It is an Investigation because the cause is unproven, so no fix can be decision-complete yet.

## Observation

- Build: Debug BrokenEngineSandbox client and server at primary `5f205af6a24a17ebf732f7005f03a48e0e6f91e0`, plus a server-only session change. The client C++ was identical to primary.
- Scenario, driven through `/agent-harness`, in order:
  1. Launch the server and one client.
  2. Click `[+F]##Fighter`.
  3. Record about 600 ticks, including a spawn.
  4. Play the recording back as a looping replay.
  5. Cancel the replay.
  6. Play about 80 s more, including the death and respawn of a fleet Fighter.
- About 80 s after the replay cancel, the client stopped responding: 0 CPU, with Windows reporting it as not responding.
  - Client private bytes were about 21.4 GB. The server's were about 10.3 GB, and each process was expected near 10 GiB.
  - Each Debug game process commits a 10 GiB mimalloc arena up front (`kiMimallocArenaReserveMebibytes`, `Engine/Source/Memory/GlobalAllocator.cpp:230`). mimalloc can add further arenas on demand (`Engine/Source/Memory/GlobalAllocator.cpp:260`).
- System commit was 68.7 of 75.9 GB. Total commit minus the client and server equaled the pre-launch baseline, so the excess of about 11 GB belonged to the client.
- Within the same second, the client's main loop stalled for about 71 ticks and dwm.exe crashed with 0xc00001ad (memory exhaustion). The server later dropped the client on an ENet timeout. The client wrote no crash report.

## Ruled out

- The session's server change: it preserves behavior, and the client binary's source was unchanged from primary.
- The harness working-directory change: every site that depends on the working directory is inert in a normal run:
  - the crash-report fallback paths (`Engine/Source/CrashReport.cpp:15-16`);
  - the PATH-relative `renderdoc.dll` load (`Engine/Source/Graphics/Managers/InstanceManager.cpp:235`), reached only with `--renderdoc`;
  - StackWalker's `.` symbol path, used only during crash handling.
- Primary's replay change: `Engine/Source/File/Replay.cpp` is entirely `BT_SERVER` code and is not in the client.

## Open questions

- Did the client's growth cause the commit exhaustion, or did the client grow after the exhaustion started?
- If the client grew first, which allocation grew: an extra on-demand mimalloc arena, or memory outside mimalloc?
- There is no valid control. An earlier run of the same scenario at `db9cbe5` quit right after the replay cancel, so it never reached the point where this run hung.

## Next check

1. With at least about 22 GB of commit free, run the same `/agent-harness` scenario.
2. Let it play at least 3 minutes past the replay cancel, through a Fighter death and respawn.
3. Sample client private bytes and system commit every 2 s.

Pass: client private bytes stay near 10.5 GB, and no dwm.exe 0xc00001ad event occurs.

If the client hangs, capture a process dump before the harness release. The `/agent-harness` skill does not document a dump step, so the investigator chooses the capture tool. The observing run took no memory samples before the hang, so if the growth reproduces, compare samples from the recording, the replay, the period after the cancel, and the time after the respawn to find where it starts.

## Provenance

- Client: `claude`. Worktree/branch UUID: `7506ab5a-9e57-43e7-b136-6f8500a09e36`. Branch: `claude/7506ab5a-9e57-43e7-b136-6f8500a09e36`. Conversation session ID: `c9339f81-d2e5-414f-9900-93426279e210`.
- Found during the `/next-plan` run that completed the Game Plan CarryFleetStateInSpawnQueue. That Plan file is deleted when it completes.

## 2026-10-26 measurement attempt

The current-revision measurement stage is **blocked before launch**. The source inventory below is complete, but
there is no successful isolated gameplay pair, three-cycle series, or fresh-process control from this attempt.
The historical Observation, Ruled out, Open questions, Next check and Provenance above remain the record of the
earlier run. Its 10 GiB arena, near-10.5 GB expectation and roughly 22 GB headroom instruction are historical;
they are not current startup defaults or acceptance thresholds.

This attempt used revision `09a4bbaf2dc752026b207ff1485b020a448e5f42` in worktree
`6f9b74e1-cd48-470b-a7a5-533c4d63ee08`. Both BrokenEngineSandbox Client and Server `Debug|x64` builds passed,
with complete retained logs and exit code 0. They used Local data generation with `/p:RunDataPacker=true` and
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data`. The build receipt is
`Temp/AgentBuildEnvelopes/compile-20261026T220151607Z-12940.md` under
`## ClientServerCommitFootprint measurement builds`. SHA-256 identities of all seven generated manifests are
retained in `Temp/commit-baseline-summary.json`; no process adopted that data in this attempt.

The current source requests 1 GiB for the Client and 4 GiB for the Server, rather than the old 10 GiB per process:
`BrokenEngineSandbox.vcxproj:642`, `BrokenEngineSandboxServer.vcxproj:530`, in
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026`. `Engine/Source/Memory/Mimalloc.cpp:6` selects eager
arena commitment; `GlobalAllocator.cpp:246` sets `mi_option_purge_delay` to -1. The Debug switch enables shutdown
diagnostics (`Engine/Source/EngineSwitches.h:24`). The project definitions do not select
`ENABLE_CRT_DEBUG_HEAP`; that alternative would route the replacements through CRT instead of mimalloc
(`GlobalAllocator.cpp:35-225`). No inherited `MIMALLOC*` environment variables were present in the measured shell.
These are the configured policies, not an observed startup-arena success or effective runtime-statistics result.
The 5 GiB pair total is a requested arena floor if both reservations succeed, not a process-memory ceiling or
a safe total-budget estimate; assets, scratch, graphics, stacks, drivers and fallback arenas can add to it.

## Memory owner inventory at the current revision

All rows below are source-derived, not measured live allocations. Current commitment, lifetime peak commitment,
live utilization, and actual physical backing for every owner remain **unknown** because no owned game process
launched. Initial sizes describe the demand passed to the allocation API, with OS page rounding where applicable.
An address-space reservation consumes no commit by itself. Heap-backed rows share allocator backing and must not
be added to the mimalloc arena as independent process commitment. Direct VirtualAlloc storage is a separate family.

| Owner / executable | Reservation or capacity policy and initial commitment | Growth and retained backing | Release and lifetime constraint / source |
| --- | --- | --- | --- |
| mimalloc startup and fallback arenas / both | Requested startup reservation and eager commit: Client 1 GiB, Server 4 GiB. Not a ceiling; failed startup reserve can fall back to on-demand arenas. Actual initial/current/peak unknown. | Purge delay -1 retains freed pages for reuse; further arenas can be acquired. Heap live bytes and arena commitment are different counters. | Freeing an object ends that object's lifetime without proving OS decommit. Shutdown diagnostics report heap/commit peaks and arena counts, not phase ownership. `Engine/Source/Memory/GlobalAllocator.cpp:242-306`, `Mimalloc.cpp:4-7`; project definitions above. |
| StableVector primitive / both | Constructor records a byte ceiling, rounded to 64 KiB; zero OS reservation/commit until first Resize. First growth reserves the ceiling and commits the requested prefix, rounded to pages. | Resize only grows; committed prefix and base address persist. Exceeding reservation asserts; no relocating fallback. | Destructor destroys elements and MEM_RELEASEs the reservation. Every pointer/span/RAII handle into it must survive growth. `Common/StableVector.h:20-81`. |
| ThreadLocal workbuffer and bookkeeping / both | Per-thread workbuffer ceiling is 64 times max(initial bytes, 64 KiB); initial prefix is the caller's configured bytes. Two bookkeeping vectors each reserve 4,096 int64 entries and initially size 64. | Workbuffer growth requests twice needed bytes. Pop and ShrinkLastPushBuffer reduce logical usage only; StableVector backing persists. The log vector is another 32 KiB heap allocation per ThreadLocal. | ThreadLocal/storage destruction releases backing; live frame handles obey LIFO and pointer stability. `Common/Threading/ThreadLocal.cpp:6-16`, `ThreadLocal.h:33-35,89-103`; `Common/Workbuffer.h:13-20,100-108,202-224`, `Workbuffer.cpp:80-94`. |
| Collision layers and layer-pair/zone indices / both, per participating thread | Layers initially hold 16 entries. Pair vector ceiling is 64 times 16 pairs; initial 16 pairs each create 8 by 8 zones, with two 2,048-int64 prefixes per zone. Each index vector's ceiling is 8,192 entries. | Indices and pair storage grow in place, doubling at overflow; count resets retain backing. The initial zone-index payload alone is 16 × 64 × 2 × 2,048 × 8 = 32 MiB per initialized thread, excluding metadata and other collision buffers. This is source arithmetic, not a measured thread count or footprint. | TLS destruction releases StableVectors. Registered collection columns must remain valid from AddLayer through collision use. `Engine/Source/Frame/Collision.cpp:72-103,108-137,224-233,298-345`; `Collision.h:8-21,29-44`. |
| Collision candidate and pending-result scratch / both, per thread | Candidate ceiling is 64 × 512 records; pending-result ceiling is 64 × 1,024 records. Initial prefixes are 512 and 1,024 records, respectively. | Live counts reset at Collide; overflow doubles prefix size. Neither reset shrinks commitment. | TLS destruction; candidates feed globally sorted collision events and pending results must survive until final result publication. `Engine/Source/Frame/Collision.cpp:47-60,366-405,473-478,587-594`. |
| Collision results, spans, generations and per-row columns / both, per thread | Result, span and tested-generation vectors have individual 64 MiB ceilings. Maximum-time/flag/terrain/boundary columns share the span row ceiling, so their byte ceilings differ by element size. Initial row prefixes are at least 1,024; results allocate at least 1,024 only when needed; generations follow layer-B demand. | Commit prefixes grow to reached row/result demand. Current counts and stored prefix capacity differ; prefixes never shrink. | TLS destruction. Layer max-time/flag pointers bind stable columns through collision/PostCollision. `Engine/Source/Frame/Collision.h:126-158`; `Collision.cpp:130-138,419-461,625-631`. |
| AreaDamage sources / both, per thread | Ceiling 64 × 16 sources, initially zero, then 16 sources on first Add. | Overflow doubles storage; resetting siAreaDamageSourceCount does not trim its StableVector. | TLS destruction; each source must survive all damage lookups for that frame. `Engine/Source/Frame/AreaDamage.h:6-37`, `AreaDamage.cpp:8-23`. |
| Frame collection SOA buffers / both; interpolation/render copies also Client | Heap storage has no separate fixed VirtualAlloc reservation. Logical iCapacity and transient iPhysicalLayoutCapacity are distinct. Empty/unallocated storage has no backing; requested bytes sum each column rounded to 64 bytes. | Spawn overflow grows logical capacity to 2 × capacity + 1. Growth copies to new backing before freeing old backing, causing temporary overlap. Removal decrements count only. AllocateAndAssign can reuse a larger physical layout after smaller logical capacity is read; other allocate/copy paths may replace it. | pData reset/destruction frees the heap object, without guaranteeing allocator decommit. Persistent columns, hydration and identity maps must stay correct. Logical capacity is serialized and CRC-visible; physical layout is transient. `Engine/Source/Frame/Collections/CollectionLifecycle.h:38-48,108-111`; `CollectionMemory.h:26-53,93-143,187-221,320-340`; `Collection.h:236-270,329-337`. |
| Eager pack vectors / Client | Scene/model/shader/raw pack bytes are loaded into heap vectors sized to complete pack-file length. No independent reserved-address-space ceiling; actual initial/current/peak unknown. Server excludes this eager data loop. | Pack buffers and chunk map remain owned by PackChunks after publication; replay cancel is not an eager-pack unload. | PackChunks/vector destruction. Chunk pointers alias vectors and publication uses release/acquire. `Engine/Source/File/PackChunks.cpp:99-108,440-494`; `PackChunks.h:120-132`. |
| Lazy pack pool / both | One stable MEM_RESERVE range equal to the sum of aligned uncompressed chunk destinations, initially uncommitted. Server selects island data; Client also consumes textures/audio. | LoadChunk recommits destination pages before reading/decompression. Logical chunk length remains after decommit and cannot stand for committed bytes. | Interior full pages can MEM_DECOMMIT; PackChunks destruction MEM_RELEASEs the pool. Texture adoption decommits source pages only after upload ownership ends. Reload must recommit at the same address; state publication controls consumer access. `Engine/Source/File/PackChunks.cpp:105-108,384-410,919-974,995-1015`; `PackChunkLoader.cpp:339-351`; `Graphics/Managers/TextureManager.cpp:585,610-624`. |
| Pack decompression scratch and read buffers / both | Two loading threads. Each scratch reserves and commits the largest compressed on-disk chunk size; none if no compressed chunks. Each aligned read buffer has RoundUp(sub-read size + sector alignment, sector alignment) bytes. | Scratch is allocated for the pack inventory up front; reduced load concurrency or smaller later chunks does not shrink it. Read buffers persist separately from the lazy destination pool. | Loader stops before scratch/pool release and aligned-buffer free in PackChunks destruction. Concurrent loaders require separate scratch/read storage. `Engine/Source/File/PackChunks.cpp:74-90,375-378,413-435`; `PackChunkLoader.h:64-66`, `PackChunkLoader.cpp:350-375`. |
| Graphics texture, staging and buffer storage / Client only | Vulkan/VMA allocations follow image dimensions, buffer sizes and framebuffer count. Host-visible mapped buffers, device-local storage and driver backing are different from mimalloc heap objects. No single fixed process reservation ceiling; initial/current/peak unknown. | Texture upload can overlap source pool, staging and GPU image. Dynamic buffers double on demand; mesh/joint buffers retain grown capacity when per-frame offsets reset. Old/new buffers can temporarily overlap. | Staging and retired images wait for all relevant framebuffer fences; Buffer/Texture destruction returns VMA allocations, not a proven equivalent amount of Windows private commit. Descriptors and GPU submissions must stop referencing old storage before release. `Engine/Source/Graphics/Objects/Buffer.cpp:10-49,106-114`; `Objects/Texture.cpp:188-225,363-374`; `Managers/TextureManager.cpp:740-778`; `Managers/BufferManager.cpp:417-488,510-574`. |
| Audio voices, pools and streams / Client only | Static voice metadata reserves bounded vector capacity; per-CRC XAudio2 voices can be pooled. Each streaming voice embeds three 16 KiB buffers (48 KiB payload), plus state/requests and XAudio2 backing. Initial/current/peak unknown. | Retiring a static voice can leave its XAudio2 voice in the pool. Streaming transitions can overlap current/previous/pending-destruction streams. These costs are separate from lazy pack storage and loader scratch. | Clear/Suspend destroys voices/streams and cancels pending reads; vector capacity and allocator commitment can remain. Callbacks/reads must finish before backing dies. Agent clients boot suspended; a later audio workload must explicitly audio_resume before claiming coverage. `Engine/Source/Audio/StaticVoices.cpp:18,130-196,604-642`; `StreamingVoice.h:17-18,109-115`, `StreamingVoice.cpp:24-28`; `StreamingVoices.cpp:124-150`; `AudioManager.cpp:247-277`; harness command reference `audio_resume`. |

The retained storage in these mechanisms has an identifiable owner and possible reuse: TLS scratch, frame SOA,
pack buffers, framebuffer resources or audio pools. That is not evidence that every allocation is useful, nor proof
of an ownerless leak. No measured underused buffer, lost owner, or time-correlated allocation source was identified.

## Preflight measurements and counter meanings

Read-only system sampling ran from `2026-10-26T22:02:49.3536343Z` to `22:05:23.9219805Z`: 747 samples,
nominal 200 ms cadence, actual maximum gap **228.0213 ms**, and zero gaps above 250 ms. The raw series is
`Temp/commit-samples.csv`, SHA-256 `D2CFAEE7C177412139F6A577BCCA04895D11A796891602C686F5977448D37118`;
`Temp/commit-sampler.ps1` records its implementation and `Temp/commit-summarize.ps1` reproduces
`Temp/commit-baseline-summary.json`. The full series also retains every value needed for a two-second view.
There are zero owned-process samples. None of the system changes below is attributed to this worktree's game.

| Preflight observation (UTC) | System committed bytes | Contemporaneous limit | Available commit |
| --- | ---: | ---: | ---: |
| First sample, 22:02:49.354 | 53,548,433,408 | 59,146,346,496 | 5,597,913,088 |
| Highest sampled commitment, 22:03:20.486 | 67,545,399,296 | 69,552,668,672 | 2,007,269,376 |
| Lowest sampled headroom, 22:04:18.929 | 60,209,827,840 | 60,225,888,256 | 16,060,416 |
| Build-completion preflight sample, 22:05:11.351 | 56,148,983,808 | 65,913,962,496 | 9,764,978,688 |
| Final sample, 22:05:23.922 | 55,944,101,888 | 65,913,962,496 | 9,969,860,608 |

The observed commit range was 41,868,353,536–67,545,399,296 bytes. The limit changed between
59,146,346,496 and 69,552,668,672 bytes without any pagefile or memory-setting change by this investigation.
The OS system peak remained 122,755,559,424 bytes throughout; it is since reboot, predates this attempt and
cannot locate or bound a new sub-sample spike in this interval. It can exceed today's changed limit.
The sampled peak is only a lower bound on the interval peak. Physical RAM capacity/availability and GPU capacity
were not measured; neither can be inferred from commit headroom. Background processes, the concurrent builds
and the sampler itself contribute to system commitment, so this is an environment baseline, not an idle-machine
or isolated engine baseline. No historical dwm crash-event search was made and no claim about new dwm events is supported.

Counter definitions were checked against primary documentation:

- [PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex):
  PrivateUsage is private process commit charge, PeakPagefileUsage is its lifetime commit peak, and WorkingSetSize
  is current physical residency. A smaller working set does not establish reclaimed commitment.
- [PERFORMANCE_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-performance_information):
  CommitTotal, CommitLimit and CommitPeak are page counts; the sampler multiplies each by PageSize. CommitLimit
  can change; CommitPeak is since the last reboot, not this run's peak.
- [VirtualQueryEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualqueryex):
  per-region state distinguishes reserved-only from committed address ranges. The sampler's process branch would
  sum MEM_RESERVE and MEM_COMMIT separately; total committed regions can include shared/image mappings and must
  not be relabeled as private charge. That branch was not exercised and no process-region totals are reported.
- [mimalloc runtime options](https://github.com/microsoft/mimalloc#environment-options): allocator purging returns
  unused pages according to its policy. The repository explicitly sets purge delay -1; this alone proves neither a
  leak nor how much live heap occupies the reserved arena. The repository's shutdown diagnostics are lifetime peaks,
  not per-replay measurements.

## Isolation blocker and unmeasured phases

Preflight enumeration first found a foreign BrokenEngineSandbox Debug Client (PID 24520), then a foreign
ModelViewer Debug process. Immediately after the successful build handoff, `Temp/commit-preflight.jsonl` at
`2026-10-26T22:05:11.1512234Z` still identified PID **5528**, `ModelViewer.Debug.exe`, with executable and
command line under worktree `763bf0d7-b9e7-4fb1-80ce-da1771f407f9`. The harness launch reference requires no
Sandbox Client or ModelViewer before a runtime session and immediately before Client launch. The foreign viewer
therefore independently blocks isolation, even when a sample shows more than 5 GiB free commit. Earlier samples
also showed headroom below the pair's requested arena floor, and no measured pair overhead is yet available.
No foreign process was signaled, no pagefile setting was changed, and no exhaustion workload was launched.

No harness claim or process launch was attempted; there was no owned PID/token to release. The ordered commands
actually executed were the read-only isolation enumeration and sampler invocation, build-completion recheck,
sampler stop-file signal, and summary generation. `Temp/commit-preflight.ps1` and its JSONL output preserve the
revision, timestamp, executable identity, command line, environment and contemporaneous system sample.

| Required phase | Initial / sampled peak / settled / final process and system commitment | Outcome |
| --- | --- | --- |
| Startup and real gameplay with one connected client | Not measured; only the preflight system series above exists | Blocked before launch |
| Same-process replay cycles 1, 2 and 3: about 600 recorded ticks, completed looping playback, cancel and at least 3 minutes afterward | Not measured; lifetime process peaks and incremental retained bytes unavailable | Blocked before launch |
| Death/respawn after cancel and fresh-process one-cycle control | Not observed; no command/query receipts or gameplay thread count | Blocked before launch |
| Active-cell high ship/blaster/missile population followed by sustained decline | No live counts, active-cell continuity or logical/physical backing observations | Blocked before launch |
| Reached scratch, asset, graphics/audio demand reduction and shutdown diagnostics | Not reached; no unload/retirement or mimalloc heap/commit statistics | Blocked before launch |

For the later run, supported `status.activeCoords`, `query_frame`, `query_collection` and `read_frame` can observe
active coordinates and live rows. Existing `edit_frame` pinning plus `inject_payload` and ordinary removal can
control an active cell through a population decline. These are capabilities, not actions performed here.
`Engine/Source/Agent/Commands/FrameEdit.h:425` and the Sandbox command schemas expose row values/counts, not
collection physical capacity, scratch commitment or generic live mimalloc statistics. No new query was added.
Replay cancellation must not be used as evidence that eager assets, lazy pools or GPU/audio owners unloaded.

## Attribution, recommendation and acceptance status

Measured process attribution is unavailable: eager-arena, asset, grow-in-place, graphics/audio and unattributed
process bytes are all unknown. The Client/Server source distinction above cannot substitute for a measured
headless comparison. No per-cycle transient peak, retained increment, plateau, continuing growth or load-overlap
conclusion is supported. The earlier roughly 11 GB Client excess remains unreproduced and its cause unresolved.
This attempt does demonstrate highly variable background system commitment, with only about 15.3 MiB free in
one sample; it does not establish which process caused that pressure or its relationship to the historical hang.

The next decision is to obtain an isolated runtime window with sufficient current commit headroom for the 5 GiB
requested arena floor plus observed non-arena overhead, then execute the approved gameplay/replay/control phases.
Keep the current allocation policy while the missing evidence is gathered. This is not a startup-sizing fix.
Do not select a trimming threshold or delay from these baseline samples: no buffer utilization or recoverable
committed-byte measurement exists. A nominally freed heap buffer can remain inside a committed purge-off arena.

The owner-specific reclamation questions for that decision are:

| Owner family | Recoverable bytes established here | Cost and safety constraints to measure before choosing a policy |
| --- | --- | --- |
| TLS workbuffer, collision and AreaDamage StableVectors | Unknown; low live count does not measure excess committed prefix. The 32 MiB zone-index initialization arithmetic is not measured unused storage. | Tail decommit could preserve reservation/base address but requires all live handles/spans and thread-local consumers to be past the tail. Recommit/initialization can stall and repeated regrowth can repeat that cost; compare measured utilization over candidate delays before changing the grow-only contract. |
| Frame SOA collections and retained frame copies | Unknown; logical capacity reduction can reuse larger physical backing. Allocator-free bytes are not equal to OS-recoverable commit. | A replacement requires old plus new backing until copying completes, adding a transient peak and copy stall; regrowth repeats allocation/copy. Preserve live/persistent columns, hydration, paired capacities, ID maps, serialization, replay and CRC determinism. Capacity policy is simulation-visible, while physical capacity is derived. |
| Pack scratch, eager/lazy assets | Unknown; scratch cannot shrink below concurrent decode demand, and no runtime page retirement was observed. | Decommitted lazy texture interiors already have an explicit upload-completion owner. A delay must retain publication/reload ownership and page boundaries; later reload adds disk/decompression/recommit cost. Pack scratch is two concurrent per-loader allocations, not one shared buffer to trim without quiescence. |
| Client graphics/audio storage | Unknown; resource sizes do not directly quantify Windows private commit. One stream embeds 48 KiB payload, but no stream count or freed backing was measured. | GPU fences/descriptors, pending audio reads/callbacks, overlapping uploads/streams and VMA/allocator retention constrain retirement. Dynamic/skinning buffer shrink would add old/new overlap, copy and regrowth costs after the correct framebuffer wait. |

Acceptance against the approved measurement preparation remains explicit:

- **A1 source inventory recorded:** every named owner family has its reserve/initial-demand/growth/release and
  lifetime behavior above; actual current/peak/live/logical/physical values remain unknown where no observation exists.
- **A2 BLOCKED:** successful isolated gameplay pair, three same-process cycles, fresh control and their phase
  counters are absent. A passing build and sub-250-ms preflight series do not satisfy runtime acceptance.
- **A3 BLOCKED:** counter meanings and source ownership are separated, but the required process-level attribution
  and measured Client/Server comparison are unavailable.
- **A4 historical record preserved:** original observation/provenance remain independently identifiable; the cause
  of that earlier growth remains unresolved.
- **A5 BLOCKED:** no cycle increments, plateau evidence or measured underutilization/reclamation tradeoff exists.
  The active-cell continuity/control capability has not been exercised and no physical-capacity query is available.

Completing this measurement stage requires the blocked runtime evidence. Any eventual allocation/reclamation
implementation requires a separate decision-complete Plan; no allocator, runtime, shared-lock, format, test or
tracked measurement-script change is part of this attempt.

## Resumed measurement at 7697f59

The blocked status above applies to the earlier 22:02–22:05 preflight. This resumed run reached isolated gameplay,
three same-process replay cycles, a pinned-cell population decline, and a fresh-process control. The historical
roughly 11 GB Client excess was not reproduced; its cause remains unresolved. No allocation policy changed.

Both BrokenEngineSandbox Debug|x64 builds passed at `7697f59e8187918c5a73e956ee7a721a18a6fe2d`.
The current receipt is `Temp/AgentBuildEnvelopes/compile-20261026T232106112Z-4664.md` (`Build Envelopes`).
These refreshed builds used **Shared** data, without DataPacker, from the primary checkout's
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data`; they replace the pre-sync Local build as
execution evidence. Manifest identities were captured during the run and checked again after it:

| Manifest | SHA-256 |
| --- | --- |
| Audio.manifest | 7288F6FDDA89BEEB073110B76B3F0621ADB482CBC6EF0503FE46AAC722EE5E4B |
| Islands.manifest | 08FC5C6ED1E70A3641420E75FCBFE25F51E2CBE262055DF70BDB2120C2DE7301 |
| Model.manifest | 5661F23389912CBEE47D2340047FACA383D829A7458C32B565CB37F610A60150 |
| Raw.manifest | D7739ADA649D037B2C945B5267C4BAEF5CADEF3881F41C5E40CE2019DE309E2F |
| Scene.manifest | 507EFE87002C2EA6A7FF06C79C312C6CCCF28F48C9587AEC823F646FCE4EE5D5 |
| Shader.manifest | 65881037C95296A2EDF421ACD50150EB4732B604B9FB5F4CAC8ACBD7CBBE5B7D |
| Texture.manifest | 0BA68DDABE2A24F89DF221256BDD17402FA835825F8F0F6D2819ABD245A28E55 |

The effective startup arenas remained Client 1 GiB and Server 4 GiB, eagerly committed, purge delay -1;
no inherited `MIMALLOC*` overrides were present. Both clients reached rendered 1600×904 gameplay, which also
passed the new foam-format startup check. Graphics UI explicitly reported High Water and checked Water Foam;
`audio_resume` succeeded. Actual foam history/dispatch and GPU allocation bytes are not exposed by the existing probe.

Ordered procedure: canonical harness claim; fresh absent AppData root; server then client launch; restore window;
require `clientCount:1`; inspect UI and click `[+F]##Fighter`; enable audio; record about 600 ticks;
stop recording; play and observe a new `End replay …, looping` line; cancel; require ordinary unpaused state;
sample for more than three minutes. Repeat three times without restarting, then run the separate decline check,
release, and repeat one replay cycle with a new pair and absent AppData root. Main PIDs were Server 13952 / Client 17360;
control PIDs were Server 656 / Client 36268. Each release accepted both quits, observed both exits, reported no crash
finding or survivor, and released its claim. There was also a cleanly released server-only sampler setup attempt.
No foreign process was signaled or pagefile setting changed. Ordered requests/results and UTC phase markers are
`Temp/commit-resume-commands.jsonl` and `Temp/commit-resume-phases.jsonl`; full logs are `Temp/commit-main-{server,client}.log`
and `Temp/commit-control-{server,client}.log`. No newly appended replay-read, CRC-client or confirmed-desync failure was found.

## Measured phases and repetition

The read-only sampler requested 150 ms intervals and retained the full series, including the longer holds.
Each table cell is **initial / sampled peak / last-10-second median / final**, in GiB (2^30 bytes), rounded to
three decimals. For a short transition the median uses its available samples; it is an end-window statistic,
not proof of equilibrium. First process values are the first available post-launch sample. Queries within one
iteration are sequential and share its UTC timestamp; region totals are not atomic allocator snapshots.

| Phase (UTC start) | Client GiB | Server GiB | System GiB | System limit GiB range |
| --- | --- | --- | --- | --- |
| Main startup/gameplay 23:24:39.464 | 1.104/3.777/3.776/3.776 | 4.104/4.277/4.277/4.277 | 40.599/64.763/49.262/49.477 | 55.084–66.072 |
| 1 record 23:26:05.094 | 3.776/3.776/3.776/3.776 | 4.277/4.277/4.277/4.277 | 49.520/57.438/50.429/50.509 | 61.384–61.384 |
| 1 playback 23:26:24.909 | 3.776/3.776/3.776/3.776 | 4.277/4.277/4.277/4.277 | 50.488/50.562/49.483/49.553 | 61.384–61.384 |
| 1 cancel 23:26:45.235 | 3.776/3.776/3.776/3.776 | 4.277/4.277/4.277/4.277 | 49.560/49.571/49.561/49.571 | 61.384–61.384 |
| 1 post-cancel 23:26:46.077 | 3.776/4.601/4.329/4.337 | 4.277/4.375/4.375/4.375 | 49.578/63.647/49.796/49.796 | 61.384–64.493 |
| 2 record 23:29:52.724 | 4.337/4.337/4.337/4.337 | 4.375/4.375/4.375/4.375 | 49.801/49.867/49.806/49.812 | 61.811–61.811 |
| 2 playback 23:30:12.344 | 4.337/4.337/4.337/4.337 | 4.375/4.375/4.375/4.375 | 49.819/49.999/49.814/49.806 | 61.811–61.811 |
| 2 cancel 23:30:32.319 | 4.337/4.337/4.337/4.337 | 4.375/4.375/4.375/4.375 | 49.817/49.817/49.808/49.808 | 61.811–61.811 |
| 2 post-cancel 23:30:32.948 | 4.337/4.659/4.330/4.330 | 4.375/4.375/4.370/4.370 | 49.807/50.866/49.495/49.492 | 61.811–61.811 |
| 3 record 23:33:37.653 | 4.330/4.330/4.330/4.330 | 4.370/4.370/4.370/4.370 | 49.498/49.737/49.506/49.629 | 61.811–61.811 |
| 3 playback 23:33:57.190 | 4.330/4.330/4.330/4.330 | 4.370/4.370/4.370/4.370 | 49.671/49.707/49.478/49.478 | 61.811–61.811 |
| 3 cancel 23:34:16.981 | 4.330/4.330/4.330/4.330 | 4.370/4.370/4.370/4.370 | 49.480/49.480/49.479/49.474 | 61.811–61.811 |
| 3 post-cancel 23:34:17.610 | 4.330/4.604/4.330/4.330 | 4.370/4.375/4.372/4.372 | 49.479/51.905/49.811/49.756 | 61.811–61.811 |
| Pinned population high 23:37:37.982 | 4.472/4.472/4.330/4.330 | 4.372/4.372/4.372/4.372 | 50.771/50.771/49.910/49.820 | 61.811–61.811 |
| Ships/missiles declining 23:38:09.251 | 4.330/4.795/4.330/4.330 | 4.372/4.372/4.370/4.370 | 49.823/66.268/52.726/51.891 | 61.811–67.112 |
| All three counts low 23:41:19.913 | 4.382/4.918/4.583/4.333 | 4.370/4.370/4.370/4.370 | 52.514/66.899/50.989/50.758 | 62.453–68.036 |
| Control startup 23:46:48.558 | 1.104/3.768/3.736/3.741 | 4.233/4.277/4.277/4.277 | 41.624/50.683/49.553/49.668 | 55.084–55.084 |
| Control gameplay 23:47:33.549 | 3.741/3.741/3.738/3.738 | 4.277/4.309/4.309/4.309 | 49.671/49.712/48.800/48.790 | 55.084–55.084 |
| Control record 23:48:24.749 | 3.738/3.738/3.738/3.738 | 4.309/4.309/4.309/4.309 | 48.793/49.617/48.791/49.617 | 55.084–55.084 |
| Control playback 23:48:44.326 | 3.738/3.738/3.738/3.738 | 4.309/4.309/4.309/4.306 | 49.659/49.709/49.112/49.559 | 55.084–55.084 |
| Control cancel 23:49:04.342 | 3.738/3.738/3.738/3.738 | 4.306/4.306/4.306/4.306 | 49.591/49.591/49.562/49.562 | 55.084–55.084 |
| Control post-cancel 23:49:04.964 | 3.738/5.090/4.546/4.546 | 4.306/4.343/4.343/4.343 | 49.562/50.496/49.543/49.537 | 55.084–55.084 |

| Cycle | Client sampled / lifetime peak bytes | Client settled bytes; change from preceding settled value | Server settled bytes; change |
| --- | ---: | ---: | ---: |
| 1 | 4940357632 / 4953899008 | 4648370176; 594378752 | 4697559040; 105377792 |
| 2 | 5002203136 / 5032054784 | 4648779776; 409600 | 4692242432; -5316608 |
| 3 | 4943339520 / 5032054784 | 4648779776; 0 | 4694896640; 2654208 |
| Fresh control | 5465464832 / 5465473024 | 4880982016; 866938880 from its own baseline | 4663517184; 37027840 from its own baseline |

| Process | Total thread range | Sampled peak working set GiB | Reserved-only GiB first / pre-quit / peak | Committed regions GiB first / pre-quit / peak |
| --- | ---: | ---: | --- | --- |
| Main Server | 4–24 | 0.566 | 4.679/7.014/7.016 | 4.160/4.427/4.432 |
| Main Client | 6–39 | 1.514 | 4.688/273.950/273.954 | 1.174/4.711/5.295 |
| Control Server | 20–24 | 0.542 | 4.894/6.515/6.515 | 4.289/4.400/4.400 |
| Control Client | 6–40 | 1.541 | 4.688/273.924/273.927 | 1.173/4.673/5.467 |

Reserved-only ranges are `MEM_RESERVE`; committed-region sums can include image/shared mappings and are not private
charge. Total OS thread counts do not identify how many threads initialized each TLS owner.

The three post-cancel holds lasted 186.645, 184.704 and 184.982 seconds; the control hold lasted 185.604 seconds.
Cycle 1 retained 594,378,752 Client bytes above its own pre-record value. Cycles 2 and 3 then changed the settled Client
value by only 409,600 and 0 bytes. This workload therefore reached a repeatable settled level across those three cycles,
with temporary peaks rather than another roughly 11 GB retained increment. This is not a claim of a universal plateau:
the fresh control settled at 4,880,982,016 Client bytes, 232,202,240 above main cycle 3, and other cells/content continued
changing during the longer population check. The control retained 866,938,880 Client and 37,027,840 Server bytes from
its own pre-record baseline. Its sampled/lifetime Client peaks were 5,465,464,832 / 5,465,473,024 bytes.
Lifetime peaks cover private commitment
missed between samples but cannot assign an old peak to a later phase or bound a shorter system-wide spike.

Main Client lifetime peak was 5,281,271,808 bytes, including the later population check; its all-low interval sampled
5,280,460,800 bytes and ended at 4,652,412,928 bytes. The last-10-second median there was higher (4,921,376,768),
so that window was still variable and must not be described as a fully settled per-buffer reclamation result.
At cycle 1 cancel the same fleet Fighter globalId 2 was dying (health -2, lifecycle 4, UUID 16);
ten seconds later it had health 10, lifecycle 0 and UUID 19. Its subsequent cell transfers and respawns were queried.
Thus the post-cancel observation includes actual death/respawn, rather than just elapsed idle time.

Raw series `Temp/commit-resume-process-samples.csv`: 30667 rows, SHA-256 `62E064D7E7FF3331B74D9A597C74BEB122CA52654A17168757440C8C88A06008`; one process-exit counter error, described below. `Temp/commit-resume-phase-summary.json` and its scratch summarizer retain exact byte totals, region/working-set/thread fields, timestamps and gaps. System commitment ranged 43062747136–71832616960 bytes against a changing 59146346496–73053388800 byte limit.
The three longer gaps were 282.663 and 273.1166 ms at 23:40:50.3036489→50.8594285, and 277.7542 ms at
23:43:47.0375678→47.3153220, all in low-demand holds (well below their two-second allowance). Loading/replay/cancel
transitions had no gap above 250 ms. The OS system peak remained boot-scoped at 122,755,559,424 bytes; it cannot locate
new spikes below that older peak. At 23:25:25.4999005, available commit was only 14,979,072 bytes while paired private
commit was 8,646,828,032 bytes. System variation far exceeded the paired private-byte changes. The remainder is
unattributed system commitment, including possible other-process, kernel, driver and shared costs; it is not proven
foreign-process usage. Current physical RAM/GPU availability and new dwm crash events were not measured.
During orderly control shutdown, the sampler could no longer read exited Server PID 656 at 23:52:48.2848946
and retained that error row. The next system sample was 597.5794 ms later, exceeding the transition target there.
That shutdown gap is an explicit measurement limit; it does not support a claim of uninterrupted sub-250-ms coverage.
Both registered process identities passed the release-entry check, with no crash finding. No workload/settling process
sample gap exceeded 282.663 ms; control process gaps were at most 178.4387 ms.

## Focused orderly-shutdown retest

On 2026-10-27, the accepted shutdown-sampling failure was retested without repeating the replay cycles.
The original 597.5794 ms gap and unavailable-process error above remain failed historical evidence.
The old sampler serializes system reads with process enumeration, `MainModule` lookup and region scans;
its exit-time null-module error is retained in that same loop. This confirms the sampling/exit race,
but the old timestamps cannot identify which individual call consumed the missing interval.
The replacement Temp sampler uses an independent system thread and exact launched-PID private-byte counters,
omits module lookup/region scans, and marks signaled process handles unobservable instead of reading exited processes.

The same Debug builds and Shared data reached fresh, visible gameplay with exactly one connected client,
Server PID 3136 and Client PID 6068, at origin with two Battleships; audio resumed successfully.
Preflight at 00:00:28.5626484Z found no competing sandbox/viewer process, no allocator override, and
41,431,973,888 committed bytes against a 59,146,346,496-byte limit. No pagefile setting changed.
The shutdown interval was marked 00:01:32.7812004Z–00:01:57.7270095Z; canonical release accepted both quits,
observed both orderly exits without forced stops, returned no crash findings or survivors, and released the claim.

| Counter | First / sampled peak / last-10-second median / final bytes | First / final available sample UTC |
| --- | --- | --- |
| Client private commit | 3743371264 / 3743830016 / 3743371264 / 1197621248 | 00:01:32.8289123 / 00:01:41.8260340 |
| Server private commit | 4592078848 / 4592078848 / 4592078848 / 4505919488 | 00:01:32.8288970 / 00:01:41.5901166 |
| System commit | 50712375296 / 51060240384 / 41749536768 / 41766768640 | 00:01:32.8291029 / 00:01:57.6984308 |

The system thread retained 919 samples across 00:01:02.0624850Z–00:01:57.6984308Z, maximum gap **66.7087 ms**;
the shutdown subset retained 422 samples, maximum gap **65.7345 ms**, including both exits.
Process maximum gaps across the full series were Server 65.6555 ms and Client 65.6585 ms; lifetime private-commit
peaks were respectively 4,680,101,888 and 4,191,686,656 bytes. After-exit observations at Server 00:01:41.6517986Z
and Client 00:01:41.8824023Z explicitly have no process counters. Final process values are last readable samples,
not zero or post-exit values; their medians use the available pre-exit interval and do not prove equilibrium.
System limit remained 59,146,346,496 bytes; its boot-scoped peak remained 122,755,559,424 bytes.
This focused retest satisfies the missing shutdown cadence check while preserving the earlier exception.
The existing three-cycle/control/population evidence and owner-observability limits remain unchanged.
Raw series and exact summary are `Temp/shutdown-retest-{system,process}.csv` and `Temp/shutdown-retest-summary.json`;
ordered requests, phase markers and release receipt are `Temp/shutdown-retest-{commands,phases}.jsonl`
and `Temp/shutdown-retest-release.json`. The system/process CSV SHA-256 identities are respectively
`88118D3B66E528184747D25C2692980C42969DC7BA0E662E8E500FEA734CCFF4` and
`49FE7F1079D368E034A6B17E50533EBA840DB3E91010B840475AF243470F6D1B`.

## Current owners, low demand and attribution

The inventory above remains the source inventory; the synced Client adds these reached owner details
(paths in the table are under `Engine/Source/Graphics/`):

| Owner | Storage and lifetime | Measurement limit |
| --- | --- | --- |
| Water foam current/history images | `Graphics.cpp:635` recreates both with the water mesh; `Managers/RenderTargetTextures.cpp:68-89` requests R16_SFLOAT 1024×512 current (one mip) and history (11 mips). Texel payload arithmetic totals 2,446,678 bytes, before tiling/alignment. | Payload is not a measured VMA allocation or Windows commitment. Reset/inactivity does not release either image. |
| Foam compute/water pipelines and descriptors | `Managers/WorldLightingShadowPipelines.cpp:356-370,401-404` creates bindings for history/current, spawn noise and material samplers. They remain graphics-owned across replay cancel and disabled presentation. | Host/driver/GPU bytes and per-frame descriptor costs are unknown. |
| Foam/breakup texture catalogs | `Managers/TextureManager.h:107-128` and pipeline construction retain the material catalog bindings. High Water plus the system toggle activates foam; the Tweaks checkbox only hides presentation (`Render/AGENTS.md`, Precision and History). | No catalog unload or owner-specific page retirement was observed. |

Main shutdown diagnostics reported Client peak heap usage **553 MiB**, peak allocator commitment **1024 MiB**, one arena;
Server **3378 MiB**, **4096 MiB**, one arena. These are separate lifetime diagnostics, not concurrent live-allocation
snapshots. Fresh control reported Client **238 MiB / 1024 MiB**, Server **3285 MiB / 4096 MiB**, also one arena each.
No extra mimalloc arena was reported. For example, cycle 3's Client settled private bytes minus its eager
arena leave 3,575,037,952 bytes outside that arena; Server leaves 399,929,344 bytes. These are unattributed remainders,
not amounts assigned to graphics or a leak. Client eager Scene/Model/Shader/Raw pack vector payload totals 129,058,880
file bytes; it is heap storage inside the allocator accounting and must not be added again to its committed arena.
Lazy destination commitment, two loader scratch buffers, TLS workbuffers/collision storage, frame copies, driver
storage and audio pools cannot be numerically separated with these aggregate counters.

For the dedicated decline, origin `[0,0]` was pinned with `edit_frame uiPinned:1`; a valid negative `fSpawnTimer`
postponed new random spawning. Supported injection reached **65 Fighters / 101 blasters / 128 missiles**. A navigation
delay above the supported bound was rejected before injection, and a mixed kill-state edit was rejected with
`Frame flying missile plane`; neither was counted as a successful action. Supported `DestroyShip` entries then
removed the random Fighters; missiles expired normally. Once missiles were absent, a blaster-only `kDestroy` edit
succeeded. All three counts were **0 / 0 / 0** for the subsequent 185.675-second observation, with a Battleship still
present, `uiPinned:1` read back, and origin present in every active-coordinate query. This was not cell destruction.
Other active coordinates changed, so aggregate process differences cannot be assigned exclusively to this cell.

AreaDamage grow warnings reached count/capacity 16/16 in both processes; source `AreaDamage.cpp:17-18` grows that TLS
prefix to 32 records. Each log contains one corresponding `DEBUG_BREAK at`, preceded by that growth warning;
both processes continued and exited normally. These are growth notes, not crash findings. Count reduction and source
growth prove demand changes; existing frame queries expose neither logical capacity nor physical backing, and scratch,
allocator-current, GPU/audio and owner-specific committed-byte queries are absent. Recoverable bytes remain unknown.
A smaller working set or a count of zero is not evidence of returned system commit.

Keep the current policy pending owner-level evidence. The zero-count interval exceeds candidate delays of 30, 90 and
180 seconds, but does not measure a positive reclaimable byte amount or the cost of a later regrowth; it cannot choose
between those delays or justify a 50% threshold. StableVector tail decommit must preserve live handles/pointers and
quiescent TLS consumers; collection replacement must account for old/new overlap, copying, hydration, physical layout,
identity, serialized logical capacity and CRC/replay determinism. Assets require publication/reload ownership, graphics
requires fences/descriptors, and audio requires completed reads/callbacks. Recommit, reload, copy stalls and repeated
shrink/regrowth costs remain unmeasured. A separate implementation Plan is required before changing any such policy.

A1/A3/A4/A5 have the consolidated inventory, phase/cycle evidence, explicit aggregate/owner limits, preserved
historical record and constrained recommendation. A2 reached all required gameplay/cycle phases; the focused retest
closes the shutdown cadence check while retaining the original exception. The earlier replay-growth cause remains open. No allocator,
startup-default, shared-lock, format, test, runtime-source or tracked measurement-script change was made.
