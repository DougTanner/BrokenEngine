# Focused rebase documentation review

## Coherence scope

Authorization: user instruction Rebase from main. Review is limited to conflict resolutions in Engine/Data/Shaders/Lighting/AGENTS.md, Engine/Data/Shaders/Water/AGENTS.md, Engine/Source/Graphics/AGENTS.md, Engine/Source/Graphics/Render/AGENTS.md and Engine/Source/Ui/AGENTS.md, plus the four uncommitted documentation updates named in Temp/rebase-propagation.md ## Exact edits. Unrelated main prose and the full lighting implementation were excluded.

Compared the old foundation d170b18b843a3a96af227cef467b824b4aa214c5, rebased source 1c3608533ba58d1a1327b51928ed4dced5426c53, and new baseline cd6d410c064a5a5f905bf68efaf28493ab6fc33a. Scope authorization, minimality and KISS checks pass.

## Coherence findings

DOC1 Required Engine/Data/Shaders/Water/AGENTS.md:69 — the conflict resolution deletes the
still-valid water view-direction contract together with obsolete local-light projection prose. New
main's former line69 required water eye/reflection/Fresnel to use f4WaterEyePosition and smoke
projections to retain ToEyeLine. Water.frag:83 still derives its view vector from
f4WaterEyePosition, and CameraBase.cpp:369 supplies the ordinary framing-based virtual water eye;
comparison overrides it at line394. Graphics/AGENTS.md:113 mentions only comparison water shading,
so the ordinary shader contract is no longer stated. Restore the water-eye requirement and retained
smoke eye-line requirement without restoring obsolete lighting projection language.

No other semantic findings. Graphics settings version21, FOV/orthographic persistence, Boolean
validation and defaults agree with GraphicsSettings.cpp. Camera comparison prose agrees with
CameraBase.cpp:388-398 and MainUniforms.cpp:236-256. Capture-control prose agrees with
TweaksScreen.cpp:312-313 and 406. The four note updates consistently identify the new baseline and
source checkpoint, keep historical version255 rejection/build evidence historical, require new
builds/static checks, and preserve the unapproved runtime-reference decision. PushersInterpolate
version5->6 is the sole version delta from old foundation; Frame.cpp:106 and FrameBase.h:313 retain
the same summed version formula, supporting current256 from historically established255. No new
runtime outcome is claimed.

## Progressive disclosure result

Status: PASS
Skill: progressive-disclosure-review
Baseline: cd6d410c064a5a5f905bf68efaf28493ab6fc33a
Files checked: 5 — Engine/Data/Shaders/Lighting/AGENTS.md; Engine/Data/Shaders/Water/AGENTS.md; Engine/Source/Graphics/AGENTS.md; Engine/Source/Graphics/Render/AGENTS.md; Engine/Source/Ui/AGENTS.md (conflict regions only)
Findings: none
Changed files: none
Decisive checks: Get-SessionChangeInventory.ps1 -Regions -Head 1c3608533ba58d1a1327b51928ed4dced5426c53 with the five instruction-document PathPrefix values — status pass, truncated false, unlistedUntracked0.
Decisive checks: Conflicted contract prose remains in its owning shader/graphics/UI instruction layer; no new duplication, misplaced mechanics or changed SKILL.md bodies.
Build required: none
Evidence: Temp/rebase-docs-inventory.json — scoped instruction-document regions.
Residuals: none

## Shared handoff

Status: NEEDS_ACTION
Scope: PASS
Coherence: DOC1 — restore the valid water-view and smoke-eye-line contract deleted with legacy lighting prose.
Findings: DOC1 Required Engine/Data/Shaders/Water/AGENTS.md:69 — live water-eye contract dropped during conflict resolution — Water.frag:83 and CameraBase.cpp:369,394 retain it; baseline AGENTS line69 required it.
Changed files: none
Decisive checks: Reviewed all five conflicted AGENTS regions and four uncommitted documentation diffs; current identities/settings/save-version claims and historical verification labeling agree.
Decisive checks: Progressive disclosure review PASS for five scoped AGENTS files; no package changes.
Build required: none
Evidence: Temp/rebase-docs-review.md ## Coherence findings — finding rationale and source evidence.
Evidence: Temp/rebase-docs-review.md ## Progressive disclosure result — complete progressive disclosure handoff.
Residuals: DOC1 needs manager disposition; builds/runtime were outside this review and remain pending.
