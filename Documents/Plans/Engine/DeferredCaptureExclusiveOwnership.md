<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:24:19.253Z","dependsOn":[]} -->
# Give deferred agent capture polls exclusive ownership

## Context

C++23 inventory F068, `std::move_only_function`, has a concrete ownership application in the agent command server. This is a refactor of existing developer capture behavior, not a new capability. Evidence was checked against baseline `6c6245f4b1dee82d7721ef271ce4925ca754ff47`.

`Engine/Source/Agent/AgentCommandServer.h:41,89` and `AgentCommandServer.cpp:431` use `std::function<std::optional<nlohmann::json>()>` for `DeferResponse` and `mDeferredPoll`. The noncopyable server moves the parameter into its sole member; every member use checks, invokes, assigns, or clears it. No consumer copies the poll. Nevertheless, the copyable wrapper forces the two capture lambdas in `AgentCommandsClientGeneric.cpp:138,307` to use shared ownership for their private state, allocated at lines 124 and 285. Neither state escapes to another owner or is used after deferral.

`CaptureCommandState::~CaptureCommandState` (line 69) and `RenderDocCaptureState::~RenderDocCaptureState` (line 96) restore an originally minimized window when capture has not completed. These side effects make ownership and destruction timing the important equivalence proof. Existing `Common/ScopedLambda.h` and `Common/Threading/PersistentWorker` already use the feature, providing toolchain precedent.

The callable constructor accepts move-only targets, clearing destroys the target, and invocation requires a nonempty wrapper; small-object allocation avoidance is not guaranteed. See [WG21 P0288R9, specification](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0288r9.html#specification). No performance or allocation-free claim motivates this plan.

## Design

The author's recommendation is to replace the three deferred-poll type occurrences with `std::move_only_function<std::optional<nlohmann::json>()>`, retaining the by-value parameter and existing move into `mDeferredPoll`. This expresses the server's existing single ownership and permits move-only closures without another abstraction.

In `BeginCaptureAndDefer`, use `std::unique_ptr<CaptureCommandState>` initialized by `std::make_unique<CaptureCommandState>()`; capture it with `pState = std::move(pState)`. Preserve the existing moved `QueueCapture` capture and mutable call operator. Apply the same pointer change and move capture only to `RenderDocCaptureState` in `CommandRenderDocCapture`, retaining the borrowed `pRenderDocApi` capture.

Keep both state objects separately allocated. Moving a unique pointer transfers the original allocation without moving the state object; moved-from pointers do not invoke its destructor. Directly capturing state values could introduce destructor side effects on moved-from state and is outside this design. Before capture construction the local pointer owns cleanup; during closure/wrapper construction the owning temporary does; after deferral the server's poll does. An exception during any ownership-transfer stage still leaves exactly one owner responsible for destruction during unwinding. The old local shared pointer survives until the setup function returns, but it is unused and cannot overlap a later main-thread drain; removing this temporary extra owner changes no normal cleanup point.

Preserve this cancellation and completion matrix without editing the branches:

| Existing site in AgentCommandServer.cpp | Required equivalence |
|---|---|
| `ClearDeferredResponse`, line 129 | Clear the closure before clearing request metadata; destructor calls this before fixture shutdown. |
| `Drain`, stale generation, line 313 | Destroy state before returning; never publish the stale response. |
| `Drain`, timeout, line 325 | Destroy state before publishing the existing timeout failure. |
| `Drain`, result or poll exception, line 348 | Destroy state before publishing the response; pending `nullopt` retains ownership. |
| `Drain`, handler exception, line 420 | Destroy any installed closure before publishing handler failure. |
| `DeferResponse`, line 433 | Move ownership into the member at the existing assignment; replacement retains the existing old-target cleanup semantics. |

The existing `if (mDeferredPoll)` at line 300 guards the only invocation at line 335. Earlier stale/timeout resets return, and the socket thread never writes the member. Retaining these paths makes the stronger nonempty-call precondition safe without adding another check or fallback.

All other callers pass compatible lambdas, including the game audio-streaming and client-subscription fixture command modules. Leave their shared state alone: this plan establishes exclusive ownership only for the two engine capture states.

## Critical files

- `Engine/Source/Agent/AgentCommandServer.h` — deferred API declaration and member type.
- `Engine/Source/Agent/AgentCommandServer.cpp` — matching definition; lifecycle paths are verification sites.
- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` — two state allocation and capture sites.
- `Engine/Source/Agent/AGENTS.md` — audit existing one-in-flight and teardown contracts through `/update-claude-docs`.
- `Documents/C++StyleGuide.txt` rule 5 — existing RAII guidance suffices; no style-policy edit is recommended.

## In scope

- Change only `DeferResponse`'s declaration/definition, `mDeferredPoll`'s type, and the two pointer declarations/initializers and lambda captures in `BeginCaptureAndDefer` and `CommandRenderDocCapture`.
- Verify every `DeferResponse` caller and all member reset/invocation sites for compatibility and lifetime equivalence.
- Audit the owning Agent AGENTS.md; its existing single-in-flight/main-thread/teardown contracts remain accurate, so leave its text unchanged unless the implementation demonstrates a necessary factual correction within this ownership scope. Do not duplicate standard-library guidance there.

## Out of scope

Other callback or shared-pointer migrations; Common's existing adoption; direct state-value capture; new aliases, helpers, includes, configuration, compatibility paths, fixtures, or unit tests; changes to capture phases, destructor logic, response formats, deadlines, request IDs, connection generations, threading, graphics or RenderDoc integration; performance benchmarking.

## Risk and invariants

Future Change Workflow Tier 2: one subsystem's ownership refactor changes a public C++ parameter type and exposes cancellation lifetimes. It changes no thread ownership, trust boundary, protocol or serialization format, data layout of simulation state, replay compatibility, or deterministic computation. Game callers require compilation verification but no independently owned subsystem behavior change.

Preserve command schemas, outputs and failure envelopes; poll frequency and phase progression; same-thread destruction and minimization restoration; live dependency order at teardown; generation rejection and timeout thresholds; allocation suppression in `Drain`. State remains heap-owned and the poll remains an indirect call. Callback wrapper allocation details are unspecified. Simulation CRC, fixed tick scheduling, `.pack` versions, saves and replays are unaffected.

## Acceptance criteria

1. All three deferred-poll declarations/definitions use the same move-only signature; all callers compile without copying the wrapper or changing their behavior.
2. Exactly the two named capture states use unique ownership and moved pointer captures. Neither state object moves or gains another owner. No original state is destroyed by a moved-from closure.
3. Every row of the lifecycle matrix retains its guard, phase, destruction/publish ordering and outcome, including setup exceptions before installation and handler exceptions after installation.
4. Screenshot success from visible and minimized windows returns the existing path/size result and creates its file; the minimized case is minimized again before successful completion. Failed asynchronous save retains the failure envelope and restoration behavior.
5. RenderDoc capture retains its capture paths and minimized restoration when RenderDoc is available; unavailable API retains its existing immediate failure. A missing optional runtime is reported explicitly rather than counted as successful capture evidence.

## Verification

Use `/compile` for BrokenEngineSandbox client and server builds with agent functionality enabled, including client Debug to compile audio/subscription fixture consumers. Inspect every `DeferResponse` call and every `mDeferredPoll` occurrence. Compare the two state bodies and each lifecycle-matrix branch before/after; they should be unchanged. Review the transfer chain for allocation/construction throws and ensure moved-from pointers are empty and borrowed RenderDoc dependencies retain their original lifetime.

Through `/agent-harness`, run screenshot from visible and minimized client states, record the returned existing file/dimensions and observed post-command window state, and exercise a save failure using an invalid destination. Run RenderDoc capture with `--renderdoc` when available and check returned existing `.rdc` files and minimized restoration. Also check the existing unavailable-API failure without RenderDoc. Exercise existing disconnect/timeout cleanup scenarios where the harness provides them; static review of every corresponding reset branch is required regardless of runtime availability. Do not add test hooks to force these branches. Record precisely which lifecycle cases have live evidence and which are settled by unchanged control flow plus ownership tracing.

Apply the repository's C++ correctness, comment, style, affected-code, documentation and acceptance reviews. No unit tests or benchmarks are required. Finalization follows the normal Change Workflow only after the implementation's acceptance evidence passes.

## Notes

No duplicate live Plan was found by searches for the deferred API/member, the two state types, `move_only_function`, and deferred-poll ownership. No dependencies or mandatory Coordination constraints are identified. This is an author's implementation recommendation for future approval; it does not authorize implementation during plan creation.
