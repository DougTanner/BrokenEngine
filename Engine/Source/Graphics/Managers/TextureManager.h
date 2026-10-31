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
	kAcquireCommandBufferBegun = 1 << 1,
	// Set when a blur is recorded into the acquire command buffer, cleared by EndAcquireCommandBuffer: set f of the blur
	// pipelines holds one blur's bindings, so a second blur in the same acquire command buffer waits for a later frame.
	kBlurRecorded = 1 << 2,
};

class TextureManager : public common::Singleton<TextureManager>
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
	void CreatePlaceholderTexture(Texture& rTexture, std::string_view name, VkImageCreateFlags vkImageCreateFlags, VkFormat vkFormat, int64_t iArrayLayers, VkImageViewType vkImageViewType, const std::function<void(std::span<std::byte>, int64_t)>& rPixelWriter);

	void AdoptUploadedChunk(common::crc_t crc, Texture& rTexture, bool bNeedAcquireBarrier);
	// Returns the acquire command buffer of the framebuffer ProcessPendingTextures last named, beginning it on first use
	// since the last EndAcquireCommandBuffer. Work recorded into it precedes that framebuffer's Global submission.
	VkCommandBuffer EnsureAcquireCommandBufferBegun();
	// Ends a begun acquire command buffer and publishes it for the next Global submission; once per frame, after the last
	// recording (RenderGlobal after RestorationSweep, and each WaitForTextures pass before its flush).
	void EndAcquireCommandBuffer();

	// Drained context: opens its own all-framebuffer bindless write epoch, so the caller must not hold one.
	void WaitForTextures(std::span<const common::crc_t> crcs);
	void WaitForTextures(std::span<Texture* const> textures);

	// Retirement: staging buffers and images recorded work or descriptors may still use are destroyed only once every
	// framebuffer current at retirement has had its fence waited and its publications applied, or after a device drain.
	void RetireStagingBuffer(std::unique_ptr<StagingBuffer> pStagingBuffer);
	// Moves rTexture's image and view into retirement; rTexture keeps its info and generation with null handles, as Destroy leaves it.
	void RetireImage(Texture& rTexture);
	// Every frame once iFramebuffer's fence has signaled: no retired resource is held by that framebuffer any longer.
	void MarkFramebufferWaited(int64_t iFramebuffer);
	// Every frame after RenderGlobal's publication block: destroys the retired resources no framebuffer holds.
	void DestroyUnheldRetiredResources();
	// After a device drain.
	void DestroyAllRetiredResources();

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

	static inline constexpr int64_t kiWaterFoamTextureCount = shaders::kiWaterFoamTextureCount;
	static inline constexpr common::crc_t kpWaterFoamCrcs[]
	{
		data::kTexturesWaterBC7FoamWhiteFinepngCrc,
		data::kTexturesWaterBC7FoamWhiteBroadpngCrc,
		data::kTexturesWaterBC7FoamAmbientCGWhitepngCrc,
	};
	static inline constexpr std::string_view kpWaterFoamNames[] {"White Fine", "White Broad", "ambientCG White"};
	static_assert(std::size(kpWaterFoamCrcs) == kiWaterFoamTextureCount);
	static_assert(std::size(kpWaterFoamNames) == kiWaterFoamTextureCount);

	static inline constexpr int64_t kiWaterFoamBreakupTextureCount = shaders::kiWaterFoamBreakupTextureCount;
	static inline constexpr common::crc_t kpWaterFoamBreakupCrcs[]
	{
		data::kTexturesWaterBC4NoisepngCrc,
		data::kTexturesSmokeBC4tex_glass_0001_MKjpgCrc,
		data::kTexturesSmokeBC4tex_swirl_0002_MKjpgCrc,
	};
	static inline constexpr std::string_view kpWaterFoamBreakupNames[] {"Noise", "Glass", "Swirl"};
	static_assert(std::size(kpWaterFoamBreakupCrcs) == kiWaterFoamBreakupTextureCount);
	static_assert(std::size(kpWaterFoamBreakupNames) == kiWaterFoamBreakupTextureCount);

	// IBL cubemap CRCs (loaded by InitializeBootTextures; referenced by Model and Water pipelines).
	static inline constexpr common::crc_t kIrradianceCrc       = data::kTexturesCKloofendalPuresky_IrradianceR16G16B16A16_SFLOATCrc;
	static inline constexpr common::crc_t kPrefilteredCrc      = data::kTexturesCKloofendalPuresky_PrefilteredR16G16B16A16_SFLOATCrc;
	static inline constexpr common::crc_t kPrefilteredWaterCrc = data::kTexturesCRyfjallet_PrefilteredR16G16B16A16_SFLOATCrc;

	// Non-water priority textures preloaded at boot, surrounding the water texture catalogs below.
	static inline constexpr common::crc_t kpPriorityHead[]
	{
		data::kTexturesWaterDepthLutpngCrc,
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
	// placeholder. The water catalogs are single-sourced from their CRC tables.
	static inline constexpr int64_t kiPriorityTextureCount = std::size(kpPriorityHead) + kiWaterFoamTextureCount + kiWaterFoamBreakupTextureCount + kiWaterNormalCount + std::size(kpPriorityTail);
	static inline constexpr std::array<common::crc_t, kiPriorityTextureCount> kPriorityTextures = []() consteval
	{
		std::array<common::crc_t, kiPriorityTextureCount> priorityTextures {};
		std::ranges::copy(kpPriorityTail, std::ranges::copy(kpWaterNormalCrcs, std::ranges::copy(kpWaterFoamBreakupCrcs, std::ranges::copy(kpWaterFoamCrcs, std::ranges::copy(kpPriorityHead, priorityTextures.begin()).out).out).out).out);
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
	// Records the blur into vkCommandBuffer through the blur pipelines' Set 1 copy iDescriptorSet, which must not be bound
	// by another recording command buffer.
	void BlurLightingTexture(common::crc_t crc, int64_t iDescriptorSet, VkCommandBuffer vkCommandBuffer);
	// Drained context only: one executed one-shot per ready lighting texture, rewriting its blur images in place.
	void ReblurAllLightingTextures();

	VkCommandPool mAcquireVkCommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> mAcquireVkCommandBuffers;
	int64_t miAcquireFramebufferIndex = 0;
	common::Flags<TextureManagerFlags> mFlags;

private:

	struct RetiredStagingBuffer
	{
		std::unique_ptr<StagingBuffer> pStagingBuffer;
		uint32_t uiFramebufferMask = 0; // Framebuffers whose fence has not been waited since retirement
	};

	struct RetiredImage
	{
		std::unique_ptr<Texture> pTexture;
		uint32_t uiFramebufferMask = 0; // Framebuffers whose fence has not been waited since retirement
	};

	std::vector<RetiredStagingBuffer> mRetiredStagingBuffers;
	std::vector<RetiredImage> mRetiredImages;
};

inline TextureManager* gpTextureManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
