#pragma once

#if defined(BT_CLIENT)

#include "Data/Texture.h"
#include "RenderTargetTextures.h"
#include "TextureCache.h"
#include "TextureDescriptors.h"

namespace engine
{

enum class TextureManagerFlags : uint8_t
{
	kPendingAcquireBarriers = 1 << 0,
	kPendingLightingReblur  = 1 << 1,
};

class TextureManager
{
public:

	static std::tuple<int64_t, int64_t> DetailTextureSize(float fMultiplier);
	static std::tuple<int64_t, int64_t> LightingDetailTextureSize(float fMultiplier);
	static std::tuple<int64_t, int64_t> WaterDetailTextureSize(float fMultiplier);
	static float DetailTextureAspectRatio();

	TextureManager();
	~TextureManager();
	// Starts asynchronous uploads and performs fatal-rethrowing boot waits after Graphics owns this manager.
	void InitializeBootTextures();

	void DestroyScreenDependentResources();
	void CreateScreenDependentResources();

	void DestroySamplers();
	void CreateSamplers();

	VkSampler GetSampler(DescriptorFlags_t flags);

	void ProcessPendingTextures(int64_t iFramebufferIndex);

	// Allocate the per-framebuffer acquire-barrier command pool + buffers (mAcquireVkCommandPool /
	// mAcquireVkCommandBuffers). Shared by the ctor and CreateScreenDependentResources.
	void CreateAcquireCommandBuffers();

	// Create a 1x1 programmatic placeholder Texture. Shared by the seven ctor placeholders (white / white
	// cube / the five island bindless-array anchors); only name/flags/format/layers/view-type/pixel differ.
	void CreatePlaceholderTexture(Texture& rTexture, std::string_view name, VkImageCreateFlags vkImageCreateFlags, VkFormat vkFormat, uint32_t uiArrayLayers, VkImageViewType vkImageViewType, const std::function<void(std::span<std::byte>, int64_t)>& rPixelWriter);

	void AdoptUploadedChunk(common::crc_t crc, Texture& rTexture, bool bNeedAcquireBarrier, VkCommandBuffer vkAcquireCommandBuffer, bool& rbRecordedBarriers);
	void EnsureAcquireCommandBufferBegun(VkCommandBuffer vkAcquireCommandBuffer, bool& rbRecordedBarriers);

	void WaitForTextures(std::span<const common::crc_t> crcs);
	void WaitForTextures(std::span<Texture* const> textures);

	// Water normal map atlas: ordered alphabetically by stripped display name; index used by Wrapper indices and shader.
	static inline constexpr int64_t kiWaterNormalCount = shaders::kiWaterNormalCount;
	static inline constexpr common::crc_t kpWaterNormalCrcs[kiWaterNormalCount]
	{
		data::kTexturesWaterBC50pngCrc,
		data::kTexturesWaterBC53jpgCrc,
		data::kTexturesWaterBC5FoamjpgCrc,
		data::kTexturesWaterBC5FoamBjpgCrc,
		data::kTexturesWaterBC5GreenCalmjpgCrc,
		data::kTexturesWaterBC5GreenSeajpgCrc,
		data::kTexturesWaterBC5GreenSeaBjpgCrc,
		data::kTexturesWaterBC5LakejpgCrc,
		data::kTexturesWaterBC5PondSedimentjpgCrc,
		data::kTexturesWaterBC5PooljpgCrc,
		data::kTexturesWaterBC5SeaDistantjpgCrc,
		data::kTexturesWaterBC5SeaWavesjpgCrc,
		data::kTexturesWaterBC5SeaWavesBjpgCrc,
		data::kTexturesWaterBC5SlimyWaterjpgCrc,
		data::kTexturesWaterBC5SlimyWaterBjpgCrc,
		data::kTexturesWaterBC5StonesAndRipplesjpgCrc,
		data::kTexturesWaterBC5WaterFalljpgCrc,
	};
	static inline constexpr std::string_view kpWaterNormalNames[kiWaterNormalCount]
	{
		"0", "3", "Foam", "FoamB", "GreenCalm", "GreenSea", "GreenSeaB", "Lake",
		"PondSediment", "Pool", "SeaDistant", "SeaWaves", "SeaWavesB", "SlimyWater",
		"SlimyWaterB", "StonesAndRipples", "WaterFall",
	};

	// IBL cubemap CRCs (loaded by InitializeBootTextures; referenced by Model and Water pipelines).
	static inline constexpr common::crc_t kIrradianceCrc       = data::kTexturesCKloofendalPuresky_IrradianceR16G16B16A16_SFLOATCrc;
	static inline constexpr common::crc_t kPrefilteredCrc      = data::kTexturesCKloofendalPuresky_PrefilteredR16G16B16A16_SFLOATCrc;
	static inline constexpr common::crc_t kPrefilteredWaterCrc = data::kTexturesCRyfjallet_PrefilteredR16G16B16A16_SFLOATCrc;

	// Non-water priority textures preloaded at boot, surrounding the water-normal block below.
	static inline constexpr common::crc_t kpPriorityHead[]
	{
		data::kTexturesWaterDepthLutpngCrc,
		data::kTexturesWaterBC4NoisepngCrc,
	};
	static inline constexpr common::crc_t kpPriorityTail[]
	{
		data::kTexturesTerrainBC7Rock0jpgCrc,
		data::kTexturesTerrainBC5RockNormal1jpgCrc,
		data::kTexturesTerrainBC5RockNormal2jpgCrc,
		data::kTexturesTerrainBC5RockNormal4jpgCrc,
		data::kTexturesTerrainBC7SandpngCrc,
		data::kTexturesTerrainBC5SandNormal0jpgCrc,
		data::kTexturesTerrainBC5SandNormal1pngCrc,
		data::kTexturesTerrainBC5SandNormal2pngCrc,
	};

	// Priority textures preloaded at boot so chevron switches and the first terrain frame never show a
	// placeholder. The water-normal block is single-sourced from kpWaterNormalCrcs — do not re-list it here.
	static inline constexpr int64_t kiPriorityTextureCount = std::size(kpPriorityHead) + kiWaterNormalCount + std::size(kpPriorityTail);
	static inline constexpr std::array<common::crc_t, kiPriorityTextureCount> kPriorityTextures = []() consteval
	{
		std::array<common::crc_t, kiPriorityTextureCount> priorityTextures {};
		std::ranges::copy(kpPriorityTail, std::ranges::copy(kpWaterNormalCrcs, std::ranges::copy(kpPriorityHead, priorityTextures.begin()).out).out);
		return priorityTextures;
	}();

	RenderTargetTextures mRenderTargetTextures;
	TextureCache mTextureCache;
	TextureDescriptors mTextureDescriptors;

	// Sampler storage indexed by SamplerSlot. CreateSamplers builds each slot with its bespoke
	// VkSamplerCreateInfo; GetSampler maps a DescriptorFlags sampler bit to one of these.
	enum SamplerSlot : int64_t
	{
		kSamplerSlotSmoke,
		kSamplerSlotWindClamp,
		kSamplerSlotLinearClamp,
		kSamplerSlotBorder,
		kSamplerSlotBorderWhite,
		kSamplerSlotClamp,
		kSamplerSlotElevation,
		kSamplerSlotRepeat,
		kSamplerSlotRepeatModelData,
		kSamplerSlotMirroredRepeat,
		kSamplerSlotMirroredRepeatWater,
		kSamplerSlotRepeatLinear,
		kSamplerSlotMirroredRepeatLinear,
		kSamplerSlotCount,
	};
	VkSampler mpSamplersVkSampler[kSamplerSlotCount] {};

	// Per-mip Toksvig variance tables for the water normal maps, copied from TextureHeader::pfMipVariance
	// during the ctor chunk walk (headers are resident at startup; the lazy pixel data is not). Indexed by
	// water-normal atlas slot; LightingUniforms uploads the three selected octave groups' tables each frame.
	float mpfWaterNormalMipVariance[kiWaterNormalCount][common::TextureHeader::kiMipVarianceCount] {};

	Texture mWhiteTexture;
	Texture mWhiteCubeTexture;

	// Slot-0 island bindless-array anchor. Programmatic 1x1 textures with neutral per-channel
	// values: ocean-bottom elevation (submerged below the water), mid-gray color, up-vector normals,
	// no-AO. Never adopted by a real island (miNextTextureSlot starts at 1); higher slots alias these
	// until their real chunks reach kReady via RestorationSweep.
	Texture mIslandPlaceholderElevation;
	Texture mIslandPlaceholderColor;
	Texture mIslandPlaceholderNormals;
	Texture mIslandPlaceholderAmbientOcclusion;
	Texture mIslandPlaceholderMasks;

	std::unordered_map<common::crc_t, Texture> mTextureMap;

	// Pre-blur lighting textures (the registered CRC set lives on TextureUploadManager, which outlives this manager)
	std::unordered_map<common::crc_t, Texture> mBlurredLightingTextures;
	std::unordered_map<common::crc_t, Texture> mBlurIntermediateTextures;
	void BlurLightingTexture(common::crc_t crc, bool bNeedAcquireBarrier = false);
	void ReblurAllLightingTextures();

	VkCommandPool mAcquireVkCommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> mAcquireVkCommandBuffers;
	int64_t miAcquireFramebufferIndex = 0;
	common::Flags<TextureManagerFlags> mFlags;
};

inline TextureManager* gpTextureManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
