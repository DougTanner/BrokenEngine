#include <cstdint>
#include <source_location>
#include <string_view>
#include "../../../../Common/ErrorUtils.h"

#pragma warning(push, 0)
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)

#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1002000 // 1.2.0
#define VMA_ASSERT(expr) ASSERT(expr)
#include <vma/vk_mem_alloc.h>

#pragma warning(pop)
