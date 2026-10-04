<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:52:19.470Z","dependsOn":[]} -->
# Express the focused keyboard snapshot copy with std::copy_n

## Context

`RawInputManager::Update` in `Engine/Source/Input/RawInputManager.cpp` currently copies the hardware keyboard scratch array into the published snapshot with an indexed loop. The operation is a complete fixed-count copy, and `std::copy_n` states that operation directly without incidental indexing. This is the narrowly accepted B56 application; the Plan contains its complete rationale and does not depend on a feature-adoption investigation.

`Engine/Source/Input/RawInputManager.h` declares `kiKeyboardKeyCount` as positive constexpr `int64_t` value `0xFF` (255). `RawInputManager::mpbKeyboardKeysDown` and `RawInput::pbKeyboardKeys` are distinct non-overlapping `bool[kiKeyboardKeyCount]` arrays. `Common/ExternalHeaders.h` already includes `<algorithm>`. There is no need for an include, helper, storage, or type change. A search of existing Plans for `RawInputManager`, `copy_n`, `copy_if`, `move_backward`, and keyboard-copy wording found no existing owner.

## Design

Inside the existing `if (bHasFocus)` keyboard branch of `RawInputManager::Update(bool bLostFocus)`, replace only the indexed loop with:

```cpp
std::copy_n(mpbKeyboardKeysDown, kiKeyboardKeyCount, mRawInput.pbKeyboardKeys);
```

In the comment immediately above the final `if (bScriptActive)` overlay branch, change only the phrase `keyboard copy loop` to `keyboard copy`. The resulting second comment line is:

```cpp
// published snapshot. Must run AFTER the keyboard copy so synthetic key bits are not overwritten by it.
```

The call performs the same 255 `bool` assignments between separate arrays, with no predicate, allocation, conversion, move semantics, additional pass, or execution policy. No negative performance mechanism is introduced; no speedup is claimed. Keep the call at the loop's existing location. Preserve the focus gate, unfocused clearing, physical-input suppression, and overlay order exactly.

## Critical files

- `Engine/Source/Input/RawInputManager.cpp`: `RawInputManager::Update`, focused keyboard-copy branch and overlay ordering comment; the only implementation edit target.
- `Engine/Source/Input/RawInputManager.h`: read-only evidence for the count and distinct array declarations.
- `Common/ExternalHeaders.h`: read-only evidence for the existing `<algorithm>` include.
- `Engine/Source/Input/AGENTS.md`: read-only owner of snapshot, focus, and synthetic-input contracts.

## In scope

- Replace the focused indexed keyboard-copy loop in `RawInputManager::Update` with the exact `std::copy_n` call above.
- Correct the nearby overlay comment's `keyboard copy loop` phrase as specified above.

## Out of scope

- Other loops or adoption sites, including `copy_if`, `move`, or `move_backward`.
- Array declarations, extents, layout, ownership, includes, helpers, or public signatures.
- Focus, input suppression, keyboard clearing, mouse/gamepad processing, synthetic input, and publication timing behavior.
- Architecture documentation, AGENTS.md, or style-policy amendments: existing guidance already covers this local expression change; none is warranted.
- Tests, benchmarking infrastructure, new runtime checks, and unrelated comment cleanup.

## Risk tier and invariants

Future implementation is **Tier 1**, triggered by a local behavior-preserving substitution with no public signature or invariant exposure. It changes only how an existing fixed-count copy is expressed. A change to focus behavior, snapshot contents, or overlay order would exceed this Plan's scope and invalidate this classification.

- Exactly indices `[0, kiKeyboardKeyCount)` are copied from hardware scratch to the published keyboard when focused; the source is unchanged.
- The early return when neither focused nor script-active remains unchanged.
- An unfocused active script still receives a cleared published keyboard before overlay.
- `gpAgentInput->Overlay(mRawInput)` stays conditional on `bScriptActive` and after hardware snapshot publication, so synthetic key bits cannot be overwritten by the copy.
- No added allocation, pass, temporary buffer, or synchronization; no deterministic frame, serialization, or protocol changes.

## Acceptance criteria and verification

| Criterion | Required future evidence |
|---|---|
| The focused keyboard copy directly expresses its operation | Diff shows the exact `std::copy_n` call replacing only the indexed loop, retaining the surrounding branch. |
| Copy bounds and semantics remain equivalent | Inspect both array declarations and `kiKeyboardKeyCount`; confirm equal `bool` extents, non-overlap, unchanged source/destination direction, and positive count. |
| Focus and overlay invariants remain intact | Inspect the full `Update` diff and surrounding function to confirm early return, unfocused `std::fill`, suppression logic, and final conditional overlay are untouched. |
| The local comment remains accurate for the replacement | Diff shows only `keyboard copy loop` becoming `keyboard copy` in the specified comment. |
| The algorithm is available to the client translation unit | Run `/compile` for BrokenEngineSandbox client, `Debug|x64`, using Shared runtime data because this source-only change touches no asset-generation inputs. Confirm compile/link success. |
| Scope and performance constraints are satisfied | Review confirms only the two prescribed edits, existing `<algorithm>` availability, and no new allocation, predicate, conversion, temporary, or extra pass. |

The client is the only affected executable; no server or tool build is required. No `/agent-harness` scenario is needed because the diff and array declarations settle the unchanged semantics without runtime observation. Do not add unit tests. Apply the repository's triggered C++ correctness, style, comment, affected-code, and documentation checks during future implementation; a no-change documentation result is expected. These future checks have not been performed by this Plan-writing task.
