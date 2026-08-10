# WindRadials - Controlled Wind Deposits

Client-only wind radials are stationary, fire-and-forget splats spawned by explosions. Their short controller lifetime deposits outward impulse into the GPU wind field.

## Invariants

- Controller keyframes are multipliers for each instance's base intensity and size; no normalization invariant is enforced.
- `PersistentMembers()` copies controller/start-time metadata and the base intensity/size. When wind is enabled, `Update` carries position from the previous frame and recomputes derived intensity/size.
- Disabling wind skips animation update and rendering, but expiry still runs. A disabled update copies each row's position from the matching previous row before returning, leaves derived intensity and size untouched, and avoids stale position state across a disable/enable transition.
- The radial shader derives outward direction from the splat center. This differs from directional wind trails even though both share the wind-deposit ping-pong buffers.
- Buffer resize must refresh the wind-deposit descriptor for both ping-pong pipelines.

## See Also

- `../WindTrails/AGENTS.md` - Directional wind deposits
