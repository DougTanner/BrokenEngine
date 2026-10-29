# Fixture zoom correction

## Correction

Before: a66da68abfd942591d92c9d69489e5e240b927e4; four existing Temp note edits.
Fixture used oblique 900/6000 m eyes and 45° FoV. Lowest/Default now use gameplay
150/300 framing, default FoV15°, straight-down view, lowest flight plane and shared clip constants.
Events retain the pan/cell crossing within these zoom endpoints. Rendering reads framing correctly.
Numeric capture cameras 0/1/2 mean Lowest/Default/Event. kiTilesPerFrame remains120.

Build required: Sandbox client/server Debug|x64 and ModelViewer Debug|x64.
Remaining: independent review/builds and fresh-binary preset/save/readiness smoke at1920x1080.
No runtime/build claims. Plan03 remains unchecked; exhaustive exports/radiometry/15-minute test incomplete.
Patch: Temp/foundation-zoom-correction.patch; scope/checks: Temp/foundation-zoom-scope.md.
