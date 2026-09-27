#pragma once

#include "Ui/LocalizationBase.h"

namespace game
{

// The engine owns the language vocabulary, the standard strings, and their initialization. These re-exports let game code
// name them without the engine:: qualifier; a game-owned string table is added only once a game string exists.
using engine::Language;
using engine::LanguageOption;
using engine::StandardString;
using engine::TranslatedString;
using engine::InitializeLocalization;
using engine::geLanguage;
using engine::kLanguageCount;
using engine::kLanguageOptions;

} // namespace game
