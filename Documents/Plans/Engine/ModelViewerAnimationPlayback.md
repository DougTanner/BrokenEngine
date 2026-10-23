<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-21T19:14:05.276Z","dependsOn":[]} -->
# Add animation playback and an exported articulated sample

## Context and design

The viewer application stage supplies an ordinary engine game host with viewer-owned frame/render/UI hooks, packed scene selection, true rest-pose rendering, engine materials/shaders, orbit/zoom and sun controls. Add animation controls and one original normal-pipeline sample through that integration. Reuse existing engine indexed animation evaluation and manager lifecycle; no pack format change or separate renderer/backend is needed.

The author recommends keeping playback state in ModelViewerApplication: selected clip (or Rest pose), time and
play/pause. Populate the animation dropdown from the selected packed scene's animation catalog, prepend Rest pose, and
expose only Rest pose for static models. Selecting a clip resets time to zero and starts looping; play/pause
controls advancement. Switching models resets to rest pose and refits; changing animation retains framing.
Advance selected clips by frame elapsed time and loop using that clip's actual duration, without relying on
the game's clip-zero looping helper. Rest pose uses bind/rest transforms, not clip time zero. Feed selected clip index/time through the viewer-owned render hook to existing indexed animation evaluation.

Add Engine/Data/Models/AnimationSample/AnimationSample.gltf as a small original articulated model with
embedded geometry, three joints, and clips named Bend and Turn. Use a simple visible material and normal glTF
skinning/transforms. Both starting clip poses differ visibly from bind pose, and the two motions visibly
differ. Export through existing DataPacker so the viewer receives it as an ordinary Scene/model/animation
asset. Keep the existing glTF loader and pack formats unchanged; do not commit generated pack files.

## Critical files

`ModelViewer/Source/ModelViewerApplication.{h,cpp}`, viewer-owned frame/render/UI hooks, `Engine/Source/Graphics/AnimationData.{h,cpp}`, and new `Engine/Data/Models/AnimationSample/AnimationSample.gltf`.

## In scope

- ModelViewerApplication state declarations, model-selection reset, animation dropdown/play-pause widgets, elapsed-time advancement and selected clip/time publication to the viewer render hook. Keep existing camera/sun/model behavior, including the rule that animation changes do not refit the camera.
- Viewer-owned scene animation catalog, selection reset and frame/render pose evaluation: connect clip index/time to existing indexed AnimationData evaluation and preserve the true-rest branch and selected-resource lifecycle. Correct only integration defects exposed by the clips/sample; retain the existing Graphics/resource owners and ImGui backend.
- AnimationData rest/indexed evaluation and material/joint output loop: only a demonstrated missing calculation or correction required by observable sample acceptance that viewer hooks cannot provide; preserve indexed engine animation behavior and packed layout. Do not assume a newly extracted rest-pose API exists.
- New AnimationSample.gltf: authored geometry, material, three-joint skin and Bend/Turn animation payloads. No loader/exporter changes are authorized by this plan.
- `ModelViewer/AGENTS.md` and `Engine/Source/Graphics/AGENTS.md`: only playback ownership and rest/indexed-pose contracts if those need updating.

Named regions and mechanically necessary declarations/includes are the target and ceiling. The sample is a shipped original asset, not a temporary source edit or test harness.

## Out of scope

Timeline scrubbing, speed controls, camera animation, animation editing/import UI, new animation blending, engine game animation behavior changes, exporter/pack-format changes, dynamic lights/shadows, unit tests, and unrelated graphics cleanup, application-neutral manager/context interfaces, resource/world separation and renderer/ImGui backend extraction.

## Risk and invariants

Tier 3: asset-to-renderer integration and skinned GPU output contracts. Preserve parent transform order, skin-joint offsets/reservation lifetime, descriptor publication/fence order, current trust validation and pack/wire/save/replay versions. Playback is viewer/client visual state and never simulation/CRC state.

## Implementation checkpoints

Complete these checkpoints in order within this single delivered stage; they are not separate landing or
approval stages. Before the first edit, capture the comparison baselines required by Acceptance criteria
below. Record technical checks and user-quality observations separately with evidence and pass/open results,
using the screenshots/video or timed observations and diagnostics required there. A failed or unobserved
gate leaves its checkpoint open and stops dependent work; technical success alone cannot close a visible
quality gate. All final acceptance obligations remain required.

1. **Authored/exported sample and rest display.** Author the specified sample and export it unchanged
   through the existing pipeline; inspect it in the working viewer before adding playback controls.
   - Technical gate: build/export through /compile, inspect the three joints and Bend/Turn payloads, and
     verify true rest evaluation and unchanged loader/pack contracts as Acceptance criteria requires.
   - User-quality gate: the sample is recognizable and completely framed at a useful size, with a readable
     material and correct visible rest geometry. Record the rest view for comparison with both clip starts
     in checkpoint 2; no playback UI is required at this checkpoint.
2. **Playback and controls.** Add the selected clip/time/play state, dropdown and play/pause behavior,
   including loop timing, clip resets, static-model choices and the true-rest branch.
   - Technical gate: verify selected clip/time publication and actual-duration looping under Context and
     design; build the viewer configurations required by Acceptance criteria.
   - User-quality gate: Rest pose differs visibly from both clip starts, Bend and Turn produce distinct
     motion, and the displayed selection and play/pause state are understandable. Observe looping, a fixed
     paused pose, continuation on resume and restart/play on clip change; controls respond promptly and
     animation changes retain framing. Static scenes expose only Rest pose.
3. **Transitions and regressions.** Verify model/clip transitions and repeat the existing viewer and engine
   scenarios; close every item in Acceptance criteria, including required reviews.
   - Technical gate: verify model-switch reset/refit, resource lifetime and validation diagnostics during
     animated switching/resizing/minimize/restore; complete affected builds and engine baseline comparison.
   - User-quality gate: switching models visibly returns to Rest pose and a useful fit; camera, sun and UI
     interactions remain readable and responsive while animated. Repeated switching and resizing show no
     stale poses or visual corruption, and baseline rest rendering and engine animation remain correct.
     Any direct GUI observation without evidence remains open.

## Acceptance criteria

- Before first edit, record the working viewer's selected static/skinned scene rest poses, framing, sun behavior and switching via direct GUI, and an existing engine animated-model scene via `/agent-harness`, for same-stage comparison.
- Build DataPacker Release and run the normal Local generation through `/compile` to export the new sample; inspect generated scene/animation data for three joints and Bend/Turn, with no pack-format change. Build viewer Debug/Release and affected client/server targets through `/compile`. Generated packs/headers remain untracked.
- Direct GUI: choose the sample, verify actual Rest pose differs from time zero of both clips; select each clip and observe distinct motion and looping; pause and confirm pose remains fixed, resume and confirm continuation; switch clips and confirm restart/play; switch models and confirm Rest pose reset and fit; static scenes offer only Rest pose. Keep camera framing unchanged during animation. Record screenshots/video or equivalent timed observations and validation diagnostics.
- Repeat existing viewer orbit/zoom, sun, ImGui capture, resize, minimize/restore and repeated model switching while animated; no resource/validation errors. Repeat existing engine animated-model scenario and compare to baseline.
- Run static checks, required C++/asset coherence reviews, docs synchronization and Tier-3 adversarial review. No new harness command or unit test is added. Report any direct GUI criterion not observed as open.

## Coordination

Keep sample and playback controls together so every new behavior is directly verifiable in this stage. The existing DataPacker pipeline is consumed unchanged; an unexpected exporter defect requires diagnosis and a scoped decision rather than an adjacent loader rewrite.
