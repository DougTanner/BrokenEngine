#if defined(BT_CLIENT)

#include "TextureManager.h"

#include "File/PackChunks.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/PbrWrappersBase.h"
#include "Ui/WaterWrappersBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

// Extra texture-descriptor slots reserved for pre-blurred lighting texture copies (one per registered lighting texture CRC)
constexpr int64_t kiLightingBlurSlots = 16;

std::tuple<int64_t, int64_t> TextureManager::DetailTextureSize(float fMultiplier)
{
	auto [iWorldDetailX, iWorldDetailY] = FullDetail();

	int64_t iX = static_cast<int64_t>(fMultiplier * static_cast<float>(iWorldDetailX));
	int64_t iY = static_cast<int64_t>(fMultiplier * static_cast<float>(iWorldDetailY));

	iX = std::max(iX, 128i64);
	iY = std::max(iY, 64i64);

	iX = std::min(iX, static_cast<int64_t>(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxImageDimension2D));
	iY = std::min(iY, static_cast<int64_t>(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxImageDimension2D));

	return std::make_tuple(iX, iY);
}

std::tuple<int64_t, int64_t> TextureManager::LightingDetailTextureSize(float fMultiplier)
{
	// Pre-size every lighting deposit/spread/combine texture by Camera::kfLightingHeadroomMultiplier so the constant
	// on-screen-pixel-size texel grid retains coverage margin while its camera-height reference expands immediately
	// outward and contracts gradually inward. Centralized so all lighting consumers stay byte-consistent
	// (deposit quads must land on the same texels the area math snaps to). Clamp AFTER the multiply (DetailTextureSize
	// clamps pre-multiply); force width even so downstream half-width math stays integer.
	auto [iBaseX, iBaseY] = DetailTextureSize(fMultiplier);
	int64_t iLimit = static_cast<int64_t>(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxImageDimension2D);
	int64_t iX = std::min(static_cast<int64_t>(std::lround(static_cast<float>(iBaseX) * engine::Camera::kfLightingHeadroomMultiplier)), iLimit);
	iX &= ~1ll;
	int64_t iY = std::min(static_cast<int64_t>(std::lround(static_cast<float>(iBaseY) * engine::Camera::kfLightingHeadroomMultiplier)), iLimit);
	return std::make_tuple(iX, iY);
}

std::tuple<int64_t, int64_t> TextureManager::WaterDetailTextureSize(float fMultiplier)
{
	auto [iWorldDetailX, iWorldDetailY] = WaterFullDetail();

	int64_t iX = static_cast<int64_t>(fMultiplier * static_cast<float>(iWorldDetailX));
	int64_t iY = static_cast<int64_t>(fMultiplier * static_cast<float>(iWorldDetailY));

	iX = std::max(iX, 128i64);
	iY = std::max(iY, 64i64);

	iX = std::min(iX, static_cast<int64_t>(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxImageDimension2D));
	iY = std::min(iY, static_cast<int64_t>(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxImageDimension2D));

	return std::make_tuple(iX, iY);
}

float TextureManager::DetailTextureAspectRatio()
{
	auto [iWorldDetailX, iWorldDetailY] = FullDetail();
	return static_cast<float>(iWorldDetailX) / static_cast<float>(iWorldDetailY);
}

void TextureManager::CreatePlaceholderTexture(Texture& rTexture, std::string_view name, VkImageCreateFlags vkImageCreateFlags, VkFormat vkFormat, uint32_t uiArrayLayers, VkImageViewType vkImageViewType, const std::function<void(std::span<std::byte>, int64_t)>& rPixelWriter)
{
	rTexture.Create(
	{
		.textureFlags = {},
		.name = name,
		.vkImageCreateFlags = vkImageCreateFlags,
		.vkFormat = vkFormat,
		.vkExtent3D = VkExtent3D {.width = 1, .height = 1, .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = uiArrayLayers,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.vkImageViewType = vkImageViewType,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	}, rPixelWriter);
}

TextureManager::TextureManager()
: mTextureDescriptors(*this)
{
	ASSERT(gpTextureManager == nullptr);

	gpTextureManager = this;

	ScopedBootTimer scopedBootTimer(kBootTimerTextureManager);

	CreateSamplers();

	mRenderTargetTextures.Create();

	gpProfileManager->BootStart(kBootTimerTextureUpload);

	// Create 1x1 white placeholder textures for deferred texture loading
	CreatePlaceholderTexture(mWhiteTexture, "WhitePlaceholder", 0, VK_FORMAT_R8G8B8A8_UNORM, 1, VK_IMAGE_VIEW_TYPE_2D, [](std::span<std::byte> data, [[maybe_unused]] int64_t iPosition)
	{
		*reinterpret_cast<uint32_t*>(data.data()) = 0xFFFFFFFF;
	});

	CreatePlaceholderTexture(mWhiteCubeTexture, "WhiteCubePlaceholder", VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_FORMAT_R8G8B8A8_UNORM, 6, VK_IMAGE_VIEW_TYPE_CUBE, [](std::span<std::byte> data, [[maybe_unused]] int64_t iPosition)
	{
		uint32_t* pPixels = reinterpret_cast<uint32_t*>(data.data());
		for (int64_t i = 0; i < 6; ++i)
		{
			pPixels[i] = 0xFFFFFFFF;
		}
	});

	// Slot-0 island placeholders. Format-matched to the bindless arrays; values chosen so
	// sampling slot 0 has no visible effect (ocean-bottom elevation submerged below the water,
	// mid-gray color, up-vector normals, full-bright AO).
	CreatePlaceholderTexture(mIslandPlaceholderElevation, "IslandPlaceholderElevation", 0, shaders::kVkFormatElevation, 1, VK_IMAGE_VIEW_TYPE_2D, [](std::span<std::byte> data, [[maybe_unused]] int64_t iPosition)
	{
		// Ocean-bottom, matching the elevation RTT clear (RenderTargetTextures.cpp) and the
		// open-ocean CPU floor (IslandTerrain::mfSeaFloorElevation). Island slots that are not yet
		// GPU-resident (startup, mid-load before RestorationSweep, evicted-slot grace window) alias
		// this placeholder; ocean-bottom keeps their footprint submerged under the water instead of
		// rendering a sea-level plane that pokes through the surface.
		*reinterpret_cast<uint16_t*>(data.data()) = DirectX::PackedVector::XMConvertFloatToHalf(gpIslandTerrain->mfSeaFloorElevation);
	});

	CreatePlaceholderTexture(mIslandPlaceholderColor, "IslandPlaceholderColor", 0, VK_FORMAT_R8G8B8A8_UNORM, 1, VK_IMAGE_VIEW_TYPE_2D, [](std::span<std::byte> data, [[maybe_unused]] int64_t iPosition)
	{
		*reinterpret_cast<uint32_t*>(data.data()) = 0xFF808080u;
	});

	CreatePlaceholderTexture(mIslandPlaceholderNormals, "IslandPlaceholderNormals", 0, VK_FORMAT_R8G8_UNORM, 1, VK_IMAGE_VIEW_TYPE_2D, [](std::span<std::byte> data, [[maybe_unused]] int64_t iPosition)
	{
		*reinterpret_cast<uint16_t*>(data.data()) = 0x8080u;
	});

	CreatePlaceholderTexture(mIslandPlaceholderAmbientOcclusion, "IslandPlaceholderAmbientOcclusion", 0, VK_FORMAT_R8_UNORM, 1, VK_IMAGE_VIEW_TYPE_2D, [](std::span<std::byte> data, [[maybe_unused]] int64_t iPosition)
	{
		*reinterpret_cast<uint8_t*>(data.data()) = 0xFFu;
	});

	// All-zero RGBA disables rock, sand, snow, and flow until the real BC7 mask chunk adopts.
	// Bindless slots permit different formats: this placeholder uses R8G8B8A8, while island masks use BC7.
	CreatePlaceholderTexture(mIslandPlaceholderMasks, "IslandPlaceholderMasks", 0, VK_FORMAT_R8G8B8A8_UNORM, 1, VK_IMAGE_VIEW_TYPE_2D, [](std::span<std::byte> data, [[maybe_unused]] int64_t iPosition)
	{
		*reinterpret_cast<uint32_t*>(data.data()) = 0x00000000u;
	});

	// Create deferred textures from ChunkHeader metadata for all texture chunks (real GPU resources allocated when data arrives)
	for (const auto& [rCrc, rLazyChunk] : gpFileManager->mpPackChunks->mLazyChunkMap)
	{
		if (!(rLazyChunk.header.flags & common::ChunkFlags::kTexture))
		{
			continue;
		}

		bool bCubemap = rLazyChunk.header.flags & common::ChunkFlags::kCubemap;

		// Store metadata and point at white placeholder (no GPU allocation until data arrives)
		auto [it, bInserted] = mTextureMap.try_emplace(rCrc);
		ASSERT(bInserted);
		it->second.InitDeferred(TextureInfo
		{
			.textureFlags = {},
			.name = rLazyChunk.header.pcPath,
			.uiCrc = rCrc,
			.vkImageCreateFlags = bCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : static_cast<VkImageCreateFlags>(0),
			.vkFormat = rLazyChunk.header.textureHeader.vkFormat,
			.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(rLazyChunk.header.textureHeader.iTextureWidth), .height = static_cast<uint32_t>(rLazyChunk.header.textureHeader.iTextureHeight), .depth = 1},
			.uiMipLevels = static_cast<uint32_t>(rLazyChunk.header.textureHeader.iMipLevels),
			.uiArrayLayers = bCubemap ? 6u : 1u,
			.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
			.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			.vkImageViewType = bCubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
			.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
			.eTextureLayout = TextureLayout::kShaderReadOnly,
		}, bCubemap ? mWhiteCubeTexture.mVkImageView : mWhiteTexture.mVkImageView);

		// Water normal maps: copy the DataPacker-baked per-mip Toksvig variance table (already
		// padded past the real mip chain with the last value) for the WATER_SPEC_AA_MIP_HANDOFF
		// uniform upload. Header-resident so no lazy chunk data is needed at startup.
		static_assert(shaders::kiWaterSpecularAntialiasingMipTableSize == common::TextureHeader::kiMipVarianceCount, "The shader-side mip-variance table length must match the pack format's");
		for (int64_t i = 0; i < kiWaterNormalCount; ++i)
		{
			if (kpWaterNormalCrcs[i] == rCrc)
			{
				std::memcpy(mpfWaterNormalMipVariance[i], rLazyChunk.header.textureHeader.pfMipVariance, sizeof(mpfWaterNormalMipVariance[i]));
				break;
			}
		}
	}

	// Pre-fill texture arrays with white placeholders for lazy index assignment
	// Extra slots reserved for pre-blurred lighting texture copies
	mTextureDescriptors.mImageInfos.resize(mTextureMap.size() + kiLightingBlurSlots, {.sampler = nullptr, .imageView = mWhiteTexture.mVkImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

	mTextureDescriptors.Create();

	// Initialize island texture pointers sized to kiMaxIslands for shader descriptor arrays.
	// .data() pointer-stability across this manager's lifetime is load-bearing: each array's .data()
	// pointer is the map key in TextureDescriptors::mBindlessArrayConsumers. Do not re-resize these
	// vectors after this point — a re-resize would dangle the registry keys.
	mRenderTargetTextures.mElevationTextures.resize(shaders::kiMaxIslands);
	mRenderTargetTextures.mColorTextures.resize(shaders::kiMaxIslands);
	mRenderTargetTextures.mNormalsTextures.resize(shaders::kiMaxIslands);
	mRenderTargetTextures.mAmbientOcclusionTextures.resize(shaders::kiMaxIslands);
	mRenderTargetTextures.mMasksTextures.resize(shaders::kiMaxIslands);

	// Device-loss recovery resets per-template slot residency so AcquireTextureSlot re-registers all five channels; elevation stays on its
	// placeholder until the four chunk-backed channels are ready. Without the reset, the initialization fan-out leaves islands on placeholders.
	gpIslandTerrainResidency->ResetTextureSlots();

	// Island textures load dynamically per ClientSession::ApplyReceivedStaticData. TextureDescriptors
	// owns the slot writes; these fixed vectors keep the stable backing addresses it registers.
	mTextureDescriptors.InitializeIslandSlots();

	gpProfileManager->BootStop(kBootTimerTextureUpload);

	// Create per-framebuffer command buffers for batched QFOT acquire barriers (before InitializeBootTextures -> ProcessPendingTextures)
	CreateAcquireCommandBuffers();
}

void TextureManager::InitializeBootTextures()
{
	gpProfileManager->BootStart(kModelTexturesGeneration);

	// Make sure to start the texture upload thread before WaitForTextures because it will wait on texture availability
	gpTextureUploadManager->StartThread();

	// Load pre-baked cubemaps from pack data
	common::crc_t pIblCrcs[] = {kIrradianceCrc, kPrefilteredCrc, kPrefilteredWaterCrc};
	WaitForTextures(pIblCrcs);
	mTextureCache.miPhysicallyBasedRenderingCubeMipmapCount = mTextureMap.at(kPrefilteredCrc).mInfo.uiMipLevels;

	gpProfileManager->BootStop(kModelTexturesGeneration);

	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(kPriorityTextures.pCrcs, LoadPriority::kRealtime);

	// Replay every registered lighting-texture request: registration runs once at startup, after the first Graphics
	// construction, so without this a full recreate would strand these textures on the white placeholder forever (the
	// set is still empty on that first boot, making this a no-op). One batched call rather than a per-CRC
	// per-texture request loop: registration requests at kNormal, and the batch locks and wakes the loader once.
	common::ScopedWorkbufferArena scopedWorkbufferArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (common::crc_t crc : gpTextureUploadManager->mLightingTextureCrcs)
	{
		common::gpThreadLocal->mWorkbuffer.PushBack<common::crc_t>(crc);
	}
	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(common::gpThreadLocal->mWorkbuffer.Span<common::crc_t>(), LoadPriority::kRealtime);
}

TextureManager::~TextureManager()
{
	mTextureDescriptors.Destroy();

	vkDestroyCommandPool(gpDeviceManager->mVkDevice, mAcquireVkCommandPool, nullptr);

	DestroySamplers();
	mRenderTargetTextures.DestroyLightingTextures();

	if (gpTextureManager == this)
	{
		gpTextureManager = nullptr;
	}
}

void TextureManager::DestroyScreenDependentResources()
{
	mTextureDescriptors.Destroy();

	vkDestroyCommandPool(gpDeviceManager->mVkDevice, mAcquireVkCommandPool, nullptr);
	mAcquireVkCommandPool = VK_NULL_HANDLE;
	mAcquireVkCommandBuffers.clear();

	mRenderTargetTextures.DestroyLightingTextures();
}

void TextureManager::CreateScreenDependentResources()
{
	mRenderTargetTextures.Create();

	mTextureDescriptors.Create();

	CreateAcquireCommandBuffers();
}

void TextureManager::CreateAcquireCommandBuffers()
{
	VkCommandPoolCreateInfo vkCommandPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = static_cast<uint32_t>(gpInstanceManager->miGraphicsQueueFamilyIndex),
	};
	CHECK_VK(vkCreateCommandPool(gpDeviceManager->mVkDevice, &vkCommandPoolCreateInfo, nullptr, &mAcquireVkCommandPool));

	uint32_t uiFramebufferCount = static_cast<uint32_t>(gpSwapchainManager->mFramebuffers.size());
	mAcquireVkCommandBuffers.resize(uiFramebufferCount);
	VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = mAcquireVkCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = uiFramebufferCount,
	};
	CHECK_VK(vkAllocateCommandBuffers(gpDeviceManager->mVkDevice, &vkCommandBufferAllocateInfo, mAcquireVkCommandBuffers.data()));
}

void TextureManager::DestroySamplers()
{
	for (VkSampler& rVkSampler : mpSamplersVkSampler)
	{
		vkDestroySampler(gpDeviceManager->mVkDevice, rVkSampler, nullptr);
		rVkSampler = VK_NULL_HANDLE;
	}
}

void TextureManager::CreateSamplers()
{
	if (gMaximumAnisotropy.mfCurrent > gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerAnisotropy)
	{
		gMaximumAnisotropy.Reset(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerAnisotropy);
	}

	if (-gMipmapLevelOfDetailBias.mfCurrent > gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerLodBias)
	{
		gMipmapLevelOfDetailBias.Reset(-gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerLodBias);
	}
	else if (gMipmapLevelOfDetailBias.mfCurrent < -gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerLodBias)
	{
		gMipmapLevelOfDetailBias.Reset(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerLodBias);
	}

	VkSamplerCreateInfo vkSmokeSamplerCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 0.0f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 14.0f,
		.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSmokeSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotSmoke]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotSmoke], "Smoke");

	// Wind sampler: linear filtering for smooth advection + clamp-to-edge preserves energy at boundaries
	vkSmokeSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	vkSmokeSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	vkSmokeSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	vkSmokeSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	vkSmokeSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	vkSmokeSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSmokeSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotWindClamp]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotWindClamp], "WindClamp");

	VkSamplerCreateInfo vkSamplerCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias = -gMipmapLevelOfDetailBias.mfCurrent,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1.0f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 14.0f,
		.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotLinearClamp]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotLinearClamp], "LinearClamp");

	vkSamplerCreateInfo.mipLodBias = -gMipmapLevelOfDetailBias.mfCurrent;
	vkSamplerCreateInfo.anisotropyEnable = gAnisotropy.Get<bool>() ? VK_TRUE : VK_FALSE;
	vkSamplerCreateInfo.maxAnisotropy = gMaximumAnisotropy.mfCurrent;
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotClamp]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotClamp], "Clamp");

	// Dedicated sampler for the per-island R16_SFLOAT heightmap (IslandTerrain bindless elevation array).
	// Mirrors mpSamplersVkSampler[kSamplerSlotClamp]; R16_SFLOAT linear filtering is spec-mandated (16-bit-float family),
	// so this stays LINEAR unconditionally.
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotElevation]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotElevation], "Elevation");

	vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	vkSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	vkSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotBorder]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotBorder], "Border");
	// White border (opaque 1.0): the shadow texture is inverse (1.0 = fully lit / no shadow), so any sample beyond the
	// texture extent reads "no shadow" instead of smearing the edge.
	vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotBorderWhite]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotBorderWhite], "BorderWhite");
	vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	vkSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	vkSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotRepeat]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotRepeat], "Repeat");

	// Model normal and metallic-roughness textures carry data rather than color. Apply their dedicated
	// bias directly so negative sharpens, positive blurs, and zero is unbiased.
	vkSamplerCreateInfo.mipLodBias = std::clamp(gPhysicallyBasedRenderingModelDataMipmapLevelOfDetailBias.mfCurrent, -gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerLodBias, gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerLodBias);
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotRepeatModelData]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotRepeatModelData], "RepeatModelData");

	vkSamplerCreateInfo.mipLodBias = -gMipmapLevelOfDetailBias.mfCurrent;
	vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	vkSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	vkSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotMirroredRepeat]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotMirroredRepeat], "MirroredRepeat");

	// Water-normal variant: its own slider-driven bias instead of the global -gMipmapLevelOfDetailBias sharpen —
	// a sharpen bias tuned for albedo pushes minified normal fetches toward noisier mips (specular
	// shimmer), and Water.frag's WATER_SPEC_AA_MIP_HANDOFF analytic LOD must track the hardware LOD
	// (the slider value is also uploaded as fWaterNormalMipmapBias). Applied directly, not negated:
	// negative = sharpen, positive = blur.
	vkSamplerCreateInfo.mipLodBias = std::clamp(gWaterNormalMipmapBias.mfCurrent, -gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerLodBias, gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxSamplerLodBias);
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotMirroredRepeatWater]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotMirroredRepeatWater], "MirroredRepeatWater");

	vkSamplerCreateInfo.mipLodBias = -gMipmapLevelOfDetailBias.mfCurrent;
	vkSamplerCreateInfo.anisotropyEnable = VK_FALSE;
	vkSamplerCreateInfo.maxAnisotropy = 1.0f;
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotMirroredRepeatLinear]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotMirroredRepeatLinear], "MirroredRepeatLinear");

	vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	vkSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	vkSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	CHECK_VK(vkCreateSampler(gpDeviceManager->mVkDevice, &vkSamplerCreateInfo, nullptr, &mpSamplersVkSampler[kSamplerSlotRepeatLinear]));
	VkName(VK_OBJECT_TYPE_SAMPLER, mpSamplersVkSampler[kSamplerSlotRepeatLinear], "RepeatLinear");
}

VkSampler TextureManager::GetSampler(DescriptorFlags_t flags)
{
	// Sampler flags are mutually exclusive.
	struct FlagToSlotEntry
	{
		DescriptorFlags eFlag;
		SamplerSlot eSlot;
	};
	static constexpr FlagToSlotEntry kFlagToSlot[]
	{
		{.eFlag = DescriptorFlags::kSamplerElevation, .eSlot = kSamplerSlotElevation},
		{.eFlag = DescriptorFlags::kSamplerClamp, .eSlot = kSamplerSlotClamp},
		{.eFlag = DescriptorFlags::kSamplerBorder, .eSlot = kSamplerSlotBorder},
		{.eFlag = DescriptorFlags::kSamplerBorderWhite, .eSlot = kSamplerSlotBorderWhite},
		{.eFlag = DescriptorFlags::kSamplerRepeat, .eSlot = kSamplerSlotRepeat},
		{.eFlag = DescriptorFlags::kSamplerMirroredRepeat, .eSlot = kSamplerSlotMirroredRepeat},
		{.eFlag = DescriptorFlags::kSamplerMirroredRepeatWater, .eSlot = kSamplerSlotMirroredRepeatWater},
		{.eFlag = DescriptorFlags::kSamplerClampLinear, .eSlot = kSamplerSlotLinearClamp},
		{.eFlag = DescriptorFlags::kSamplerRepeatLinear, .eSlot = kSamplerSlotRepeatLinear},
		{.eFlag = DescriptorFlags::kSamplerMirroredRepeatLinear, .eSlot = kSamplerSlotMirroredRepeatLinear},
		{.eFlag = DescriptorFlags::kSamplerSmoke, .eSlot = kSamplerSlotSmoke},
		{.eFlag = DescriptorFlags::kSamplerWindClamp, .eSlot = kSamplerSlotWindClamp},
	};

	int64_t iSamplerFlagCount = 0;
	for (const FlagToSlotEntry& rEntry : kFlagToSlot)
	{
		iSamplerFlagCount += (flags & rEntry.eFlag ? 1 : 0);
	}
	ASSERT(iSamplerFlagCount <= 1);

	for (const FlagToSlotEntry& rEntry : kFlagToSlot)
	{
		if (flags & rEntry.eFlag)
		{
			return mpSamplersVkSampler[rEntry.eSlot];
		}
	}

	// Unflagged descriptors sample internal render targets. They need linear clamp filtering, but player-facing
	// anisotropy applies only to the explicitly flagged image-render samplers.
	return mpSamplersVkSampler[kSamplerSlotLinearClamp];
}

void TextureManager::ProcessPendingTextures(int64_t iFramebufferIndex)
{
	mFlags.Set(TextureManagerFlags::kPendingAcquireBarriers, false);

	// kGpuUploadComplete awaits transfer-queue adoption; kDiskLoaded uses the same-queue or re-armed Create fallback.
	// Both states write per-slot, texture-array, and lighting-blur descriptors, requiring RenderGlobal's all-framebuffer-fence drain first.
	// The pending-adoption counter excludes kUploading and provides an O(1) idle check.
	if (gpTextureUploadManager->miPendingAdoptions.load(std::memory_order_relaxed) == 0)
	{
		return;
	}

	miAcquireFramebufferIndex = iFramebufferIndex;
	bool bNeedAcquireBarrier = gpInstanceManager->miTransferQueueFamilyIndex != gpInstanceManager->miGraphicsQueueFamilyIndex;
	bool bRecordedBarriers = false;
	bool bAdoptedTextures = false;
	VkCommandBuffer vkAcquireCommandBuffer = mAcquireVkCommandBuffers.at(iFramebufferIndex);

	int64_t iAdoptedCount = 0;
	static constexpr int64_t kiMaxAdoptionsPerFrame = 4;

	for (auto& [rCrc, rTexture] : mTextureMap)
	{
		LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(rCrc);
		ChunkState eState = rLazyChunk.eState.value.load(std::memory_order_acquire);

		if (eState >= ChunkState::kReady)
		{
			continue;
		}

		if (eState == ChunkState::kGpuUploadComplete)
		{
			AdoptUploadedChunk(rCrc, rTexture, bNeedAcquireBarrier, vkAcquireCommandBuffer, bRecordedBarriers);
			bAdoptedTextures = true;

			if (++iAdoptedCount >= kiMaxAdoptionsPerFrame)
			{
				break;
			}
		}
		else if (eState == ChunkState::kDiskLoaded)
		{
			// Fallback: upload thread didn't GPU upload (same queue family)

			TextureUploadManager::ValidateTextureDimensions(rLazyChunk);
			int64_t iExpectedBytes = common::ComputeImageByteSize(rTexture.mInfo.vkFormat, rTexture.mInfo.vkExtent3D.width, rTexture.mInfo.vkExtent3D.height, rTexture.mInfo.uiMipLevels, rTexture.mInfo.uiArrayLayers, rTexture.mInfo.vkExtent3D.depth);
			ASSERT(iExpectedBytes > 0 && iExpectedBytes <= rLazyChunk.iDataSize);

			rTexture.Create(rTexture.mInfo, [&](std::span<std::byte> data, int64_t iPosition)
			{
				std::memcpy(data.data(), &rLazyChunk.pData[iPosition], static_cast<int64_t>(data.size_bytes()));
			});

			// The copy above is synchronous and no upload thread owns this chunk on the early-out path, so the
			// pool pages are reclaimable here exactly as on the kGpuUploadComplete path in AdoptUploadedChunk.
			gpFileManager->mpPackChunks->DecommitChunkRange(rCrc, 0, rLazyChunk.iDataSize);
			mTextureDescriptors.UpdateDescriptorsForTexture(rCrc);
			bAdoptedTextures = true;

			rLazyChunk.eState.value.store(ChunkState::kReady, std::memory_order_release);
			gpTextureUploadManager->miPendingAdoptions.fetch_sub(1, std::memory_order_relaxed); // adoptable -> kReady: disarm the pending-adoption counter

			if (gpTextureUploadManager->mLightingTextureCrcs.contains(rCrc))
			{
				BlurLightingTexture(rCrc);
			}

			// Only upload one texture a frame
			break;
		}
	}

	if (bAdoptedTextures)
	{
		mTextureDescriptors.UpdateTextureArrayDescriptors();
	}

	// CommandBufferManager prepends this acquire buffer. ProcessPendingTextures writes the non-atomic kPendingAcquireBarriers
	// flag and miAcquireFramebufferIndex on Graphics::RenderGlobal's main thread; SubmitGlobalToQueue reads them on
	// mSubmitGlobal. Publication depends on that worker's Wake/Wait edge.
	if (bRecordedBarriers)
	{
		CHECK_VK(vkEndCommandBuffer(vkAcquireCommandBuffer));
		mFlags.Set(TextureManagerFlags::kPendingAcquireBarriers);
	}
}

void TextureManager::AdoptUploadedChunk(common::crc_t crc, Texture& rTexture, bool bNeedAcquireBarrier, VkCommandBuffer vkAcquireCommandBuffer, bool& rbRecordedBarriers)
{
	LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(crc);

	// Adopt the GPU-uploaded image (sets mVkImage and creates VkImageView)
	rTexture.AdoptTransferredImage(rLazyChunk.vkUploadImage, rLazyChunk.vmaAllocation);

	bool bIsLightingTexture = gpTextureUploadManager->mLightingTextureCrcs.contains(crc);

	// Lighting textures handle their own acquire barrier inside BlurLightingTexture's OneShotCommandBuffer
	if (bNeedAcquireBarrier && !bIsLightingTexture)
	{
		EnsureAcquireCommandBufferBegun(vkAcquireCommandBuffer, rbRecordedBarriers);
		rTexture.RecordAcquireBarrier(vkAcquireCommandBuffer);
	}

	// Race-free decommit of worker-thread-shared CPU-pool state: reaching kGpuUploadComplete means the
	// transfer thread (UploadThread) finished this chunk and released ownership, so returning its pages
	// here (on the main thread) cannot race that thread. pData and iDataSize keep their construction
	// values and describe the reserved pool range, not resident bytes — a whole reload recommits that
	// range before writing it, and eState stays the residency authority.
	gpFileManager->mpPackChunks->DecommitChunkRange(crc, 0, rLazyChunk.iDataSize);
	mTextureDescriptors.UpdateDescriptorsForTexture(crc);

	rLazyChunk.eState.value.store(ChunkState::kReady, std::memory_order_release);
	gpTextureUploadManager->miPendingAdoptions.fetch_sub(1, std::memory_order_relaxed); // adoptable -> kReady: disarm the pending-adoption counter

	if (bIsLightingTexture)
	{
		BlurLightingTexture(crc, bNeedAcquireBarrier);
	}
}

void TextureManager::EnsureAcquireCommandBufferBegun(VkCommandBuffer vkAcquireCommandBuffer, bool& rbRecordedBarriers)
{
	if (rbRecordedBarriers)
	{
		return;
	}

	VkCommandBufferBeginInfo vkCommandBufferBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	CHECK_VK(vkBeginCommandBuffer(vkAcquireCommandBuffer, &vkCommandBufferBeginInfo));
	rbRecordedBarriers = true;
}

void TextureManager::WaitForTextures(std::span<const common::crc_t> crcs)
{
	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(crcs, LoadPriority::kRealtime);

	for (common::crc_t crc : crcs)
	{
		LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(crc);
		if (rLazyChunk.eState.value.load(std::memory_order_acquire) >= ChunkState::kReady)
		{
			continue;
		}

		// Upload in progress — spin until upload thread finishes and ProcessPendingTextures adopts
		while (rLazyChunk.eState.value.load(std::memory_order_acquire) < ChunkState::kReady)
		{
			gpTextureUploadManager->RethrowException();

			// Signal upload thread to process one chunk (drain then release to avoid binary_semaphore double-release UB)
			// Return value intentionally discarded: we only need to drain the semaphore to 0 before release()
			std::ignore = gpTextureUploadManager->mFrameSignal.try_acquire();
			gpTextureUploadManager->mFrameSignal.release();

			std::this_thread::yield();
			ProcessPendingTextures(0);

			// Flush pending acquire barriers since we're not in the render loop
			if (mFlags & TextureManagerFlags::kPendingAcquireBarriers)
			{
				VkFenceCreateInfo vkFenceCreateInfo
				{
					.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
				};
				VkFence vkFence = VK_NULL_HANDLE;
				CHECK_VK(vkCreateFence(gpDeviceManager->mVkDevice, &vkFenceCreateInfo, nullptr, &vkFence));

				VkCommandBuffer vkAcquireCommandBuffer = mAcquireVkCommandBuffers.at(miAcquireFramebufferIndex);
				VkSubmitInfo vkSubmitInfo
				{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.waitSemaphoreCount = 0,
					.pWaitSemaphores = nullptr,
					.pWaitDstStageMask = nullptr,
					.commandBufferCount = 1,
					.pCommandBuffers = &vkAcquireCommandBuffer,
					.signalSemaphoreCount = 0,
					.pSignalSemaphores = nullptr,
				};
				CHECK_VK(vkQueueSubmit(gpDeviceManager->mGraphicsVkQueue, 1, &vkSubmitInfo, vkFence));
				CHECK_VK(vkWaitForFences(gpDeviceManager->mVkDevice, 1, &vkFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));

				vkDestroyFence(gpDeviceManager->mVkDevice, vkFence, nullptr);
				mFlags.Set(TextureManagerFlags::kPendingAcquireBarriers, false);
			}
		}
	}
}

void TextureManager::WaitForTextures(std::span<Texture* const> textures)
{
	common::ScopedWorkbufferArena scopedWorkbufferArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (Texture* pTexture : textures)
	{
		common::gpThreadLocal->mWorkbuffer.PushBack<common::crc_t>(pTexture->mInfo.uiCrc);
	}

	WaitForTextures(common::gpThreadLocal->mWorkbuffer.Span<common::crc_t>());
}

void RegisterLightingTextureCrc(common::crc_t crc)
{
	// Heap: unordered_set insert during startup registration
	ScopedSuppressAllocationTracking suppress;
	gpTextureUploadManager->mLightingTextureCrcs.insert(crc);
	// Each registered lighting texture consumes one of the kiLightingBlurSlots reserved descriptor slots.
	ASSERT(static_cast<int64_t>(gpTextureUploadManager->mLightingTextureCrcs.size()) <= kiLightingBlurSlots);
}

void TextureManager::BlurLightingTexture(common::crc_t crc, bool bNeedAcquireBarrier)
{
	// Heap: GPU textures for pre-blurred lighting
	ScopedSuppressAllocationTracking suppress;

	Texture& rSource = mTextureMap.at(crc);
	uint32_t uiWidth = rSource.mInfo.vkExtent3D.width * 2;
	uint32_t uiHeight = rSource.mInfo.vkExtent3D.height * 2;

	// Texture::Create destroys the existing image before recreation.
	auto itIntermediate = mBlurIntermediateTextures.try_emplace(crc).first;
	itIntermediate->second.Create(
	{
		.textureFlags = {},
		.name = "LightingBlurIntermediate",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatCombine,
		.vkExtent3D = VkExtent3D {.width = uiWidth, .height = uiHeight, .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kComputeReadWrite,
	});

	auto itResult = mBlurredLightingTextures.try_emplace(crc).first;
	itResult->second.Create(
	{
		.textureFlags = {},
		.name = "LightingBlurResult",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatCombine,
		.vkExtent3D = VkExtent3D {.width = uiWidth, .height = uiHeight, .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});

	Texture& rIntermediate = itIntermediate->second;
	Texture& rResult = itResult->second;

	Pipeline& rHorizontalBlur = gpPipelineManager->mpPipelines[kPipelineLightingBlurH];
	Pipeline& rVerticalBlur = gpPipelineManager->mpPipelines[kPipelineLightingBlurV];

	PipelineDescriptorWriter::UpdateImageDescriptor(rHorizontalBlur, 0, mpSamplersVkSampler[kSamplerSlotLinearClamp], rSource.mVkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	PipelineDescriptorWriter::UpdateImageDescriptor(rHorizontalBlur, 1, VK_NULL_HANDLE, rIntermediate.mVkImageView, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	PipelineDescriptorWriter::UpdateImageDescriptor(rVerticalBlur, 0, mpSamplersVkSampler[kSamplerSlotLinearClamp], rIntermediate.mVkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	PipelineDescriptorWriter::UpdateImageDescriptor(rVerticalBlur, 1, VK_NULL_HANDLE, rResult.mVkImageView, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	int32_t iWidth = static_cast<int32_t>(uiWidth);
	int32_t iHeight = static_cast<int32_t>(uiHeight);
	float fSigma = gLightingBlurSigma.mfCurrent;
	float fPackedW = static_cast<float>(static_cast<int32_t>(gLightingBlurSampleCount.mfCurrent)) + gLightingBlurEdgeFalloff.mfCurrent / 100.0f;

	OneShotCommandBuffer oneShotCommandBuffer;
	VkCommandBuffer vkCommandBuffer = oneShotCommandBuffer.mVkCommandBuffer;

	// Complete queue family ownership transfer if texture was uploaded on a separate transfer queue
	if (bNeedAcquireBarrier)
	{
		rSource.RecordAcquireBarrier(vkCommandBuffer);
	}

	// Horizontal pass: source → intermediate
	rIntermediate.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kComputeReadWrite);
	rHorizontalBlur.RecordCompute(0, vkCommandBuffer, TileCount(uiWidth), TileCount(uiHeight), 1, {std::bit_cast<float>(iWidth), std::bit_cast<float>(iHeight), fSigma, fPackedW});

	// Transition intermediate: storage write → shader read for V pass sampler
	rIntermediate.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);

	// Vertical pass: intermediate → result
	rResult.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	rVerticalBlur.RecordCompute(0, vkCommandBuffer, TileCount(uiWidth), TileCount(uiHeight), 1, {std::bit_cast<float>(iWidth), std::bit_cast<float>(iHeight), fSigma, fPackedW});

	// Transition result back to shader read for bindless sampling
	rResult.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);

	oneShotCommandBuffer.Execute();

	// Register blurred texture in bindless array. Render-phase caller: CrcToIndex is lock-free, safe only
	//   because no worker Spawn runs concurrently (see TextureDescriptors::CrcToIndex).
	ASSERT(common::gpThreadLocal != nullptr && !common::gpThreadLocal->mbInFrameTick);
	common::crc_t blurredCrc = crc ^ TextureDescriptors::kBlurSalt;
	int64_t iBlurredIndex = mTextureDescriptors.CrcToIndex(blurredCrc);
	mTextureDescriptors.mImageInfos.at(iBlurredIndex).imageView = rResult.mVkImageView;
	mTextureDescriptors.UpdateTextureArrayDescriptors();
}

void TextureManager::ReblurAllLightingTextures()
{
	for (common::crc_t crc : gpTextureUploadManager->mLightingTextureCrcs)
	{
		// A registered CRC can be a cross-pack reference with no chunk in this pack set; IsChunkReady reports it
		//   not ready, so it stays unblurred like the request path leaves it
		if (gpFileManager->mpPackChunks->IsChunkReady(crc))
		{
			BlurLightingTexture(crc);
		}
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
