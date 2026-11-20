#include <codeanalysis/warnings.h>
#include <cstdint>
#include <source_location>
#include <string_view>
#include "../../../../Common/ErrorUtils.h"

#pragma warning(push, 0)
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)

// stb
// Filenames passed to stbi_load* are interpreted as UTF-8 and opened via _wfopen
#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) ASSERT(x)
#include "stb/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_ASSERT(x) ASSERT(x)
#include "stb/stb_image_resize2.h"

// STB_IMAGE_WRITE_IMPLEMENTATION lives in Engine/Stb.cpp — same ThirdParty.lib, defining it here too triggers LNK4006

#pragma warning(pop)
