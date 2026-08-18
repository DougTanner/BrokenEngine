<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T17:51:38.755Z","dependsOn":[]} -->
# D7: Split engine standard localization from the game string extension

## Context

Implementation baseline: 9428bde972a63560667458798e368fd74d0d5982 (Update world-grid ownership investigation).

Projects/BrokenEngineSandbox/Source/Ui/Localization.h currently owns every localization concern in game: language values, selected-language state, language labels, all 19 standard strings, the UTF-32 table, fallback, locale setup, and uppercasing. Engine-owned menu helpers already consume UTF-32 views but do not own the common localization vocabulary.

The current string enum has an unused kBaseStringsCount seam. This plan deliberately does not use a shared numeric index space or a game offset; the selected design uses separate strong engine and game types.

GameSettings in Projects/BrokenEngineSandbox/Source/ClientSettings.cpp persists language as an int32_t. Its field order, version (2), filename (GameSettings.bin), padding, and serialized width are load-bearing and remain unchanged.

## Design

### Engine ownership

Add Engine/Source/Ui/LocalizationBase.h and Engine/Source/Ui/LocalizationBase.cpp. The unique LocalizationBase name avoids shadowing the game facade's Ui/Localization.h include path.

The engine owns the six-language vocabulary, typed language options, selected-language state, standard strings, standard fallback, and standard initialization. The public header interface is:

~~~cpp
namespace engine
{

enum class Language : int32_t
{
	kEnglish = 0,
	kChinese = 1,
	kSpanish = 2,
	kPortuguese = 3,
	kFrench = 4,
	kGerman = 5,
};

inline constexpr int64_t kLanguageCount = 6;

struct LanguageOption
{
	Language eLanguage;
	const char* pcLabel;
};

inline constexpr LanguageOption kLanguageOptions[]
{
	{.eLanguage = Language::kEnglish, .pcLabel = "ENGLISH"},
	{.eLanguage = Language::kChinese, .pcLabel = "中文"},
	{.eLanguage = Language::kSpanish, .pcLabel = "ESPANOL"},
	{.eLanguage = Language::kPortuguese, .pcLabel = "PORTUGUES"},
	{.eLanguage = Language::kFrench, .pcLabel = "FRANCAIS"},
	{.eLanguage = Language::kGerman, .pcLabel = "DEUTSCH"},
};

static_assert(std::size(kLanguageOptions) == static_cast<size_t>(kLanguageCount));

inline Language geLanguage = Language::kEnglish;

enum class StandardString : uint8_t
{
	kStringComplete,
	kStringDefaults,
	kStringGameOver,
	kStringGamepad,
	kStringGameSettings,
	kStringMainMenu,
	kStringMouse,
	kStringMoveWith,
	kStringGraphics,
	kStringOr,
	kStringLocalServer,
	kStringRemoteServer,
	kStringStart,
	kStringReady,
	kStringRespawn,
	kStringResume,
	kStringAudio,
	kStringQuit,
	kStringPaused,

	kCount,
};

std::u32string_view TranslatedString(StandardString eString);
void InitializeLocalization();

} // namespace engine
~~~

Language values are explicitly pinned to 0..5; kLanguageCount is a separate int64_t constant, not an enum member. LanguageOption and kLanguageOptions are the engine-owned language selection records and retain the existing six labels exactly. geLanguage remains mutable runtime state owned by the engine.

LocalizationBase.cpp defines an internal gppTranslatedStrings[][kLanguageCount][256] standard table with a deduced outer extent. Copy the 19 existing rows from Projects/BrokenEngineSandbox/Source/Ui/Localization.h, including every translation and empty cell, without changing text or order, followed by one all-empty sentinel row. Keep:

~~~cpp
static_assert(std::size(gppTranslatedStrings) == static_cast<size_t>(StandardString::kCount) + 1);
~~~

engine::TranslatedString(StandardString) indexes by the strongly typed enum, returns the selected-language cell when non-empty, and falls back to English for an empty cell. engine::InitializeLocalization() retains std::setlocale(LC_ALL, "en_US.utf8") and the existing in-place towupper pass over standard rows only.

Do not add an unscoped compatibility enum or namespace-level compatibility aliases for Language or StandardString.

### Game facade and extension

Rewrite Projects/BrokenEngineSandbox/Source/Ui/Localization.h to include Ui/LocalizationBase.h, re-export the engine types and enumerators, and own only the game extension:

~~~cpp
namespace game
{

using engine::Language;
using enum engine::Language;
using engine::LanguageOption;
using engine::StandardString;
using enum engine::StandardString;
using engine::TranslatedString;
using engine::geLanguage;
using engine::kLanguageCount;
using engine::kLanguageOptions;

enum class Strings : uint8_t
{
	kStringsCount = 0,
};

std::u32string_view TranslatedString(Strings eString);
void InitializeLocalization();

} // namespace game
~~~

using enum engine::Language keeps kEnglish through kGerman available to current game code, and using enum engine::StandardString keeps all existing kString... calls unqualified. using engine::TranslatedString preserves the engine overload while allowing the game overload for Strings. The facade must not re-export an engine initializer under the same name; the game initializer remains the construction entry point.

Add Projects/BrokenEngineSandbox/Source/Ui/Localization.cpp. It defines an independent zero-based game table with only the sentinel row initially:

~~~cpp
char32_t gppTranslatedStrings[][kLanguageCount][256]
{
	{
		U"", U"", U"", U"", U"", U"",
	},
};

static_assert(std::size(gppTranslatedStrings) == static_cast<size_t>(Strings::kStringsCount) + 1);
~~~

game::TranslatedString(Strings) performs the same selected-language lookup and English fallback against the game table. game::InitializeLocalization() calls engine::InitializeLocalization() first, then applies the existing towupper loop to game rows. The sentinel-only table produces zero game-row iterations now but leaves the game extension independent and ready for later rows.

The engine and game tables each retain their own deduced outer extent and sentinel assertion. The game table does not depend on an engine string count or numeric offset.

### Existing callers and settings

Keep the existing Game::Game() call at Projects/BrokenEngineSandbox/Source/Game.cpp:41 as the call to game::InitializeLocalization(). It must reach the game facade, which initializes the engine table before the game table.

In Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:

- Keep the GameSettings field order, field types, padding, and trivially-copyable assertion unchanged.
- Keep kiVersion = 2 and kpcGameSettingsPath = "GameSettings.bin" unchanged.
- Because Language becomes a scoped enum, initialize iLanguage with static_cast<int32_t>(kEnglish); this does not alter the POD layout or serialized bytes.
- Keep saving static_cast<int32_t>(geLanguage).
- Keep invalid persisted-language validation local at the opaque file boundary in LoadGameSettings():

  ~~~cpp
  geLanguage = (gameSettings.iLanguage >= 0 && gameSettings.iLanguage < kLanguageCount)
      ? static_cast<Language>(gameSettings.iLanguage)
      : kEnglish;
  ~~~

- Do not add or export a public LanguageFromPersisted helper.
- Keep reset behavior assigning kEnglish.

In Projects/BrokenEngineSandbox/Source/Ui/Screens/GameSettingsScreen.cpp, remove the local kpcLanguageNames array and its local count assertion. Use kLanguageOptions[i].pcLabel for measurement and button labels, and kLanguageOptions[i].eLanguage for comparison and assignment. Preserve all language-row geometry, button behavior, and current menu/workbuffer calls.

Projects/BrokenEngineSandbox/Source/Ui/Screens/GameSettingsScreen.h:16 remains an explicit consumer of the exported count:

~~~cpp
float mfLanguageHoverAnims[kLanguageCount] {};
~~~

All other existing standard-string callers continue including Ui/Localization.h and calling TranslatedString(kString...) unqualified. No mechanical qualification pass is needed. Engine/Source/Ui/MenuUtils.h and .cpp remain unchanged.

## Critical files

- Engine/Source/Ui/LocalizationBase.h — new engine language, option, selected-state, and standard-string interface.
- Engine/Source/Ui/LocalizationBase.cpp — new engine standard table, fallback, and uppercase initialization.
- Projects/BrokenEngineSandbox/Source/Ui/Localization.h — game facade and independent typed Strings declaration.
- Projects/BrokenEngineSandbox/Source/Ui/Localization.cpp — new game sentinel table, fallback overload, and game initializer.
- Projects/BrokenEngineSandbox/Source/ClientSettings.cpp — local persisted-language range guard; preserve settings layout and file identity.
- Projects/BrokenEngineSandbox/Source/Ui/Screens/GameSettingsScreen.h — exported kLanguageCount hover-array consumer.
- Projects/BrokenEngineSandbox/Source/Ui/Screens/GameSettingsScreen.cpp — typed engine option consumer.
- Projects/BrokenEngineSandbox/Source/Game.cpp — verify the existing constructor initializer call remains the game entry point.
- Projects/BrokenEngineSandbox/Source/Ui/Screens/DeathMenuScreen.cpp
- Projects/BrokenEngineSandbox/Source/Ui/Screens/GraphicsMenuScreen.cpp
- Projects/BrokenEngineSandbox/Source/Ui/Screens/MainMenuScreen.cpp
- Projects/BrokenEngineSandbox/Source/Ui/Screens/PauseMenuScreen.cpp
- Projects/BrokenEngineSandbox/Source/Ui/Screens/SoundMenuScreen.cpp — verify existing unqualified standard-string calls resolve through the facade; no expected semantic edits.
- Engine/Source/Ui/AGENTS.md — document engine localization ownership, strong types, and both-build affinity.
- Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md — replace the stale single header-only table description with the engine-standard/game-extension contract.
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters — register both new localization implementations and headers in both targets and matching filters.

The sibling CLAUDE.md files remain import stubs and need no independent content.

## In scope

- Add Engine/Source/Ui/LocalizationBase.h/.cpp with the exact strong engine interface above.
- Move the six language values, kLanguageCount, typed options, geLanguage, and all 19 existing standard rows from the game implementation into the engine.
- Preserve standard row order, every language cell, empty fallback cells, fixed [kLanguageCount][256] inner extent, and one trailing sentinel.
- Add Projects/BrokenEngineSandbox/Source/Ui/Localization.cpp and convert the game header into the using enum facade plus independent zero-based enum class Strings : uint8_t.
- Preserve overload resolution for existing standard-string calls and provide the game Strings overload.
- Keep game::InitializeLocalization() as the constructor entry point and delegate to the engine initializer first.
- Keep invalid persisted-language validation local to ClientSettings.cpp; do not expose a helper.
- Preserve GameSettings field order, types, padding, default semantics, version, filename, and serialized width.
- Move the language option source from GameSettingsScreen.cpp to engine kLanguageOptions while preserving labels and UI behavior.
- Keep GameSettingsScreen.h bound to the exported kLanguageCount.
- Register both new .cpp files and both new headers in both client and server projects and filters.
- Update the two affected UI ownership documents and run the required affected-code, project-membership, style, and documentation checks during implementation.

## Out of scope

- Any unscoped compatibility enum or namespace-level compatibility alias for Language or StandardString.
- Any public LanguageFromPersisted helper.
- Any new translation, changed translation text, changed language label, or added language.
- Any game string beyond the sentinel row.
- Any shared string index, base count, or numeric offset between engine and game tables.
- Changes to Engine/Source/Ui/MenuUtils.h, MenuUtils.cpp, UTF-32/UTF-8 conversion, workbuffer allocation, button sizing, or menu chrome.
- Moving or rewriting screens, changing screen ownership, or introducing RmlUi.
- Changes to Engine.h, PCH include order, network packets, frame state, CRC state, save/replay formats, or deterministic simulation.
- Changes to GameSettings fields, field order, padding, version, filename, file flags, or serialized representation.
- Compatibility shims for the removed kBaseStringsCount/shared-index design.
- Unit tests or a new test framework.
- Unrelated localization, settings, documentation, or project cleanup.

## Risk tier and invariants

Tier 3 applies because the change crosses the engine/game ownership boundary, changes public strongly typed C++ interfaces, changes persisted settings interpretation, and changes client/server project membership. The localization tables are outside CRC/network state, but persisted language values and overload contracts are load-bearing.

Invariants:

1. Language remains enum class Language : int32_t; Language::kEnglish through Language::kGerman remain exactly 0..5.
2. kLanguageCount remains inline constexpr int64_t with value 6.
3. kLanguageOptions contains exactly one LanguageOption for each language, in enum order, with the six existing labels unchanged.
4. geLanguage remains engine-owned and defaults to Language::kEnglish.
5. Invalid persisted values are rejected at the ClientSettings.cpp file trust boundary; only values in [0, kLanguageCount) are cast to Language, and all others select English.
6. StandardString remains enum class StandardString : uint8_t; its 19 existing enumerators retain order and kCount is the sentinel index.
7. The engine standard table contains exactly 19 rows plus one sentinel, with deduced outer extent and fixed inner dimensions.
8. Strings remains an independent enum class Strings : uint8_t; Strings::kStringsCount == 0 initially and the game table contains one sentinel row.
9. An empty selected-language cell falls back to the English cell in both tables.
10. Locale setup and in-place towupper behavior remain in the same order: game initializer → engine initializer → game-table pass.
11. Both localization .cpp implementations compile and link in both client and server projects, without client/server-only whole-file guards.
12. Existing standard-string calls compile through using enum engine::StandardString and using engine::TranslatedString; future game strings use the distinct overload.
13. GameSettings keeps its current field order, types, padding, version, filename, and serialized width.
14. MenuUtils and its workbuffer lifetime contract remain byte-unchanged.
15. No main-loop allocation path is introduced; localization table setup remains startup initialization.

## Coordination, documentation, and project membership

Implementation is one localization slice followed by /update-affected-code. The implementer owns the source, facade, settings, screen, and documentation changes and reports all callers checked. A builder runs the client and server compile targets. A mechanic runs /update-vcxproj for both project/filter pairs and /code-style-review for changed C++. A fresh implementer pass runs /update-claude-docs. Tier-3 review includes plan audit, simplicity review, external grill, C++ correctness review, scope review, and adversarial review as routed by the Change Workflow.

The engine and game localization .cpp files are shared startup code, not client-only UI code. Add each new .cpp and header to both .vcxproj files and corresponding .filters files. Engine files use Engine\Ui; game files use Game\Ui. Existing game Localization.h entries remain present in both projects.

Do not add LocalizationBase.h to Engine.h; consumers include the direct unique header through the game facade or directly where needed. Do not modify generated data, DataPacker inputs, or RmlUi feature documents.

## Acceptance criteria

1. **Strong engine language contract**
   - Check: inspect LocalizationBase.h, compile both targets, and run the existing language-row UI check.
   - Expected: Language is enum class : int32_t with values exactly 0..5; kLanguageCount is inline constexpr int64_t == 6; LanguageOption and kLanguageOptions are strongly typed and contain the six unchanged labels.
   - Independent signal: the client build exercises typed option use and GameSettingsScreen.h array sizing; the header static assertion independently checks option count.

2. **Strong standard-string contract and facade resolution**
   - Check: inspect StandardString, the two using enum declarations, using engine::TranslatedString, and compile all existing unqualified standard-string callers.
   - Expected: StandardString is enum class : uint8_t; all 19 existing enumerators remain ordered; no unscoped compatibility enum or public conversion helper exists; standard calls resolve to the engine overload.
   - Independent signal: compiler overload resolution and link success independently prove the imported standard overload is callable.

3. **Independent game extension**
   - Check: inspect enum class Strings : uint8_t, the game table/assertion, and the game overload; compile both targets.
   - Expected: Strings::kStringsCount == 0 initially, the game table has one sentinel, and no engine count or numeric offset is used.
   - Independent signal: the game implementation's static assertion proves its table extent independently of the engine assertion.

4. **Table integrity and behavior**
   - Check: compare the moved rows against the baseline Localization.h; compile both deduced-extent assertions; run client UI and server startup through the existing harness path.
   - Expected: 19 standard rows plus one sentinel are unchanged; game has sentinel-only table; initialization calls engine first; locale setup, uppercasing, and empty-cell English fallback remain effective.
   - Independent signal: source/table comparison proves content, static assertions prove extents, client UI observes labels, and server startup exercises the shared implementation without graphics.

5. **Persisted settings compatibility**
   - Check: compare GameSettings against the baseline declaration and inspect the save/load diff; compile the local range guard.
   - Expected: field order, types, padding, defaults, version, filename, and serialized width are unchanged; saving still writes the same int32_t; invalid values map to English locally at the opaque file boundary; no public LanguageFromPersisted symbol exists.
   - Independent signal: baseline declaration comparison settles layout/file identity, while the local guard source settles invalid-input behavior.

6. **Menu and scope preservation**
   - Check: inspect the session diff and run scope review.
   - Expected: MenuUtils.{h,cpp}, conversion/workbuffer behavior, RmlUi files, and translation content are untouched; only the language-option source moves out of GameSettingsScreen.cpp.
   - Independent signal: diff path/content review is separate from C++ compilation.

7. **Client/server project coverage**
   - Check: /update-vcxproj validation plus client Debug and server Debug builds with the repository's normal RunDataPacker=false agent settings.
   - Expected: both localization .cpp files compile and link in both executables, and all four project/filter files contain matching membership.
   - Independent signal: XML membership validation and the two target build receipts independently prove registration and compilation.

8. **Documentation coherence**
   - Check: /update-claude-docs and direct inspection of the two affected AGENTS.md files.
   - Expected: engine and game ownership, strong types, initialization order, sentinel rules, local settings validation, and both-build affinity are documented; no stale single-table/header-only statement remains.
   - Independent signal: documentation synchronization is separate from source/build checks.

No unit tests are added.

## Implementation order

1. Add the engine base header/source with strong language and standard types, typed options, selected state, standard table, fallback, and initialization.
2. Rewrite the game facade with using enum imports and add the game sentinel implementation.
3. Update the local persisted-language guard and the game-settings language-option consumer.
4. Register both implementations in both projects and filters.
5. Update affected UI ownership documentation.
6. Run propagation, membership/style checks, client/server builds, and the required review and acceptance workflow.
