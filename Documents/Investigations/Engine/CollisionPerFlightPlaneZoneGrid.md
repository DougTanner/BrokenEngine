# Per-Flight-Plane Collision Zone Grid

**The edge case to watch is always ships, blasters, or missiles entering a layer by transiting between planes: Fighters climbing or descending through 27.5 m and 52.5 m, Omnis through 27.5 m (they fly only up to the 40 m plane), Omni blasters fired on a pitch, and falling missiles. Any scheme that files an object under one plane must still pair it with everything within reach of every height it passes during the tick, or it silently drops a hit and changes the CRC.**

## Baseline: the height check

`TestAndCollectPair` (`Engine/Source/Frame/Collision.cpp`) starts with a height check, before the alignment lookup: it skips a projectile/ship pair when the two objects' start-to-end Z ranges for the tick are more than the combined radius plus 10 m apart.
It reads no flight plane, so transits are covered by each object's whole tick range, and it never changes an accepted candidate. The XY zone grid is unchanged: a projectile on the 65 m plane still visits every ship below it that shares an XY zone, and the check rejects the pair with a few comparisons.

## Alternative to test

- Each layer-pair zone grid gets a flight-plane dimension.
- Ship-side (layer B) objects register in every flight-plane band their start-to-end height range, widened by their own radius plus 1 m, reaches. A height maps to a band by the pushers' nearest-plane rule (`PusherLayer` in `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`, moved into `engine::FlightPlanes` only if this approach is adopted).
- Projectiles (layer A) query only the bands their own widened range reaches.
- `sTestedBGeneration` deduplicates a ship found in more than one band.
- Each grid's index storage is sized on its first insert, so a band no object reaches commits no zone memory.

## Question

In massive battles with hundreds of ships across all flight planes, how do `kCpuTimerPostRenderCollide`, pairs visited, pairs reaching the 3D test, and committed zone memory per simulation thread compare between the grid and the landed height check?

## Method

- Record one replay of such a battle, with Fighters and Omnis ordered between planes in both directions, Omnis firing while between planes, and missiles falling through planes. Harness-injected positions always take z 15 m, so ships reach the higher planes only by orders.
- Play it on both builds; both must end every loop with bit-identical CRCs (`.agents/skills/agent-harness/references/replay.md` `### Replay determinism acceptance`).
- Count pairs visited and pairs reaching the 3D test with temporary counters, and read committed zone memory per thread.
- Time `kCpuTimerPostRenderCollide` with `query_profile` cohorts per `.agents/skills/agent-harness/references/command-reference.md` `## Durable caveats`; the two sides are different builds, so they cannot alternate within one process lifetime.

This becomes a Plan only if the grid wins.
