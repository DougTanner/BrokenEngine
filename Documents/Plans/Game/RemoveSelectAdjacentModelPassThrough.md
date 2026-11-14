<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:05:34.372Z","dependsOn":[]} -->
# Remove the SelectAdjacentModel pass-through from ModelViewer

## Context

`ModelViewerApplication::SelectAdjacentModel` in
`Projects/ModelViewer/Source/ModelViewerApplication.cpp` (lines 65-68) has a
one-statement body: `SelectModel(miSelected + iStep);`. It exists only so that
`Main.cpp` can step the selection without reading the private `miSelected`.
`Documents/C++StyleGuide.txt` rule 49 forbids a function whose implementation
is only one pass-through call and says a forwarding layer is never justified.

Current callers, verified against commit `213d2577`:

- `SelectAdjacentModel`: one caller, the Left/Right key branch of
  `WindowProcedure` at `Projects/ModelViewer/Source/Main.cpp:55`, passing `-1`
  or `1`.
- `SelectModel` (definition at `ModelViewerApplication.cpp:52-63`): two
  callers, `SelectAdjacentModel` and the scene list in `RenderUi` at
  `ModelViewerApplication.cpp:1122`, passing the absolute row index `i`.

`miSelected` (`ModelViewerApplication.h:83`) is correctly private under rule
49's second paragraph: `SelectModel` changes it together with
`miSelectedAnimation`, `mAnimationTime`, the `kPlaying` flag and the reloaded
source metadata. Logged as a refactor candidate on 2026-11-07.

## Design

Recommended: make `SelectModel` take a step relative to the current selection
and delete `SelectAdjacentModel`. The other shape the candidate offered —
`Main.cpp` computing the index from a public read of `miSelected` — is not
recommended, because rule 49 allows no getter, so the read would require moving
`miSelected` to `public:`, exposing to outside writes a member that must change
together with the playback and metadata state.

1. `ModelViewerApplication.h`: change the declaration to
   `void SelectModel(int64_t iStep);` and delete the `SelectAdjacentModel`
   declaration.
2. `ModelViewerApplication.cpp`, `SelectModel`: rename the parameter to
   `iStep`, compute `const int64_t iSelection = miSelected + iStep;` first, and
   replace the `iSelection == miSelected` term of the early-return test with
   `iStep == 0`. The range test and the rest of the body stay as they are.
3. `ModelViewerApplication.cpp`: delete the `SelectAdjacentModel` definition.
4. `ModelViewerApplication.cpp`, scene list in `RenderUi`: change
   `SelectModel(i)` to `SelectModel(i - miSelected)`.
5. `Main.cpp`, Left/Right key branch: call `SelectModel` with the same
   `uiWordParameter == VK_LEFT ? -1 : 1` argument.

Behavior is unchanged: a step of zero (clicking the selected row) and a step
past either catalog endpoint still return without effect, including with an
empty catalog.

## Critical files

- `Projects/ModelViewer/Source/ModelViewerApplication.h`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`
- `Projects/ModelViewer/Source/Main.cpp`

## In scope

- `ModelViewerApplication::SelectModel`: declaration, parameter, and the
  early-return test.
- `ModelViewerApplication::SelectAdjacentModel`: declaration and definition,
  deleted.
- The `SelectModel` call in the scene list of `ModelViewerApplication::RenderUi`.
- The `SelectAdjacentModel` call in `WindowProcedure` (`Main.cpp`).

## Out of scope

- The visibility of `miSelected` and every other member: unchanged.
- `ModelViewerApplication::TogglePause` and any other short function in these
  files, and the separate refactor candidate near
  `ModelViewerApplication.cpp:1306`.
- The key-handling conditions in `WindowProcedure` (repeat bit, text-editing
  test, withholding the keys from ImGui).
- `Projects/ModelViewer/AGENTS.md`: its Left/Right and mouse selection
  contract names no function and stays true.

## Risk tier

Tier 2 — scoped behavior. Trigger: the change alters the public signature of
`ModelViewerApplication::SelectModel` and removes a public method, which Tier 1
excludes; it stays inside one application (ModelViewer). No determinism/CRC,
wire, serialization, replay, threading, or trust exposure: ModelViewer has no
simulation state or network session.

## Acceptance criteria

- No `SelectAdjacentModel` identifier remains under `Projects/ModelViewer/`.
- `miSelected` is still declared under `private:`.
- ModelViewer builds without errors or new warnings.
- In a running viewer, Left and Right each move the selection by one model and
  stop at the first and last catalog entries, and clicking a row in the scene
  list selects that row; clicking the already selected row keeps the selected
  clip and playback state.

## Notes

The live criterion needs a catalog of at least two models; the supplied
evidence records no harness setup for it.
