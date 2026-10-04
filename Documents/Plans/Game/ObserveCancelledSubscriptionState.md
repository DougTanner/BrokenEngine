<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:40:46.307Z","dependsOn":[]} -->
# Observe cancelled-subscription state without a dummy lifetime token

## Context

`Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures.cpp` allocates a `CancelledSubscriptionState` for its deferred response and a second `shared_ptr<int>` solely to tell whether that response remains alive. The anonymous-namespace `spCancelledFixture` observes the integer; `CommandClientCancelledSubscriptionFixture` captures both pointers and casts the unused integer pointer to void. Observing the existing state directly removes the redundant allocation, control block, and capture while preserving the fixture's admission lifetime.

Source evidence was checked against baseline `d29fed456d3ede935c5e672f95f13d6733f0660c`:

- `ClientSubscriptionFixtures.cpp:21,216,307-317,367-370`: the observer is used only by the admission `expired()` check, assignment, and detach reset; the deferred callback already owns `pState`.
- `Engine/Source/Agent/Commands/ClientNetworkFixtures.cpp:65-73,199-229`: `ArmCancelledSubscription` stores only a weak pointer. `ObserveUnsubscribeAck` and `Reset` lock it into function-local shared pointers on existing main-thread paths. Neither stores or returns a strong owner. Reset marks the outcome and clears the engine observer while the callback retains ownership.
- `Engine/Source/Agent/AgentCommandServer.cpp`, `Drain`, `ClearDeferredResponse`, and `DeferResponse`: pending polls retain their captures; completion, failure, request-connection cancellation, drain timeout, and teardown clear the owning poll before later command dispatch. No owner extends the cancelled state across that next admission check.

No existing Plan found by a search for cancelled-subscription fixture/state/token ownership duplicates this work. This Plan carries its own rationale and does not depend on the C++11 investigation document.

## Design

1. Change `spCancelledFixture` to `std::weak_ptr<engine::ClientNetworkFixtures::CancelledSubscriptionState>`.
2. In `CommandClientCancelledSubscriptionFixture`, remove `std::shared_ptr<int> pLifetime = std::make_shared<int>(0);` and assign `spCancelledFixture = pState` at the existing assignment location after `ArmCancelledSubscription`.
3. Remove `pLifetime` from the deferred lambda capture and remove `(void)pLifetime;`. Keep the existing `pState` capture and all other callback code.

The old token and state share the callback's persistent lifetime. Temporary engine locks do not overlap subsequent command admission, and dropping the engine observer on reset does not release callback ownership. Game detach explicitly clears its observer today and must continue to do so. This equivalence is the basis for the local refactor; do not replace the weak observer with a strong owner or add `enable_shared_from_this`.

The change removes one `make_shared` allocation/control block and one strong callback capture per invocation. It adds no operation to the callback, no lock, and no hot-loop work. Static inspection suffices for that performance claim; a timing benchmark is unnecessary.

## Critical files

- Edit: `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures.cpp`.
- Read-only ownership evidence: `Engine/Source/Agent/Commands/ClientNetworkFixtures.h` and `.cpp`; `Engine/Source/Agent/AgentCommandServer.cpp`.
- Verification contract: `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md`, `client_cancelled_subscription_fixture`.
- Existing ownership documentation: engine and game `Source/Agent/AGENTS.md`; existing RAII guidance: `Documents/C++StyleGuide.txt` rule 5. No documentation or style amendment is warranted because these contracts remain accurate.

## In scope

Only the anonymous-namespace `spCancelledFixture` declaration and the dummy-token declaration, observer assignment, lambda capture, and void cast in `CommandClientCancelledSubscriptionFixture`. Verify the unchanged admission check and `DetachClientSubscriptionFixtures` reset against the ownership evidence above.

## Out of scope

Changes to other subscription fixtures, engine hooks, transport cancellation, command schemas/results, subscription policy, timing, session teardown, allocation tracking, public interfaces, project membership, or documentation. No repository-wide smart-pointer adoption, new helpers, `enable_shared_from_this`, new harness commands, or unit tests.

## Risk and invariants

Future implementation is Tier 1: a local behavior-preserving simplification with no public-signature or invariant exposure. The implementation must preserve the existing ownership contract rather than change it. If ownership inspection discovers a persistent additional strong owner or cross-thread ownership, return that evidence for reclassification instead of broadening this Plan.

- The deferred callback remains the persistent strong owner between command frames.
- `spCancelledFixture.expired()` admission and detach reset remain unchanged.
- Reset, completion, failure, connection cancellation, timeout, and teardown preserve their existing next-admission behavior.
- Every command result, transition, timeout, and failure envelope remains unchanged.
- No deterministic state, CRC, wire format, serialization, or threading behavior changes.

## Acceptance criteria

| ID | Criterion | Settling evidence |
|---|---|---|
| A1 | The observer names `CancelledSubscriptionState`; no dummy integer allocation, `pLifetime` capture, or void cast remains. | Focused source diff. |
| A2 | Persistent and temporary ownership prove the same admission lifetime across pending, reset, completion, cancellation, failure, and detach paths. | Fresh scoped ownership review of the named functions, including clearing the poll before later dispatch; unchanged guards/reset. |
| A3 | The changed client compiles and links, and runtime prerequisites are available. | Successful `/compile` Debug x64 client and server results. |
| A4 | The existing cancelled-subscription scenario still completes through a real server unsubscribe ACK with unchanged transitions and policy. | `/agent-harness` result and Network Debug log described below. |
| A5 | Completion releases admission for a subsequent invocation in the same connected session. | A second successful invocation after the first response, using an eligible coordinate and checking the same fields. |

## Verification

These checks are required during future implementation; none has been run by this Plan-writing task.

1. Review every use of `CancelledSubscriptionState`, `spCancelledFixture`, `ArmCancelledSubscription`, `ObserveUnsubscribeAck`, `Reset`, and deferred-poll clearing. Confirm no state reference escapes an engine hook, and no owning cycle or asynchronous owner was introduced. Confirm the diff is limited to the five edits above. Preserve allocation suppression as written.
2. Run `/compile` for BrokenEngineSandbox client and server, `Debug|x64`, explicitly naming the following agent-harness scenario as the build trigger. Use Shared runtime data: only this game Agent C++ file changes, with no asset or data-generation change. The build result must provide both executables and the harness-required `DataBuildMode`, `RunDataPacker=false`, and normalized `GameDataDirectory`.
3. Through `/agent-harness`, launch and connect those endpoints, wait for an assigned player and settled subscriptions, and choose a coordinate outside active slots and desired/sticky/queued policy with no outstanding subscribe record and a clean unsubscribed slot. Set client `set_log_level {"category":"Network","level":"Debug"}`. Run `client_cancelled_subscription_fixture {"coord":[x,y]}` with that eligible coordinate. Require `initialState="subscribing"`, `afterCancelState="unsubscribed"`, `afterAcceptState="unsubscribing"`; require `subscribingToUnsubscribed`, `acceptToUnsubscribing`, `policyUnchanged`, and `ackRetired` all true. Retain the `Client::ServerSubscribeAccept Cancelled` log and response.
4. After the first response completes and the same prerequisites are satisfied, invoke the fixture again in the same session with an eligible coordinate. Require the same successful fields and absence of a stale `is already active` failure. This exercises callback completion and subsequent admission through existing capabilities. Cancellation/reset ownership equivalence is settled by A2; no new mechanism to force those timings is required.
5. Complete the applicable C++ correctness, comment, style, affected-code, and documentation consistency workflow checks. No extra source or documentation change is authorized merely to produce review activity.
