#include <codeanalysis/warnings.h>
#include <cstdint>
#include <source_location>
#include <string_view>
#include "../../../../Common/ErrorUtils.h"

#pragma warning(push, 0)
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_JSON
#define JSON_ASSERT(x) ASSERT(static_cast<bool>(x))

#include "json/single_include/nlohmann/json.hpp"
#include "tinygltf/attic/tiny_gltf.h"

#pragma warning(pop)
