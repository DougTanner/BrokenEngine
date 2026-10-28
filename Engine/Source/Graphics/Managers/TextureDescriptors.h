#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class TextureManager;

class TextureDescriptors
{
public:

	explicit TextureDescriptors(TextureManager& rTextureManager);

	void Create();
	void Destroy();

	void WriteGlobalDescriptorSets();
	void SetLightingReference(int64_t iFramebuffer, const Texture* pTexture, bool bEnabled);
	void InitializeIslandSlots();

	// Epoch framebuffer of a drained context, where no pending command buffer can use any framebuffer's sets.
	static constexpr int64_t kiAllFramebuffers = -1;

	class [[nodiscard]] ScopedBindlessWriteEpoch
	{
	public:
		// iFramebuffer names the only framebuffer whose sets are writable (RenderGlobal, once that framebuffer's fence
		// has signaled), or kiAllFramebuffers in a drained context.
		ScopedBindlessWriteEpoch(TextureDescriptors& rTextureDescriptors, int64_t iFramebuffer);
		ScopedBindlessWriteEpoch(const ScopedBindlessWriteEpoch&) = delete;
		ScopedBindlessWriteEpoch& operator=(const ScopedBindlessWriteEpoch&) = delete;
		~ScopedBindlessWriteEpoch();

	private:
		TextureDescriptors& mrTextureDescriptors;
	};

	// Adoption, restoration, eviction and first blur update live bookkeeping and append publication entries; publishing
	// writes each entry a framebuffer has not applied into that framebuffer's sets only, from live state, so replay is idempotent.
	// iSlot < 0 appends a key entry (texture, island or salted blurred CRC); iSlot >= 0 appends an evicted island slot.
	void AppendPublication(common::crc_t bindingKey, int64_t iSlot);
	void PublishFramebuffer(int64_t iFramebuffer);
	void PublishAllFramebuffers();
	bool HasUnappliedPublications(int64_t iFramebuffer) const;
	// kiAllFramebuffers asserts the drained mode.
	void AssertSetsWritable(int64_t iFramebuffer) const;

	// Parameter bundle for RegisterTextureBinding. Encodes three mutually-exclusive binding shapes:
	// single texture (pTexture), full array (ppTextures + iCount), or single array element (also iArrayIndex >= 0).
	struct TextureBindingInfo
	{
		common::crc_t crc = 0;
		Pipeline* pPipeline = nullptr;
		int64_t iBinding = -1;
		DescriptorFlags_t samplerFlags;
		Texture* pTexture = nullptr;
		Texture** ppTextures = nullptr;
		int64_t iCount = 0;
		int64_t iArrayIndex = -1;
	};
	void RegisterTextureBinding(const TextureBindingInfo& rInfo);
	void RegisterBindlessArrayConsumer(std::span<Texture*> textures, Pipeline* pPipeline, int64_t iBinding, DescriptorFlags_t samplerFlags);
	void RegisterStandaloneSamplerBinding(Pipeline* pPipeline, int64_t iBinding, DescriptorFlags_t samplerFlags);
	void UnregisterPipeline(const Pipeline* pPipeline);
	void UpdateDescriptorsForTexture(common::crc_t crc);
	// Island slots own five coordinated array elements: template-owned elevation plus four lazy
	// chunk-backed channels. The registry keeps their metadata and descriptor registrations together.
	void MintIslandSlot(int64_t iSlot, common::crc_t islandCrc, Texture& rElevationTexture, const common::crc_t (&textureCrcs)[4]);
	// Redirects every island channel to slot-0 placeholders, queues publication of the four texture keys and the slot,
	// and removes all five binding records before the caller retires the old resources or reuses iSlot.
	void EvictIslandSlot(int64_t iSlot, common::crc_t islandCrc, const common::crc_t (&textureCrcs)[4]);
	// Template-owned elevation has no lazy chunk; restore it at the four-channel-ready transition.
	void RestoreIslandSlot(common::crc_t islandCrc);
	void VerifyAllDescriptorGenerations() const;
	void RewriteSamplerDescriptors();
	void ClearTextureBindings();

	int64_t CrcToIndex(common::crc_t crc);

	// Global descriptor Set 0 shared by all graphics pipelines
	VkDescriptorSetLayout mGlobalVkDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> mGlobalDescriptorSets;
	Texture mLightingFallback;
	std::vector<Buffer> mLightingControls;

	struct TextureBinding
	{
		Pipeline* pPipeline = nullptr;
		int64_t iBinding = -1;
		DescriptorFlags_t samplerFlags;
		Texture* pTexture = nullptr;
		// Owned copy of texture pointers for full-array bindings (e.g. water normals). Per-island
		// bindings retain only pTexture, the descriptor element they own.
		std::vector<Texture*> textures;
		// pTexture->miGeneration / textures[i]->miGeneration snapshotted at descriptor-write time.
		// VerifyAllDescriptorGenerations breaks if the live texture's generation
		// has drifted (texture destroyed/recreated since this descriptor was written).
		int64_t iTextureGeneration = 0;
		std::vector<int64_t> iTextureGenerations;
		// True when this descriptor element deliberately resolves to the placeholder because its
		// owned Texture has no view. Verification accepts that state until a real view appears.
		bool bTextureUsesPlaceholder = false;
		int64_t iArrayIndex = -1;
	};

	struct StandaloneSamplerBinding
	{
		Pipeline* pPipeline = nullptr;
		int64_t iBinding = -1;
		DescriptorFlags_t samplerFlags;
	};

	// Writes rBinding into Set 1 copy iSet from live state. A full-array binding writes only the elements holding
	// pKeyTexture, or every element when pKeyTexture is null; a single-texture binding always holds a non-null texture.
	void WriteBindingToSet(const TextureBinding& rBinding, int64_t iSet, const Texture* pKeyTexture);
	// Snapshots the live generations a write of rBinding publishes.
	void SnapshotBindingGenerations(TextureBinding& rBinding);
	void SynchronizeFullArrayBindingGenerations(const TextureBinding& rBinding);
	void WriteImageElement(VkDescriptorSet vkDescriptorSet, int64_t iBinding, int64_t iElement, VkSampler vkSampler, const Texture* pTexture);
	void WriteFullArrayDescriptors(const Pipeline& rPipeline, int64_t iSet, int64_t iBinding, std::span<Texture* const> textures, VkSampler vkSampler);
	void WriteArrayElementFromLive(Texture* const* ppArray, int64_t iIndex, int64_t iSet);
	void RegisterIslandSlotBindings(common::crc_t bindingKey, Texture** ppTextures, int64_t iSlot);
	void UnregisterBindingsForKey(common::crc_t bindingKey);
	void AssertBindlessWriteEpoch() const;

	// PipelineDescriptorWriter registers kBindlessArrayConsumer descriptors for arrays with lazily assigned per-slot binding keys, such as IslandTerrainResidency.
	// The consumer map uses the live array pointer, such as RenderTargetTextures::mElevationTextures.data(), as its key.
	struct BindlessArrayConsumer
	{
		Pipeline* pPipeline = nullptr;
		int64_t iBinding = -1;
		DescriptorFlags_t samplerFlags;
		int64_t iCount = 0;
	};

	struct IslandSlot
	{
		common::crc_t islandCrc = 0;
		common::crc_t pTextureCrcs[4] {};
		Texture* pElevationTexture = nullptr;
	};

	std::unordered_map<common::crc_t, std::vector<TextureBinding>> mTextureBindings;
	std::unordered_map<Texture* const*, std::vector<BindlessArrayConsumer>> mBindlessArrayConsumers;
	std::unordered_map<int64_t, IslandSlot> mIslandSlots;
	std::vector<StandaloneSamplerBinding> mStandaloneSamplerBindings;

	std::vector<VkDescriptorImageInfo> mImageInfos;
	std::unordered_map<common::crc_t, int64_t> mImageInfosMap;
	int64_t miNextTextureIndex = 0;

private:

	struct PendingPublication
	{
		common::crc_t bindingKey = 0;
		int64_t iSlot = -1;
	};

	TextureManager& mrTextureManager;
	bool mbBindlessWriteEpoch = false;
	int64_t miBindlessWriteFramebuffer = kiAllFramebuffers;
	std::vector<PendingPublication> mPendingPublications;
	// Per framebuffer: the count of leading mPendingPublications entries already written into its sets.
	std::vector<int64_t> mAppliedPublicationCursors;
};

} // namespace engine

#endif // defined(BT_CLIENT)
