# Queue raw-profile activation events

Revisit When: the `ASSERT` in `ProfileManagerBase::ArmRawCpuTimerEventLocked` fires in real harness use, meaning a caller needs more than one pending `navQueryActivation` arm at a time.

## Context

A raw CPU timer activation event is one slot: one pending arm, one retained payload, cleared only by an exact-sequence acknowledgement or, for an unpublished arm, by a rejected latch. Arming while an arm is still pending asserts, because a second arm would silently overwrite the first arm's minimum sample tick, so the first arm would lose its measurement window.

## Design

Replace the one-slot arm with a small bounded queue of pending arms, each carrying its own minimum sample tick and receiving its own event sequence on publication. The retained event stays one slot, so the next queued arm publishes only after the harness acknowledges the previous event by its exact sequence, as today. Keep the queue outside deterministic frame state.

## Acceptance

- Two `inject_status_changes` requests with `navQueryActivation:{arm:true}` in a row both succeed; the first publishes its event, and after the harness acknowledges it the second publishes its own event with its own floor.
- A full queue rejects the arm before the status-change queue is swapped.
