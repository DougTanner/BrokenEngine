#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct CommandBufferRecordGlobal
{
	static void Record(int64_t iFramebuffer);

private:

	static void RecordShadowPasses(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, Pipeline* pPipelines);
	static void RecordTerrainPasses(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, Pipeline* pPipelines);
	static void RecordWindSpreadPipeline(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, int64_t iWindTilesX, int64_t iWindTilesY, Pipeline* pPipelines);
	static void RecordSmokeSpreadPipeline(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, Pipeline* pPipelines);
	static void RecordSmokeSpreadHalf(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, Pipeline& rDilatePipeline, VkBuffer vkOutputOccupancyBuffer, Texture& rSmokeTexture, Pipeline& rSpreadPipeline);
	static void RecordParticleUpdatePasses(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, Pipeline* pPipelines);
};

} // namespace engine

#endif // defined(BT_CLIENT)
