#if defined(BT_CLIENT)

#include "Pipeline.h"

#include "File/PackChunks.h"
#include "PipelineCreator.h"
#include "PipelineDescriptorWriter.h"

namespace engine
{

using enum DescriptorFlags;
using enum PipelineFlags;

// Always pushes the default 16-byte PushConstantsLayout range; callers guard that the pipeline uses the default range
// (iPushConstantBytes == 0), since a sub-16-byte override would overflow the layout's reserved push-constant range.
static void RecordPushConstants(VkCommandBuffer vkCommandBuffer, VkPipelineLayout vkPipelineLayout, VkShaderStageFlags vkStageFlags, const XMFLOAT4& rf4PushConstants)
{
	shaders::PushConstantsLayout pushConstantsLayout {};
	pushConstantsLayout.f4Pipeline = rf4PushConstants;
	vkCmdPushConstants(vkCommandBuffer, vkPipelineLayout, vkStageFlags, 0, sizeof(pushConstantsLayout), &pushConstantsLayout);
}

static void BindGraphicsDescriptorSets(VkCommandBuffer vkCommandBuffer, VkPipelineLayout vkPipelineLayout, VkDescriptorSetLayout vkExternalLayout, int64_t iDescriptorSetIndex, const std::vector<VkDescriptorSet>& rDescriptorSets)
{
	if (vkExternalLayout != VK_NULL_HANDLE)
	{
		VkDescriptorSet pVkDescriptorSets[2] = {gpTextureManager->mTextureDescriptors.mGlobalDescriptorSets.at(iDescriptorSetIndex), rDescriptorSets.at(iDescriptorSetIndex)};
		vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout, 0, 2, pVkDescriptorSets, 0, nullptr);
	}
	else
	{
		vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout, 0, 1, &rDescriptorSets.at(iDescriptorSetIndex), 0, nullptr);
	}
}

void BindComputeDescriptorSets(VkCommandBuffer vkCommandBuffer, VkPipelineLayout vkPipelineLayout, VkDescriptorSetLayout vkExternalLayout, int64_t iCommandBuffer, int64_t iDescriptorSetIndex, const std::vector<VkDescriptorSet>& rDescriptorSets)
{
	// Global Set 0 is indexed per-framebuffer (iCommandBuffer); per-pipeline Set 1 follows mbPerCommandBuffer (iDescriptorSetIndex).
	if (vkExternalLayout != VK_NULL_HANDLE)
	{
		VkDescriptorSet pVkDescriptorSets[2] = {gpTextureManager->mTextureDescriptors.mGlobalDescriptorSets.at(iCommandBuffer), rDescriptorSets.at(iDescriptorSetIndex)};
		vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipelineLayout, 0, 2, pVkDescriptorSets, 0, nullptr);
	}
	else
	{
		vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipelineLayout, 0, 1, &rDescriptorSets.at(iDescriptorSetIndex), 0, nullptr);
	}
}

int64_t Pipeline::ResolveBindingSetIndex(const PipelineInfo& rPipelineInfo, int64_t iBinding)
{
	const Shader* pFirstShader = rPipelineInfo.ppShaders[0];
	if (iBinding < pFirstShader->mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings && pFirstShader->mInformation.pVkDescriptorBindings[iBinding].descriptorCount > 0)
	{
		return pFirstShader->mInformation.puiDescriptorSetIndices[iBinding];
	}
	if (rPipelineInfo.flags & kCompute)
	{
		return 0;
	}
	const Shader* pSecondShader = rPipelineInfo.ppShaders[1];
	if (iBinding < pSecondShader->mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings && pSecondShader->mInformation.pVkDescriptorBindings[iBinding].descriptorCount > 0)
	{
		return pSecondShader->mInformation.puiDescriptorSetIndices[iBinding];
	}
	return 0;
}

Pipeline::Pipeline(const PipelineInfo& rInfo)
{
	Create(rInfo);
}

Pipeline::~Pipeline()
{
	Destroy();
}

void Pipeline::Create(const PipelineInfo& rInfo)
{
	Destroy();

	mbTexturesRequested = false;
	mTextureCrcs.clear();

	mInfo = rInfo;

	mExternalVkDescriptorSetLayout = mInfo.vkExternalDescriptorSetLayout != VK_NULL_HANDLE ? mInfo.vkExternalDescriptorSetLayout : gpTextureManager->mTextureDescriptors.mGlobalVkDescriptorSetLayout;
	mExternalSet1VkDescriptorSetLayout = mInfo.vkExternalSet1DescriptorSetLayout;

	if (mInfo.flags & kRenderTarget)
	{
		ASSERT(mInfo.vkTargetRenderPass != VK_NULL_HANDLE);
		ASSERT(mInfo.vkExtent3D.width != 0 && mInfo.vkExtent3D.height != 0);
	}

	mbPerCommandBuffer = mInfo.flags & kIndirectHostVisible;
	for (const DescriptorInfo& rDescriptorInfo : mInfo.descriptorInfos)
	{
		if (rDescriptorInfo.flags & kPerCommandBufferUniformBuffers || rDescriptorInfo.flags & kPerCommandBufferStorageBuffers || rDescriptorInfo.flags & kGlobalLayoutUniformBuffers || rDescriptorInfo.flags & kMainLayoutUniformBuffers)
		{
			mbPerCommandBuffer = true;
		}
	}

	// Bound descriptor entries by ShaderHeader::kiMaxDescriptorSetLayoutBindings; PipelineDescriptorWriter's scratch arrays use the same cap. A
	// kModel entry expands into several writes, guarded by per-push cursor assertions.
	ASSERT(std::ssize(mInfo.descriptorInfos) <= common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);

	if (mInfo.flags & kCompute)
	{
		PipelineCreator::CreateComputePipeline(*this);
	}
	else
	{
		PipelineCreator::CreateGraphicsPipeline(*this);
	}

	PipelineDescriptorWriter::Write(*this);

	// Non-indirect pipelines (e.g. terrain, water) use RecordDraw and never call
	// WriteIndirectBuffer, so request their textures immediately
	if (!(mInfo.flags & kIndirectHostVisible) && !(mInfo.flags & kIndirectDeviceLocal) && !mTextureCrcs.empty())
	{
		mbTexturesRequested = true;
		gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(mTextureCrcs, LoadPriority::kNormal);
	}
}

void Pipeline::Destroy() noexcept
{
	if (mVkPipelineLayout == VK_NULL_HANDLE)
	{
		return;
	}

	gpTextureManager->mTextureDescriptors.UnregisterPipeline(this);

	VkDescriptorPool vkDescriptorPool = gpDeviceManager->mVkDescriptorPool;

	// Free Set 1 descriptor sets (not allocated for inner multi-set pipelines with external Set 1)
	if (!mVkDescriptorSets.empty())
	{
		vkFreeDescriptorSets(gpDeviceManager->mVkDevice, vkDescriptorPool, static_cast<uint32_t>(std::ssize(mVkDescriptorSets)), mVkDescriptorSets.data());
	}
	mVkDescriptorSets.clear();

	if (!mVkDescriptorSetsSet2.empty())
	{
		vkFreeDescriptorSets(gpDeviceManager->mVkDevice, vkDescriptorPool, static_cast<uint32_t>(std::ssize(mVkDescriptorSetsSet2)), mVkDescriptorSetsSet2.data());
	}
	mVkDescriptorSetsSet2.clear();

	vkDestroyPipeline(gpDeviceManager->mVkDevice, mVkPipeline, nullptr);
	mVkPipeline = VK_NULL_HANDLE;

	vkDestroyPipelineLayout(gpDeviceManager->mVkDevice, mVkPipelineLayout, nullptr);
	mVkPipelineLayout = VK_NULL_HANDLE;

	// Destroy Set 1 layout if we own it (not external from ModelPipeline or TextureManager)
	if (mVkDescriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(gpDeviceManager->mVkDevice, mVkDescriptorSetLayout, nullptr);
	}
	mVkDescriptorSetLayout = VK_NULL_HANDLE;

	if (mSet2VkDescriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(gpDeviceManager->mVkDevice, mSet2VkDescriptorSetLayout, nullptr);
		mSet2VkDescriptorSetLayout = VK_NULL_HANDLE;
	}

	if (mIndirectVkBuffer != VK_NULL_HANDLE)
	{
		if (mInfo.flags & kIndirectHostVisible)
		{
			mpIndirectVkDrawIndexedIndirectCommand = nullptr;
			mpIndirectComputeVkDispatchIndirectCommand = nullptr;
		}

		vmaDestroyBuffer(gpDeviceManager->mpAllocator, mIndirectVkBuffer, mIndirectVmaAllocation);
		mIndirectVkBuffer = VK_NULL_HANDLE;
		mIndirectVmaAllocation = VK_NULL_HANDLE;
	}

	// WriteModelDescriptor's null-handle guard creates mModelMaterialsStorageBuffer once across the per-framebuffer loop; the member Buffer
	// destructor releases it. Model pipelines require fresh objects: Destroy on a pipeline holding this buffer is followed by object
	// destruction, and in-place recreation retains stale materials.
}

void Pipeline::RecordDraw(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, int64_t iInstanceCount, int64_t iFirstInstance, const XMFLOAT4& rf4PushConstants)
{
	ASSERT(!(mInfo.flags & kIndirectHostVisible) && !(mInfo.flags & kIndirectDeviceLocal) && !(mInfo.flags & kCompute));

	if (mInfo.flags & kPushConstants)
	{
		ASSERT(mInfo.iPushConstantBytes == 0);
		RecordPushConstants(vkCommandBuffer, mVkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, rf4PushConstants);
	}

	int64_t iDescriptorSetIndex = mbPerCommandBuffer ? iCommandBuffer : 0;
	vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipeline);
	BindGraphicsDescriptorSets(vkCommandBuffer, mVkPipelineLayout, mExternalVkDescriptorSetLayout, iDescriptorSetIndex, mVkDescriptorSets);
	mInfo.pVertexBuffer->RecordBindVertexBuffer(vkCommandBuffer);

	vkCmdDrawIndexed(vkCommandBuffer, static_cast<uint32_t>(mInfo.pVertexBuffer->mInfo.iCount), static_cast<uint32_t>(iInstanceCount), 0, 0, static_cast<uint32_t>(iFirstInstance));
}

void Pipeline::RecordBindPipelineAndDescriptors(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants)
{
	if (mInfo.flags & kPushConstants)
	{
		ASSERT(mInfo.iPushConstantBytes == 0);
		RecordPushConstants(vkCommandBuffer, mVkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, rf4PushConstants);
	}

	int64_t iDescriptorSetIndex = mbPerCommandBuffer ? iCommandBuffer : 0;
	vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipeline);
	BindGraphicsDescriptorSets(vkCommandBuffer, mVkPipelineLayout, mExternalVkDescriptorSetLayout, iDescriptorSetIndex, mVkDescriptorSets);
}

void Pipeline::RecordDrawIndirect(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants)
{
	ASSERT((mInfo.flags & kIndirectHostVisible || mInfo.flags & kIndirectDeviceLocal) && !(mInfo.flags & kCompute));

	if (mInfo.flags & kPushConstants)
	{
		ASSERT(mInfo.iPushConstantBytes == 0);
		RecordPushConstants(vkCommandBuffer, mVkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, rf4PushConstants);
	}

	vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipeline);
	BindGraphicsDescriptorSets(vkCommandBuffer, mVkPipelineLayout, mExternalVkDescriptorSetLayout, iCommandBuffer, mVkDescriptorSets);
	mInfo.pVertexBuffer->RecordBindVertexBuffer(vkCommandBuffer);
	// Device-local reads slot 0; host-visible indexes per-framebuffer
	int64_t iIndirectSlot = mInfo.flags & kIndirectDeviceLocal ? 0 : iCommandBuffer;
	int64_t iIndirectOffset = iIndirectSlot * static_cast<int64_t>(sizeof(VkDrawIndexedIndirectCommand));

	ASSERT(iIndirectSlot < miIndirectSlotCount);

	vkCmdDrawIndexedIndirect(vkCommandBuffer, mIndirectVkBuffer, static_cast<VkDeviceSize>(iIndirectOffset), 1, sizeof(VkDrawIndexedIndirectCommand));
}

void Pipeline::RecordDrawIndirectSet2(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants)
{
	ASSERT(mInfo.flags & kMultiSet);
	ASSERT((mInfo.flags & kIndirectHostVisible || mInfo.flags & kIndirectDeviceLocal) && !(mInfo.flags & kCompute));

	if (mInfo.flags & kPushConstants)
	{
		ASSERT(mInfo.iPushConstantBytes == 0);
		RecordPushConstants(vkCommandBuffer, mVkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, rf4PushConstants);
	}

	// Bind pipeline and Set 2 only (Set 0, Set 1, and vertex buffer already bound by ModelPipeline)
	vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipeline);
	vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelineLayout, 2, 1, &mVkDescriptorSetsSet2.at(iCommandBuffer), 0, nullptr);
	// Device-local reads slot 0; host-visible indexes per-framebuffer
	int64_t iIndirectSlot = mInfo.flags & kIndirectDeviceLocal ? 0 : iCommandBuffer;
	int64_t iIndirectOffset = iIndirectSlot * static_cast<int64_t>(sizeof(VkDrawIndexedIndirectCommand));

	ASSERT(iIndirectSlot < miIndirectSlotCount);

	vkCmdDrawIndexedIndirect(vkCommandBuffer, mIndirectVkBuffer, static_cast<VkDeviceSize>(iIndirectOffset), 1, sizeof(VkDrawIndexedIndirectCommand));
}

void Pipeline::RecordCompute(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, int64_t iGroupCountX, int64_t iGroupCountY, int64_t iGroupCountZ, const XMFLOAT4& rf4PushConstants)
{
	ASSERT(!(mInfo.flags & kIndirectHostVisible || mInfo.flags & kIndirectDeviceLocal) && mInfo.flags & kCompute);

	if (mInfo.flags & kPushConstants)
	{
		ASSERT(mInfo.iPushConstantBytes == 0);
		RecordPushConstants(vkCommandBuffer, mVkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, rf4PushConstants);
	}

	int64_t iDescriptorSetIndex = mbPerCommandBuffer ? iCommandBuffer : 0;
	vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mVkPipeline);
	BindComputeDescriptorSets(vkCommandBuffer, mVkPipelineLayout, mExternalVkDescriptorSetLayout, iCommandBuffer, iDescriptorSetIndex, mVkDescriptorSets);
	vkCmdDispatch(vkCommandBuffer, static_cast<uint32_t>(iGroupCountX), static_cast<uint32_t>(iGroupCountY), static_cast<uint32_t>(iGroupCountZ));
}

void Pipeline::RecordComputeIndirect(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants)
{
	ASSERT((mInfo.flags & kIndirectHostVisible || mInfo.flags & kIndirectDeviceLocal) && mInfo.flags & kCompute);

	if (mInfo.flags & kPushConstants)
	{
		ASSERT(mInfo.iPushConstantBytes == 0);
		RecordPushConstants(vkCommandBuffer, mVkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, rf4PushConstants);
	}

	// Host-visible indexes per-framebuffer; device-local reads slot 0
	int64_t iIndirectSlot = mInfo.flags & kIndirectHostVisible ? iCommandBuffer : 0;
	int64_t iDispatchOffset = iIndirectSlot * static_cast<int64_t>(sizeof(VkDispatchIndirectCommand));

	ASSERT(iIndirectSlot < miIndirectSlotCount);

	RecordComputeIndirectFrom(iCommandBuffer, vkCommandBuffer, mIndirectVkBuffer, iDispatchOffset);
}

void Pipeline::RecordComputeIndirectFrom(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, VkBuffer vkIndirectBuffer, int64_t iIndirectOffset)
{
	ASSERT(mInfo.flags & kCompute);

	int64_t iDescriptorSetIndex = mbPerCommandBuffer ? iCommandBuffer : 0;
	vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mVkPipeline);
	BindComputeDescriptorSets(vkCommandBuffer, mVkPipelineLayout, mExternalVkDescriptorSetLayout, iCommandBuffer, iDescriptorSetIndex, mVkDescriptorSets);
	vkCmdDispatchIndirect(vkCommandBuffer, vkIndirectBuffer, static_cast<VkDeviceSize>(iIndirectOffset));
}

void Pipeline::WriteIndirectBuffer(int64_t iCommandBuffer, int64_t iInstanceCount, int64_t iIndexCount, int64_t iFirstIndex, int64_t iVertexOffset)
{
	ASSERT(!(mInfo.flags & kCompute));

	// Request texture loading on first render with visible instances
	if (iInstanceCount > 0 && !mbTexturesRequested)
	{
		mbTexturesRequested = true;
		gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(mTextureCrcs, LoadPriority::kNormal);
	}

	ASSERT(mpIndirectVkDrawIndexedIndirectCommand != nullptr);

	VkDrawIndexedIndirectCommand& rVkCommand = mpIndirectVkDrawIndexedIndirectCommand[iCommandBuffer];

	// A negative iIndexCount means "draw the whole vertex buffer"; an explicit 0 stays a real zero-index draw, which is
	// how an empty material range is expressed. Never normalize one of the two into the other.
	rVkCommand.indexCount = static_cast<uint32_t>(iIndexCount < 0 ? mInfo.pVertexBuffer->mInfo.iCount : iIndexCount);
	rVkCommand.instanceCount = static_cast<uint32_t>(iInstanceCount);
	rVkCommand.firstIndex = static_cast<uint32_t>(iFirstIndex);
	rVkCommand.vertexOffset = static_cast<int32_t>(iVertexOffset);
	rVkCommand.firstInstance = 0;
}

void Pipeline::WriteIndirectComputeBuffer(int64_t iCommandBuffer, int64_t iGroupCountX, int64_t iGroupCountY, int64_t iGroupCountZ)
{
	// Unlike WriteIndirectBuffer, this does not service the indirect demand-load deferral (Pipeline::Create
	// defers texture requests for indirect pipelines): a host-visible compute-indirect pipeline must bind
	// render-target textures only, never lazily disk-loaded ones.
	ASSERT((mInfo.flags & kIndirectHostVisible) && (mInfo.flags & kCompute));
	ASSERT(mpIndirectComputeVkDispatchIndirectCommand != nullptr);

	VkDispatchIndirectCommand& rVkCommand = mpIndirectComputeVkDispatchIndirectCommand[iCommandBuffer];
	rVkCommand.x = static_cast<uint32_t>(iGroupCountX);
	rVkCommand.y = static_cast<uint32_t>(iGroupCountY);
	rVkCommand.z = static_cast<uint32_t>(iGroupCountZ);
}

} // namespace engine

#endif // defined(BT_CLIENT)
