#pragma once

namespace common
{

// The SizeInBytes switch defines supported VkFormats; unsupported formats assert. Callers supply
// positive dimensions whose byte-size calculation fits int64_t; runtime dimensions come from
// Vulkan-bounded extents.
int64_t SizeInBytes(VkFormat vkFormat, int64_t iWidth, int64_t iHeight);

// Total byte size of a full mip chain across all array layers and depth slices: sums SizeInBytes per mip with each dimension halved and clamped at 1 (never 0).
// Single source of truth for texture staging / readback / cache size math so the size computation and the data that fills it cannot drift. Parameters mirror Vulkan image extent / mip / layer counts.
int64_t ComputeImageByteSize(VkFormat vkFormat, int64_t iWidth, int64_t iHeight, int64_t iMipLevels, int64_t iArrayLayers, int64_t iDepth);

} // namespace common
