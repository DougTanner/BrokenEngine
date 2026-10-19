# Omnis - Armored Shielded Ships

Omnis are the armored, shielded ship type; fleet navigation, flagship identity, and the other shared ship rules are in `../Ships/AGENTS.md`. Collision, terrain-push, and pusher sizing all derive from the shared omni radius, so tuning size means changing that one constant.

## State Boundaries

- Shared CRC excludes client animation time, the local records tracking each pusher, and server-assigned global identity. Some excluded state still participates in save or transfer.
- The simulation keeps `pNextBlasterFireTimes` in [0, `kBlasterFireInterval`], which `SpawnBlasters` asserts. `PostRead` rejects out-of-range rows on every Frame read.
- The simulation keeps the Interpolate `pDestroyedTimes` in [0, `kDestroyTime`]. The Interpolate `PostRead` rejects out-of-range rows on every Frame read.
- `OmnisPostRender::Spawn(SpawnInfo)` is the asserting overload of the engine bounds rule (`../../../../../../Engine/Source/Frame/AGENTS.md`): an omni outside its cell is a bug, so it asserts instead of refusing and always reports success.
