# Engine/Data/Shaders/Shadow - Top-Down Shadow Compute Pipelines

## Overview

Two independent compute chains post-process top-down (orthographic, world-XY-mapped) shadow data for the RTS camera. The terrain chain ray-marches sun occlusion, runs horizontal and vertical Gaussian blur, temporally resolves the final result, then refreshes distinct history storage.
The object chain blurs its top-down object-coverage raster separately, merged by MAX blending.
R and G split each ship's ground coverage as `1 - t` and `t`, where `t` is its centre's height above the ground directly below it (terrain elevation, or sea level over water) minus the lowest flight-plane height, as a fraction of the lowest-to-highest plane span, clamped to `[0, 1]`, so a ship over high terrain trends back toward Sigma Low.
B and A hold coverage by ships on flight planes 1 and 2 only. Vertical blur recombines R and G into R, capped at full coverage, so terrain and water read object-shadow R; models consume both final textures as `../Model/AGENTS.md` describes.

## Terrain Shadow Contract

- Inverse coverage is load-bearing: terrain shadow stores `1 - shadow` (1.0 = fully lit), so clamp-to-border-white sampling outside texture coverage degrades to no shadow rather than an artifact.
- Full-texture work: every terrain pass that runs processes its whole texture. Group counts are written to per-framebuffer indirect buffers every frame from the texture extent, and each invocation rejects the tile-rounding tail against `imageSize` of the image it writes.
- Record-once dispatching: the elevation prepass and the five terrain passes—source, horizontal blur, vertical blur, temporal resolve, and history copy—remain in the recorded Global command buffer as indirect draws and dispatches. Between refreshes the final texture and history keep their last refresh; when the chain refreshes is owned by Render (`../../../Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`).
- Consumer validity: terrain-shadow consumers read the final texture directly through the border-white sampler, which degrades to no shadow outside coverage.
- Temporal validity: temporal reprojection samples history only when the reprojected coordinate stays inside `[0, 1]`; disocclusions and reset history use current output. A separate history-copy pass transfers the result between distinct storage images after temporal resolve, avoiding same-image read/write aliasing.
- Sun direction is loop parameters, not branches: morning versus evening flips the march increment, start offset, and width-scale sign. The ray-march is 1-D along texture rows because the sun azimuth is east/west; the wider, sunward-extended elevation raster admits off-screen occluders.
- Shadow shaping: per-occluder shadow is feathered by sun-versus-terrain angle and distance falloff, then max-accumulated. An elevation height-fade band limits low-beach noise at dawn and dusk, while the moon multiplier fades terrain shadow with the lighting envelope.
- Terrain and object blur differ: terrain radius is a compile-time constant, while object blur receives its radius and a per-channel weight half-kernel built from runtime settings: R blurs with Sigma Low and G with Sigma High, so the ground blur follows ship height smoothly, and B and A interpolate Low to High by their plane height's fraction of the lowest-to-highest span.
  A per-channel sigma rules out bilinear tap pairing, so both object passes use discrete output-grid taps. Both object inputs remain linear, clamp-to-edge; both passes blur every channel, the horizontal pass on raw coverage, and vertical blur converts it to multiply-with-lighting form.
