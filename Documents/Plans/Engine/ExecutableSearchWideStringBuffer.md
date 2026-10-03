<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:48:52.932Z","dependsOn":[]} -->
# Use a wide string for executable search output

## Context

`DataPacker/Source/FileManager.cpp` anonymous-namespace `FindExecutableOnPath(const wchar_t* pcExecutable)` currently sizes a `std::vector<wchar_t>` with a `SearchPathW` query, fills it with a second call, then constructs a `std::wstring` from the successful range before constructing the returned filesystem path. The extra text copy is unnecessary: the destination already owns native wide text.

The [SearchPathW contract](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-searchpathw) specifies buffer size including terminator space, successful length excluding the terminator, and zero for failure. An insufficient buffer reports the required size including the terminator. [MSVC's filesystem implementation](https://raw.githubusercontent.com/microsoft/STL/main/stl/inc/filesystem) defines `path::string_type` as `std::wstring` and moves `path(string_type&&)` into its native storage. These facts support replacing the intermediate vector without altering API behavior.

This applies contiguous string storage through the repository's current C++23 idiom. C++11 guarantees contiguous `basic_string` storage; writable `data()` arrived in C++17 and is not a C++11 library addition. Inspection at baseline `d29fed456d3ede935c5e672f95f13d6733f0660c` found no existing Plan owning this function or `SearchPathW` change.

## Design

1. Replace `std::vector<wchar_t> path(uiCharacters);` with `std::wstring path(uiCharacters, L'\0');`.
2. Preserve both `SearchPathW` calls, all their arguments, and both failure guards exactly. The second call continues to receive `static_cast<DWORD>(path.size())` and `path.data()`.
3. After the second failure guard, add `path.resize(uiWritten);` so the logical string contains precisely the returned path characters and excludes the API terminator.
4. Replace the final expression with `return std::filesystem::path(std::move(path));`.

The string has the same writable element count as the old vector while the API runs. Shrinking after success requires no additional text buffer; moving the native string removes the vector-to-string copy and its separate allocation for heap-backed paths. No new helper or benchmark framework is needed.

## Critical files

- `DataPacker/Source/FileManager.cpp` — `FindExecutableOnPath`, the only implementation change.
- `DataPacker/Source/AGENTS.md` — existing FileManager ownership constraints; read-only reference.
- `Documents/C++StyleGuide.txt` rule 40 — retains the null-terminated API parameter exception; read-only reference.

## In scope

Only the buffer declaration, successful resize, and return expression in `FindExecutableOnPath`. Preserve the current parameter name/type and local names. No documentation or style amendment is warranted for this local representation change.

## Out of scope

Other Win32 buffers, `GetRepositoryRootFromExecutable`, encoding helpers, executable lookup policy, retries, new failure diagnostics, signature changes, includes unrelated to this edit, repository-wide adoption guidance, project membership, runtime/game changes, and new tests.

## Risk tier and invariants

Future implementation is Tier 1: a local behavior-preserving representation change with no public signature or invariant exposure. Changing lookup behavior, failure handling, or FileManager's worktree validation would exceed this scope and require reclassification.

- First-call failure returns `std::nullopt` before allocating.
- The writable range during the second API call remains `uiCharacters` wide characters, including API terminator space; do not substitute `reserve`, subtract one, or resize before validating.
- Second-call zero or `uiWritten >= path.size()` returns `std::nullopt`, preserving current behavior if the required size changes between calls.
- Success satisfies `0 < uiWritten < path.size()` before resizing, and returns the same native characters with no embedded trailing terminator in the logical path.
- Search arguments, search order, Unicode representation, and FileManager consumers remain unchanged.

## Acceptance criteria and verification

| Criterion | Required future evidence |
|---|---|
| Single wide-text owner replaces vector-to-string conversion | Focused diff shows the specified declaration, post-guard resize, and native-string move return; no intermediate string construction remains. |
| Success and failure behavior is preserved | Source review maps both unchanged guards and API calls to the documented length/terminator contract, including zero and insufficient-buffer outcomes. |
| No unnecessary copying or allocation is introduced | Source review confirms shrinking the existing string and selecting MSVC's native `path(string_type&&)` constructor; no conversion or explicit copy remains. |
| Supported tool target compiles and links | Run `/compile` for `DataPacker Release|x64`; require a successful structured build result. |
| Scope and conventions are preserved | Review the changed function and final diff; follow the applicable C++ change workflow reviews. |

No `/agent-harness` scenario is required: this changes an offline tool's private buffer ownership and its behavior is settled by the focused diff and API contract. No asset export or game launch is required. These checks are future implementation requirements; plan authoring has not run them.
