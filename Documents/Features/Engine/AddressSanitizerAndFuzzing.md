# AddressSanitizer and Fuzzing

Revisit When: before any build is offered to players whose clients the host does not control (a public or open multiplayer release), or when a memory-corruption bug — a crash far from its cause, a torn collection, a heap error inside mimalloc — resists ordinary diagnosis.

## Context

- **AddressSanitizer (ASan)** — MSVC `/fsanitize=address`. The compiler instruments every load and store against shadow memory, and the runtime surrounds each heap allocation with poisoned bytes, so an out-of-bounds access, use-after-free, or double free stops the process at the faulting line instead of corrupting memory silently. Roughly a 2x slowdown and more memory.
- **Fuzzer** — libFuzzer, which MSVC supports through `/fsanitize=fuzzer`. It calls one entry point, `LLVMFuzzerTestOneInput(const uint8_t*, size_t)`, millions of times with byte buffers mutated toward new code coverage. libFuzzer supplies its own `main`, so each fuzz target is its own executable.
- **Why they pair.** A fuzzer only notices a crash; most memory bugs do not crash. ASan turns every silent overrun the fuzzer reaches into a stop with a stack trace. ASan alone only sees bugs the existing harness scenarios happen to reach.

No sanitizer configuration exists today: nothing in the tree outside `ThirdParty/` sets `EnableASAN` or `/fsanitize`, or calls `ASAN_POISON_MEMORY_REGION`.

## Obstacles

- **mimalloc replaces the allocator.** `Engine/Source/Memory/GlobalAllocator.cpp` replaces global `operator new`/`delete` with mimalloc, `<mimalloc-override.h>` in `Common/ExternalHeaders.h` replaces the C `malloc` family (including `_aligned_malloc`, which `common::MakeAligned` uses), and `ThirdParty/Prebuilts/Source/Engine/Mimalloc.cpp` reserves a 10 GiB arena at process init. ASan only guards allocations made through the CRT allocator it intercepts. The existing `ENABLE_CRT_DEBUG_HEAP` switch already routes all of these to the CRT and compiles out every mimalloc call (including the statistics in `Engine/Source/Server/ServerDisplay.cpp`); the ASan configuration reuses that routing. Check: whether its CRT debug-heap pieces (`_CRTDBG_MAP_ALLOC`, `_CrtSetDbgFlag` in `MemoryInitializer`) coexist with ASan; if not, the ASan configuration selects the same CRT operators without them. The ASan ThirdParty build also drops the `Mimalloc.cpp` unit so the arena reserve never runs.
- **Sub-allocators hide interior bounds.** ASan sees only the outer edges of a block:
  - A collection's SoA storage is one `common::MakeAligned` block that `AllocateAndAssign` (`Engine/Source/Frame/Collections/CollectionMemory.h`) carves into member arrays, each start rounded up to 64 bytes. A read past one member's capacity lands in the padding or the next member, and rows in `[iCount, iCapacity)` are allocated, so neither is reported.
  - The thread-local `common::Workbuffer` (`Common/Workbuffer.h`) is a stack sub-allocator over a `StableVector` `VirtualAlloc` reservation (`Common/StableVector.h`), not a heap block, so ASan sees nothing inside it, including use after `Pop`.
  - Closing both needs manual poisoning under `__SANITIZE_ADDRESS__` only: `ASAN_POISON_MEMORY_REGION` over inter-member padding (and optionally the unused rows past `iCount`), and over Workbuffer ranges on `Pop`, unpoisoned again on push.
- **ThirdParty prebuilts and STL annotations.** `ThirdParty.<Config>.lib` is built by `ThirdParty/Prebuilts/Platforms/VisualStudio2026/ThirdParty.vcxproj` without ASan. Linking instrumented and uninstrumented objects fails with an `LNK2038` mismatch on `annotate_vector`/`annotate_string`. Either build ThirdParty under the same ASan configuration (preferred; it also instruments ENet's parsing of raw datagrams), or define `_DISABLE_VECTOR_ANNOTATION` and `_DISABLE_STRING_ANNOTATION` across every project and lose container-overflow checks. Consumer provisioning requires the primary checkout's `Output/ThirdParty.$(Configuration).lib` for every configuration a worktree builds (`ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md` `## Consumer Provisioning`), so the new configuration needs a primary ThirdParty library too.
- **Tooling knows three configurations.** `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` accepts only Debug, Profile, and Release, and `.agents/scripts/Bootstrap-AgentTools.ps1` rebuilds the three primary ThirdParty configurations. Both, plus the harness launch, must accept the ASan configuration. `Projects/BrokenEngineSandbox/Source/Pch.h` selects its compile-time switches through an `#if BT_DEBUG` / `#elif BT_PROFILE` / `#elif BT_RELEASE` chain with no `#else`, so the ASan configuration must define one of those macros; that choice sets `kbDebugInput` and so which game packet types the network fuzz target can reach.
- **Platform.** Windows x64 only, like every existing configuration. Check: the ASan runtime DLL must be found by a harness-launched process.
- **Timing.** At about 2x slowdown the fixed-tick server can fall behind, and the per-tick rate budgets in `Server::Receive` and harness timeouts can fire. Timing-sensitive scenarios do not gate an ASan run.
- **Determinism.** Expected unaffected: instrumentation adds shadow checks around memory accesses, not arithmetic, and `/fp:strict` already holds results equal across differently optimized binaries (`Documents/FloatingPointDeterminism.txt`). The CRT allocator does change addresses, which would expose any address-dependent ordering. Check it, do not assume it: the replay determinism check passes on the ASan server, and a session between an ASan server and a normal client shows no CRC mismatch.

## Phases

1. **Server ASan.** An ASan configuration of `BrokenEngineSandboxServer` plus ThirdParty, run through the existing `/agent-harness` scenarios. The server is headless, so this is the simplest first step and covers the untrusted-input code under real traffic. Add the collection and Workbuffer poisoning here.
2. **Client ASan.** The same configuration for `BrokenEngineSandbox`. GPU memory, VMA's sub-allocated mapped memory, and the uninstrumented Vulkan loader and driver DLLs are invisible to ASan; triage reports raised inside driver code before treating them as engine bugs.
3. **Fuzz targets.** Separate `BT_SERVER` fuzz executables built with `/fsanitize=address /fsanitize=fuzzer`, ranked below. `/update-vcxproj` owns their project membership.

## Fuzz Focus Areas

Ranked by trust policy (root `AGENTS.md` `## Key Patterns`): the server fully validates client-to-server records and grid saves, so those readers are where hostile bytes arrive.

### 1. Client-to-server network records (intensive)

- **Message decoders (no seam).** `engine::NetworkMessages::Read(std::span<const uint8_t>, TMESSAGE&)` in `Engine/Source/Network/NetworkMessages.h` decodes from a byte span alone. One target switches on the first byte across `ClientAckStreamMessage`, `ClientDesyncReportMessage`, `ClientDebugFrameRequestMessage`, `ClientHelloMessage`, `ClientSubscribeMessage`, `ClientUnsubscribeMessage`, and `ClientResyncRequestMessage`.
- **Engine dispatch.** `Server::Receive(std::span<const uint8_t>, ENetPeer*)` (`Engine/Source/Network/Server/Server.cpp`) is public and takes bytes, and reaches the handlers past the decoders: `ClientAcknowledgementStream` into `ServerBufferedFrames::ApplyAckStream` (slot indices from the packet), `ClientHello`, `ClientSubscribe`, and the rest. **Seam needed:** it finds the client through `pPeer->data`, and `ClientHello`, `ClientDebugFrameRequest`, and `RecordContractViolation` (`enet_peer_disconnect`) act on the live `ENetPeer`. A target needs a constructed `Server` holding one handshaken `ClientConnection`, a way to send replies and disconnects without an ENet host, and per-input reset of the per-tick budget counters so inputs are not simply rate-dropped.
- **Game packets.** Game-range types queue into `Server::mReceivedGamePackets` and are decoded by `ServerSession::ParseReceivedGamePackets` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`) after `Server::AdmitGamePacket` checks `GetGamePacketContract`. The handlers read with raw cursor reads (`engine::ReadInt64`, `engine::ReadFloat`, `ReadBooleanByte`) whose bounds rest entirely on the contract sizes, which is exactly the mismatch ASan would catch. **Seam needed:** the target needs `gpGame`, `gpServer`, and the session's `mpFleetManager` and `mpBroadcaster`. Exclude the debug-control types (save, load, reset, replay, pause, timespeed): in Debug, `kbDebugInput` is `true` (`Projects/BrokenEngineSandbox/Source/Pch.h`), so they are client-sendable (in Profile and Release `GetGamePacketContract` in `Projects/BrokenEngineSandbox/Source/Network/GamePacketType.h` returns the non-sendable sentinel for them), but they do file I/O or whole-state replacement rather than parsing.
- Not worth a target: `NetworkDiscoveryResponder::Poll` reads only a fixed-size magic value. ENet's own protocol parsing is ThirdParty, covered by ASan only (Phase 1, instrumented ThirdParty).

### 2. Grid save reading

`engine::ReadGridSave(const FileFlags_t&, const std::filesystem::path&, StagedGridSave&)` (`Engine/Source/File/GridSave.cpp`) is the file trust boundary. It runs `ReadAndValidateVersionHeader<game::Frame>`, `game::ReadSaveState`, per-cell static data and island placement checks, then `operator>>(std::istream&, Frame&)` (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`), which reaches each collection's read, `AllocateAndAssign`, `ValidateAfterRead` (`Engine/Source/Frame/Collections/Collection.h`), and `ValidateCollectionPairs`/`ValidateCollectionPair` (`Engine/Source/Frame/FrameUtils.h`). **Seam needed:** it opens its own file through `gpFileManager->OpenFile`, and both `ReadAndValidateVersionHeader` (`Engine/Source/File/FileManager.h`) and `game::ReadSaveState` take `std::fstream&`, so a target needs a `std::istream` entry (or writes each input to a scratch file, far slower). It also needs `gpIslandTerrain` with the island templates loaded, since placements are checked against `mIslands`. Seed the corpus with saves written by `WriteGridSave` during harness scenarios. A single allocation may legitimately reach `common::kiMaxDeserializedBytes` (256 MiB), so libFuzzer's memory limits must sit above the reader's legitimate peak.

### 3. Lower priority

- **Replay streams and the agent command channel** — exempt developer tools (`Engine/Source/File/AGENTS.md` `## Replay Streams`, `Engine/Source/Agent/AGENTS.md` `## Architecture`); their checks may stay but no new ones are required.
- **Server-to-client data** — the client trusts it under the ENet datagram checksum and `ASSERT`s on corruption (`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`).
- **Pack files** — produced offline by DataPacker and trusted at runtime.

Phase 1 and 2 ASan runs still cover these passively.

## Routing Findings

An ASan report or fuzzer crash is a bug: diagnose it with `/external-diagnose-bug`, then fix it under the bad-value rule (`.agents/references/cpp-conventions.md`). The reader throws `std::ios_base::failure` where it detects the bad value, the existing dispatch catches drop the packet and call `RecordContractViolation` or fail the load, and the value is never clamped or repaired. A finding inside ThirdParty is reported upstream, not patched here. The crashing input is kept as a fuzz corpus seed, not as a unit test.

## Acceptance Criteria

- An ASan configuration builds the server and client with `/fsanitize=address`, the CRT allocator in place of mimalloc, and ThirdParty under the same configuration, with no `annotate_vector`/`annotate_string` link mismatch. Debug, Profile, and Release builds are unchanged.
- `/compile`, primary ThirdParty provisioning, and `/agent-harness` accept the configuration, and `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` and `ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md` document it.
- Collection inter-member padding and popped Workbuffer ranges are poisoned in ASan builds only; a one-off manual out-of-bounds probe into each is reported.
- The standard harness scenarios run on the ASan server with no ASan report, or every report is fixed or recorded as a follow-up Plan; the replay determinism check passes on the ASan server.
- One fuzz target each for the network message decoders, `Server::Receive`, game packet decoding, and `ReadGridSave` runs without a live ENet connection, seeded from real sessions and saves, for an agreed run budget with no crash, ASan report, or leak.

## Out of scope

- Other sanitizers and the Clang toolchain.
- Fuzzing ThirdParty code directly, and continuous fuzzing infrastructure.
- New validation in exempt or trusted readers.
