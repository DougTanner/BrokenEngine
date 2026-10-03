<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:57:01.204Z","dependsOn":[]} -->
# Construct DataPacker UTF-8 path storage from its character range

## Context

`DataPacker/Source/FileManager.cpp:23`, anonymous-namespace `PathFromUtf8`, currently constructs its already-owned UTF-8 intermediate with `std::u8string value(reinterpret_cast<const char8_t*>(utf8Value.data()), utf8Value.size());`, then returns `std::filesystem::path(value)`. The accepted adoption review identified this one-line opportunity to remove an incorrectly typed source range without introducing storage or changing the path conversion. This is existing-code debt reduction, not a new char8_t rollout.

C++23 [N4950 basic.lval/11](https://timsong-cpp.github.io/cppwp/n4950/basic.lval#11) permits object access through its own/similar type, its corresponding signed/unsigned type, or char, unsigned char and std::byte. [basic.fundamental/9](https://timsong-cpp.github.io/cppwp/n4950/basic.fundamental#9) defines char8_t as distinct, with unsigned-char underlying type. It is neither the corresponding unsigned type of char nor a universal object-access type. Reading char storage through a char8_t glvalue is therefore invalid. A pointer cast alone is not an access, and installed bulk-copy implementations can avoid typed element reads: this is not evidence of an observed miscompile. Reverse access to char8_t storage through char is permitted.

## Design

I recommend replacing only the constructor statement with:

```cpp
std::u8string value(utf8Value.begin(), utf8Value.end());
```

Keep the return expression, signature and all callers. Integral conversion preserves every byte, including negative signed-char values, by the destination-width congruent-value rule in [N4950 conv.integral/3](https://timsong-cpp.github.io/cppwp/n4950/conv.integral#3). For example C3 A9 remains C3 A9; embedded zero remains zero. The supplied range retains its length and undergoes no normalization or locale conversion.

The installed MSVC 14.51.36231 headers under `C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/include/` provide the structural performance evidence: `xstring:848` unwraps/counts the char iterator range; `_Construct_from_iter` at `xstring:982` computes capacity and makes the existing single allocation beyond SSO, then calls `_Copy_n_unchecked4` at line 1013. `xutility:4890` recognizes same-size integral types as bit-copy assignable; `xutility:5078` selects `_Copy_memmove_n` at runtime. `filesystem:212` still converts the resulting u8string from UTF-8 to wide characters. This retains the existing allocation and bulk copy, with no additional copy, transcoding loop or repeated growth. This is implementation-equivalence evidence, not a measured speedup; recheck the route if the toolchain has changed.

Risk: **Tier 1**, local behavior-preserving implementation change with no public signature or invariant exposure. Git parsing, path encoding, ownership, allocation count, and filesystem conversion remain unchanged. No simulation, CRC, threading, wire, replay, `.pack` or version change is involved.

## Critical files

- Change: `DataPacker/Source/FileManager.cpp:PathFromUtf8`, constructor statement at line 25.
- Read-only evidence: the same file's `RunGit`, `TrimLine` and worktree discovery; `Common/WindowsUtils.cpp:RunExecutable`; the installed MSVC headers cited above.
- Read-only boundary evidence: `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:PathFromParameter` and `CommandRenderDocCapture`; `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:BareFilenameParam` and `PathToUtf8`.

## In scope

- Replace the single `PathFromUtf8` u8string constructor statement with the iterator-range form shown above.
- Review the unchanged callers and library implementation to substantiate byte preservation and no added allocation/copy.

## Out of scope

- All three remaining agent-command casts: client `PathFromParameter:24`, client `CommandRenderDocCapture:372` existence check, and game server `BareFilenameParam:82`. They currently convert borrowed narrow storage directly. An owning u8string replacement adds a copy and can add a heap allocation, so the no-negative-performance requirement has not been proven for them. Their infrequency does not establish non-regression.
- `PathFromParameter` currently passes `c_str()` and truncates at the first embedded NUL; a full-range owning replacement changes that behavior. No NUL rejection or truncation-policy decision is authorized here.
- RenderDoc's `GetCapture` buffer has its final terminator removed, then the filesystem checks a cast of `c_str()` while the original narrow bytes enter the JSON `paths` array. Preserve these distinct uses.
- `BareFilenameParam` rejects non-string JSON, empty names, embedded NUL, separators, colon, `..`, and reserved device basenames including UTF-8 superscript COM/LPT suffixes. Its contract belongs to `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts`. The permitted reverse char8_t-to-char read in `PathToUtf8`, save/load echoes, narrow JSON parsing and response `dump()` remain unchanged.
- No cross-site conversion helper, JSON representation/framing changes, normal-code-page narrow path construction, or deprecated `std::filesystem::u8path`. The latter provides direct UTF-8 conversion in installed `filesystem:1410` but is marked `_CXX20_DEPRECATE_U8PATH`. No reusable engine/common UTF-8-to-wide helper was found; `Common/StringUtils.h:ToString` converts the opposite direction, and DataPacker's private `DiagnosticReporter.cpp:Utf8ToWide` falls back to ACP on invalid UTF-8, changing the contract.
- No caller changes, process transport/Git parsing changes, unit tests, general cleanup or policy rollout.

The rejected agent scope requires separate evidence of a current nondeprecated replacement preserving encoding and existing NUL semantics without an added allocation/copy, or explicit user acceptance of that cost. This Plan does not approve or require that follow-up.

## Acceptance criteria

1. The implementation diff contains only the specified constructor replacement; the return and callers are unchanged.
2. Source/library review confirms full-range byte preservation for empty input, ASCII, embedded NUL and high-bit UTF-8 bytes, plus the existing SSO/one-allocation and bulk-copy route. `RunExecutable` still captures `ReadFile` byte counts into std::string; `RunGit` still trims trailing CR/LF/NUL, and worktree discovery still splits porcelain-z output at its first NUL before conversion.
3. Build DataPacker Release x64 through `/compile` and record the result. No game harness run is needed for a private offline constructor-only change; no runtime or measured performance claim is made by this Plan.
4. Required C++ style, comment, affected-code and documentation reviews find no propagation or documentation delta. `Documents/C++StyleGuide.txt` rule 11 governs C++ casts but needs no new char8_t policy: the edit removes a cast and leaves naming/formatting intact. `/update-claude-docs` records no AGENTS.md update because ownership, encoding and external behavior do not change.

## Coordination

No prerequisites or mandatory coordination. Existing Plans were searched by PathFromUtf8, FileManager, char8_t, UTF-8, aliasing and Coordination; none owns this DataPacker constructor or outcome. Engine FileManager replay/trait Plans concern a separate runtime file.

## Notes

Planning only: no implementation, build, runtime check or benchmark has been performed. Line numbers are source-review anchors; symbols identify the intended sites if lines move. The language proof and rejected agent boundary above are self-contained and do not depend on temporary audit files.
