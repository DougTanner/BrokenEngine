# Second-Game Code Sharing — Open Decisions

**Investigation, not an executable plan.** It carries no byte-zero `broken-engine-plan/v1` metadata, so WorktreeCli never schedules it. It authorizes no implementation work. Everything here is either a decision the user still has to make, or a candidate that cannot become a plan until that decision exists.

Unless a path starts with `Engine/` or `Documents/`, it is relative to `Projects/BrokenEngineSandbox/Source/` — the current game project.

## 1. Purpose and method

Imagine building a second game on this engine tomorrow: a different game with no Spaceships, no Players, no Fleets, no missiles. What would that second game be forced to copy-paste out of BrokenEngineSandbox before it could even start? To answer that, three researchers swept the entire game source tree and compared every file against the engine's existing Base layer, sorting each piece of code into three buckets: *already engine's job* (nothing to do), *should move to the engine and every choice needed to move it is already made* (those became executable Plans, listed in section 3), and *should move to the engine but a design question blocks it* (section 4 — the reason this document exists). This document records only the third bucket, plus the one structural finding that shapes all of it.

## 2. Load-bearing structural finding: the engine currently depends on the game

The normal expectation is that the engine knows nothing about the game. That is not true today. Engine code directly names `game::` symbols in several places, so a second game cannot compile or link until each one is inverted or formally declared a required contract:

- **Engine simulation reads the game's world constants.** `Engine/Source/Frame/IslandChainPlacement.cpp:296-310` reads `game::Frame::kfCellWidth`, `kfCellHeight`, `kfBaseAreaMinX/MaxY`, and calls `game::SeedFromGridCoord`; `Engine/Source/Frame/IslandTerrain.cpp` reads the same constants plus `game::Frame::kiElevationGridDim` at ten-plus sites (`:252-255`, `:424-426`, `:490-494`, `:512-515`, `:545-546`, …) and reaches into `game::gpGame->mCoordFrames` at `:406-407`; `Engine/Source/Frame/TimeStep.cpp:38` reads `game::kTickNs`.
- **The engine's ImGui manager hard-codes every game screen class.** `Engine/Source/Graphics/Managers/ImGuiManager.h:5-14` forward-declares nine `game::` screen classes and the manager owns a `std::unique_ptr` to each (`:63`, `:80-87`).
- **Engine code calls game globals.** `Engine/Source/GameBase.cpp:35-36` calls `game::gpInput`; `Engine/Source/Audio/StaticVoices.cpp:530` reads `game::gpCamera`; `Engine/Source/Graphics/Render/MainUniforms.cpp:426-428` reads the game's hex-shield tuning wrappers; `Engine/Source/Frame/Collections/Explosions/Explosions.cpp:91-178` and `ExplosionsSpawn.cpp:120` read dozens of game explosion/wind tuning wrappers.
- **The engine's per-cell frame container stores game types.** `Engine/Source/GameBase.h:44-45` holds `std::unique_ptr<game::Frame>` and `:69` holds `std::vector<game::StatusChange>`.

The last one is not an accident. `Engine/Source/AGENTS.md:11` records the deliberate **Engine/game contract**: "Engine code may consume game types, hooks, globals, and compile-time symbols that the game is required to provide."

**Why this matters for every decision below.** A second game is a *second build* of the engine that supplies its own `game::` types — not a second game running inside one shared binary. That is good news for extraction cost: moved code almost never needs templates or type erasure, because the compiler resolves `game::Frame` freshly per build. It is bad news for decision cost: every move is really the question *"which game concepts do we make mandatory for all games forever?"* That is what each decision below is asking.

## 3. Already-planned, decision-complete extractions

These need no decision and are being written up as executable Plans alongside this investigation. Listed for completeness only — their content lives in the Plan files, not here.

| Plan | One-liner |
|---|---|
| `Documents/Plans/Frame/EngineOwnedFrameConstants.md` | Invert the engine's reads of `game::Frame` cell/grid constants and `kTickNs`. |
| `Documents/Plans/Frame/FrameUtilsSharedHelpers.md` | Move game-agnostic frame helper math into the engine. |
| `Documents/Plans/Engine/UiStateHoistToGameBase.md` | Hoist the UI-state enum and its bookkeeping from `game::Game` to `engine::GameBase`. |
| `Documents/Plans/Ui/MenuUtilsToEngine.md` | Move the shared ImGui menu widget/layout helpers to the engine. |
| `Documents/Plans/Ui/GraphicsQualityWrappersToEngine.md` | Move graphics-quality tuning wrappers the engine already reads. |
| `Documents/Plans/Ui/HexShieldWrappersToEngine.md` | Move the hex-shield wrappers `MainUniforms.cpp` reads. |
| `Documents/Plans/Network/ServerSubscriptionServicingToEngine.md` | Move per-client cell subscription servicing to the engine. |
| `Documents/Plans/Network/GamePacketAdmissionGateToEngine.md` | Move the game-packet admission/validation gate to the engine. |
| `Documents/Plans/Network/ClientRareRequestSendHelper.md` | Share the client's rare-request send helper. |
| `Documents/Plans/Network/EngineMessageVisitorGaps.md` | Close gaps in the engine message visitor. |
| `Documents/Plans/Network/LastClientLeavesResetToEngine.md` | Move the "last client disconnects → reset" policy to the engine. |
| `Documents/Plans/Network/OwnedEntityRegistryToEngine.md` | Move the owned-entity registry to the engine. |
| `Documents/Plans/Network/ReceivedUpdateAdoptionToEngine.md` | Move received-update adoption bookkeeping to the engine. |
| `Documents/Plans/Network/ActiveSetSkeletonToEngine.md` | Move the active-set skeleton to the engine. |
| `Documents/Plans/Network/Guid128Promotion.md` | Promote the 128-bit GUID type from game to engine. |
| `Documents/Plans/Frame/TerrainTraceToEngine.md` | Move terrain ray/segment tracing to the engine. |
| `Documents/Plans/Frame/RunFrameTickHoistToEngine.md` | Hoist the per-tick frame driver to the engine. |

## 4. Open decisions and the candidates they gate

Effort / Impact / Risk scores use the anchors in `Documents/AGENTS.md` (written `E`/`I`/`R`).

---

### D1 — What shape is a game's per-tick change list? (keystone decision)

**Plain language.** Every tick, the simulation produces a list of "things that happened" — spawn this, damage that, this unit moved to a neighbouring cell. Today that list is a flat array of items, each stamped with a small type tag (`game::StatusChange` / `StatusChangeType`). The network layer, the server, the client rollback system and the replay files *all* assume that exact shape.

**The decision.** Does the engine make that shape mandatory for every game ("your per-tick change list is an array of type-tagged items, and here is the engine machinery that ships, sorts, compresses, validates and replays it"), or does each game get to invent its own per-tick payload format and re-implement all of that machinery?

**Candidates this unlocks (all Tier 3 — they touch the wire format and the determinism CRC):**

| Candidate | Where | E / I / R |
|---|---|---|
| Change-list batch codec envelope: counting-sort grouping by type, `[type:1][count:2]` group framing, bounded-cursor whole-batch rejection on malformed input, LZ4 envelope with a hostile-prefix clamp | `Network/NetworkSerialization.cpp:176-461` | E3 / I4 / R3 |
| Server cross-cell transfer harvest: collect, validate adjacency, materialize in the destination cell, sort by type, apply, then recompute the destination frame CRC (`:261`) | `Network/Server/ServerTransferManager.cpp:29-349` | E4 / I4 / R3 |
| Deferred agent-injected change queue (test/automation spawns ride the same broadcast/CRC/replay channel as real ones) | `Network/Server/ServerBroadcaster.cpp:82-113`, `:267-291` | E2 / I2 / R3 |
| Per-cell tick publication assembly | `Network/Server/ServerBroadcaster.cpp:136-191` | E2 / I2 / R3 |
| Client-side transfer harvest loop — currently duplicated four times, roughly 200 lines including a word-for-word copied comment block | `Frame/Collections/Blasters/Blasters.cpp:190-238`, `Missiles/Missiles.cpp:332-389`, `Spaceships/Spaceships.cpp:377-439`, `Players/PlayersNavigation.cpp:66-137` | E3 / I4 / R3 |

**Sub-decision inside the last row.** Does the engine template the transfer record on a game-supplied payload type, so `FramePostRenderBase` can own the `transferRequests` buffer directly, or does the engine stay payload-blind and let each game own that buffer? The first removes the duplication outright; the second keeps the engine simpler but leaves the four copies to the game.

**Researched recommendation (2026-08-11): mandate the type-tagged item array for every game.** The "payload-blind engine" alternative was never actually available: the engine's own resend ring, its per-cell update records and its publication path already traffic in arrays of `game::StatusChange` — `Engine/Source/GameBase.h:69` stores `std::vector<game::StatusChange>` inside an engine struct today. So a payload-blind engine would not *preserve* a clean engine; it would require tearing out coupling the engine already depends on, at the price of every second game re-writing the most desync-prone code in the repository.

Draw the line like this:

- **Engine owns** the batch codec envelope (counting-sort grouping, `[type:1][count:2]` group framing, bounded-cursor whole-batch rejection, LZ4 with the hostile-prefix clamp), the transfer pipeline (collect → validate adjacency → sort by type → apply → recompute CRC), the deferred injection queue, per-cell publication assembly, and the `transferRequests` buffer declared **directly on `FramePostRenderBase`**.
- **Game owns** its change-type enum ending in `kCount`, and for each type: wire size, write, read, default, and name; plus transfer classification, destination-liveness policy, and `SpawnTransfer`.

**Answer to the sub-decision:** no template. `transferRequests` is declared as a plain member of `FramePostRenderBase` holding the game's item type — the engine storing game types is the documented Engine/game contract (section 2), and `engine::CoordFrames` already does exactly this. A template would add syntax for a freedom no build can use, since each build resolves one `game::` type anyway.

**What it unlocks, in dependency order.** All five rows above become writable plans. The codec envelope (E3/I4/R3) and the server transfer harvest (E4/I4/R3) are the substantial ones; the injection queue and the publication assembly (both E2/I2/R3) are small and follow the codec. The `transferRequests` hoist plus one shared capacity-checked push (E3/I4/R3) deletes all four duplicated client harvest loops outright. The client-side seam rides the D2 chain, so sequence it after D2's first plans.

**No protocol bump from D1 itself.** The extraction is byte-identical on the wire — acceptance is a fixture comparison of encoded batches before and after. (D12's packet promotion *does* bump the version, and is deliberately bundled into this stage; see D12.)

**Risks and prerequisites.** Confirm the wording in `Documents/Architecture/Network.md` and `GameReconciliation.md` before writing the D1 or D2 plans (see section 5). Named counterargument, accepted rather than dismissed: fixed-size items constrain a future game that wants variable-length payloads — that is answerable later with an engine framing extension, and does not justify paying option B's cost now.

---

### D2 — How does the engine hook into the rollback-and-replay chain?

**Plain language.** When the client's prediction disagrees with the server, the client rewinds to the last agreed-upon tick and re-simulates forward. That whole stack — the fast path that notices "nothing actually changed", the ring buffer of past frames, the tick re-run driver, the parallel per-cell dispatch, and the desync reporting/escalation policy — is almost entirely engine-shaped already. It should move as one sequence, not piecemeal, because the pieces call each other.

**Candidates (all Tier 3 — determinism and CRC):**

| Candidate | Where | Game coupling remaining | E / I / R |
|---|---|---|---|
| CRC fast path | `Network/Client/ReconcileReplayCrc.cpp` (whole file, 287 lines) | Diagnostics only: `StatusChangeType::kCount` and `StatusChangeTypeName` (`:87-101`) | E2 / I4 / R3 |
| Ring buffer and orchestration | `Network/Client/ReconcileReplay.cpp` + `ClientReconciler.h:34-113` | None — zero game symbols | E2 / I4 / R3 |
| Replay tick driver | `Network/Client/ReconcileReplayTick.cpp:24-403` | One seam: transfer application `:128-179` and the CRC recompute at `:178` | E3 / I4 / R3 |
| Parallel per-cell dispatch | `Network/Client/ClientReconciler.cpp:35-228` | One seam: confirmed-client-state and visual-error smoothing `:164-203` | E3 / I3 / R2 |
| Desync report and escalation policy | `Network/Client/ClientDesyncManager.{h,cpp}` (whole class) | Writes `gpGame->mModalMessage` (`:33`, `:58`, `:80`, `:106`) and calls the game frame's `LogDifferences` (`:44`) | E2 / I4 / R2 |

**The four questions inside this decision.**

1. **Diagnostic symbols.** The CRC fast path only needs the game's type names *to write a log line*. Does the engine require every game to supply `kCount` plus a name function, or does the engine take a small log callback and stay ignorant? (Callback is looser; the required-symbol form is simpler and matches the existing Engine/game contract.)
2. **Is "apply transfers after the tick, then recompute the CRC" an engine rule or a game hook?** This is the same question as D1's ordering guarantee, seen from the client side — answer D1 first and this usually follows.
3. **Is "the client follows one entity" an engine concept?** Confirmed client state plus a visual error offset (the smoothing that hides a correction from the player's eye) presumes each client owns one focal entity. Many games do; a spectator or god-view game does not.
4. **How does the engine tell the player something broke?** Today the desync manager writes a string into `gpGame->mModalMessage`. Options: a `GameBase` modal hook every game implements, or an engine-owned message buffer the game chooses to display.

**Ordering caveat.** This area overlaps three plans already queued: `Documents/Plans/Network/ReconcileFullStateInjectionRingBase.md`, `Documents/Plans/Network/DesyncDebugFrameResponseCorrelation.md`, and `Documents/Plans/Network/ClientLoadClockJitterReset.md`. Land those first; extracting underneath them would force a rewrite.

**Also gated behind D2** (decision-complete on its own, but only sensible once the chain has moved): client poll timing and clock-snap recovery, `Network/Client/ClientSession.cpp:190-244`, E2 / I3 / R2.

**Researched recommendation (2026-08-11): move the chain as one sequence, with these four answers.**

1. **Diagnostic symbols — required game symbols, no callback.** The game supplies `StatusChangeType::kCount` and `StatusChangeTypeName`. That is a one-line extension of an obligation the game already carries for the codec (D1), where a log callback would be a whole new indirection serving one log line.
2. **Transfer-after-tick then CRC recompute is an engine-mandated contract, not a game hook.** The engine driver owns the ordering, the erase, and the recompute; the game supplies only the predicate ("is this item a transfer?"), the payload, the materializer, and its summary logging. Both the engine-side and game-side AGENTS.md already state this ordering as a hard invariant, in matching words — writing it into the engine driver removes a rule each game would otherwise have to re-obey by hand.
3. **"The client follows one entity" stays optional.** Engine `ClientReconcilerBase` owns dispatch, merge and desync handling and gains exactly two optional no-op virtuals: focus-position capture returning `std::optional<GridCoord>`, and confirmed-client-state update. Armor handling, visual-error smoothing and `ReconcileReplayClientState.cpp` stay in the game override, so a spectator or god-view game overrides nothing at all.
4. **Player-facing failure message — no new hook. This overturns the earlier lean toward a `GameBase` modal hook.** The already-planned `Documents/Plans/Engine/UiStateHoistToGameBase.md` moves `mModalMessage` onto `engine::GameBase`. After it lands, the desync manager simply writes an engine-owned member. Adding a modal hook would invent a mechanism that plan is about to make unnecessary.

**Decomposition — three chained Tier-3 plans.** `ReconcileReplayChainToEngine` (the three `ReconcileReplay*` files plus the pure structs; `dependsOn` `ReconcileFullStateInjectionRingBase` and `RunFrameTickHoistToEngine`) → `ClientReconcilerBaseToEngine` → `ClientDesyncManagerToEngine` (`dependsOn` additionally `DesyncDebugFrameResponseCorrelation` and `UiStateHoistToGameBase`).

**Residual to accept:** "Desynced from server" becomes engine-authored English text, sitting in the engine ahead of the localization split in D7.

---

### D3 — Are engine collections mandatory, or opt-in?

**Plain language.** The engine defines a fixed set of per-cell data collections that every frame carries. There is no way today for a game to say "I want the engine's Targets collection but not its Explosions" — the tuple is fixed at compile time and its *order* is baked into the CRC and the wire format.

**The decision.** Keep the fixed mandatory tuple, or add an opt-in registration mechanism so a game selects which engine collections it uses?

**Candidates:**

| Candidate | Where | E / I / R |
|---|---|---|
| Trackable-target registry (a generic "things that can be targeted, with subscriber counts") — contains zero client-only code | `Frame/Collections/Targets/*` (whole collection, ~230 lines) | E3 / I4 / R3 |
| Target acquisition: pick the least-subscribed target with the smallest angle to the firing direction | `Frame/Frame.cpp:458-517` (`GetMissileTarget`; the hard-coded 45.0f range at `:460` becomes a parameter) | E2 / I3 / R3 |

Both are Tier 3 because `Collections()` ordering is CRC- and wire-load-bearing, and moving a collection changes `Frame::kiVersion` (invalidating existing saves and replays).

**Researched recommendation (2026-08-11): mandatory, not opt-in.** Targets joins the fixed engine base tuples, **appended last**, and appears in both `Collections()` and `ServerCollections()`. Appending is safe here specifically because Targets is passive — nothing walks it implicitly; it is driven by explicit Sync calls — so tuple order carries no hidden phase dependency. `GetMissileTarget` moves to the engine base with the hard-coded 45.0f range becoming a parameter, and the `target_t` alias moves with it.

**Cost, stated plainly.** `Frame::kiVersion` bumps, which invalidates existing saves and replays. That is acceptable rather than alarming: the version gate makes a format change announce itself as a format change instead of surfacing as a mysterious desync.

**Why opt-in was rejected.** A registration mechanism would have to reproduce one identical deterministic collection order across two independently compiled builds. Getting that wrong produces desyncs, not errors — a brand-new failure class — and it would serve zero games today, since every game already carries eleven unconditional engine collections. One more empty collection costs a pointer-sized nothing next to that.

---

### D4 — What shape are the save and replay hooks?

**Plain language.** Saving a game and recording a replay both need the same thing from the game: "hand me your bytes" on write, and "here are your bytes back" on load. Only the shape of that handoff is open.

**The decision.** Virtual functions on the engine base class, `std::function` callbacks registered at startup, or CRTP (a compile-time template technique with zero call overhead but heavier syntax)?

**Candidates:**

| Candidate | Where | E / I / R |
|---|---|---|
| Grid save framing: versioned header, coordinates written in sorted order, staged-then-adopt load, clock-consistency check | `Save/GameSaveLoad.cpp:1118-1276` | E3 / I4 / R3 |
| Replay manifest / digest / writer-reader lifecycle: SHA-256 inventory with a domain-separated generation root, terminal writer retention, delayed reader activation, per-tick checksum validation | `Save/GameSaveLoad.cpp:18-1116` (~900 lines) | E5 / I5 / R3 |

The second is the single largest item in this document. A second game would otherwise reimplement roughly 900 lines including a security-shaped digest protocol — exactly the kind of code that is easy to write *almost* correctly.

**Researched recommendation (2026-08-11): required `game::` free functions. This overturns the earlier lean toward virtuals on the engine base.** Save and replay are driven from the server-side startup and file paths, and that layer's existing seam is uniformly free functions, not virtuals: `game::RunFrameTick`, `game::ReadFleetData` (already free), and the settings loaders called from `Engine/Source/Main.cpp:212-276`. Routing save/replay through base-class virtuals would introduce a second, competing seam style in the same call layer for no benefit — nothing here is on a hot path, so the choice is purely about consistency.

**The whole game-facing surface collapses to about ten domain-neutral symbols:** `WriteSaveState` / `ReadSaveState` / `AdoptSaveState` / `ResetSaveState` (staged state travels as an opaque forward-declared `game::SaveStagedState` behind a `unique_ptr`, so the engine never sees its fields), `WriteReplayMeta` / `LoadAndRestoreReplayMeta`, `OnReplayStreamsInvalidated`, `ApplyTransferStatusChanges` / `IsTransferType` (shared with D2's answer 2), and the reset/resync notifications. Hook names must be domain-neutral — no "Fleet" in any engine-side name.

**Two plans.** `Documents/Plans/File/GridSaveFramingToEngine.md` (E3/I4/R3, no dependencies) as the smaller proof, then `Documents/Plans/File/ReplayLifecycleToEngine.md` (E5/I5/R3, `dependsOn` the framing plan and `ActiveSetSkeletonToEngine`; it also hoists `mFrameInputs` onto `GameBase`).

**Named risk.** File-format identity must survive verbatim: manifest version 3, the domain-separation string, the artifact filenames, and the grid write order. So must the existing trust-boundary guards on everything read back off disk — this is the file's security-shaped code, and it is easy to rewrite *almost* correctly.

---

### D5 — Who owns the menu screens?

**Plain language.** The engine's ImGui manager currently holds a `std::unique_ptr` to each of nine concrete game screen classes by name (section 2). Seven of them — main menu, pause, sound, game settings, graphics, modal, death — are generic shells that any game would want.

**The decision.** Does `ImGuiManager` keep owning concrete screens (engine screens plus a game-supplied list), or does the game *register* its screens with the engine, which then knows only an abstract screen interface?

**Candidate.** The seven standard screens, ~880 lines across `Ui/Screens/` (757 `.cpp` lines plus headers). E3 / I5 / R2. They are near-move-as-is; the only game-specific residue is `NetworkSessionContract::kiCoordSlots` (`MainMenuScreen.cpp:101`, `:111`), the literal `"BROKEN ENGINE"` title (`MainMenuScreen.cpp:54`, `:61`), and a `ChangeFrame` virtual.

**Ordering caveat — this is the important part.** `Documents/Features/Engine/RmlUiPlayerFacingUi.md` proposes re-authoring exactly these screens on a different UI toolkit (RmlUi instead of ImGui). Extracting them to the engine first means porting the same seven screens twice.

**Researched recommendation (2026-08-11): move all seven screens into the engine as concrete `engine::` classes that `ImGuiManager` keeps hard-coding — and start now. This overturns both the "wait for RmlUi" ordering and the lean toward a registration mechanism.**

*No registration mechanism, no game-supplied screen list.* The engine simply owns seven concrete screens by name, exactly as `ImGuiManager` owns them today; a game that needs to change one derives from that screen, which is the precedent the existing `TweaksScreenBase` split already sets. Registration would be a new mechanism whose only payoff is aesthetic (no `game::` names in an engine header) — and the engine-owned screens remove those names anyway.

*Small residues.* `kiCoordSlots` stays a read of the game's `NetworkSessionContract`; the `"BROKEN ENGINE"` title becomes a required game symbol; `ChangeFrame` becomes a `GameBase` virtual. HudScreen stays in the game.

*Why not wait for RmlUi.* `Documents/Features/Engine/RmlUiPlayerFacingUi.md` is a manually executed Feature gated on a "revisit when" condition that has not fired, so waiting is waiting on nothing scheduled. More importantly, the eventual RmlUi port deletes each ImGui screen at parity — it does the same amount of work regardless of which tree the screen lives in, so there is no double-porting cost. Engine-owned screens mean that port happens once for all games instead of once per game.

**Shape:** one Tier-2 plan; `dependsOn` `UiStateHoistToGameBase`, `MenuUtilsToEngine`, `GraphicsQualityWrappersToEngine`, and the D7 localization plan.

---

### D6 — Do games share settings files, or get their own?

**Plain language.** Graphics quality, audio volume, and control preferences are saved to small binary files. A second game needs the same machinery.

**The decision.** One shared engine schema — the engine owns `GraphicsSettings.bin` and friends, including the version counter — or per-game files with an engine helper doing the read/write work?

**Candidate.** Split `ClientSettings.cpp:18-335` into an engine part; the game-specific `ClientStateSettings` at `:337-383` (fleet GUID, focused ship, camera height) stays in the game. E2 / I4 / R2.

**Hard constraint either way.** The version constants and the `.bin` filenames are file-format identity. If they change, every existing install silently loses its settings. Whichever option is chosen, those bytes must survive the move unchanged.

**Researched recommendation (2026-08-11): the engine owns the generic settings files outright** — schema, `.bin` filenames and version counters all move verbatim, guarded by a per-POD `static_assert(sizeof(...))` so a byte-layout drift is a compile error rather than a silently wiped settings file.

**What removes the strongest objection.** The obvious worry is two games fighting over one settings file. They cannot: `Engine/Source/File/FileManager.cpp:111-126` puts every save and settings path under a per-game AppData directory named from `game::kGameName`. Same schema, separate files on disk.

**Sequence and split.**

- `SoundSettings` — now, no dependencies.
- `GraphicsSettings` — after `GraphicsQualityWrappersToEngine`.
- `GameSettings` — with `iLanguage` validated against a game-supplied language count (see D7).
- `TweaksSettings` — stays in the game for now: developer-only and entangled with the D5 screens.
- `ClientStateSettings` — stays in the game (fleet GUID, focused ship, camera height are game concepts).

**Risk accepted.** Once a version counter is engine-owned, an engine-side bump resets that settings file for *every* game. That is tolerable because settings degrade gracefully to defaults — nobody loses progress, they lose a volume slider position.

---

### D7 — How do game strings extend engine strings?

**Plain language.** Localized UI text lives in one indexed table. The engine's own screens need generic strings ("Quit", "Resume", "Audio"); a game needs its own on top.

The header already anticipates the split — `Ui/Localization.h:20-44` ends the enum with `kBaseStringsCount` and then defines `kStringsCount = kBaseStringsCount`, which is the seam sitting there unused.

**The decision.** Two arrays with a dispatch on index (engine array below the base count, game array above), or one game array that simply starts numbering at the base count?

**Candidate.** The `Localization.h` mechanism plus the generic strings. E2 / I3 / R1.

**Researched recommendation (2026-08-11): two arrays sharing one index space.** The engine owns a base enum ending at `kBaseStringsCount` plus its own string table. The game's enum pins its first enumerator to `engine::kBaseStringsCount`, so the two enums form a single continuous numbering. `TranslatedString` dispatches on the index range: below the base count reads the engine table, at or above it reads the game table.

Each table keeps its own trailing sentinel row and its own deduced-extent `static_assert`, which preserves the row-count discipline the existing header already documents — each owner's table is checked against its own enum, independently, at compile time.

**Cheapest item in the document** (E2/I3/R1) with no dependencies, and it becomes a `dependsOn` edge for the D5 screens plan, since engine-owned screens need engine-owned strings.

---

### D8 — How does the camera learn what to follow?

**Plain language.** The engine has a top-down RTS camera. It needs to know where to point. Today it asks the game directly via `game::gpCamera`, which is one of the engine-depends-on-game inversions from section 2.

**The decision.** A virtual `ComputeTargetPosition()` on `CameraBase` that each game implements (camera pulls), or the game pushing a target position to the engine camera each frame (camera is told)?

**Candidate.** Hoist ~250 lines of the top-down camera update: `Graphics/Camera.cpp:15-326` — screen shake, day/night sun angle, jump/snap re-anchoring, Hermite-eased mouse-wheel zoom, texel-aligned eye-height reference, and the `SunAngle()` override at `:311-326`. E3 / I5 / R2. Also removes the engine→`game::gpCamera` dependency.

**Researched recommendation (2026-08-11): virtual pull, confirmed.** `CameraBase` takes the ~250-line update and gains `virtual std::optional<XMVECTOR> ComputeTargetPosition(const game::FrameInterpolate&)` — returning nothing means "I have no target right now", and the base extrapolates from the last-known position and velocity — plus `virtual void OnUpdateComplete()`. The last-known/extrapolation machinery stays in the base, so a game implements one small function and gets all the smoothing behaviour.

**Push stays rejected** for two concrete reasons, not just style: it re-creates the silent-freeze-if-forgotten hazard (a game that misses one frame's push gets a stuck camera with no error), and it sets an ordering trap against the camera's own `mfTime` advance — the game would have to push at exactly the right point relative to an internal clock it cannot see.

**Scope note.** Engine code stops reaching for `game::gpCamera` and uses an engine-typed `engine::gpCamera`; roughly 35 engine dereference sites close in the same change. The small `CameraInput` struct moves onto `CameraBase`.

**Sequencing and acceptance.** `dependsOn` the three pending Graphics camera plans — `CameraUpdateDecomposition` is the one that creates the seam boundaries this hoist needs — and acceptance reuses that plan's bit-identical eye-position harness check.

**Correction to this document.** Section 2 lists `GameBase.cpp:35-36` among the camera couplings; those two lines are `game::gpInput` (a D9 concern). The engine's actual camera calls are `Engine/Source/GameBase.cpp:603` and `Engine/Source/Main.cpp:291`.

**Follow-up, not part of this move.** Hoisting the `SunAngle()` override is gated on `UiStateHoistToGameBase`.

---

### D9 — How does a game add its own menu inputs, and where does deterministic input live?

**Plain language.** Two related things. First, menu actions (quit, pause, confirm, back) are stored as bit flags in one 64-bit value (`Input/Input.h:10-37`). A second game needs its own actions in there. Second, the input file currently mixes two unrelated jobs, which its own `Input/AGENTS.md` documents: the display-rate `Input` class that reads hardware (client only), and `FrameInput` — the per-tick deterministic change list — which compiles into *both* client and server builds. That dual role is why the file appears in both project files.

**The decisions.**
- (a) How does a game add flag bits? Reserve a high bit range for game use, or have the game poll separately and keep engine flags closed?
- (b) Split `FrameInput` (`Input/Input.h:55-65`, `Input/Input.cpp:154-201`) into its own game-owned file, which has project-membership consequences documented in `Input/AGENTS.md`.

**Candidates:**

| Candidate | Where | E / I / R |
|---|---|---|
| Input polling and the ImGui scroll-arbitration logic | `Input/Input.{h,cpp}` | E3 / I4 / R2 |
| Generic menu and debug input handling moved onto `GameBase` | `Game.cpp:591-883` (generic parts) | E2 / I3 / R2 |

**Sub-decision.** Does the base class handle input first and then delegate leftovers to the game, or does the game get first refusal? This is not academic: `Game.cpp:594-598` implements "Escape quits from the main menu, but backs out one level when a settings sub-menu is open" — that ordering is the behaviour, so whichever model is chosen must preserve it.

**Researched recommendation (2026-08-11): all three parts confirmed, as three plans in sequence.**

**(a) One shared engine `MenuInputFlags`, bits 48–63 reserved for games** (`kFirstGameMenuInputBit = 48`, following the existing profile-counter precedent for reserved ranges). A `GameBase` virtual `PollGameMenuInput` is called **before** the previous-snapshot capture — that placement is load-bearing, because edge detection ("was this pressed *this* frame?") compares against that snapshot, and polling after it would make every game bit permanently look like a held key. Separate game-side polling is rejected: it duplicates the snapshot state, which is a drifting-double-snapshot bug class.

**(b) Split first, hoist second.** Move `FrameInput` into `Frame/FrameInput.{h,cpp}` and register the new files in both vcxprojs. `FrameInput::kiVersion` stays 14 and the stream bytes are unchanged — file location is not part of format identity. `Input.{h,cpp}` then becomes wholly client-only and leaves the server vcxproj, which resolves the guard-scope contortion its own AGENTS.md documents. Only then hoist the generic `Input` class to `Engine/Source/Input/`, which deletes the `game::gpInput` inversion at `Engine/Source/GameBase.cpp:35-36`; that hoist `dependsOn` `UiStateHoistToGameBase`.

**(c) `ProcessMenuInput` hoist has a fixed ordering contract:** quit resolution → modal gate → cursor visibility → pause/back-out → engine tweaks → **game virtual last**. Base-handles-then-delegates, written down as an acceptance criterion. Game-first would turn correct Escape-key behaviour into a per-game obligation, which is precisely the copy-paste burden this document exists to remove.

---

### D10 — Does the client's "where am I in the world" state belong to the engine?

**Plain language.** The client tracks which world cell it is standing in, which neighbouring cells are visible, and the full list of active cells. That lives in `game::Game` today (`Game.h:163-173`: `mClientGridCoord`, `mVisibleNeighbors`, `mActiveCoords`, plus the `SetClientGridCoord` cache-invalidating setter).

**The decision.** Move that state to `engine::GameBase`, or leave it in each game?

**Candidates (both are developer-facing tools, so their risk is low):**

| Candidate | Where | E / I / R |
|---|---|---|
| Profile Frames and Network overlay screens — contain zero references to game entities | `Profile/ProfileManager.cpp:78-389`, `Profile/NetworkGraphs.cpp` | E3 / I3 / R1 |
| Server monitoring window — roughly 600 of 676 lines are generic | `Server/ServerDisplay.cpp` | E3 / I4 / R1 |

**Sub-decision.** What shape are the hooks the server display uses for the game-specific remainder? Note the caveat found during the sweep: that file uses file-static globals per window, so a naive extraction leaves shared mutable state in the engine.

**Researched recommendation (2026-08-11): move all five members to `engine::GameBase`** — `mClientGridCoord`, `mVisibleNeighbors[8]`, `miVisibleNeighborCount`, `mActiveCoords`, and the cache-invalidating `SetClientGridCoord` — and make `ComputeActiveSet()` a pure virtual. The engine does not merely read this state today, it *rebuilds* it: `Engine/Source/GameBase.cpp:183`, `:464`, `:481`, `:699-702`, `:735-740` and `Engine/Source/Main.cpp:289`, `:298`, `:419-420`. Leaving the storage in the game while the engine writes it is the inversion, not a boundary.

**Population policy stays in the game:** which cells a client subscribes to (camera visibility) and `UpdateDesiredCoords` are game decisions; the engine owns only the resulting state and its shape.

**ServerDisplay:** the engine takes the file roughly as-is — the per-window file-statics stay file-statics — and calls two required game free functions: one for per-cell per-collection counts, one for entity stat rows. **No display base class.** There is exactly one server window and no second one in prospect; a class hierarchy here is YAGNI.

**Three plans:** `GridStateHoistToGameBase` (Tier 2, sequenced with `ActiveSetSkeletonToEngine`) → profile overlay screens → ServerDisplay.

**Residual, noted not fixed.** `mClientGridCoord` is a misnomer on the server, where it means the save/replay focus coord. Renaming it is out of scope for these plans.

---

### D11 — How do games override engine build switches?

**Plain language.** `Pch.h:5-95` holds roughly 40 compile-time capability switches — desync recovery on/off, logging on/off, render thread on/off, per-category log levels. Engine code reads them, but the *game* declares them. A second game must declare all 40 correctly or fail to build in confusing ways.

**The decision.** A closed engine set that games may add to, or engine defaults a game can override? The current `inline constexpr bool` form matters here: it forbids the usual `#ifndef`-style override trick, so "overridable defaults" would need a different mechanism rather than a small edit.

**Candidate.** Move the engine-owned switches into an engine defaults header. E2 / I4 / R1.

**Constraint.** `Pch.h:97-98` carries a load-bearing include-order note (external headers must precede `ShaderLayouts.h`, which must precede `Engine.h`). Any restructuring must preserve that ordering and the comment explaining it.

**Researched recommendation (2026-08-11): a required game-supplied `BuildSwitches.h`, plus an engine-side contract header and a shipped reference copy. This overturns the earlier "closed engine set plus game additions" lean.**

**What the census found.** 34 of the roughly 41 switches are read by engine, Common or Tools code, yet each one genuinely needs a *per-game value* — the sandbox hard-enables `kbAgent` and `kbRenderDoc`, and the per-configuration blocks are game policy tables, not engine defaults. A closed engine set therefore cannot hold the values; it can only hold the *requirement*. Two more corrections fell out of the sweep: `keLogLevelNavData` is consumed by Common (not game-only as assumed), and `keNetworkSimulation` needs the `engine::NetworkSimulationLevel` type, so it must be declared *below* the `Engine.h` include — a second, deliberately documented post-engine switch site.

**The shape.**

1. The game owns `BuildSwitches.h`, split verbatim out of `Pch.h:5-95` (the include-order note below it is untouched and stays load-bearing).
2. The engine ships a contract header with one `static_assert` per required switch, so "you forgot a switch" becomes one documented diagnostic site instead of a confusing cascade of unknown-identifier errors deep in engine code.
3. The engine ships a reference copy of the switch file as the template a new game starts from.

**Rejected alternative, and why.** Making the switches `#ifndef`-overridable defaults loses the typed constants (`LogLevel`, `NetworkSimulationLevel` become raw macros) and is, honestly, ugly — macro-shaped configuration in a codebase that otherwise uses `inline constexpr` throughout.

---

### D12 — Does time-scaling become an engine packet?

**Plain language.** The server can run the simulation faster or slower and tells clients about it. The code on both sides is entirely engine-shaped except for one thing: the packet's ID number lives in the *game's* packet enum (`GamePacketType::kServerTimespeedUpdate`).

**Candidate.** `Network/Server/ServerSession.cpp:451-495` plus the client mirror `Network/Client/ClientSession.cpp:96-120`. E2 / I2 / R3.

**Researched recommendation (2026-08-11): promote it to an engine packet, with an ID below `kGamePacketStart`, and bump `kuiProtocolVersion` from 10 to 11 — landed *inside* the D1 network stage, never as a standalone change.** There are no other pending wire-format riders to amortize the bump against, so the right move is to bundle this with D1's codec work and pay for one upgrade event rather than two.

**Why the break is cheap here.** The packet is server→client only, and the handshake already rejects a stale build cleanly with a version mismatch rather than misparsing bytes — the failure mode is a clear "your client is out of date", not a desync.

**Land in the same change:** `Documents/Architecture/Network.md:77` (which currently states the protocol version as 10) and the packet contract table in that document.

---

### D13 — Where do the terrain-steering tuning numbers live?

**Plain language.** Two functions steer AI units around terrain: follow the contour at a preferred elevation, and avoid steep ground ahead. They contain zero game-specific symbols but 17 file-scope tuning constants (`Frame/TerrainUtils.cpp:8-130`, e.g. preferred elevation 0.2, steer rate 3.0, look-ahead distance 20.0).

**The decision.** Pass a caller-supplied parameters struct, or keep engine constants a game may override at compile time?

**Candidate.** E2 / I3 / R2. The only trade-off is cost in a hot loop: a parameter struct means the values are no longer compile-time constants, which costs a little per call in code that runs for every steering unit every tick.

**Researched recommendation (2026-08-11): an engine-owned `constexpr` tuning struct.** `engine::TerrainSteeringTuning` declares the 17 fields with today's values as its default member initializers; each game is required to define one `inline constexpr game::kTerrainSteeringTuning` instance, overriding only the fields it cares about with designated initializers and inheriting the rest. Function signatures do not change — the functions read the game's constant directly.

**Why not a runtime parameters struct.** These functions run per steering entity per tick inside CRC'd deterministic code. `constexpr` members keep compile-time folding and loop unrolling exactly as they are today; a runtime struct would defeat the unrolling and change hot-loop codegen for zero present benefit.

**Sequence** after `TerrainTraceToEngine`.

**Wider relevance.** This struct-of-defaults shape is also the natural resolution for the Explosions/WindDeposits tuning inversion listed in section 5 — same problem (engine mechanism, game values), same answer.

---

### D14 — What does the automation `describe_ui` report?

**Plain language.** The test-automation channel exposes about 850 lines of client commands — screenshots, RenderDoc capture, window resize, UI automation, profile queries (`Agent/AgentCommandsClient.cpp:463-1298` and `:1400-1428`). Only two lines touch the game at all (`:933-934`, which read the game's UI state and flag names). Once the already-planned `UiStateHoistToGameBase` lands, those two lines are engine state and the whole block can move to the existing `Engine/Source/Agent/AgentCommandsShared.cpp`.

**Correction to this document.** Those two coupled lines live in the `describe_ui` command (`Agent/AgentCommandsClient.cpp:929-934`, the `BuildDescribeUi` helper), not in `window_state` — `window_state` is at `:817-828` and has zero game coupling. The heading and framing above are corrected accordingly.

**The decision.** Does the report keep including game-specific flags via a small append hook, or does the engine report only its own UI state?

**Candidate.** E2 / I4 / R1. Near decision-complete — this becomes a plan the moment the question is answered.

**Researched recommendation (2026-08-11): no append hook — the engine reports both fields itself.** `GameFlags` is already an engine type, and `UiState` becomes engine state via `UiStateHoistToGameBase`, so after that plan lands there is no game-specific content left in these two lines for a hook to append. `UiStateName` and `GameFlagNames` move to the engine with byte-identical JSON key names and value strings, so existing automation scripts keep working unchanged — which was the whole point of the append hook, achieved without the hook. Games that need to report more still have `describe_scene` as the game-side extension point.

**Shape:** one Tier-2 plan; `dependsOn` `UiStateHoistToGameBase` only.

**Documentation caveat.** `Engine/Source/Agent/AGENTS.md:10` says shared handlers stay engine-generic and take game state such as the tick "as a plain `int64_t` rather than read from game globals". That sentence describes the build-agnostic handlers and will need a clarifying update when this block moves.

---

## 5. Residual inversions and notes

- **Explosion and wind tuning globals.** `Engine/Source/Frame/Collections/Explosions/Explosions.cpp:91-178` reads roughly two dozen `game::gExplosion*` wrappers, and `ExplosionsSpawn.cpp:120` reads `game::gWindDepositExplosions*`. These are engine→game inversions like the ones in section 2: the *values* are game content (how big this game's explosions look), while the *mechanism* is engine. **Proposed shape (2026-08-11):** the same struct-of-defaults answer reached in D13 — an engine-owned `constexpr` tuning struct whose default member initializers are today's values, with each game required to define one instance and override only the fields it cares about. Still needs its own pass to enumerate the fields, but the shape question is no longer open.
- **`FleetSelection` — recommended keep-in-game.** `FleetSelection.{h,cpp}`: the selection mechanism reads as generic, but every step of it calls `gpGame` for game-specific state. Extracting it would be E4 for only I2 of value at R2. Leave it in the game.
- **Architecture docs need confirming before D1/D2 execution — still the prerequisite.** The sweep flagged that `Documents/Architecture/Network.md` and `Documents/Architecture/GameReconciliation.md` describe the wire protocol and reconciliation flow in wording that may not match current code. Confirm both before any D1 or D2 plan is written, since those documents are the intended-behaviour authority above code. Partial progress (2026-08-11): the D2 research pass spot-checked `GameReconciliation.md` and found it matches the current code flow; its scope header would still need updating once the reconciliation files move to the engine. `Network.md` remains unconfirmed, and `:77` needs the protocol-version edit D12 carries.
- **New subdirectory.** `Documents/Investigations/Engine/` did not exist before this document; it follows the area-subfolder convention used by `Documents/Plans/Engine/`.
