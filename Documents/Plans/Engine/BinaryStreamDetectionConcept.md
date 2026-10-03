<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T17:55:09.340Z","dependsOn":[]} -->
# Express binary stream detection as one concept

## Context

`Engine/Source/File/FileManager.h:301-324` computes a Boolean using the primary `has_binary_stream_operators` trait, its `enable_if_t`/`void_t` specialization, and the `has_binary_stream_operators_v` variable template. This is pre-existing clarity debt: three declarations express one expression-validity predicate. The user authorized worthwhile C++20 clarity improvements; this proposal removes that scaffolding without expanding adoption elsewhere.

The only code consumers are `WriteVersionedFile` at line 388 and `ReadVersionedFile` at line 411 in that header. Both require their fallback to `common::Write`/`common::Read`. Searches of Engine, Common, Projects, and DataPacker found no other references or specializations. Known versioned-file consumers include `Engine/Source/Ui/GameSettings.cpp`, `GraphicsSettings.cpp`, `SoundSettings.cpp`, and `Engine/Source/Network/Client/ClientSessionRuntime.cpp:ClientGuidFile`.

## Design

The recommended replacement is one namespace-scope `template <typename T> concept HasBinaryStreamOperators`, in the same location as the removed declarations. This directly names the existing Boolean question and preserves lookup context. Its expression is the following four predicates, in their current order, conjoined with a requires-expression:

```cpp
!std::is_arithmetic_v<T> &&
!std::is_pointer_v<T> &&
!std::is_same_v<std::decay_t<T>, std::string> &&
!std::is_same_v<std::decay_t<T>, std::string_view> &&
requires
{
	std::declval<std::ostream&>() << std::declval<const T&>();
	std::declval<std::istream&>() >> std::declval<T&>();
}
```

Replace the two `if constexpr` predicates with `HasBinaryStreamOperators<STRUCT_TYPE>` and delete the trait and variable template. Keep the surrounding templates unconstrained. Preserve the exact operands, constness, exclusions, and declaration location; add no return-type, conversion, or `noexcept` requirements.

The language basis is [simple requirements](https://eel.is/c++draft/expr.prim.req.simple) and [requires-expression substitution](https://eel.is/c++draft/expr.prim.req.general): simple requirements ask whether these unevaluated expressions are valid, and substitution failure yields false. This is the same validity question as the existing `void_t<decltype(...)>` specialization. Existing exclusions and the requirement that both operators exist remain unchanged.

Recommend removing the introductory detection-summary comment and retaining only the concise rationale `// Exclude text formatters from binary stream detection.` above the concept. The concept expresses detection and the excluded types directly; the remaining comment explains why.

## Risk and invariants

Tier 1: local behavior-preserving compile-time simplification, with no public function signature or invariant change. The predicate guards persistence, so equivalence is an acceptance condition, not permission to change serialization. Both branch bodies, their selection for each type, file headers, versions, error handling, and fallback behavior remain identical. No layout, save/replay format, wire, trust, CRC, simulation, affinity, threading, or allocation change is proposed. No version bump is appropriate under `Engine/Source/File/AGENTS.md` `## File Contracts` because no on-disk layout changes.

Both forms feed the same `if constexpr`; the replacement adds no runtime work, allocation, dispatch, synchronization, or data movement. No speedup is claimed.

## Critical files

- `Engine/Source/File/FileManager.h:has_binary_stream_operators`, `has_binary_stream_operators_v`, `WriteVersionedFile`, and `ReadVersionedFile` — the entire edit boundary.
- `Engine/Source/Ui/GameSettings.cpp`, `GraphicsSettings.cpp`, `SoundSettings.cpp`, and `Engine/Source/Network/Client/ClientSessionRuntime.cpp:ClientGuidFile` — existing consumers to inspect and compile, not edit.

## In scope

- Replace the three stream-detection declarations in FileManager.h with the concept specified above.
- Update the two existing `if constexpr` conditions and the immediately preceding detection comments only.

## Out of scope

- Changing stream overloads, read/write signatures, branch bodies, fallback behavior, headers, format versions, or persisted bytes.
- Constraining the versioned-file templates themselves or broadening/tightening the predicate.
- Converting other traits/templates, adding a generic detection utility, changing includes or project membership, or adding unit tests.
- Changes to AGENTS.md or the C++ style guide.

## Acceptance criteria

1. The old trait and variable template have no remaining code declarations, uses, or specializations; exactly the two existing selectors use the new concept.
2. Diff inspection confirms the four exclusions and two operand expressions are unchanged and no stronger requirements were introduced. The concept occupies the same namespace and declaration position.
3. Both serialization branch bodies and surrounding file behavior remain byte-for-byte unchanged; no format or version edits occur.
4. The affected client and server targets compile successfully through `/compile`, including their shared FileManager header and existing client persistence consumers.

## Verification

During implementation, search all source for both old identifiers and the new concept, inspect the bounded diff against each equivalence criterion, and build BrokenEngineSandbox client and server Debug|x64 through `/compile`. Apply the normal C++ correctness, style, comment, affected-code, and documentation review routes. No runtime harness run or benchmark is necessary for this compile-time-only replacement: unchanged branch bodies and an equivalent selector settle runtime behavior and performance. If equivalence cannot be shown, return the mismatch instead of changing persistence semantics. Do not add unit tests.

## Documentation and style

No AGENTS.md invariant changes or style-guide update is warranted. `Documents/C++StyleGuide.txt` rules 7 and 19 govern PascalCase naming and `typename`/uppercase template parameters; rule 15 needs no new `auto` exception. Rule 64 governs the short rationale comment above. The future `/update-claude-docs` review should confirm a no-change result because the File contract is unchanged.

## Coordination

No dependencies or mandatory coordination constraints. A search of current Plans for FileManager, binary-stream detection, SFINAE, and concepts found no competing implementation boundary.

## Notes

This Plan records a source-inspected clarity improvement only. No implementation, build, runtime verification, or performance measurement has been performed during planning.
