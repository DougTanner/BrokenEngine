#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct ModelPipelineSpec
{
	common::crc_t sceneCrc = 0;
	PipelineInfo pipelineInfo {};
	bool bIsPipelineShadow = false;
};

enum DynamicPipelineType
{
	kDynamicPipelineLighting,
	kDynamicPipelineAxisAlignedLighting,
	kDynamicPipelineVisibleLights,
	kDynamicPipelineBillboards,
	kDynamicPipelineSmokeAxisAligned,
	kDynamicPipelineSmoke,
	kDynamicPipelineWindDepositA,
	kDynamicPipelineWindDepositB,
	kDynamicPipelineWindDepositAxisAlignedA,
	kDynamicPipelineWindDepositAxisAlignedB,
	kDynamicPipelineHexShields,
	kDynamicPipelineHexShieldsLighting,

	kDynamicPipelineCount,
};

enum DynamicModelPipelineType
{
	kDynamicModelPipelineModel,
	kDynamicModelPipelineModelShadow,

	kDynamicModelPipelineCount,
};

class DynamicPipelines
{
public:

	explicit DynamicPipelines(std::unordered_map<common::crc_t, Shader>& rShaders);

	ModelPipeline* CreateModelPipeline(const ModelPipelineSpec& rModelPipelineSpec);
	// Creates the pipeline pair's ModelLayout buffer, holding iInitialCapacity elements, before the pipelines.
	void CreateModelPipelines(common::crc_t crc, std::string_view name, common::crc_t sceneCrc, int64_t iInitialCapacity);
	void UpdateModelPipelineDescriptors(common::crc_t crc, int64_t iCommandBuffer, int64_t iBinding, Buffer* pBuffer);
	void WriteModelPipelineIndirectBuffers(common::crc_t crc, int64_t iCommandBuffer, int64_t iCount);

	// The pipeline pair's one draw publication per frame: grows its ModelLayout buffer to iCapacity, has Fill write
	// the instances from slot 0 and return their count, then writes both indirect counts, zero included.
	template <typename FILL>
	int64_t RenderModelPipelines(common::crc_t crc, std::string_view name, int64_t iCommandBuffer, int64_t iCapacity, FILL Fill)
	{
		if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(crc, kBufferMain, name, sizeof(shaders::ModelLayout), iCapacity, iCommandBuffer); pBuffer != nullptr)
		{
			UpdateModelPipelineDescriptors(crc, iCommandBuffer, 2, pBuffer);
		}

		auto [pLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(crc, kBufferMain, iCommandBuffer);
		int64_t iCount = Fill(pLayouts);
		ASSERT(iCount <= iBufferCapacity);
		WriteModelPipelineIndirectBuffers(crc, iCommandBuffer, iCount);
		return iCount;
	}

	void RequestModelPipelineTextures(common::crc_t crc);
	// Requests only the pipelines whose presence flag is set.
	void RequestModelPipelineTextures(std::span<const common::crc_t> crcs, std::span<const bool> presentFlags);
	void CreatePipelineVisibleLights(common::crc_t crc, std::string_view name, Buffer* pStorageBuffers);
	void CreatePipelineBillboards(common::crc_t crc, std::string_view name);
	void CreatePipelineHexShields(common::crc_t crc, std::string_view name);
	void CreatePipelineHexShieldsLighting(common::crc_t crc, std::string_view name);
	void UpdateAllModelPipelineDescriptors(int64_t iCommandBuffer, int64_t iBinding, Buffer* pBuffer);

	std::vector<std::unique_ptr<Pipeline>> mPipelines;
	std::vector<std::unique_ptr<ModelPipeline>> mModelPipelines;
	std::unordered_map<common::crc_t, Pipeline*> mpPipelineMaps[kDynamicPipelineCount];
	std::unordered_map<common::crc_t, ModelPipeline*> mpModelPipelineMaps[kDynamicModelPipelineCount];
	std::unordered_map<common::crc_t, std::string> mShadowPipelineNames; // Owns shadow pipeline name strings

	void CreateAreaLightingPipeline(DynamicPipelineType eType, common::crc_t crc, std::string_view name, common::crc_t vertexShaderCrc, common::crc_t fragmentShaderCrc);
	void CreateDepositPipeline(DynamicPipelineType eType, common::crc_t crc, std::string_view name, common::crc_t vertexShaderCrc, common::crc_t fragmentShaderCrc, const Texture& rTargetTexture, const DescriptorInfo& rTextureDescriptor, VkBuffer* pVkOccupancyBuffer);

private:

	void AddPipeline(DynamicPipelineType eType, common::crc_t crc, const PipelineInfo& rPipelineInfo);
	void CreateMainModelPipeline(common::crc_t crc, std::string_view name, common::crc_t sceneCrc, Buffer* pStorageBuffers);
	void CreateShadowModelPipeline(common::crc_t crc, std::string_view name, common::crc_t sceneCrc, Buffer* pStorageBuffers);

	std::unordered_map<common::crc_t, Shader>& mrShaders;
};

} // namespace engine

#endif // defined(BT_CLIENT)
