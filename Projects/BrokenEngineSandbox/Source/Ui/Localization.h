#pragma once

#include "Ui/LocalizationBase.h"

namespace game
{

// The engine owns the language vocabulary, standard strings, and their initialization.
using engine::Language;
using engine::LanguageOption;
using engine::StandardString;
using engine::TranslatedString;
using engine::InitializeLocalization;
using engine::geLanguage;
using engine::kiLanguageCount;
using engine::kLanguageOptions;

} // namespace game
