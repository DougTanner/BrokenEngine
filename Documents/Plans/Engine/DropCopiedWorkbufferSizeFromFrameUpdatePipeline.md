<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T22:26:51.677Z","dependsOn":[]} -->
# Drop the Copied Workbuffer Size from FrameUpdatePipeline

## Context

`Documents/Architecture/FrameUpdatePipeline.md` `## Frame Registry Query Windows` states that "the
worker workbuffer's 64 KiB initial size remains a performance preallocation, not a hard bound".
Code owns that value: the `common::Multithreading` constructor
(`Common/Threading/Multithreading.cpp:12`) passes the literal `65'536` to each
`PersistentWorker`, which `ThreadLocal` uses as the initial workbuffer size
(`Common/Threading/ThreadLocal.cpp:9-16`).

Root cause: the document copies a number code owns, which the root `AGENTS.md` progressive
disclosure directive forbids ("A doc names the owning identifier and never copies a number,
default, range, or tuning value that code owns"). The copy matches the code today, so nothing
reads wrong yet; it goes stale silently on the next change to the literal.

Originating gap: found while recording a frame registry follow-up. The sentence is pre-existing:
it reads the same at `8fa794fa` (line 41).

## Design

The author recommends deleting the number and naming the owner, because the sentence's point is
that the size is a preallocation rather than a bound, which needs no value. For example: "The
worker workbuffer's initial size (`common::Multithreading`) remains a performance preallocation,
not a hard bound". The rest of the sentence and paragraph stay as they are.

## Critical files

- `Documents/Architecture/FrameUpdatePipeline.md`

## In scope

- The workbuffer initial-size sentence in `## Frame Registry Query Windows`.

## Out of scope

- `Common/Threading/Multithreading.cpp` and `ThreadLocal`: no named constant is introduced for
  the `65'536` literal.
- Any other number in the document, and every other document.
- The registry scratch wording in the same paragraph, owned by
  `Documents/Plans/Engine/SizeRegistryScratchForSubscriberCountsOnly.md`.

## Risk and invariants

- Tier 1. Trigger: documentation only.
- No code, CRC, wire, save, replay, or build exposure.

## Acceptance criteria

- `## Frame Registry Query Windows` no longer states a workbuffer size and names the owner.

## Coordination

No directional prerequisites. Reciprocal coordination with
`Documents/Plans/Engine/SizeRegistryScratchForSubscriberCountsOnly.md`: both Plans edit the
second paragraph of `## Frame Registry Query Windows`, so whichever lands second refreshes that
paragraph. Each Plan keeps its own acceptance.

## Notes

- Line 41 is from `8fa794fa`; locate the sentence by its text, since that paragraph is reworded
  by the change landing with this Plan.
